#include "Render/AdHDRTexture.h"

#include "AdApplication.h"
#include "Graphic/AdVKBuffer.h"
#include "Graphic/AdVKDevice.h"
#include "Graphic/AdVKImage.h"
#include "Graphic/AdVKImageView.h"
#include "Render/AdRenderContext.h"
#include "Render/AdSampler.h"
#include "stb/stb_image.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ade{
    namespace{
        uint32_t CalculateMipLevels(uint32_t width, uint32_t height) {
            uint32_t maxDim = std::max(width, height);
            return static_cast<uint32_t>(std::floor(std::log2(static_cast<float>(maxDim)))) + 1;
        }

        std::vector<float> CreateFallbackHDR() {
            return {
                2.5f, 2.7f, 3.1f, 1.0f,
                0.9f, 1.2f, 1.8f, 1.0f,
                0.18f, 0.22f, 0.28f, 1.0f,
                1.4f, 1.1f, 0.75f, 1.0f
            };
        }
    }

    AdHDRTexture::AdHDRTexture(const std::string &filePath) {
        int width = 0;
        int height = 0;
        int channels = 0;
        float *loadedPixels = stbi_loadf(filePath.c_str(), &width, &height, &channels, 4);
        std::vector<float> fallbackPixels;

        if(loadedPixels){
            mWidth = static_cast<uint32_t>(width);
            mHeight = static_cast<uint32_t>(height);
            LOG_I("Loaded HDR environment: {0} ({1}x{2}, channels: {3})", filePath, mWidth, mHeight, channels);
            CreateImage(loadedPixels);
            stbi_image_free(loadedPixels);
        } else {
            LOG_W("Can not load HDR image: {0}. Using procedural fallback environment.", filePath);
            mWidth = 2;
            mHeight = 2;
            fallbackPixels = CreateFallbackHDR();
            CreateImage(fallbackPixels.data());
        }
    }

    AdHDRTexture::~AdHDRTexture() {
        mSampler.reset();
        mImageView.reset();
        mImage.reset();
    }

    bool AdHDRTexture::SupportsLinearMipBlit(VkFormat format) const {
        AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        AdVKDevice *device = renderCxt->GetDevice();

        VkFormatProperties formatProperties{};
        vkGetPhysicalDeviceFormatProperties(device->GetPhysicalDevice(), format, &formatProperties);
        return (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
    }

    void AdHDRTexture::CreateImage(const float *pixels) {
        AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        AdVKDevice *device = renderCxt->GetDevice();

        bool canGenerateMips = SupportsLinearMipBlit(mFormat);
        mMipLevels = canGenerateMips ? CalculateMipLevels(mWidth, mHeight) : 1;
        if(!canGenerateMips){
            LOG_W("HDR environment format does not support linear mip generation. Roughness reflection will use base mip only.");
        }

        VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        if(mMipLevels > 1){
            usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }
        mImage = std::make_shared<AdVKImage>(device,
                                             VkExtent3D{ mWidth, mHeight, 1 },
                                             mFormat,
                                             usage,
                                             VK_SAMPLE_COUNT_1_BIT,
                                             mMipLevels);
        mImageView = std::make_shared<AdVKImageView>(device,
                                                     mImage->GetHandle(),
                                                     mFormat,
                                                     VK_IMAGE_ASPECT_COLOR_BIT,
                                                     VK_IMAGE_VIEW_TYPE_2D,
                                                     0,
                                                     mMipLevels);
        mSampler = std::make_shared<AdSampler>(VK_FILTER_LINEAR,
                                               VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                               0.f,
                                               static_cast<float>(mMipLevels - 1),
                                               VK_SAMPLER_MIPMAP_MODE_LINEAR);

        size_t size = sizeof(float) * 4 * mWidth * mHeight;
        std::shared_ptr<AdVKBuffer> stageBuffer = std::make_shared<AdVKBuffer>(device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, const_cast<float*>(pixels), true);

        VkCommandBuffer cmdBuffer = device->CreateAndBeginOneCmdBuffer();
        AdVKImage::TransitionLayout(cmdBuffer,
                                    mImage->GetHandle(),
                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_IMAGE_ASPECT_COLOR_BIT,
                                    0,
                                    mMipLevels,
                                    0,
                                    1);
        mImage->CopyFromBuffer(cmdBuffer, stageBuffer.get());
        mImage->GenerateMipmaps2D(cmdBuffer);
        device->SubmitOneCmdBuffer(cmdBuffer);
        stageBuffer.reset();
    }
}
