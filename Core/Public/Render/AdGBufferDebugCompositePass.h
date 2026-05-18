#ifndef AD_GBUFFER_DEBUG_COMPOSITE_PASS_H
#define AD_GBUFFER_DEBUG_COMPOSITE_PASS_H

#include "Graphic/AdVKCommon.h"
#include <array>
#include <memory>
#include <vector>

namespace ade{
    class AdVKDescriptorPool;
    class AdVKDescriptorSetLayout;
    class AdVKImage;
    class AdVKImageView;
    class AdVKPipeline;
    class AdVKPipelineLayout;
    class AdVKRenderPass;
    class AdRenderTarget;
    class AdSampler;

    enum class AdGBufferDebugViewMode : uint32_t{
        BaseColor = 1,
        Normal = 2,
        Roughness = 3,
        Metallic = 4,
        DepthPlaceholder = 5
    };

    class AdGBufferDebugCompositePass{
    public:
        AdGBufferDebugCompositePass(AdVKRenderPass *renderPass, uint32_t framesInFlight);
        ~AdGBufferDebugCompositePass();

        void Render(VkCommandBuffer cmdBuffer,
                    AdRenderTarget *renderTarget,
                    uint32_t frameSlot,
                    uint32_t imageIndex,
                    AdVKImage *baseColor,
                    AdVKImage *normal,
                    AdVKImage *material,
                    AdGBufferDebugViewMode debugViewMode);
        void InvalidateSourceViews();
    private:
        void UpdateDescriptor(uint32_t frameSlot, AdVKImage *baseColor, AdVKImage *normal, AdVKImage *material);

        struct DebugPushConstants{
            uint32_t debugViewMode = static_cast<uint32_t>(AdGBufferDebugViewMode::BaseColor);
        };

        uint32_t mFramesInFlight = 0;
        std::shared_ptr<AdVKDescriptorSetLayout> mDescriptorSetLayout;
        std::shared_ptr<AdVKDescriptorPool> mDescriptorPool;
        std::vector<VkDescriptorSet> mDescriptorSets;
        std::shared_ptr<AdSampler> mSampler;
        std::shared_ptr<AdVKPipelineLayout> mPipelineLayout;
        std::shared_ptr<AdVKPipeline> mPipeline;
        std::vector<std::array<VkImage, 3>> mSourceImages;
        std::vector<std::array<std::shared_ptr<AdVKImageView>, 3>> mSourceViews;
    };
}

#endif
