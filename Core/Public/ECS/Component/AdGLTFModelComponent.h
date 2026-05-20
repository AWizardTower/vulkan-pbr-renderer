#ifndef AD_GLTF_MODEL_COMPONENT_H
#define AD_GLTF_MODEL_COMPONENT_H

#include "ECS/AdComponent.h"

namespace ade{
    class AdGLTFModel;

    class AdGLTFModelComponent : public AdComponent{
    public:
        explicit AdGLTFModelComponent(AdGLTFModel *model = nullptr) : Model(model) {}

        AdGLTFModel *Model = nullptr;
    };
}

#endif
