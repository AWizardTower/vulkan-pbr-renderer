#ifndef AD_GLTF_MODEL_H
#define AD_GLTF_MODEL_H

#include "AdGraphicContext.h"
#include "Render/AdMesh.h"
#include "Render/AdSampler.h"
#include "Render/AdTexture.h"

#include <array>

namespace ade{
    enum AdGLTFTextureSlot : uint32_t{
        AD_GLTF_TEXTURE_BASE_COLOR = 0,
        AD_GLTF_TEXTURE_METALLIC_ROUGHNESS = 1,
        AD_GLTF_TEXTURE_NORMAL = 2,
        AD_GLTF_TEXTURE_OCCLUSION = 3,
        AD_GLTF_TEXTURE_COUNT = 4
    };

    struct AdGLTFTextureBinding{
        std::shared_ptr<AdTexture> Texture;
        std::shared_ptr<AdSampler> Sampler;
        bool bEnabled = false;

        bool IsValid() const {
            return bEnabled && Texture && Sampler && Texture->GetImageView();
        }
    };

    struct AdGLTFPBRMaterial{
        std::string Name;
        glm::vec4 BaseColorFactor{ 1.f, 1.f, 1.f, 1.f };
        float MetallicFactor = 1.f;
        float RoughnessFactor = 1.f;
        float NormalScale = 1.f;
        float OcclusionStrength = 1.f;
        bool bNormalMapEnabled = false;
        std::array<AdGLTFTextureBinding, AD_GLTF_TEXTURE_COUNT> Textures;
    };

    struct AdGLTFPrimitive{
        std::shared_ptr<AdMesh> Mesh;
        uint32_t MaterialIndex = 0;
        glm::mat4 NodeTransform{ 1.f };
        glm::vec3 BoundsMin{ 0.f };
        glm::vec3 BoundsMax{ 0.f };
        uint32_t VertexCount = 0;
        uint32_t IndexCount = 0;
    };

    class AdGLTFModel{
    public:
        bool LoadFromFile(const std::string &filePath);

        const std::vector<AdGLTFPrimitive> &GetPrimitives() const { return mPrimitives; }
        const std::vector<AdGLTFPBRMaterial> &GetMaterials() const { return mMaterials; }
        const glm::vec3 &GetBoundsMin() const { return mBoundsMin; }
        const glm::vec3 &GetBoundsMax() const { return mBoundsMax; }
        glm::vec3 GetBoundsCenter() const { return (mBoundsMin + mBoundsMax) * 0.5f; }
        glm::vec3 GetBoundsExtent() const { return mBoundsMax - mBoundsMin; }
        bool IsLoaded() const { return mLoaded; }
    private:
        void Reset();

        std::vector<AdGLTFPrimitive> mPrimitives;
        std::vector<AdGLTFPBRMaterial> mMaterials;
        glm::vec3 mBoundsMin{ 0.f };
        glm::vec3 mBoundsMax{ 0.f };
        bool mLoaded = false;
    };
}

#endif
