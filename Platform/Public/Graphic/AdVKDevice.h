#ifndef AD_VK_DEVICE_H
#define AD_VK_DEVICE_H

#include "AdVKCommon.h"

namespace ade{
    class AdVKGraphicContext;
    class AdVKQueue;
    class AdVKCommandPool;

    struct AdVkSettings{
        VkFormat surfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        uint32_t swapchainImageCount = 3;
    };

    class AdVKDevice{
    public:
        AdVKDevice(AdVKGraphicContext *context, uint32_t graphicQueueCount, uint32_t presentQueueCount, const AdVkSettings &settings = {});
        ~AdVKDevice();

        VkDevice GetHandle() const { return mHandle; }
        VkPhysicalDevice GetPhysicalDevice() const;

        const AdVkSettings &GetSettings() const { return mSettings; }
        VkPipelineCache GetPipelineCache() const { return mPipelineCache; }
        const VkPhysicalDeviceProperties &GetPhysicalDeviceProperties() const { return mPhyDeviceProperties; }
        const VkQueueFamilyProperties &GetGraphicsQueueFamilyProperties() const { return mGraphicsQueueFamilyProperties; }
        bool IsTimestampQuerySupported() const { return bTimestampQuerySupported; }
        float GetTimestampPeriod() const { return mPhyDeviceProperties.limits.timestampPeriod; }

        AdVKQueue* GetGraphicQueue(uint32_t index) const { return mGraphicQueues.size() < index + 1 ? nullptr : mGraphicQueues[index].get(); };
        AdVKQueue* GetFirstGraphicQueue() const { return mGraphicQueues.empty() ? nullptr : mGraphicQueues[0].get(); };
        AdVKQueue* GetPresentQueue(uint32_t index) const { return mPresentQueues.size() < index + 1 ? nullptr : mPresentQueues[index].get(); };
        AdVKQueue* GetFirstPresentQueue() const { return mPresentQueues.empty() ? nullptr : mPresentQueues[0].get(); };
        AdVKCommandPool *GetDefaultCmdPool() const { return mDefaultCmdPool.get(); }

        int32_t GetMemoryIndex(VkMemoryPropertyFlags memProps, uint32_t memoryTypeBits) const;
        VkCommandBuffer CreateAndBeginOneCmdBuffer();
        void SubmitOneCmdBuffer(VkCommandBuffer cmdBuffer);

        VkResult CreateSimpleSampler(VkFilter filter,
                                     VkSamplerAddressMode addressMode,
                                     VkSampler *outSampler,
                                     VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                     float minLod = 0.f,
                                     float maxLod = 1.f);
    private:
        void CreatePipelineCache();
        void CreateDefaultCmdPool();

        VkDevice mHandle = VK_NULL_HANDLE;
        AdVKGraphicContext *mContext;

        std::vector<std::shared_ptr<AdVKQueue>> mGraphicQueues;
        std::vector<std::shared_ptr<AdVKQueue>> mPresentQueues;
        std::shared_ptr<AdVKCommandPool> mDefaultCmdPool;

        AdVkSettings mSettings;

        VkPipelineCache mPipelineCache = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties mPhyDeviceProperties{};
        VkQueueFamilyProperties mGraphicsQueueFamilyProperties{};
        bool bTimestampQuerySupported = false;
    };
}

#endif
