# AeroGUI WPF C++ 实现规格

- **状态**：Architecture Baseline
- **版本**：0.3
- **语言**：ISO C++17
- **兼容基准**：WPF 公开行为与 XAML 语义
- **产品参考**：NoesisGUI 的公开产品边界与宿主集成经验
- **实现方式**：clean-room reimplementation

## 1. 产品原则

AeroGUI 是可嵌入、保留模式、原生 GPU 的 C++ XAML UI 引擎。WPF
决定公开类名、对象关系和可观察语义；Facet、metadata、service arena、
RenderFrame 与 GPU backend 是实现机制，不形成第二套开发模型。

已接受的约束：

1. Runtime、工具和产品统一使用 C++17；
2. 公共 ABI 不暴露 STL owning type、异常、编译器 RTTI 或 C++20 类型；
3. `Aero::Application`、`Window`、`DependencyObject`、`UIElement`、
   `FrameworkElement`、Controls、Data、Media、Input、Documents 与 Threading
   使用熟悉的 WPF 名称和语义；
4. 默认桌面宿主是可选产品，核心 Gui 可独立嵌入游戏引擎和现有应用；
5. 生产渲染不使用 Skia；
6. 所有 GPU backend 消费同一 Renderer 与最小 RenderDevice 合同；
7. GLX、EGL、WGL、Canvas 和 native window 是平台 adapter，不是绘制层；
8. 核心不创建渲染线程，宿主拥有 event loop、线程、queue 和 present 调度；
9. FreeType、HarfBuzz、Expat 等第三方实现通过私有边界集成；
10. 安装包只公开产品目标，不公开仓库内部模块或 `_Detail` targets。

## 2. 支持的产品面

```text
Aero::Base
    allocator, strings, containers, Result, Object and Ref

Aero::Gui
    WPF/XAML object model, controls, markup, layout and drawing

Aero::Render
    backend-neutral rendering contracts implemented by Aero::Gui

Aero::RenderD3D11 / Aero::RenderOpenGL33
    opt-in native backend factories

Aero::App
    optional Application/Window desktop host

Aero::Audio
    optional audio product
```

普通桌面应用从 `Application::Run()` 开始；高级宿主加载 XAML 后创建 View。
公开 API 不要求开发者理解内部 GuiKernel、ModuleSet、View implementation、
RenderDevice cache 或平台 peer。

## 3. WPF 对象模型

### 3.1 根类型

```text
Object
└─ DispatcherObject
   └─ DependencyObject
      ├─ Visual
      │  └─ UIElement
      │     └─ FrameworkElement
      │        ├─ Control
      │        ├─ Panel
      │        ├─ Shape
      │        └─ Window
      └─ ContentElement
         └─ FrameworkContentElement
            └─ TextElement
```

Facet 可以替代 WPF 内部实现继承，但不得改变公开语义。外部控件作者看到的仍然是
DependencyProperty、RoutedEvent、Measure/Arrange、Style、Template 和 Binding。

### 3.2 属性与事件语法

- CLR-style wrapper 映射为 `GetXxx()` / `SetXxx()`；
- 布尔 `IsXxx` 使用 `GetIsXxx()` / `SetIsXxx()`；
- 依赖属性标识符为 `XxxProperty`；
- 路由事件标识符为 `XxxEvent`；
- C++ 事件代理使用 `button.Click() += handler`；
- EventArgs 使用 Get/Set API，不把私有存储字段作为 SDK 语法；
- DependencyProperty 和 RoutedEvent 静态声明保持单行，便于审查和工具扫描。

### 3.3 Application 与 Window

`Application` 负责：

- `Current`；
- application resources；
- MainWindow 与真实 Windows 集合；
- Startup/Exit/Activated/Deactivated；
- `OnLastWindowClose`、`OnMainWindowClose`、`OnExplicitShutdown`；
- `Run()` 与 `Shutdown()`。

默认 App host 为每个顶层 Window 保存独立的 native window、View、render
attachment 和输入服务。Application 本身不拥有音频、GPU device 或通用 service
locator。

## 4. XAML 与 metadata

主链路只有一条：

```text
UTF-8 XAML
→ XML token stream
→ XAML node stream
→ Schema and metadata
→ ObjectWriter
→ DependencyObject graph
→ logical/visual attachment
```

Runtime 与 `aero-xamlc` 使用同一 Schema/Facet 语义。未知类型、未知成员、
资源循环、无效 Binding path、线程违规和缺失 capability 必须返回稳定诊断，
不得静默忽略。

普通公开加载入口为 `Gui::LoadXaml<T>()` 与 `Gui::LoadComponent()`；
`Markup::XamlReader` 保留给 Parse、compiled XAML、热重载和工具链。View 不充当
XAML parser、schema registry 或 source-provider service locator。

## 5. 树与路由

AeroGUI 区分：

| 结构 | 职责 |
| --- | --- |
| Object graph | C++ ownership 和一般引用 |
| Logical tree | Content、DataContext、资源与属性继承 |
| Visual tree | layout、hit test、模板视觉结构 |
| Render tree | 紧凑的渲染状态，不保存用户对象指针 |

公开遍历只通过 `LogicalTreeHelper` 和 `VisualTreeHelper`。不得重新引入
ObjectTree、MountService 或 VisualTreeMount 产品抽象。

输入、命令、Control class handlers 和 ContentElement 共用一条稳定 EventRoute：

```text
source
→ snapshot route
→ preview/tunnel
→ bubble
→ class handlers
→ instance handlers
```

命令不得自行遍历 visual/logical parent。

## 6. View 与宿主边界

View 负责一个 UI 实例的：

- root content；
- size；
- input dispatch；
- binding/animation/layout update；
- immutable RenderFrame commit；
- opaque render attachment。

推荐嵌入流程：

```text
Gui.Initialize()
→ Gui.LoadXaml<T>()
→ Gui.CreateView(root, options)
→ View.SetSize(size)
→ host dispatches input
→ View.Update(totalTimeSeconds)
```

宿主拥有 event loop、线程、GPU context/device、queue、surface 和 frame
scheduling。View update 不创建 background thread。

## 7. 渲染架构

实现只保留一条渲染链路：

```text
retained UI
→ RenderTree::Commit()
→ immutable RenderFrame
→ Renderer batching/lowering
→ RenderDevice command stream
→ D3D/OpenGL/GLES/WebGL/Metal/Vulkan/private backend
→ present or return to host
```

不再存在独立 RenderTransaction、通用 RHI 产品、HostedGraphics command
vocabulary 或 backend service lookup。`RenderFrame` 只包含复制的值、稳定 ID、
资源 token 和绘制命令；UI 对象不得跨越该边界。

`RenderDevice` 是 UI renderer 所需的最小私有 GPU 合同，不追求成为通用 3D
engine。Compatibility backend 不依赖 compute、bindless、SSBO、indirect draw
或 persistent mapping。

### 7.1 Backend 等级

| 等级 | Backend |
| --- | --- |
| Strategic | D3D12、Vulkan、Metal、private console |
| Compatibility | D3D11、OpenGL 3.3 Core、OpenGL ES 3.0、WebGL 2 |
| Validation | Null RenderDevice、可选确定性 CPU reference rasterizer |
| Optional | sokol adapter，仅用于 bring-up 和交叉验证 |

WebGL 1 和 OpenGL fixed-function/compatibility profile 不进入 v1。

### 7.2 线程模型

- UI thread 读写 UI 对象并提交 immutable RenderFrame；
- Renderer/RenderDevice 可在宿主选择的同线程或渲染线程运行；
- core 不创建 worker，不维护隐藏 pending-frame queue；
- device/context loss 通过显式状态和 generation 恢复；
- 多个 View 可以共享重量级 native device/resource infrastructure。

## 8. 私有实现与性能

### 8.1 View implementation

每个 View 的稳定服务使用一个对齐 arena placement-construct：

```text
ObjectFactoryScope
EffectiveValueEngine
AnimationEngine
ElementTree
LayoutEngine
RenderTree
ImageCache / TextPipeline
BindingEngine
EventRouter / InputRouter
TemplateEngine / StyleEngine
```

这将多次小对象分配收敛为一次分配，并保留明确的析构顺序。只有生命周期独立的
fragment、trigger、control interaction 和 deferred session 才单独分配。

### 8.2 热路径规则

- 不在 frame/input/layout/render 热路径执行同步日志 I/O；
- 不通过字符串 service ID 查询 runtime service；
- 不在 RenderTree commit 时读取 mutable user object from another thread；
- resource handle 使用 generation，避免 ABA；
- cache 和 deferred release 由 fence/context generation 约束；
- dirty propagation、layout queue、binding update 和 render invalidation 保持增量化。

### 8.3 第三方边界

FreeType、HarfBuzz、Expat、libtess2、Ryu 和 sokol 类型不得进入公共 Aero API。
Provider implementation、warning suppression 和单头库 implementation macro 必须位于
独立私有 translation unit。

## 9. 构建和安装

内部域使用 build-only Object components：

```text
GuiKernel/Text/Controls/Markup objects → Aero::Gui
AppModel/ModuleSet/Runtime/Rendering objects → Aero::Gui
DesktopHost/OS adapters → Aero::App
```

安装包只导出：

```text
Aero::Base
Aero::Gui
Aero::Render
Aero::RenderD3D11
Aero::RenderOpenGL33
Aero::App
Aero::Audio
```

静态包可附带 `_PrivateFreeType`、`_PrivateHarfBuzz` 和 `_PrivateExpat` 来解析
私有第三方符号；shared 包只导出六个产品目标。

## 10. 验证门禁

每次结构重构至少验证：

1. C++17、exceptions-off、RTTI-off、warnings-as-errors 静态构建；
2. shared 构建；
3. architecture checks；
4. `aero-schema-gen`、`aero-xamlc` 和 built-in theme compilation；
5. static/shared `cmake --install`；
6. 独立 `find_package(Aero CONFIG REQUIRED)` 消费者；
7. shared libraries 无 unresolved symbol；
8. Windows 路径在 MSVC 上验证 D3D11、WGL、Win32 Window/IME/Clipboard；
9. 后续恢复 conformance、fuzz、pixel 和 performance tests 时，不重新发布内部层次。

## 11. 非目标

v1 不承诺：

- WPF/.NET binary 或 C# source compatibility；
- BAML；
- FlowDocument、XPS、Printing、MediaElement 或 WPF 3D 全覆盖；
- 使用 C++20；
- Skia renderer；
- WebGL 1；
- 通用 3D RHI；
- 复制 WPF、Moonlight 或 NoesisGUI 内部实现。

详细渲染合同见 `docs/spec/RENDERING_PLATFORM.md`，后端差异见
`docs/spec/COMPATIBILITY_BACKENDS.md`。
