#ifndef ADAPPLICATIONCONTEXT_H
#define ADAPPLICATIONCONTEXT_H

#include "ECS/AdScene.h"

namespace ade{
    class AdApplication;
    class AdRenderContext;
    class AdRenderer;

    struct AdAppContext{
        AdApplication *app = nullptr;
        AdScene *scene = nullptr;
        AdRenderContext *renderCxt = nullptr;
        AdRenderer *renderer = nullptr;
    };
}

#endif
