#ifndef ADTEXTURE_H
#define ADTEXTURE_H

#include "Graphic/AdVKCommon.h"

namespace ade{
    class AdVKImage;
    class AdVKImageView;
    class AdVKBuffer;

    struct RGBAColor{
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    };

    class AdTexture{
    public:
        AdTexture(const std::string &filePath);
        AdTexture(const std::string &filePath,
                  VkFormat format,
                  bool generateMipmaps = true,
                  const RGBAColor &fallbackColor = RGBAColor{ 255, 255, 255, 255 });
        AdTexture(uint32_t width,
                  uint32_t height,
                  RGBAColor *pixels,
                  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);
        ~AdTexture();

        uint32_t GetWidth() const { return mWidth; }
        uint32_t GetHeight() const { return mHeight; }
        uint32_t GetMipLevels() const { return mMipLevels; }
        VkFormat GetFormat() const { return mFormat; }
        AdVKImage *GetImage() const { return mImage.get(); }
        AdVKImageView *GetImageView() const { return mImageView.get(); }
    private:
        void CreateImage(size_t size, void *data, bool generateMipmaps);
        static uint32_t CalculateMipLevels(uint32_t width, uint32_t height);

        uint32_t mWidth = 1;
        uint32_t mHeight = 1;
        uint32_t mMipLevels = 1;
        VkFormat mFormat = VK_FORMAT_R8G8B8A8_UNORM;
        std::shared_ptr<AdVKImage> mImage;
        std::shared_ptr<AdVKImageView> mImageView;
    };
}

#endif
