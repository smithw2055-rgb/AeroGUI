#include "gui/core/Describe.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/Selectors.hpp>
#include <Aero/Controls/TreeView.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include "gui/media/BrushRendering.hpp"
#include <Aero/Documents.hpp>
#include "RichText.hpp"

#include "TextBlockLayout.hpp"

#if defined(_WIN32)
extern "C" unsigned short RtlCaptureStackBackTrace(unsigned long, unsigned long, void**, unsigned long*);
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>


namespace Aero::Controls {
using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Render;

void Panel::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    auto& builder = Aero::Render::DrawingBridge::Builder(context);
    PaintBrushRect(
        builder,
        GetBackground(),
        Rect{
            0.0, 0.0,
            GetRenderSize().width,
            GetRenderSize().height});
}
StackPanel::StackPanel() noexcept : StackPanel(Orientation::Vertical) {}

StackPanel::StackPanel(Orientation orientation) noexcept
    : StackPanel(StaticTypeId(), orientation) {}

StackPanel::StackPanel(TypeId runtimeType, Orientation orientation) noexcept
    : Panel(runtimeType) {
    if (orientation != Orientation::Vertical) {
        SetOrientation(orientation);
    }
}
Orientation StackPanel::GetOrientation() const noexcept {
    return GetValue(OrientationProperty);
}
void StackPanel::SetOrientation(Orientation value) noexcept {
    SetValue(OrientationProperty, value);
}
Size StackPanel::MeasureOverride(Size availableSize) noexcept {
    Size desired;
    const Orientation orientation = GetOrientation();
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Size childAvailable = availableSize;
        if (orientation == Orientation::Vertical) childAvailable.height = 1.0e12;
        else childAvailable.width = 1.0e12;
        MeasureChild(*child, childAvailable);
        const Size childDesired = child->GetDesiredSize();
        if (orientation == Orientation::Vertical) {
            desired.width = std::max(desired.width, childDesired.width);
            desired.height += childDesired.height;
        } else {
            desired.width += childDesired.width;
            desired.height = std::max(desired.height, childDesired.height);
        }
    }
    return desired;
}
Size StackPanel::ArrangeOverride(Size finalSize) noexcept {
    double offset = 0.0;
    const Orientation orientation = GetOrientation();
    const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size desired = child->GetDesiredSize();
        const Rect slot = orientation == Orientation::Vertical
            ? Rect{0.0, offset, finalSize.width, desired.height}
            : (isRtl
                ? Rect{finalSize.width - offset - desired.width, 0.0, desired.width, finalSize.height}
                : Rect{offset, 0.0, desired.width, finalSize.height});
        ArrangeChild(*child, slot);
        offset += orientation == Orientation::Vertical
            ? desired.height : desired.width;
    }
    return finalSize;
}
bool DockPanel::GetLastChildFill() const noexcept {
    return GetValue(LastChildFillProperty);
}
void DockPanel::SetLastChildFill(
    bool value) noexcept {
    SetValue(LastChildFillProperty, value);
}
void DockPanel::SetChildDock(
    UIElement& child,
    Dock value) noexcept {
    bool attached = false;
    for (UIElement* current : LayoutChildren()) {
        attached = attached || current == &child;
    }
    if (!attached) {
        return;
    }
    child.SetValue(DockProperty, value);
}
Dock DockPanel::GetChildDock(
    const UIElement& child) const noexcept {
    return child.GetValue(DockProperty);
}
Size DockPanel::MeasureOverride(
    Size availableSize) noexcept {
    Size desired;
    float consumedWidth = 0.0f;
    float consumedHeight = 0.0f;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size remaining{
            std::max(0.0f, availableSize.width - consumedWidth),
            std::max(0.0f, availableSize.height - consumedHeight)};
        MeasureChild(*child, remaining);
        const Size childDesired = child->GetDesiredSize();
        const Dock dock = GetChildDock(*child);
        if (dock == Dock::Left || dock == Dock::Right) {
            consumedWidth += childDesired.width;
            desired.width = std::max(
                desired.width,
                consumedWidth);
            desired.height = std::max(
                desired.height,
                consumedHeight + childDesired.height);
        } else {
            consumedHeight += childDesired.height;
            desired.height = std::max(
                desired.height,
                consumedHeight);
            desired.width = std::max(
                desired.width,
                consumedWidth + childDesired.width);
        }
    }
    return desired;
}
Size DockPanel::ArrangeOverride(
    Size finalSize) noexcept {
    float left = 0.0f;
    float top = 0.0f;
    float right = finalSize.width;
    float bottom = finalSize.height;
    const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
    const UIElementChildRange children = LayoutChildren();
    for (std::uint32_t index = 0U;
         index < children.Size();
         ++index) {
        UIElement* child = children[index];
        if (child == nullptr) continue;
        Rect slot{
            left,
            top,
            std::max(0.0f, right - left),
            std::max(0.0f, bottom - top)};
        const bool fill =
            GetLastChildFill() &&
            index + 1U == children.Size();
        if (!fill) {
            const Size desired = child->GetDesiredSize();
            Dock dock = GetChildDock(*child);
            if (isRtl) {
                if (dock == Dock::Left) dock = Dock::Right;
                else if (dock == Dock::Right) dock = Dock::Left;
            }
            switch (dock) {
            case Dock::Left:
                slot.width = std::min(
                    slot.width, desired.width);
                left += slot.width;
                break;
            case Dock::Top:
                slot.height = std::min(
                    slot.height, desired.height);
                top += slot.height;
                break;
            case Dock::Right:
                slot.width = std::min(
                    slot.width, desired.width);
                slot.x = right - slot.width;
                right -= slot.width;
                break;
            case Dock::Bottom:
                slot.height = std::min(
                    slot.height, desired.height);
                slot.y = bottom - slot.height;
                bottom -= slot.height;
                break;
            }
        }
        ArrangeChild(*child, slot);
    }
    return finalSize;
}
Orientation WrapPanel::GetOrientation() const noexcept {
    return GetValue(OrientationProperty);
}
void WrapPanel::SetOrientation(
    Orientation value) noexcept {
    SetValue(OrientationProperty, value);
}
double WrapPanel::GetItemWidth() const noexcept {
    return GetValue(ItemWidthProperty);
}
double WrapPanel::GetItemHeight() const noexcept {
    return GetValue(ItemHeightProperty);
}
void WrapPanel::SetItemWidth(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) {
        return;
    }
    SetValue(ItemWidthProperty, value);
}
void WrapPanel::SetItemHeight(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) {
        return;
    }
    SetValue(ItemHeightProperty, value);
}
Size WrapPanel::MeasureOverride(
    Size availableSize) noexcept {
    const bool horizontal =
        GetOrientation() == Orientation::Horizontal;
    const double primaryLimit = horizontal
        ? availableSize.width : availableSize.height;
    const bool constrained =
        std::isfinite(primaryLimit) &&
        primaryLimit < 1.0e11;
    double linePrimary = 0.0;
    double lineCross = 0.0;
    double desiredPrimary = 0.0;
    double desiredCross = 0.0;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size childAvailable{
            GetItemWidth() > 0.0
                ? GetItemWidth() : availableSize.width,
            GetItemHeight() > 0.0
                ? GetItemHeight() : availableSize.height};
        MeasureChild(*child, childAvailable);
        const Size desired = child->GetDesiredSize();
        const double childPrimary = horizontal
            ? (GetItemWidth() > 0.0
                ? GetItemWidth() : desired.width)
            : (GetItemHeight() > 0.0
                ? GetItemHeight() : desired.height);
        const double childCross = horizontal
            ? (GetItemHeight() > 0.0
                ? GetItemHeight() : desired.height)
            : (GetItemWidth() > 0.0
                ? GetItemWidth() : desired.width);
        if (constrained && linePrimary > 0.0 &&
            linePrimary + childPrimary > primaryLimit) {
            desiredPrimary = std::max(
                desiredPrimary, linePrimary);
            desiredCross += lineCross;
            linePrimary = 0.0;
            lineCross = 0.0;
        }
        linePrimary += childPrimary;
        lineCross = std::max(lineCross, childCross);
    }
    desiredPrimary = std::max(
        desiredPrimary, linePrimary);
    desiredCross += lineCross;
    return horizontal
        ? Size{desiredPrimary, desiredCross}
        : Size{desiredCross, desiredPrimary};
}
Size WrapPanel::ArrangeOverride(
    Size finalSize) noexcept {
    const bool horizontal =
        GetOrientation() == Orientation::Horizontal;
    const bool isRtl = horizontal && GetFlowDirection() == FlowDirection::RightToLeft;
    const double primaryLimit = horizontal
        ? finalSize.width : finalSize.height;
    double primary = 0.0;
    double cross = 0.0;
    double lineCross = 0.0;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size desired = child->GetDesiredSize();
        const double childPrimary = horizontal
            ? (GetItemWidth() > 0.0
                ? GetItemWidth() : desired.width)
            : (GetItemHeight() > 0.0
                ? GetItemHeight() : desired.height);
        const double childCross = horizontal
            ? (GetItemHeight() > 0.0
                ? GetItemHeight() : desired.height)
            : (GetItemWidth() > 0.0
                ? GetItemWidth() : desired.width);
        if (primary > 0.0 &&
            primary + childPrimary > primaryLimit) {
            primary = 0.0;
            cross += lineCross;
            lineCross = 0.0;
        }
        const Rect slot = horizontal
            ? (isRtl
                ? Rect{finalSize.width - primary - childPrimary, cross, childPrimary, childCross}
                : Rect{primary, cross, childPrimary, childCross})
            : Rect{cross, primary, childCross, childPrimary};
        ArrangeChild(*child, slot);
        primary += childPrimary;
        lineCross = std::max(lineCross, childCross);
    }
    return finalSize;
}
std::uint32_t UniformGrid::GetRows() const noexcept {
    return GetValue(RowsProperty);
}
std::uint32_t UniformGrid::GetColumns() const noexcept {
    return GetValue(ColumnsProperty);
}
std::uint32_t UniformGrid::GetFirstColumn() const noexcept {
    return GetValue(FirstColumnProperty);
}
void UniformGrid::SetRows(
    std::uint32_t value) noexcept {
    SetValue(RowsProperty, value);
}
void UniformGrid::SetColumns(
    std::uint32_t value) noexcept {
    SetValue(ColumnsProperty, value);
}
void UniformGrid::SetFirstColumn(
    std::uint32_t value) noexcept {
    SetValue(FirstColumnProperty, value);
}
void UniformGrid::ResolveDimensions(
    std::uint32_t childCount,
    std::uint32_t& rows,
    std::uint32_t& columns) const noexcept {
    rows = GetRows();
    columns = GetColumns();
    if (childCount == 0U) {
        rows = rows == 0U ? 1U : rows;
        columns = columns == 0U ? 1U : columns;
        return;
    }
    if (rows == 0U && columns == 0U) {
        columns = static_cast<std::uint32_t>(
            std::ceil(std::sqrt(
                static_cast<double>(childCount))));
        rows = (childCount + columns - 1U) / columns;
    } else if (rows == 0U) {
        rows = (childCount + columns - 1U) / columns;
    } else if (columns == 0U) {
        columns = (childCount + rows - 1U) / rows;
    }
}
Size UniformGrid::MeasureOverride(
    Size availableSize) noexcept {
    std::uint32_t count = 0U;
    for (UIElement* child : LayoutChildren()) {
        if (child != nullptr) ++count;
    }
    std::uint32_t rows = 0U;
    std::uint32_t columns = 0U;
    ResolveDimensions(count + GetFirstColumn(), rows, columns);
    constexpr double Unconstrained = 1.0e12;
    const bool widthUnconstrained =
        availableSize.width >= Unconstrained * 0.5;
    const bool heightUnconstrained =
        availableSize.height >= Unconstrained * 0.5;
    Size cellAvailable{
        availableSize.width / static_cast<double>(columns),
        availableSize.height / static_cast<double>(rows)};
    // ScrollViewer gives children infinite height. Slot templates bind
    // Height to ActualWidth, so the first measure reports 0 height and a
    // VerticalAlignment=Top grid collapses to nothing. Use the finite
    // axis as the unconstrained cell size so square cells appear on the
    // first pass.
    if (heightUnconstrained && !widthUnconstrained &&
        cellAvailable.width > 0.0) {
        cellAvailable.height = cellAvailable.width;
    } else if (widthUnconstrained && !heightUnconstrained &&
               cellAvailable.height > 0.0) {
        cellAvailable.width = cellAvailable.height;
    }
    Size cellDesired;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        MeasureChild(*child, cellAvailable);
        cellDesired.width = std::max(
            cellDesired.width,
            child->GetDesiredSize().width);
        cellDesired.height = std::max(
            cellDesired.height,
            child->GetDesiredSize().height);
    }
    if (heightUnconstrained && !widthUnconstrained &&
        cellAvailable.width > 0.0 &&
        cellAvailable.width < Unconstrained * 0.5) {
        if (cellDesired.height < cellAvailable.width) {
            cellDesired.height = cellAvailable.width;
        }
        if (cellDesired.width < cellAvailable.width) {
            cellDesired.width = cellAvailable.width;
        }
    } else if (widthUnconstrained && !heightUnconstrained &&
               cellAvailable.height > 0.0 &&
               cellAvailable.height < Unconstrained * 0.5) {
        if (cellDesired.width < cellAvailable.height) {
            cellDesired.width = cellAvailable.height;
        }
        if (cellDesired.height < cellAvailable.height) {
            cellDesired.height = cellAvailable.height;
        }
    } else {
        if (cellDesired.height <= 0.0 && cellAvailable.width > 0.0 &&
            cellAvailable.width < Unconstrained * 0.5) {
            cellDesired.height = cellAvailable.width;
        }
        if (cellDesired.width <= 0.0 && cellAvailable.width > 0.0 &&
            cellAvailable.width < Unconstrained * 0.5) {
            cellDesired.width = cellAvailable.width;
        } else if (cellDesired.width <= 0.0 && cellDesired.height > 0.0 &&
                   widthUnconstrained) {
            cellDesired.width = cellDesired.height;
        }
    }
    return Size{
        cellDesired.width * columns,
        cellDesired.height * rows};
}
Size UniformGrid::ArrangeOverride(
    Size finalSize) noexcept {
    std::uint32_t count = 0U;
    for (UIElement* child : LayoutChildren()) {
        if (child != nullptr) ++count;
    }
    std::uint32_t rows = 0U;
    std::uint32_t columns = 0U;
    ResolveDimensions(count + GetFirstColumn(), rows, columns);
    const double width =
        finalSize.width / static_cast<double>(columns);
    const double height =
        finalSize.height / static_cast<double>(rows);
    const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
    std::uint32_t index = std::min(
        GetFirstColumn(), columns - 1U);
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const std::uint32_t row = index / columns;
        const std::uint32_t column = index % columns;
        const double x = isRtl
            ? finalSize.width - (column + 1U) * width
            : column * width;
        ArrangeChild(
            *child,
            {x, row * height, width, height});
        ++index;
    }
    return finalSize;
}
Canvas::Canvas() noexcept : Panel(StaticTypeId()) {}

void Canvas::SetChildPosition(
    UIElement& child, Point position) noexcept {
    if (!VerifyAccess()) return;
    if (!IsFinite(position)) {
        return;
    }
    bool attached = false;
    for (UIElement* current : LayoutChildren()) attached = attached || current == &child;
    if (!attached) {
        return;
    }
    child.SetValue(LeftProperty, position.x);
    child.SetValue(TopProperty, position.y);
}
Point Canvas::GetChildPosition(const UIElement& child) const noexcept {
    const double left = child.GetValue(LeftProperty);
    const double top = child.GetValue(TopProperty);
    return {
        std::isfinite(left) ? left : 0.0,
        std::isfinite(top) ? top : 0.0};
}
Size Canvas::MeasureOverride(Size) noexcept {
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        MeasureChild(*child, {1.0e12, 1.0e12});
    }
    return Size{};
}
Size Canvas::ArrangeOverride(Size finalSize) noexcept {
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size desired = child->GetDesiredSize();
        const double left = child->GetValue(LeftProperty);
        const double top = child->GetValue(TopProperty);
        const double right = child->GetValue(RightProperty);
        const double bottom = child->GetValue(BottomProperty);
        Point position{0.0f, 0.0f};
        if (std::isfinite(left)) {
            position.x = static_cast<float>(left);
        } else if (std::isfinite(right)) {
            position.x = static_cast<float>(finalSize.width - right - desired.width);
        }
        if (std::isfinite(top)) {
            position.y = static_cast<float>(top);
        } else if (std::isfinite(bottom)) {
            position.y = static_cast<float>(finalSize.height - bottom - desired.height);
        }
        ArrangeChild(*child,
            {position.x, position.y, desired.width, desired.height});
    }
    return finalSize;
}
std::uint32_t UIElementCollection::GetCount() const noexcept {
    return owner_ != nullptr ? owner_->ChildCountCore() : 0U;
}
UIElement* UIElementCollection::GetItem(std::uint32_t index) const noexcept {
    if (owner_ == nullptr) return nullptr;
    Base::Ref<Base::Object> child = owner_->ChildAtCore(index);
    return child ? static_cast<UIElement*>(child.Get()) : nullptr;
}

// Live items-source view over a Panel's children (see Panel::GetChildrenObject).
// Heap-allocated so bindings and ItemsControls can hold a Ref; owned by the
// Panel through Panel::childrenView_ and detached in ~Panel.
class PanelChildrenView : public Collections::ObservableCollectionBase {
public:
    explicit PanelChildrenView(Panel& owner) noexcept : owner_(&owner) {}
    void DetachOwner() noexcept { owner_ = nullptr; }
    std::uint32_t GetCount() const noexcept override {
        return owner_ != nullptr ? owner_->ChildCountCore() : 0U;
    }
    Ref<Base::Object> GetItem(std::uint32_t index) const noexcept override {
        if (owner_ == nullptr) return {};
        return owner_->ChildAtCore(index);
    }
    void AddItemsChanged(const Collections::ItemsChangedHandler& handler) noexcept override {
        changed_.Add(handler);
    }
    bool RemoveItemsChanged(const Collections::ItemsChangedHandler& handler) noexcept override {
        return changed_.Remove(handler);
    }
    void NotifyReset() noexcept {
        if (changed_.Empty()) return;
        changed_.Invoke(Collections::ItemsChangedEvent{
            Collections::ItemsChangeAction::Reset,
            UINT32_MAX,
            UINT32_MAX,
            0U,
            0U});
    }

private:
    Panel* owner_ = nullptr;
    Collections::ItemsChangedHandler changed_;
};

Panel::~Panel() {
    zOrderCache_.Clear();
    for (std::uint32_t index = 0U; index < ownedChildren_.Size(); ++index) {
        UIElement* child = ownedChildren_[index]
            ? static_cast<UIElement*>(ownedChildren_[index].Get())
            : nullptr;
        if (child != nullptr && child->GetVisualParent() == this) {
            RemoveVisualChild(child);
        }
    }
    if (childrenView_) {
        static_cast<PanelChildrenView*>(childrenView_.Get())->DetachOwner();
        childrenView_.Reset();
    }
}

void Panel::NotifyChildrenViewReset() noexcept {
    if (!childrenView_) return;
    static_cast<PanelChildrenView*>(childrenView_.Get())->NotifyReset();
}
void UIElementCollection::Add(Base::Ref<UIElement> child) noexcept {
    if (owner_ == nullptr || !child) {
        return;
    }
    Base::Ref<Base::Object> object(child);
    UIElement& element = *child;
    owner_->AddChildCore(object, element);
    // AttachVisual already calls PanelAddChild. Nested AttachElement here
    // double-mounts layout/render and drops ControlTemplates.
    ElementTree* tree = VisualTree(owner_);
    if (tree != nullptr) {
        if (VisualTree(element) == nullptr &&
            element.GetLogicalParent() == nullptr) {
            Base::Result<ElementAttachment> attached =
                tree->AttachElement(*owner_, element);
            if (!attached) {
                (void)owner_->RemoveChildCore(element);
                return;
            }
        } else if (
            element.GetVisualParent() != owner_ ||
            !element.GetIsLayoutAttached()) {
            if (element.GetVisualParent() != nullptr &&
                element.GetVisualParent() != owner_) {
                return;
            }
            Base::Result<VisualAttachment> attached =
                tree->AttachVisualChild(*owner_, element);
            if (!attached) {
                (void)owner_->RemoveChildCore(element);
                return;
            }
        }
    }
}
void UIElementCollection::Remove(UIElement& child) noexcept {
    if (owner_ == nullptr) {
        return;
    }
    (void)owner_->RemoveChildCore(child);
}
void UIElementCollection::Clear() noexcept {
    if (owner_ == nullptr) {
        return;
    }
    owner_->ClearChildrenCore();
}
std::uint32_t Panel::GetVisualChildrenCount() const noexcept {
    EnsureZOrderCache();
    return zOrderCache_.Size();
}

namespace {
struct PanelZOrder {
    std::int32_t z = 0;
    std::uint32_t document = 0U;
    UIElement* child = nullptr;
};
} // namespace

void Panel::EnsureZOrderCache() const noexcept {
    if (!zOrderDirty_) {
        return;
    }
    zOrderCache_.Clear();
    bool hasNonZeroZ = false;
    std::uint32_t document = 0U;
    Base::Vector<PanelZOrder> ordered;
    for (std::uint32_t childIndex = 0U; childIndex < ownedChildren_.Size(); ++childIndex) {
        UIElement* child = ownedChildren_[childIndex]
            ? static_cast<UIElement*>(ownedChildren_[childIndex].Get())
            : nullptr;
        if (child == nullptr || child->GetVisualParent() != this) continue;
        PanelZOrder record;
        record.z = child->GetValue(ZIndexProperty);
        if (record.z != 0) hasNonZeroZ = true;
        record.document = document++;
        record.child = child;
        ordered.PushBack(record);
    }
    if (hasNonZeroZ) {
        std::sort(
            ordered.Data(),
            ordered.Data() + ordered.Size(),
            [](const PanelZOrder& left, const PanelZOrder& right) noexcept {
                if (left.z != right.z) return left.z < right.z;
                return left.document < right.document;
            });
    }
    zOrderCache_.Reserve(ordered.Size());
    for (std::uint32_t i = 0U; i < ordered.Size(); ++i) {
        zOrderCache_.PushBack(ordered[i].child);
    }
    zOrderDirty_ = false;
}

::Aero::Media::Visual* Panel::GetVisualChild(std::uint32_t index) const noexcept {
    EnsureZOrderCache();
    return index < zOrderCache_.Size() ? zOrderCache_[index] : nullptr;
}

std::uint32_t Panel::GetLayoutChildrenCount() const noexcept {
    return GetVisualChildrenCount();
}

UIElement* Panel::GetLayoutChild(std::uint32_t index) const noexcept {
    EnsureZOrderCache();
    if (zOrderCache_.Size() == ownedChildren_.Size()) {
        if (index < ownedChildren_.Size()) {
            return static_cast<UIElement*>(ownedChildren_[index].Get());
        }
        return nullptr;
    }
    std::uint32_t current = 0U;
    for (std::uint32_t childIndex = 0U; childIndex < ownedChildren_.Size();
         ++childIndex) {
        UIElement* child = ownedChildren_[childIndex]
            ? static_cast<UIElement*>(ownedChildren_[childIndex].Get())
            : nullptr;
        if (child == nullptr || child->GetVisualParent() != this) continue;
        if (current == index) return child;
        ++current;
    }
    return nullptr;
}

void Panel::AddChildCore(const Base::Ref<Base::Object>& childObject, UIElement& child) noexcept {
    if (!childObject || childObject.Get() != &child) {
        return;
    }
    if (!VerifyAccess()) return;
    for (const Base::Ref<Base::Object>& owned : ownedChildren_) {
        if (owned.Get() == &child) {
            return;
        }
    }
    ownedChildren_.PushBack(childObject);
    if (child.GetVisualParent() != this) {
        if (ElementTree* tree = VisualTree(this)) {
            // XAML Panel content uses AddChildCore, not UIElementCollection::Add.
            // A live-tree Grid (LoadComponent ColorSelector) must still parent
            // children visually or star rows measure 0 and ColorRect stays 0x0.
            if (VisualTree(child) == nullptr &&
                child.GetLogicalParent() == nullptr) {
                Base::Result<ElementAttachment> attached =
                    tree->AttachElement(*this, child);
                if (!attached) {
                    ownedChildren_.PopBack();
                    return;
                }
            } else if (
                child.GetVisualParent() != this ||
                !child.GetIsLayoutAttached()) {
                if (child.GetVisualParent() != nullptr &&
                    child.GetVisualParent() != this) {
                    InvalidateMeasure();
                    return;
                }
                Base::Result<VisualAttachment> attached =
                    tree->AttachVisualChild(*this, child);
                if (!attached) {
                    ownedChildren_.PopBack();
                    return;
                }
            }
        } else {
            AddVisualChild(&child);
        }
    }
    zOrderDirty_ = true;
    NotifyChildrenViewReset();
    InvalidateMeasure();
}
bool Panel::RemoveChildCore(UIElement& child) noexcept {
    if (!VerifyAccess()) return false;
    for (std::uint32_t index = 0U; index < ownedChildren_.Size(); ++index) {
        if (ownedChildren_[index].Get() != &child) continue;
        // Remove from storage BEFORE detach: DetachVisual/DetachLogical can
        // re-enter (Unloaded/template cleanup) and mutate ownedChildren_.
        // Detaching first then shifting+PopBack double-pops when re-entered.
        Base::Ref<Base::Object> retained = ownedChildren_[index];
        for (std::uint32_t next = index + 1U; next < ownedChildren_.Size(); ++next) {
            ownedChildren_[next - 1U] = std::move(ownedChildren_[next]);
        }
        if (!ownedChildren_.Empty()) {
            ownedChildren_.PopBack();
        }
        ElementTree* tree = VisualTree(this);
        if (tree != nullptr && VisualTree(child) == tree) {
            if (child.GetVisualParent() == this) {
                static_cast<void>(tree->DetachVisual(*this, child));
            }
            if (child.GetLogicalParent() == this) {
                static_cast<void>(tree->DetachLogical(*this, child));
            }
        } else if (child.GetVisualParent() == this) {
            RemoveVisualChild(&child);
        }
        zOrderDirty_ = true;
        NotifyChildrenViewReset();
        InvalidateMeasure();
        return true;
    }
    return false;
}
void Panel::ClearChildrenCore() noexcept {
    if (!VerifyAccess()) return;
    ElementTree* tree = VisualTree(this);
    for (std::uint32_t index = 0U; index < ownedChildren_.Size(); ++index) {
        UIElement* child = ownedChildren_[index]
            ? static_cast<UIElement*>(ownedChildren_[index].Get())
            : nullptr;
        if (child == nullptr) continue;
        if (tree != nullptr && VisualTree(child) == tree) {
            if (child->GetVisualParent() == this) {
                (void)tree->DetachVisual(*this, *child);
            }
            if (child->GetLogicalParent() == this) {
                (void)tree->DetachLogical(*this, *child);
            }
        } else if (child->GetVisualParent() == this) {
            RemoveVisualChild(child);
        }
    }
    ownedChildren_.Clear();
    zOrderDirty_ = true;
    zOrderCache_.Clear();
    NotifyChildrenViewReset();
    InvalidateMeasure();
}

Ref<Base::Object> Panel::GetChildrenObject() const noexcept {
    if (!childrenView_) {
        Ref<PanelChildrenView> made =
            Base::MakeRef<PanelChildrenView>(const_cast<Panel&>(*this));
        if (!made) return {};
        childrenView_ = Ref<Base::Object>(std::move(made));
    }
    return childrenView_;
}

namespace {

void SetPanelContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }

    (static_cast<Panel&>(owner)).AddChildCore( child, *static_cast<Aero::UIElement*>(child.Get()));
}

void ClearPanelContent(
    Base::Object& owner,
    void*) noexcept {
    (static_cast<Panel&>(owner)).ClearChildrenCore();
}

void OnZIndexPropertyChanged(
    DependencyObject& target,
    const DependencyPropertyChangedEventArgs&) noexcept {
    if (auto* element = ::Aero::TryCast<UIElement>(&target)) {
        if (auto* panel = ::Aero::TryCast<Panel>(element->GetVisualParent())) {
            panel->InvalidateZOrder();
        }
    }
}

} // namespace

AERO_DESCRIBE(Panel) {
    using namespace Aero::Meta;
    Register<Panel>(context, TypeFlags::Abstract)
        .Property(Panel::BackgroundProperty, Base::Ref<Media::Brush>{}, AffectsRender)
        .Property(Panel::ZIndexProperty,
            FrameworkPropertyMetadata(std::int32_t{0}, AffectsParentArrange)
                .Changed(&OnZIndexPropertyChanged))
        .Property(Panel::IsItemsHostProperty, false)
        .Property<Base::Ref<Base::Object>, &Panel::GetChildrenObject, &Panel::SetChildrenObject>("Children", PropertyFlags::Collection | PropertyFlags::Structural)
        .ContentAccessor(MakeMemberId(Panel::StaticTypeId(), MemberKind::Property, "Children"), ContentKind::Collection, &SetPanelContent, &ClearPanelContent, ContentFlags::Visual);
}

AERO_DESCRIBE(StackPanel) {
    using namespace Aero::Meta;
    Register<StackPanel>(context)
        .Property(StackPanel::OrientationProperty, Orientation::Vertical, AffectsMeasure)
        .Factory();
}

AERO_DESCRIBE(DockPanel) {
    using namespace Aero::Meta;
    Register<DockPanel>(context)
        .Property(DockPanel::LastChildFillProperty, true, AffectsArrange)
        .Property(DockPanel::DockProperty, Dock::Left, AffectsParentMeasure)
        .Factory();
}

AERO_DESCRIBE(WrapPanel) {
    using namespace Aero::Meta;
    Register<WrapPanel>(context)
        .Property(WrapPanel::OrientationProperty, Orientation::Horizontal, AffectsMeasure)
        .Property(WrapPanel::ItemWidthProperty, 0.0, AffectsMeasure, &Base::Validate::NonNegative<double>)
        .Property(WrapPanel::ItemHeightProperty, 0.0, AffectsMeasure, &Base::Validate::NonNegative<double>)
        .Factory();
}

AERO_DESCRIBE(UniformGrid) {
    using namespace Aero::Meta;
    Register<UniformGrid>(context)
        .Property(UniformGrid::RowsProperty, std::uint32_t{0}, AffectsMeasure)
        .Property(UniformGrid::ColumnsProperty, std::uint32_t{0}, AffectsMeasure)
        .Property(UniformGrid::FirstColumnProperty, std::uint32_t{0}, AffectsMeasure | AffectsArrange)
        .Factory();
}

AERO_DESCRIBE(Canvas) {
    using namespace Aero::Meta;
    Register<Canvas>(context)
        .Property(Canvas::LeftProperty, std::numeric_limits<double>::infinity(), AffectsParentArrange)
        .Property(Canvas::TopProperty, std::numeric_limits<double>::infinity(), AffectsParentArrange)
        .Property(Canvas::RightProperty, std::numeric_limits<double>::infinity(), AffectsParentArrange)
        .Property(Canvas::BottomProperty, std::numeric_limits<double>::infinity(), AffectsParentArrange)
        .Factory();
}

} // namespace Aero::Controls
