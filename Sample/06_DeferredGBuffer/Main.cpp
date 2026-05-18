#include "AdEntryPoint.h"
#include "AdGeometryUtil.h"
#include "Event/AdEventObserver.h"
#include "Event/AdKeyEvent.h"
#include "Render/AdGBufferDebugCompositePass.h"
#include "Render/AdMesh.h"
#include "Render/AdRenderContext.h"
#include "Render/AdRenderGraph.h"
#include "Render/AdRenderer.h"
#include "Render/AdRenderTarget.h"
#include "Graphic/AdVKCommandBuffer.h"
#include "Graphic/AdVKRenderPass.h"

#include "ECS/AdEntity.h"
#include "ECS/Component/AdLookAtCameraComponent.h"
#include "ECS/System/AdGBufferMaterialSystem.h"

class DeferredGBufferApp : public ade::AdApplication{
protected:
    void OnConfiguration(ade::AppSettings *appSettings) override {
        appSettings->width = 1360;
        appSettings->height = 768;
        appSettings->title = "06_DeferredGBuffer";
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
        CreateGraphResources(device->GetSettings().depthFormat, extent);

        mGBufferRenderTarget = std::make_shared<ade::AdRenderTarget>(
                mGBufferRenderPass.get(),
                mGraph->BuildAttachmentImages({ mGBufferBaseColor, mGBufferNormal, mGBufferMaterial, mGBufferDepth }),
                extent);
        mGBufferRenderTarget->SetColorClearValue(0, { 0.f, 0.f, 0.f, 1.f });
        mGBufferRenderTarget->SetColorClearValue(1, { 0.f, 0.f, 1.f, 1.f });
        mGBufferRenderTarget->SetColorClearValue(2, { 0.5f, 0.f, 0.f, 1.f });
        mGBufferRenderTarget->SetDepthStencilClearValue({ 1.f, 0 });
        mGBufferRenderTarget->AddMaterialSystem<ade::AdGBufferMaterialSystem>();

        mPresentRenderTarget = std::make_shared<ade::AdRenderTarget>(mPresentRenderPass.get());
        mPresentRenderTarget->SetColorClearValue({ 0.f, 0.f, 0.f, 1.f });

        mDebugCompositePass = std::make_shared<ade::AdGBufferDebugCompositePass>(mPresentRenderPass.get(), mRenderer->GetFramesInFlight());

        SetupDebugInput();
        BuildGraphPasses();
        EnsureCommandBuffers();
        CreateMeshes();
    }

    void OnSceneInit(ade::AdScene *scene) override {
        ade::AdEntity *camera = scene->CreateEntity("Deferred GBuffer Camera");
        auto &cameraComp = camera->AddComponent<ade::AdLookAtCameraComponent>();
        cameraComp.SetRadius(2.3f);
        mGBufferRenderTarget->SetCamera(camera);

        auto baseMat0 = ade::AdMaterialFactory::GetInstance()->CreateMaterial<ade::AdBaseMaterial>();
        baseMat0->colorType = ade::COLOR_TYPE_NORMAL;
        auto baseMat1 = ade::AdMaterialFactory::GetInstance()->CreateMaterial<ade::AdBaseMaterial>();
        baseMat1->colorType = ade::COLOR_TYPE_TEXCOORD;

        {
            ade::AdEntity *cube = scene->CreateEntity("GBuffer Cube Center");
            auto &materialComp = cube->AddComponent<ade::AdBaseMaterialComponent>();
            materialComp.AddMesh(mCubeMesh.get(), baseMat1);
            auto &transComp = cube->GetComponent<ade::AdTransformComponent>();
            transComp.scale = { 1.f, 1.f, 1.f };
            transComp.position = { 0.f, 0.f, 0.f };
            transComp.rotation = { 17.f, 30.f, 0.f };
        }
        {
            ade::AdEntity *cube = scene->CreateEntity("GBuffer Cube Left");
            auto &materialComp = cube->AddComponent<ade::AdBaseMaterialComponent>();
            materialComp.AddMesh(mCubeMesh.get(), baseMat0);
            auto &transComp = cube->GetComponent<ade::AdTransformComponent>();
            transComp.scale = { 0.5f, 0.5f, 0.5f };
            transComp.position = { -1.f, 0.f, 0.f };
            transComp.rotation = { 17.f, 30.f, 0.f };
        }
        {
            ade::AdEntity *cube = scene->CreateEntity("GBuffer Cube Right");
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
        mObserver.reset();

        ade::AdRenderContext *renderCxt = ade::AdApplication::GetAppContext()->renderCxt;
        ade::AdVKDevice *device = renderCxt->GetDevice();
        vkDeviceWaitIdle(device->GetHandle());

        mCubeMesh.reset();
        mCmdBuffers.clear();
        mDebugCompositePass.reset();
        mGBufferRenderTarget.reset();
        mPresentRenderTarget.reset();
        mGraph.reset();
        mGBufferRenderPass.reset();
        mPresentRenderPass.reset();
        mRenderer.reset();
    }
private:
    void CreateRenderPasses(ade::AdVKDevice *device, VkFormat swapchainFormat) {
        std::vector<ade::Attachment> gbufferAttachments = {
            {
                .format = VK_FORMAT_R8G8B8A8_UNORM,
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
                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
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
                .format = VK_FORMAT_R8G8B8A8_UNORM,
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
        std::vector<ade::RenderSubPass> gbufferSubpasses = {
            {
                .colorAttachments = { 0, 1, 2 },
                .depthStencilAttachments = { 3 },
                .sampleCount = VK_SAMPLE_COUNT_1_BIT
            }
        };
        mGBufferRenderPass = std::make_shared<ade::AdVKRenderPass>(device, gbufferAttachments, gbufferSubpasses);

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

    void CreateGraphResources(VkFormat depthFormat, VkExtent2D extent) {
        ade::AdRenderGraphBuilder builder(mGraph.get());
        mGBufferBaseColor = builder.CreateTexture({
            .Name = "GBuffer.BaseColor",
            .Extent = extent,
            .Format = VK_FORMAT_R8G8B8A8_UNORM,
            .Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .AspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .SampleCount = VK_SAMPLE_COUNT_1_BIT,
            .InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .bResizeWithBackbuffer = true
        });
        mGBufferNormal = builder.CreateTexture({
            .Name = "GBuffer.Normal",
            .Extent = extent,
            .Format = VK_FORMAT_R16G16B16A16_SFLOAT,
            .Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .AspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .SampleCount = VK_SAMPLE_COUNT_1_BIT,
            .InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .bResizeWithBackbuffer = true
        });
        mGBufferMaterial = builder.CreateTexture({
            .Name = "GBuffer.Material",
            .Extent = extent,
            .Format = VK_FORMAT_R8G8B8A8_UNORM,
            .Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .AspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .SampleCount = VK_SAMPLE_COUNT_1_BIT,
            .InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .bResizeWithBackbuffer = true
        });
        mGBufferDepth = builder.CreateTexture({
            .Name = "GBuffer.Depth",
            .Extent = extent,
            .Format = depthFormat,
            .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .SampleCount = VK_SAMPLE_COUNT_1_BIT,
            .InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .bResizeWithBackbuffer = true
        });
    }

    void BuildGraphPasses() {
        mGraph->ClearPasses();
        mGraph->AddPass("RG.GBufferPass", {}, { mGBufferBaseColor, mGBufferNormal, mGBufferMaterial, mGBufferDepth },
            [this](ade::AdRenderGraphContext &context) {
                context.DiscardTexture(mGBufferBaseColor);
                context.DiscardTexture(mGBufferNormal);
                context.DiscardTexture(mGBufferMaterial);
                context.DiscardTexture(mGBufferDepth);

                mGBufferRenderTarget->BeginAt(context.GetCommandBuffer(), context.GetFrameSlot());
                mGBufferRenderTarget->RenderMaterialSystems(context.GetCommandBuffer());
                mGBufferRenderTarget->End(context.GetCommandBuffer());

                context.SetTextureLayout(mGBufferBaseColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                context.SetTextureLayout(mGBufferNormal, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                context.SetTextureLayout(mGBufferMaterial, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                context.SetTextureLayout(mGBufferDepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            });

        mGraph->AddPass("RG.GBufferDebugComposite", { mGBufferBaseColor, mGBufferNormal, mGBufferMaterial }, {},
            [this](ade::AdRenderGraphContext &context) {
                context.TransitionTexture(mGBufferBaseColor, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                context.TransitionTexture(mGBufferNormal, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                context.TransitionTexture(mGBufferMaterial, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                mDebugCompositePass->Render(context.GetCommandBuffer(),
                                            mPresentRenderTarget.get(),
                                            context.GetFrameSlot(),
                                            context.GetImageIndex(),
                                            context.GetTextureImage(mGBufferBaseColor),
                                            context.GetTextureImage(mGBufferNormal),
                                            context.GetTextureImage(mGBufferMaterial),
                                            mDebugViewMode);
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
        mGBufferRenderTarget->SetExternalImages(
                mGraph->BuildAttachmentImages({ mGBufferBaseColor, mGBufferNormal, mGBufferMaterial, mGBufferDepth }),
                extent);
        mPresentRenderTarget->SetBufferCount(static_cast<uint32_t>(swapchain->GetImages().size()));
        mPresentRenderTarget->SetExtent(extent);
        mDebugCompositePass->InvalidateSourceViews();
        EnsureCommandBuffers();
    }

    void SetupDebugInput() {
        mObserver = std::make_shared<ade::AdEventObserver>();
        mObserver->OnEvent<ade::AdKeyPressEvent>([this](const ade::AdKeyPressEvent &event){
            if(event.IsRepeat()){
                return;
            }
            switch (event.mKeyCode) {
                case ade::KEY_1:
                    mDebugViewMode = ade::AdGBufferDebugViewMode::BaseColor;
                    LOG_I("GBuffer debug view: BaseColor");
                    break;
                case ade::KEY_2:
                    mDebugViewMode = ade::AdGBufferDebugViewMode::Normal;
                    LOG_I("GBuffer debug view: Normal");
                    break;
                case ade::KEY_3:
                    mDebugViewMode = ade::AdGBufferDebugViewMode::Roughness;
                    LOG_I("GBuffer debug view: Roughness");
                    break;
                case ade::KEY_4:
                    mDebugViewMode = ade::AdGBufferDebugViewMode::Metallic;
                    LOG_I("GBuffer debug view: Metallic");
                    break;
                default:
                    break;
            }
        });
    }

    std::shared_ptr<ade::AdRenderer> mRenderer;
    std::shared_ptr<ade::AdRenderGraph> mGraph;
    ade::AdRGTextureHandle mGBufferBaseColor;
    ade::AdRGTextureHandle mGBufferNormal;
    ade::AdRGTextureHandle mGBufferMaterial;
    ade::AdRGTextureHandle mGBufferDepth;

    std::shared_ptr<ade::AdVKRenderPass> mGBufferRenderPass;
    std::shared_ptr<ade::AdVKRenderPass> mPresentRenderPass;
    std::shared_ptr<ade::AdRenderTarget> mGBufferRenderTarget;
    std::shared_ptr<ade::AdRenderTarget> mPresentRenderTarget;
    std::shared_ptr<ade::AdGBufferDebugCompositePass> mDebugCompositePass;
    ade::AdGBufferDebugViewMode mDebugViewMode = ade::AdGBufferDebugViewMode::BaseColor;

    std::vector<VkCommandBuffer> mCmdBuffers;
    std::shared_ptr<ade::AdMesh> mCubeMesh;
    std::shared_ptr<ade::AdEventObserver> mObserver;
};

ade::AdApplication *CreateApplicationEntryPoint(){
    return new DeferredGBufferApp();
}
