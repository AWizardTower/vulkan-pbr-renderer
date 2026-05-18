#include "Render/AdIBLResources.h"

#include "Graphic/AdVKDevice.h"
#include "Graphic/AdVKImage.h"
#include "Graphic/AdVKImageView.h"
#include "Render/AdSampler.h"

#include <algorithm>
#include <cmath>

namespace ade{
    namespace{
        constexpr uint32_t CubeFaceCount = 6;

        bool SupportsFormatFeatures(AdVKDevice *device, VkFormat format, VkFormatFeatureFlags requiredFeatures) {
            VkFormatProperties formatProperties{};
            vkGetPhysicalDeviceFormatProperties(device->GetPhysicalDevice(), format, &formatProperties);
            return (formatProperties.optimalTilingFeatures & requiredFeatures) == requiredFeatures;
        }
    }

    AdIBLResources::AdIBLResources(AdVKDevice *device) {
        mColorFormat = ChooseColorFormat(device);
        bool canGenerateEnvironmentMips = SupportsLinearMipBlit(device, mColorFormat);
        mEnvironmentMipLevels = canGenerateEnvironmentMips ? CalculateMipLevels(mEnvironmentSize) : 1;
        if(!canGenerateEnvironmentMips){
            LOG_W("IBL environment format {0} does not support linear blit. EnvironmentCube will use one mip.", vk_format_string(mColorFormat));
        }
        mPrefilterMipLevels = CalculateMipLevels(mPrefilterSize);

        VkImageUsageFlags cubeUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        mEnvironmentCubeImage = std::make_shared<AdVKImage>(device,
                                                            VkExtent3D{ mEnvironmentSize, mEnvironmentSize, 1 },
                                                            mColorFormat,
                                                            cubeUsage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                            VK_SAMPLE_COUNT_1_BIT,
                                                            mEnvironmentMipLevels,
                                                            CubeFaceCount,
                                                            VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
        mEnvironmentCubeView = std::make_shared<AdVKImageView>(device,
                                                               mEnvironmentCubeImage->GetHandle(),
                                                               mColorFormat,
                                                               VK_IMAGE_ASPECT_COLOR_BIT,
                                                               VK_IMAGE_VIEW_TYPE_CUBE,
                                                               0,
                                                               mEnvironmentMipLevels,
                                                               0,
                                                               CubeFaceCount);
        mEnvironmentSampler = std::make_shared<AdSampler>(VK_FILTER_LINEAR,
                                                          VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                                          0.f,
                                                          static_cast<float>(std::max(1u, mEnvironmentMipLevels) - 1),
                                                          VK_SAMPLER_MIPMAP_MODE_LINEAR);

        mIrradianceCubeImage = std::make_shared<AdVKImage>(device,
                                                           VkExtent3D{ mIrradianceSize, mIrradianceSize, 1 },
                                                           mColorFormat,
                                                           cubeUsage,
                                                           VK_SAMPLE_COUNT_1_BIT,
                                                           1,
                                                           CubeFaceCount,
                                                           VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
        mIrradianceCubeView = std::make_shared<AdVKImageView>(device,
                                                              mIrradianceCubeImage->GetHandle(),
                                                              mColorFormat,
                                                              VK_IMAGE_ASPECT_COLOR_BIT,
                                                              VK_IMAGE_VIEW_TYPE_CUBE,
                                                              0,
                                                              1,
                                                              0,
                                                              CubeFaceCount);
        mIrradianceSampler = std::make_shared<AdSampler>(VK_FILTER_LINEAR,
                                                         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                                         0.f,
                                                         0.f,
                                                         VK_SAMPLER_MIPMAP_MODE_LINEAR);

        mPrefilteredCubeImage = std::make_shared<AdVKImage>(device,
                                                            VkExtent3D{ mPrefilterSize, mPrefilterSize, 1 },
                                                            mColorFormat,
                                                            cubeUsage,
                                                            VK_SAMPLE_COUNT_1_BIT,
                                                            mPrefilterMipLevels,
                                                            CubeFaceCount,
                                                            VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
        mPrefilteredCubeView = std::make_shared<AdVKImageView>(device,
                                                               mPrefilteredCubeImage->GetHandle(),
                                                               mColorFormat,
                                                               VK_IMAGE_ASPECT_COLOR_BIT,
                                                               VK_IMAGE_VIEW_TYPE_CUBE,
                                                               0,
                                                               mPrefilterMipLevels,
                                                               0,
                                                               CubeFaceCount);
        mPrefilteredSampler = std::make_shared<AdSampler>(VK_FILTER_LINEAR,
                                                          VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                                          0.f,
                                                          static_cast<float>(std::max(1u, mPrefilterMipLevels) - 1),
                                                          VK_SAMPLER_MIPMAP_MODE_LINEAR);

        mBRDFLUTImage = std::make_shared<AdVKImage>(device,
                                                    VkExtent3D{ mBRDFLUTSize, mBRDFLUTSize, 1 },
                                                    mColorFormat,
                                                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                                    VK_SAMPLE_COUNT_1_BIT);
        mBRDFLUTView = std::make_shared<AdVKImageView>(device,
                                                       mBRDFLUTImage->GetHandle(),
                                                       mColorFormat,
                                                       VK_IMAGE_ASPECT_COLOR_BIT);
        mBRDFLUTSampler = std::make_shared<AdSampler>(VK_FILTER_LINEAR,
                                                      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                                      0.f,
                                                      0.f,
                                                      VK_SAMPLER_MIPMAP_MODE_NEAREST);
    }

    AdIBLResources::~AdIBLResources() {
        mBRDFLUTSampler.reset();
        mBRDFLUTView.reset();
        mBRDFLUTImage.reset();
        mPrefilteredSampler.reset();
        mPrefilteredCubeView.reset();
        mPrefilteredCubeImage.reset();
        mIrradianceSampler.reset();
        mIrradianceCubeView.reset();
        mIrradianceCubeImage.reset();
        mEnvironmentSampler.reset();
        mEnvironmentCubeView.reset();
        mEnvironmentCubeImage.reset();
    }

    void AdIBLResources::SetEnvironmentLayouts(VkImageLayout baseLayout, VkImageLayout mipLayout) {
        mEnvironmentBaseLayout = baseLayout;
        mEnvironmentMipLayout = mipLayout;
    }

    uint32_t AdIBLResources::CalculateMipLevels(uint32_t size) {
        return static_cast<uint32_t>(std::floor(std::log2(static_cast<float>(std::max(1u, size))))) + 1;
    }

    VkFormat AdIBLResources::ChooseColorFormat(AdVKDevice *device) const {
        VkFormatFeatureFlags required = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if(SupportsFormatFeatures(device, VK_FORMAT_R16G16B16A16_SFLOAT, required)){
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        }
        LOG_W("VK_FORMAT_R16G16B16A16_SFLOAT is not color-attachment sampled capable. Falling back to VK_FORMAT_R32G32B32A32_SFLOAT for IBL resources.");
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }

    bool AdIBLResources::SupportsLinearMipBlit(AdVKDevice *device, VkFormat format) const {
        VkFormatProperties formatProperties{};
        vkGetPhysicalDeviceFormatProperties(device->GetPhysicalDevice(), format, &formatProperties);
        return (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
    }
}
