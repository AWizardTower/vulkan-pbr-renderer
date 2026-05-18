#ifndef AD_SHADOW_DEPTH_MATERIAL_SYSTEM_H
#define AD_SHADOW_DEPTH_MATERIAL_SYSTEM_H

#include "ECS/System/AdMaterialSystem.h"
#include "ECS/Component/AdTransformComponent.h"
#include "ECS/Component/Material/AdBaseMaterialComponent.h"
#include "AdGraphicContext.h"

namespace ade{
    class AdVKPipelineLayout;
    class AdVKPipeline;

    struct ShadowDepthPushConstants{
        glm::mat4 lightMVP{ 1.f };
    };

    class AdShadowDepthMaterialSystem : public AdMaterialSystem{
    public:
        explicit AdShadowDepthMaterialSystem(const glm::mat4 *lightViewProj = nullptr);

        void OnInit(AdVKRenderPass *renderPass) override;
        void OnRender(VkCommandBuffer cmdBuffer, AdRenderTarget *renderTarget) override;
        void OnDestroy() override;
    private:
        const glm::mat4 *mLightViewProj = nullptr;
        std::shared_ptr<AdVKPipelineLayout> mPipelineLayout;
        std::shared_ptr<AdVKPipeline> mPipeline;
    };
}

#endif
