# Vulkan PBR Renderer

#### 介绍

一个基于 Vulkan 的现代延迟渲染器原型。

当前项目主线是从基础 Vulkan 框架扩展到完整可验证的 Deferred PBR 渲染链路，包含 frames-in-flight、Render Graph、Deferred GBuffer、Cook-Torrance PBR、Directional Shadow Map + PCF、split-sum IBL 预计算和 glTF PBR 静态资产渲染。

- 主示例：`Sample/11_GLTFPBR`
- 展示资产：`DamagedHelmet.gltf`
- 验证方式：RenderDoc capture、GPU timestamp、debug label、debug view

#### 功能列表

- Vulkan frame/resource hygiene：区分 FrameSlot / ImageIndex，使用 fence / semaphore 管理 acquire、submit、present 和 frame slot 复用。
- deferred destruction：避免主渲染路径每帧 `vkDeviceWaitIdle` / `vkQueueWaitIdle`。
- Minimal RenderGraph：支持 transient texture、imported swapchain image 和基础 image layout transition。
- Deferred GBuffer：输出 BaseColor、Normal、Material、WorldPosition、Depth。
- Deferred PBR：实现 Cook-Torrance directional light 和 HDR LightingColor。
- Shadow Map + PCF：实现 Directional ShadowMap.Depth、3x3 PCF 和 ShadowFactor debug view。
- Real IBL：实现 HDR equirectangular -> EnvironmentCube、IrradianceCube、PrefilteredSpecularCube、BRDF LUT。
- glTF PBR：使用 cgltf 解析 glTF，MikkTSpace 生成 tangent，支持 baseColor / metallicRoughness / normal / AO 贴图。
- debug view：支持 Lit + Sky、EnvironmentCube、IrradianceCube、PrefilteredCube、BRDF LUT、ShadowFactor、BaseColor、Normal、Roughness、Metallic。

##### TODO list:

- 抽离 `AdDeferredRenderer` 或 `AdGLTFPBRRenderer`，让 `Sample/11_GLTFPBR` 只负责场景配置和交互。
- 新增 `AdGBufferResources`、`AdShadowResources`、`AdLightingResources` 等资源聚合结构。
- 将 Shadow / GBuffer / RealIBLDeferredLighting / DebugComposite 注册逻辑拆成独立 pass 类。
- 增加轻量 DrawCollector，逐步把 ECS 遍历和 draw submission 解耦。
- 改进 RenderGraph 的 transition 描述和调试输出，减少 pass 内手动 layout 管理。
- 加入 tone mapping pass，并把 HDR LightingColor 到 swapchain 的输出路径显式化。
- 为 glTF 材质补充 emissive、alpha mode、double-sided 等常见兼容项。

#### 软件架构

- Platform：封装窗口、事件、Vulkan 图形上下文和第三方库接入。
- Core：包含 Renderer、RenderGraph、RenderTarget、MaterialSystem、ECS、Mesh、Texture、glTF、IBL 等核心模块。
- Resource：保存 shader、HDR environment map、texture 和 glTF 资产。
- Sample：保存阶段性渲染样例，当前主样例为 `11_GLTFPBR`。
- reference：保存原始项目博客和重构参考资料，不参与运行。

#### 运行示例

`11_GLTFPBR` 运行时支持以下操作：

- 拖拽鼠标：环绕相机
- 鼠标滚轮：缩放
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
- `R`: 切换连续 IBL precompute，便于 RenderDoc 抓帧
- `M`: 切换 DamagedHelmet 自动旋转
- `N`: 切换 glTF normal map
- `O`: 切换 glTF ambient occlusion

#### 安装教程

依赖：

- CMake 3.22+
- C++17 编译器
- Vulkan SDK
- `glslangValidator`

Windows PowerShell：

```powershell
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug --target 11_GLTFPBR --config Debug
```

运行：

```powershell
.\cmake-build-debug\bin\11_GLTFPBR.exe
```

如果使用 Visual Studio 等 multi-config generator，可执行文件可能位于：

```powershell
.\cmake-build-debug\bin\Debug\11_GLTFPBR.exe
```

#### 关键文件

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

#### 项目边界

- 当前是 Vulkan 延迟渲染器原型，不是完整商业引擎。
- 暂无 Render Thread / RHI Thread。
- 暂无 UE 风格 RenderScene / PrimitiveProxy / MeshBatch / FMeshDrawCommand cache。
- 暂无 GPU culling / indirect draw / bindless / descriptor indexing。
- MaterialSystem 目前仍直接遍历 ECS 并提交 draw。
- RenderGraph 是最小 append-order graph，还不是完整 RDG 编译器。
