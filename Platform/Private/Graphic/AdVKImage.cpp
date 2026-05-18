#include "Graphic/AdVKImage.h"
#include "Graphic/AdVKDevice.h"
#include "Graphic/AdVKBuffer.h"

#include <algorithm>

namespace ade{
    AdVKImage::AdVKImage(AdVKDevice *device,
                         VkExtent3D extent,
                         VkFormat format,
                         VkImageUsageFlags usage,
                         VkSampleCountFlagBits sampleCount,
                         uint32_t mipLevels,
                         uint32_t arrayLayers,
                         VkImageCreateFlags imageCreateFlags) : mDevice(device),
                                                                mExtent(extent),
                                                                mFormat(format),
                                                                mUsage(usage),
                                                                mSampleCount(sampleCount),
                                                                mMipLevels(std::max(1u, mipLevels)),
                                                                mArrayLayers(std::max(1u, arrayLayers)) {
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;

        VkImageCreateInfo imageInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = imageCreateFlags,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = format,
                .extent = extent,
                .mipLevels = mMipLevels,
                .arrayLayers = mArrayLayers,
                .samples = sampleCount,
                .tiling = tiling,
                .usage = usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices = nullptr,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        CALL_VK(vkCreateImage(mDevice->GetHandle(), &imageInfo, nullptr, &mHandle));

        // allocate memory
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(mDevice->GetHandle(), mHandle, &memReqs);

        VkMemoryAllocateInfo allocateInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .pNext = nullptr,
                .allocationSize = memReqs.size,
                .memoryTypeIndex = static_cast<uint32_t>(mDevice->GetMemoryIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memReqs.memoryTypeBits))
        };
        CALL_VK(vkAllocateMemory(mDevice->GetHandle(), &allocateInfo, nullptr, &mMemory));
        CALL_VK(vkBindImageMemory(mDevice->GetHandle(), mHandle, mMemory, 0));
    }

    AdVKImage::AdVKImage(AdVKDevice *device,
                         VkImage image,
                         VkExtent3D extent,
                         VkFormat format,
                         VkImageUsageFlags usage,
                         VkSampleCountFlagBits sampleCount,
                         uint32_t mipLevels,
                         uint32_t arrayLayers)
                            : mHandle(image), mDevice(device), mExtent(extent), mFormat(format), mUsage(usage),
                              mSampleCount(sampleCount), mMipLevels(std::max(1u, mipLevels)),
                              mArrayLayers(std::max(1u, arrayLayers)), bCreateImage(false) {
    }

    AdVKImage::~AdVKImage() {
        if(bCreateImage){
            VK_D(Image, mDevice->GetHandle(), mHandle);
            VK_F(mDevice->GetHandle(), mMemory);
        }
    }

    void AdVKImage::CopyFromBuffer(VkCommandBuffer cmdBuffer, AdVKBuffer *buffer) {
        CopyFromBuffer(cmdBuffer, buffer, 0, 0, 1, mExtent);
    }

    void AdVKImage::CopyFromBuffer(VkCommandBuffer cmdBuffer,
                                   AdVKBuffer *buffer,
                                   uint32_t mipLevel,
                                   uint32_t baseArrayLayer,
                                   uint32_t layerCount,
                                   VkExtent3D extent) {
        VkBufferImageCopy region = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = mipLevel,
                    .baseArrayLayer = baseArrayLayer,
                    .layerCount = layerCount
            },
            .imageOffset = { 0, 0, 0 },
            .imageExtent = extent
        };
        vkCmdCopyBufferToImage(cmdBuffer, buffer->GetHandle(), mHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    bool AdVKImage::TransitionLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask) {
        return TransitionLayout(cmdBuffer, image, oldLayout, newLayout, aspectMask, 0, 1, 0, 1);
    }

    bool AdVKImage::TransitionLayout(VkCommandBuffer cmdBuffer,
                                     VkImage image,
                                     VkImageLayout oldLayout,
                                     VkImageLayout newLayout,
                                     VkImageAspectFlags aspectMask,
                                     uint32_t baseMipLevel,
                                     uint32_t mipLevelCount,
                                     uint32_t baseArrayLayer,
                                     uint32_t layerCount) {
        if(image == VK_NULL_HANDLE){
            return false;
        }
        if(oldLayout == newLayout){
            return true;
        }
        VkImageMemoryBarrier barrier;
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspectMask;
        barrier.subresourceRange.baseMipLevel = baseMipLevel;
        barrier.subresourceRange.levelCount = mipLevelCount;
        barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
        barrier.subresourceRange.layerCount = layerCount;

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        // Source layouts (old)
        // The source access mask controls actions to be finished on the old
        // layout before it will be transitioned to the new layout.
        switch (oldLayout) {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                // Image layout is undefined (or does not matter).
                // Only valid as initial layout. No flags required.
                barrier.srcAccessMask = 0;
                break;

            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                // Image is preinitialized.
                // Only valid as initial layout for linear images; preserves memory
                // contents. Make sure host writes have finished.
                barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // Image is a color attachment.
                // Make sure writes to the color buffer have finished
                barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image is a depth/stencil attachment.
                // Make sure any writes to the depth/stencil buffer have finished.
                barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image is a transfer source.
                // Make sure any reads from the image have finished
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image is a transfer destination.
                // Make sure any writes to the image have finished.
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Image is read by a shader.
                // Make sure any shader reads from the image have finished
                barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default:
                LOG_E("Unsupported layout transition : {0} --> {1}", vk_image_layout_string(oldLayout), vk_image_layout_string(newLayout));
                return false;
        }

        // Target layouts (new)
        // The destination access mask controls the dependency for the new image
        // layout.
        switch (newLayout) {
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image will be used as a transfer destination.
                // Make sure any writes to the image have finished.
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image will be used as a transfer source.
                // Make sure any reads from and writes to the image have finished.
                barrier.srcAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // Image will be used as a color attachment.
                // Make sure any writes to the color buffer have finished.
                barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image layout will be used as a depth/stencil attachment.
                // Make sure any writes to depth/stencil buffer have finished.
                barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Image will be read in a shader (sampler, input attachment).
                // Make sure any writes to the image have finished.
                if (barrier.srcAccessMask == 0)
                {
                    barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
                }
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default:
                LOG_E("Unsupported layout transition : {0} --> {1}", vk_image_layout_string(oldLayout), vk_image_layout_string(newLayout));
                return false;
        }

        vkCmdPipelineBarrier(
                cmdBuffer,
                srcStage, dstStage,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
        );
        return true;
    }

    bool AdVKImage::GenerateMipmaps2D(VkCommandBuffer cmdBuffer) {
        if(mMipLevels <= 1){
            return TransitionLayout(cmdBuffer, mHandle,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, mArrayLayers);
        }

        VkFormatProperties formatProperties{};
        vkGetPhysicalDeviceFormatProperties(mDevice->GetPhysicalDevice(), mFormat, &formatProperties);
        if((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) == 0){
            LOG_W("Format {0} does not support linear blit mip generation. Using base mip only.", vk_format_string(mFormat));
            return TransitionLayout(cmdBuffer, mHandle,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_IMAGE_ASPECT_COLOR_BIT, 0, mMipLevels, 0, mArrayLayers);
        }

        int32_t mipWidth = static_cast<int32_t>(mExtent.width);
        int32_t mipHeight = static_cast<int32_t>(mExtent.height);

        for(uint32_t i = 1; i < mMipLevels; i++){
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = mHandle;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = mArrayLayers;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(cmdBuffer,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &barrier);

            VkImageBlit blit{};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = mArrayLayers;
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { std::max(1, mipWidth / 2), std::max(1, mipHeight / 2), 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = mArrayLayers;

            vkCmdBlitImage(cmdBuffer,
                           mHandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           mHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blit,
                           VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmdBuffer,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &barrier);

            mipWidth = std::max(1, mipWidth / 2);
            mipHeight = std::max(1, mipHeight / 2);
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = mHandle;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = mMipLevels - 1;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = mArrayLayers;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmdBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        return true;
    }

    bool AdVKImage::GenerateMipmaps2DArray(VkCommandBuffer cmdBuffer,
                                           VkImageLayout baseMipOldLayout,
                                           VkImageLayout dstMipsOldLayout) {
        if(mMipLevels <= 1){
            return TransitionLayout(cmdBuffer, mHandle,
                                    baseMipOldLayout,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, mArrayLayers);
        }

        VkFormatProperties formatProperties{};
        vkGetPhysicalDeviceFormatProperties(mDevice->GetPhysicalDevice(), mFormat, &formatProperties);
        if((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) == 0){
            LOG_W("Format {0} does not support linear blit mip generation. Using base mip only.", vk_format_string(mFormat));
            TransitionLayout(cmdBuffer, mHandle,
                             baseMipOldLayout,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, mArrayLayers);
            TransitionLayout(cmdBuffer, mHandle,
                             dstMipsOldLayout,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT, 1, mMipLevels - 1, 0, mArrayLayers);
            return false;
        }

        TransitionLayout(cmdBuffer, mHandle,
                         baseMipOldLayout,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, mArrayLayers);

        int32_t mipWidth = static_cast<int32_t>(mExtent.width);
        int32_t mipHeight = static_cast<int32_t>(mExtent.height);

        for(uint32_t i = 1; i < mMipLevels; i++){
            TransitionLayout(cmdBuffer, mHandle,
                             dstMipsOldLayout,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, mArrayLayers);

            VkImageBlit blit{};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = mArrayLayers;
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { std::max(1, mipWidth / 2), std::max(1, mipHeight / 2), 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = mArrayLayers;

            vkCmdBlitImage(cmdBuffer,
                           mHandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           mHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blit,
                           VK_FILTER_LINEAR);

            TransitionLayout(cmdBuffer, mHandle,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, mArrayLayers);

            if(i < mMipLevels - 1){
                TransitionLayout(cmdBuffer, mHandle,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, mArrayLayers);
            }

            mipWidth = std::max(1, mipWidth / 2);
            mipHeight = std::max(1, mipHeight / 2);
        }

        TransitionLayout(cmdBuffer, mHandle,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_IMAGE_ASPECT_COLOR_BIT, mMipLevels - 1, 1, 0, mArrayLayers);
        return true;
    }
}
