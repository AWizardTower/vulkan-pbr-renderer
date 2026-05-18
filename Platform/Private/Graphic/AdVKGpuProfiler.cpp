#include "Graphic/AdVKGpuProfiler.h"
#include "Graphic/AdVKDevice.h"

namespace ade{
    AdVKGpuProfiler::AdVKGpuProfiler(AdVKDevice *device, uint32_t maxScopeCount) {
        Init(device, maxScopeCount);
    }

    AdVKGpuProfiler::~AdVKGpuProfiler() {
        if(mDevice && mQueryPool != VK_NULL_HANDLE){
            vkDestroyQueryPool(mDevice->GetHandle(), mQueryPool, nullptr);
            mQueryPool = VK_NULL_HANDLE;
        }
    }

    void AdVKGpuProfiler::Init(AdVKDevice *device, uint32_t maxScopeCount) {
        static bool bTimestampWarningLogged = false;

        mDevice = device;
        mMaxScopeCount = maxScopeCount;
        mLastFrameTimeMs = 0.0f;
        mNextQuery = 0;
        mSubmittedQueryCount = 0;
        mActiveScopes.clear();
        mSubmittedScopes.clear();

        bSupported = device && device->IsTimestampQuerySupported();
        if(!bSupported){
            if(!bTimestampWarningLogged){
                LOG_W("GPU timestamp query is not supported on this device, profiler will be disabled.");
                bTimestampWarningLogged = true;
            }
            return;
        }

        VkQueryPoolCreateInfo queryPoolInfo = {
                .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .queryType = VK_QUERY_TYPE_TIMESTAMP,
                .queryCount = maxScopeCount * 2,
                .pipelineStatistics = 0
        };
        CALL_VK(vkCreateQueryPool(device->GetHandle(), &queryPoolInfo, nullptr, &mQueryPool));
        bSupported = mQueryPool != VK_NULL_HANDLE;
    }

    void AdVKGpuProfiler::BeginFrame(VkCommandBuffer cmdBuffer) {
        if(!bSupported || !cmdBuffer){
            return;
        }

        mNextQuery = 0;
        mSubmittedQueryCount = 0;
        mActiveScopes.clear();
        mSubmittedScopes.clear();
        mLastFrameTimeMs = 0.0f;
        vkCmdResetQueryPool(cmdBuffer, mQueryPool, 0, mMaxScopeCount * 2);
    }

    void AdVKGpuProfiler::BeginScope(VkCommandBuffer cmdBuffer, const char *name) {
        if(!bSupported || !cmdBuffer || !name){
            return;
        }
        if(mNextQuery + 1 >= mMaxScopeCount * 2){
            LOG_W("GPU profiler scope limit reached, scope '{0}' will be ignored.", name);
            return;
        }

        Scope scope = {
                .Name = name,
                .BeginQuery = mNextQuery++,
                .EndQuery = mNextQuery++
        };
        vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, mQueryPool, scope.BeginQuery);
        mActiveScopes.push_back(scope);
        mSubmittedQueryCount = mNextQuery;
    }

    void AdVKGpuProfiler::EndScope(VkCommandBuffer cmdBuffer) {
        if(!bSupported || !cmdBuffer || mActiveScopes.empty()){
            return;
        }

        Scope scope = mActiveScopes.back();
        mActiveScopes.pop_back();
        vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, mQueryPool, scope.EndQuery);
        mSubmittedScopes.push_back(scope);
        mSubmittedQueryCount = mNextQuery;
    }

    void AdVKGpuProfiler::ResolveFrame() {
        if(!bSupported || mSubmittedQueryCount == 0 || mSubmittedScopes.empty()){
            return;
        }

        std::vector<uint64_t> timestamps(mSubmittedQueryCount, 0);
        VkResult ret = vkGetQueryPoolResults(mDevice->GetHandle(), mQueryPool,
                                             0, mSubmittedQueryCount,
                                             sizeof(uint64_t) * timestamps.size(), timestamps.data(),
                                             sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
        if(ret == VK_NOT_READY){
            return;
        }
        if(ret != VK_SUCCESS){
            LOG_W("vkGetQueryPoolResults failed: {0}", vk_result_string(ret));
            return;
        }

        const float timestampPeriod = mDevice->GetTimestampPeriod();
        for(const auto &scope: mSubmittedScopes){
            if(scope.Name != "Frame"){
                continue;
            }
            const uint64_t begin = timestamps[scope.BeginQuery];
            const uint64_t end = timestamps[scope.EndQuery];
            if(end >= begin){
                mLastFrameTimeMs = static_cast<float>(end - begin) * timestampPeriod / 1000000.0f;
            }
            break;
        }
    }
}
