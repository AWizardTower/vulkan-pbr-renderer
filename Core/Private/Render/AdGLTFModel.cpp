#include "Render/AdGLTFModel.h"

#include "AdFileUtil.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"
#include "MikkTSpace/mikktspace.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <unordered_map>

namespace ade{
    namespace{
        struct MikkMeshData{
            std::vector<AdVertex> *Vertices = nullptr;
            std::vector<uint32_t> *Indices = nullptr;
        };

        uint32_t MikkIndex(const MikkMeshData *data, int face, int vert) {
            return (*data->Indices)[static_cast<size_t>(face) * 3 + static_cast<size_t>(vert)];
        }

        int MikkGetNumFaces(const SMikkTSpaceContext *context) {
            const auto *data = static_cast<const MikkMeshData*>(context->m_pUserData);
            return static_cast<int>(data->Indices->size() / 3);
        }

        int MikkGetNumVerticesOfFace(const SMikkTSpaceContext*, const int) {
            return 3;
        }

        void MikkGetPosition(const SMikkTSpaceContext *context, float out[], const int face, const int vert) {
            const auto *data = static_cast<const MikkMeshData*>(context->m_pUserData);
            const glm::vec3 &position = (*data->Vertices)[MikkIndex(data, face, vert)].position;
            out[0] = position.x;
            out[1] = position.y;
            out[2] = position.z;
        }

        void MikkGetNormal(const SMikkTSpaceContext *context, float out[], const int face, const int vert) {
            const auto *data = static_cast<const MikkMeshData*>(context->m_pUserData);
            const glm::vec3 &normal = (*data->Vertices)[MikkIndex(data, face, vert)].normal;
            out[0] = normal.x;
            out[1] = normal.y;
            out[2] = normal.z;
        }

        void MikkGetTexCoord(const SMikkTSpaceContext *context, float out[], const int face, const int vert) {
            const auto *data = static_cast<const MikkMeshData*>(context->m_pUserData);
            const glm::vec2 &uv = (*data->Vertices)[MikkIndex(data, face, vert)].texcoord0;
            out[0] = uv.x;
            out[1] = uv.y;
        }

        void MikkSetTSpaceBasic(const SMikkTSpaceContext *context,
                                const float tangent[],
                                const float sign,
                                const int face,
                                const int vert) {
            auto *data = static_cast<MikkMeshData*>(context->m_pUserData);
            (*data->Vertices)[MikkIndex(data, face, vert)].tangent = glm::vec4(tangent[0], tangent[1], tangent[2], sign);
        }

        bool GenerateTangents(std::vector<AdVertex> &vertices, std::vector<uint32_t> &indices) {
            if(vertices.empty() || indices.empty() || indices.size() % 3 != 0){
                return false;
            }

            MikkMeshData meshData{ &vertices, &indices };
            SMikkTSpaceInterface mikkInterface{};
            mikkInterface.m_getNumFaces = MikkGetNumFaces;
            mikkInterface.m_getNumVerticesOfFace = MikkGetNumVerticesOfFace;
            mikkInterface.m_getPosition = MikkGetPosition;
            mikkInterface.m_getNormal = MikkGetNormal;
            mikkInterface.m_getTexCoord = MikkGetTexCoord;
            mikkInterface.m_setTSpaceBasic = MikkSetTSpaceBasic;

            SMikkTSpaceContext context{};
            context.m_pInterface = &mikkInterface;
            context.m_pUserData = &meshData;
            return genTangSpaceDefault(&context) != 0;
        }

        glm::mat4 ToMat4(const cgltf_float *matrix) {
            return glm::make_mat4(matrix);
        }

        const cgltf_accessor *FindAccessor(const cgltf_primitive &primitive, cgltf_attribute_type type) {
            return cgltf_find_accessor(&primitive, type, 0);
        }

        glm::vec3 ReadVec3(const cgltf_accessor *accessor, cgltf_size index, const glm::vec3 &fallback) {
            if(!accessor){
                return fallback;
            }
            cgltf_float values[4] = {};
            if(!cgltf_accessor_read_float(accessor, index, values, 3)){
                return fallback;
            }
            return { values[0], values[1], values[2] };
        }

        glm::vec2 ReadVec2(const cgltf_accessor *accessor, cgltf_size index, const glm::vec2 &fallback) {
            if(!accessor){
                return fallback;
            }
            cgltf_float values[4] = {};
            if(!cgltf_accessor_read_float(accessor, index, values, 2)){
                return fallback;
            }
            return { values[0], values[1] };
        }

        glm::vec4 ReadVec4(const cgltf_accessor *accessor, cgltf_size index, const glm::vec4 &fallback) {
            if(!accessor){
                return fallback;
            }
            cgltf_float values[4] = {};
            if(!cgltf_accessor_read_float(accessor, index, values, 4)){
                return fallback;
            }
            return { values[0], values[1], values[2], values[3] };
        }

        VkSamplerAddressMode ToAddressMode(cgltf_wrap_mode wrapMode) {
            switch (wrapMode) {
                case cgltf_wrap_mode_clamp_to_edge:
                    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                case cgltf_wrap_mode_mirrored_repeat:
                    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
                case cgltf_wrap_mode_repeat:
                default:
                    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            }
        }

        std::string TextureCacheKey(const std::filesystem::path &path, VkFormat format) {
            return path.string() + "#" + std::to_string(static_cast<int>(format));
        }

        std::shared_ptr<AdTexture> CreateFallbackTexture(VkFormat format, const RGBAColor &color) {
            RGBAColor pixel = color;
            return std::make_shared<AdTexture>(1, 1, &pixel, format);
        }

        AdGLTFTextureBinding LoadTextureBinding(const std::filesystem::path &baseDir,
                                                const cgltf_texture_view &view,
                                                VkFormat format,
                                                const RGBAColor &fallbackColor,
                                                std::unordered_map<std::string, std::shared_ptr<AdTexture>> &textureCache) {
            AdGLTFTextureBinding binding{};
            if(!view.texture || !view.texture->image || !view.texture->image->uri){
                binding.Texture = CreateFallbackTexture(format, fallbackColor);
                binding.Sampler = std::make_shared<AdSampler>(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 0.f, 0.f, VK_SAMPLER_MIPMAP_MODE_LINEAR);
                binding.bEnabled = false;
                return binding;
            }

            std::filesystem::path imagePath = baseDir / view.texture->image->uri;
            std::string cacheKey = TextureCacheKey(imagePath, format);
            auto iter = textureCache.find(cacheKey);
            if(iter != textureCache.end()){
                binding.Texture = iter->second;
            } else {
                binding.Texture = std::make_shared<AdTexture>(imagePath.string(), format, true, fallbackColor);
                textureCache.insert({ cacheKey, binding.Texture });
            }

            VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            if(view.texture->sampler){
                addressMode = ToAddressMode(view.texture->sampler->wrap_s);
            }
            float maxLod = static_cast<float>(std::max(1u, binding.Texture->GetMipLevels()) - 1);
            binding.Sampler = std::make_shared<AdSampler>(VK_FILTER_LINEAR, addressMode, 0.f, maxLod, VK_SAMPLER_MIPMAP_MODE_LINEAR);
            binding.bEnabled = true;
            return binding;
        }

        void ExpandBounds(glm::vec3 &boundsMin, glm::vec3 &boundsMax, const glm::vec3 &point) {
            boundsMin.x = std::min(boundsMin.x, point.x);
            boundsMin.y = std::min(boundsMin.y, point.y);
            boundsMin.z = std::min(boundsMin.z, point.z);
            boundsMax.x = std::max(boundsMax.x, point.x);
            boundsMax.y = std::max(boundsMax.y, point.y);
            boundsMax.z = std::max(boundsMax.z, point.z);
        }
    }

    void AdGLTFModel::Reset() {
        mPrimitives.clear();
        mMaterials.clear();
        mBoundsMin = glm::vec3(std::numeric_limits<float>::max());
        mBoundsMax = glm::vec3(std::numeric_limits<float>::lowest());
        mLoaded = false;
    }

    bool AdGLTFModel::LoadFromFile(const std::string &filePath) {
        Reset();

        cgltf_options options{};
        cgltf_data *rawData = nullptr;
        cgltf_result result = cgltf_parse_file(&options, filePath.c_str(), &rawData);
        if(result != cgltf_result_success || !rawData){
            LOG_E("Failed to parse glTF file: {0}", filePath);
            return false;
        }
        std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(rawData, cgltf_free);

        result = cgltf_load_buffers(&options, data.get(), filePath.c_str());
        if(result != cgltf_result_success){
            LOG_E("Failed to load glTF buffers: {0}", filePath);
            return false;
        }
        result = cgltf_validate(data.get());
        if(result != cgltf_result_success){
            LOG_E("Invalid glTF file: {0}", filePath);
            return false;
        }

        std::filesystem::path baseDir = std::filesystem::path(filePath).parent_path();
        std::unordered_map<std::string, std::shared_ptr<AdTexture>> textureCache;

        if(data->materials_count == 0){
            AdGLTFPBRMaterial material{};
            material.Name = "Default";
            material.Textures[AD_GLTF_TEXTURE_BASE_COLOR] = LoadTextureBinding(baseDir, {}, VK_FORMAT_R8G8B8A8_SRGB, { 255, 255, 255, 255 }, textureCache);
            material.Textures[AD_GLTF_TEXTURE_METALLIC_ROUGHNESS] = LoadTextureBinding(baseDir, {}, VK_FORMAT_R8G8B8A8_UNORM, { 255, 255, 255, 255 }, textureCache);
            material.Textures[AD_GLTF_TEXTURE_NORMAL] = LoadTextureBinding(baseDir, {}, VK_FORMAT_R8G8B8A8_UNORM, { 128, 128, 255, 255 }, textureCache);
            material.Textures[AD_GLTF_TEXTURE_OCCLUSION] = LoadTextureBinding(baseDir, {}, VK_FORMAT_R8G8B8A8_UNORM, { 255, 255, 255, 255 }, textureCache);
            mMaterials.push_back(material);
        } else {
            mMaterials.reserve(data->materials_count);
            for(cgltf_size i = 0; i < data->materials_count; i++){
                const cgltf_material &src = data->materials[i];
                AdGLTFPBRMaterial material{};
                material.Name = src.name ? src.name : ("Material_" + std::to_string(i));

                const cgltf_pbr_metallic_roughness *pbr = src.has_pbr_metallic_roughness ? &src.pbr_metallic_roughness : nullptr;
                if(pbr){
                    material.BaseColorFactor = glm::vec4(pbr->base_color_factor[0], pbr->base_color_factor[1], pbr->base_color_factor[2], pbr->base_color_factor[3]);
                    material.MetallicFactor = pbr->metallic_factor;
                    material.RoughnessFactor = pbr->roughness_factor;
                    material.Textures[AD_GLTF_TEXTURE_BASE_COLOR] = LoadTextureBinding(baseDir, pbr->base_color_texture, VK_FORMAT_R8G8B8A8_SRGB, { 255, 255, 255, 255 }, textureCache);
                    material.Textures[AD_GLTF_TEXTURE_METALLIC_ROUGHNESS] = LoadTextureBinding(baseDir, pbr->metallic_roughness_texture, VK_FORMAT_R8G8B8A8_UNORM, { 255, 255, 255, 255 }, textureCache);
                } else {
                    material.Textures[AD_GLTF_TEXTURE_BASE_COLOR] = LoadTextureBinding(baseDir, {}, VK_FORMAT_R8G8B8A8_SRGB, { 255, 255, 255, 255 }, textureCache);
                    material.Textures[AD_GLTF_TEXTURE_METALLIC_ROUGHNESS] = LoadTextureBinding(baseDir, {}, VK_FORMAT_R8G8B8A8_UNORM, { 255, 255, 255, 255 }, textureCache);
                }

                material.NormalScale = src.normal_texture.scale == 0.f ? 1.f : src.normal_texture.scale;
                material.OcclusionStrength = src.occlusion_texture.scale == 0.f ? 1.f : src.occlusion_texture.scale;
                material.Textures[AD_GLTF_TEXTURE_NORMAL] = LoadTextureBinding(baseDir, src.normal_texture, VK_FORMAT_R8G8B8A8_UNORM, { 128, 128, 255, 255 }, textureCache);
                material.Textures[AD_GLTF_TEXTURE_OCCLUSION] = LoadTextureBinding(baseDir, src.occlusion_texture, VK_FORMAT_R8G8B8A8_UNORM, { 255, 255, 255, 255 }, textureCache);
                material.bNormalMapEnabled = material.Textures[AD_GLTF_TEXTURE_NORMAL].bEnabled;
                mMaterials.push_back(material);
            }
        }

        auto loadNode = [&](auto &&self, const cgltf_node *node) -> void {
            if(!node){
                return;
            }

            cgltf_float nodeMatrix[16]{};
            cgltf_node_transform_world(node, nodeMatrix);
            glm::mat4 nodeTransform = ToMat4(nodeMatrix);

            if(node->mesh){
                for(cgltf_size primitiveIndex = 0; primitiveIndex < node->mesh->primitives_count; primitiveIndex++){
                    const cgltf_primitive &srcPrimitive = node->mesh->primitives[primitiveIndex];
                    if(srcPrimitive.type != cgltf_primitive_type_triangles){
                        LOG_W("Skipping non-triangle glTF primitive in mesh {0}", node->mesh->name ? node->mesh->name : "<unnamed>");
                        continue;
                    }

                    const cgltf_accessor *positionAccessor = FindAccessor(srcPrimitive, cgltf_attribute_type_position);
                    const cgltf_accessor *normalAccessor = FindAccessor(srcPrimitive, cgltf_attribute_type_normal);
                    const cgltf_accessor *uvAccessor = FindAccessor(srcPrimitive, cgltf_attribute_type_texcoord);
                    const cgltf_accessor *tangentAccessor = FindAccessor(srcPrimitive, cgltf_attribute_type_tangent);
                    if(!positionAccessor || !normalAccessor || !uvAccessor){
                        LOG_W("Skipping glTF primitive without POSITION/NORMAL/TEXCOORD_0.");
                        continue;
                    }

                    std::vector<AdVertex> vertices(positionAccessor->count);
                    glm::vec3 primitiveMin(std::numeric_limits<float>::max());
                    glm::vec3 primitiveMax(std::numeric_limits<float>::lowest());
                    for(cgltf_size vertexIndex = 0; vertexIndex < positionAccessor->count; vertexIndex++){
                        vertices[vertexIndex].position = ReadVec3(positionAccessor, vertexIndex, glm::vec3(0.f));
                        vertices[vertexIndex].normal = glm::normalize(ReadVec3(normalAccessor, vertexIndex, glm::vec3(0.f, 1.f, 0.f)));
                        vertices[vertexIndex].texcoord0 = ReadVec2(uvAccessor, vertexIndex, glm::vec2(0.f));
                        vertices[vertexIndex].tangent = ReadVec4(tangentAccessor, vertexIndex, glm::vec4(1.f, 0.f, 0.f, 1.f));

                        glm::vec3 transformed = glm::vec3(nodeTransform * glm::vec4(vertices[vertexIndex].position, 1.f));
                        ExpandBounds(primitiveMin, primitiveMax, transformed);
                        ExpandBounds(mBoundsMin, mBoundsMax, transformed);
                    }

                    std::vector<uint32_t> indices;
                    if(srcPrimitive.indices){
                        indices.resize(srcPrimitive.indices->count);
                        for(cgltf_size index = 0; index < srcPrimitive.indices->count; index++){
                            indices[index] = static_cast<uint32_t>(cgltf_accessor_read_index(srcPrimitive.indices, index));
                        }
                    } else {
                        indices.resize(positionAccessor->count);
                        for(cgltf_size index = 0; index < positionAccessor->count; index++){
                            indices[index] = static_cast<uint32_t>(index);
                        }
                    }

                    uint32_t materialIndex = 0;
                    bool needsGeneratedTangents = tangentAccessor == nullptr;
                    if(srcPrimitive.material){
                        materialIndex = static_cast<uint32_t>(cgltf_material_index(data.get(), srcPrimitive.material));
                        if(materialIndex >= mMaterials.size()){
                            materialIndex = 0;
                        }
                    }
                    needsGeneratedTangents = needsGeneratedTangents && mMaterials[materialIndex].bNormalMapEnabled;
                    if(needsGeneratedTangents && !GenerateTangents(vertices, indices)){
                        LOG_W("Failed to generate MikkTSpace tangents. Normal map disabled for material {0}", mMaterials[materialIndex].Name);
                        mMaterials[materialIndex].bNormalMapEnabled = false;
                    }

                    AdGLTFPrimitive primitive{};
                    primitive.Mesh = std::make_shared<AdMesh>(vertices, indices);
                    primitive.MaterialIndex = materialIndex;
                    primitive.NodeTransform = nodeTransform;
                    primitive.BoundsMin = primitiveMin;
                    primitive.BoundsMax = primitiveMax;
                    primitive.VertexCount = static_cast<uint32_t>(vertices.size());
                    primitive.IndexCount = static_cast<uint32_t>(indices.size());
                    mPrimitives.push_back(primitive);
                }
            }

            for(cgltf_size child = 0; child < node->children_count; child++){
                self(self, node->children[child]);
            }
        };

        const cgltf_scene *scene = data->scene ? data->scene : (data->scenes_count > 0 ? &data->scenes[0] : nullptr);
        if(scene){
            for(cgltf_size i = 0; i < scene->nodes_count; i++){
                loadNode(loadNode, scene->nodes[i]);
            }
        }

        mLoaded = !mPrimitives.empty();
        if(!mLoaded){
            mBoundsMin = glm::vec3(0.f);
            mBoundsMax = glm::vec3(0.f);
            LOG_E("glTF model loaded no renderable primitives: {0}", filePath);
        } else {
            LOG_I("Loaded glTF model {0}: {1} primitives, {2} materials", filePath, mPrimitives.size(), mMaterials.size());
        }
        return mLoaded;
    }
}
