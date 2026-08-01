# Third-Party Dependency Policy

- **状态**：Architecture Baseline
- **原则**：private、pinned、auditable、replaceable where product semantics allow

AeroGUI 自有对象模型、XAML、布局、绑定、渲染树、Renderer 和 RenderDevice
合同。第三方库只提供边界清晰的通用能力，不决定公共 API 或 WPF/XAML 语义。

## 1. 通用规则

每个第三方依赖必须：

- 位于私有 adapter/provider 或独立 implementation translation unit；
- 不把第三方 struct、enum、handle、allocator 或 error type 带入公共 Aero API；
- 固定 source/version/commit/checksum，并记录 license、patch 和 owner；
- 不在 CMake configure 时下载未锁定源码；
- 对不可信字体、XML、图片和几何输入实施配额、fuzz 和安全更新；
- 限制 warning suppression 到 provider implementation target；
- 不改变已声明的 WPF/XAML 语义或静默降低 capability。

## 2. 当前依赖

| 依赖 | Build option | 默认 | 用途 |
| --- | --- | --- | --- |
| FreeType | `AERO_WITH_FREETYPE` | ON | font face、outline、hinting、glyph raster |
| HarfBuzz | `AERO_WITH_HARFBUZZ` | ON | OpenType/AAT shaping |
| Expat | `AERO_WITH_EXPAT` | ON | streaming XML tokenization |
| miniaudio | `AERO_WITH_MINIAUDIO` | ON | optional audio implementation |
| stb_image | Runtime private source | ON | current image decode implementation |
| sokol_gfx | `AERO_ENABLE_SOKOL_BACKEND` | OFF | optional RenderDevice validation bridge |

libtess2 与 Ryu 仍是可选候选，不进入当前默认产品闭包。

## 3. Text providers

### FreeType

FreeType 实现字体文件访问、metrics、outline 和 rasterization，不负责 bidi、line
breaking 或 paragraph layout。`FT_Face`、load flags 和 allocator callback 不得离开
adapter。字体输入视为不可信数据。

### HarfBuzz

HarfBuzz 实现 script/language/direction-aware shaping，不负责完整 bidi、line
breaking 或 UI paragraph formatting。HarfBuzz 与 FreeType 作为内置 View text
pipeline 编译进 Integration 产品，但不会形成独立 Aero SDK target。

静态安装包可携带 `_PrivateFreeType` 与 `_PrivateHarfBuzz` 来解析私有符号；shared
安装包不导出这些依赖目标。

## 4. Expat

Expat 只实现：

```text
byte stream → XML tokens → Aero XAML node stream
```

XAML namespace、type/member resolution、markup extension、ObjectWriter、Facet 和
WPF 语义全部由 AeroGUI 实现。

Runtime XML：

- 禁用外部实体和网络解析；
- 默认禁止 DTD；
- 限制 bytes、depth、attribute count、name/text length；
- callback 只产生 bounded token，不直接构造对象；
- 保留 cancellation 和 source location。

Vendored static package 可携带 `_PrivateExpat`；使用 system Expat 时 package config
显式查找 `EXPAT::EXPAT`。

## 5. miniaudio 与 stb_image

单头库 implementation macro 必须位于独立 `.cpp`：

```text
src/audio/MiniaudioImplementation.cpp
src/runtime/StbImageImplementation.cpp
```

高层 Audio/Image runtime 不包含 implementation macro，不继承第三方 warning
策略，也不暴露第三方类型。

## 6. Optional sokol adapter

`sokol_gfx` 只可作为私有 RenderDevice bridge，用于：

- backend bring-up；
- WebAssembly 实验；
- RenderFrame/Renderer 差异验证；
- 示例或工具。

它不得：

- 定义公共 Aero API；
- 成为所有 native backend 的共同最低层；
- 限制 D3D12/Vulkan/Metal/private-console capability；
- 把 `sg_*` 类型带入公共头或 serialized data；
- 让 `sokol_app` 接管嵌入式宿主的窗口、输入或主循环。

## 7. 构建和安装

第三方与 Aero 内部 source domains 的关系：

```text
FreeType/HarfBuzz objects + private archives
    → folded into Aero::Integration

Expat-backed markup objects
    → folded into Aero::Gui

miniaudio
    → Aero::Audio

optional sokol bridge
    → folded into Aero::Integration
```

Gui kernel、Controls、Markup、Runtime 和 Rendering 都是 build-only Object
components；第三方依赖不会导致额外 Aero `_Detail` 产品目标。

## 8. License

- FreeType：发行配置采用并履行 FreeType License 路径；
- HarfBuzz：Old MIT-style；
- Expat：MIT/X Consortium-style；
- miniaudio：按 vendored license 记录；
- stb：按 vendored public-domain/MIT dual terms 记录；
- sokol：zlib/libpng-style。

实际分发必须随依赖 manifest 和 NOTICE 保留准确版权与许可证文本。本文件不替代
正式法律审查。
