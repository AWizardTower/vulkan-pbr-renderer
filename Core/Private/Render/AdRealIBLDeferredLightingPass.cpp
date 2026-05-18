#include "Render/AdRealIBLDeferredLightingPass.h"

#include "AdApplication.h"
#include "AdFileUtil.h"
#include "Graphic/AdVKDescriptorSet.h"
#include "Graphic/AdVKDevice.h"
#include "Graphic/AdVKFrameBuffer.h"
#include "Graphic/AdVKImage.h"
#include "Graphic/AdVKImageView.h"
#include "Graphic/AdVKPipeline.h"
#include "Graphic/AdVKRenderPass.h"
#include "Render/AdIBLResources.h"
#include "Render/AdRenderContext.h"
#include "Render/AdRenderTarget.h"
#include "Render/AdSampler.h"

#include <algorithm>

namespace ade{
    AdRealIBLDeferredLightingPass::AdRealIBLDeferredLightingPass(AdVKRenderPass *renderPass, uint32_t framesInFlight)
            : mFramesInFlight(framesInFlight) {
        AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        AdVKDevice *device = renderCxt->GetDevice();

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(8);
        for(uint32_t i = 0; i < 8; i++){
            bindings.push_back({
                .binding = i,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            });
        }
        mDescriptorSetLayout = std::make_shared<AdVKDescriptorSetLayout>(device, bindings);

        std::vector<VkDescriptorPoolSize> poolSizes = {
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = mFramesInFlight * 8
            }
        };
        mDescriptorPool = std::make_shared<AdVKDescriptorPool>(device, mFramesInFlight, poolSizes);
        mDescriptorSets = mDescriptorPool->AllocateDescriptorSet(mDescriptorSetLayout.get(), mFramesInFlight);

        mSampler = std::make_shared<AdSampler>(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        VkPushConstantRange lightPC = {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(AdRealIBLLightSettings)
        };
        ShaderLayout shaderLayout = {
            .descriptorSetLayouts = { mDescriptorSetLayout->GetHandle() },
            .pushConstants = { lightPC }
        };
        mPipelineLayout = std::make_shared<AdVKPipelineLayout>(device,
                                                               AD_RES_SHADER_DIR"09_real_ibl_lighting.vert",
                                                               AD_RES_SHADER_DIR"09_real_ibl_lighting.frag",
                                                               shaderLayout);
        mPipeline = std::make_shared<AdVKPipeline>(device, renderPass, mPipelineLayout.get());
        mPipeline->SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        mPipeline->SetDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR });
        mPipeline->SetMultisampleState(renderPass->GetSubPassSampleCount(), VK_FALSE);
        mPipeline->Create();

        mSourceImages.resize(mFramesInFlight, { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE });
        mSourceViews.resize(mFramesInFlight);
    }

    AdRealIBLDeferredLightingPass::~AdRealIBLDeferredLightingPass() {
        mSourceViews.clear();
        mPipeline.reset();
        mPipelineLayout.reset();
        mSampler.reset();
        mDescriptorPool.reset();
        mDescriptorSetLayout.reset();
    }

    void AdRealIBLDeferredLightingPass::Render(VkCommandBuffer cmdBuffer,
                                               AdRenderTarget *renderTarget,
                                               uint32_t frameSlot,
                                               AdVKImage *baseColor,
                                               AdVKImage *normal,
                                               AdVKImage *material,
                                               AdVKImage *worldPosition,
                                               AdVKImage *shadowDepth,
                                               AdIBLResources *iblResources,
                                               const AdRealIBLLightSettings &settings) {
        if(!renderTarget || !baseColor || !normal || !material || !worldPosition || !shadowDepth || !iblResources || mFramesInFlight == 0 || mDescriptorSets.empty()){
            return;
        }

        frameSlot %= mFramesInFlight;
        UpdateDescriptor(frameSlot, baseColor, normal, material, worldPosition, shadowDepth, iblResources);

        AdVKFrameBuffer *frameBuffer = renderTarget->GetFrameBuffer(frameSlot);
        if(!frameBuffer){
            return;
        }

        renderTarget->BeginAt(cmdBuffer, frameSlot);
        mPipeline->Bind(cmdBuffer);

        VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = static_cast<float>(frameBuffer->GetWidth()),
            .height = static_cast<float>(frameBuffer->GetHeight()),
            .minDepth = 0.f,
            .maxDepth = 1.f
        };
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
        VkRect2D scissor = {
            .offset = { 0, 0 },
            .extent = { frameBuffer->GetWidth(), frameBuffer->GetHeight() }
        };
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

        vkCmdPushConstants(cmdBuffer, mPipelineLayout->GetHandle(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(settings), &settings);
        VkDescriptorSet descriptorSet = mDescriptorSets[frameSlot];
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout->GetHandle(), 0, 1, &descriptorSet, 0, nullptr);
        vkCmdDraw(cmdBuffer, 3, 1, 0, 0);

        renderTarget->End(cmdBuffer);
    }

    void AdRealIBLDeferredLightingPass::InvalidateSourceViews() {
        std::fill(mSourceImages.begin(), mSourceImages.end(), std::array<VkImage, 8>{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE });
        mSourceViews.clear();
        mSourceViews.resize(mFramesInFlight);
    }

    void AdRealIBLDeferredLightingPass::UpdateDescriptor(uint32_t frameSlot,
                                                         AdVKImage *baseColor,
                                                         AdVKImage *normal,
                                                         AdVKImage *material,
                                                         AdVKImage *worldPosition,
                                                         AdVKImage *shadowDepth,
                                                         AdIBLResources *iblResources) {
        if(!baseColor || !normal || !material || !worldPosition || !shadowDepth || !iblResources || frameSlot >= mSourceImages.size()){
            return;
        }

        std::array<AdVKImage*, 5> sourceImages = { baseColor, normal, material, worldPosition, shadowDepth };
        std::array<VkImage, 8> sourceHandles = {
            baseColor->GetHandle(),
            normal->GetHandle(),
            material->GetHandle(),
            worldPosition->GetHandle(),
            shadowDepth->GetHandle(),
            iblResources->GetIrradianceCubeImage()->GetHandle(),
            iblResources->GetPrefilteredCubeImage()->GetHandle(),
            iblResources->GetBRDFLUTImage()->GetHandle()
        };
        if(mSourceImages[frameSlot] == sourceHandles
           && mSourceViews[frameSlot][0]
           && mSourceViews[frameSlot][1]
           && mSourceViews[frameSlot][2]
           && mSourceViews[frameSlot][3]
           && mSourceViews[frameSlot][4]){
            return;
        }

        AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        AdVKDevice *device = renderCxt->GetDevice();

        mSourceImages[frameSlot] = sourceHandles;
        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(8);
        std::array<VkDescriptorImageInfo, 8> imageInfos{};
        for(uint32_t i = 0; i < sourceImages.size(); i++){
            VkImageAspectFlags aspect = i == 4 ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            mSourceViews[frameSlot][i] = std::make_shared<AdVKImageView>(device, sourceImages[i]->GetHandle(), sourceImages[i]->GetFormat(), aspect);
            imageInfos[i] = DescriptorSetWriter::BuildImageInfo(mSampler->GetHandle(), mSourceViews[frameSlot][i]->GetHandle());
            writes.push_back(DescriptorSetWriter::WriteImage(mDescriptorSets[frameSlot], i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfos[i]));
        }

        imageInfos[5] = DescriptorSetWriter::BuildImageInfo(iblResources->GetIrradianceSampler()->GetHandle(), iblResources->GetIrradianceCubeView()->GetHandle());
        imageInfos[6] = DescriptorSetWriter::BuildImageInfo(iblResources->GetPrefilteredSampler()->GetHandle(), iblResources->GetPrefilteredCubeView()->GetHandle());
        imageInfos[7] = DescriptorSetWriter::BuildImageInfo(iblResources->GetBRDFLUTSampler()->GetHandle(), iblResources->GetBRDFLUTView()->GetHandle());
        writes.push_back(DescriptorSetWriter::WriteImage(mDescriptorSets[frameSlot], 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfos[5]));
        writes.push_back(DescriptorSetWriter::WriteImage(mDescriptorSets[frameSlot], 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfos[6]));
        writes.push_back(DescriptorSetWriter::WriteImage(mDescriptorSets[frameSlot], 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfos[7]));
        DescriptorSetWriter::UpdateDescriptorSets(device->GetHandle(), writes);
    }
}
