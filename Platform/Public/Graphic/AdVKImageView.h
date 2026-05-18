#ifndef ADVKIMAGEVIEW_H
#define ADVKIMAGEVIEW_H

#include "Graphic/AdVKCommon.h"

namespace ade{
    class AdVKDevice;

    class AdVKImageView{
    public:
        AdVKImageView(AdVKDevice *device,
                      VkImage image,
                      VkFormat format,
                      VkImageAspectFlags aspectFlags,
                      VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
                      uint32_t baseMipLevel = 0,
                      uint32_t mipLevelCount = 1,
                      uint32_t baseArrayLayer = 0,
                      uint32_t layerCount = 1);
        ~AdVKImageView();

        VkImageView GetHandle() const { return mHandle; }
    private:
        VkImageView mHandle = VK_NULL_HANDLE;

        AdVKDevice *mDevice;
    };
}

#endif
