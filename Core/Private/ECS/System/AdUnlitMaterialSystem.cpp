#include "ECS/System/AdUnlitMaterialSystem.h"

#include "AdFileUtil.h"
#include "AdApplication.h"
#include "Graphic/AdVKPipeline.h"
#include "Graphic/AdVKDescriptorSet.h"
#include "Graphic/AdVKImageView.h"
#include "Graphic/AdVKFrameBuffer.h"
#include "Graphic/AdVKRenderPass.h"

#include "Render/AdRenderTarget.h"
#include "Render/AdRenderer.h"

#include "ECS/Component/AdTransformComponent.h"

namespace ade{
    void AdUnlitMaterialSystem::OnInit(AdVKRenderPass *renderPass) {
        AdVKDevice *device = GetDevice();

        //Frame Ubo
        {
             const std::vector<VkDescriptorSetLayoutBinding> bindings = {
                {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                }
            };
            mFrameUboDescSetLayout = std::make_shared<AdVKDescriptorSetLayout>(device, bindings);
        }

        // Material Params
        {
            const std::vector<VkDescriptorSetLayoutBinding> bindings = {
                {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                }
            };
            mMaterialParamDescSetLayout = std::make_shared<AdVKDescriptorSetLayout>(device, bindings);
        }

        // Material Resource
        {
            const std::vector<VkDescriptorSetLayoutBinding> bindings = {
                {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                },
                {
                    .binding = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                }
            };
            mMaterialResourceDescSetLayout = std::make_shared<AdVKDescriptorSetLayout>(device, bindings);
        }

        VkPushConstantRange modelPC = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(ModelPC)
        };

        ShaderLayout shaderLayout = {
            .descriptorSetLayouts = { mFrameUboDescSetLayout->GetHandle(), mMaterialParamDescSetLayout->GetHandle(), mMaterialResourceDescSetLayout->GetHandle() },
            .pushConstants = { modelPC }
        };
        mPipelineLayout = std::make_shared<AdVKPipelineLayout>(device,
                                                               AD_RES_SHADER_DIR"03_unlit_material.vert",
                                                               AD_RES_SHADER_DIR"03_unlit_material.frag",
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
        mPipeline->EnableDepthTest();
        mPipeline->SetDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR });
        mPipeline->SetMultisampleState(renderPass->GetSubPassSampleCount(), VK_FALSE);
        mPipeline->Create();

        AdRenderer *renderer = GetRenderer();
        uint32_t framesInFlight = renderer ? renderer->GetFramesInFlight() : RENDERER_NUM_BUFFER;
        if(framesInFlight == 0){
            framesInFlight = RENDERER_NUM_BUFFER;
        }
        mFrameDescriptors.resize(framesInFlight);
        for(auto &frame: mFrameDescriptors){
            std::vector<VkDescriptorPoolSize> poolSizes = {
                {
                    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1
                }
            };
            frame.FrameDescriptorPool = std::make_shared<AdVKDescriptorPool>(device, 1, poolSizes);
            frame.FrameUboDescSet = frame.FrameDescriptorPool->AllocateDescriptorSet(mFrameUboDescSetLayout.get(), 1)[0];
            frame.FrameUboBuffer = std::make_shared<ade::AdVKBuffer>(device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(FrameUbo), nullptr, true);
        }

        ReCreateMaterialDescPool(NUM_MATERIAL_BATCH);
    }

    void AdUnlitMaterialSystem::OnRender(VkCommandBuffer cmdBuffer, AdRenderTarget *renderTarget) {
        AdScene *scene = GetScene();
        if(!scene){
            return;
        }
        entt::registry &reg = scene->GetEcsRegistry();

        auto view = reg.view<AdTransformComponent, AdUnlitMaterialComponent>();
        if(view.begin() == view.end()){
            return;
        }
        if(mFrameDescriptors.empty()){
            return;
        }

        PerFrameDescriptors &frame = mFrameDescriptors[GetCurrentFrameSlot() % mFrameDescriptors.size()];

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

        UpdateFrameUboDescSet(frame, renderTarget);

        uint32_t materialCount = AdMaterialFactory::GetInstance()->GetMaterialSize<AdUnlitMaterial>();
        if(materialCount > mLastDescriptorSetCount){
            ReCreateMaterialDescPool(materialCount);
        }

        std::vector<bool> updateFlags(materialCount);
        view.each([this, &frame, &updateFlags, &cmdBuffer](AdTransformComponent &transComp, AdUnlitMaterialComponent &materialComp){
            for (const auto &entry: materialComp.GetMeshMaterials()){
                AdUnlitMaterial *material = entry.first;
                if(!material || material->GetIndex() < 0){
                    LOG_W("TODO: default material or error material ?");
                    continue;
                }

                uint32_t materialIndex = material->GetIndex();
                if(materialIndex >= updateFlags.size()){
                    LOG_W("Material update index out of range: {0}", materialIndex);
                    continue;
                }
                if(materialIndex >= frame.MaterialParamDescSets.size() || materialIndex >= frame.MaterialResourceDescSets.size()){
                    LOG_W("Material descriptor index out of range: {0}", materialIndex);
                    continue;
                }

                VkDescriptorSet paramsDescSet = frame.MaterialParamDescSets[materialIndex];
                VkDescriptorSet resourceDescSet = frame.MaterialResourceDescSets[materialIndex];

                if(!updateFlags[materialIndex]){
                    if(frame.SyncedParamsVersions[materialIndex] != material->GetParamsVersion()){
                        //LOG_T("Update material params : {0}", materialIndex);
                        UpdateMaterialParamsDescSet(frame, paramsDescSet, material);
                        frame.SyncedParamsVersions[materialIndex] = material->GetParamsVersion();
                        material->FinishFlushParams();
                    }
                    if(frame.SyncedResourceVersions[materialIndex] != material->GetResourceVersion()){
                        //LOG_T("Update material resource : {0}", materialIndex);
                        UpdateMaterialResourceDescSet(resourceDescSet, material);
                        frame.SyncedResourceVersions[materialIndex] = material->GetResourceVersion();
                        material->FinishFlushResource();
                    }
                    updateFlags[materialIndex] = true;
                }

                VkDescriptorSet descriptorSets[] = { frame.FrameUboDescSet, paramsDescSet, resourceDescSet };
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout->GetHandle(),
                                        0, ARRAY_SIZE(descriptorSets), descriptorSets, 0, nullptr);

                ModelPC pc = { transComp.GetTransform() };
                vkCmdPushConstants(cmdBuffer, mPipelineLayout->GetHandle(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

                for (const auto &meshIndex: entry.second){
                    materialComp.GetMesh(meshIndex)->Draw(cmdBuffer);
                }
            }
        });
    }

    void AdUnlitMaterialSystem::OnDestroy() {
        mFrameDescriptors.clear();
        mPipeline.reset();
        mPipelineLayout.reset();
        mMaterialResourceDescSetLayout.reset();
        mMaterialParamDescSetLayout.reset();
        mFrameUboDescSetLayout.reset();
    }

    void AdUnlitMaterialSystem::ReCreateMaterialDescPool(uint32_t materialCount) {
        AdVKDevice *device = GetDevice();

        uint32_t newDescriptorSetCount = mLastDescriptorSetCount;
        if(mLastDescriptorSetCount == 0){
            newDescriptorSetCount = NUM_MATERIAL_BATCH;
        }

        while (newDescriptorSetCount < materialCount) {
            newDescriptorSetCount *= 2;
        }

        if(newDescriptorSetCount > NUM_MATERIAL_BATCH_MAX){
            LOG_E("Descriptor Set max count is : {0}, but request : {1}", NUM_MATERIAL_BATCH_MAX, newDescriptorSetCount);
            return;
        }

        LOG_W("{0}: {1} -> {2} S.", __FUNCTION__, mLastDescriptorSetCount, newDescriptorSetCount);

        std::vector<VkDescriptorPoolSize> poolSizes = {
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = newDescriptorSetCount
            },
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = newDescriptorSetCount * 2               // because has color_tex0 and color_tex1
            }
        };

        std::vector<std::shared_ptr<AdVKDescriptorPool>> oldPools;
        std::vector<std::shared_ptr<AdVKBuffer>> oldBuffers;

        for(auto &frame: mFrameDescriptors){
            if(frame.MaterialDescriptorPool){
                oldPools.push_back(frame.MaterialDescriptorPool);
            }
            oldBuffers.insert(oldBuffers.end(), frame.MaterialBuffers.begin(), frame.MaterialBuffers.end());

            frame.MaterialParamDescSets.clear();
            frame.MaterialResourceDescSets.clear();
            frame.MaterialBuffers.clear();
            frame.SyncedParamsVersions.clear();
            frame.SyncedResourceVersions.clear();
            frame.MaterialDescriptorPool.reset();

            frame.MaterialDescriptorPool = std::make_shared<ade::AdVKDescriptorPool>(device, newDescriptorSetCount * 2, poolSizes);  //because has params and resource desc. set
            frame.MaterialParamDescSets = frame.MaterialDescriptorPool->AllocateDescriptorSet(mMaterialParamDescSetLayout.get(), newDescriptorSetCount);
            frame.MaterialResourceDescSets = frame.MaterialDescriptorPool->AllocateDescriptorSet(mMaterialResourceDescSetLayout.get(), newDescriptorSetCount);
            assert(frame.MaterialParamDescSets.size() == newDescriptorSetCount && "Failed to AllocateDescriptorSet");
            assert(frame.MaterialResourceDescSets.size() == newDescriptorSetCount && "Failed to AllocateDescriptorSet");

            frame.MaterialBuffers.reserve(newDescriptorSetCount);
            for(int i = 0; i < newDescriptorSetCount; i++){
                frame.MaterialBuffers.push_back(std::make_shared<AdVKBuffer>(device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(UnlitMaterialUbo), nullptr, true));
            }
            frame.SyncedParamsVersions.resize(newDescriptorSetCount, std::numeric_limits<uint64_t>::max());
            frame.SyncedResourceVersions.resize(newDescriptorSetCount, std::numeric_limits<uint64_t>::max());
        }

        if(!oldPools.empty() || !oldBuffers.empty()){
            AdRenderer *renderer = GetRenderer();
            if(renderer){
                renderer->EnqueueDeferredDelete([oldPools = std::move(oldPools), oldBuffers = std::move(oldBuffers)]() mutable {
                    oldPools.clear();
                    oldBuffers.clear();
                });
            } else {
                oldPools.clear();
                oldBuffers.clear();
            }
        }

        LOG_W("{0}: {1} -> {2} E.", __FUNCTION__, mLastDescriptorSetCount, newDescriptorSetCount);
        mLastDescriptorSetCount = newDescriptorSetCount;
    }

    void AdUnlitMaterialSystem::UpdateFrameUboDescSet(PerFrameDescriptors &frame, AdRenderTarget *renderTarget) {
        AdApplication *app = GetApp();
        AdVKDevice *device = GetDevice();

        AdVKFrameBuffer *frameBuffer = renderTarget->GetFrameBuffer();
        glm::ivec2 resolution = { frameBuffer->GetWidth(), frameBuffer->GetHeight() };

        FrameUbo frameUbo = {
            .projMat = GetProjMat(renderTarget),
            .viewMat = GetViewMat(renderTarget),
            .resolution = resolution,
            .frameId = static_cast<uint32_t>(app->GetFrameIndex()),
            .time = app->GetStartTimeSecond()
        };

        frame.FrameUboBuffer->WriteData(&frameUbo);
        VkDescriptorBufferInfo bufferInfo = DescriptorSetWriter::BuildBufferInfo(frame.FrameUboBuffer->GetHandle(), 0, sizeof(frameUbo));
        VkWriteDescriptorSet bufferWrite = DescriptorSetWriter::WriteBuffer(frame.FrameUboDescSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufferInfo);
        DescriptorSetWriter::UpdateDescriptorSets(device->GetHandle(), { bufferWrite });
    }

    void AdUnlitMaterialSystem::UpdateMaterialParamsDescSet(PerFrameDescriptors &frame, VkDescriptorSet descSet, AdUnlitMaterial *material) {
        AdVKDevice *device = GetDevice();

        AdVKBuffer *materialBuffer = frame.MaterialBuffers[material->GetIndex()].get();

        UnlitMaterialUbo params = material->GetParams();

        const TextureView *texture0 = material->GetTextureView(UNLIT_MAT_BASE_COLOR_0);
        if(texture0){
            AdMaterial::UpdateTextureParams(texture0, &params.textureParam0);
        }

        const TextureView *texture1 = material->GetTextureView(UNLIT_MAT_BASE_COLOR_1);
        if(texture1){
            AdMaterial::UpdateTextureParams(texture1, &params.textureParam1);
        }

        materialBuffer->WriteData(&params);
        VkDescriptorBufferInfo bufferInfo = DescriptorSetWriter::BuildBufferInfo(materialBuffer->GetHandle(), 0, sizeof(params));
        VkWriteDescriptorSet bufferWrite = DescriptorSetWriter::WriteBuffer(descSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufferInfo);
        DescriptorSetWriter::UpdateDescriptorSets(device->GetHandle(), { bufferWrite });
    }

    void AdUnlitMaterialSystem::UpdateMaterialResourceDescSet(VkDescriptorSet descSet, AdUnlitMaterial *material) {
        AdVKDevice *device = GetDevice();

        const TextureView *texture0 = material->GetTextureView(UNLIT_MAT_BASE_COLOR_0);
        const TextureView *texture1 = material->GetTextureView(UNLIT_MAT_BASE_COLOR_1);

        if(!texture0 || !texture1 || !texture0->texture || !texture0->sampler || !texture1->texture || !texture1->sampler){
            LOG_W("Unlit material {0} has invalid texture binding.", material->GetIndex());
            return;
        }

        VkDescriptorImageInfo textureInfo0 = DescriptorSetWriter::BuildImageInfo(texture0->sampler->GetHandle(), texture0->texture->GetImageView()->GetHandle());
        VkDescriptorImageInfo textureInfo1 = DescriptorSetWriter::BuildImageInfo(texture1->sampler->GetHandle(), texture1->texture->GetImageView()->GetHandle());

        VkWriteDescriptorSet textureWrite0 = DescriptorSetWriter::WriteImage(descSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &textureInfo0);
        VkWriteDescriptorSet textureWrite1 = DescriptorSetWriter::WriteImage(descSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &textureInfo1);

        DescriptorSetWriter::UpdateDescriptorSets(device->GetHandle(), { textureWrite0, textureWrite1 });
    }
}
