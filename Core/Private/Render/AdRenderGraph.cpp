#include "Render/AdRenderGraph.h"

#include "Graphic/AdVKDevice.h"
#include "Graphic/AdVKImage.h"
#include "Render/AdRenderer.h"

namespace ade{
    AdRenderGraphContext::AdRenderGraphContext(AdRenderGraph *graph, VkCommandBuffer cmdBuffer, uint32_t frameSlot, uint32_t imageIndex)
            : mGraph(graph), mCmdBuffer(cmdBuffer), mFrameSlot(frameSlot), mImageIndex(imageIndex) {
    }

    AdVKImage *AdRenderGraphContext::GetTextureImage(AdRGTextureHandle handle) const {
        return mGraph->GetTextureImage(handle, mFrameSlot, mImageIndex);
    }

    bool AdRenderGraphContext::TransitionTexture(AdRGTextureHandle handle, VkImageLayout newLayout) {
        return mGraph->TransitionTexture(mCmdBuffer, handle, mFrameSlot, mImageIndex, newLayout);
    }

    void AdRenderGraphContext::SetTextureLayout(AdRGTextureHandle handle, VkImageLayout layout) {
        mGraph->SetTextureLayout(handle, mFrameSlot, mImageIndex, layout);
    }

    void AdRenderGraphContext::DiscardTexture(AdRGTextureHandle handle) {
        mGraph->DiscardTexture(handle, mFrameSlot, mImageIndex);
    }

    AdRenderGraph::AdRenderGraph(AdVKDevice *device, uint32_t framesInFlight, VkExtent2D backbufferExtent)
            : mDevice(device), mFramesInFlight(framesInFlight), mBackbufferExtent(backbufferExtent) {
    }

    AdRGTextureHandle AdRenderGraph::CreateTexture(const AdRGTextureDesc &desc) {
        TextureResource resource;
        resource.Desc = desc;
        resource.bImported = false;
        CreateTransientImages(resource);

        mTextures.push_back(std::move(resource));
        return { static_cast<uint32_t>(mTextures.size() - 1) };
    }

    AdRGTextureHandle AdRenderGraph::ImportTexture(const AdRGTextureDesc &desc, const std::vector<std::shared_ptr<AdVKImage>> &images, bool useImageIndex) {
        TextureResource resource;
        resource.Desc = desc;
        resource.bImported = true;
        resource.bUseImageIndex = useImageIndex;
        resource.Images = images;
        resource.Layouts.resize(images.size(), desc.InitialLayout);

        mTextures.push_back(std::move(resource));
        return { static_cast<uint32_t>(mTextures.size() - 1) };
    }

    void AdRenderGraph::AddPass(const std::string &name,
                                const std::vector<AdRGTextureHandle> &reads,
                                const std::vector<AdRGTextureHandle> &writes,
                                std::function<void(AdRenderGraphContext&)> execute) {
        mPasses.push_back({
            .Name = name,
            .Reads = reads,
            .Writes = writes,
            .Execute = std::move(execute)
        });
    }

    void AdRenderGraph::ClearPasses() {
        mPasses.clear();
    }

    void AdRenderGraph::Resize(VkExtent2D backbufferExtent) {
        if(backbufferExtent.width == 0 || backbufferExtent.height == 0){
            return;
        }

        mBackbufferExtent = backbufferExtent;
        for(auto &resource: mTextures){
            if(resource.bImported || !resource.Desc.bResizeWithBackbuffer){
                continue;
            }
            resource.Desc.Extent = backbufferExtent;
            CreateTransientImages(resource);
        }
    }

    void AdRenderGraph::Execute(VkCommandBuffer cmdBuffer, AdRenderer *renderer, uint32_t frameSlot, uint32_t imageIndex) {
        AdRenderGraphContext context(this, cmdBuffer, frameSlot, imageIndex);
        for(auto &pass: mPasses){
            if(renderer){
                renderer->BeginGpuScope(cmdBuffer, pass.Name.c_str());
            }
            if(pass.Execute){
                pass.Execute(context);
            }
            if(renderer){
                renderer->EndGpuScope(cmdBuffer);
            }
        }
    }

    AdVKImage *AdRenderGraph::GetTextureImage(AdRGTextureHandle handle, uint32_t frameSlot, uint32_t imageIndex) const {
        if(!IsValidHandle(handle)){
            return nullptr;
        }

        const TextureResource &resource = mTextures[handle.Index];
        uint32_t resolvedIndex = ResolveImageIndex(resource, frameSlot, imageIndex);
        if(resolvedIndex >= resource.Images.size()){
            return nullptr;
        }
        return resource.Images[resolvedIndex].get();
    }

    bool AdRenderGraph::TransitionTexture(VkCommandBuffer cmdBuffer, AdRGTextureHandle handle, uint32_t frameSlot, uint32_t imageIndex, VkImageLayout newLayout) {
        if(!IsValidHandle(handle)){
            return false;
        }

        TextureResource &resource = mTextures[handle.Index];
        uint32_t resolvedIndex = ResolveImageIndex(resource, frameSlot, imageIndex);
        if(resolvedIndex >= resource.Images.size() || resolvedIndex >= resource.Layouts.size()){
            return false;
        }

        VkImageLayout oldLayout = resource.Layouts[resolvedIndex];
        AdVKImage *image = resource.Images[resolvedIndex].get();
        bool bSuccess = AdVKImage::TransitionLayout(cmdBuffer, image->GetHandle(), oldLayout, newLayout, resource.Desc.AspectMask);
        if(bSuccess){
            resource.Layouts[resolvedIndex] = newLayout;
        }
        return bSuccess;
    }

    void AdRenderGraph::SetTextureLayout(AdRGTextureHandle handle, uint32_t frameSlot, uint32_t imageIndex, VkImageLayout layout) {
        if(!IsValidHandle(handle)){
            return;
        }

        TextureResource &resource = mTextures[handle.Index];
        uint32_t resolvedIndex = ResolveImageIndex(resource, frameSlot, imageIndex);
        if(resolvedIndex < resource.Layouts.size()){
            resource.Layouts[resolvedIndex] = layout;
        }
    }

    void AdRenderGraph::DiscardTexture(AdRGTextureHandle handle, uint32_t frameSlot, uint32_t imageIndex) {
        SetTextureLayout(handle, frameSlot, imageIndex, VK_IMAGE_LAYOUT_UNDEFINED);
    }

    std::vector<std::vector<std::shared_ptr<AdVKImage>>> AdRenderGraph::BuildAttachmentImages(const std::vector<AdRGTextureHandle> &handles) const {
        std::vector<std::vector<std::shared_ptr<AdVKImage>>> result(mFramesInFlight);
        for(uint32_t frameSlot = 0; frameSlot < mFramesInFlight; frameSlot++){
            result[frameSlot].reserve(handles.size());
            for(AdRGTextureHandle handle: handles){
                if(!IsValidHandle(handle)){
                    continue;
                }
                const TextureResource &resource = mTextures[handle.Index];
                uint32_t resolvedIndex = ResolveImageIndex(resource, frameSlot, UINT32_MAX);
                if(resolvedIndex < resource.Images.size()){
                    result[frameSlot].push_back(resource.Images[resolvedIndex]);
                }
            }
        }
        return result;
    }

    void AdRenderGraph::CreateTransientImages(TextureResource &resource) {
        VkExtent2D extent = resource.Desc.bResizeWithBackbuffer ? mBackbufferExtent : resource.Desc.Extent;
        if(extent.width == 0 || extent.height == 0){
            return;
        }

        resource.Images.clear();
        resource.Layouts.clear();
        resource.Images.reserve(mFramesInFlight);
        resource.Layouts.reserve(mFramesInFlight);

        for(uint32_t i = 0; i < mFramesInFlight; i++){
            resource.Images.push_back(std::make_shared<AdVKImage>(
                    mDevice,
                    VkExtent3D{ extent.width, extent.height, 1 },
                    resource.Desc.Format,
                    resource.Desc.Usage,
                    resource.Desc.SampleCount));
            resource.Layouts.push_back(resource.Desc.InitialLayout);
        }
    }

    uint32_t AdRenderGraph::ResolveImageIndex(const TextureResource &resource, uint32_t frameSlot, uint32_t imageIndex) const {
        if(resource.bImported && resource.bUseImageIndex && imageIndex != UINT32_MAX){
            return imageIndex;
        }
        return frameSlot;
    }

    bool AdRenderGraph::IsValidHandle(AdRGTextureHandle handle) const {
        return handle.IsValid() && handle.Index < mTextures.size();
    }
}
