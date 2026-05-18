#include "Render/AdRenderer.h"
#include "AdApplication.h"
#include "Graphic/AdVKDebugUtils.h"
#include "Graphic/AdVKQueue.h"

namespace ade{
    AdRenderer::AdRenderer() {
        ade::AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        ade::AdVKDevice *device = renderCxt->GetDevice();
        ade::AdVKSwapchain *swapchain = renderCxt->GetSwapchain();

        mFrameResources.resize(RENDERER_NUM_BUFFER);
        VkSemaphoreCreateInfo semaphoreInfo = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0
        };
        VkFenceCreateInfo fenceInfo = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .pNext = nullptr,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT

        };
        for(int i = 0; i < RENDERER_NUM_BUFFER; i++){
            AdFrameResource &frame = mFrameResources[i];
            frame.FrameSlot = i;
            frame.GpuProfiler = std::make_shared<AdVKGpuProfiler>(device);
            CALL_VK(vkCreateSemaphore(device->GetHandle(), &semaphoreInfo, nullptr, &frame.ImageAvailable));
            CALL_VK(vkCreateFence(device->GetHandle(), &fenceInfo, nullptr, &frame.FrameFence));
        }
        CreateSwapchainImageSemaphores(device, swapchain->GetImages().size());

        AdAppContext *appContext = AdApplication::GetAppContext();
        if(appContext){
            appContext->renderer = this;
        }
    }

    AdRenderer::~AdRenderer() {
        ade::AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        ade::AdVKDevice *device = renderCxt->GetDevice();

        if(device){
            CALL_VK(vkDeviceWaitIdle(device->GetHandle()));
        }
        FlushDeferredDeletes();
        DestroySwapchainImageSemaphores(device);

        for (auto &frame: mFrameResources){
            VK_D(Semaphore, device->GetHandle(), frame.ImageAvailable);
            VK_D(Fence, device->GetHandle(), frame.FrameFence);
            frame.GpuProfiler.reset();
        }

        AdAppContext *appContext = AdApplication::GetAppContext();
        if(appContext && appContext->renderer == this){
            appContext->renderer = nullptr;
        }
    }

    bool AdRenderer::Begin(int32_t *outImageIndex) {
        ade::AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        ade::AdVKDevice *device = renderCxt->GetDevice();
        ade::AdVKSwapchain *swapchain = renderCxt->GetSwapchain();
        AdFrameResource &frame = mFrameResources[mCurrentFrameSlot];

        bool bShouldUpdateTarget = false;

        CALL_VK(vkWaitForFences(device->GetHandle(), 1, &frame.FrameFence, VK_TRUE, UINT64_MAX));
        if(frame.GpuProfiler){
            frame.GpuProfiler->ResolveFrame();
            const float gpuFrameTimeMs = frame.GpuProfiler->GetLastFrameTimeMs();
            if(gpuFrameTimeMs > 0.0f && frame.FrameNumber % 6000 == 0){
                LOG_D("GPU Frame {0}: {1} ms", frame.FrameNumber, gpuFrameTimeMs);
            }
        }
        ExecuteDeferredDeletes(frame);
        CALL_VK(vkResetFences(device->GetHandle(), 1, &frame.FrameFence));

        VkResult ret = swapchain->AcquireImage(outImageIndex, frame.ImageAvailable);
        if(ret == VK_ERROR_OUT_OF_DATE_KHR){
            bShouldUpdateTarget = ReCreateSwapchainIfNeeded(device, swapchain);
            ret = swapchain->AcquireImage(outImageIndex, frame.ImageAvailable);
            if(ret != VK_SUCCESS && ret != VK_SUBOPTIMAL_KHR){
                LOG_E("Recreate swapchain error: {0}", vk_result_string(ret));
            }
        }
        if(ret == VK_SUCCESS || ret == VK_SUBOPTIMAL_KHR){
            frame.FrameNumber = mFrameNumber;
            frame.ImageIndex = *outImageIndex;
            frame.RenderFinished = GetRenderFinishedSemaphore(*outImageIndex);
        }
        return bShouldUpdateTarget;
    }

    bool AdRenderer::End(int32_t imageIndex, const std::vector<VkCommandBuffer> &cmdBuffers) {
        ade::AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        ade::AdVKDevice *device = renderCxt->GetDevice();
        ade::AdVKSwapchain *swapchain = renderCxt->GetSwapchain();
        AdFrameResource &frame = mFrameResources[mCurrentFrameSlot];
        bool bShouldUpdateTarget = false;

        VkSemaphore renderFinished = GetRenderFinishedSemaphore(imageIndex);
        frame.RenderFinished = renderFinished;

        device->GetFirstGraphicQueue()->Submit(cmdBuffers, { frame.ImageAvailable }, { renderFinished }, frame.FrameFence);

        VkResult ret = swapchain->Present(imageIndex, { renderFinished });
        if(ret == VK_SUBOPTIMAL_KHR || ret == VK_ERROR_OUT_OF_DATE_KHR){
            bShouldUpdateTarget = ReCreateSwapchainIfNeeded(device, swapchain);
        }

        mCurrentFrameSlot = (mCurrentFrameSlot + 1) % GetFramesInFlight();
        mFrameNumber++;
        return bShouldUpdateTarget;
    }

    void AdRenderer::EnqueueDeferredDelete(std::function<void()> task) {
        if(!task){
            return;
        }
        if(mFrameResources.empty()){
            task();
            return;
        }
        mFrameResources[mCurrentFrameSlot].DeferredDeletes.push_back(std::move(task));
    }

    void AdRenderer::BeginFrameScope(VkCommandBuffer cmdBuffer) {
        if(mFrameResources.empty()){
            return;
        }
        AdFrameResource &frame = mFrameResources[mCurrentFrameSlot];
        AdVKDebugUtils::BeginLabel(cmdBuffer, "Frame");
        if(frame.GpuProfiler){
            frame.GpuProfiler->BeginFrame(cmdBuffer);
            frame.GpuProfiler->BeginScope(cmdBuffer, "Frame");
        }
    }

    void AdRenderer::EndFrameScope(VkCommandBuffer cmdBuffer) {
        if(mFrameResources.empty()){
            return;
        }
        AdFrameResource &frame = mFrameResources[mCurrentFrameSlot];
        if(frame.GpuProfiler){
            frame.GpuProfiler->EndScope(cmdBuffer);
        }
        AdVKDebugUtils::EndLabel(cmdBuffer);
    }

    void AdRenderer::ExecuteDeferredDeletes(AdFrameResource &frame) {
        if(frame.DeferredDeletes.empty()){
            return;
        }

        auto deletes = std::move(frame.DeferredDeletes);
        frame.DeferredDeletes.clear();
        for(auto &task: deletes){
            task();
        }
    }

    void AdRenderer::FlushDeferredDeletes() {
        for(auto &frame: mFrameResources){
            ExecuteDeferredDeletes(frame);
        }
    }

    bool AdRenderer::ReCreateSwapchainIfNeeded(AdVKDevice *device, AdVKSwapchain *swapchain) {
        CALL_VK(vkDeviceWaitIdle(device->GetHandle()));
        FlushDeferredDeletes();

        VkExtent2D originExtent = { swapchain->GetWidth(), swapchain->GetHeight() };
        bool bSuc = swapchain->ReCreate();
        if(bSuc){
            DestroySwapchainImageSemaphores(device);
            CreateSwapchainImageSemaphores(device, swapchain->GetImages().size());
        }

        VkExtent2D newExtent = { swapchain->GetWidth(), swapchain->GetHeight() };
        return bSuc && (originExtent.width != newExtent.width || originExtent.height != newExtent.height);
    }

    void AdRenderer::CreateSwapchainImageSemaphores(AdVKDevice *device, uint32_t imageCount) {
        mSwapchainImageRenderFinishedSemaphores.resize(imageCount, VK_NULL_HANDLE);
        VkSemaphoreCreateInfo semaphoreInfo = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0
        };
        for(auto &semaphore: mSwapchainImageRenderFinishedSemaphores){
            CALL_VK(vkCreateSemaphore(device->GetHandle(), &semaphoreInfo, nullptr, &semaphore));
        }
    }

    void AdRenderer::DestroySwapchainImageSemaphores(AdVKDevice *device) {
        if(!device){
            return;
        }
        for(auto &semaphore: mSwapchainImageRenderFinishedSemaphores){
            VK_D(Semaphore, device->GetHandle(), semaphore);
        }
        mSwapchainImageRenderFinishedSemaphores.clear();
        for(auto &frame: mFrameResources){
            frame.RenderFinished = VK_NULL_HANDLE;
        }
    }

    VkSemaphore AdRenderer::GetRenderFinishedSemaphore(int32_t imageIndex) const {
        if(imageIndex < 0 || imageIndex >= mSwapchainImageRenderFinishedSemaphores.size()){
            LOG_E("Invalid swapchain image index for render-finished semaphore: {0}", imageIndex);
            return VK_NULL_HANDLE;
        }
        return mSwapchainImageRenderFinishedSemaphores[imageIndex];
    }
}
