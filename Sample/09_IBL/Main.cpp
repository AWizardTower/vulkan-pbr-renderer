#include "AdEntryPoint.h"
#include "AdFileUtil.h"
#include "AdGeometryUtil.h"
#include "Event/AdEventObserver.h"
#include "Event/AdKeyEvent.h"
#include "Event/AdMouseEvent.h"
#include "Render/AdMesh.h"
#include "Render/AdHDRTexture.h"
#include "Render/AdIBLDebugCompositePass.h"
#include "Render/AdIBLDeferredLightingPass.h"
#include "Render/AdRenderContext.h"
#include "Render/AdRenderGraph.h"
#include "Render/AdRenderer.h"
#include "Render/AdRenderTarget.h"
#include "Graphic/AdVKCommandBuffer.h"
#include "Graphic/AdVKRenderPass.h"

#include "ECS/AdEntity.h"
#include "ECS/Component/AdLookAtCameraComponent.h"
#include "ECS/Component/AdTransformComponent.h"
#include "ECS/System/AdPBRGBufferMaterialSystem.h"
#include "ECS/System/AdShadowDepthMaterialSystem.h"

#include <cmath>

class IBLApp : public ade::AdApplication{
protected:
    void OnConfiguration(ade::AppSettings *appSettings) override {
        appSettings->width = 1360;
        appSettings->height = 768;
        appSettings->title = "09_IBL";
    }

    void OnInit() override {
        ade::AdRenderContext *renderCxt = AdApplication::GetAppContext()->renderCxt;
        ade::AdVKDevice *device = renderCxt->GetDevice();
        ade::AdVKSwapchain *swapchain = renderCxt->GetSwapchain();

        VkExtent2D extent = { swapchain->GetWidth(), swapchain->GetHeight() };
        VkFormat swapchainFormat = swapchain->GetSurfaceInfo().surfaceFormat.format;

        UpdateLightMatrices();

        mRenderer = std::make_shared<ade::AdRenderer>();
        mGraph = std::make_shared<ade::AdRenderGraph>(device, mRenderer->GetFramesInFlight(), extent);

        CreateRenderPasses(device, swapchainFormat);
        CreateGraphResources(device->GetSettings().depthFormat, extent);
        mEnvironmentMap = std::make_shared<ade::AdHDRTexture>(AD_RES_TEXTURE_DIR"Environment/studio_small_09_1k.hdr");

        mShadowRenderTarget = std::make_shared<ade::AdRenderTarget>(
                mShadowRenderPass.get(),
                mGraph->BuildAttachmentImages({ mShadowDepth }),
                VkExtent2D{ ShadowMapSize, ShadowMapSize });
        mShadowRenderTarget->SetDepthStencilClearValue({ 1.f, 0 });
        mShadowRenderTarget->AddMaterialSystem<ade::AdShadowDepthMaterialSystem>(&mLightSettings.lightViewProj);

        mGBufferRenderTarget = std::make_shared<ade::AdRenderTarget>(
                mGBufferRenderPass.get(),
                mGraph->BuildAttachmentImages({ mGBufferBaseColor, mGBufferNormal, mGBufferMaterial, mGBufferWorldPosition, mGBufferDepth }),
                extent);
        mGBufferRenderTarget->SetColorClearValue(0, { 0.f, 0.f, 0.f, 1.f });
        mGBufferRenderTarget->SetColorClearValue(1, { 0.f, 0.f, 1.f, 1.f });
        mGBufferRenderTarget->SetColorClearValue(2, { 0.5f, 0.f, 0.f, 1.f });
        mGBufferRenderTarget->SetColorClearValue(3, { 0.f, 0.f, 0.f, 0.f });
        mGBufferRenderTarget->SetDepthStencilClearValue({ 1.f, 0 });
        mGBufferRenderTarget->AddMaterialSystem<ade::AdPBRGBufferMaterialSystem>();

        mLightingRenderTarget = std::make_shared<ade::AdRenderTarget>(
                mLightingRenderPass.get(),
                mGraph->BuildAttachmentImages({ mLightingColor }),
                extent);
        mLightingRenderTarget->SetColorClearValue({ 0.f, 0.f, 0.f, 1.f });

        mPresentRenderTarget = std::make_shared<ade::AdRenderTarget>(mPresentRenderPass.get());
        mPresentRenderTarget->SetColorClearValue({ 0.f, 0.f, 0.f, 1.f });

        mLightingPass = std::make_shared<ade::AdIBLDeferredLightingPass>(mLightingRenderPass.get(), mRenderer->GetFramesInFlight());
        mDebugCompositePass = std::make_shared<ade::AdIBLDebugCompositePass>(mPresentRenderPass.get(), mRenderer->GetFramesInFlight());

        SetupDebugInput();
        BuildGraphPasses();
        EnsureCommandBuffers();
        CreateMeshes();
    }

    void OnSceneInit(ade::AdScene *scene) override {
        ade::AdEntity *camera = scene->CreateEntity("Shadow PCF Camera");
        auto &cameraComp = camera->AddComponent<ade::AdLookAtCameraComponent>();
        cameraComp.SetRadius(3.2f);
        cameraComp.SetTarget({ 0.f, -0.18f, 0.f });
        mGBufferRenderTarget->SetCamera(camera);
        mCamera = camera;

        auto groundMaterial = ade::AdMaterialFactory::GetInstance()->CreateMaterial<ade::AdBaseMaterial>();
        groundMaterial->colorType = ade::COLOR_TYPE_TEXCOORD;
        groundMaterial->roughness = 0.82f;
        groundMaterial->metallic = 0.0f;

        auto lowRoughness = ade::AdMaterialFactory::GetInstance()->CreateMaterial<ade::AdBaseMaterial>();
        lowRoughness->colorType = ade::COLOR_TYPE_TEXCOORD;
        lowRoughness->roughness = 0.15f;
        lowRoughness->metallic = 0.0f;

        auto highRoughness = ade::AdMaterialFactory::GetInstance()->CreateMaterial<ade::AdBaseMaterial>();
        highRoughness->colorType = ade::COLOR_TYPE_NORMAL;
        highRoughness->roughness = 0.65f;
        highRoughness->metallic = 0.0f;

        auto metallic = ade::AdMaterialFactory::GetInstance()->CreateMaterial<ade::AdBaseMaterial>();
        metallic->colorType = ade::COLOR_TYPE_TEXCOORD;
        metallic->roughness = 0.25f;
        metallic->metallic = 1.0f;

        {
            ade::AdEntity *ground = scene->CreateEntity("Shadow Ground");
            auto &materialComp = ground->AddComponent<ade::AdBaseMaterialComponent>();
            materialComp.AddMesh(mCubeMesh.get(), groundMaterial);
            auto &transComp = ground->GetComponent<ade::AdTransformComponent>();
            transComp.scale = { 7.f, 0.05f, 7.f };
            transComp.position = { 0.f, -0.7f, 0.f };
            transComp.rotation = { 0.f, 0.f, 0.f };
        }
        {
            ade::AdEntity *cube = scene->CreateEntity("Shadow Cube Low Roughness");
            auto &materialComp = cube->AddComponent<ade::AdBaseMaterialComponent>();
            materialComp.AddMesh(mCubeMesh.get(), lowRoughness);
            auto &transComp = cube->GetComponent<ade::AdTransformComponent>();
            transComp.scale = { 0.75f, 0.75f, 0.75f };
            transComp.position = { -1.f, -0.46f, -0.15f };
            transComp.rotation = { 17.f, 30.f, 0.f };
        }
        {
            ade::AdEntity *cube = scene->CreateEntity("Shadow Cube High Roughness");
            auto &materialComp = cube->AddComponent<ade::AdBaseMaterialComponent>();
            materialComp.AddMesh(mCubeMesh.get(), highRoughness);
            auto &transComp = cube->GetComponent<ade::AdTransformComponent>();
            transComp.scale = { 0.75f, 0.75f, 0.75f };
            transComp.position = { 0.f, -0.46f, 0.2f };
            transComp.rotation = { 17.f, 30.f, 0.f };
        }
        {
            ade::AdEntity *cube = scene->CreateEntity("Shadow Cube Metallic");
            auto &materialComp = cube->AddComponent<ade::AdBaseMaterialComponent>();
            materialComp.AddMesh(mCubeMesh.get(), metallic);
            auto &transComp = cube->GetComponent<ade::AdTransformComponent>();
            transComp.scale = { 0.75f, 0.75f, 0.75f };
            transComp.position = { 1.f, -0.46f, -0.05f };
            transComp.rotation = { 17.f, 30.f, 0.f };
        }
    }

    void OnSceneDestroy(ade::AdScene *scene) override {
        mCamera = nullptr;
    }

    void OnUpdate(float deltaTime) override {
        UpdateOrbitCamera();
    }

    void OnRender() override {
        int32_t imageIndex = -1;
        if(mRenderer->Begin(&imageIndex)){
            OnSwapchainResized();
        }
        if(imageIndex < 0){
            return;
        }

        UpdateViewDependentSettings();
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
        mEnvironmentMap.reset();
        mCmdBuffers.clear();
        mDebugCompositePass.reset();
        mLightingPass.reset();
        mShadowRenderTarget.reset();
        mGBufferRenderTarget.reset();
        mLightingRenderTarget.reset();
        mPresentRenderTarget.reset();
        mGraph.reset();
        mShadowRenderPass.reset();
        mGBufferRenderPass.reset();
        mLightingRenderPass.reset();
        mPresentRenderPass.reset();
        mRenderer.reset();
    }
private:
    static constexpr uint32_t ShadowMapSize = 2048;
    static constexpr float ShadowDepthBias = 0.0015f;
    static constexpr float EnvironmentDiffuseIntensity = 0.55f;
    static constexpr float EnvironmentSpecularIntensity = 1.0f;

    void UpdateLightMatrices() {
        glm::vec3 lightDirection = glm::normalize(glm::vec3(-0.35f, -0.8f, -0.45f));
        glm::vec3 sceneCenter{ 0.f, -0.15f, 0.f };
        float lightDistance = 4.0f;
        float orthoHalfSize = 3.0f;
        float nearPlane = 0.1f;
        float farPlane = 10.0f;

        glm::vec3 up{ 0.f, 1.f, 0.f };
        if(std::abs(glm::dot(up, lightDirection)) > 0.95f){
            up = { 0.f, 0.f, 1.f };
        }
        glm::vec3 lightPosition = sceneCenter - lightDirection * lightDistance;
        glm::mat4 lightView = glm::lookAt(lightPosition, sceneCenter, up);
        glm::mat4 lightProj = glm::ortho(-orthoHalfSize, orthoHalfSize,
                                         -orthoHalfSize, orthoHalfSize,
                                         nearPlane, farPlane);
        lightProj[1][1] *= -1.f;

        mLightSettings.lightViewProj = lightProj * lightView;
        mLightSettings.lightDirection = glm::vec4(lightDirection, 0.f);
        mLightSettings.iblParams = glm::vec4(1.f / static_cast<float>(ShadowMapSize),
                                             ShadowDepthBias,
                                             EnvironmentDiffuseIntensity,
                                             EnvironmentSpecularIntensity);

        mDebugSettings.lightViewProj = mLightSettings.lightViewProj;
        mDebugSettings.shadowParams = glm::vec4(1.f / static_cast<float>(ShadowMapSize), ShadowDepthBias, 0.f, 0.f);
        mDebugSettings.iblParams = glm::vec4(EnvironmentDiffuseIntensity, EnvironmentSpecularIntensity, 0.f, 0.f);
        mDebugSettings.debugParams = glm::vec4(static_cast<float>(mDebugViewMode), 1.f, 2.2f, 0.f);
    }

    void UpdateViewDependentSettings() {
        if(!mCamera || !ade::AdEntity::HasComponent<ade::AdLookAtCameraComponent>(mCamera)){
            return;
        }

        auto &cameraComp = mCamera->GetComponent<ade::AdLookAtCameraComponent>();
        VkExtent2D extent = mGBufferRenderTarget->GetExtent();
        if(extent.height > 0){
            cameraComp.SetAspect(static_cast<float>(extent.width) / static_cast<float>(extent.height));
        }
        glm::mat4 viewMat = cameraComp.GetViewMat();
        glm::mat4 projMat = cameraComp.GetProjMat();
        glm::mat4 viewProj = projMat * viewMat;
        if(ade::AdEntity::HasComponent<ade::AdTransformComponent>(mCamera)){
            auto &transComp = mCamera->GetComponent<ade::AdTransformComponent>();
            mLightSettings.cameraPosition = glm::vec4(transComp.position, 0.f);
            mDebugSettings.cameraPosition = glm::vec4(transComp.position, 0.f);
        }
        mDebugSettings.inverseViewProj = glm::inverse(viewProj);
        mDebugSettings.debugParams = glm::vec4(static_cast<float>(mDebugViewMode), 1.f, 2.2f, 0.f);
    }

    void CreateRenderPasses(ade::AdVKDevice *device, VkFormat swapchainFormat) {
        std::vector<ade::Attachment> shadowAttachments = {
            {
                .format = device->GetSettings().depthFormat,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
            }
        };
        std::vector<ade::RenderSubPass> shadowSubpasses = {
            {
                .depthStencilAttachments = { 0 },
                .sampleCount = VK_SAMPLE_COUNT_1_BIT
            }
        };
        mShadowRenderPass = std::make_shared<ade::AdVKRenderPass>(device, shadowAttachments, shadowSubpasses);

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
                .colorAttachments = { 0, 1, 2, 3 },
                .depthStencilAttachments = { 4 },
                .sampleCount = VK_SAMPLE_COUNT_1_BIT
            }
        };
        mGBufferRenderPass = std::make_shared<ade::AdVKRenderPass>(device, gbufferAttachments, gbufferSubpasses);

        std::vector<ade::Attachment> lightingAttachments = {
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
            }
        };
        std::vector<ade::RenderSubPass> lightingSubpasses = {
            {
                .colorAttachments = { 0 },
                .sampleCount = VK_SAMPLE_COUNT_1_BIT
            }
        };
        mLightingRenderPass = std::make_shared<ade::AdVKRenderPass>(device, lightingAttachments, lightingSubpasses);

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
        mShadowDepth = builder.CreateTexture({
            .Name = "ShadowMap.Depth",
            .Extent = { ShadowMapSize, ShadowMapSize },
            .Format = depthFormat,
            .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .SampleCount = VK_SAMPLE_COUNT_1_BIT,
            .InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .bResizeWithBackbuffer = false
        });
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
        mGBufferWorldPosition = builder.CreateTexture({
            .Name = "GBuffer.WorldPosition",
            .Extent = extent,
            .Format = VK_FORMAT_R16G16B16A16_SFLOAT,
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
        mLightingColor = builder.CreateTexture({
            .Name = "LightingColor",
            .Extent = extent,
            .Format = VK_FORMAT_R16G16B16A16_SFLOAT,
            .Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .AspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .SampleCount = VK_SAMPLE_COUNT_1_BIT,
            .InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .bResizeWithBackbuffer = true
        });
    }

    void BuildGraphPasses() {
        mGraph->ClearPasses();
        mGraph->AddPass("RG.ShadowDepthPass", {}, { mShadowDepth },
            [this](ade::AdRenderGraphContext &context) {
                context.DiscardTexture(mShadowDepth);

                mShadowRenderTarget->BeginAt(context.GetCommandBuffer(), context.GetFrameSlot());
                mShadowRenderTarget->RenderMaterialSystems(context.GetCommandBuffer());
                mShadowRenderTarget->End(context.GetCommandBuffer());

                context.SetTextureLayout(mShadowDepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            });

        mGraph->AddPass("RG.GBufferPass", {}, { mGBufferBaseColor, mGBufferNormal, mGBufferMaterial, mGBufferWorldPosition, mGBufferDepth },
            [this](ade::AdRenderGraphContext &context) {
                context.DiscardTexture(mGBufferBaseColor);
                context.DiscardTexture(mGBufferNormal);
                context.DiscardTexture(mGBufferMaterial);
                context.DiscardTexture(mGBufferWorldPosition);
                context.DiscardTexture(mGBufferDepth);

                mGBufferRenderTarget->BeginAt(context.GetCommandBuffer(), context.GetFrameSlot());
                mGBufferRenderTarget->RenderMaterialSystems(context.GetCommandBuffer());
                mGBufferRenderTarget->End(context.GetCommandBuffer());

                context.SetTextureLayout(mGBufferBaseColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                context.SetTextureLayout(mGBufferNormal, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                context.SetTextureLayout(mGBufferMaterial, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                context.SetTextureLayout(mGBufferWorldPosition, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                context.SetTextureLayout(mGBufferDepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            });

        mGraph->AddPass("RG.IBLDeferredLighting", { mGBufferBaseColor, mGBufferNormal, mGBufferMaterial, mGBufferWorldPosition, mShadowDepth }, { mLightingColor },
            [this](ade::AdRenderGraphContext &context) {
                context.TransitionTexture(mGBufferBaseColor, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                context.TransitionTexture(mGBufferNormal, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                context.TransitionTexture(mGBufferMaterial, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                context.TransitionTexture(mGBufferWorldPosition, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                context.TransitionTexture(mShadowDepth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                context.DiscardTexture(mLightingColor);

                mLightingPass->Render(context.GetCommandBuffer(),
                                      mLightingRenderTarget.get(),
                                      context.GetFrameSlot(),
                                      context.GetTextureImage(mGBufferBaseColor),
                                      context.GetTextureImage(mGBufferNormal),
                                      context.GetTextureImage(mGBufferMaterial),
                                      context.GetTextureImage(mGBufferWorldPosition),
                                      context.GetTextureImage(mShadowDepth),
                                      mEnvironmentMap.get(),
                                      mLightSettings);

                context.SetTextureLayout(mLightingColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            });

        mGraph->AddPass("RG.IBLDebugComposite", { mLightingColor, mGBufferBaseColor, mGBufferNormal, mGBufferMaterial, mGBufferWorldPosition, mShadowDepth }, {},
            [this](ade::AdRenderGraphContext &context) {
                context.TransitionTexture(mLightingColor, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                mDebugSettings.debugParams = glm::vec4(static_cast<float>(mDebugViewMode), 1.f, 2.2f, 0.f);
                mDebugCompositePass->Render(context.GetCommandBuffer(),
                                            mPresentRenderTarget.get(),
                                            context.GetFrameSlot(),
                                            context.GetImageIndex(),
                                            context.GetTextureImage(mLightingColor),
                                            context.GetTextureImage(mGBufferBaseColor),
                                            context.GetTextureImage(mGBufferNormal),
                                            context.GetTextureImage(mGBufferMaterial),
                                            context.GetTextureImage(mGBufferWorldPosition),
                                            context.GetTextureImage(mShadowDepth),
                                            mEnvironmentMap.get(),
                                            mDebugSettings);
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
                mGraph->BuildAttachmentImages({ mGBufferBaseColor, mGBufferNormal, mGBufferMaterial, mGBufferWorldPosition, mGBufferDepth }),
                extent);
        mLightingRenderTarget->SetExternalImages(mGraph->BuildAttachmentImages({ mLightingColor }), extent);
        mPresentRenderTarget->SetBufferCount(static_cast<uint32_t>(swapchain->GetImages().size()));
        mPresentRenderTarget->SetExtent(extent);
        mLightingPass->InvalidateSourceViews();
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
                    mDebugViewMode = ade::AdIBLDebugViewMode::Lit;
                    LOG_I("IBL debug view: Lit + Sky");
                    break;
                case ade::KEY_2:
                    mDebugViewMode = ade::AdIBLDebugViewMode::Environment;
                    LOG_I("IBL debug view: Environment");
                    break;
                case ade::KEY_3:
                    mDebugViewMode = ade::AdIBLDebugViewMode::IBLDiffuse;
                    LOG_I("IBL debug view: IBL Diffuse");
                    break;
                case ade::KEY_4:
                    mDebugViewMode = ade::AdIBLDebugViewMode::IBLSpecular;
                    LOG_I("IBL debug view: IBL Specular");
                    break;
                case ade::KEY_5:
                    mDebugViewMode = ade::AdIBLDebugViewMode::ShadowFactor;
                    LOG_I("IBL debug view: ShadowFactor");
                    break;
                case ade::KEY_6:
                    mDebugViewMode = ade::AdIBLDebugViewMode::BaseColor;
                    LOG_I("IBL debug view: BaseColor");
                    break;
                case ade::KEY_7:
                    mDebugViewMode = ade::AdIBLDebugViewMode::Normal;
                    LOG_I("IBL debug view: Normal");
                    break;
                case ade::KEY_8:
                    mDebugViewMode = ade::AdIBLDebugViewMode::Roughness;
                    LOG_I("IBL debug view: Roughness");
                    break;
                case ade::KEY_9:
                    mDebugViewMode = ade::AdIBLDebugViewMode::Metallic;
                    LOG_I("IBL debug view: Metallic");
                    break;
                default:
                    break;
            }
        });
        mObserver->OnEvent<ade::AdMouseScrollEvent>([this](const ade::AdMouseScrollEvent &event){
            if(!mCamera || !ade::AdEntity::HasComponent<ade::AdLookAtCameraComponent>(mCamera)){
                return;
            }

            auto &cameraComp = mCamera->GetComponent<ade::AdLookAtCameraComponent>();
            float radius = cameraComp.GetRadius() + event.mYOffset * -0.3f;
            radius = glm::clamp(radius, 1.2f, 8.0f);
            cameraComp.SetRadius(radius);
        });
    }

    void UpdateOrbitCamera() {
        if(!mCamera || !mWindow || !ade::AdEntity::HasComponent<ade::AdLookAtCameraComponent>(mCamera)){
            return;
        }

        if(!mWindow->IsMouseDown()){
            bFirstMouseDrag = true;
            return;
        }

        glm::vec2 mousePos;
        mWindow->GetMousePos(mousePos);
        glm::vec2 mousePosDelta = { mLastMousePos.x - mousePos.x, mousePos.y - mLastMousePos.y };
        mLastMousePos = mousePos;

        if(std::abs(mousePosDelta.x) < 0.1f && std::abs(mousePosDelta.y) < 0.1f){
            return;
        }
        if(bFirstMouseDrag){
            bFirstMouseDrag = false;
            return;
        }

        auto &transComp = mCamera->GetComponent<ade::AdTransformComponent>();
        transComp.rotation.x += mousePosDelta.x * MouseSensitivity;
        transComp.rotation.y = glm::clamp(transComp.rotation.y + mousePosDelta.y * MouseSensitivity, -82.0f, 82.0f);
    }

    std::shared_ptr<ade::AdRenderer> mRenderer;
    std::shared_ptr<ade::AdRenderGraph> mGraph;
    ade::AdRGTextureHandle mShadowDepth;
    ade::AdRGTextureHandle mGBufferBaseColor;
    ade::AdRGTextureHandle mGBufferNormal;
    ade::AdRGTextureHandle mGBufferMaterial;
    ade::AdRGTextureHandle mGBufferWorldPosition;
    ade::AdRGTextureHandle mGBufferDepth;
    ade::AdRGTextureHandle mLightingColor;

    std::shared_ptr<ade::AdVKRenderPass> mShadowRenderPass;
    std::shared_ptr<ade::AdVKRenderPass> mGBufferRenderPass;
    std::shared_ptr<ade::AdVKRenderPass> mLightingRenderPass;
    std::shared_ptr<ade::AdVKRenderPass> mPresentRenderPass;
    std::shared_ptr<ade::AdRenderTarget> mShadowRenderTarget;
    std::shared_ptr<ade::AdRenderTarget> mGBufferRenderTarget;
    std::shared_ptr<ade::AdRenderTarget> mLightingRenderTarget;
    std::shared_ptr<ade::AdRenderTarget> mPresentRenderTarget;
    std::shared_ptr<ade::AdIBLDeferredLightingPass> mLightingPass;
    std::shared_ptr<ade::AdIBLDebugCompositePass> mDebugCompositePass;
    ade::AdIBLDebugViewMode mDebugViewMode = ade::AdIBLDebugViewMode::Lit;
    ade::AdIBLLightSettings mLightSettings{};
    ade::AdIBLDebugSettings mDebugSettings{};

    std::vector<VkCommandBuffer> mCmdBuffers;
    std::shared_ptr<ade::AdMesh> mCubeMesh;
    std::shared_ptr<ade::AdHDRTexture> mEnvironmentMap;
    std::shared_ptr<ade::AdEventObserver> mObserver;
    ade::AdEntity *mCamera = nullptr;
    bool bFirstMouseDrag = true;
    glm::vec2 mLastMousePos{ 0.f, 0.f };
    static constexpr float MouseSensitivity = 0.25f;
};

ade::AdApplication *CreateApplicationEntryPoint(){
    return new IBLApp();
}
