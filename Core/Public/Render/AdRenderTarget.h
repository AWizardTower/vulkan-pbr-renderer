#ifndef ADRENDETARGET_H
#define ADRENDETARGET_H

#include "Graphic/AdVKFrameBuffer.h"
#include "Graphic/AdVKDebugUtils.h"
#include "Render/AdRenderContext.h"
#include "ECS/System/AdMaterialSystem.h"
#include "ECS/AdEntity.h"

namespace ade{
    class AdRenderTarget{
    public:
        AdRenderTarget(AdVKRenderPass *renderPass);
        AdRenderTarget(AdVKRenderPass *renderPass, uint32_t bufferCount, VkExtent2D extent);
        AdRenderTarget(AdVKRenderPass *renderPass, const std::vector<std::vector<std::shared_ptr<AdVKImage>>> &externalImages, VkExtent2D extent);
        ~AdRenderTarget();

        void Begin(VkCommandBuffer cmdBuffer);
        void BeginAt(VkCommandBuffer cmdBuffer, uint32_t bufferIndex);
        void End(VkCommandBuffer cmdBuffer);

        AdVKRenderPass *GetRenderPass() const { return mRenderPass; }
        AdVKFrameBuffer *GetFrameBuffer() const { return mFrameBuffers[mCurrentBufferIdx].get(); }
        AdVKFrameBuffer *GetFrameBuffer(uint32_t bufferIndex) const;
        AdVKImage *GetAttachmentImage(uint32_t bufferIndex, uint32_t attachmentIndex) const;
        AdVKImage *GetCurrentAttachmentImage(uint32_t attachmentIndex) const { return GetAttachmentImage(mCurrentBufferIdx, attachmentIndex); }
        uint32_t GetBufferCount() const { return mBufferCount; }
        VkExtent2D GetExtent() const { return mExtent; }

        void SetExtent(const VkExtent2D &extent);
        void SetBufferCount(uint32_t bufferCount);
        void SetExternalImages(const std::vector<std::vector<std::shared_ptr<AdVKImage>>> &externalImages, VkExtent2D extent);

        void SetColorClearValue(VkClearColorValue colorClearValue);
        void SetColorClearValue(uint32_t attachmentIndex, VkClearColorValue colorClearValue);
        void SetDepthStencilClearValue(VkClearDepthStencilValue depthStencilValue);
        void SetDepthStencilClearValue(uint32_t attachmentIndex, VkClearDepthStencilValue depthStencilValue);

        template<typename T, typename... Args>
        std::shared_ptr<T> AddMaterialSystem(Args&&... args) {
            std::shared_ptr<T> system = std::make_shared<T>(std::forward<Args>(args)...);
            system->OnInit(mRenderPass);
            mMaterialSystemList.push_back(system);
            return system;
        }

        void RenderMaterialSystems(VkCommandBuffer cmdBuffer) {
            AdVKDebugUtils::BeginLabel(cmdBuffer, "MaterialSystems");
            for (auto &item: mMaterialSystemList){
                item->OnRender(cmdBuffer, this);
            }
            AdVKDebugUtils::EndLabel(cmdBuffer);
        }

        void SetCamera(AdEntity *camera) { mCamera = camera; }
        AdEntity *GetCamera() const { return mCamera; }
    private:
        void Init();
        void ReCreate();
        void BeginInternal(VkCommandBuffer cmdBuffer, uint32_t bufferIndex);

        std::vector<std::shared_ptr<AdVKFrameBuffer>> mFrameBuffers;
        std::vector<std::vector<std::shared_ptr<AdVKImage>>> mFrameBufferImages;
        std::vector<std::vector<std::shared_ptr<AdVKImage>>> mExternalImages;

        AdVKRenderPass *mRenderPass;
        std::vector<VkClearValue> mClearValues;
        uint32_t mBufferCount;
        uint32_t mCurrentBufferIdx = 0;
        VkExtent2D mExtent;

        bool bSwapchainTarget = false;
        bool bExternalImageTarget = false;
        bool bBeginTarget = false;

        std::vector<std::shared_ptr<AdMaterialSystem>> mMaterialSystemList;
        AdEntity *mCamera = nullptr;

        bool bShouldUpdate = false;
    };
}

#endif
