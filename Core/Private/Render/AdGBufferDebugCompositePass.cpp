#include "Render/AdGBufferDebugCompositePass.h"

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
    AdGBufferDebugCompositePass::AdGBufferDebugCompositePass(AdVKRenderPass *renderPass, uint32_t framesInFlight)
            : mFramesInFlight(framesInFlight) {
        AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        AdVKDevice *device = renderCxt->GetDevice();

        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            },
            {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            },
            {
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            }
        };
        mDescriptorSetLayout = std::make_shared<AdVKDescriptorSetLayout>(device, bindings);

        std::vector<VkDescriptorPoolSize> poolSizes = {
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = mFramesInFlight * 3
            }
        };
        mDescriptorPool = std::make_shared<AdVKDescriptorPool>(device, mFramesInFlight, poolSizes);
        mDescriptorSets = mDescriptorPool->AllocateDescriptorSet(mDescriptorSetLayout.get(), mFramesInFlight);

        mSampler = std::make_shared<AdSampler>(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        VkPushConstantRange debugPC = {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(DebugPushConstants)
        };
        ShaderLayout shaderLayout = {
            .descriptorSetLayouts = { mDescriptorSetLayout->GetHandle() },
            .pushConstants = { debugPC }
        };
        mPipelineLayout = std::make_shared<AdVKPipelineLayout>(device,
                                                               AD_RES_SHADER_DIR"05_gbuffer_debug.vert",
                                                               AD_RES_SHADER_DIR"05_gbuffer_debug.frag",
                                                               shaderLayout);
        mPipeline = std::make_shared<AdVKPipeline>(device, renderPass, mPipelineLayout.get());
        mPipeline->SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        mPipeline->SetDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR });
        mPipeline->SetMultisampleState(renderPass->GetSubPassSampleCount(), VK_FALSE);
        mPipeline->Create();

        mSourceImages.resize(mFramesInFlight, { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE });
        mSourceViews.resize(mFramesInFlight);
    }

    AdGBufferDebugCompositePass::~AdGBufferDebugCompositePass() {
        mSourceViews.clear();
        mPipeline.reset();
        mPipelineLayout.reset();
        mSampler.reset();
        mDescriptorPool.reset();
        mDescriptorSetLayout.reset();
    }

    void AdGBufferDebugCompositePass::Render(VkCommandBuffer cmdBuffer,
                                             AdRenderTarget *renderTarget,
                                             uint32_t frameSlot,
                                             uint32_t imageIndex,
                                             AdVKImage *baseColor,
                                             AdVKImage *normal,
                                             AdVKImage *material,
                                             AdGBufferDebugViewMode debugViewMode) {
        if(!renderTarget || !baseColor || !normal || !material || mFramesInFlight == 0 || mDescriptorSets.empty()){
            return;
        }

        frameSlot %= mFramesInFlight;
        UpdateDescriptor(frameSlot, baseColor, normal, material);

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

        DebugPushConstants pc{
            .debugViewMode = static_cast<uint32_t>(debugViewMode)
        };
        vkCmdPushConstants(cmdBuffer, mPipelineLayout->GetHandle(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

        VkDescriptorSet descriptorSet = mDescriptorSets[frameSlot];
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout->GetHandle(),
                                0, 1, &descriptorSet, 0, nullptr);
        vkCmdDraw(cmdBuffer, 3, 1, 0, 0);

        renderTarget->End(cmdBuffer);
    }

    void AdGBufferDebugCompositePass::InvalidateSourceViews() {
        std::fill(mSourceImages.begin(), mSourceImages.end(), std::array<VkImage, 3>{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE });
        mSourceViews.clear();
        mSourceViews.resize(mFramesInFlight);
    }

    void AdGBufferDebugCompositePass::UpdateDescriptor(uint32_t frameSlot, AdVKImage *baseColor, AdVKImage *normal, AdVKImage *material) {
        if(!baseColor || !normal || !material || frameSlot >= mSourceImages.size()){
            return;
        }

        std::array<AdVKImage*, 3> sourceImages = { baseColor, normal, material };
        std::array<VkImage, 3> sourceHandles = {
            baseColor->GetHandle(),
            normal->GetHandle(),
            material->GetHandle()
        };
        if(mSourceImages[frameSlot] == sourceHandles
           && mSourceViews[frameSlot][0]
           && mSourceViews[frameSlot][1]
           && mSourceViews[frameSlot][2]){
            return;
        }

        AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        AdVKDevice *device = renderCxt->GetDevice();

        mSourceImages[frameSlot] = sourceHandles;
        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(3);
        std::array<VkDescriptorImageInfo, 3> imageInfos{};
        for(uint32_t i = 0; i < sourceImages.size(); i++){
            mSourceViews[frameSlot][i] = std::make_shared<AdVKImageView>(device, sourceImages[i]->GetHandle(), sourceImages[i]->GetFormat(), VK_IMAGE_ASPECT_COLOR_BIT);
            imageInfos[i] = DescriptorSetWriter::BuildImageInfo(mSampler->GetHandle(), mSourceViews[frameSlot][i]->GetHandle());
            writes.push_back(DescriptorSetWriter::WriteImage(mDescriptorSets[frameSlot], i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfos[i]));
        }
        DescriptorSetWriter::UpdateDescriptorSets(device->GetHandle(), writes);
    }
}
