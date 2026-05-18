#include "Render/AdIBLPrecomputePass.h"

#include "AdApplication.h"
#include "AdFileUtil.h"
#include "Graphic/AdVKDescriptorSet.h"
#include "Graphic/AdVKDebugUtils.h"
#include "Graphic/AdVKDevice.h"
#include "Graphic/AdVKImage.h"
#include "Graphic/AdVKImageView.h"
#include "Graphic/AdVKPipeline.h"
#include "Graphic/AdVKRenderPass.h"
#include "Render/AdHDRTexture.h"
#include "Render/AdIBLResources.h"
#include "Render/AdRenderContext.h"
#include "Render/AdRenderer.h"
#include "Render/AdSampler.h"

#include <algorithm>
#include <array>

namespace ade{
    namespace{
        constexpr uint32_t CubeFaceCount = 6;

        struct AdIBLPrecomputePushConstants{
            glm::vec4 Params{ 0.f };
        };

        uint32_t MipSize(uint32_t baseSize, uint32_t mipLevel) {
            return std::max(1u, baseSize >> mipLevel);
        }
    }

    AdIBLPrecomputePass::AdIBLPrecomputePass(VkFormat colorFormat) {
        AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        mDevice = renderCxt->GetDevice();

        CreateRenderPass(colorFormat);
        CreateDescriptors();
        CreatePipelines();
    }

    AdIBLPrecomputePass::~AdIBLPrecomputePass() {
        DestroyTargets(mBRDFLUTTargets);
        DestroyTargets(mPrefilterTargets);
        DestroyTargets(mIrradianceTargets);
        DestroyTargets(mEnvironmentTargets);
        mBRDFLUTPipeline.reset();
        mPrefilterPipeline.reset();
        mIrradiancePipeline.reset();
        mEquirectPipeline.reset();
        mBRDFLUTPipelineLayout.reset();
        mPrefilterPipelineLayout.reset();
        mIrradiancePipelineLayout.reset();
        mEquirectPipelineLayout.reset();
        mDescriptorPool.reset();
        mCubeDescriptorSetLayout.reset();
        mEquirectDescriptorSetLayout.reset();
        mColorRenderPass.reset();
    }

    void AdIBLPrecomputePass::Precompute(VkCommandBuffer cmdBuffer,
                                         AdRenderer *renderer,
                                         AdHDRTexture *environmentHDR,
                                         AdIBLResources *resources) {
        if(!cmdBuffer || !environmentHDR || !resources){
            return;
        }

        EnsureTargets(resources);
        UpdateDescriptors(environmentHDR, resources);

        auto beginScope = [renderer, cmdBuffer](const char *name) {
            if(renderer){ renderer->BeginGpuScope(cmdBuffer, name); }
            else { AdVKDebugUtils::BeginLabel(cmdBuffer, name); }
        };
        auto endScope = [renderer, cmdBuffer]() {
            if(renderer){ renderer->EndGpuScope(cmdBuffer); }
            else { AdVKDebugUtils::EndLabel(cmdBuffer); }
        };

        beginScope("IBL.EquirectToCubemap");
        RunEquirectToCubemap(cmdBuffer, resources);
        endScope();

        resources->GetEnvironmentCubeImage()->GenerateMipmaps2DArray(cmdBuffer,
                                                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                                     resources->GetEnvironmentMipLayout());
        resources->SetEnvironmentLayouts(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        beginScope("IBL.IrradianceConvolution");
        RunIrradianceConvolution(cmdBuffer, resources);
        endScope();

        beginScope("IBL.PrefilterSpecular");
        RunPrefilterSpecular(cmdBuffer, resources);
        endScope();

        beginScope("IBL.GenerateBRDFLUT");
        RunBRDFLUT(cmdBuffer, resources);
        endScope();
    }

    void AdIBLPrecomputePass::CreateRenderPass(VkFormat colorFormat) {
        std::vector<Attachment> attachments = {
            {
                .format = colorFormat,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
            }
        };
        std::vector<RenderSubPass> subpasses = {
            {
                .colorAttachments = { 0 },
                .sampleCount = VK_SAMPLE_COUNT_1_BIT
            }
        };
        mColorRenderPass = std::make_shared<AdVKRenderPass>(mDevice, attachments, subpasses);
    }

    void AdIBLPrecomputePass::CreateDescriptors() {
        std::vector<VkDescriptorSetLayoutBinding> singleSamplerBinding = {
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            }
        };
        mEquirectDescriptorSetLayout = std::make_shared<AdVKDescriptorSetLayout>(mDevice, singleSamplerBinding);
        mCubeDescriptorSetLayout = std::make_shared<AdVKDescriptorSetLayout>(mDevice, singleSamplerBinding);

        std::vector<VkDescriptorPoolSize> poolSizes = {
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 2
            }
        };
        mDescriptorPool = std::make_shared<AdVKDescriptorPool>(mDevice, 2, poolSizes);
        mEquirectDescriptorSet = mDescriptorPool->AllocateDescriptorSet(mEquirectDescriptorSetLayout.get(), 1)[0];
        mEnvironmentCubeDescriptorSet = mDescriptorPool->AllocateDescriptorSet(mCubeDescriptorSetLayout.get(), 1)[0];
    }

    void AdIBLPrecomputePass::CreatePipelines() {
        VkPushConstantRange facePush = {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(AdIBLPrecomputePushConstants)
        };
        ShaderLayout equirectLayout = {
            .descriptorSetLayouts = { mEquirectDescriptorSetLayout->GetHandle() },
            .pushConstants = { facePush }
        };
        ShaderLayout cubeLayout = {
            .descriptorSetLayouts = { mCubeDescriptorSetLayout->GetHandle() },
            .pushConstants = { facePush }
        };
        ShaderLayout brdfLayout = {};

        mEquirectPipelineLayout = std::make_shared<AdVKPipelineLayout>(mDevice,
                                                                       AD_RES_SHADER_DIR"09_equirect_to_cube.vert",
                                                                       AD_RES_SHADER_DIR"09_equirect_to_cube.frag",
                                                                       equirectLayout);
        mIrradiancePipelineLayout = std::make_shared<AdVKPipelineLayout>(mDevice,
                                                                         AD_RES_SHADER_DIR"09_irradiance_convolution.vert",
                                                                         AD_RES_SHADER_DIR"09_irradiance_convolution.frag",
                                                                         cubeLayout);
        mPrefilterPipelineLayout = std::make_shared<AdVKPipelineLayout>(mDevice,
                                                                        AD_RES_SHADER_DIR"09_prefilter_env.vert",
                                                                        AD_RES_SHADER_DIR"09_prefilter_env.frag",
                                                                        cubeLayout);
        mBRDFLUTPipelineLayout = std::make_shared<AdVKPipelineLayout>(mDevice,
                                                                      AD_RES_SHADER_DIR"09_brdf_lut.vert",
                                                                      AD_RES_SHADER_DIR"09_brdf_lut.frag",
                                                                      brdfLayout);

        auto buildPipeline = [this](std::shared_ptr<AdVKPipelineLayout> &layout) {
            std::shared_ptr<AdVKPipeline> pipeline = std::make_shared<AdVKPipeline>(mDevice, mColorRenderPass.get(), layout.get());
            pipeline->SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            pipeline->SetDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR });
            pipeline->SetMultisampleState(VK_SAMPLE_COUNT_1_BIT, VK_FALSE);
            pipeline->Create();
            return pipeline;
        };

        mEquirectPipeline = buildPipeline(mEquirectPipelineLayout);
        mIrradiancePipeline = buildPipeline(mIrradiancePipelineLayout);
        mPrefilterPipeline = buildPipeline(mPrefilterPipelineLayout);
        mBRDFLUTPipeline = buildPipeline(mBRDFLUTPipelineLayout);
    }

    void AdIBLPrecomputePass::EnsureTargets(AdIBLResources *resources) {
        if(!resources){
            return;
        }

        if(mEnvironmentTargetImage == resources->GetEnvironmentCubeImage()->GetHandle()
           && mIrradianceTargetImage == resources->GetIrradianceCubeImage()->GetHandle()
           && mPrefilterTargetImage == resources->GetPrefilteredCubeImage()->GetHandle()
           && mBRDFLUTTargetImage == resources->GetBRDFLUTImage()->GetHandle()){
            return;
        }

        DestroyTargets(mBRDFLUTTargets);
        DestroyTargets(mPrefilterTargets);
        DestroyTargets(mIrradianceTargets);
        DestroyTargets(mEnvironmentTargets);

        mEnvironmentTargetImage = resources->GetEnvironmentCubeImage()->GetHandle();
        mIrradianceTargetImage = resources->GetIrradianceCubeImage()->GetHandle();
        mPrefilterTargetImage = resources->GetPrefilteredCubeImage()->GetHandle();
        mBRDFLUTTargetImage = resources->GetBRDFLUTImage()->GetHandle();

        mEnvironmentTargets.reserve(CubeFaceCount);
        for(uint32_t face = 0; face < CubeFaceCount; face++){
            mEnvironmentTargets.push_back(CreateTarget(resources->GetEnvironmentCubeImage(),
                                                       resources->GetEnvironmentSize(),
                                                       resources->GetEnvironmentSize(),
                                                       0,
                                                       face));
        }

        mIrradianceTargets.reserve(CubeFaceCount);
        for(uint32_t face = 0; face < CubeFaceCount; face++){
            mIrradianceTargets.push_back(CreateTarget(resources->GetIrradianceCubeImage(),
                                                      resources->GetIrradianceSize(),
                                                      resources->GetIrradianceSize(),
                                                      0,
                                                      face));
        }

        for(uint32_t mip = 0; mip < resources->GetPrefilterMipLevels(); mip++){
            uint32_t size = MipSize(resources->GetPrefilterSize(), mip);
            for(uint32_t face = 0; face < CubeFaceCount; face++){
                mPrefilterTargets.push_back(CreateTarget(resources->GetPrefilteredCubeImage(),
                                                         size,
                                                         size,
                                                         mip,
                                                         face));
            }
        }

        mBRDFLUTTargets.push_back(CreateTarget(resources->GetBRDFLUTImage(),
                                               resources->GetBRDFLUTSize(),
                                               resources->GetBRDFLUTSize(),
                                               0,
                                               0));
    }

    void AdIBLPrecomputePass::DestroyTargets(std::vector<SubresourceTarget> &targets) {
        for(auto &target: targets){
            VK_D(Framebuffer, mDevice->GetHandle(), target.Framebuffer);
            target.View.reset();
        }
        targets.clear();
    }

    AdIBLPrecomputePass::SubresourceTarget AdIBLPrecomputePass::CreateTarget(AdVKImage *image,
                                                                             uint32_t width,
                                                                             uint32_t height,
                                                                             uint32_t mipLevel,
                                                                             uint32_t arrayLayer) {
        SubresourceTarget target;
        target.Width = width;
        target.Height = height;
        target.View = std::make_shared<AdVKImageView>(mDevice,
                                                      image->GetHandle(),
                                                      image->GetFormat(),
                                                      VK_IMAGE_ASPECT_COLOR_BIT,
                                                      VK_IMAGE_VIEW_TYPE_2D,
                                                      mipLevel,
                                                      1,
                                                      arrayLayer,
                                                      1);
        VkImageView attachment = target.View->GetHandle();
        VkFramebufferCreateInfo frameBufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = mColorRenderPass->GetHandle(),
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .width = width,
            .height = height,
            .layers = 1
        };
        CALL_VK(vkCreateFramebuffer(mDevice->GetHandle(), &frameBufferInfo, nullptr, &target.Framebuffer));
        return target;
    }

    void AdIBLPrecomputePass::BeginTarget(VkCommandBuffer cmdBuffer, const SubresourceTarget &target) {
        VkClearValue clearValue{};
        clearValue.color = { 0.f, 0.f, 0.f, 1.f };
        VkRenderPassBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = mColorRenderPass->GetHandle(),
            .framebuffer = target.Framebuffer,
            .renderArea = {
                .offset = { 0, 0 },
                .extent = { target.Width, target.Height }
            },
            .clearValueCount = 1,
            .pClearValues = &clearValue
        };
        vkCmdBeginRenderPass(cmdBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void AdIBLPrecomputePass::EndTarget(VkCommandBuffer cmdBuffer) {
        vkCmdEndRenderPass(cmdBuffer);
    }

    void AdIBLPrecomputePass::RenderFullscreen(VkCommandBuffer cmdBuffer, const SubresourceTarget &target) {
        VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = static_cast<float>(target.Width),
            .height = static_cast<float>(target.Height),
            .minDepth = 0.f,
            .maxDepth = 1.f
        };
        VkRect2D scissor = {
            .offset = { 0, 0 },
            .extent = { target.Width, target.Height }
        };
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
        vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
    }

    void AdIBLPrecomputePass::UpdateDescriptors(AdHDRTexture *environmentHDR, AdIBLResources *resources) {
        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(2);
        std::array<VkDescriptorImageInfo, 2> imageInfos{};
        imageInfos[0] = DescriptorSetWriter::BuildImageInfo(environmentHDR->GetSampler()->GetHandle(), environmentHDR->GetImageView()->GetHandle());
        imageInfos[1] = DescriptorSetWriter::BuildImageInfo(resources->GetEnvironmentSampler()->GetHandle(), resources->GetEnvironmentCubeView()->GetHandle());
        writes.push_back(DescriptorSetWriter::WriteImage(mEquirectDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfos[0]));
        writes.push_back(DescriptorSetWriter::WriteImage(mEnvironmentCubeDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfos[1]));
        DescriptorSetWriter::UpdateDescriptorSets(mDevice->GetHandle(), writes);
    }

    void AdIBLPrecomputePass::RunEquirectToCubemap(VkCommandBuffer cmdBuffer, AdIBLResources *resources) {
        AdVKImage::TransitionLayout(cmdBuffer,
                                    resources->GetEnvironmentCubeImage()->GetHandle(),
                                    resources->GetEnvironmentBaseLayout(),
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_ASPECT_COLOR_BIT,
                                    0,
                                    1,
                                    0,
                                    CubeFaceCount);
        resources->SetEnvironmentLayouts(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, resources->GetEnvironmentMipLayout());

        mEquirectPipeline->Bind(cmdBuffer);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mEquirectPipelineLayout->GetHandle(),
                                0, 1, &mEquirectDescriptorSet, 0, nullptr);
        for(uint32_t face = 0; face < CubeFaceCount; face++){
            AdIBLPrecomputePushConstants pc{};
            pc.Params = glm::vec4(static_cast<float>(face), 0.f, 0.f, 0.f);
            vkCmdPushConstants(cmdBuffer, mEquirectPipelineLayout->GetHandle(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
            BeginTarget(cmdBuffer, mEnvironmentTargets[face]);
            RenderFullscreen(cmdBuffer, mEnvironmentTargets[face]);
            EndTarget(cmdBuffer);
        }
    }

    void AdIBLPrecomputePass::RunIrradianceConvolution(VkCommandBuffer cmdBuffer, AdIBLResources *resources) {
        AdVKImage::TransitionLayout(cmdBuffer,
                                    resources->GetIrradianceCubeImage()->GetHandle(),
                                    resources->GetIrradianceLayout(),
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_ASPECT_COLOR_BIT,
                                    0,
                                    1,
                                    0,
                                    CubeFaceCount);
        resources->SetIrradianceLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        mIrradiancePipeline->Bind(cmdBuffer);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mIrradiancePipelineLayout->GetHandle(),
                                0, 1, &mEnvironmentCubeDescriptorSet, 0, nullptr);
        for(uint32_t face = 0; face < CubeFaceCount; face++){
            AdIBLPrecomputePushConstants pc{};
            pc.Params = glm::vec4(static_cast<float>(face), 0.f, static_cast<float>(resources->GetEnvironmentMipLevels() - 1), 0.f);
            vkCmdPushConstants(cmdBuffer, mIrradiancePipelineLayout->GetHandle(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
            BeginTarget(cmdBuffer, mIrradianceTargets[face]);
            RenderFullscreen(cmdBuffer, mIrradianceTargets[face]);
            EndTarget(cmdBuffer);
        }

        AdVKImage::TransitionLayout(cmdBuffer,
                                    resources->GetIrradianceCubeImage()->GetHandle(),
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_IMAGE_ASPECT_COLOR_BIT,
                                    0,
                                    1,
                                    0,
                                    CubeFaceCount);
        resources->SetIrradianceLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void AdIBLPrecomputePass::RunPrefilterSpecular(VkCommandBuffer cmdBuffer, AdIBLResources *resources) {
        AdVKImage::TransitionLayout(cmdBuffer,
                                    resources->GetPrefilteredCubeImage()->GetHandle(),
                                    resources->GetPrefilteredLayout(),
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_ASPECT_COLOR_BIT,
                                    0,
                                    resources->GetPrefilterMipLevels(),
                                    0,
                                    CubeFaceCount);
        resources->SetPrefilteredLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        mPrefilterPipeline->Bind(cmdBuffer);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPrefilterPipelineLayout->GetHandle(),
                                0, 1, &mEnvironmentCubeDescriptorSet, 0, nullptr);

        uint32_t targetIndex = 0;
        float maxMip = static_cast<float>(std::max(1u, resources->GetPrefilterMipLevels()) - 1);
        for(uint32_t mip = 0; mip < resources->GetPrefilterMipLevels(); mip++){
            float roughness = maxMip > 0.f ? static_cast<float>(mip) / maxMip : 0.f;
            for(uint32_t face = 0; face < CubeFaceCount; face++){
                AdIBLPrecomputePushConstants pc{};
                pc.Params = glm::vec4(static_cast<float>(face), roughness, static_cast<float>(resources->GetEnvironmentMipLevels() - 1), 0.f);
                vkCmdPushConstants(cmdBuffer, mPrefilterPipelineLayout->GetHandle(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
                BeginTarget(cmdBuffer, mPrefilterTargets[targetIndex]);
                RenderFullscreen(cmdBuffer, mPrefilterTargets[targetIndex]);
                EndTarget(cmdBuffer);
                targetIndex++;
            }
        }

        AdVKImage::TransitionLayout(cmdBuffer,
                                    resources->GetPrefilteredCubeImage()->GetHandle(),
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_IMAGE_ASPECT_COLOR_BIT,
                                    0,
                                    resources->GetPrefilterMipLevels(),
                                    0,
                                    CubeFaceCount);
        resources->SetPrefilteredLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void AdIBLPrecomputePass::RunBRDFLUT(VkCommandBuffer cmdBuffer, AdIBLResources *resources) {
        AdVKImage::TransitionLayout(cmdBuffer,
                                    resources->GetBRDFLUTImage()->GetHandle(),
                                    resources->GetBRDFLUTLayout(),
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_ASPECT_COLOR_BIT);
        resources->SetBRDFLUTLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        mBRDFLUTPipeline->Bind(cmdBuffer);
        BeginTarget(cmdBuffer, mBRDFLUTTargets[0]);
        RenderFullscreen(cmdBuffer, mBRDFLUTTargets[0]);
        EndTarget(cmdBuffer);

        AdVKImage::TransitionLayout(cmdBuffer,
                                    resources->GetBRDFLUTImage()->GetHandle(),
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_IMAGE_ASPECT_COLOR_BIT);
        resources->SetBRDFLUTLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}
