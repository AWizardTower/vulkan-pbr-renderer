#include "ECS/System/AdPBRGBufferMaterialSystem.h"

#include "AdFileUtil.h"
#include "Graphic/AdVKFrameBuffer.h"
#include "Graphic/AdVKPipeline.h"
#include "Graphic/AdVKRenderPass.h"
#include "Render/AdMesh.h"
#include "Render/AdRenderTarget.h"

#include "ECS/AdEntity.h"

#include <algorithm>

namespace ade{
    namespace{
        glm::vec4 BuildWorldRow(const glm::mat4 &matrix, uint32_t row) {
            return {
                    matrix[0][row],
                    matrix[1][row],
                    matrix[2][row],
                    matrix[3][row]
            };
        }
    }

    void AdPBRGBufferMaterialSystem::OnInit(AdVKRenderPass *renderPass) {
        AdVKDevice *device = GetDevice();

        ShaderLayout shaderLayout = {
            .pushConstants = {
                {
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    .offset = 0,
                    .size = sizeof(PBRGBufferPushConstants)
                }
            }
        };
        mPipelineLayout = std::make_shared<AdVKPipelineLayout>(device,
                                                               AD_RES_SHADER_DIR"06_pbr_gbuffer.vert",
                                                               AD_RES_SHADER_DIR"06_pbr_gbuffer.frag",
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
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(AdVertex, texcoord0)
            },
            {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(AdVertex, normal)
            }
        };

        mPipeline = std::make_shared<AdVKPipeline>(device, renderPass, mPipelineLayout.get());
        mPipeline->SetVertexInputState(vertexBindings, vertexAttrs);
        mPipeline->SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)->EnableDepthTest();
        mPipeline->SetDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR });
        mPipeline->SetMultisampleState(renderPass->GetSubPassSampleCount(), VK_FALSE);
        mPipeline->SetColorBlendAttachmentCount(4);
        mPipeline->Create();
    }

    void AdPBRGBufferMaterialSystem::OnRender(VkCommandBuffer cmdBuffer, AdRenderTarget *renderTarget) {
        AdScene *scene = GetScene();
        if(!scene){
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

        glm::mat4 projMat = GetProjMat(renderTarget);
        glm::mat4 viewMat = GetViewMat(renderTarget);

        view.each([this, &cmdBuffer, &projMat, &viewMat](const auto &e, const AdTransformComponent &transComp, const AdBaseMaterialComponent &materialComp){
            auto meshMaterials = materialComp.GetMeshMaterials();
            glm::mat4 modelMat = transComp.GetTransform();

            for (const auto &entry: meshMaterials){
                AdBaseMaterial *material = entry.first;
                if(!material){
                    LOG_W("TODO: default material or error material ?");
                    continue;
                }

                PBRGBufferPushConstants pushConstants{};
                pushConstants.mvp = projMat * viewMat * modelMat;
                pushConstants.worldRows[0] = BuildWorldRow(modelMat, 0);
                pushConstants.worldRows[1] = BuildWorldRow(modelMat, 1);
                pushConstants.worldRows[2] = BuildWorldRow(modelMat, 2);
                pushConstants.material = glm::vec4(
                        static_cast<float>(material->colorType),
                        std::clamp(material->roughness, 0.04f, 1.0f),
                        std::clamp(material->metallic, 0.0f, 1.0f),
                        0.f);
                vkCmdPushConstants(cmdBuffer, mPipelineLayout->GetHandle(),
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(pushConstants), &pushConstants);

                for (const auto &meshIndex: entry.second){
                    AdMesh *mesh = materialComp.GetMesh(meshIndex);
                    if(mesh){
                        mesh->Draw(cmdBuffer);
                    }
                }
            }
        });
    }

    void AdPBRGBufferMaterialSystem::OnDestroy() {
        mPipeline.reset();
        mPipelineLayout.reset();
    }
}
