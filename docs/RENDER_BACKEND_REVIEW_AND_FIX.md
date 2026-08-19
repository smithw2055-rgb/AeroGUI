# Render 后端审查与修复实施文档

本文记录以 `C:\Projects\AeroGUI`（参考实现，NoesisGUI 派生）为基准，
对 AeroGUI-R 渲染后端能力缺失与错误进行的审查结论与修复实施过程。

## 审查结论摘要

参考实现（`BridgeRenderer` + `NsRender` + D3D11/GL 后端）支持：54 种 Shader
（Linear/Radial 渐变、SDF_LCD、Opacity、Blur、Shadow、Mask、Upsample/Downsample）、
完整 BlendMode（Src/SrcOver/Multiply/Screen/Additive/Dual）、渐变 ramp 纹理、
mask、offscreen 合成、脏区部分重绘（`CopyRenderTargetRegion`）、MSAA resolve、
设备垃圾回收。

AeroGUI-R 当前缺口与错误（详见各阶段）：

- **A. 阻断性编译错误**：`src/gui/controls/Trees.cpp` 在 `-fno-rtti` 下使用
  `dynamic_cast`（3 处），Linux/Windows 均无法编译。
- **B. 渲染正确性错误**：
  1. 圆角矩形 `FillRoundedRect` 使用 `Path_AA_Solid`，但两个后端都未实现 AA，
     圆角渲染成硬直角（corner radius 从未传给后端）。
  2. `StrokeRect` 无圆角、四边 quad 角部重叠（半透明描边出现接缝）。
  3. 渐变是 CPU Gouraud 近似，带 stop 偏移 / Repeat / Reflect spread 的渐变
     不准确（参考实现用 ramp 纹理 + shader）。
  4. ImageBrush 平铺（TileMode Repeat/Mirror）不支持。
  5. 混合模式只实现 Src/SrcOver，Additive 退化为 SrcOver，
     Multiply/Screen/Dual 缺失。
  6. Effect（Blur/DropShadow/Pixelate）完全未接入渲染管线
     （`RenderTree::BuildSubtree` 从不填充 `mask`/`effect`/`gradientRamps`）。
  7. Opacity 是逐顶点折入而非组语义（参考用 offscreen 合成）。
  8. Mesh 16-bit 索引重基在 >65535 顶点时静默截断。
  9. 死代码/桩：`RecordOffscreen` 实际为空、`GetOrCreateOffscreenTarget`
     无调用者、`CollectDeviceGarbage` 与 `TextRenderer::CollectGarbage`
     恒返回 0、`ResolveRenderTarget`(MSAA) 与 `CopyRenderTargetRegion` 是 no-op。
  10. conformance 像素门禁（`tools/conformance/*`）使用过时 `Aero::Graphics::*`
      API，与当前 `Aero::Render::*` 不匹配，无法编译。
  11. `D3D11RenderDevice` 的 `vertexShaders_/pixelShaders_/inputLayouts_` 数组
      声明了但从未填充。
- **C. 前序工作区改动**（本任务开工前已存在，评估为基本正确）：预乘 alpha 颜色、
  D3D11 纹理 IMMUTABLE/R8 bpp、stencil 深度缓冲、网格/字形 run 注册、
  `FillGradientQuad`、Path S/T/Q/T 贝塞尔与描边修复、渐变 flattening。

## 环境限制

- 本机为 WSL/Linux：**无法编译/验证 D3D11 后端与 Windows 像素门禁**。
- 验证策略：Linux 构建核心库 + `AeroRenderOpenGL33` + `AeroFrameworkConformanceTests`
  并跑绿；D3D11 改动仅静态审查；Windows 像素门禁由用户在 `out/build` 验证。

## 修复阶段

### P1 — 恢复可编译（阻塞项）

| # | 改动 | 文件 |
|---|---|---|
| 1 | 3 处 `dynamic_cast<IItemsSource*>` → 按 `RuntimeType()` 匹配
      `ObservableCollection` / `GradientStopCollection` 后 `static_cast`
      （照 `metadata/Support.inl` 既有模式） | `src/gui/controls/Trees.cpp` |
| 2 | 全量构建 `AeroGui` + `AeroRenderOpenGL33` +
      `AeroFrameworkConformanceTests` 并跑绿 | — |

### P2 — 渲染正确性错误

| # | 改动 |
|---|---|
| 1 | 圆角矩形：编码器侧周长细分三角扇（corner radius 经 `cmd.scalar` 传入），
      `Path_AA_Solid` 与 `Path_Solid` 等同映射；修复 `StrokeRect` 角部重叠。 |
| 2 | Border 圆角描边：`StrokeRect` 支持 corner radius。 |
| 3 | Mesh 16-bit 索引溢出防护：超限时先 flush 并按 64k 分块。 |
| 4 | 移除 D3D11 死状态表数组或改由真实 shader 填充。 |

### P3 — 核心能力缺口

| # | 能力 | 实现 |
|---|---|---|
| 1 | 渐变精度 | 线性渐变按 stop 位置分段 Gouraud 带；径向扇形支持
      Pad/Repeat/Reflect spread 与偏心 focal；保留 RelativeToBoundingBox。 |
| 2 | 混合模式 | `node.blendMode` 接入编码器；D3D11 补
      Multiply/Screen/Additive/Dual，GL 补 `glBlendFuncSeparate`。 |
| 3 | ImageBrush 平铺 | `DrawImage` 增加 wrap mode，UV 按 tile 数学映射。 |
| 4 | 组 Opacity | 节点 opacity<1 且子树复杂时走 offscreen 合成，
      否则保留逐顶点快速路径。 |

### P4 — Effect/Mask（`BackgroundBlur` 示例依赖）

| # | 实现 |
|---|---|
| 1 | `RenderTree::BuildSubtree` 填充 `snapshot.effect`（`GetEffect()`）、
      `snapshot.mask`（`GetOpacityMask()`）、`gradientRamps_`，并在
      `ValidateRenderFrame` 校验 effect。 |
| 2 | 编码器 offscreen 子树重定向 + 合成：effect/mask/组 opacity 节点
      录制到逐节点 offscreen `RenderTarget`（含 mask 第二 offscreen），
      再用 Blur/Shadow/Mask shader 合成回主目标；GL 新增 3 个 pixel shader
      （9-tap 模糊、alpha 阴影、双纹理 mask），D3D11 新增对应 HLSL。
      DropShadow 先画偏移模糊阴影再画源；Solid mask 无纹理时并入 tint alpha。 |
| 3 | Mask：非 Solid mask 渲染到独立 offscreen 后经 `Shader::Mask` 双采样合成。 |
| 4 | `RecordOffscreen` 保持 ramp 纹理与 offscreen surface 生命周期职责。 |

已知限制：offscreen 内嵌套的 effect/mask 被扁平化（不递归合成）；offscreen
目标无 stencil，子元素 clip 在其内近似失效；渐变 mask 按四角 Gouraud 近似。

### P5 — 门禁恢复与清理

| # | 实现 |
|---|---|
| 1 | 4 个 conformance 工具迁移到当前 `Aero::Render::*` API，保证编译。 |
| 2 | `CollectDeviceGarbage` 与 `TextRenderer::CollectGarbage`（glyph-run/atlas LRU）实现。 |
| 3 | MSAA `ResolveRenderTarget`、`CopyRenderTargetRegion` 按需补齐。 |

## 里程碑

M1(P1) → M2(P2) → M3(P3) → M4(P4) → M5(P5)。每步保持可编译，
P2/P3 优先保证 GL/D3D11 双后端行为一致。

## 验证记录

- P1：`ninja AeroGui` 绿；`ctest -R AeroFrameworkConformanceTests` 100%。
- P2.1：`ninja AeroGui` 绿；框架测试绿（FillRoundedRect 三角扇 / StrokeRect 圆角）。
- P2.2：`ninja AeroGui` 绿（`cornerRadius` 字段 + 哈希 + 两处调用）。
- P2.3：`ninja AeroGui` 绿（DrawMesh 65535 分块）。
- P2.4：`ninja AeroGui` 绿（移除 D3D11 死 shader 数组）。
- P3.1：`ninja AeroGui` 绿（线性带 / 径向环）。
- P3.2：`ninja AeroGui` 绿（blend 接入；GL 常量 0x0306/0x0301 补全）。
- P3.3：`ninja AeroGui` 绿（ImageBrush Flip 镜像）。
- P4.1：`ninja AeroGui` 绿（effect/mask/ramp 快照填充 + 校验）。
- P4.2：`ninja AeroGui AeroRenderOpenGL33` 绿；`ctest -R AeroFrameworkConformanceTests` 绿。
  GL Blur/Shadow/Mask shader、offscreen 重定向与合成已接入；D3D11 HLSL
  （`RenderFrameBlur/Shadow/Mask.hlsl`）与状态绑定为静态审查。
- 全量 `ninja` + `ctest`：仅预存的 `AeroArchitectureChecks` 失败（跳过）。
- 仍存警告：FrameEncoder.cpp 的 `uint8_t → bitfield` 转换（既有 in-flight 代码，未引入）。