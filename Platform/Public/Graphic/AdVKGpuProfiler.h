#ifndef AD_VK_GPU_PROFILER_H
#define AD_VK_GPU_PROFILER_H

#include "AdVKCommon.h"
#include <string>
#include <vector>

namespace ade{
    class AdVKDevice;

    class AdVKGpuProfiler{
    public:
        AdVKGpuProfiler() = default;
        AdVKGpuProfiler(AdVKDevice *device, uint32_t maxScopeCount = 16);
        ~AdVKGpuProfiler();

        AdVKGpuProfiler(const AdVKGpuProfiler&) = delete;
        AdVKGpuProfiler &operator=(const AdVKGpuProfiler&) = delete;

        void Init(AdVKDevice *device, uint32_t maxScopeCount = 16);
        bool IsSupported() const { return bSupported; }

        void BeginFrame(VkCommandBuffer cmdBuffer);
        void BeginScope(VkCommandBuffer cmdBuffer, const char *name);
        void EndScope(VkCommandBuffer cmdBuffer);
        void ResolveFrame();

        float GetLastFrameTimeMs() const { return mLastFrameTimeMs; }
    private:
        struct Scope{
            std::string Name;
            uint32_t BeginQuery = 0;
            uint32_t EndQuery = 0;
        };

        AdVKDevice *mDevice = nullptr;
        VkQueryPool mQueryPool = VK_NULL_HANDLE;
        uint32_t mMaxScopeCount = 0;
        uint32_t mNextQuery = 0;
        uint32_t mSubmittedQueryCount = 0;
        std::vector<Scope> mActiveScopes;
        std::vector<Scope> mSubmittedScopes;
        float mLastFrameTimeMs = 0.0f;
        bool bSupported = false;
    };
}

#endif
