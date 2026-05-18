#ifndef AD_IBL_DEBUG_COMPOSITE_PASS_H
#define AD_IBL_DEBUG_COMPOSITE_PASS_H

#include "AdGraphicContext.h"
#include "Graphic/AdVKCommon.h"
#include <array>
#include <memory>
#include <vector>

namespace ade{
    class AdHDRTexture;
    class AdRenderTarget;
    class AdSampler;
    class AdVKBuffer;
    class AdVKDescriptorPool;
    class AdVKDescriptorSetLayout;
    class AdVKImage;
    class AdVKImageView;
    class AdVKPipeline;
    class AdVKPipelineLayout;
    class AdVKRenderPass;

    enum class AdIBLDebugViewMode : uint32_t{
        Lit = 1,
        Environment = 2,
        IBLDiffuse = 3,
        IBLSpecular = 4,
        ShadowFactor = 5,
        BaseColor = 6,
        Normal = 7,
        Roughness = 8,
        Metallic = 9
    };

    struct AdIBLDebugSettings{
        glm::mat4 inverseViewProj{ 1.f };
        glm::mat4 lightViewProj{ 1.f };
        glm::vec4 cameraPosition{ 0.f, 0.f, 2.3f, 0.f };
        glm::vec4 debugParams{ static_cast<float>(AdIBLDebugViewMode::Lit), 1.f, 2.2f, 0.f };
        glm::vec4 shadowParams{ 1.f / 2048.f, 0.0015f, 0.f, 0.f };
        glm::vec4 iblParams{ 0.55f, 1.0f, 0.f, 0.f };
    };

    class AdIBLDebugCompositePass{
    public:
        AdIBLDebugCompositePass(AdVKRenderPass *renderPass, uint32_t framesInFlight);
        ~AdIBLDebugCompositePass();

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
                    AdHDRTexture *environmentMap,
                    const AdIBLDebugSettings &settings);
        void InvalidateSourceViews();
    private:
        void UpdateDescriptor(uint32_t frameSlot,
                              AdVKImage *lightingColor,
                              AdVKImage *baseColor,
                              AdVKImage *normal,
                              AdVKImage *material,
                              AdVKImage *worldPosition,
                              AdVKImage *shadowDepth,
                              AdHDRTexture *environmentMap,
                              const AdIBLDebugSettings &settings);

        uint32_t mFramesInFlight = 0;
        std::shared_ptr<AdVKDescriptorSetLayout> mDescriptorSetLayout;
        std::shared_ptr<AdVKDescriptorPool> mDescriptorPool;
        std::vector<VkDescriptorSet> mDescriptorSets;
        std::shared_ptr<AdSampler> mSampler;
        std::shared_ptr<AdVKPipelineLayout> mPipelineLayout;
        std::shared_ptr<AdVKPipeline> mPipeline;
        std::vector<std::array<VkImage, 7>> mSourceImages;
        std::vector<std::array<std::shared_ptr<AdVKImageView>, 6>> mSourceViews;
        std::vector<std::shared_ptr<AdVKBuffer>> mSettingsBuffers;
    };
}

#endif
