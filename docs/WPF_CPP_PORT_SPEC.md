# AeroGUI WPF C++ Port 重构规格

- **状态**：Architecture Baseline
- **版本**：0.2
- **目标语言**：ISO C++17
- **兼容基准**：WPF 公开行为
- **架构参考**：Moonlight 与 NoesisGUI 的公开资料
- **产品定位**：可嵌入、保留模式、原生 GPU 的 XAML UI engine
- **实现模式**：clean-room reimplementation
- **规范用语**：MUST / SHOULD / MAY 分别表示必须、建议和可选

> 仓库当前没有可重构的既有 C++ UI 实现。本规范中的“重构”是建立一套替代逐类翻译 WPF 的模块边界、行为合同、基础设施、原生 GPU 渲染架构、测试方法和迁移目标，再按垂直切片实现。

## 1. 核心结论

AeroGUI 已接受以下方向：

1. 所有 Runtime、工具、测试和示例只使用 C++17，不采用 C++20；
2. `AeroBase` 自实现 allocator、String、Vector、HashMap、HashSet、Result、Ref/WeakRef 等关键基础类型；
3. 标准库可用于合适的私有实现和工具，但 STL owning type 不进入稳定公共 ABI；
4. AeroGUI 是类似 NoesisGUI 产品定位的原生 GPU UI engine，但采用独立 clean-room 实现；
5. 生产渲染不支持 Skia；
6. 自有 `AeroRHI` 支持 D3D12、Vulkan、Metal 和私有 console backend；
7. sokol 只作为可选 adapter、sample 或 bring-up backend，不是核心 RHI；
8. FreeType、HarfBuzz、Expat、libtess2、Ryu 通过可替换 provider 可选集成；
9. 宿主拥有窗口、线程、event loop、GPU device、queue、command submission 和 presentation；
10. Runtime 公共合同不依赖 exceptions、C++ RTTI 或 C++20 library。

## 2. 为什么不能逐类翻译 WPF

逐类把 `System.Windows.*` 改写成 C++ 容易造成：

- property、Binding、Style、Animation 和 layout 相互硬编码；
- logical、visual 和 render 数据混成一棵对象树；
- render thread 回调 UI/user object，引入锁、重入和悬空引用；
- XAML loader 依赖具体 control，无法独立测试或 AOT；
- Win32、字体库和 GPU API 渗透到 Core；
- 公共 API 暴露 STL/编译器 ABI，难以接入游戏引擎和主机 SDK；
- 看起来像 WPF，却没有 property precedence、resource lookup 和 event order 的行为测试。

AeroGUI 必须打通一条最小而完整的主链路：

```text
UTF-8 XAML
 -> XML token stream
 -> XAML node stream
 -> Type/Property metadata
 -> Dependency Objects
 -> Logical/Visual Trees
 -> Binding/Resource/Style
 -> Measure/Arrange
 -> Immutable RenderTransaction
 -> Retained Render Tree
 -> Native GPU RenderPlan
 -> AeroRHI
```

高级功能不得绕过这条链路。

## 3. 目标

AeroGUI MUST：

1. 提供无 CLR 依赖、可嵌入的 C++17 UI runtime；
2. 对已声明支持的 WPF/XAML 子集提供可测试语义兼容；
3. 实现统一的 Dependency Property、Binding、Resource、Style 和 Template 基础；
4. 分离 object graph、logical tree、visual tree 和 render tree；
5. 使用 retained render tree 与 immutable/incremental scene transaction；
6. 使用自有原生 GPU render pipeline，不依赖 Skia；
7. 支持 D3D12、Vulkan、Metal 和 console-private backend；
8. 支持单线程和 UI/render 双线程宿主模型；
9. 允许宿主提供 allocator、threading、file、text、image、window、GPU 和 accessibility provider；
10. 支持 exceptions-off、RTTI-off 构建；
11. 对未知、受限或部分支持行为给出稳定诊断；
12. 用 conformance、golden、fuzz、sanitizer 和 performance gates 约束实现。

## 4. 非目标

v1 不承诺：

- WPF/.NET 二进制、ABI 或 C# 源码兼容；
- BAML 兼容；
- FlowDocument、XPS、Printing、MediaElement、WPF 3D 或浏览器插件；
- 首次发布即覆盖全部控件和全部边缘行为；
- C++20、Modules、Ranges、Coroutines 或 C++20 标准库；
- Skia renderer、Skia fallback 或以 Skia 作为 golden oracle；
- 复制 WPF、Moonlight 或 NoesisGUI 内部实现；
- 将 sokol、FreeType、HarfBuzz、Expat、libtess2 或 Ryu 变成不可替换公共依赖。

## 5. 兼容性合同

| 层级 | 合同 | 验证 |
| --- | --- | --- |
| Syntax | XAML 文法、namespace、type/member resolution | parser/object-writer golden tests |
| Semantic | 属性优先级、资源、Binding、布局、事件路由 | WPF differential probes |
| Visual | 几何、文本位置、颜色、clip、template | layout snapshots + native GPU pixel diff |
| API | AeroGUI C++17 source compatibility | compile tests + semantic versioning |
| ABI | versioned C function table/opaque handles | struct-size/version compatibility tests |
| Platform | host/device/input/text contracts | adapter conformance suites |

每个 release MUST 发布 capability manifest，列出：

- 支持的 XAML namespace、markup extension 和 controls；
- features 的 `core` / `partial` / `unsupported` 状态；
- text、font、XML、geometry 和 RHI provider；
- GPU backend 与 capability bits；
- exceptions/RTTI/build flags；
- 已知兼容差异；
- manifest schema version。

Runtime loader 与 `aero-xamlc` MUST 使用同一 manifest。未知类型、未知成员、资源循环、Binding path 错误、缺失 provider 和线程违规不得静默忽略。

## 6. 总体架构

```mermaid
flowchart TB
    Host[Host Application / Game Engine] --> Platform[AeroPlatform]
    Host --> UI[AeroApplication / Dispatcher]
    Host --> Device[GPU Device / Queue / Frame Scheduler]

    UI --> Markup[AeroMarkup]
    Markup --> Core[AeroCore]
    UI --> Presentation[AeroPresentation]
    Presentation --> Core
    Controls[AeroControls] --> Presentation

    Core --> Base[AeroBase]
    Markup --> Base
    Presentation --> Base

    Presentation --> Tx[Immutable RenderTransaction]
    Tx --> Render[AeroRender]
    Render --> RHI[AeroRHI]
    Platform --> RHI
    Device --> RHI

    RHI --> D3D12[D3D12]
    RHI --> Vulkan[Vulkan]
    RHI --> Metal[Metal]
    RHI --> Console[Console Private]
    RHI -. optional .-> Sokol[sokol_gfx adapter]
```

### 6.1 模块职责

| 模块 | 职责 |
| --- | --- |
| `AeroBase` | allocator、memory、String、containers、Result、Ref/WeakRef、diagnostics 基础 |
| `AeroCore` | Object、Dispatcher、TypeRegistry、DependencyProperty、Expression、事件基础 |
| `AeroMarkup` | XML/XAML node stream、schema、object writer、markup extension、compiled XAML IR |
| `AeroPresentation` | Visual/UIElement/FrameworkElement、树、layout、Binding、Resource、Style、Template、Input |
| `AeroControls` | Control、ContentControl、ItemsControl、Panel 和标准控件 |
| `AeroRender` | render tree、scene transactions、geometry/text/image cache、RenderPlan |
| `AeroRHI` | native GPU resource、pipeline、pass、command 和 synchronization contracts |
| `AeroPlatform` | window、input、IME、clipboard、file、time、DPI 和 accessibility bridge |
| `AeroTestKit` | WPF probes、golden XAML、layout/render snapshots、fuzz harness |

约束：

- Base 不依赖第三方 runtime 库；
- Core 不依赖 Presentation、Controls、Platform 或 renderer；
- Presentation 不包含 Win32/X11/Cocoa/D3D/Vulkan/Metal/sokol 类型；
- Render 不依赖 control class；
- RHI 不依赖 XAML、Binding 或 Visual；
- module graph MUST 保持有向无环；
- backend/provider 通过显式 factory/function table 注入。

## 7. C++17 与 Foundation

C++17 是唯一语言基线。Visual Studio 2026 等更新工具链以 `/std:c++17` 构建，不能因为 IDE/toolset 更新而使用更高标准。

`AeroBase` MUST 提供：

```text
IAllocator / MemoryTag / OOM policy
String / StringView / UTF conversion adapters
Span / Vector / SmallVector
HashMap / HashSet
Optional / Result / Value
Object / Ref / WeakRef / Unique
Delegate / Subscription / Handle
```

规则：

- UTF-8 是 Runtime 字符串规范编码；
- containers 使用显式 allocator；
- `Collection<T>` 是 Presentation 层 observable model，不是 Vector 别名；
- public binary boundary 不暴露 STL owning types；
- Runtime API 不 throw，也不依赖 RTTI；
- dynamic SDK/plugin boundary 使用 versioned C function table、POD 和 opaque handles；
- 不使用 `AutoPtr` 名称，intrusive 指针命名为 `Ref<T>` / `WeakRef<T>`。

完整合同见 [`spec/FOUNDATION_ABI.md`](spec/FOUNDATION_ABI.md)。

## 8. 四种结构

| 结构 | 用途 |
| --- | --- |
| Object graph | C++ 所有权和一般引用关系 |
| Logical tree | 内容模型、DataContext/属性继承、资源查找 |
| Visual tree | layout、hit test、event route、template visuals |
| Render tree | render domain 紧凑场景，不含用户对象指针 |

一个 Visual MAY 产生零个、一个或多个 render nodes。Logical parent、visual parent 和 render parent 不要求相同。

## 9. 线程与帧不变量

- 可变 UI 对象只允许在所属 Dispatcher 访问；
- Core 不创建永久线程；
- UI/render 之间只传不可变 transaction、稳定 ID 和显式资源消息；
- render thread 不执行 Binding、layout、XAML 或 user callback；
- 同一合同支持 single-thread immediate apply 和 dual-thread queue；
- platform callback marshal 到 UI Dispatcher；
- Freezable 只有冻结后才可跨线程只读共享；
- GPU submission、fence 和 presentation 由 host policy 控制；
- reference-count thread safety 不等于对象状态 thread-safe。

标准阶段：

```text
PumpPlatformEvents
 -> DispatchInput
 -> FlushPropertyChanges
 -> UpdateBindings
 -> UpdateAnimations
 -> Measure/Arrange
 -> RaiseLifecycleEvents
 -> BuildRenderTransaction
 -> ApplyRenderTransaction
 -> BuildRenderPlan
 -> RecordNativeGpuCommands
```

## 10. 原生 GPU 架构

AeroGUI 的“纯 GPU 渲染”定义为：

> 所有产品级 raster/composition 由 native GPU API 执行；XAML、property、Binding、layout、text shaping、scene diff 和必要的 CPU tessellation 仍在 CPU 侧完成。

`AeroRHI` MUST 支持：

- external/embedded device mode；
- host-provided render target 和 command context；
- resource/pipeline/sampler/texture/buffer handles；
- pass、barrier、upload、readback 和 fence contracts；
- capability query；
- device loss/recreate；
- offline shader package；
- generation-safe handles；
- no implicit Present in embedded mode。

正式公开 backend：

```text
AeroRHI_D3D12
AeroRHI_Vulkan
AeroRHI_Metal
AeroRHI_Null
AeroRHI_ConsolePrivate (restricted repositories)
```

Skia 不进入依赖图、测试图或 fallback 图。

## 11. sokol 决策

`sokol_gfx` MAY 通过 `AeroRHI_Sokol` 使用，但仅限：

- bring-up、sample、tool、WASM experiment；
- 对 RenderPlan/RHI contract 的额外适配验证；
- 它公开覆盖的 D3D11、Metal、GL/GLES、WebGPU 等环境。

`sokol_gfx` MUST NOT：

- 定义 AeroRHI API；
- 成为所有 native backend 的 mandatory lower layer；
- 替代 D3D12/Vulkan/Metal native backend；
- 被视为公开 console support；
- 让 `sokol_app` 接管嵌入式 runtime 主循环；
- 把 `sg_*` type 泄漏到公共接口。

原因是 AeroGUI 的目标包含 D3D12、现代 Vulkan、existing-engine command integration 和专有游戏主机，超出 sokol 公开通用后端合同。保留 adapter 能得到开发效率，但不会锁死产品架构。

## 12. 第三方 provider

| 库 | Interface boundary | 状态 |
| --- | --- | --- |
| FreeType | `IFontFace` / `IGlyphRasterizer` | 推荐默认，可替换 |
| HarfBuzz | `ITextShaper` | 完整复杂文本推荐默认，可替换 |
| Expat | `IXmlTokenizer` | Runtime XAML 推荐默认，可替换/关闭 |
| libtess2 | `IGeometryTessellator` | experimental，可替换 |
| Ryu | `IFloatFormatter` | 推荐默认，可替换 |
| sokol | `IRenderBackend/AeroRHI` adapter | optional，默认关闭 |

规则：

- third-party type 不进入 public API；
- dependency 可以 system、vendored 或 host-provider 模式提供；
- 版本、commit、checksum、license、NOTICE 和 patch 必须锁定；
- dependency-off build 必须存在；
- capability manifest 记录实际 provider；
- untrusted XML/font/geometry 输入必须 fuzz 和配额限制；
- libtess2 因维护状态必须有替代计划；
- HarfBuzz 不负责完整 bidi/line break，保留独立 Unicode/paragraph service；
- 详见 [`THIRD_PARTY.md`](THIRD_PARTY.md)。

## 13. 实施原则

1. **公开行为优先于内部相似**：WPF 行为是基准，参考产品只帮助分层。
2. **基础设施先行**：allocator、String、containers、Ref/WeakRef 先于 UI objects。
3. **失效驱动**：property metadata 精确标记 Measure/Arrange/Render/Inheritance 影响。
4. **事务化变更**：property、tree、template 和 render commit 不暴露半更新状态。
5. **先垂直切片**：禁止先批量创建空 control class。
6. **原生 GPU contract-first**：RenderPlan 不绑定某个 backend。
7. **诊断优先**：不支持状态必须可定位。
8. **测试可重复**：时间、字体、DPI、资源、shader 和 backend fixture 可锁定。
9. **先 source compatibility**：v1 不承诺 C++ ABI，稳定 binary boundary 使用 C API。
10. **依赖可关闭**：第三方库不能成为隐含不可移除的设计支柱。

## 14. 规范分章

以下文件与本文件具有相同规范效力：

- [`spec/FOUNDATION_ABI.md`](spec/FOUNDATION_ABI.md)：C++17、allocator、String、containers、Ref/WeakRef 与 ABI；
- [`spec/CORE_RUNTIME.md`](spec/CORE_RUNTIME.md)：Object、Dispatcher、metadata、Dependency Property 和 tree transaction；
- [`spec/XAML_PRESENTATION.md`](spec/XAML_PRESENTATION.md)：XAML、Resource、Binding、Layout、Event、Style、Template 和 Controls；
- [`spec/RENDERING_PLATFORM.md`](spec/RENDERING_PLATFORM.md)：RenderTransaction、AeroRHI、native GPU、platform、text 和 image；
- [`spec/QUALITY_ROADMAP.md`](spec/QUALITY_ROADMAP.md)：diagnostics、build、test、performance、security、milestone；
- [`THIRD_PARTY.md`](THIRD_PARTY.md)：optional dependency policy。

冲突优先级：最新 Accepted ADR > 本主规范 > 分章规范 > README 示例。

## 15. 已决策事项

| ID | 决策 | 状态 |
| --- | --- | --- |
| D-001 | 全项目 ISO C++17，不采用 C++20 | Accepted |
| D-002 | WPF 公开语义是主要兼容基准 | Accepted |
| D-003 | 自有 AeroBase allocator/String/containers/Result | Accepted |
| D-004 | intrusive `Ref<T>` / `WeakRef<T>` | Accepted |
| D-005 | logical、visual、render tree 分离 | Accepted |
| D-006 | retained render tree + immutable transactions | Accepted |
| D-007 | 原生 GPU AeroRHI；不支持 Skia | Accepted |
| D-008 | 正式 backend 为 D3D12/Vulkan/Metal/console-private | Accepted |
| D-009 | sokol 仅 optional adapter，默认关闭 | Accepted |
| D-010 | third-party 通过可替换 provider，可全部关闭 | Accepted |
| D-011 | public binary boundary 使用 versioned C function table | Accepted |
| D-012 | Runtime API 不依赖 exceptions 或 C++ RTTI | Accepted |
| D-013 | runtime 与 compiled XAML 共享 node/object-writer 语义 | Accepted |
| D-014 | v1 只保证 C++ source compatibility | Accepted |
| D-015 | BAML、FlowDocument、WPF 3D 不进入 v1 | Accepted |

对应 ADR 位于 [`adr`](adr)。

## 16. Clean-room 与许可证

### WPF

允许阅读公开文档、调用公开 API、编写行为测试和记录可观察结果。禁止依赖 WPF 私有实现细节。

### Moonlight

仅作为跨平台 native runtime、宿主边界和 render backend 的历史参考。默认不复制源码；任何源码级复用必须单独评估许可证、隔离方式和 NOTICE。

### NoesisGUI

只允许参考公开文档中的架构概念。禁止提交 SDK 私有源码、反编译结果、受 NDA/许可限制材料、私有 shader 或序列化格式。AeroBase、对象模型、RHI 和渲染算法必须独立设计。

### Third-party

第三方依赖的工程记录不构成法律意见。发行前必须确认实际源码版本、启用组件、许可证选择、归属声明和平台分发限制。

## 17. 参考资料

### WPF

- <https://learn.microsoft.com/dotnet/desktop/wpf/advanced/wpf-architecture>
- <https://learn.microsoft.com/dotnet/desktop/wpf/properties/dependency-properties-overview>
- <https://learn.microsoft.com/dotnet/desktop/wpf/properties/dependency-property-value-precedence>
- <https://learn.microsoft.com/dotnet/desktop/wpf/advanced/layout>
- <https://learn.microsoft.com/dotnet/desktop/wpf/advanced/trees-in-wpf>
- <https://learn.microsoft.com/dotnet/desktop/wpf/events/routed-events-overview>
- <https://learn.microsoft.com/dotnet/desktop/wpf/data/>
- <https://learn.microsoft.com/dotnet/desktop/wpf/advanced/threading-model>

### Moonlight

- <https://www.mono-project.com/docs/web/moonlight/>
- <https://www.mono-project.com/archived/release_notes_moonlight4_preview/>

### NoesisGUI

- <https://www.noesisengine.com/docs/Gui.Core.Architecture.html>
- <https://www.noesisengine.com/docs/Gui.Core.CppArchitectureGuide.html>
- <https://www.noesisengine.com/docs/Gui.DependencySystem.Index.html>
- <https://www.noesisengine.com/docs/Gui.Core.RenderingTutorial.html>

### Optional dependencies

- <https://github.com/floooh/sokol>
- <https://freetype.org/>
- <https://github.com/harfbuzz/harfbuzz>
- <https://libexpat.github.io/>
- <https://github.com/memononen/libtess2>
- <https://github.com/ulfjack/ryu>