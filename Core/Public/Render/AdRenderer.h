#ifndef ADRENDERER_H
#define ADRENDERER_H

#include "AdRenderContext.h"
#include "Graphic/AdVKGpuProfiler.h"
#include <functional>

namespace ade{
#define RENDERER_NUM_BUFFER     2

    struct AdFrameResource{
        uint32_t FrameSlot = 0;
        uint64_t FrameNumber = 0;
        int32_t ImageIndex = -1;
        VkSemaphore ImageAvailable = VK_NULL_HANDLE;
        VkSemaphore RenderFinished = VK_NULL_HANDLE;
        VkFence FrameFence = VK_NULL_HANDLE;
        std::vector<std::function<void()>> DeferredDeletes;
        std::shared_ptr<AdVKGpuProfiler> GpuProfiler;
    };

    class AdRenderer{
    public:
        AdRenderer();
        ~AdRenderer();

        bool Begin(int32_t *outImageIndex);
        bool End(int32_t imageIndex, const std::vector<VkCommandBuffer> &cmdBuffers);

        uint32_t GetCurrentFrameSlot() const { return mCurrentFrameSlot; }
        uint64_t GetCurrentFrameNumber() const { return mFrameNumber; }
        const AdFrameResource &GetCurrentFrameResource() const { return mFrameResources[mCurrentFrameSlot]; }
        uint32_t GetFramesInFlight() const { return static_cast<uint32_t>(mFrameResources.size()); }

        void EnqueueDeferredDelete(std::function<void()> task);
        void BeginGpuScope(VkCommandBuffer cmdBuffer, const char *name);
        void EndGpuScope(VkCommandBuffer cmdBuffer);
        void BeginFrameScope(VkCommandBuffer cmdBuffer);
        void EndFrameScope(VkCommandBuffer cmdBuffer);
    private:
        void ExecuteDeferredDeletes(AdFrameResource &frame);
        void FlushDeferredDeletes();
        bool ReCreateSwapchainIfNeeded(AdVKDevice *device, AdVKSwapchain *swapchain);
        void CreateSwapchainImageSemaphores(AdVKDevice *device, uint32_t imageCount);
        void DestroySwapchainImageSemaphores(AdVKDevice *device);
        VkSemaphore GetRenderFinishedSemaphore(int32_t imageIndex) const;

        uint32_t mCurrentFrameSlot = 0;
        uint64_t mFrameNumber = 0;
        std::vector<AdFrameResource> mFrameResources;
        std::vector<VkSemaphore> mSwapchainImageRenderFinishedSemaphores;
    };
}

#endif
