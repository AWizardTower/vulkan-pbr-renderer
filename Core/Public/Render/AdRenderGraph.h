#ifndef AD_RENDER_GRAPH_H
#define AD_RENDER_GRAPH_H

#include "Graphic/AdVKCommon.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ade{
    class AdVKDevice;
    class AdVKImage;
    class AdRenderer;
    class AdRenderGraph;

    struct AdRGTextureHandle{
        uint32_t Index = UINT32_MAX;
        bool IsValid() const { return Index != UINT32_MAX; }
    };

    struct AdRGTextureDesc{
        std::string Name;
        VkExtent2D Extent = {};
        VkFormat Format = VK_FORMAT_UNDEFINED;
        VkImageUsageFlags Usage = 0;
        VkImageAspectFlags AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        VkSampleCountFlagBits SampleCount = VK_SAMPLE_COUNT_1_BIT;
        VkImageLayout InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        bool bResizeWithBackbuffer = true;
    };

    class AdRenderGraphContext{
    public:
        AdRenderGraphContext(AdRenderGraph *graph, VkCommandBuffer cmdBuffer, uint32_t frameSlot, uint32_t imageIndex);

        VkCommandBuffer GetCommandBuffer() const { return mCmdBuffer; }
        uint32_t GetFrameSlot() const { return mFrameSlot; }
        uint32_t GetImageIndex() const { return mImageIndex; }
        AdVKImage *GetTextureImage(AdRGTextureHandle handle) const;
        bool TransitionTexture(AdRGTextureHandle handle, VkImageLayout newLayout);
        void SetTextureLayout(AdRGTextureHandle handle, VkImageLayout layout);
        void DiscardTexture(AdRGTextureHandle handle);
    private:
        AdRenderGraph *mGraph;
        VkCommandBuffer mCmdBuffer;
        uint32_t mFrameSlot;
        uint32_t mImageIndex;
    };

    struct AdRGPass{
        std::string Name;
        std::vector<AdRGTextureHandle> Reads;
        std::vector<AdRGTextureHandle> Writes;
        std::function<void(AdRenderGraphContext&)> Execute;
    };

    class AdRenderGraph{
    public:
        AdRenderGraph(AdVKDevice *device, uint32_t framesInFlight, VkExtent2D backbufferExtent);
        ~AdRenderGraph() = default;

        AdRGTextureHandle CreateTexture(const AdRGTextureDesc &desc);
        AdRGTextureHandle ImportTexture(const AdRGTextureDesc &desc, const std::vector<std::shared_ptr<AdVKImage>> &images, bool useImageIndex);

        void AddPass(const std::string &name,
                     const std::vector<AdRGTextureHandle> &reads,
                     const std::vector<AdRGTextureHandle> &writes,
                     std::function<void(AdRenderGraphContext&)> execute);
        void ClearPasses();
        void Resize(VkExtent2D backbufferExtent);
        void Execute(VkCommandBuffer cmdBuffer, AdRenderer *renderer, uint32_t frameSlot, uint32_t imageIndex);

        AdVKImage *GetTextureImage(AdRGTextureHandle handle, uint32_t frameSlot, uint32_t imageIndex = UINT32_MAX) const;
        bool TransitionTexture(VkCommandBuffer cmdBuffer, AdRGTextureHandle handle, uint32_t frameSlot, uint32_t imageIndex, VkImageLayout newLayout);
        void SetTextureLayout(AdRGTextureHandle handle, uint32_t frameSlot, uint32_t imageIndex, VkImageLayout layout);
        void DiscardTexture(AdRGTextureHandle handle, uint32_t frameSlot, uint32_t imageIndex);
        std::vector<std::vector<std::shared_ptr<AdVKImage>>> BuildAttachmentImages(const std::vector<AdRGTextureHandle> &handles) const;
    private:
        struct TextureResource{
            AdRGTextureDesc Desc;
            bool bImported = false;
            bool bUseImageIndex = false;
            std::vector<std::shared_ptr<AdVKImage>> Images;
            std::vector<VkImageLayout> Layouts;
        };

        void CreateTransientImages(TextureResource &resource);
        uint32_t ResolveImageIndex(const TextureResource &resource, uint32_t frameSlot, uint32_t imageIndex) const;
        bool IsValidHandle(AdRGTextureHandle handle) const;

        AdVKDevice *mDevice = nullptr;
        uint32_t mFramesInFlight = 0;
        VkExtent2D mBackbufferExtent = {};
        std::vector<TextureResource> mTextures;
        std::vector<AdRGPass> mPasses;
    };

    class AdRenderGraphBuilder{
    public:
        explicit AdRenderGraphBuilder(AdRenderGraph *graph) : mGraph(graph) {}

        AdRGTextureHandle CreateTexture(const AdRGTextureDesc &desc) { return mGraph->CreateTexture(desc); }
        AdRGTextureHandle ImportTexture(const AdRGTextureDesc &desc, const std::vector<std::shared_ptr<AdVKImage>> &images, bool useImageIndex) {
            return mGraph->ImportTexture(desc, images, useImageIndex);
        }
        void AddPass(const std::string &name,
                     const std::vector<AdRGTextureHandle> &reads,
                     const std::vector<AdRGTextureHandle> &writes,
                     std::function<void(AdRenderGraphContext&)> execute) {
            mGraph->AddPass(name, reads, writes, std::move(execute));
        }
    private:
        AdRenderGraph *mGraph;
    };
}

#endif
