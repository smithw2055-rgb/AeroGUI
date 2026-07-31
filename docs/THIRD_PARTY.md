# Third-Party Dependency Policy

- **状态**：Architecture Baseline
- **原则**：optional、replaceable、private、pinned、auditable

AeroGUI 是自有对象/布局/渲染 runtime。第三方库只提供边界清晰的通用能力，不决定公共 API、对象模型、XAML 语义或 `AeroRHI` 架构。

## 1. 通用规则

每个第三方依赖 MUST：

- 通过私有 adapter/provider 使用；
- 支持编译期开关或由宿主实现替代 provider；
- 不把第三方 struct、enum、pointer、allocator 或 error type 暴露到公共 AeroGUI API；
- 在 dependency manifest 中记录 source URL、version/commit、checksum、license、用途、patch 和 owner；
- 在发行包包含所需 NOTICE/license 文本；
- 禁止在 CMake configure 阶段下载未锁定源码；
- 对不可信输入路径进行 fuzz、配额和安全更新；
- 在 capability manifest 中声明启用状态和行为差异；
- 支持至少一种无该依赖的 CI build，验证替换边界真实存在。

依赖许可证信息是工程记录，不替代正式法律审查。

## 2. 推荐配置

| 依赖 | Build option | Generic profile | 作用 |
| --- | --- | --- | --- |
| FreeType | `AERO_WITH_FREETYPE` | ON | font face、outline、hinting、glyph raster |
| HarfBuzz | `AERO_WITH_HARFBUZZ` | ON | OpenType/AAT shaping |
| Expat | `AERO_WITH_EXPAT` | ON | streaming XML tokenization |
| libtess2 | `AERO_WITH_LIBTESS2` | OFF/experimental | CPU polygon tessellation fallback |
| Ryu | `AERO_WITH_RYU` | ON | deterministic float formatting |
| sokol | `AERO_WITH_SOKOL` | OFF | optional RHI/sample adapter |

“ON” 表示通用开发 profile 的推荐值，不表示硬依赖。Console、engine integration 或 compiled-XAML-only profile 可以替换或关闭对应 provider。

## 3. FreeType

### 3.1 定位

FreeType 可作为默认 `IFontFaceProvider` 与 `IGlyphRasterizer`：

- 读取 TrueType/OpenType 等字体；
- 获取 metrics、glyph outline 和 bitmap；
- 提供 hinting/rasterization；
- 与 HarfBuzz 的 `hb-ft` adapter 配合。

FreeType 不负责完整 text layout、bidi、line breaking 或 UI text formatting。

### 3.2 边界

```text
AeroText
  -> IFontDatabase
  -> IFontFace
  -> IGlyphOutlineProvider
  -> IGlyphRasterizer
       \-> FreeType adapter (optional)
       \-> Host/platform adapter
```

- `FT_Face` 等类型不得离开 adapter；
- font bytes 的 ownership 由 AeroGUI asset/host provider 明确；
- face/cache 的 thread ownership 必须定义；
- font 输入视为不可信数据，启用 size、table、glyph 和 allocation 配额；
- 需要可重复 visual tests 时固定 FreeType build 配置和测试字体。

### 3.3 License

FreeType 官方提供 FreeType License（BSD-style with credit clause）或 GPLv2 选择。AeroGUI 发行配置应采用并履行 FreeType License 路径，不选择 GPLv2 路径，除非发行方案另有明确决定。

## 4. HarfBuzz

### 4.1 定位

HarfBuzz 可作为默认 `ITextShaper`：

- OpenType/AAT glyph substitution 与 positioning；
- script/language/direction-aware shaping；
- variable font 和 color-font 相关 shaping 数据；
- 与 FreeType、CoreText、DirectWrite 或宿主 font callbacks 组合。

HarfBuzz 不是完整 bidi、line-breaking 或 paragraph layout engine。AeroGUI 必须保留独立 `IUnicodeService`、`ITextBreaker` 和 paragraph formatter 边界。

### 4.2 Capability

- `AERO_WITH_HARFBUZZ=ON`：可声明完整 shaping provider；
- OFF 且无宿主 shaper：只能声明 limited/simple-text capability；
- 任何 build 都不得声称支持复杂 script，却只按 Unicode code point 直接映射 glyph。

### 4.3 License

HarfBuzz 核心采用其 COPYING 中说明的 Old MIT 风格许可；子目录或可选组件可能存在单独许可，实际 vendoring 必须检查所启用文件。

## 5. Expat

### 5.1 定位

Expat 只实现 XML byte stream 到 token/event 的解析层：

```text
Byte stream
 -> IXmlTokenizer
     \-> Expat adapter
     \-> Host parser adapter
 -> XAML node stream
 -> Aero schema/object writer
```

XAML namespace、type/member resolution、markup extension、object construction 和 WPF 语义全部由 AeroGUI 实现。

### 5.2 安全配置

Runtime XAML 输入视为不可信：

- 禁用外部实体和网络解析；
- 默认禁止 DTD；
- 设置 document bytes、depth、attributes、name length、text length 和 entity limits；
- parser callback 只产生 bounded token，不直接实例化对象；
- cancellation 和 error location 必须保留；
- 使用持续维护的安全版本并跟踪上游安全公告。

compiled-XAML-only profile 或宿主提供 parser 时可关闭 Expat。

### 5.3 License

Expat 使用 MIT/X Consortium 风格许可；发行包必须保留版权和许可文本。

## 6. libtess2

### 6.1 定位

libtess2 MAY 作为初期 CPU path triangulation fallback：

- fill contour tessellation；
- complex polygon triangulation；
- 将 path geometry 转换为 GPU vertex/index buffer。

它不能定义 AeroGUI 的 Geometry API、mesh cache format 或 GPU path rendering 策略。

### 6.2 风险与默认状态

官方仓库自述为 minimally maintained，且最新正式 release 较旧，因此：

- 默认标记为 experimental；
- 必须通过 `IGeometryTessellator` adapter 隔离；
- 必须使用自有 allocator callback；
- 必须 fuzz self-intersection、degenerate contour、NaN、overflow 和巨大输入；
- 必须记录 vendored patch；
- 中长期可由自研 tessellator、平台实现或其他经过审查的 provider 替换。

### 6.3 License

libtess2 官方仓库说明使用 SGI Free Software License B Version 2.0。纳入发行前必须单独确认许可证义务、NOTICE 和 Apache-2.0 项目分发兼容性。

## 7. Ryu

### 7.1 定位

Ryu 用于 float/double 到十进制字符串的确定性转换：

- XAML number serialization；
- diagnostics；
- snapshot/golden files；
- compiled XAML tooling；
- cross-platform round-trip tests。

Parsing 使用独立严格 parser；Ryu 不定义用户可见 locale formatting。

### 7.2 策略

- 可 vendor 经过裁剪的 C implementation；
- output contract 必须有 NaN、Infinity、negative zero、subnormal 和 round-trip tests；
- 若平台 `to_chars` 替代实现通过完全一致性测试，可关闭 Ryu；
- serialized format 必须固定 decimal separator 为 `.`，不使用 process locale。

### 7.3 License

Ryu 仓库允许 Apache-2.0；`ryu/` 目录还提供 Boost Software License 1.0 选择。依赖 manifest 必须记录实际采用的许可路径。

## 8. sokol

### 8.1 允许用途

`sokol_gfx` MAY 被封装为 `AeroRHI_Sokol`：

- 快速 bring-up；
- samples、tools、WASM experiment；
- D3D11、Metal、GL/GLES、WebGPU 等已支持环境的兼容验证；
- 验证 `RenderPlan` 不依赖具体原生 API。

`sokol_app` 只能用于独立 sample/tool，不进入可嵌入 Runtime。生产宿主仍拥有 window、input、event loop、device 和 presentation。

### 8.2 禁止用途

sokol MUST NOT：

- 成为 `AeroRHI` 的公共类型系统；
- 成为所有 native backend 必经的转发层；
- 阻止 D3D12/Vulkan/Metal 使用其原生 command/synchronization 能力；
- 被视为 Xbox、PlayStation、Nintendo 等专有平台的正式解决方案；
- 将 `sg_*` handle 存入 serialized scene 或公共 SDK；
- 让 renderer 依赖 `sokol_app` 的单窗口/主循环模型。

官方公开后端覆盖并不等同 AeroGUI 目标矩阵，尤其不提供通用 D3D12 和公开 console backend。因此 sokol 只能是 optional adapter，而不是核心 RHI。

### 8.3 License

sokol 使用 zlib/libpng 风格许可。若启用，必须保留许可文本和上游版权声明。

## 9. Dependency 模式

每个 provider 使用独立 target：

```text
AeroText_FreeType
AeroText_HarfBuzz
AeroMarkup_Expat
AeroGeometry_Libtess2
AeroFormat_Ryu
AeroRHI_Sokol
```

Core targets 不直接 link 第三方库：

```text
AeroBase       -> no third-party runtime dependency
AeroCore       -> AeroBase only
AeroCore -> AeroCore only
AeroControls   -> AeroCore only
AeroMarkup     -> AeroControls + IXmlTokenizer
AeroRender     -> AeroCore + AeroRHI + text/geometry interfaces
AeroPlatform   -> host/platform contracts
```

## 10. Build 与 packaging

建议 CMake：

```cmake
option(AERO_WITH_FREETYPE "Enable FreeType provider" ON)
option(AERO_WITH_HARFBUZZ "Enable HarfBuzz provider" ON)
option(AERO_WITH_EXPAT "Enable Expat XML tokenizer" ON)
option(AERO_WITH_LIBTESS2 "Enable experimental libtess2 provider" OFF)
option(AERO_WITH_RYU "Enable Ryu formatting" ON)
option(AERO_WITH_SOKOL "Enable optional sokol RHI adapter" OFF)
```

支持三种 source mode：

```text
SYSTEM    use package supplied by toolchain/platform
VENDORED  use pinned source in approved dependency cache/submodule
HOST      do not build library; application supplies provider
```

禁止自动选择不同 provider 后静默改变 capability。最终 manifest 必须记录实际 provider、version 和 build flags。

## 11. CI matrix

至少验证：

- generic recommended profile；
- all-optional-dependencies-off；
- host-provider stubs；
- FreeType without HarfBuzz；
- HarfBuzz with FreeType callbacks；
- Expat parser fuzz/security limits；
- libtess2 fuzz target when enabled；
- Ryu differential/round-trip tests；
- sokol adapter sample when enabled；
- license/NOTICE generation；
- dependency version/CVE audit。

## 12. 更新策略

- 上游安全修复优先于固定 release 节奏；
- 更新必须通过 API compatibility、golden、fuzz、performance 和 packaging tests；
- patch 不直接散落在源码中，统一存放并记录原因；
- abandoned/minimally-maintained dependency 必须有替代计划；
- console SDK build 的第三方源码和许可遵循对应平台保密与分发规则，不提交到公开仓库。
