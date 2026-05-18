#ifndef AD_FULLSCREEN_TEXTURE_PASS_H
#define AD_FULLSCREEN_TEXTURE_PASS_H

#include "Graphic/AdVKCommon.h"
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

    class AdFullscreenTexturePass{
    public:
        AdFullscreenTexturePass(AdVKRenderPass *renderPass, uint32_t framesInFlight);
        ~AdFullscreenTexturePass();

        void Render(VkCommandBuffer cmdBuffer, AdRenderTarget *renderTarget, uint32_t frameSlot, uint32_t imageIndex, AdVKImage *sourceImage);
        void InvalidateSourceViews();
    private:
        void UpdateDescriptor(uint32_t frameSlot, AdVKImage *sourceImage);

        uint32_t mFramesInFlight = 0;
        std::shared_ptr<AdVKDescriptorSetLayout> mDescriptorSetLayout;
        std::shared_ptr<AdVKDescriptorPool> mDescriptorPool;
        std::vector<VkDescriptorSet> mDescriptorSets;
        std::shared_ptr<AdSampler> mSampler;
        std::shared_ptr<AdVKPipelineLayout> mPipelineLayout;
        std::shared_ptr<AdVKPipeline> mPipeline;
        std::vector<VkImage> mSourceImages;
        std::vector<std::shared_ptr<AdVKImageView>> mSourceViews;
    };
}

#endif
