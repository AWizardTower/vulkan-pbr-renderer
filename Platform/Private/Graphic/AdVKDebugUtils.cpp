#include "Graphic/AdVKDebugUtils.h"

namespace ade{
    namespace{
        PFN_vkCmdBeginDebugUtilsLabelEXT gCmdBeginDebugUtilsLabel = nullptr;
        PFN_vkCmdEndDebugUtilsLabelEXT gCmdEndDebugUtilsLabel = nullptr;
        PFN_vkCmdInsertDebugUtilsLabelEXT gCmdInsertDebugUtilsLabel = nullptr;
        bool gDebugUtilsInitialized = false;
    }

    void AdVKDebugUtils::Init(VkInstance instance) {
        if(!instance){
            return;
        }

        gCmdBeginDebugUtilsLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
                vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
        gCmdEndDebugUtilsLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
                vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));
        gCmdInsertDebugUtilsLabel = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
                vkGetInstanceProcAddr(instance, "vkCmdInsertDebugUtilsLabelEXT"));
        gDebugUtilsInitialized = true;

        if(IsSupported()){
            LOG_D("VK_EXT_debug_utils command labels enabled.");
        } else {
            LOG_W("VK_EXT_debug_utils is not available, command labels will be disabled.");
        }
    }

    bool AdVKDebugUtils::IsSupported() {
        return gDebugUtilsInitialized
            && gCmdBeginDebugUtilsLabel
            && gCmdEndDebugUtilsLabel
            && gCmdInsertDebugUtilsLabel;
    }

    void AdVKDebugUtils::BeginLabel(VkCommandBuffer cmdBuffer, const char *name) {
        if(!IsSupported() || !cmdBuffer || !name){
            return;
        }

        VkDebugUtilsLabelEXT label = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                .pNext = nullptr,
                .pLabelName = name,
                .color = { 0.2f, 0.6f, 1.0f, 1.0f }
        };
        gCmdBeginDebugUtilsLabel(cmdBuffer, &label);
    }

    void AdVKDebugUtils::EndLabel(VkCommandBuffer cmdBuffer) {
        if(!IsSupported() || !cmdBuffer){
            return;
        }
        gCmdEndDebugUtilsLabel(cmdBuffer);
    }

    void AdVKDebugUtils::InsertLabel(VkCommandBuffer cmdBuffer, const char *name) {
        if(!IsSupported() || !cmdBuffer || !name){
            return;
        }

        VkDebugUtilsLabelEXT label = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                .pNext = nullptr,
                .pLabelName = name,
                .color = { 0.8f, 0.8f, 0.2f, 1.0f }
        };
        gCmdInsertDebugUtilsLabel(cmdBuffer, &label);
    }
}
