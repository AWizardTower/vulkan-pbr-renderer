# Vulkan PBR Renderer

#### Description

A modern deferred rendering prototype built on Vulkan.

The current project extends a basic Vulkan framework into a verifiable Deferred PBR rendering pipeline, including frames-in-flight resource hygiene, a Render Graph, Deferred GBuffer, Cook-Torrance PBR, directional shadow mapping with PCF, split-sum IBL precomputation, and static glTF PBR asset rendering.

- Main sample: `Sample/11_GLTFPBR`
- Demo asset: `DamagedHelmet.gltf`
- Validation: RenderDoc capture, GPU timestamps, debug labels, and debug views

#### Feature list

- Vulkan frame/resource hygiene: separates FrameSlot from ImageIndex and uses fences / semaphores for acquire, submit, present, and frame-slot reuse.
- Deferred destruction: avoids per-frame `vkDeviceWaitIdle` / `vkQueueWaitIdle` on the main rendering path.
- Minimal RenderGraph: supports transient textures, imported swapchain images, and basic image layout transitions.
- Deferred GBuffer: outputs BaseColor, Normal, Material, WorldPosition, and Depth.
- Deferred PBR: implements Cook-Torrance directional lighting and HDR LightingColor.
- Shadow Map + PCF: implements Directional ShadowMap.Depth, 3x3 PCF, and a ShadowFactor debug view.
- Real IBL: implements HDR equirectangular -> EnvironmentCube, IrradianceCube, PrefilteredSpecularCube, and BRDF LUT.
- glTF PBR: uses cgltf for glTF parsing and MikkTSpace for tangent generation; supports baseColor / metallicRoughness / normal / AO textures.
- Debug views: Lit + Sky, EnvironmentCube, IrradianceCube, PrefilteredCube, BRDF LUT, ShadowFactor, BaseColor, Normal, Roughness, and Metallic.

##### TODO list:

- Extract `AdDeferredRenderer` or `AdGLTFPBRRenderer` so `Sample/11_GLTFPBR` only handles scene setup and interaction.
- Add resource grouping structs such as `AdGBufferResources`, `AdShadowResources`, and `AdLightingResources`.
- Split Shadow / GBuffer / RealIBLDeferredLighting / DebugComposite registration into dedicated pass classes.
- Add a lightweight DrawCollector to gradually separate ECS traversal from draw submission.
- Improve RenderGraph transition descriptions and debug output to reduce manual layout management inside passes.
- Add a tone-mapping pass and make the HDR LightingColor -> swapchain output path explicit.
- Extend glTF material support with common features such as emissive, alpha mode, and double-sided rendering.

#### Software Architecture

- Platform: wraps windowing, events, Vulkan graphics context setup, and third-party integration.
- Core: contains Renderer, RenderGraph, RenderTarget, MaterialSystem, ECS, Mesh, Texture, glTF, IBL, and related modules.
- Resource: stores shaders, HDR environment maps, textures, and glTF assets.
- Sample: stores staged rendering samples; the current main sample is `11_GLTFPBR`.
- reference: stores historical project notes and refactor references; it is not used at runtime.

#### Sample

`11_GLTFPBR` supports:

- Drag mouse: orbit camera
- Mouse wheel: zoom
- `1`: Lit + Sky
- `2`: EnvironmentCube
- `3`: IrradianceCube
- `4`: PrefilteredCube
- `5`: BRDF LUT
- `6`: ShadowFactor
- `7`: BaseColor
- `8`: Normal
- `9`: Roughness
- `0`: Metallic
- `[` / `]`: Prefilter debug mip
- `R`: Toggle continuous IBL precompute for RenderDoc capture
- `M`: Toggle DamagedHelmet auto-rotation
- `N`: Toggle glTF normal map
- `O`: Toggle glTF ambient occlusion

#### Installation

Requirements:

- CMake 3.22+
- A C++17 compiler
- Vulkan SDK
- `glslangValidator`

Windows PowerShell:

```powershell
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug --target 11_GLTFPBR --config Debug
```

Run:

```powershell
.\cmake-build-debug\bin\11_GLTFPBR.exe
```

With Visual Studio or another multi-config generator, the executable may be under:

```powershell
.\cmake-build-debug\bin\Debug\11_GLTFPBR.exe
```

#### Key files

- `Core/Public/Render/AdRenderer.h`
- `Core/Private/Render/AdRenderer.cpp`
- `Core/Public/Render/AdRenderGraph.h`
- `Core/Private/Render/AdRenderGraph.cpp`
- `Core/Public/Render/AdRenderTarget.h`
- `Core/Private/Render/AdRenderTarget.cpp`
- `Core/Public/Render/AdGLTFModel.h`
- `Core/Private/Render/AdGLTFModel.cpp`
- `Core/Private/ECS/System/AdGLTFPBRGBufferMaterialSystem.cpp`
- `Core/Private/Render/AdIBLPrecomputePass.cpp`
- `Core/Private/Render/AdRealIBLDeferredLightingPass.cpp`
- `Resource/Shader/10_gltf_pbr_gbuffer.vert`
- `Resource/Shader/10_gltf_pbr_gbuffer.frag`
- `Resource/Shader/09_real_ibl_lighting.frag`
- `Sample/11_GLTFPBR/Main.cpp`

#### Scope

- This is a Vulkan deferred-renderer prototype, not a full commercial engine.
- No Render Thread / RHI Thread yet.
- No UE-style RenderScene / PrimitiveProxy / MeshBatch / FMeshDrawCommand cache yet.
- No GPU culling / indirect draw / bindless / descriptor indexing yet.
- MaterialSystem currently iterates ECS views directly and submits draws.
- RenderGraph is a minimal append-order graph, not a full RDG compiler.
