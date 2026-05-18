cmake_minimum_required(VERSION 3.22)

add_library(adiosy_core
        Private/AdApplication.cpp
        Private/Render/AdRenderContext.cpp
        Private/Render/AdRenderTarget.cpp
        Private/Render/AdRenderer.cpp
        Private/Render/AdRenderGraph.cpp
        Private/Render/AdFullscreenTexturePass.cpp
        Private/Render/AdGBufferDebugCompositePass.cpp
        Private/Render/AdDeferredPBRLightingPass.cpp
        Private/Render/AdPBRDebugCompositePass.cpp
        Private/Render/AdShadowedDeferredPBRLightingPass.cpp
        Private/Render/AdShadowDebugCompositePass.cpp
        Private/Render/AdIBLDeferredLightingPass.cpp
        Private/Render/AdIBLDebugCompositePass.cpp
        Private/Render/AdIBLResources.cpp
        Private/Render/AdIBLPrecomputePass.cpp
        Private/Render/AdRealIBLDeferredLightingPass.cpp
        Private/Render/AdRealIBLDebugCompositePass.cpp
        Private/Render/AdMesh.cpp
        Private/Render/AdTexture.cpp
        Private/Render/AdHDRTexture.cpp
        Private/Render/AdSampler.cpp
        Private/Render/AdMaterial.cpp

        Private/ECS/AdUUID.cpp
        Private/ECS/AdNode.cpp
        Private/ECS/AdScene.cpp
        Private/ECS/AdEntity.cpp
        Private/ECS/Component/AdLookAtCameraComponent.cpp
        Private/ECS/System/AdMaterialSystem.cpp
        Private/ECS/System/AdBaseMaterialSystem.cpp
        Private/ECS/System/AdGBufferMaterialSystem.cpp
        Private/ECS/System/AdPBRGBufferMaterialSystem.cpp
        Private/ECS/System/AdShadowDepthMaterialSystem.cpp
        Private/ECS/System/AdUnlitMaterialSystem.cpp
)
target_link_libraries(adiosy_core PUBLIC adiosy_platform)
