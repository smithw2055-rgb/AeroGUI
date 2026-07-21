# Quality、测试与路线图规范

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

- `XAMLxxxx`
- `DPxxxx`
- `BINDxxxx`
- `LAYOUTxxxx`
- `INPUTxxxx`
- `RENDERxxxx`
- `PLATFORMxxxx`

错误必须包含可行动信息。XAML/Binding/runtime 数据错误不得终止 frame loop。

## 2. Trace 与 Inspector

Trace channels：

- property evaluation；
- resource lookup；
- Binding attach/evaluate/update；
- layout invalidation/pass；
- event route；
- scene transaction；
- frame timing；
- cache hit/miss；
- object lifetime。

Release 默认关闭；开启不得改变语义。

M4 Inspector 展示 logical/visual/render trees、effective value providers、active expressions、layout slots、event listeners、frame timings 和 render batches。

## 3. C++ API 与错误

- namespace `Aero`；
- public type/method 使用 PascalCase；
- enum class；
- UTF-8 String；
- 可恢复错误使用 `Result<T>`；
- debug assertion 用于 programmer contract；
- exception facade MAY 由 build option 提供；
- v1 只保证 source compatibility；
- backend/plugin 边界 SHOULD 使用 C function table 或 PImpl。

## 4. Build

- C++20、CMake presets、CTest；
- dependency lockfile；
- third-party dependency 记录版本、许可证、用途和替代方案；
- 默认 static，可选 shared；
- warning-as-error 只针对项目代码；
- generated code 写 build tree；
- configure 阶段不下载未锁定依赖；
- CI 覆盖 MSVC、Clang、GCC；
- ASan、UBSan、TSan 和静态分析进入 CI；
- Windows runner 执行真实 WPF conformance probes。

建议目录：

```text
include/Aero/{Base,Core,Markup,Presentation,Controls,Render,Platform}
src/{base,core,markup,presentation,controls,render,platform}
backends/{render_skia,render_d3d12,render_vulkan,render_metal}
backends/{window_win32,window_sdl}
tools/{xamlc,inspector}
tests/{unit,conformance,golden,layout,render,fuzz,perf}
samples
docs/adr
```

## 5. Unit 与 conformance

Unit tests 覆盖：

- Ref/WeakRef 和 TypeRegistry；
- DP registration/metadata/precedence/inheritance；
- Dispatcher ordering；
- ResourceDictionary；
- Binding path；
- layout algorithms；
- event route；
- render transaction apply。

Windows 上维护独立 C# WPF probe：

- 输入确定 XAML/代码；
- 输出规范化 JSON；
- 记录公开可观察的 effective values、trees、DesiredSize、ArrangeRect 和 event order；
- AeroGUI 执行等价 fixture；
- known difference 写入 capability manifest。

禁止使用 WPF 私有反射。

## 6. Golden 与 render tests

每个支持的语法/控件至少包含：

- valid fixture；
- invalid fixture；
- expected diagnostics；
- expected object tree；
- expected layout；
- 可选 expected image。

Reference backend 产生 golden。GPU backend 与 reference 做容差比较。Text tests 分离 glyph IDs/positions 和 raster pixels，并锁定字体、DPI、locale 和 color space。

## 7. Fuzzing

Fuzz targets：

- XML/XAML tokenizer；
- schema resolution；
- object writer；
- property conversion；
- Binding path parser；
- geometry parser；
- compiled XAML decoder；
- render transaction decoder。

全部 fuzz target MUST 可 headless 运行，并有深度、对象数、字符串长度和资源大小限制。

## 8. 性能门禁

至少测量：

- 10k object create/destroy；
- 100k property set/clear；
- 10k Binding updates；
- 深度 100 与宽度 10k 的 tree attach；
- Grid/StackPanel layout；
- 1k/10k Visual scene commit；
- glyph/image cache；
- 10k items virtualized scroll。

初始方向性预算：

| 场景 | M2 | M4 |
| --- | ---: | ---: |
| 空 frame UI update | < 0.20 ms | < 0.10 ms |
| 1k Visual 无变化 commit | O(1) dirty roots | < 0.10 ms |
| 1k simple elements layout | < 2.0 ms | < 1.0 ms |
| 1k property changes | < 1.0 ms | < 0.5 ms |
| 10k item scroll | 不全量实例化 | < 2 ms UI work/frame |

数值必须绑定 reference hardware。优化不得绕过语义测试。

## 9. 安全与稳健性

- XAML loader 限制深度、对象数、字符串和资源大小；
- URI provider 默认禁止网络；
- compiled XAML 校验所有 offset/length；
- render transaction decoder 不信任输入；
- 处理 integer overflow、NaN 和超大几何；
- image/font decoder 位于隔离边界；
- markup extension 可被 policy 禁用；
- diagnostic 不泄露敏感文件内容；
- user callback 错误在 frame boundary 捕获并报告。

## 10. 路线图

### M0 — Architecture baseline

交付 README、主规范、分章规范、ADR 模板和 manifest schema。

验收：

- module DAG 无循环；
- clean-room policy 明确；
- README 与规范互链；
- M1 work items 能映射到规范。

### M1 — Core runtime

交付 Object/Ref/WeakRef、TypeRegistry、Dispatcher、DependencyProperty、property transactions 和 diagnostics。

验收：

- precedence tests；
- thread violation tests；
- 10k lifetime 无泄漏；
- sanitizer 通过；
- Core 无 platform/render 依赖。

### M2 — Vertical slice

交付 runtime XAML、StaticResource、NameScope、Visual/UIElement/FrameworkElement、Canvas/StackPanel/Grid/Border/TextBlock、single-thread reference renderer 和 sample。

验收：

- XAML → layout → image；
- 50+ golden XAML；
- 20+ WPF layout probes；
- headless tests。

### M3 — Application model

交付 Binding/DataContext、DynamicResource、Style/Template、Routed Event/Input/Command、Button/ItemsControl/ListBox/ScrollViewer 和 compiled XAML prototype。

验收：

- Binding modes/triggers；
- template swap 无泄漏；
- event order 对齐；
- 1k item interactive sample。

### M4 — Production runtime

交付 UI/render queue、GPU backend、offscreen/effects、text/image caches、animation、virtualization、accessibility 和 inspector。

验收：

- no shared UI pointers；
- device loss；
- transaction merge；
- locked performance budget；
- 24h stress；
- Windows/Linux 一致性。

## 11. 首批 Issues

建议依次创建：

1. Bootstrap CMake targets and CI；
2. Implement Object/Ref/WeakRef；
3. Implement deterministic TypeRegistry；
4. Implement Dispatcher and thread affinity；
5. Implement DependencyProperty registration/metadata；
6. Implement sparse effective value table；
7. Implement precedence and invalidation transactions；
8. Define diagnostics and trace schema；
9. Build headless unit test harness；
10. Add Windows WPF probe skeleton；
11. Implement XAML node interfaces；
12. Implement Visual/UIElement layout skeleton；
13. Implement render transaction contracts；
14. Add first vertical-slice sample。

任何控件 Issue 必须依赖相应基础任务。
