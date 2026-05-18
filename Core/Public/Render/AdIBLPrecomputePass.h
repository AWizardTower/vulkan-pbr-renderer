#ifndef AD_IBL_PRECOMPUTE_PASS_H
#define AD_IBL_PRECOMPUTE_PASS_H

#include "AdGraphicContext.h"
#include "Graphic/AdVKCommon.h"
#include <memory>
#include <vector>

namespace ade{
    class AdHDRTexture;
    class AdIBLResources;
    class AdRenderer;
    class AdVKDescriptorPool;
    class AdVKDescriptorSetLayout;
    class AdVKDevice;
    class AdVKImage;
    class AdVKImageView;
    class AdVKPipeline;
    class AdVKPipelineLayout;
    class AdVKRenderPass;

    class AdIBLPrecomputePass{
    public:
        explicit AdIBLPrecomputePass(VkFormat colorFormat);
        ~AdIBLPrecomputePass();

        void Precompute(VkCommandBuffer cmdBuffer,
                        AdRenderer *renderer,
                        AdHDRTexture *environmentHDR,
                        AdIBLResources *resources);
    private:
        struct SubresourceTarget{
            std::shared_ptr<AdVKImageView> View;
            VkFramebuffer Framebuffer = VK_NULL_HANDLE;
            uint32_t Width = 0;
            uint32_t Height = 0;
        };

        void CreateRenderPass(VkFormat colorFormat);
        void CreateDescriptors();
        void CreatePipelines();
        void EnsureTargets(AdIBLResources *resources);
        void DestroyTargets(std::vector<SubresourceTarget> &targets);
        SubresourceTarget CreateTarget(AdVKImage *image, uint32_t width, uint32_t height, uint32_t mipLevel, uint32_t arrayLayer);
        void BeginTarget(VkCommandBuffer cmdBuffer, const SubresourceTarget &target);
        void EndTarget(VkCommandBuffer cmdBuffer);
        void RenderFullscreen(VkCommandBuffer cmdBuffer, const SubresourceTarget &target);
        void UpdateDescriptors(AdHDRTexture *environmentHDR, AdIBLResources *resources);
        void RunEquirectToCubemap(VkCommandBuffer cmdBuffer, AdIBLResources *resources);
        void RunIrradianceConvolution(VkCommandBuffer cmdBuffer, AdIBLResources *resources);
        void RunPrefilterSpecular(VkCommandBuffer cmdBuffer, AdIBLResources *resources);
        void RunBRDFLUT(VkCommandBuffer cmdBuffer, AdIBLResources *resources);

        AdVKDevice *mDevice = nullptr;
        std::shared_ptr<AdVKRenderPass> mColorRenderPass;
        std::shared_ptr<AdVKDescriptorSetLayout> mEquirectDescriptorSetLayout;
        std::shared_ptr<AdVKDescriptorSetLayout> mCubeDescriptorSetLayout;
        std::shared_ptr<AdVKDescriptorPool> mDescriptorPool;
        VkDescriptorSet mEquirectDescriptorSet = VK_NULL_HANDLE;
        VkDescriptorSet mEnvironmentCubeDescriptorSet = VK_NULL_HANDLE;

        std::shared_ptr<AdVKPipelineLayout> mEquirectPipelineLayout;
        std::shared_ptr<AdVKPipelineLayout> mIrradiancePipelineLayout;
        std::shared_ptr<AdVKPipelineLayout> mPrefilterPipelineLayout;
        std::shared_ptr<AdVKPipelineLayout> mBRDFLUTPipelineLayout;
        std::shared_ptr<AdVKPipeline> mEquirectPipeline;
        std::shared_ptr<AdVKPipeline> mIrradiancePipeline;
        std::shared_ptr<AdVKPipeline> mPrefilterPipeline;
        std::shared_ptr<AdVKPipeline> mBRDFLUTPipeline;

        VkImage mEnvironmentTargetImage = VK_NULL_HANDLE;
        VkImage mIrradianceTargetImage = VK_NULL_HANDLE;
        VkImage mPrefilterTargetImage = VK_NULL_HANDLE;
        VkImage mBRDFLUTTargetImage = VK_NULL_HANDLE;
        std::vector<SubresourceTarget> mEnvironmentTargets;
        std::vector<SubresourceTarget> mIrradianceTargets;
        std::vector<SubresourceTarget> mPrefilterTargets;
        std::vector<SubresourceTarget> mBRDFLUTTargets;
    };
}

#endif
