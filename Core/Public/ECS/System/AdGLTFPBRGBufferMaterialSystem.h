#ifndef AD_GLTF_PBR_GBUFFER_MATERIAL_SYSTEM_H
#define AD_GLTF_PBR_GBUFFER_MATERIAL_SYSTEM_H

#include "AdGraphicContext.h"
#include "ECS/Component/AdGLTFModelComponent.h"
#include "ECS/Component/AdTransformComponent.h"
#include "ECS/System/AdMaterialSystem.h"
#include "Render/AdGLTFModel.h"

#include <unordered_map>

namespace ade{
    class AdVKBuffer;
    class AdVKDescriptorPool;
    class AdVKDescriptorSetLayout;
    class AdVKPipeline;
    class AdVKPipelineLayout;

    struct GLTFPBRGBufferPushConstants{
        glm::mat4 mvp{ 1.f };
        glm::vec4 worldRows[3]{
            glm::vec4(1.f, 0.f, 0.f, 0.f),
            glm::vec4(0.f, 1.f, 0.f, 0.f),
            glm::vec4(0.f, 0.f, 1.f, 0.f)
        };
        glm::vec4 debugFeatureFlags{ 1.f, 1.f, 0.f, 0.f };
    };

    static_assert(sizeof(GLTFPBRGBufferPushConstants) <= 128, "glTF GBuffer push constants must stay within Vulkan's guaranteed 128-byte limit.");

    struct GLTFPBRMaterialParams{
        glm::vec4 baseColorFactor{ 1.f };
        glm::vec4 pbrParams{ 1.f, 1.f, 1.f, 1.f };
        glm::vec4 featureFlags{ 0.f };
    };

    class AdGLTFPBRGBufferMaterialSystem : public AdMaterialSystem{
    public:
        void OnInit(AdVKRenderPass *renderPass) override;
        void OnRender(VkCommandBuffer cmdBuffer, AdRenderTarget *renderTarget) override;
        void OnDestroy() override;

        void SetDebugFeatureFlags(bool normalMapEnabled, bool ambientOcclusionEnabled);
    private:
        struct MaterialDescriptor{
            std::shared_ptr<AdVKBuffer> ParamBuffer;
            VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;
        };

        VkDescriptorSet EnsureMaterialDescriptor(const AdGLTFPBRMaterial *material);

        std::shared_ptr<AdVKDescriptorSetLayout> mMaterialDescSetLayout;
        std::shared_ptr<AdVKDescriptorPool> mDescriptorPool;
        std::shared_ptr<AdVKPipelineLayout> mPipelineLayout;
        std::shared_ptr<AdVKPipeline> mPipeline;
        std::unordered_map<const AdGLTFPBRMaterial*, MaterialDescriptor> mMaterialDescriptors;
        uint32_t mAllocatedMaterialDescriptors = 0;
        bool mDebugNormalMapEnabled = true;
        bool mDebugAmbientOcclusionEnabled = true;
    };
}

#endif
