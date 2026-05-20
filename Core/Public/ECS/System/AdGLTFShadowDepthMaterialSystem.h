#ifndef AD_GLTF_SHADOW_DEPTH_MATERIAL_SYSTEM_H
#define AD_GLTF_SHADOW_DEPTH_MATERIAL_SYSTEM_H

#include "AdGraphicContext.h"
#include "ECS/Component/AdGLTFModelComponent.h"
#include "ECS/Component/AdTransformComponent.h"
#include "ECS/System/AdMaterialSystem.h"

namespace ade{
    class AdVKPipeline;
    class AdVKPipelineLayout;

    struct GLTFShadowDepthPushConstants{
        glm::mat4 lightMVP{ 1.f };
    };

    class AdGLTFShadowDepthMaterialSystem : public AdMaterialSystem{
    public:
        explicit AdGLTFShadowDepthMaterialSystem(const glm::mat4 *lightViewProj = nullptr);

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
