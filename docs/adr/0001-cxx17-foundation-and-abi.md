# ADR-0001：C++17、Foundation 类型与 ABI

- **状态**：Accepted
- **日期**：2026-07-21
- **决策者**：AeroGUI maintainers

## 背景

AeroGUI 目标包括 Windows、Linux、macOS、Android、iOS、游戏引擎和受限游戏主机 SDK。项目需要统一 allocator、无异常错误路径、对象生命周期、内存追踪、确定性序列化和跨动态模块 ABI，同时不能迫使宿主升级到 C++20 或暴露某一标准库实现的 ABI。

## 决策

1. 所有 Runtime、工具、测试和示例统一使用 ISO C++17；项目不采用 C++20。
2. CMake target 必须设置：

   ```cmake
   CXX_STANDARD 17
   CXX_STANDARD_REQUIRED YES
   CXX_EXTENSIONS NO
   ```

3. `AeroBase` 自主实现并维护以下基础合同：
   - allocator、memory tag、OOM policy；
   - UTF-8 `String` / `StringView`；
   - `Span`、`Vector`、`SmallVector`；
   - `HashMap`、`HashSet`；
   - `Optional`、`Result`、通用 `Value`；
   - `Object`、intrusive `Ref<T>` / `WeakRef<T>`、`Unique<T>`；
   - delegate、subscription 和 opaque handle。
4. 不采用 `AutoPtr` 名称；对象自动引用类型明确命名为 `Ref<T>` / `WeakRef<T>`，避免与已废弃的 `std::auto_ptr` 概念混淆。
5. `Collection<T>` / `ObservableCollection<T>` 是 Presentation 层的可观察对象模型，不是 `Vector<T>` 的别名或继承包装。
6. 标准库并非全面禁止：Runtime 私有实现和工具可在 C++17、ABI 和构建约束允许时使用 STL；但公共二进制边界不暴露 STL owning type、allocator、iterator、exception 或 RTTI 类型。
7. Runtime 公共 API 不依赖 C++ exception 或 C++ RTTI。
8. 稳定 shared-library/plugin 边界使用版本化 C function table、opaque handle、fixed-width POD、`StringView` 和 `Span`。
9. 必须支持宿主 allocator，并验证 exceptions-off、RTTI-off、shared/static 配置。

## 原因

- C++17 能覆盖更保守的移动、引擎和主机工具链。
- 自有关键类型可以统一内存域、OOM、调试、profiling 和 ABI。
- 限定实现范围比重写完整标准库风险更低。
- intrusive 引用计数适合大量细粒度 UI 对象，并能与 TypeRegistry、WeakRef 和 Dispatcher 生命周期整合。
- C function table 可隔离 MSVC、libc++、libstdc++ 和编译选项之间的二进制差异。

## 后果

### 正面

- 宿主不必启用 C++20。
- 公共 SDK 可在多工具链、多 Runtime 和多动态模块环境中稳定集成。
- 内存与对象生命周期可统一追踪。
- exceptions-off、RTTI-off 和主机 SDK 适配成为一等配置。

### 代价

- Foundation 类型需要完整的生命周期、OOM、fuzz、sanitizer 和性能测试。
- 不能直接把标准库容器作为公共接口。
- 需要维护 C++ source API 与稳定 C ABI 两层合同。

## 被否决方案

- **全项目 C++20**：会增加工具链和标准库能力假设，且对 GPU 性能没有决定性收益。
- **公共 API 直接使用 STL**：跨 DLL、编译器、allocator 和主机引擎边界风险过高。
- **全面重写标准库**：范围过大；AeroGUI 只实现 runtime 必需、需要控制合同的部分。
- **使用 `std::shared_ptr` 作为核心对象模型**：无法充分控制对象布局、weak block、allocator、metadata 和 ABI；可在外部 adapter 内使用。

## 验证

- 三大编译器 C++17-only header/build gate；
- exceptions-off、RTTI-off；
- allocator/OOM injection；
- String/container property/fuzz tests；
- Ref/WeakRef stress；
- C ABI version/struct-size compatibility；
- public header 扫描无 C++20 与禁止的 STL ABI。