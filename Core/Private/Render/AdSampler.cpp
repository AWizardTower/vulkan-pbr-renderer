#include "Render/AdSampler.h"

#include "AdApplication.h"
#include "Render/AdRenderContext.h"

namespace ade{
    AdSampler::AdSampler(VkFilter filter,
                         VkSamplerAddressMode addressMode,
                         float minLod,
                         float maxLod,
                         VkSamplerMipmapMode mipmapMode)
            : mFilter(filter), mAddressMode(addressMode), mMinLod(minLod), mMaxLod(maxLod), mMipmapMode(mipmapMode) {
        ade::AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        ade::AdVKDevice *device = renderCxt->GetDevice();

        CALL_VK(device->CreateSimpleSampler(mFilter, mAddressMode, &mHandle, mMipmapMode, mMinLod, mMaxLod));
    }

    AdSampler::~AdSampler() {
        ade::AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        ade::AdVKDevice *device = renderCxt->GetDevice();
        VK_D(Sampler, device->GetHandle(), mHandle);
    }
}
