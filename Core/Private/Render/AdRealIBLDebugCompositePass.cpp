#include "Render/AdRealIBLDebugCompositePass.h"

#include "AdApplication.h"
#include "AdFileUtil.h"
#include "Graphic/AdVKBuffer.h"
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
    AdRealIBLDebugCompositePass::AdRealIBLDebugCompositePass(AdVKRenderPass *renderPass, uint32_t framesInFlight)
            : mFramesInFlight(framesInFlight) {
        AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        AdVKDevice *device = renderCxt->GetDevice();

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(11);
        for(uint32_t i = 0; i < 10; i++){
            bindings.push_back({
                .binding = i,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            });
        }
        bindings.push_back({
            .binding = 10,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
        });
        mDescriptorSetLayout = std::make_shared<AdVKDescriptorSetLayout>(device, bindings);

        std::vector<VkDescriptorPoolSize> poolSizes = {
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = mFramesInFlight * 10
            },
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = mFramesInFlight
            }
        };
        mDescriptorPool = std::make_shared<AdVKDescriptorPool>(device, mFramesInFlight, poolSizes);
        mDescriptorSets = mDescriptorPool->AllocateDescriptorSet(mDescriptorSetLayout.get(), mFramesInFlight);

        mSampler = std::make_shared<AdSampler>(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        mSettingsBuffers.reserve(mFramesInFlight);
        for(uint32_t i = 0; i < mFramesInFlight; i++){
            mSettingsBuffers.push_back(std::make_shared<AdVKBuffer>(device,
                                                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                     sizeof(AdRealIBLDebugSettings),
                                                                     nullptr,
                                                                     true));
        }

        ShaderLayout shaderLayout = {
            .descriptorSetLayouts = { mDescriptorSetLayout->GetHandle() }
        };
        mPipelineLayout = std::make_shared<AdVKPipelineLayout>(device,
                                                               AD_RES_SHADER_DIR"09_real_ibl_debug_composite.vert",
                                                               AD_RES_SHADER_DIR"09_real_ibl_debug_composite.frag",
                                                               shaderLayout);
        mPipeline = std::make_shared<AdVKPipeline>(device, renderPass, mPipelineLayout.get());
        mPipeline->SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        mPipeline->SetDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR });
        mPipeline->SetMultisampleState(renderPass->GetSubPassSampleCount(), VK_FALSE);
        mPipeline->Create();

        mSourceImages.resize(mFramesInFlight, { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE });
        mSourceViews.resize(mFramesInFlight);
    }

    AdRealIBLDebugCompositePass::~AdRealIBLDebugCompositePass() {
        mSettingsBuffers.clear();
        mSourceViews.clear();
        mPipeline.reset();
        mPipelineLayout.reset();
        mSampler.reset();
        mDescriptorPool.reset();
        mDescriptorSetLayout.reset();
    }

    void AdRealIBLDebugCompositePass::Render(VkCommandBuffer cmdBuffer,
                                             AdRenderTarget *renderTarget,
                                             uint32_t frameSlot,
                                             uint32_t imageIndex,
                                             AdVKImage *lightingColor,
                                             AdVKImage *baseColor,
                                             AdVKImage *normal,
                                             AdVKImage *material,
                                             AdVKImage *worldPosition,
                                             AdVKImage *shadowDepth,
                                             AdIBLResources *iblResources,
                                             const AdRealIBLDebugSettings &settings) {
        if(!renderTarget || !lightingColor || !baseColor || !normal || !material || !worldPosition || !shadowDepth || !iblResources || mFramesInFlight == 0 || mDescriptorSets.empty()){
            return;
        }

        frameSlot %= mFramesInFlight;
        UpdateDescriptor(frameSlot, lightingColor, baseColor, normal, material, worldPosition, shadowDepth, iblResources, settings);

        AdVKFrameBuffer *frameBuffer = renderTarget->GetFrameBuffer(imageIndex);
        if(!frameBuffer){
            return;
        }

        renderTarget->BeginAt(cmdBuffer, imageIndex);
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

        VkDescriptorSet descriptorSet = mDescriptorSets[frameSlot];
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout->GetHandle(), 0, 1, &descriptorSet, 0, nullptr);
        vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
        renderTarget->End(cmdBuffer);
    }

    void AdRealIBLDebugCompositePass::InvalidateSourceViews() {
        std::fill(mSourceImages.begin(), mSourceImages.end(), std::array<VkImage, 10>{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE });
        mSourceViews.clear();
        mSourceViews.resize(mFramesInFlight);
    }

    void AdRealIBLDebugCompositePass::UpdateDescriptor(uint32_t frameSlot,
                                                       AdVKImage *lightingColor,
                                                       AdVKImage *baseColor,
                                                       AdVKImage *normal,
                                                       AdVKImage *material,
                                                       AdVKImage *worldPosition,
                                                       AdVKImage *shadowDepth,
                                                       AdIBLResources *iblResources,
                                                       const AdRealIBLDebugSettings &settings) {
        if(!lightingColor || !baseColor || !normal || !material || !worldPosition || !shadowDepth || !iblResources || frameSlot >= mSourceImages.size()){
            return;
        }

        mSettingsBuffers[frameSlot]->WriteData(const_cast<AdRealIBLDebugSettings*>(&settings));

        std::array<AdVKImage*, 6> sourceImages = { lightingColor, baseColor, normal, material, worldPosition, shadowDepth };
        std::array<VkImage, 10> sourceHandles = {
            lightingColor->GetHandle(),
            baseColor->GetHandle(),
            normal->GetHandle(),
            material->GetHandle(),
            worldPosition->GetHandle(),
            shadowDepth->GetHandle(),
            iblResources->GetEnvironmentCubeImage()->GetHandle(),
            iblResources->GetIrradianceCubeImage()->GetHandle(),
            iblResources->GetPrefilteredCubeImage()->GetHandle(),
            iblResources->GetBRDFLUTImage()->GetHandle()
        };
        if(mSourceImages[frameSlot] == sourceHandles
           && mSourceViews[frameSlot][0]
           && mSourceViews[frameSlot][1]
           && mSourceViews[frameSlot][2]
           && mSourceViews[frameSlot][3]
           && mSourceViews[frameSlot][4]
           && mSourceViews[frameSlot][5]){
            return;
        }

        AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        AdVKDevice *device = renderCxt->GetDevice();

        mSourceImages[frameSlot] = sourceHandles;
        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(11);
        std::array<VkDescriptorImageInfo, 10> imageInfos{};
        for(uint32_t i = 0; i < sourceImages.size(); i++){
            VkImageAspectFlags aspect = i == 5 ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            mSourceViews[frameSlot][i] = std::make_shared<AdVKImageView>(device, sourceImages[i]->GetHandle(), sourceImages[i]->GetFormat(), aspect);
            imageInfos[i] = DescriptorSetWriter::BuildImageInfo(mSampler->GetHandle(), mSourceViews[frameSlot][i]->GetHandle());
            writes.push_back(DescriptorSetWriter::WriteImage(mDescriptorSets[frameSlot], i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfos[i]));
        }

        imageInfos[6] = DescriptorSetWriter::BuildImageInfo(iblResources->GetEnvironmentSampler()->GetHandle(), iblResources->GetEnvironmentCubeView()->GetHandle());
        imageInfos[7] = DescriptorSetWriter::BuildImageInfo(iblResources->GetIrradianceSampler()->GetHandle(), iblResources->GetIrradianceCubeView()->GetHandle());
        imageInfos[8] = DescriptorSetWriter::BuildImageInfo(iblResources->GetPrefilteredSampler()->GetHandle(), iblResources->GetPrefilteredCubeView()->GetHandle());
        imageInfos[9] = DescriptorSetWriter::BuildImageInfo(iblResources->GetBRDFLUTSampler()->GetHandle(), iblResources->GetBRDFLUTView()->GetHandle());
        writes.push_back(DescriptorSetWriter::WriteImage(mDescriptorSets[frameSlot], 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfos[6]));
        writes.push_back(DescriptorSetWriter::WriteImage(mDescriptorSets[frameSlot], 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfos[7]));
        writes.push_back(DescriptorSetWriter::WriteImage(mDescriptorSets[frameSlot], 8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfos[8]));
        writes.push_back(DescriptorSetWriter::WriteImage(mDescriptorSets[frameSlot], 9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfos[9]));

        VkDescriptorBufferInfo settingsInfo = DescriptorSetWriter::BuildBufferInfo(mSettingsBuffers[frameSlot]->GetHandle(), 0, sizeof(AdRealIBLDebugSettings));
        writes.push_back(DescriptorSetWriter::WriteBuffer(mDescriptorSets[frameSlot], 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &settingsInfo));
        DescriptorSetWriter::UpdateDescriptorSets(device->GetHandle(), writes);
    }
}
