# Original Project Blog References

Recorded on: 2026-05-28

This project is based on AdiosyEngine / Adiosy's Vulkan learning renderer. These links are the main upstream blog references for understanding the original project structure, Vulkan object model, render framework design, and material system design.

## Blog Links

1. [Learn_Vulkan00_第一个三角形](https://www.adiosy.com/posts/learn_vulkan/learn_vulkan00_%E7%AC%AC%E4%B8%80%E4%B8%AA%E4%B8%89%E8%A7%92%E5%BD%A2.html)
   - Date: 2023-06-13
   - Focus: First Vulkan triangle; basic Vulkan objects, render pass, framebuffer, pipeline, command buffer, semaphore, and frame loop.

2. [Learn_Vulkan01_重要对象浅析](https://www.adiosy.com/posts/learn_vulkan/learn_vulkan01_%E9%87%8D%E8%A6%81%E5%AF%B9%E8%B1%A1%E6%B5%85%E6%9E%90.html)
   - Date: 2023-06-15
   - Focus: Vulkan layers/extensions, instance, surface, physical/logical device, queue families, queues, and swapchain.

3. [Learn_Vulkan02_渲染框架实现_开篇](https://www.adiosy.com/posts/learn_vulkan/learn_vulkan02_%E6%B8%B2%E6%9F%93%E6%A1%86%E6%9E%B6%E5%AE%9E%E7%8E%B0_%E5%BC%80%E7%AF%87.html)
   - Date: 2024-04-07
   - Focus: AdiosyEngine project motivation, feature roadmap, initial project layering, and directory structure.
   - Upstream source mentioned by author: <https://gitee.com/xingchen0085/adiosy_engine>

4. [Learn_Vulkan04_渲染框架实现_材质系统设计](https://www.adiosy.com/posts/learn_vulkan/learn_vulkan04_%E6%B8%B2%E6%9F%93%E6%A1%86%E6%9E%B6%E5%AE%9E%E7%8E%B0_%E6%9D%90%E8%B4%A8%E7%B3%BB%E7%BB%9F%E8%AE%BE%E8%AE%A1.html)
   - Date: 2024-07-18
   - Focus: Original render loop model, RenderPass / RenderTarget / MaterialSystem relationship, material parameters, material components, and descriptor/material separation rules.
   - Upstream source mentioned by author: <https://gitee.com/xingchen0085/adiosy_engine>

## Refactor Notes

- The original architecture is organized around Platform / Core / Application layers.
- The original rendering flow emphasizes RenderPass -> RenderTarget -> MaterialSystem -> Material -> Mesh.
- Current local modifications extend the original project toward a modern Vulkan deferred renderer: frames-in-flight hygiene, minimal RenderGraph, Deferred GBuffer, Cook-Torrance PBR, shadow map + PCF, split-sum IBL, and glTF PBR static asset rendering.
- When documenting or presenting this project, distinguish upstream framework work from the later renderer pipeline extensions.

## Attribution Reminder

The referenced blog pages identify the author as xingchen / Adiosy and include licensing information on the pages. Keep attribution clear when reusing design notes, diagrams, or written explanations from the original blog.
