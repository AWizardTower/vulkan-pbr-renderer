#ifndef AD_HDR_TEXTURE_H
#define AD_HDR_TEXTURE_H

#include "Graphic/AdVKCommon.h"
#include <memory>
#include <string>

namespace ade{
    class AdSampler;
    class AdVKImage;
    class AdVKImageView;

    class AdHDRTexture{
    public:
        explicit AdHDRTexture(const std::string &filePath);
        ~AdHDRTexture();

        uint32_t GetWidth() const { return mWidth; }
        uint32_t GetHeight() const { return mHeight; }
        uint32_t GetMipLevels() const { return mMipLevels; }
        AdVKImage *GetImage() const { return mImage.get(); }
        AdVKImageView *GetImageView() const { return mImageView.get(); }
        AdSampler *GetSampler() const { return mSampler.get(); }
    private:
        void CreateImage(const float *pixels);
        bool SupportsLinearMipBlit(VkFormat format) const;

        uint32_t mWidth = 0;
        uint32_t mHeight = 0;
        uint32_t mMipLevels = 1;
        VkFormat mFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
        std::shared_ptr<AdVKImage> mImage;
        std::shared_ptr<AdVKImageView> mImageView;
        std::shared_ptr<AdSampler> mSampler;
    };
}

#endif
