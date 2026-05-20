#include "Render/AdTexture.h"

#include "AdApplication.h"
#include "Render/AdRenderContext.h"
#include "Graphic/AdVKDevice.h"
#include "Graphic/AdVKImage.h"
#include "Graphic/AdVKImageView.h"
#include "Graphic/AdVKBuffer.h"

#include <algorithm>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

namespace ade{
    namespace{
        bool IsSupportedTextureFormat(VkFormat format) {
            return format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R8G8B8A8_SRGB;
        }
    }

    AdTexture::AdTexture(const std::string &filePath)
            : AdTexture(filePath, VK_FORMAT_R8G8B8A8_UNORM, true, RGBAColor{ 255, 255, 255, 255 }) {
    }

    AdTexture::AdTexture(const std::string &filePath,
                         VkFormat format,
                         bool generateMipmaps,
                         const RGBAColor &fallbackColor) {
        if(!IsSupportedTextureFormat(format)){
            LOG_W("Unsupported AdTexture format {0}. Falling back to VK_FORMAT_R8G8B8A8_UNORM.", vk_format_string(format));
            format = VK_FORMAT_R8G8B8A8_UNORM;
        }
        mFormat = format;

        int numChannel;
        uint8_t *data = stbi_load(filePath.c_str(), reinterpret_cast<int *>(&mWidth), reinterpret_cast<int *>(&mHeight), &numChannel, STBI_rgb_alpha);
        if(!data){
            LOG_W("Can not load image {0}. Creating 1x1 fallback texture.", filePath);
            RGBAColor fallback = fallbackColor;
            mWidth = 1;
            mHeight = 1;
            CreateImage(sizeof(RGBAColor), &fallback, false);
            return;
        }

        size_t size = sizeof(uint8_t) * 4 * mWidth * mHeight;
        CreateImage(size, data, generateMipmaps);

        stbi_image_free(data);
    }

    AdTexture::AdTexture(uint32_t width, uint32_t height, RGBAColor *pixels, VkFormat format) : mWidth(width), mHeight(height) {
        mFormat = IsSupportedTextureFormat(format) ? format : VK_FORMAT_R8G8B8A8_UNORM;
        size_t size = sizeof(uint8_t) * 4 * mWidth * mHeight;
        CreateImage(size, pixels, false);
    }

    AdTexture::~AdTexture() {
        mImageView.reset();
        mImage.reset();
    }

    uint32_t AdTexture::CalculateMipLevels(uint32_t width, uint32_t height) {
        uint32_t maxSize = std::max(1u, std::max(width, height));
        return static_cast<uint32_t>(std::floor(std::log2(static_cast<float>(maxSize)))) + 1;
    }

    void AdTexture::CreateImage(size_t size, void *data, bool generateMipmaps) {
        ade::AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        ade::AdVKDevice *device = renderCxt->GetDevice();
        mMipLevels = generateMipmaps ? CalculateMipLevels(mWidth, mHeight) : 1;
        VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        if(mMipLevels > 1){
            usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }
        mImage = std::make_shared<AdVKImage>(device, VkExtent3D{ mWidth, mHeight, 1 }, mFormat, usage, VK_SAMPLE_COUNT_1_BIT, mMipLevels);
        mImageView = std::make_shared<AdVKImageView>(device, mImage->GetHandle(), mFormat, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 0, mMipLevels);

        // copy data to buffer
        std::shared_ptr<AdVKBuffer> stageBuffer = std::make_shared<AdVKBuffer>(device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, data, true);

        // copy buffer to image
        VkCommandBuffer cmdBuffer = device->CreateAndBeginOneCmdBuffer();
        AdVKImage::TransitionLayout(cmdBuffer, mImage->GetHandle(),
                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_IMAGE_ASPECT_COLOR_BIT, 0, mMipLevels, 0, 1);
        mImage->CopyFromBuffer(cmdBuffer, stageBuffer.get());
        mImage->GenerateMipmaps2D(cmdBuffer);
        device->SubmitOneCmdBuffer(cmdBuffer);
        stageBuffer.reset();
    }
}
