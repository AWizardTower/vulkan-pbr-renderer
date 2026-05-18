#ifndef AD_SHADOW_DEBUG_COMPOSITE_PASS_H
#define AD_SHADOW_DEBUG_COMPOSITE_PASS_H

#include "AdGraphicContext.h"
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

    enum class AdShadowDebugViewMode : uint32_t{
        Lit = 1,
        ShadowMap = 2,
        ShadowFactor = 3,
        BaseColor = 4,
        Normal = 5,
        Roughness = 6,
        Metallic = 7
    };

    struct AdShadowDebugSettings{
        glm::mat4 lightViewProj{ 1.f };
        glm::vec4 shadowParams{ 1.f / 2048.f, 0.0015f, 0.f, 0.f };
        glm::vec4 debugParams{ static_cast<float>(AdShadowDebugViewMode::Lit), 0.f, 0.f, 0.f };
    };

    class AdShadowDebugCompositePass{
    public:
        AdShadowDebugCompositePass(AdVKRenderPass *renderPass, uint32_t framesInFlight);
        ~AdShadowDebugCompositePass();

        void Render(VkCommandBuffer cmdBuffer,
                    AdRenderTarget *renderTarget,
                    uint32_t frameSlot,
                    uint32_t imageIndex,
                    AdVKImage *lightingColor,
                    AdVKImage *baseColor,
                    AdVKImage *normal,
                    AdVKImage *material,
                    AdVKImage *worldPosition,
                    AdVKImage *shadowDepth,
                    const AdShadowDebugSettings &settings);
        void InvalidateSourceViews();
    private:
        void UpdateDescriptor(uint32_t frameSlot,
                              AdVKImage *lightingColor,
                              AdVKImage *baseColor,
                              AdVKImage *normal,
                              AdVKImage *material,
                              AdVKImage *worldPosition,
                              AdVKImage *shadowDepth);

        uint32_t mFramesInFlight = 0;
        std::shared_ptr<AdVKDescriptorSetLayout> mDescriptorSetLayout;
        std::shared_ptr<AdVKDescriptorPool> mDescriptorPool;
        std::vector<VkDescriptorSet> mDescriptorSets;
        std::shared_ptr<AdSampler> mSampler;
        std::shared_ptr<AdVKPipelineLayout> mPipelineLayout;
        std::shared_ptr<AdVKPipeline> mPipeline;
        std::vector<std::array<VkImage, 6>> mSourceImages;
        std::vector<std::array<std::shared_ptr<AdVKImageView>, 6>> mSourceViews;
    };
}

#endif
