#ifndef AD_GBUFFER_MATERIAL_SYSTEM_H
#define AD_GBUFFER_MATERIAL_SYSTEM_H

#include "ECS/System/AdMaterialSystem.h"
#include "ECS/Component/AdTransformComponent.h"
#include "ECS/Component/Material/AdBaseMaterialComponent.h"
#include "AdGraphicContext.h"

namespace ade{
    class AdVKPipelineLayout;
    class AdVKPipeline;

    struct GBufferPushConstants{
        glm::mat4 matrix{ 1.f };
        glm::vec4 normalCols[3]{
            glm::vec4(1.f, 0.f, 0.f, 0.f),
            glm::vec4(0.f, 1.f, 0.f, 0.f),
            glm::vec4(0.f, 0.f, 1.f, 0.f)
        };
        glm::vec4 params{ 0.f, 0.5f, 0.f, 0.f };
    };

    class AdGBufferMaterialSystem : public AdMaterialSystem{
    public:
        void OnInit(AdVKRenderPass *renderPass) override;
        void OnRender(VkCommandBuffer cmdBuffer, AdRenderTarget *renderTarget) override;
        void OnDestroy() override;
    private:
        std::shared_ptr<AdVKPipelineLayout> mPipelineLayout;
        std::shared_ptr<AdVKPipeline> mPipeline;
    };
}

#endif
