#ifndef AD_PBR_DEBUG_COMPOSITE_PASS_H
#define AD_PBR_DEBUG_COMPOSITE_PASS_H

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

    enum class AdPBRDebugViewMode : uint32_t{
        Lit = 1,
        BaseColor = 2,
        Normal = 3,
        Roughness = 4,
        Metallic = 5
    };

    class AdPBRDebugCompositePass{
    public:
        AdPBRDebugCompositePass(AdVKRenderPass *renderPass, uint32_t framesInFlight);
        ~AdPBRDebugCompositePass();

        void Render(VkCommandBuffer cmdBuffer,
                    AdRenderTarget *renderTarget,
                    uint32_t frameSlot,
                    uint32_t imageIndex,
                    AdVKImage *lightingColor,
                    AdVKImage *baseColor,
                    AdVKImage *normal,
                    AdVKImage *material,
                    AdPBRDebugViewMode debugViewMode);
        void InvalidateSourceViews();
    private:
        void UpdateDescriptor(uint32_t frameSlot,
                              AdVKImage *lightingColor,
                              AdVKImage *baseColor,
                              AdVKImage *normal,
                              AdVKImage *material);

        struct DebugPushConstants{
            uint32_t debugViewMode = static_cast<uint32_t>(AdPBRDebugViewMode::Lit);
        };

        uint32_t mFramesInFlight = 0;
        std::shared_ptr<AdVKDescriptorSetLayout> mDescriptorSetLayout;
        std::shared_ptr<AdVKDescriptorPool> mDescriptorPool;
        std::vector<VkDescriptorSet> mDescriptorSets;
        std::shared_ptr<AdSampler> mSampler;
        std::shared_ptr<AdVKPipelineLayout> mPipelineLayout;
        std::shared_ptr<AdVKPipeline> mPipeline;
        std::vector<std::array<VkImage, 4>> mSourceImages;
        std::vector<std::array<std::shared_ptr<AdVKImageView>, 4>> mSourceViews;
    };
}

#endif
