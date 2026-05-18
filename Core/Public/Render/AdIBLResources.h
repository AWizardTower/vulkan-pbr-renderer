#ifndef AD_IBL_RESOURCES_H
#define AD_IBL_RESOURCES_H

#include "Graphic/AdVKCommon.h"
#include <memory>

namespace ade{
    class AdSampler;
    class AdVKDevice;
    class AdVKImage;
    class AdVKImageView;

    class AdIBLResources{
    public:
        explicit AdIBLResources(AdVKDevice *device);
        ~AdIBLResources();

        VkFormat GetColorFormat() const { return mColorFormat; }
        uint32_t GetEnvironmentSize() const { return mEnvironmentSize; }
        uint32_t GetIrradianceSize() const { return mIrradianceSize; }
        uint32_t GetPrefilterSize() const { return mPrefilterSize; }
        uint32_t GetBRDFLUTSize() const { return mBRDFLUTSize; }
        uint32_t GetEnvironmentMipLevels() const { return mEnvironmentMipLevels; }
        uint32_t GetPrefilterMipLevels() const { return mPrefilterMipLevels; }

        AdVKImage *GetEnvironmentCubeImage() const { return mEnvironmentCubeImage.get(); }
        AdVKImageView *GetEnvironmentCubeView() const { return mEnvironmentCubeView.get(); }
        AdSampler *GetEnvironmentSampler() const { return mEnvironmentSampler.get(); }

        AdVKImage *GetIrradianceCubeImage() const { return mIrradianceCubeImage.get(); }
        AdVKImageView *GetIrradianceCubeView() const { return mIrradianceCubeView.get(); }
        AdSampler *GetIrradianceSampler() const { return mIrradianceSampler.get(); }

        AdVKImage *GetPrefilteredCubeImage() const { return mPrefilteredCubeImage.get(); }
        AdVKImageView *GetPrefilteredCubeView() const { return mPrefilteredCubeView.get(); }
        AdSampler *GetPrefilteredSampler() const { return mPrefilteredSampler.get(); }

        AdVKImage *GetBRDFLUTImage() const { return mBRDFLUTImage.get(); }
        AdVKImageView *GetBRDFLUTView() const { return mBRDFLUTView.get(); }
        AdSampler *GetBRDFLUTSampler() const { return mBRDFLUTSampler.get(); }

        VkImageLayout GetEnvironmentBaseLayout() const { return mEnvironmentBaseLayout; }
        VkImageLayout GetEnvironmentMipLayout() const { return mEnvironmentMipLayout; }
        VkImageLayout GetIrradianceLayout() const { return mIrradianceLayout; }
        VkImageLayout GetPrefilteredLayout() const { return mPrefilteredLayout; }
        VkImageLayout GetBRDFLUTLayout() const { return mBRDFLUTLayout; }

        void SetEnvironmentLayouts(VkImageLayout baseLayout, VkImageLayout mipLayout);
        void SetIrradianceLayout(VkImageLayout layout) { mIrradianceLayout = layout; }
        void SetPrefilteredLayout(VkImageLayout layout) { mPrefilteredLayout = layout; }
        void SetBRDFLUTLayout(VkImageLayout layout) { mBRDFLUTLayout = layout; }
    private:
        static uint32_t CalculateMipLevels(uint32_t size);
        VkFormat ChooseColorFormat(AdVKDevice *device) const;
        bool SupportsLinearMipBlit(AdVKDevice *device, VkFormat format) const;

        VkFormat mColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        uint32_t mEnvironmentSize = 512;
        uint32_t mIrradianceSize = 32;
        uint32_t mPrefilterSize = 128;
        uint32_t mBRDFLUTSize = 256;
        uint32_t mEnvironmentMipLevels = 1;
        uint32_t mPrefilterMipLevels = 1;

        std::shared_ptr<AdVKImage> mEnvironmentCubeImage;
        std::shared_ptr<AdVKImageView> mEnvironmentCubeView;
        std::shared_ptr<AdSampler> mEnvironmentSampler;
        std::shared_ptr<AdVKImage> mIrradianceCubeImage;
        std::shared_ptr<AdVKImageView> mIrradianceCubeView;
        std::shared_ptr<AdSampler> mIrradianceSampler;
        std::shared_ptr<AdVKImage> mPrefilteredCubeImage;
        std::shared_ptr<AdVKImageView> mPrefilteredCubeView;
        std::shared_ptr<AdSampler> mPrefilteredSampler;
        std::shared_ptr<AdVKImage> mBRDFLUTImage;
        std::shared_ptr<AdVKImageView> mBRDFLUTView;
        std::shared_ptr<AdSampler> mBRDFLUTSampler;

        VkImageLayout mEnvironmentBaseLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout mEnvironmentMipLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout mIrradianceLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout mPrefilteredLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout mBRDFLUTLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    };
}

#endif
