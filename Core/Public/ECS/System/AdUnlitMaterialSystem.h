#ifndef ADUNLITMATERIALSYSTEM_H
#define ADUNLITMATERIALSYSTEM_H

#include "ECS/System/AdMaterialSystem.h"
#include "ECS/Component/Material/AdUnlitMaterialComponent.h"
#include <limits>

namespace ade{
#define NUM_MATERIAL_BATCH              16
#define NUM_MATERIAL_BATCH_MAX          2048

    class AdVKPipelineLayout;
    class AdVKPipeline;
    class AdVKDescriptorSetLayout;
    class AdVKDescriptorPool;
    class AdVKBuffer;

    class AdUnlitMaterialSystem : public AdMaterialSystem{
    public:
        void OnInit(AdVKRenderPass *renderPass) override;
        void OnRender(VkCommandBuffer cmdBuffer, AdRenderTarget *renderTarget) override;
        void OnDestroy() override;
    private:
        struct PerFrameDescriptors{
            std::shared_ptr<AdVKDescriptorPool> FrameDescriptorPool;
            VkDescriptorSet FrameUboDescSet = VK_NULL_HANDLE;
            std::shared_ptr<AdVKBuffer> FrameUboBuffer;

            std::shared_ptr<AdVKDescriptorPool> MaterialDescriptorPool;
            std::vector<VkDescriptorSet> MaterialParamDescSets;
            std::vector<VkDescriptorSet> MaterialResourceDescSets;
            std::vector<std::shared_ptr<AdVKBuffer>> MaterialBuffers;
            std::vector<uint64_t> SyncedParamsVersions;
            std::vector<uint64_t> SyncedResourceVersions;
        };

        void ReCreateMaterialDescPool(uint32_t materialCount);
        void UpdateFrameUboDescSet(PerFrameDescriptors &frame, AdRenderTarget *renderTarget);
        void UpdateMaterialParamsDescSet(PerFrameDescriptors &frame, VkDescriptorSet descSet, AdUnlitMaterial *material);
        void UpdateMaterialResourceDescSet(VkDescriptorSet descSet, AdUnlitMaterial *material);

        std::shared_ptr<AdVKDescriptorSetLayout> mFrameUboDescSetLayout;
        std::shared_ptr<AdVKDescriptorSetLayout> mMaterialParamDescSetLayout;
        std::shared_ptr<AdVKDescriptorSetLayout> mMaterialResourceDescSetLayout;

        std::shared_ptr<AdVKPipelineLayout> mPipelineLayout;
        std::shared_ptr<AdVKPipeline> mPipeline;

        std::vector<PerFrameDescriptors> mFrameDescriptors;
        uint32_t mLastDescriptorSetCount = 0;
    };
}

#endif
