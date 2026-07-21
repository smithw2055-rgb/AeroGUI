# Core Runtime 规范

本章定义 AeroGUI 的对象、线程、metadata、Dependency Property 与树基础。

## 1. 对象和所有权

核心对象使用 intrusive reference counting：

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

MUST：

- child ownership 使用 `Ref<T>`；
- parent、event target 和 Binding source 默认使用 `WeakRef<T>`；
- subscription 返回可确定性解除的 RAII handle；
- render tree 不保存 `Object*`；
- 析构期间禁止重新挂接到树；
- UI 对象引用计数线程安全不代表对象可跨线程修改。

`std::shared_ptr` MAY 用于外部 adapter，但不进入 Core ABI。

## 2. DispatcherObject 与 Freezable

```cpp
class DispatcherObject : public Object {
public:
    bool CheckAccess() const noexcept;
    Result<void> VerifyAccess() const;
    Dispatcher& GetDispatcher() const noexcept;
};
```

所有可变 UI 类型 MUST 派生自 `DispatcherObject`。

`Freezable` 用于 Brush、Geometry、Transform 等共享资源：

- `Freeze()` 后不可变；
- 含动态 Binding、DynamicResource 或 animation 时不能冻结；
- frozen object MAY 跨线程只读共享；
- 修改前必须 `Clone()`；
- 失败必须给出原因。

## 3. 初始化状态

建议生命周期：

```text
Allocated -> Constructed -> Loading -> Initialized -> Attached -> Unloaded -> Destroying
```

构造函数只建立 C++ 不变量。`Loading` 期间 property write 可记录但不立即执行昂贵 callback；`EndInit()` 统一解析 Binding/Resource，并产生最小失效。

## 4. Dispatcher

Dispatcher 提供：

- thread affinity；
- priority queue；
- delayed work 和 timer；
- cancellation；
- frame phase hooks；
- reentrancy guard。

建议优先级：

```text
Send > Input > DataBind > Animation > Layout > RenderCommit
     > Loaded > Normal > Background > Idle
```

XAML `EndInit`、template apply、layout、resource merge、tree attach/detach 和 render commit MUST 受 reentrancy guard 保护。重入工作排队到事务结束后。

## 5. TypeRegistry

XAML、Dependency Property、Binding path 和 Style target 使用统一 metadata。

```cpp
using TypeId = uint64_t;
using MemberId = uint64_t;

struct TypeInfo {
    TypeId id;
    StringView xamlName;
    TypeId baseType;
    Span<const PropertyInfo> properties;
    Span<const EventInfo> events;
    ObjectFactory factory;
};
```

MUST：

- TypeId 在不同机器构建中稳定；
- registration order 可确定；
- duplicate type/member 失败；
- namespace mapping 与 C++ namespace 解耦；
- registry freeze 后不可改变；
- 不依赖 C++ RTTI 字符串或不受控静态初始化。

Metadata SHOULD 由轻量宏和 codegen 组合生成，release SHOULD 支持裁剪未引用类型。

## 6. Dependency Property 注册

```cpp
class DependencyProperty {
public:
    static Result<const DependencyProperty*> Register(
        StringView name,
        TypeId valueType,
        TypeId ownerType,
        PropertyMetadata metadata);

    Result<const DependencyProperty*> AddOwner(
        TypeId ownerType,
        PropertyMetadata metadata) const;

    Result<void> OverrideMetadata(
        TypeId forType,
        PropertyMetadata metadata) const;
};
```

Metadata 至少包含：

- default value/default factory；
- validate、coerce、changed callbacks；
- `AffectsMeasure`、`AffectsArrange`、`AffectsRender`；
- parent invalidation flags；
- `Inherits`；
- `BindsTwoWayByDefault`；
- default `UpdateSourceTrigger`；
- read-only key；
- value comparer。

## 7. 有效值存储

每个 `DependencyObject` 使用稀疏 property table。Entry 逻辑上包含：

```text
base provider
+ expression (Binding / DynamicResource)
+ animation overlay
+ coercion result
+ source and dirty flags
```

禁止为每个对象预分配全部 property slots。

普通 property 的可观察优先级：

1. coercion 后最终值；
2. active animation/hold value；
3. local value，包括 `SetValue`、XAML attribute、Binding 和 DynamicResource；
4. TemplatedParent template value；
5. style triggers；
6. template triggers；
7. style setters；
8. theme triggers；
9. theme setters；
10. inherited value；
11. metadata default。

说明：

- Binding/DynamicResource 是 local provider；
- `ClearValue` 恢复下一个 provider；
- `SetCurrentValue` 保留已有 expression；
- `Style` property 的 Style 选择单独实现；
- 边缘次序以 WPF differential tests 固化。

## 8. Property 变更事务

标准流程：

```text
VerifyAccess
 -> Validate
 -> Replace provider
 -> Evaluate expression
 -> Apply animation
 -> Coerce
 -> Compare effective value
 -> Queue callbacks
 -> Queue inheritance propagation
 -> Queue measure/arrange/render invalidation
 -> Queue source update
```

MUST：

- callback 不观察半更新状态；
- 同一 Dispatcher tick 的重复失效可合并；
- inherited value 传播可批处理；
- change callback 造成的二次修改不得递归破坏当前 entry；
- read-only property 只能通过私有 key 写入；
- attached property 使用同一 token/metadata 机制。

## 9. 四种结构

### Object graph

表达所有 C++ 引用，不保证树形，只用于生命周期和调试。

### Logical tree

负责 content model、DataContext/属性继承、resource lookup 和 Loaded/Unloaded。一个节点最多一个 logical parent。

### Visual tree

负责 layout、render ordering、clip/transform/opacity、hit test、event route 和 template visuals。一个 Visual 最多一个 visual parent。

### Render tree

只存在 render domain，包含 stable IDs、parent/child order、drawing data、resource handles 和 generation，不含 user pointers。

## 10. Tree transaction

Attach/detach MUST 原子化：

1. 验证 dispatcher、parent 和 child；
2. 检测循环与重复 parent；
3. 更新 parent link 和 child collection；
4. 传播 inherited context；
5. attach/detach resource 与 Binding context；
6. 标记 layout/render dirty；
7. 延迟 lifecycle events。

失败时不得留下半连接结构。Route、layout 和 template 操作期间的 tree mutation 必须延迟。

## 11. Core 验收

M1 完成时至少满足：

- 10k Object/WeakRef 生命周期测试无泄漏；
- TypeRegistry 输出跨运行稳定；
- property precedence、inheritance、coercion、clear/current value 测试通过；
- thread violation 在 debug 和 Result path 中可检测；
- tree cycle/half-attach 测试通过；
- Core target 不依赖 platform、control 或 render backend。
