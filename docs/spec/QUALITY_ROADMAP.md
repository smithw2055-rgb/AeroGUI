# Quality、测试与路线图规范

- **状态**：Runtime Vertical Slice / M3.5 in progress
- **语言**：C++17-only
- **生产 renderer**：AeroRHI native GPU / WebGL 2；不支持 Skia

当前已验证基线：

- M0/M1 的架构、Foundation、Core、ABI 和严格 C++17 构建约束已落地主线；
- M2 的 runtime XAML、布局、RenderTransaction/RenderPlan、AeroRHI_Null 与 Windows D3D11/WARP 垂直切片已落地；
- M3 的 Binding/DataContext、Style/ControlTemplate、compiled XAML document、module SDK 和 `aero-xamlc` 已落地；
- compiled document encoding 固定为 v1，compiled cache format 固定为 v3；
- Debug/Release 与 static/shared 由 CI 矩阵覆盖，`aero-xamlc --check` smoke test 作为 CTest 正式执行；
- M3.5 文本垂直切片已完成 provider-neutral `AeroText` 合同、FreeType/HarfBuzz adapter、glyph atlas、TextLayout 与 atlas-backed TextBlock/D3D11 接入；完整 Unicode line breaking/bidi 仍是后续增量；
- Command、统一交互状态、键盘焦点导航、setter-based VisualStateManager、Button/RepeatButton、ToggleButton/CheckBox/RadioButton 与 Generic/Light/Dark 主题已完成并有 XAML/交互回归；
- ScrollViewer/ScrollBar、ItemsControl/container generator、Selector/ListBox 与 recycling VirtualizingStackPanel 基线已完成，10k realization-window benchmark 已进入 CTest；
- OpenGL 3.3 的 host-injected function table、Core Profile/线程/context-generation 合同、capability/limits 查询、`HostReset`/`PreserveAndRestore` state cache 与完整 AeroRHI 资源/提交/GLsync/读回/外部导入切片已完成，并由注入式 fake-GL conformance 覆盖；RenderPlan lowering、WGL/GLX 真实 context/surface、共享像素 conformance、TextBox/IME、ControlGallery 和最终质量门禁仍待完成。

## 1. Diagnostics

```cpp
struct Diagnostic {
    DiagnosticCode code;
    Severity severity;
    String message;
    SourceSpan source;
    ObjectId object;
    PropertyId property;
    Vector<DiagnosticNote> notes;
};
```

稳定分类：

- `BASExxxx`
- `XAMLxxxx`
- `DPxxxx`
- `BINDxxxx`
- `LAYOUTxxxx`
- `INPUTxxxx`
- `RENDERxxxx`
- `RHIxxxx`
- `GLCTXxxxx`
- `WEBGLxxxx`
- `PLATFORMxxxx`
- `DEPENDxxxx`

错误必须包含可行动信息。XAML、Binding、runtime data、device/context loss、shader compile/link 和 provider 缺失不得以未处理 exception 终止 frame loop。

## 2. Trace 与 Inspector

Trace channels：

- allocator、memory tag、OOM；
- object/ref/weak lifetime；
- property evaluation；
- resource lookup；
- Binding attach/evaluate/update；
- layout invalidation/pass；
- event route；
- scene transaction；
- RenderPlan/batch/pass；
- GPU upload/fence/device loss；
- GL state/context/current-thread；
- WebGL extension/context loss/restore；
- glyph/image/geometry cache；
- frame/browser scheduling；
- provider/version/capability。

Release 默认关闭详细 trace；启用不得改变语义。M4 Inspector 展示 logical/visual/render trees、effective-value providers、layout slots、event listeners、RenderPlan、GPU batches、GL/WebGL caps、cache budgets 和 frame timings。

## 3. C++ API 与 ABI

- namespace `Aero`；
- public C++ type/method 使用 PascalCase；
- `enum class`；
- UTF-8 `String`/`StringView`；
- 可恢复错误使用 `Result<T>`；
- 项目 Runtime API 不 throw；
- reflection/cast 不依赖 C++ RTTI；
- v1 只保证 C++ source compatibility；
- shared-library/plugin boundary 使用 versioned C function table、opaque handle、POD Span/StringView；
- public header 不暴露 STL owning type、iterator、allocator、exception、`type_info` 或 C++20 type；
- callback 记录 calling convention、thread、reentrancy 和 lifetime；
- JS/WASM bridge 不成为 C++ public ABI 的替代品。

## 4. Build 基线

所有 C++ target MUST 使用：

```cmake
set_target_properties(target PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
```

禁止：

- target 设置 C++20 或更高标准；
- C++20-only header/API；
- configure 阶段下载未锁定依赖；
- public headers 依赖 compiler extension；
- 依赖/后端是否启用却不进入 capability manifest；
- WebGL build 静默退回 WebGL 1；
- GL build 静默使用 compatibility profile。

构建系统：

- CMake presets + CTest；
- dependency lockfile/manifest；
- 默认 static，可选 shared；
- generated code 写入 build tree；
- warning-as-error 只针对项目代码；
- shader/package generation 可重复；
- console/private SDK targets 与公开仓库隔离；
- WebAssembly/browser package 记录 Emscripten/toolchain revision 与 linker flags。

## 5. Compiler 与平台矩阵

### Windows

- Visual Studio 2026 / MSVC，以 `/std:c++17 /permissive- /Zc:__cplusplus /Zc:preprocessor /utf-8` 构建；
- x64 与 ARM64；
- D3D12、D3D11、WGL/OpenGL 3.3；
- shared/static ABI smoke tests；
- Windows WPF conformance probes。

### Linux

- Clang/GCC C++17；
- Vulkan；
- X11 + GLX 1.4 + OpenGL 3.3 Core；
- EGL + GLES/OpenGL where available；
- headless/fuzz/sanitizer runners。

### Android

- Android NDK C++17 + libc++；
- Vulkan strategic path；
- EGL + GLES 3.0 compatibility path；
- pause/resume/surface loss tests；
- allocator and single C++ runtime integration checks。

### Apple

- Apple Clang C++17；
- Metal strategic path；
- iOS/macOS legacy GL only when host supplies it，not default release target；
- mobile lifecycle and accessibility tests。

### Web

- C++17 → WebAssembly；
- WebGL 2；
- at least two browser engines in automated CI where available；
- main-thread baseline；
- optional Worker/OffscreenCanvas job；
- no WebGL 1 fallback；
- context-loss recovery tests。

### Console

- 由授权 SDK 固定 toolchain；
- public core 必须能在 C++17、exceptions-off、RTTI-off 配置编译；
- private backend CI 不暴露 NDA log/artifact。

## 6. Build profiles

```text
AERO_PROFILE_PORTABLE
  C++17
  exceptions API off
  RTTI dependency off
  all optional dependencies off
  AeroRHI_Null

AERO_PROFILE_GENERIC
  C++17
  FreeType + HarfBuzz + Expat + Ryu
  strategic native backend

AERO_PROFILE_COMPAT
  C++17
  D3D11 or GL3.3 or GLES3
  no compute/bindless assumptions

AERO_PROFILE_WEB
  C++17 -> WebAssembly
  WebGL2 only
  main-thread requestAnimationFrame baseline
  context-loss recovery

AERO_PROFILE_ENGINE
  C++17
  host allocator/filesystem/jobs/text/image/device/context
  no owned window or Present

AERO_PROFILE_CONSOLE
  C++17
  exceptions-off / RTTI-off
  offline shader package
  private platform/RHI adapter
```

同一 public behavior 不得因 profile 改变；差异必须体现在 capability manifest。

## 7. 建议目录

```text
include/Aero/{Base,Core,Markup,Presentation,Controls,Render,Platform}
src/{base,core,markup,presentation,controls,render,platform}
backends/{rhi_d3d12,rhi_d3d11,rhi_vulkan,rhi_metal}
backends/{rhi_opengl33,rhi_gles30,rhi_webgl2,rhi_null,rhi_sokol}
backends/{platform_win32,platform_glx,platform_egl,platform_wgl}
backends/{platform_android,platform_apple,platform_web}
private_backends/{console_*}
tools/{xamlc,shaderpack,inspector}
tests/{unit,conformance,golden,layout,render,rhi,web,fuzz,perf,abi}
third_party/{manifests,patches,licenses}
samples
docs/{adr,spec}
```

不存在 `render_skia` target 或目录。

## 8. Foundation tests

Unit/property/fuzz tests 覆盖：

- allocator alignment、tag、lifetime、injected OOM；
- UTF-8 validation、iteration、UTF-16 bridge；
- String SSO/heap transition；
- Vector/SmallVector 对 trivial/non-trivial type 的 construction/move/destruction；
- HashMap/HashSet collision、rehash、erase、OOM 和 differential tests；
- Ref/WeakRef copy/move/lock/concurrent-destroy；
- Result/Optional/Value；
- C API struct-size/version；
- public header compile with C++17 only；
- source scan 阻止 C++20 token/header accidental use。

## 9. Core 与 Presentation tests

Unit tests 覆盖：

- TypeRegistry；
- DP registration/metadata/precedence/inheritance；
- Dispatcher ordering/reentrancy；
- ResourceDictionary；
- Binding path；
- layout algorithms；
- event route；
- collection notification batch；
- RenderTransaction apply/merge/replay。

Windows 上维护独立 C# WPF probe：

- 输入确定 XAML/代码；
- 输出规范化 JSON；
- 记录公开可观察的 effective values、trees、DesiredSize、ArrangeRect 和 event order；
- AeroGUI 执行等价 fixture；
- known difference 写 capability manifest；
- 禁止使用 WPF 私有反射。

## 10. XML/XAML tests

每个支持语法至少包含：

- valid fixture；
- invalid fixture；
- expected diagnostics；
- expected object tree；
- expected source span；
- expected layout；
- 可选 expected RenderPlan/image。

`IXmlTokenizer` conformance suite 对 Expat 和 host test tokenizer 运行同一输入。

安全测试：

- DTD/external entity 默认拒绝；
- depth、attribute、name、text、object 和 byte limit；
- cancellation；
- malformed UTF-8；
- parser callback reentrancy；
- allocation failure；
- Expat 更新后的 regression corpus。

## 11. Render 与 RHI 测试分层

1. Scene/RenderTransaction structural tests；
2. RenderPlan snapshot；
3. `AeroRHI_Null` validation；
4. common RHI backend conformance；
5. geometry/glyph placement snapshots；
6. native/WebGL pixel tests；
7. context/device loss tests；
8. long-running resource/stress tests。

所有 backend 使用同一基础 fixture；capability-specific fixture 必须显式标注 required caps。

## 12. Strategic backend gates

### D3D12

- hardware/debug layer clean；
- owned/borrowed device；
- resource state/fence；
- device loss；
- Windows + GDK adapter contract where available。

### Vulkan

- validation clean；
- owned/borrowed device/queue；
- Android/Linux；
- descriptor/pipeline cache；
- device/surface recreation。

### Metal

- macOS/iOS；
- tile/offscreen budget；
- drawable loss/backgrounding；
- host command-buffer integration。

## 13. D3D11 gates

- FL10_0；
- FL11_0/11_1；
- VS/PS-only baseline；
- optional feature query；
- owned/borrowed device/context；
- debug layer clean；
- RT/SRV hazard cleanup；
- state preserve vs host reset modes；
- DXBC package/reflection consistency。

Feature level 9_x 不是 release gate。

## 14. OpenGL/GLES/GLX/EGL/WGL gates

### OpenGL 3.3

- Core Profile only；
- Linux X11 + GLX 1.4；
- Windows + WGL；
- no-extension baseline；
- function table validation；
- context-current thread violations；
- embedded state leak/restore；
- context recreation；
- GLSL 330 compile/link diagnostics。

### GLX

- owned and borrowed Display/Context/Drawable；
- FBConfig requirements；
- `GLX_ARB_create_context` query/path；
- resize/expose/swap interval；
- X error handling；
- no GLX use on Wayland preset。

### GLES 3.0/EGL

- Android EGL + ES 3.0；
- headless EGL where available；
- no GLES 3.1 assumption；
- tile/offscreen budget；
- pause/resume/surface loss；
- GLSL ES 300 diagnostics。

## 15. WebGL 2 gates

WebGL 2 自动化至少覆盖：

- context creation success/failure；
- WebGL 2 only，拒绝/报告 WebGL 1；
- no-extension core baseline；
- Canvas resize 和 devicePixelRatio；
- requestAnimationFrame scheduling；
- background throttling/time jump；
- shader compile/link error diagnostics；
- buffer/texture/FBO/VAO/sampler lifetime；
- N-frame/sync polling retirement；
- `webglcontextlost`/`webglcontextrestored`；
- `WEBGL_lose_context` repeated loss/restore；
- extension/caps re-query after restore；
- full glyph/image/geometry atlas rebuild；
- shutdown while context lost；
- main-thread baseline；
- optional Worker/OffscreenCanvas；
- pixel tests with locked tolerance。

WebGL test package 必须在至少两个可用浏览器引擎运行；单一浏览器通过不能代表跨浏览器通过。

## 16. Golden 策略（无 Skia）

AeroGUI 不使用 Skia 产生 reference image。Golden 可由以下组合产生：

- 自有、受限、确定性的 CPU reference rasterizer；
- RenderPlan、geometry mesh、glyph ID/position 的结构快照；
- 锁定 native/WebGL backend、driver/browser 的 image；
- 多 backend consensus + 人工批准 baseline。

Text tests 分离：

```text
font selection
shaping glyph IDs/clusters/advances
glyph positions
outline/bitmap hash
atlas placement
final pixels
```

锁定测试字体、FreeType/HarfBuzz build、DPI、locale、hinting、color space 和 tolerance。

## 17. Shader validation matrix

| Backend | Gate |
| --- | --- |
| D3D11 | HLSL → DXBC SM4/5 + reflection |
| D3D12 | HLSL → DXIL |
| Vulkan | canonical source → SPIR-V validation |
| Metal | MSL/metallib package validation |
| OpenGL 3.3 | generated GLSL 330 offline validation + runtime link |
| GLES 3.0 | generated GLSL ES 300 offline validation + runtime link |
| WebGL 2 | WebGL-profile GLSL ES 300 offline validation + browser compile/link |
| Console | private offline compiler/package |

所有 dialect 必须验证 vertex semantics/location、uniform layout、texture/sampler binding、precision define 和 feature guard 等价。

## 18. Optional dependency matrix

CI 至少包含：

| 配置 | 目的 |
| --- | --- |
| all OFF | 验证核心无隐藏依赖 |
| FreeType ON / HarfBuzz OFF | limited text provider |
| FreeType ON / HarfBuzz ON | generic full shaping |
| Expat OFF | compiled XAML/host parser boundary |
| libtess2 ON | experimental geometry adapter fuzz |
| Ryu OFF | alternate formatter conformance |
| sokol ON | optional D3D11/GL/WebGL adapter sample |
| sokol OFF | 所有正式 backend 独立 |

Dependency audit 生成版本、commit、license、NOTICE 和已知安全问题报告。

## 19. Fuzzing

Fuzz targets：

- UTF-8/String conversion；
- Vector/HashMap serialized helpers；
- XML tokenizer；
- XAML node/schema/object writer；
- property conversion；
- Binding path parser；
- geometry parser/tessellator；
- font table/provider wrapper；
- compiled XAML decoder；
- RenderTransaction decoder；
- RenderPlan validator；
- shader metadata/package decoder；
- Web/GL capability manifest parser。

全部 fuzz target MUST headless，具有深度、对象数、字符串、geometry、glyph、resource 和 allocation limits。

## 20. Sanitizer 与静态分析

- ASan、UBSan：Linux/Clang 和可用平台；
- TSan：threaded queue/ref/resource tests；
- MSVC AddressSanitizer；
- compiler warnings；
- clang-tidy/static analyzer；
- D3D debug layers；
- Vulkan validation；
- OpenGL debug callback where available；
- browser console treated as test failure for unexpected WebGL errors；
- API/header ABI scanner；
- dependency license/security scanner。

缺失依赖、浏览器或 SDK 的 job 可明确跳过，但不能把 required gate 伪装为成功。

## 21. 性能门禁

至少测量：

- 10k/100k allocation by tag；
- 10k Object create/destroy；
- 100k Ref copy/move；
- 100k property set/clear；
- 10k Binding updates；
- 深度 100 与宽度 10k tree attach；
- Grid/StackPanel layout；
- 1k/10k Visual scene commit；
- RenderPlan build/batching；
- D3D11/GL state changes and draw calls；
- path tessellation/cache；
- glyph shaping/raster/atlas；
- image upload/cache；
- WebAssembly → WebGL bridge overhead；
- WebGL shader startup time；
- 10k items virtualized scroll；
- mobile/browser offscreen budget。

初始方向性预算：

| 场景 | M2 | M4 |
| --- | ---: | ---: |
| 空 frame UI update | < 0.20 ms | < 0.10 ms |
| 1k Visual 无变化 commit | O(1) dirty roots | < 0.10 ms |
| 1k simple elements layout | < 2.0 ms | < 1.0 ms |
| 1k property changes | < 1.0 ms | < 0.5 ms |
| 10k item scroll | 不全量实例化 | < 2 ms UI work/frame |

数值必须绑定 reference hardware/toolchain/backend/browser。优化不得绕过语义测试。

## 22. 安全与稳健性

- XAML loader 限制 depth、objects、strings、attributes 和 resources；
- URI provider 默认禁止 network；Web fetch 由 host policy 显式允许；
- compiled XAML 校验 offset/length/version；
- RenderTransaction/RenderPlan decoder 不信任输入；
- 处理 integer overflow、NaN、Infinity 和超大 geometry；
- image/font/XML/tessellation provider 位于隔离边界；
- markup extension 可被 policy 禁用；
- diagnostics 不泄露敏感文件内容；
- callback 失败在 frame boundary 转为 Result/diagnostic；
- native GPU resource release 等待 fence；
- GL/WebGL 不 busy-wait，使用安全延迟回收；
- mobile suspend/surface loss 可恢复；
- WebGL context loss 可恢复；
- dependency security release 触发快速升级流程。

## 23. 路线图

### M0 — Architecture baseline

交付：

- README、主规范与分章规范；
- C++17/Foundation、native GPU、compatibility backend、dependency ADR；
- dependency manifest/NOTICE policy；
- CMake/CI skeleton；
- capability manifest schema。

验收：

- 无 C++20/Skia 冲突描述；
- D3D11/GL/GLX/GLES/WebGL2 分层明确；
- module DAG 无循环；
- clean-room policy 明确；
- M1 work items 映射到规范。

### M1 — Foundation 与 Core

交付：

- allocator/String/Vector/HashMap/HashSet；
- Object/Ref/WeakRef；
- TypeRegistry、Dispatcher、DependencyProperty；
- diagnostics/C API baseline。

验收：

- exceptions-off/RTTI-off；
- C++17 MSVC/Clang/GCC；
- lifetime/OOM/container/property tests；
- Core 无 platform/render/third-party dependency。

### M2 — Vertical slice

状态：**完成（主线 runtime vertical-slice 基线）**。

交付：

- runtime XAML、StaticResource、NameScope；
- Visual/UIElement/FrameworkElement；
- Canvas/StackPanel/Grid/Border/TextBlock；
- RenderTransaction、RenderPlan、AeroRHI_Null；
- 第一个 strategic 或 compatibility GPU backend；
- XAML → layout → GPU image sample。

### M3 — Application model 与桌面/移动兼容

状态：**进行中（M3.5）**。Binding/DataContext、Style/Template、compiled XAML、D3D11 基线与 `AeroText` provider 合同已完成；Command/controls、FreeType/HarfBuzz/atlas/TextBlock 文本实现、Items/virtualization、OpenGL 3.3/WGL/GLX 与应用样例尚未完成。

交付：

- Binding/DataContext、DynamicResource、Style/Template；
- Routed Event/Input/Command；
- Button/ItemsControl/ListBox/ScrollViewer；
- FreeType/HarfBuzz text provider；
- strategic backend 至少两个；
- D3D11/GL3.3/GLES3 至少两个；
- GLX/EGL/WGL integration tests；
- Android/Linux sample。

### M4 — Production runtime 与 Web

交付：

- UI/render queue；
- D3D12/Vulkan/Metal；
- D3D11/GL3.3/GLES3/WebGL2；
- offscreen/effects/atlas；
- Web browser host、context-loss recovery 和 WASM sample；
- animation/virtualization/accessibility/inspector；
- console adapter contract；
- locked performance/security/stress gates。

## 24. 首批 Issues

建议顺序：

1. Bootstrap strict C++17 CMake targets and CI；
2. Define allocator/memory tags/OOM injection；
3. Implement String/StringView/UTF conversion；
4. Implement Vector/SmallVector；
5. Implement HashMap/HashSet；
6. Implement Object/Ref/WeakRef；
7. Implement versioned C ABI skeleton；
8. Implement deterministic TypeRegistry；
9. Implement Dispatcher/thread affinity；
10. Implement DependencyProperty registration/metadata；
11. Implement sparse effective-value table；
12. Define diagnostics/trace schema；
13. Implement IXmlTokenizer + Expat adapter；
14. Implement XAML node interfaces；
15. Implement Visual/UIElement layout skeleton；
16. Implement RenderTransaction/RenderPlan；
17. Implement AeroRHI_Null；
18. Implement first strategic backend；
19. Implement D3D11 FL10_0 compatibility backend；
20. Implement GL3.3 core backend and state contract；
21. Implement GLX/WGL adapters；
22. Implement GLES3/EGL adapter；
23. Implement Web platform host and WebGL2 backend；
24. Add WebGL context-loss browser suite；
25. Integrate optional Ryu；
26. Prototype FreeType/HarfBuzz provider；
27. Evaluate/fuzz optional libtess2；
28. Add optional sokol adapter only after AeroRHI contract stabilizes。

任何 control Issue 必须依赖相应 Foundation/Core/Presentation 任务；任何 backend Issue 必须依赖 RenderPlan 与 AeroRHI contract。
