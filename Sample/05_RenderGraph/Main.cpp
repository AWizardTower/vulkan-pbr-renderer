#include "AdEntryPoint.h"
#include "AdGeometryUtil.h"
#include "Render/AdFullscreenTexturePass.h"
#include "Render/AdMesh.h"
#include "Render/AdRenderContext.h"
#include "Render/AdRenderGraph.h"
#include "Render/AdRenderer.h"
#include "Render/AdRenderTarget.h"
#include "Graphic/AdVKCommandBuffer.h"
#include "Graphic/AdVKRenderPass.h"

#include "ECS/AdEntity.h"
#include "ECS/Component/AdLookAtCameraComponent.h"
#include "ECS/System/AdBaseMaterialSystem.h"

class RenderGraphApp : public ade::AdApplication{
protected:
    void OnConfiguration(ade::AppSettings *appSettings) override {
        appSettings->width = 1360;
        appSettings->height = 768;
        appSettings->title = "05_RenderGraph";
    }

    void OnInit() override {
        ade::AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        ade::AdVKDevice *device = renderCxt->GetDevice();
        ade::AdVKSwapchain *swapchain = renderCxt->GetSwapchain();

        VkExtent2D extent = { swapchain->GetWidth(), swapchain->GetHeight() };
        VkFormat swapchainFormat = swapchain->GetSurfaceInfo().surfaceFormat.format;

        mRenderer = std::make_shared<ade::AdRenderer>();
        mGraph = std::make_shared<ade::AdRenderGraph>(device, mRenderer->GetFramesInFlight(), extent);

        CreateRenderPasses(device, swapchainFormat);
        CreateGraphResources(swapchainFormat, device->GetSettings().depthFormat, extent);

        mSceneRenderTarget = std::make_shared<ade::AdRenderTarget>(
                mSceneRenderPass.get(),
                mGraph->BuildAttachmentImages({ mSceneColor, mSceneDepth }),
                extent);
        mSceneRenderTarget->SetColorClearValue({ 0.08f, 0.11f, 0.16f, 1.f });
        mSceneRenderTarget->SetDepthStencilClearValue({ 1.f, 0 });
        mSceneRenderTarget->AddMaterialSystem<ade::AdBaseMaterialSystem>();

        mPresentRenderTarget = std::make_shared<ade::AdRenderTarget>(mPresentRenderPass.get());
        mPresentRenderTarget->SetColorClearValue({ 0.f, 0.f, 0.f, 1.f });

        mFullscreenPass = std::make_shared<ade::AdFullscreenTexturePass>(mPresentRenderPass.get(), mRenderer->GetFramesInFlight());

        BuildGraphPasses();
        EnsureCommandBuffers();
        CreateMeshes();
    }

    void OnSceneInit(ade::AdScene *scene) override {
        ade::AdEntity *camera = scene->CreateEntity("RenderGraph Camera");
        auto &cameraComp = camera->AddComponent<ade::AdLookAtCameraComponent>();
        cameraComp.SetRadius(2.3f);
        mSceneRenderTarget->SetCamera(camera);

        auto baseMat0 = ade::AdMaterialFactory::GetInstance()->CreateMaterial<ade::AdBaseMaterial>();
        baseMat0->colorType = ade::COLOR_TYPE_NORMAL;
        auto baseMat1 = ade::AdMaterialFactory::GetInstance()->CreateMaterial<ade::AdBaseMaterial>();
        baseMat1->colorType = ade::COLOR_TYPE_TEXCOORD;

        {
            ade::AdEntity *cube = scene->CreateEntity("RG Cube Center");
            auto &materialComp = cube->AddComponent<ade::AdBaseMaterialComponent>();
            materialComp.AddMesh(mCubeMesh.get(), baseMat1);
            auto &transComp = cube->GetComponent<ade::AdTransformComponent>();
            transComp.scale = { 1.f, 1.f, 1.f };
            transComp.position = { 0.f, 0.f, 0.f };
            transComp.rotation = { 17.f, 30.f, 0.f };
        }
        {
            ade::AdEntity *cube = scene->CreateEntity("RG Cube Left");
            auto &materialComp = cube->AddComponent<ade::AdBaseMaterialComponent>();
            materialComp.AddMesh(mCubeMesh.get(), baseMat0);
            auto &transComp = cube->GetComponent<ade::AdTransformComponent>();
            transComp.scale = { 0.5f, 0.5f, 0.5f };
            transComp.position = { -1.f, 0.f, 0.f };
            transComp.rotation = { 17.f, 30.f, 0.f };
        }
        {
            ade::AdEntity *cube = scene->CreateEntity("RG Cube Right");
            auto &materialComp = cube->AddComponent<ade::AdBaseMaterialComponent>();
            materialComp.AddMesh(mCubeMesh.get(), baseMat1);
            auto &transComp = cube->GetComponent<ade::AdTransformComponent>();
            transComp.scale = { 0.5f, 0.5f, 0.5f };
            transComp.position = { 1.f, 0.f, 0.f };
            transComp.rotation = { 17.f, 30.f, 0.f };
        }
    }

    void OnSceneDestroy(ade::AdScene *scene) override {
    }

    void OnRender() override {
        ade::AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        ade::AdVKSwapchain *swapchain = renderCxt->GetSwapchain();

        int32_t imageIndex = -1;
        if(mRenderer->Begin(&imageIndex)){
            OnSwapchainResized();
        }
        if(imageIndex < 0){
            return;
        }

        EnsureCommandBuffers();
        VkCommandBuffer cmdBuffer = mCmdBuffers[imageIndex];
        ade::AdVKCommandPool::BeginCommandBuffer(cmdBuffer);

        mRenderer->BeginFrameScope(cmdBuffer);
        mGraph->Execute(cmdBuffer, mRenderer.get(), mRenderer->GetCurrentFrameSlot(), static_cast<uint32_t>(imageIndex));
        mRenderer->EndFrameScope(cmdBuffer);

        ade::AdVKCommandPool::EndCommandBuffer(cmdBuffer);
        if(mRenderer->End(imageIndex, { cmdBuffer })){
            OnSwapchainResized();
        }
    }

    void OnDestroy() override {
        ade::AdRenderContext *renderCxt = ade::AdApplication::GetAppContext()->renderCxt;
        ade::AdVKDevice *device = renderCxt->GetDevice();
        vkDeviceWaitIdle(device->GetHandle());

        mCubeMesh.reset();
        mCmdBuffers.clear();
        mFullscreenPass.reset();
        mSceneRenderTarget.reset();
        mPresentRenderTarget.reset();
        mGraph.reset();
        mSceneRenderPass.reset();
        mPresentRenderPass.reset();
        mRenderer.reset();
    }
private:
    void CreateRenderPasses(ade::AdVKDevice *device, VkFormat swapchainFormat) {
        std::vector<ade::Attachment> sceneAttachments = {
            {
                .format = swapchainFormat,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
            },
            {
                .format = device->GetSettings().depthFormat,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
            }
        };
        std::vector<ade::RenderSubPass> sceneSubpasses = {
            {
                .colorAttachments = { 0 },
                .depthStencilAttachments = { 1 },
                .sampleCount = VK_SAMPLE_COUNT_1_BIT
            }
        };
        mSceneRenderPass = std::make_shared<ade::AdVKRenderPass>(device, sceneAttachments, sceneSubpasses);

        std::vector<ade::Attachment> presentAttachments = {
            {
                .format = swapchainFormat,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            }
        };
        std::vector<ade::RenderSubPass> presentSubpasses = {
            {
                .colorAttachments = { 0 },
                .sampleCount = VK_SAMPLE_COUNT_1_BIT
            }
        };
        mPresentRenderPass = std::make_shared<ade::AdVKRenderPass>(device, presentAttachments, presentSubpasses);
    }

    void CreateGraphResources(VkFormat sceneColorFormat, VkFormat sceneDepthFormat, VkExtent2D extent) {
        ade::AdRenderGraphBuilder builder(mGraph.get());
        mSceneColor = builder.CreateTexture({
            .Name = "SceneColor",
            .Extent = extent,
            .Format = sceneColorFormat,
            .Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .AspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .SampleCount = VK_SAMPLE_COUNT_1_BIT,
            .InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .bResizeWithBackbuffer = true
        });
        mSceneDepth = builder.CreateTexture({
            .Name = "SceneDepth",
            .Extent = extent,
            .Format = sceneDepthFormat,
            .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .SampleCount = VK_SAMPLE_COUNT_1_BIT,
            .InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .bResizeWithBackbuffer = true
        });
    }

    void BuildGraphPasses() {
        mGraph->ClearPasses();
        mGraph->AddPass("RG.SceneBasePass", {}, { mSceneColor, mSceneDepth },
            [this](ade::AdRenderGraphContext &context) {
                context.DiscardTexture(mSceneColor);
                context.DiscardTexture(mSceneDepth);

                mSceneRenderTarget->BeginAt(context.GetCommandBuffer(), context.GetFrameSlot());
                mSceneRenderTarget->RenderMaterialSystems(context.GetCommandBuffer());
                mSceneRenderTarget->End(context.GetCommandBuffer());

                context.SetTextureLayout(mSceneColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                context.SetTextureLayout(mSceneDepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            });

        mGraph->AddPass("RG.PresentComposite", { mSceneColor }, {},
            [this](ade::AdRenderGraphContext &context) {
                context.TransitionTexture(mSceneColor, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                mFullscreenPass->Render(context.GetCommandBuffer(),
                                        mPresentRenderTarget.get(),
                                        context.GetFrameSlot(),
                                        context.GetImageIndex(),
                                        context.GetTextureImage(mSceneColor));
            });
    }

    void CreateMeshes() {
        std::vector<ade::AdVertex> vertices;
        std::vector<uint32_t> indices;
        ade::AdGeometryUtil::CreateCube(-0.3f, 0.3f, -0.3f, 0.3f, -0.3f, 0.3f, vertices, indices);
        mCubeMesh = std::make_shared<ade::AdMesh>(vertices, indices);
    }

    void EnsureCommandBuffers() {
        ade::AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        ade::AdVKDevice *device = renderCxt->GetDevice();
        ade::AdVKSwapchain *swapchain = renderCxt->GetSwapchain();
        uint32_t imageCount = static_cast<uint32_t>(swapchain->GetImages().size());
        if(mCmdBuffers.size() == imageCount){
            return;
        }
        mCmdBuffers = device->GetDefaultCmdPool()->AllocateCommandBuffer(imageCount);
    }

    void OnSwapchainResized() {
        ade::AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        ade::AdVKSwapchain *swapchain = renderCxt->GetSwapchain();
        VkExtent2D extent = { swapchain->GetWidth(), swapchain->GetHeight() };
        if(extent.width == 0 || extent.height == 0){
            return;
        }

        mGraph->Resize(extent);
        mSceneRenderTarget->SetExternalImages(mGraph->BuildAttachmentImages({ mSceneColor, mSceneDepth }), extent);
        mPresentRenderTarget->SetBufferCount(static_cast<uint32_t>(swapchain->GetImages().size()));
        mPresentRenderTarget->SetExtent(extent);
        mFullscreenPass->InvalidateSourceViews();
        EnsureCommandBuffers();
    }

    std::shared_ptr<ade::AdRenderer> mRenderer;
    std::shared_ptr<ade::AdRenderGraph> mGraph;
    ade::AdRGTextureHandle mSceneColor;
    ade::AdRGTextureHandle mSceneDepth;

    std::shared_ptr<ade::AdVKRenderPass> mSceneRenderPass;
    std::shared_ptr<ade::AdVKRenderPass> mPresentRenderPass;
    std::shared_ptr<ade::AdRenderTarget> mSceneRenderTarget;
    std::shared_ptr<ade::AdRenderTarget> mPresentRenderTarget;
    std::shared_ptr<ade::AdFullscreenTexturePass> mFullscreenPass;

    std::vector<VkCommandBuffer> mCmdBuffers;
    std::shared_ptr<ade::AdMesh> mCubeMesh;
};

ade::AdApplication *CreateApplicationEntryPoint(){
    return new RenderGraphApp();
}
