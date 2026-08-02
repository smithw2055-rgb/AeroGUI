# Foundation 与 ABI 规范

- **状态**：Architecture Baseline
- **语言基线**：ISO C++17
- **适用模块**：`AeroBase` 及所有公共 SDK 边界

本章定义 AeroGUI 自有基础设施、内存合同、字符串、容器、引用计数和二进制边界。目标不是重写完整标准库，而是控制跨平台 UI runtime 真正敏感的 ABI、allocator、错误处理、对象生命周期、确定性和调试能力。

## 1. C++17-only 合同

所有 Runtime、工具、测试和示例 MUST：

- 以 C++17 模式编译；
- 设置 `CXX_STANDARD 17`、`CXX_STANDARD_REQUIRED YES`、`CXX_EXTENSIONS NO`；
- 不使用 C++20 language/library feature；
- 不要求 Modules、Concepts、Ranges、Coroutines、`std::span`、`std::format`、`char8_t` 或 C++20 atomic/synchronization API；
- 在 MSVC、Clang 和 GCC 的 C++17 模式进入 CI。

代码可在更新的编译器和 IDE 中构建，但不得因为编译器支持更高标准而引入更高标准依赖。

## 2. Standard Library 使用边界

AeroGUI 不禁止标准库，但区分三个区域：

| 区域 | STL 策略 |
| --- | --- |
| Runtime 公共 ABI | 禁止暴露 STL owning type、iterator、allocator、exception 或 RTTI 类型 |
| Runtime 私有实现 | MAY 使用不穿越模块边界、可在 C++17/no-exception 配置工作的算法或类型 |
| Tools/tests | MAY 广泛使用 C++17 STL，但产物格式必须稳定、显式版本化 |

Runtime 热点中的 owning string、dynamic array、hash container 和 object pointer 使用 AeroGUI 自有类型，以便统一 allocator、memory tag、OOM、profiling 和 console toolchain 行为。

## 3. Allocator

```cpp
struct AllocationRequest {
    size_t size;
    size_t alignment;
    MemoryTag tag;
};

class IAllocator {
public:
    virtual void* Allocate(const AllocationRequest&) noexcept = 0;
    virtual void Deallocate(void* memory, size_t size,
        size_t alignment, MemoryTag tag) noexcept = 0;
protected:
    ~IAllocator() = default;
};
```

MUST：

- 支持宿主注入默认 allocator；
- alignment 必须显式；
- 每次分配具有模块/用途 `MemoryTag`；
- 允许 arena、pool、linear 和 platform allocator adapter；
- 同一分配必须由兼容 allocator 释放；
- 不允许跨动态模块使用一侧 `new`、另一侧 `delete`；
- debug build 记录 size、alignment、tag、callsite token 和 generation；
- allocator 生命周期长于所有使用它的对象和容器。

### 3.1 OOM

Runtime 不通过 C++ exception 报告 OOM。

- 只有解析、值转换、条件所有权获取和缓存命中查询使用约定的
  `TryParse`、`TryFromString`、`TryFromCustom`、`TryFromBorrowed`、
  `TryConvertText`、`TryEncodeValue`、`TryCreateValue` 或
  `TryGetCachedReloadRevision`；其它可失败 API
  直接返回 `Result<T>`；
- 明确标注 no-fail 的内部路径 MAY 调用宿主 `OutOfMemoryHandler`；
- OOM handler 不得返回到无法恢复的操作；
- 容器扩容失败不得破坏原内容；
- fuzz/test build 必须支持 deterministic allocation failure injection。

## 4. String 与 Unicode 边界

```cpp
class StringView {
public:
    const char* Data() const noexcept;
    uint32_t SizeBytes() const noexcept;
};

class String {
public:
    String() noexcept;
    explicit String(IAllocator*) noexcept;
    StringView View() const noexcept;
    const char* CStr() const noexcept;
    Result<void> Assign(StringView);
    Result<void> Append(StringView);
};
```

合同：

- `String` 和 `StringView` 的规范编码是 UTF-8；
- 长度以 byte 计，不以 Unicode scalar 或 grapheme 计；
- owning `String` 保证尾部 NUL，但 API 必须使用显式长度；
- embedded NUL 的允许范围由具体 API 声明；XAML name、URI 和 type name 禁止 embedded NUL；
- 构造入口可选择 strict validation 或 trusted/already-validated 模式；
- UTF-16 仅用于 Windows/Apple/platform bridge，并通过显式 `Utf16View`/conversion buffer；
- normalization、bidi、line breaking 和 shaping 不由 `String` 实现；
- small-string optimization MAY 实现，但对象布局必须由 ABI 版本控制。

`StringView` 不拥有内存，禁止从临时 `String` 隐式构造后跨语句保存。

## 5. Span、Vector 与 SmallVector

```cpp
template<class T> class Span;
template<class T> class Vector;
template<class T, uint32_t InlineCount> class SmallVector;
```

### 5.1 Span

- non-owning contiguous view；
- layout SHOULD 等价于 pointer + count；
- 不允许隐式延长生命周期；
- public C ABI 使用对应 POD `AeroSpan`。

### 5.2 Vector

- contiguous storage；
- 显式 allocator；
- size/capacity 使用固定宽度或经过边界验证的 size type；
- `Reserve`、`Resize`、`PushBack` 具有强失败保证；
- trivially relocatable 优化只能由显式 trait 启用；
- 不承诺 iterator 跨 mutation 稳定；
- 不把内部 capacity 或 growth factor 作为 API 合同；
- release build 必须防止 size multiplication overflow。

### 5.3 SmallVector

`SmallVector<T, N>` 用于短 event route、property change batch、draw state stack 等已通过 profiling 证明的热点。禁止无依据地用它替换所有 `Vector`，避免对象尺寸膨胀。

## 6. HashMap 与 HashSet

```cpp
template<class K, class V, class Hash = DefaultHash<K>,
    class Equal = DefaultEqual<K>>
class HashMap;

template<class T, class Hash = DefaultHash<T>,
    class Equal = DefaultEqual<T>>
class HashSet;
```

MUST：

- 显式 allocator；
- 支持 reserve 和 load-factor 查询；
- insertion failure 不破坏已有映射；
- hash/equality policy 是类型合同的一部分；
- 对字符串使用 byte-stable、跨平台定义明确的 hash；
- 不把 bucket 顺序或 iteration order 当作序列化格式；
- 需要稳定输出时必须排序或使用单独的 ordered container；
- 接收不可信大量 key 的路径必须具备输入配额和 hash-flood 防护策略；
- TypeRegistry 等 build-stable ID 不得依赖进程随机 seed。

具体 probing 算法、bucket layout 和 growth policy 是实现细节，可在不改变可观察合同的情况下优化。

## 7. Collection 不是底层容器

`Collection<T>`、`ObservableCollection<T>` 和 `CollectionView` 属于 UI/Application model：

- 它们是 `Object`；
- 具有 reflection/type metadata；
- 产生 add/remove/replace/reset 通知；
- 受 Dispatcher 线程亲和性约束；
- 内部 MAY 使用 `Vector<Ref<T>>`，但不能继承或暴露底层容器实现；
- 批量修改通过 transaction/batch API 合并通知。

底层 `Vector<T>` 不发送 UI notification，也不参与 Binding。

## 8. Result、Optional 与 Variant

```cpp
template<class T> class Result;
template<class T> class Optional;
class Value;
```

- `Result<T>` 表达可恢复失败，不分配 exception object；
- error 至少包含稳定 code，可附带 diagnostic handle；
- `Optional<T>` 不区分 error，只表达 presence；
- XAML/DP 通用 `Value` 使用受控 tagged storage，不使用未经版本化的 `std::any`；
- boxing 仅在 metadata/binding 需要统一 Object 表示时使用，并应通过 pool 或 inline storage 控制成本；
- public API 不依赖 `typeid`、`dynamic_cast` 或 C++ exception matching。

## 9. Intrusive Ref 与 WeakRef

AeroGUI 对 `Object` 派生类型使用 intrusive reference counting：

```cpp
class Object {
public:
    void AddRef() noexcept;
    void Release() noexcept;
    WeakControlBlock* GetWeakControlBlock() noexcept;
protected:
    virtual ~Object() = default;
};

template<class T> class Ref;
template<class T> class WeakRef;
```

### 9.1 API

推荐入口：

```cpp
auto value = MakeRef<MyObject>(args...);
auto adopted = AdoptRef(rawWithInitialRef);
auto retained = RetainRef(existingRaw);
WeakRef<MyObject> weak = value;
Ref<MyObject> locked = weak.Lock();
```

MUST：

- `Ref<T>` 自动 AddRef/Release；
- move 不改变引用计数；
- `WeakRef<T>` 不延长对象生命周期；
- weak control block 可延迟创建；
- strong count 到零后对象析构，control block 保持到最后一个 weak reference；
- weak lock 必须处理并发销毁；
- parent/event target/binding source 默认使用 weak ownership；
- render tree 不持有 UI `Object*`；
- reference-count thread safety 不授予对象成员跨线程访问权限。

不采用 `AutoPtr` 名称，以避免与已废弃的 `std::auto_ptr` 概念混淆。

### 9.2 Unique ownership

非 `Object` 资源可使用 `Unique<T, Deleter>`。它必须携带与分配匹配的 deleter/allocator，不得跨 DLL 使用默认 `delete` 假设。

## 10. Thread 与原子策略

- `Object` strong/weak 生命周期操作必须定义 thread-safe memory ordering；
- `DispatcherObject` 的业务状态仍只能在所属 Dispatcher 访问；
- immutable/frozen resource 可跨线程读取；
- render-domain handle 使用 generation 防止 ABA；
- container 默认不 thread-safe；并发由拥有者或更高层同步；
- lock-free 数据结构只有在 benchmark、formal invariant 和 sanitizer/stress 覆盖后才能引入。

## 11. Public ABI

AeroGUI 同时提供：

1. **C++ source/static integration API**：可使用 `Aero::*` 类型；
2. **稳定动态模块边界**：版本化 C function table + opaque handle。

示例：

```cpp
extern "C" {
struct AeroStringView {
    const char* data;
    uint32_t size;
};

struct AeroApiHeader {
    uint32_t structSize;
    uint32_t abiVersion;
};

struct AeroRuntimeApi {
    AeroApiHeader header;
    AeroResult (*CreateView)(AeroRuntimeHandle, const AeroViewDesc*,
        AeroViewHandle* outView);
    void (*ReleaseView)(AeroViewHandle);
};
}
```

规则：

- 每个 struct 以 `structSize`/version 实现向前扩展；
- 使用 fixed-width integer、opaque handle、POD span/string view；
- 不跨 ABI 抛异常；
- 不跨 ABI 传 STL、virtual C++ interface、compiler RTTI 或 ownership 不明确的 raw pointer；
- callback 明确 calling convention、thread、lifetime 和 reentrancy；
- allocator、logger、assert、file、time 和 job system 由 host function table 注入。

## 12. Build 配置

Runtime MUST 支持并在 CI 验证：

```text
AERO_EXCEPTIONS=OFF
AERO_RTTI=OFF
AERO_CUSTOM_ALLOCATOR=ON
AERO_BUILD_SHARED=ON/OFF
AERO_ENABLE_MEMORY_TRACKING=ON/OFF
AERO_ENABLE_THREAD_CHECKS=ON/OFF
```

项目代码不主动 throw；即使宿主启用 exceptions，公共合同仍使用 `Result<T>`。Reflection 与 cast 使用 TypeRegistry，不依赖 C++ RTTI。

## 13. Foundation 验收

M1 前半段至少完成：

- allocator alignment、tag、leak、double-free 和 injected-OOM tests；
- UTF-8 validation/conversion/property tests；
- Vector/SmallVector 对 non-trivial type 的生命周期测试；
- HashMap/HashSet differential、collision 和 fuzz tests；
- 100k Ref/WeakRef create/copy/move/lock/destroy stress；
- exceptions-off、RTTI-off 构建；
- MSVC/Clang/GCC C++17 ABI smoke tests；
- dynamic C API struct-size/version compatibility tests；
- public headers 扫描，确认无 C++20 和禁止的 STL ABI 暴露。
