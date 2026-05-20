#include "ECS/System/AdGLTFPBRGBufferMaterialSystem.h"

#include "AdFileUtil.h"
#include "ECS/AdEntity.h"
#include "Graphic/AdVKBuffer.h"
#include "Graphic/AdVKDescriptorSet.h"
#include "Graphic/AdVKFrameBuffer.h"
#include "Graphic/AdVKImageView.h"
#include "Graphic/AdVKPipeline.h"
#include "Graphic/AdVKRenderPass.h"
#include "Render/AdMesh.h"
#include "Render/AdRenderTarget.h"

namespace ade{
    namespace{
        constexpr uint32_t MaxMaterialDescriptors = 256;

        glm::vec4 BuildWorldRow(const glm::mat4 &matrix, uint32_t row) {
            return {
                matrix[0][row],
                matrix[1][row],
                matrix[2][row],
                matrix[3][row]
            };
        }
    }

    void AdGLTFPBRGBufferMaterialSystem::OnInit(AdVKRenderPass *renderPass) {
        AdVKDevice *device = GetDevice();

        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
            { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
            { .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
            { .binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
            { .binding = 4, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
        };
        mMaterialDescSetLayout = std::make_shared<AdVKDescriptorSetLayout>(device, bindings);

        std::vector<VkDescriptorPoolSize> poolSizes = {
            { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = MaxMaterialDescriptors },
            { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = MaxMaterialDescriptors * 4 }
        };
        mDescriptorPool = std::make_shared<AdVKDescriptorPool>(device, MaxMaterialDescriptors, poolSizes);

        ShaderLayout shaderLayout = {
            .descriptorSetLayouts = { mMaterialDescSetLayout->GetHandle() },
            .pushConstants = {
                { .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = sizeof(GLTFPBRGBufferPushConstants) }
            }
        };
        mPipelineLayout = std::make_shared<AdVKPipelineLayout>(device,
                                                               AD_RES_SHADER_DIR"10_gltf_pbr_gbuffer.vert",
                                                               AD_RES_SHADER_DIR"10_gltf_pbr_gbuffer.frag",
                                                               shaderLayout);

        std::vector<VkVertexInputBindingDescription> vertexBindings = {
            { .binding = 0, .stride = sizeof(AdVertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX }
        };
        std::vector<VkVertexInputAttributeDescription> vertexAttrs = {
            { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(AdVertex, position) },
            { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(AdVertex, texcoord0) },
            { .location = 2, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(AdVertex, normal) },
            { .location = 3, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(AdVertex, tangent) }
        };

        mPipeline = std::make_shared<AdVKPipeline>(device, renderPass, mPipelineLayout.get());
        mPipeline->SetVertexInputState(vertexBindings, vertexAttrs);
        mPipeline->SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)->EnableDepthTest();
        mPipeline->SetDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR });
        mPipeline->SetMultisampleState(renderPass->GetSubPassSampleCount(), VK_FALSE);
        mPipeline->SetColorBlendAttachmentCount(4);
        mPipeline->Create();
    }

    void AdGLTFPBRGBufferMaterialSystem::OnRender(VkCommandBuffer cmdBuffer, AdRenderTarget *renderTarget) {
        AdScene *scene = GetScene();
        if(!scene){
            return;
        }

        entt::registry &reg = scene->GetEcsRegistry();
        auto view = reg.view<AdTransformComponent, AdGLTFModelComponent>();
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

        view.each([this, &cmdBuffer, &projMat, &viewMat](const auto&, const AdTransformComponent &transComp, const AdGLTFModelComponent &modelComp){
            if(!modelComp.Model || !modelComp.Model->IsLoaded()){
                return;
            }

            const auto &materials = modelComp.Model->GetMaterials();
            glm::mat4 entityTransform = transComp.GetTransform();
            for(const AdGLTFPrimitive &primitive : modelComp.Model->GetPrimitives()){
                if(!primitive.Mesh || primitive.MaterialIndex >= materials.size()){
                    continue;
                }

                VkDescriptorSet materialSet = EnsureMaterialDescriptor(&materials[primitive.MaterialIndex]);
                if(materialSet == VK_NULL_HANDLE){
                    continue;
                }

                glm::mat4 modelMat = entityTransform * primitive.NodeTransform;
                GLTFPBRGBufferPushConstants pushConstants{};
                pushConstants.mvp = projMat * viewMat * modelMat;
                pushConstants.worldRows[0] = BuildWorldRow(modelMat, 0);
                pushConstants.worldRows[1] = BuildWorldRow(modelMat, 1);
                pushConstants.worldRows[2] = BuildWorldRow(modelMat, 2);
                pushConstants.debugFeatureFlags = glm::vec4(mDebugNormalMapEnabled ? 1.f : 0.f,
                                                            mDebugAmbientOcclusionEnabled ? 1.f : 0.f,
                                                            0.f,
                                                            0.f);
                vkCmdPushConstants(cmdBuffer, mPipelineLayout->GetHandle(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(pushConstants), &pushConstants);
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout->GetHandle(),
                                        0, 1, &materialSet, 0, nullptr);
                primitive.Mesh->Draw(cmdBuffer);
            }
        });
    }

    void AdGLTFPBRGBufferMaterialSystem::SetDebugFeatureFlags(bool normalMapEnabled, bool ambientOcclusionEnabled) {
        mDebugNormalMapEnabled = normalMapEnabled;
        mDebugAmbientOcclusionEnabled = ambientOcclusionEnabled;
    }

    VkDescriptorSet AdGLTFPBRGBufferMaterialSystem::EnsureMaterialDescriptor(const AdGLTFPBRMaterial *material) {
        if(!material){
            return VK_NULL_HANDLE;
        }
        auto iter = mMaterialDescriptors.find(material);
        if(iter != mMaterialDescriptors.end()){
            return iter->second.DescriptorSet;
        }
        if(mAllocatedMaterialDescriptors >= MaxMaterialDescriptors){
            LOG_W("Too many glTF material descriptors. Max supported: {0}", MaxMaterialDescriptors);
            return VK_NULL_HANDLE;
        }

        AdVKDevice *device = GetDevice();
        MaterialDescriptor descriptor{};
        GLTFPBRMaterialParams params{};
        params.baseColorFactor = material->BaseColorFactor;
        params.pbrParams = glm::vec4(material->MetallicFactor, material->RoughnessFactor, material->NormalScale, material->OcclusionStrength);
        params.featureFlags = glm::vec4(material->bNormalMapEnabled ? 1.f : 0.f, 0.f, 0.f, 0.f);
        descriptor.ParamBuffer = std::make_shared<AdVKBuffer>(device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                              sizeof(GLTFPBRMaterialParams), &params, true);

        auto allocated = mDescriptorPool->AllocateDescriptorSet(mMaterialDescSetLayout.get(), 1);
        if(allocated.empty()){
            return VK_NULL_HANDLE;
        }
        descriptor.DescriptorSet = allocated[0];

        VkDescriptorBufferInfo paramInfo = DescriptorSetWriter::BuildBufferInfo(descriptor.ParamBuffer->GetHandle(), 0, sizeof(GLTFPBRMaterialParams));
        std::array<VkDescriptorImageInfo, 4> imageInfos{};
        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(5);
        writes.push_back(DescriptorSetWriter::WriteBuffer(descriptor.DescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &paramInfo));

        for(uint32_t slot = 0; slot < AD_GLTF_TEXTURE_COUNT; slot++){
            const AdGLTFTextureBinding &binding = material->Textures[slot];
            if(!binding.Texture || !binding.Sampler || !binding.Texture->GetImageView()){
                LOG_W("Invalid glTF material texture binding in material {0}", material->Name);
                return VK_NULL_HANDLE;
            }
            imageInfos[slot] = DescriptorSetWriter::BuildImageInfo(binding.Sampler->GetHandle(), binding.Texture->GetImageView()->GetHandle());
            writes.push_back(DescriptorSetWriter::WriteImage(descriptor.DescriptorSet, slot + 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfos[slot]));
        }

        DescriptorSetWriter::UpdateDescriptorSets(device->GetHandle(), writes);
        mAllocatedMaterialDescriptors++;
        VkDescriptorSet result = descriptor.DescriptorSet;
        mMaterialDescriptors.insert({ material, descriptor });
        return result;
    }

    void AdGLTFPBRGBufferMaterialSystem::OnDestroy() {
        mMaterialDescriptors.clear();
        mPipeline.reset();
        mPipelineLayout.reset();
        mDescriptorPool.reset();
        mMaterialDescSetLayout.reset();
        mAllocatedMaterialDescriptors = 0;
    }
}
