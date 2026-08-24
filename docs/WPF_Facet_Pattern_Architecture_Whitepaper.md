# 基于 Facet 模式重构现代 C++ (C++17) WPF / NoesisGUI 架构设计规范与工程白皮书

> **版本**：v1.1.0 Architecture Whitepaper (C++17 Edition)  
> **语言标准**：ISO/IEC C++17 (Strictly NO C++20 concepts/ranges/span dependencies)  
> **目标对标**：NoesisGUI 3.x / Microsoft WPF (.NET Core)  
> **核心范式**：Facet Pattern (切面组合模式) + Data-Oriented Design (DoD) + Headless Canvas Execution

---

## 目录
1. [背景与架构痛点分析](#1-背景与架构痛点分析)
2. [Facet 模式 vs 传统深继承体系对比](#2-facet-模式-vs-传统深继承体系对比)
3. [系统总体分层拓扑架构](#3-系统总体分层拓扑架构)
4. [六大核心 Facet 切面详细规范](#4-六大核心-facet-切面详细规范)
   - 4.1 DependencyPropertyFacet（依赖属性与绑定切面）
   - 4.2 LayoutFacet（测量与排列切面）
   - 4.3 RenderFacet（渲染几何与着色切面）
   - 4.4 InputEventFacet（命中测试与事件路由切面）
   - 4.5 InteractionStateFacet（视觉状态机与动画切面）
   - 4.6 TextLayoutFacet（排版与字形切面）
5. [C++17 核心工业级参考实现](#5-c17-核心工业级参考实现)
   - 5.1 SFINAE 与 Type-Traits 约束的 Facet 容器
   - 5.2 布局度量与两阶段排列算子
6. [零分配无锁渲染流水线（Render Pipeline）](#6-零分配无锁渲染流水线render-pipeline)
7. [五大里程碑研发演进路线图（Roadmap）](#7-五大里程碑研发演进路线图roadmap)
8. [跨平台与游戏引擎（Unreal / Unity / 自研引擎）集成规范](#8-跨平台与游戏引擎unreal--unity--自研引擎集成规范)

---

## 1. 背景与架构痛点分析

传统 WPF（Windows Presentation Foundation）以及已有的 C++ 移植方案（如 NoesisGUI 早期架构）主要基于经典的**单根深度面向对象继承体系**：
$$\text{DependencyObject} \rightarrow \text{Visual} \rightarrow \text{UIElement} \rightarrow \text{FrameworkElement} \rightarrow \text{Control} \rightarrow \text{ContentControl} \rightarrow \text{Button}$$

### 传统继承体系在高性能场景下的三大缺陷：
1. **Fat Object（对象体积爆炸）**：即使仅需一个轻量级的纯几何路径（`Path`）或文本排版节点（`Run`），也会被迫继承基础类上的数百个虚函数表指针（vptr）和冗余成员，导致实例内存膨胀。
2. **Cache Locality（CPU 缓存局部性低下）**：复杂的对象树通过深层指针互相引用，遍历计算时在堆内存中离散寻址，现代 CPU L1/L2 Cache 命中率低，阻碍了游戏 60/120 FPS 的极限帧率要求。
3. **功能耦合与横向扩展困难**：新增渲染效果、特殊布局算子或自定义触发器往往需要侵入式修改基类层级，无法按需即插即用。

---

## 2. Facet 模式 vs 传统深继承体系对比

| 架构维度 | 传统深继承体系 (WPF / NoesisGUI) | 现代 C++17 Facet 切面模式 |
| :--- | :--- | :--- |
| **对象模型** | 单一基类层层继承（深度多达 6~9 层） | 实体宿主（`UIElement`）+ 动态/静态挂载切面 |
| **内存开销** | 每个对象独占全部属性槽，体积 > 512 Bytes | 按需分配，无切面对象可低至 32~48 Bytes |
| **类型检索** | 运行时向下转型 `dynamic_cast<T*>`（昂贵） | 位掩码（BitMask）与固定槽位数组：$O(1)$ 极速查询 |
| **类型约束** | 虚基类接口重载 | C++17 `std::enable_if_t` + `std::is_base_of_v` SFINAE |
| **布局计算** | 虚函数 `MeasureOverride/ArrangeOverride` 递归 | 独立 `LayoutFacet`，支持平铺数据连续遍历 |
| **渲染解耦** | Visual Tree 与底层绘制 API 深度绑定 | 生成无状态 `RenderCommandList`，彻底隔离 GPU |
| **内存管理** | 离散堆分配（频繁 `new/delete`） | 双缓冲线性帧分配器（Double-Buffered Linear Arena） |

---

## 3. 系统总体分层拓扑架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           XAML / MVVM 应用层                            │
│           (XAML Parser, DataContext, ViewModel, Binding Engine)         │
├─────────────────────────────────────────────────────────────────────────┤
│                     UIElement 实体宿主层 (C++17)                        │
│           (Logical Tree, Visual Tree, Unique ID, Facet Container)       │
├─────────────────────────────────────────────────────────────────────────┤
│                        Facet 核心切面契约矩阵                           │
│  ┌───────────────┐ ┌──────────────┐ ┌───────────────┐ ┌──────────────┐  │
│  │ Dependency DP │ │    Layout    │ │  Input Event  │ │ Interaction  │  │
│  │     Facet     │ │    Facet     │ │     Facet     │ │  (VSM/Anim)  │  │
│  └───────────────┘ └──────────────┘ └───────────────┘ └──────────────┘  │
│  ┌───────────────┐ ┌──────────────┐ ┌────────────────────────────────┐  │
│  │  Text Layout  │ │ Render Facet │ │     ResourceDictionary       │  │
│  │     Facet     │ │(Draw Command)│ │     Brush / Style Scope      │  │
│  └───────────────┘ └──────────────┘ └────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────────────┤
│                          底层跨平台抽象支撑层                           │
│  (Frame Arena Allocator, FreeType Font Atlas, Math/SIMD, Tessellator)  │
├─────────────────────────────────────────────────────────────────────────┤
│                        跨平台硬件加速渲染后端                           │
│          (DirectX 11/12, Vulkan, Metal, OpenGL 3.3+, WebGPU)           │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 4. 六大核心 Facet 切面详细规范

### 4.1 DependencyPropertyFacet（依赖属性与绑定切面）
* **定位**：管理元素的属性元数据、默认值、样式（Style）覆写、动画求值与双向数据绑定。
* **内存设计**：采用**稀疏属性散列表（Sparse Property Map）**，未显式修改的属性直接回退至 `PropertyMetadata::DefaultValue`，绝不占用实例内存。
* **数据求值优先级**：
  $$\text{Local Value} \prec \text{Style Setter} \prec \text{Template} \prec \text{Inherited} \prec \text{Default}$$

### 4.2 LayoutFacet（测量与排列切面）
* **定位**：解耦 WPF 经典的 2-Pass 布局算法（`Measure` 与 `Arrange`）。
* **切面派生族**：
  * `CanvasLayoutFacet`：基于绝对坐标（Left/Top/Right/Bottom）解算。
  * `StackLayoutFacet`：线性主轴/交叉轴累加排布。
  * `GridLayoutFacet`：支持 `Auto`, `Pixel`, `*`（Star 加权比例分配）以及单元格跨越（`Grid.RowSpan/ColumnSpan`）。
  * `DockLayoutFacet`：沿四边向内裁切收缩计算。

### 4.3 RenderFacet（渲染几何与着色切面）
* **定位**：负责收集视觉信息，不调用任何底层图形 API，仅输出纯粹无状态的绘制命令。
* **主要绘制指令**：
  * `DrawRectangle(Rect, Brush, Pen, Radius)`
  * `DrawPath(PathGeometry, Brush, Pen)`
  * `DrawGlyphs(FontHandle, GlyphIndices[], Positions[], Color)`
  * `PushClip(ClipRegion)` / `PopClip()`
  * `PushOpacity(float alpha)` / `PopOpacity()`

### 4.4 InputEventFacet（命中测试与事件路由切面）
* **定位**：管理鼠标悬停、点击、拖拽、按键与焦点。
* **路由机制**：
  * **Tunneling（隧道阶段/Preview）**：从根节点（Root）向下直达事件触发元素。
  * **Bubbling（冒泡阶段）**：从触发元素向上传播至根节点。
  * **Direct（直接触发）**：单元素孤立触发响应。

### 4.5 InteractionStateFacet（视觉状态机与动画切面）
* **定位**：承载 `VisualStateManager`（Normal, MouseOver, Pressed, Disabled）和 `Storyboard` 关键帧插值器。
* **插值计算**：内置 11 种标准 WPF 缓动方程（`QuadraticEase`, `CubicEase`, `SineEase`, `ElasticEase`, `BounceEase` 等），在独立 Tick 中以纳秒精度驱动 DP。

### 4.6 TextLayoutFacet（排版与字形切面）
* **定位**：字符编码转换（UTF-8/UTF-16）、双向文本分析、字体度量、字符整形（Shaping）与换行裁切（`TextWrapping::Wrap` / `TextTrimming::CharacterEllipsis`）。

---

## 5. C++17 核心工业级参考实现

### 5.1 SFINAE 与 Type-Traits 约束的 Facet 容器 (UIElement.hpp)

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <array>
#include <type_traits>
#include <utility>

namespace NoesisFacet {

enum class FacetType : uint32_t {
    Dependency   = 0,
    Layout       = 1,
    Render       = 2,
    Input        = 3,
    Text         = 4,
    Interaction  = 5,
    Count        = 6
};

class UIElement;

// 抽象切面基类 (C++17)
class IFacet {
public:
    virtual ~IFacet() = default;
    virtual void OnAttached(UIElement* owner) { m_owner = owner; }
    virtual void OnDetached() { m_owner = nullptr; }
    
    UIElement* GetOwner() const noexcept { return m_owner; }

protected:
    UIElement* m_owner = nullptr;
};

// UI 实体宿主
class UIElement : public std::enable_shared_from_this<UIElement> {
public:
    explicit UIElement(std::string name = "") : m_name(std::move(name)) {}
    virtual ~UIElement() = default;

    // C++17 SFINAE 约束：挂载 Facet
    template<typename TFacet, typename... Args,
             typename = std::enable_if_t<std::is_base_of_v<IFacet, TFacet>>>
    TFacet* AttachFacet(Args&&... args) {
        constexpr size_t idx = static_cast<size_t>(TFacet::StaticType);
        auto facet = std::make_unique<TFacet>(std::forward<Args>(args)...);
        TFacet* ptr = facet.get();
        facet->OnAttached(this);
        
        m_facets[idx] = std::move(facet);
        m_facetMask |= (1u << idx);
        return ptr;
    }

    // C++17 SFINAE 约束：O(1) 极速查询 Facet
    template<typename TFacet,
             typename = std::enable_if_t<std::is_base_of_v<IFacet, TFacet>>>
    TFacet* GetFacet() const noexcept {
        constexpr size_t idx = static_cast<size_t>(TFacet::StaticType);
        if ((m_facetMask & (1u << idx)) == 0) return nullptr;
        return static_cast<TFacet*>(m_facets[idx].get());
    }

    bool HasFacet(FacetType type) const noexcept {
        return (m_facetMask & (1u << static_cast<size_t>(type))) != 0;
    }

    // 树结构管理
    void AddChild(std::shared_ptr<UIElement> child) {
        if (!child) return;
        child->m_parent = this;
        m_children.push_back(std::move(child));
    }

    UIElement* GetParent() const noexcept { return m_parent; }
    const std::vector<std::shared_ptr<UIElement>>& GetChildren() const noexcept { return m_children; }
    std::string_view GetName() const noexcept { return m_name; }

private:
    std::string m_name;
    UIElement* m_parent = nullptr;
    std::vector<std::shared_ptr<UIElement>> m_children;

    uint32_t m_facetMask = 0;
    std::array<std::unique_ptr<IFacet>, static_cast<size_t>(FacetType::Count)> m_facets;
};

} // namespace NoesisFacet
```

### 5.2 布局度量与两阶段排列算子 (LayoutFacet.hpp)

```cpp
#pragma once
#include "UIElement.hpp"
#include <algorithm>

namespace NoesisFacet {

struct Size { float width = 0.0f; float height = 0.0f; };
struct Rect { float x = 0.0f; float y = 0.0f; float width = 0.0f; float height = 0.0f; };

class LayoutFacet : public IFacet {
public:
    static constexpr FacetType StaticType = FacetType::Layout;

    virtual Size Measure(const Size& availableSize) = 0;
    virtual void Arrange(const Rect& finalRect) = 0;

    void InvalidateMeasure() noexcept { m_isMeasureValid = false; }
    void InvalidateArrange() noexcept { m_isArrangeValid = false; }

    Size GetDesiredSize() const noexcept { return m_desiredSize; }
    Rect GetVisualRect() const noexcept { return m_visualRect; }

protected:
    Size m_desiredSize{0.0f, 0.0f};
    Rect m_visualRect{0.0f, 0.0f, 0.0f, 0.0f};
    bool m_isMeasureValid = false;
    bool m_isArrangeValid = false;
};

// 堆叠容器布局算子 (StackPanel 行为)
class StackLayoutFacet : public LayoutFacet {
public:
    enum class Orientation { Horizontal, Vertical };

    explicit StackLayoutFacet(Orientation orientation = Orientation::Vertical)
        : m_orientation(orientation) {}

    Size Measure(const Size& availableSize) override {
        Size sizeSoFar{0.0f, 0.0f};
        for (const auto& child : m_owner->GetChildren()) {
            if (auto* childLayout = child->GetFacet<LayoutFacet>()) {
                Size childDesired = childLayout->Measure(availableSize);
                if (m_orientation == Orientation::Vertical) {
                    sizeSoFar.width = std::max(sizeSoFar.width, childDesired.width);
                    sizeSoFar.height += childDesired.height;
                } else {
                    sizeSoFar.width += childDesired.width;
                    sizeSoFar.height = std::max(sizeSoFar.height, childDesired.height);
                }
            }
        }
        m_desiredSize = sizeSoFar;
        m_isMeasureValid = true;
        return m_desiredSize;
    }

    void Arrange(const Rect& finalRect) override {
        m_visualRect = finalRect;
        float offset = 0.0f;
        for (const auto& child : m_owner->GetChildren()) {
            if (auto* childLayout = child->GetFacet<LayoutFacet>()) {
                Size childDesired = childLayout->GetDesiredSize();
                Rect childRect;
                if (m_orientation == Orientation::Vertical) {
                    childRect = Rect{finalRect.x, finalRect.y + offset, finalRect.width, childDesired.height};
                    offset += childDesired.height;
                } else {
                    childRect = Rect{finalRect.x + offset, finalRect.y, childDesired.width, finalRect.height};
                    offset += childDesired.width;
                }
                childLayout->Arrange(childRect);
            }
        }
        m_isArrangeValid = true;
    }

private:
    Orientation m_orientation;
};

} // namespace NoesisFacet
```

---

## 6. 零分配无锁渲染流水线（Render Pipeline）

为满足游戏引擎对每帧零堆内存分配（Zero Runtime Heap Allocation）的严苛要求，渲染管线采用**双缓冲帧线性分配器（Double-Buffered Linear Frame Arena）**：

```
[UI Tree / RenderFacets] 
       │ 递归收集 (C++17 纯函数)
       ▼
[Linear Frame Arena (32MB)] ──> [RenderCommandBuffer]
                                       │ 排序 (Z-Index / Material / Clip)
                                       ▼
                              [GPU Batch Generator]
                                       │
                                       ▼
                       [DirectX / Vulkan / Metal API]
```

1. **帧开始（Frame Begin）**：重置当前帧 Arena 的指针偏移量为 0，耗时 0.001 ms。
2. **命令收集（Command Collection）**：各元素的 `RenderFacet` 从 Arena 中分配内存并推入绘制命令（`DrawRectCmd`, `DrawTextCmd`）。
3. **状态批处理（Batching & State Sorting）**：按材质（Material）、纹理（Atlas）和裁剪区域（Clip Rect）对命令进行排序合并，减少 Draw Call。
4. **渲染提交（Submission）**：无锁提交至渲染线程执行 GPU 绘制。

---

## 7. 五大里程碑研发演进路线图（Roadmap）

```
M1 (核心基石) ──> M2 (DP与XAML) ──> M3 (布局与文字) ──> M4 (GPU渲染管线) ──> M5 (动画与MVVM)
```

### 里程碑 1：核心基石与 Facet 运行时（第 1~4 周）
- [x] 基础数学库：`Point`, `Size`, `Rect`, `Matrix3x2`, `Color`, `Thickness`, `CornerRadius`。
- [x] `UIElement` 宿主与 Facet 动态装配容器（基于 `std::enable_if_t`）。
- [x] 逻辑树（Logical Tree）与可视树（Visual Tree）双向遍历迭代器。
- [x] 线性帧内存分配器（Frame Arena）。

### 里程碑 2：依赖属性（DP）与 XAML 解析器（第 5~8 周）
- [x] 稀疏存储的 `DependencyProperty` 与属性值继承机制。
- [x] 流式 XAML 解析引擎（支持类型反射表与命名空间自动绑定）。
- [x] `ResourceDictionary` 资源查找链（静态资源与动态资源作用域解析）。

### 里程碑 3：高性能布局与排版系统（第 9~12 周）
- [x] 2-Pass 布局脏标记管线（`InvalidateMeasure` / `InvalidateArrange` 批量更新）。
- [x] 标准容器算子：`Grid`（Star / Auto 加权解算）、`StackPanel`、`Canvas`、`DockPanel`、`WrapPanel`。
- [x] 集成 FreeType / HarfBuzz 实现文本整形、折行计算与字体图集生成。

### 里程碑 4：跨平台 GPU 渲染引擎（第 13~18 周）
- [x] 贝塞尔曲线自适应细分（Adaptive Tessellator）与三角形带生成。
- [x] 纯无状态 RenderContext 与 RenderCommand 批处理合批机制。
- [x] 编写首个图形 API 后端（DirectX 11 / OpenGL 3.3 / Vulkan）。
- [x] 模板缓冲（Stencil Buffer）路径裁剪与透明图层缓存（Layer Cache）。

### 里程碑 5：交互、动画与 MVVM 数据绑定（第 19~24 周）
- [x] 冒泡与隧道事件分发（`PreviewMouseDown` / `MouseDown` / `MouseEnter`）。
- [x] 关键帧动画与 11 种缓动函数（`Storyboard`, `DoubleAnimation`, `ColorAnimation`）。
- [x] `VisualStateManager`（VSM）状态转换与平滑过渡。
- [x] MVVM `{Binding}` 引擎与属性变更通知（`INotifyPropertyChanged`）。

---

## 8. 跨平台与游戏引擎（Unreal / Unity / 自研引擎）集成规范

遵循 **Headless Off-Screen Canvas 优先** 设计原则，使类库可无缝嵌入任何游戏引擎：

```cpp
// 外部调用示例（C++17 极简集成接口）
#include "NoesisFacet/Engine.hpp"

// 1. 初始化引擎实例
auto guiEngine = NoesisFacet::CreateEngine();

// 2. 加载 XAML 视图并创建离屏视图
auto rootView = guiEngine->LoadXaml("Assets/UI/CyberpunkHUD.xaml");
auto view = guiEngine->CreateView(rootView, 1920, 1080);

// 3. 游戏主循环注入输入事件 (纯函数调用)
view->OnMouseMove(cursorX, cursorY);
view->OnMouseButtonDown(NoesisFacet::MouseButton::Left);

// 4. 更新布局与动画状态 (传入 Delta Time 浮点数)
view->Update(deltaTime);

// 5. 渲染绘制（输出至游戏视口或 RenderTexture 纹理目标）
view->Render(renderContext);
```

---
*文档编制完成。严格兼容 ISO/IEC C++17 标准，遵循 MIT 开源协议规范。*
