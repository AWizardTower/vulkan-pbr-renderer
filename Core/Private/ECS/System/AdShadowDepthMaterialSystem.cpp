#include "ECS/System/AdShadowDepthMaterialSystem.h"

#include "AdFileUtil.h"
#include "Graphic/AdVKFrameBuffer.h"
#include "Graphic/AdVKPipeline.h"
#include "Graphic/AdVKRenderPass.h"
#include "Render/AdMesh.h"
#include "Render/AdRenderTarget.h"

#include "ECS/AdEntity.h"

namespace ade{
    AdShadowDepthMaterialSystem::AdShadowDepthMaterialSystem(const glm::mat4 *lightViewProj)
            : mLightViewProj(lightViewProj) {
    }

    void AdShadowDepthMaterialSystem::OnInit(AdVKRenderPass *renderPass) {
        AdVKDevice *device = GetDevice();

        ShaderLayout shaderLayout = {
            .pushConstants = {
                {
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                    .offset = 0,
                    .size = sizeof(ShadowDepthPushConstants)
                }
            }
        };
        mPipelineLayout = std::make_shared<AdVKPipelineLayout>(device,
                                                               AD_RES_SHADER_DIR"07_shadow_depth.vert",
                                                               AD_RES_SHADER_DIR"07_shadow_depth.frag",
                                                               shaderLayout);

        std::vector<VkVertexInputBindingDescription> vertexBindings = {
            {
                .binding = 0,
                .stride = sizeof(AdVertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            }
        };
        std::vector<VkVertexInputAttributeDescription> vertexAttrs = {
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(AdVertex, position)
            }
        };

        PipelineRasterizationState rasterizationState{};
        rasterizationState.cullMode = VK_CULL_MODE_NONE;
        rasterizationState.depthBiasEnable = VK_TRUE;
        rasterizationState.depthBiasConstantFactor = 1.25f;
        rasterizationState.depthBiasSlopeFactor = 1.75f;
        rasterizationState.depthBiasClamp = 0.0f;

        mPipeline = std::make_shared<AdVKPipeline>(device, renderPass, mPipelineLayout.get());
        mPipeline->SetVertexInputState(vertexBindings, vertexAttrs);
        mPipeline->SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)->EnableDepthTest();
        mPipeline->SetRasterizationState(rasterizationState);
        mPipeline->SetDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR });
        mPipeline->SetMultisampleState(renderPass->GetSubPassSampleCount(), VK_FALSE);
        mPipeline->SetColorBlendAttachmentCount(0);
        mPipeline->Create();
    }

    void AdShadowDepthMaterialSystem::OnRender(VkCommandBuffer cmdBuffer, AdRenderTarget *renderTarget) {
        AdScene *scene = GetScene();
        if(!scene || !mLightViewProj){
            return;
        }

        entt::registry &reg = scene->GetEcsRegistry();
        auto view = reg.view<AdTransformComponent, AdBaseMaterialComponent>();
        if(view.begin() == view.end()){
            return;
        }

        mPipeline->Bind(cmdBuffer);
        AdVKFrameBuffer *frameBuffer = renderTarget->GetFrameBuffer();
        VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = static_cast<float>(frameBuffer->GetWidth()),
            .height = static_cast<float>(frameBuffer->GetHeight()),
            .minDepth = 0.f,
            .maxDepth = 1.f
        };
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
        VkRect2D scissor = {
            .offset = { 0, 0 },
            .extent = { frameBuffer->GetWidth(), frameBuffer->GetHeight() }
        };
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

        view.each([this, &cmdBuffer](const auto &e, const AdTransformComponent &transComp, const AdBaseMaterialComponent &materialComp){
            auto meshMaterials = materialComp.GetMeshMaterials();
            glm::mat4 modelMat = transComp.GetTransform();

            ShadowDepthPushConstants pushConstants{};
            pushConstants.lightMVP = (*mLightViewProj) * modelMat;
            vkCmdPushConstants(cmdBuffer, mPipelineLayout->GetHandle(),
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0, sizeof(pushConstants), &pushConstants);

            for (const auto &entry: meshMaterials){
                for (const auto &meshIndex: entry.second){
                    AdMesh *mesh = materialComp.GetMesh(meshIndex);
                    if(mesh){
                        mesh->Draw(cmdBuffer);
                    }
                }
            }
        });
    }

    void AdShadowDepthMaterialSystem::OnDestroy() {
        mPipeline.reset();
        mPipelineLayout.reset();
    }
}
