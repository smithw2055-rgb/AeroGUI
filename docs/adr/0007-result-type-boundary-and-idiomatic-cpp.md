# ADR-0007：Result<> 包装系统性治理与现代原生 C++ 契约

- **状态**：Accepted
- **日期**：2026-10-24
- **决策者**：AeroGUI Maintainers & Architecture Review
- **相关规格**：`docs/spec/FOUNDATION_ABI.md`、`docs/WPF_CPP_PORT_SPEC.md`

## 背景

在 AeroGUI 早期架构迭代中，`Base::Result<T>` 模式被过度泛化应用至应用层、UI 核心交互层与基础值系统中。这种泛化导致了以下严重的架构痛点：
1. **双重布尔样板膨胀**：如 `ICommand::CanExecute(...)` 与 `UIElement::Focus()` 返回 `Result<bool>`，迫使调用端充斥 `if (res && res.Value())` 等繁冗代码。
2. **指针双重包裹**：`ApplicationCommands::Copy()` 与 `Gui::CreateView()` 返回 `Result<Ref<T>>`，无法直接链式调用，违背了现代 C++ 智能指针以空引用代表失效的标准惯用习惯。
3. **无错操作强制防错**：标量封箱 `ValueCodec<T>::Encode(...)`、音频播放 `PlayMusic(...)`、窗口展示 `Window::Show()`、文本编辑 `TextBox::DeleteBackward()` 等无异常/必成功的轻量操作被包裹进 `Result<void>`，产生了大量 `(void)res` 抛弃逻辑和热路径对象构造开销。

## 决策

建立严格的**外部受控边界保留**与**内部全面移除**原则：

1. **受控边界保留 `Result<T>`**：
   - 外部不可信 XAML 文本解析（`XamlReader::Load`、`XamlParser`、`TypeResolver::ResolveType`、`MarkupExtension::ProvideValue`）。
   - 外部动态 I/O（`ImageSource::LoadFromFile`、`Stream::Read`、`Value::TryFromString`）。
   - 动态未类型化绑定转换（`IValueConverter::Convert`）。

2. **内部与应用层全面移除 `Result<T>`，拥抱现代 C++ 原生契约**：
   - **静态命令**：`ApplicationCommands` 22 个静态命令返回 `Ref<RoutedCommand>`，`RegisterDefaults()` 返回 `void`。
   - **交互谓词**：`ICommand::CanExecute`、`RoutedCommand::CanExecute` 返回原生 `bool`。
   - **UI 核心交互**：`UIElement::Focus()`、`CaptureMouse()` 等返回 `bool`（严格遵循 WPF 行为规范：状态成功转移返回 `true`，已有焦点且未发生转移返回 `false`）；`ReleaseMouseCapture()` 返回 `void`。
   - **运行时视图**：`Gui::CreateView(...)` 返回 `Ref<View>`；`View::SetContent(...)` 返回 `bool`；`SetViewport(...)` 返回 `void`。
   - **控件与编辑系统**：`Panels::RemoveChildCore` 返回 `bool`；`IScrollInfo` 滚动操作返回 `bool` 或 `double`；`TextBoxBase`/`TextBox` 内部编辑操作（`DeleteBackward`、`ReplaceSelection`、`Paste`、`AttachScrollViewer` 等）全部改为 `void`。
   - **标量封箱**：`ValueCodec<T>::Encode(...)` 基础标量、枚举、`Nullable<bool>`、`Ref<T>` 直接返回 `Value`，利用 `Result<Value>` 隐式转换无缝适配泛型接口。
   - **编程错误隔离**：内部前置契约违背由 `AERO_ASSERT` 在 Debug 构建中快速暴露，严禁使用 `Result<void>` 掩盖代码逻辑缺陷。

## 后果与收益

- **代码净瘦身**：全栈净删除 432 行繁冗样板代码（116 个文件重构）。
- **零性能开销**：完全消除了热路径（渲染调度、鼠标命中、焦点切换、滚动计算、属性赋值）中的包装对象构造。
- **严格 C++17 合规**：未引入任何 C++20 特性，100% 保持 ABI 与构建系统稳定性。
- **测试全量通过**：`AeroFrameworkConformanceTests`、`aero-conformance`、`aero-inventory-conformance` 均一次性通过。
