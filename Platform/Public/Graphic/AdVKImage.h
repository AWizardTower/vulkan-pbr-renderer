#ifndef ADVKIMAGE_H
#define ADVKIMAGE_H

#include "AdVKCommon.h"

namespace ade{
    class AdVKDevice;
    class AdVKBuffer;

    class AdVKImage{
    public:
        AdVKImage(AdVKDevice *device,
                  VkExtent3D extent,
                  VkFormat format,
                  VkImageUsageFlags usage,
                  VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT,
                  uint32_t mipLevels = 1,
                  uint32_t arrayLayers = 1,
                  VkImageCreateFlags imageCreateFlags = 0);
        AdVKImage(AdVKDevice *device,
                  VkImage image,
                  VkExtent3D extent,
                  VkFormat format,
                  VkImageUsageFlags usage,
                  VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT,
                  uint32_t mipLevels = 1,
                  uint32_t arrayLayers = 1);
        ~AdVKImage();

        static bool TransitionLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                                     VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);
        static bool TransitionLayout(VkCommandBuffer cmdBuffer,
                                     VkImage image,
                                     VkImageLayout oldLayout,
                                     VkImageLayout newLayout,
                                     VkImageAspectFlags aspectMask,
                                     uint32_t baseMipLevel,
                                     uint32_t mipLevelCount,
                                     uint32_t baseArrayLayer,
                                     uint32_t layerCount);

        void CopyFromBuffer(VkCommandBuffer cmdBuffer, AdVKBuffer *buffer);
        void CopyFromBuffer(VkCommandBuffer cmdBuffer,
                            AdVKBuffer *buffer,
                            uint32_t mipLevel,
                            uint32_t baseArrayLayer,
                            uint32_t layerCount,
                            VkExtent3D extent);
        bool GenerateMipmaps2D(VkCommandBuffer cmdBuffer);
        bool GenerateMipmaps2DArray(VkCommandBuffer cmdBuffer,
                                    VkImageLayout baseMipOldLayout,
                                    VkImageLayout dstMipsOldLayout);

        VkFormat GetFormat() const { return mFormat; }
        VkImage GetHandle() const { return mHandle; }
        const VkExtent3D &GetExtent() const { return mExtent; }
        VkImageUsageFlags GetUsage() const { return mUsage; }
        VkSampleCountFlagBits GetSampleCount() const { return mSampleCount; }
        uint32_t GetMipLevels() const { return mMipLevels; }
        uint32_t GetArrayLayers() const { return mArrayLayers; }
    private:
        VkImage mHandle = VK_NULL_HANDLE;
        VkDeviceMemory mMemory  =VK_NULL_HANDLE;

        bool bCreateImage = true;

        AdVKDevice *mDevice;

        VkFormat mFormat;
        VkExtent3D mExtent;
        VkImageUsageFlags mUsage;
        VkSampleCountFlagBits mSampleCount = VK_SAMPLE_COUNT_1_BIT;
        uint32_t mMipLevels = 1;
        uint32_t mArrayLayers = 1;
    };
}

#endif
