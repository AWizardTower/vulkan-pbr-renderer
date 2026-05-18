#ifndef AD_VK_DEBUG_UTILS_H
#define AD_VK_DEBUG_UTILS_H

#include "AdVKCommon.h"

namespace ade{
    class AdVKDebugUtils{
    public:
        static void Init(VkInstance instance);
        static bool IsSupported();

        static void BeginLabel(VkCommandBuffer cmdBuffer, const char *name);
        static void EndLabel(VkCommandBuffer cmdBuffer);
        static void InsertLabel(VkCommandBuffer cmdBuffer, const char *name);
    };
}

#endif
