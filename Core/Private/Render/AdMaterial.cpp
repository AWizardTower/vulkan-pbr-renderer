#include "Render/AdMaterial.h"

namespace ade{
    AdMaterialFactory AdMaterialFactory::s_MaterialFactory{};

    bool AdMaterial::HasTexture(uint32_t id) const {
        if(mTextures.find(id) != mTextures.end()){
            return true;
        }
        return false;
    }

    const TextureView *AdMaterial::GetTextureView(uint32_t id) const {
        if(HasTexture(id)){
            return &mTextures.at(id);
        }
        return nullptr;
    }

    void AdMaterial::MarkParamsDirty() {
        bShouldFlushParams = true;
        mParamsVersion++;
    }

    void AdMaterial::MarkResourceDirty() {
        bShouldFlushResource = true;
        mResourceVersion++;
    }

    void AdMaterial::SetTextureView(uint32_t id, AdTexture *texture, AdSampler *sampler) {
        if(HasTexture(id)){
            mTextures[id].texture = texture;
            mTextures[id].sampler = sampler;
        } else {
            mTextures[id] = { texture, sampler };
        }
        MarkResourceDirty();
    }

    void AdMaterial::UpdateTextureViewEnable(uint32_t id, bool enable) {
        if(HasTexture(id)){
            mTextures[id].bEnable = enable;
            MarkParamsDirty();
        }
    }

    void AdMaterial::UpdateTextureViewUVTranslation(uint32_t id, const glm::vec2 &uvTranslation) {
        if(HasTexture(id)){
            mTextures[id].uvTranslation = uvTranslation;
            MarkParamsDirty();
        }
    }

    void AdMaterial::UpdateTextureViewUVRotation(uint32_t id, float uvRotation) {
        if(HasTexture(id)){
            mTextures[id].uvRotation = uvRotation;
            MarkParamsDirty();
        }
    }

    void AdMaterial::UpdateTextureViewUVScale(uint32_t id, const glm::vec2 &uvScale) {
        if(HasTexture(id)){
            mTextures[id].uvScale = uvScale;
            MarkParamsDirty();
        }
    }
}
