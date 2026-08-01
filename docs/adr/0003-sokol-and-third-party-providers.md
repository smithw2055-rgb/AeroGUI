# ADR-0003：sokol 与可选第三方 Provider

- **状态**：Accepted
- **日期**：2026-07-21
- **决策者**：AeroGUI maintainers

## 背景

AeroGUI 需要字体、文本 shaping、XML tokenization、几何 tessellation、浮点格式化和快速跨平台 bring-up 能力。成熟第三方库可以显著降低实现和安全风险，但不能决定公共 API、锁死渲染架构或妨碍游戏主机与宿主引擎集成。

## 决策

1. 所有第三方依赖必须是 optional、replaceable、private、pinned、auditable。
2. 任何第三方 type、allocator、error、handle 或 object lifetime 不得泄漏到公共 AeroGUI API。
3. 通用开发 profile 推荐：

   ```text
   AERO_WITH_FREETYPE=ON
   AERO_WITH_HARFBUZZ=ON
   AERO_WITH_EXPAT=ON
   AERO_WITH_LIBTESS2=OFF
   AERO_WITH_RYU=ON
   AERO_WITH_SOKOL=OFF
   ```

4. `FreeType` 可作为默认字体访问、outline、hinting 与 glyph raster provider；允许平台或宿主 provider 替换。
5. `HarfBuzz` 可作为默认 complex-text shaping provider；它不替代 bidi、line breaking 或 paragraph layout。
6. `Expat` 可作为 Runtime XAML 的 streaming XML tokenizer；XAML schema、object writer 与 WPF 语义仍完全属于 AeroGUI。compiled-XAML-only 或 host-parser profile 可关闭 Expat。
7. `libtess2` 仅作为 experimental CPU tessellation fallback，默认关闭；必须经过 adapter、allocator integration、fuzz 和许可证审查，并保留替代计划。
8. `Ryu` 可作为确定性 float-to-string provider，用于 XAML、diagnostics、snapshot 和序列化；允许经过一致性测试的替代实现。
9. `sokol_gfx` 仅可封装为默认关闭的 `AeroGraphics_Sokol` adapter，用于 bring-up、sample、tool、WASM experiment 和额外 backend contract 验证。
10. `sokol_gfx` 不得定义 `AeroGraphics`，不得成为 D3D12/Vulkan/Metal 或 console backend 的共同底层，不得被当作正式游戏主机支持。
11. `sokol_app` 只能用于独立 sample/tool，不能接管可嵌入 Runtime 的 window、input、event loop 或 Present。
12. 每个 release 的 capability manifest 必须记录实际 provider、版本、构建开关和能力差异。
13. CI 必须包含 all-optional-dependencies-off 配置，证明核心边界可替换。

## Provider 边界

```text
IXmlTokenizer
  -> Expat adapter | Host adapter

IFontDatabase / IFontFace / IGlyphRasterizer
  -> FreeType adapter | Platform/Host adapter

ITextShaper
  -> HarfBuzz adapter | Platform/Host adapter

IGeometryTessellator
  -> Aero implementation | libtess2 adapter

IFloatFormatter
  -> Ryu adapter | Verified replacement

AeroGraphics
  -> D3D12 / Vulkan / Metal / ConsolePrivate
  -> optional sokol adapter
```

## sokol 的适用边界

采用 sokol 的主要收益是：

- 单头文件/小型 C 风格集成；
- 快速创建 sample 和验证基本 GPU pipeline；
- 对其公开支持平台提供较低成本 bring-up；
- 帮助检查 RenderFrame 没有意外绑定某个 native API。

但它不作为核心的原因是：

- AeroGUI 明确需要 D3D12、Vulkan、Metal 和专有主机 backend；
- 公开 sokol_gfx backend 集合不能覆盖完整目标矩阵；
- 游戏引擎需要复用已有 command context、resource state 和 frame graph；
- 最低共同抽象可能遮蔽 UI renderer 所需的 descriptor、barrier、tile/offscreen、external texture 和同步能力。

## 依赖治理

每个依赖必须记录：

- upstream source；
- version/commit 与 checksum；
- 实际 vendored 文件和 build flags；
- license 选择、版权和 NOTICE；
- 本地 patch 及原因；
- 安全公告/CVE 更新责任人；
- system、vendored、host 三种 source mode；
- 替代 provider 与关闭行为；
- fuzz、performance 和 regression corpus。

配置阶段不得下载未锁定源码。

## 许可证方向

- FreeType：发行配置选择并履行 FreeType License 路径，除非另有明确决定；
- HarfBuzz：按实际启用文件审查其 COPYING 与子组件许可；
- Expat：保留 MIT/X Consortium 风格许可文本；
- libtess2：在采用前单独审查 SGI Free Software License B 2.0 与分发义务；
- Ryu：记录采用 Apache-2.0 或 `ryu/` 目录可用的 Boost 1.0 路径；
- sokol：保留 zlib/libpng 风格许可和版权声明。

该列表是工程决策，不替代发行前法律审查。

## 后果

### 正面

- 利用成熟字体、shaping、XML 和数值算法；
- 依赖不会污染 AeroGUI public API；
- 主机和平台可替换 provider；
- 可按 console、mobile、engine 或 compiled-XAML profile 裁剪；
- sokol 提供开发便利而不限制正式 graphics layer。

### 代价

- 需要维护 provider conformance suites；
- 多种依赖组合增加 CI 矩阵；
- dependency update、patch、NOTICE 和安全治理需要持续投入；
- all-off profile 需要最小 stub/host contracts。

## 被否决方案

- **直接把第三方 API 暴露给用户**：形成 ABI、lifetime 和替换锁定。
- **所有第三方库强制启用**：不适合主机、引擎和裁剪场景。
- **完全不使用成熟第三方库**：字体、complex text 和安全 XML 的成本与风险过高。
- **以 sokol 取代 AeroGraphics**：目标平台和集成能力不足。
- **默认启用 libtess2**：维护状态与许可证需要先完成独立验证。

## 验证

- all optional dependencies OFF；
- 推荐 generic profile；
- host provider stubs；
- Expat security/fuzz suite；
- FreeType/HarfBuzz fixed-font shaping suite；
- libtess2 geometry fuzz when enabled；
- Ryu round-trip/differential tests；
- sokol optional sample job；
- dependency version/license/NOTICE audit。
