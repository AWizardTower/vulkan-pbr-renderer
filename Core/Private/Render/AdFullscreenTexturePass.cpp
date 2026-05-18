#include "Render/AdFullscreenTexturePass.h"

#include "AdApplication.h"
#include "AdFileUtil.h"
#include "Graphic/AdVKDescriptorSet.h"
#include "Graphic/AdVKDevice.h"
#include "Graphic/AdVKFrameBuffer.h"
#include "Graphic/AdVKImage.h"
#include "Graphic/AdVKImageView.h"
#include "Graphic/AdVKPipeline.h"
#include "Graphic/AdVKRenderPass.h"
#include "Render/AdRenderContext.h"
#include "Render/AdRenderTarget.h"
#include "Render/AdSampler.h"

#include <algorithm>

namespace ade{
    AdFullscreenTexturePass::AdFullscreenTexturePass(AdVKRenderPass *renderPass, uint32_t framesInFlight)
            : mFramesInFlight(framesInFlight) {
        AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        AdVKDevice *device = renderCxt->GetDevice();

        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            }
        };
        mDescriptorSetLayout = std::make_shared<AdVKDescriptorSetLayout>(device, bindings);

        std::vector<VkDescriptorPoolSize> poolSizes = {
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = mFramesInFlight
            }
        };
        mDescriptorPool = std::make_shared<AdVKDescriptorPool>(device, mFramesInFlight, poolSizes);
        mDescriptorSets = mDescriptorPool->AllocateDescriptorSet(mDescriptorSetLayout.get(), mFramesInFlight);

        mSampler = std::make_shared<AdSampler>(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        ShaderLayout shaderLayout = {
            .descriptorSetLayouts = { mDescriptorSetLayout->GetHandle() }
        };
        mPipelineLayout = std::make_shared<AdVKPipelineLayout>(device,
                                                               AD_RES_SHADER_DIR"04_fullscreen_sample.vert",
                                                               AD_RES_SHADER_DIR"04_fullscreen_sample.frag",
                                                               shaderLayout);
        mPipeline = std::make_shared<AdVKPipeline>(device, renderPass, mPipelineLayout.get());
        mPipeline->SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        mPipeline->SetDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR });
        mPipeline->SetMultisampleState(renderPass->GetSubPassSampleCount(), VK_FALSE);
        mPipeline->Create();

        mSourceImages.resize(mFramesInFlight, VK_NULL_HANDLE);
        mSourceViews.resize(mFramesInFlight);
    }

    AdFullscreenTexturePass::~AdFullscreenTexturePass() {
        mSourceViews.clear();
        mPipeline.reset();
        mPipelineLayout.reset();
        mSampler.reset();
        mDescriptorPool.reset();
        mDescriptorSetLayout.reset();
    }

    void AdFullscreenTexturePass::Render(VkCommandBuffer cmdBuffer, AdRenderTarget *renderTarget, uint32_t frameSlot, uint32_t imageIndex, AdVKImage *sourceImage) {
        if(!renderTarget || !sourceImage || mFramesInFlight == 0 || mDescriptorSets.empty()){
            return;
        }

        frameSlot %= mFramesInFlight;
        UpdateDescriptor(frameSlot, sourceImage);

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
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout->GetHandle(),
                                0, 1, &descriptorSet, 0, nullptr);
        vkCmdDraw(cmdBuffer, 3, 1, 0, 0);

        renderTarget->End(cmdBuffer);
    }

    void AdFullscreenTexturePass::InvalidateSourceViews() {
        std::fill(mSourceImages.begin(), mSourceImages.end(), VK_NULL_HANDLE);
        mSourceViews.clear();
        mSourceViews.resize(mFramesInFlight);
    }

    void AdFullscreenTexturePass::UpdateDescriptor(uint32_t frameSlot, AdVKImage *sourceImage) {
        if(!sourceImage || frameSlot >= mSourceImages.size()){
            return;
        }
        if(mSourceImages[frameSlot] == sourceImage->GetHandle() && mSourceViews[frameSlot]){
            return;
        }

        AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        AdVKDevice *device = renderCxt->GetDevice();

        mSourceImages[frameSlot] = sourceImage->GetHandle();
        mSourceViews[frameSlot] = std::make_shared<AdVKImageView>(device, sourceImage->GetHandle(), sourceImage->GetFormat(), VK_IMAGE_ASPECT_COLOR_BIT);

        VkDescriptorImageInfo imageInfo = DescriptorSetWriter::BuildImageInfo(mSampler->GetHandle(), mSourceViews[frameSlot]->GetHandle());
        VkWriteDescriptorSet imageWrite = DescriptorSetWriter::WriteImage(mDescriptorSets[frameSlot], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo);
        DescriptorSetWriter::UpdateDescriptorSets(device->GetHandle(), { imageWrite });
    }
}
