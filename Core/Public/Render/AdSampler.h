#ifndef ADSAMPLER_H
#define ADSAMPLER_H

#include "Graphic/AdVKCommon.h"

namespace ade{
    class AdSampler{
    public:
        AdSampler(VkFilter filter = VK_FILTER_LINEAR,
                  VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                  float minLod = 0.f,
                  float maxLod = 1.f,
                  VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR);
        ~AdSampler();

        VkSampler GetHandle() const { return mHandle; }
    private:
        VkSampler mHandle = VK_NULL_HANDLE;

        VkFilter mFilter;
        VkSamplerAddressMode mAddressMode;
        float mMinLod = 0.f;
        float mMaxLod = 1.f;
        VkSamplerMipmapMode mMipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    };
}

#endif
