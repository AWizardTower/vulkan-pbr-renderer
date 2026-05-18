#include "Graphic/AdVKImageView.h"
#include "Graphic/AdVKDevice.h"

namespace ade{
    AdVKImageView::AdVKImageView(AdVKDevice *device,
                                 VkImage image,
                                 VkFormat format,
                                 VkImageAspectFlags aspectFlags,
                                 VkImageViewType viewType,
                                 uint32_t baseMipLevel,
                                 uint32_t mipLevelCount,
                                 uint32_t baseArrayLayer,
                                 uint32_t layerCount) : mDevice(device) {
        VkImageViewCreateInfo imageViewInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .image = image,
                .viewType = viewType,
                .format = format,
                .components = {
                        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
                },
                .subresourceRange = {
                        .aspectMask = aspectFlags,
                        .baseMipLevel = baseMipLevel,
                        .levelCount = mipLevelCount,
                        .baseArrayLayer = baseArrayLayer,
                        .layerCount = layerCount
                }
        };
        CALL_VK(vkCreateImageView(device->GetHandle(), &imageViewInfo, nullptr, &mHandle));
    }

    AdVKImageView::~AdVKImageView() {
        VK_D(ImageView, mDevice->GetHandle(), mHandle);
    }
}
