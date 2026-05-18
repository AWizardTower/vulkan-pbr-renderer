#ifndef AD_IBL_DEFERRED_LIGHTING_PASS_H
#define AD_IBL_DEFERRED_LIGHTING_PASS_H

#include "AdGraphicContext.h"
#include "Graphic/AdVKCommon.h"
#include <array>
#include <memory>
#include <vector>

namespace ade{
    class AdHDRTexture;
    class AdRenderTarget;
    class AdSampler;
    class AdVKDescriptorPool;
    class AdVKDescriptorSetLayout;
    class AdVKImage;
    class AdVKImageView;
    class AdVKPipeline;
    class AdVKPipelineLayout;
    class AdVKRenderPass;

    struct AdIBLLightSettings{
        glm::mat4 lightViewProj{ 1.f };
        glm::vec4 cameraPosition{ 0.f, 0.f, 2.3f, 0.f };
        glm::vec4 lightDirection{ -0.35f, -0.8f, -0.45f, 0.f };
        glm::vec4 lightColorIntensity{ 1.f, 0.95f, 0.85f, 4.f };
        glm::vec4 iblParams{ 1.f / 2048.f, 0.0015f, 0.55f, 1.0f };
    };

    class AdIBLDeferredLightingPass{
    public:
        AdIBLDeferredLightingPass(AdVKRenderPass *renderPass, uint32_t framesInFlight);
        ~AdIBLDeferredLightingPass();

        void Render(VkCommandBuffer cmdBuffer,
                    AdRenderTarget *renderTarget,
                    uint32_t frameSlot,
                    AdVKImage *baseColor,
                    AdVKImage *normal,
                    AdVKImage *material,
                    AdVKImage *worldPosition,
                    AdVKImage *shadowDepth,
                    AdHDRTexture *environmentMap,
                    const AdIBLLightSettings &settings);
        void InvalidateSourceViews();
    private:
        void UpdateDescriptor(uint32_t frameSlot,
                              AdVKImage *baseColor,
                              AdVKImage *normal,
                              AdVKImage *material,
                              AdVKImage *worldPosition,
                              AdVKImage *shadowDepth,
                              AdHDRTexture *environmentMap);

        uint32_t mFramesInFlight = 0;
        std::shared_ptr<AdVKDescriptorSetLayout> mDescriptorSetLayout;
        std::shared_ptr<AdVKDescriptorPool> mDescriptorPool;
        std::vector<VkDescriptorSet> mDescriptorSets;
        std::shared_ptr<AdSampler> mSampler;
        std::shared_ptr<AdVKPipelineLayout> mPipelineLayout;
        std::shared_ptr<AdVKPipeline> mPipeline;
        std::vector<std::array<VkImage, 6>> mSourceImages;
        std::vector<std::array<std::shared_ptr<AdVKImageView>, 5>> mSourceViews;
    };
}

#endif
