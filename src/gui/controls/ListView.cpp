#include <cstdio>
#include "gui/core/ElementTree.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Style.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/FrameworkElement.hpp>
#include "gui/core/ValueConversion.hpp"

#include <cmath>
#include <utility>
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero::Controls {

Value GridViewColumn::GetHeader()
    const noexcept {
    return GetValue(HeaderProperty);
}

void GridViewColumn::SetHeader(
    Value value) noexcept {
    SetValue(HeaderProperty, std::move(value));
}

void GridViewColumn::SetHeader(
    Base::StringView value) noexcept {
    Base::Result<Value> boxed = Value::TryFromString(
        Meta::TypeOf<Base::String>(), value);
    if (!boxed) { AERO_ASSERT(false); return; }
    SetHeader(std::move(boxed).Value());
}

double GridViewColumn::GetWidth()
    const noexcept {
    return GetValue(WidthProperty);
}

void GridViewColumn::SetWidth(
    double value) noexcept {
    if (!std::isfinite(value) ||
        value < 0.0) {
        return;
    }
    SetValue(WidthProperty, value);
}

Base::Ref<DataTemplate>
GridViewColumn::GetCellTemplate() const noexcept {
    return GetValue(CellTemplateProperty);
}

void
GridViewColumn::SetCellTemplate(
    Base::Ref<DataTemplate> value) noexcept {
    SetValue(
        CellTemplateProperty,
        std::move(value));
}

Base::Ref<DataTemplate>
GridViewColumn::GetHeaderTemplate() const noexcept {
    return GetValue(HeaderTemplateProperty);
}

void
GridViewColumn::SetHeaderTemplate(
    Base::Ref<DataTemplate> value) noexcept {
    SetValue(
        HeaderTemplateProperty,
        std::move(value));
}

Base::StringView
GridViewColumn::GetDisplayMemberPath()
    const noexcept {
    return GetValue(DisplayMemberPathProperty);
}

void
GridViewColumn::SetDisplayMemberPath(
    Base::StringView value) noexcept {
    SetValue(
        DisplayMemberPathProperty, value);
}

Base::Ref<Data::Binding>
GridViewColumn::GetDisplayMemberBinding() const noexcept {
    return GetValue(DisplayMemberBindingProperty);
}

void
GridViewColumn::SetDisplayMemberBinding(
    Base::Ref<Data::Binding> value) noexcept {
    SetValue(
        DisplayMemberBindingProperty, std::move(value));
}

void GridView::AddColumn(
    Base::Ref<GridViewColumn> column)
    noexcept {
    if (!column) { AERO_ASSERT(false); return; }
    columns_.PushBack(
        std::move(column));
}

Base::Ref<ViewBase>
ListView::GetView() const noexcept {
    return GetValue(ViewProperty);
}

void ListView::SetView(
    Base::Ref<ViewBase> value) noexcept {
    SetValue(ViewProperty, std::move(value));
    SynchronizeColumnHeaders();
}

void
ListView::OnApplyTemplate() noexcept {
    ListBox::OnApplyTemplate();
    DependencyObject* headers =
        GetTemplateChild("ColumnHeaders");
    columnHeaders_ =
        headers != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            headers->RuntimeType(),
            TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(headers)
        : nullptr;
    if (columnHeaders_ == nullptr) {
        return;
    }
    SynchronizeColumnHeaders();
}

void ListView::OnTemplateDetached() noexcept {
    columnHeaders_ = nullptr;
    ListBox::OnTemplateDetached();
}

void
ListView::SynchronizeColumnHeaders() noexcept {
    if (columnHeaders_ == nullptr) return;
    Base::String text;
    GridView* view = ::Aero::TryCast<GridView>(GetView().Get());
    if (view != nullptr) {
        for (const Base::Ref<GridViewColumn>&
             column : view->GetColumns()) {
            if (!column) continue;
            const Value header = column->GetHeader();
            if (!text.Append(
                header.Kind() == ValueKind::String
                ? header.AsString()
                : Base::StringView{})) {
                return;
            }
            const std::uint32_t headerCharacters =
                header.Kind() == ValueKind::String
                ? header.AsString().SizeBytes()
                : 0U;
            const std::uint32_t columnCharacters =
                column->GetWidth() > 0.0
                ? static_cast<std::uint32_t>(
                      std::max(
                          1.0,
                          std::floor(
                              column->GetWidth() /
                              8.0)))
                : headerCharacters + 2U;
            const std::uint32_t padding =
                std::max(
                    std::uint32_t{2U},
                    columnCharacters >
                            headerCharacters
                        ? columnCharacters -
                            headerCharacters
                        : std::uint32_t{2U});
            for (std::uint32_t index = 0U;
                 index < padding;
                 ++index) {
                if (!text.Append(Base::StringView(" "))) {
                    return;
                }
            }
        }
    }
    columnHeaders_->SetText(text.View());
}

Base::Ref<FrameworkElement>
ListView::GetContainerForItemOverride() const noexcept {
    return Base::MakeRef<ListViewItem>();
}


namespace {

// Resolve GridView from a Columns DP value. The Columns property is exposed
// as Ref<Base::Object> holding the owning GridView itself (see
// GridView::GetColumnsObject), so a direct cast is the fast path.
const GridView* GridViewFromColumnsValue(const Base::Ref<Base::Object>& value) noexcept {
    if (!value) return nullptr;
    return ::Aero::TryCast<GridView>(value.Get());
}

DependencyObject* GetAncestorOrParent(const DependencyObject* current) noexcept {
    if (current == nullptr) return nullptr;
    const FrameworkElement* fe = ::Aero::TryCast<FrameworkElement>(current);
    const ::Aero::Media::Visual* vis = ::Aero::TryCast<::Aero::Media::Visual>(current);
    if (fe != nullptr && fe->GetTemplatedParent() != nullptr) {
        return fe->GetTemplatedParent();
    }
    if (vis != nullptr && vis->GetVisualParent() != nullptr) {
        return vis->GetVisualParent();
    }
    if (vis != nullptr && vis->GetLogicalParent() != nullptr) {
        return vis->GetLogicalParent();
    }
    return nullptr;
}

// Walk templated/visual/logical parents to the owning ListView and return its
// GridView. This covers two WPF patterns used by the gallery theme:
//  - GridViewHeaderRowPresenter inside ScrollViewer template whose
//    TemplatedParent is the ScrollViewer, whose TemplatedParent is ListView.
//    The authored bindings Path=TemplatedParent.View.* require a
//    "TemplatedParent" path segment which the binding engine does not resolve
//    (no such DP), so the Columns DP often stays null.
//  - GridViewRowPresenter inside ListViewItem template with no Columns/Content
//    bindings at all (<GridViewRowPresenter />).
// Auto-discovery makes both presenters work even when those bindings fail.
const GridView* FindAncestorGridView(const FrameworkElement& start) noexcept {
    const DependencyObject* current = &start;
    for (std::uint32_t depth = 0U; depth < 64U && current != nullptr; ++depth) {
        if (const auto* listView = ::Aero::TryCast<ListView>(current)) {
            Base::Ref<ViewBase> view = listView->GetView();
            if (const auto* grid = ::Aero::TryCast<GridView>(view.Get())) {
                return grid;
            }
            return nullptr;
        }
        DependencyObject* next = GetAncestorOrParent(current);
        if (next == nullptr || next == current) break;
        current = next;
    }
    return nullptr;
}

const GridView* ResolvePresenterView(const FrameworkElement& presenter,
    const Base::Ref<Base::Object>& columnsValue) noexcept {
    if (const GridView* direct = GridViewFromColumnsValue(columnsValue)) {
        return direct;
    }
    return FindAncestorGridView(presenter);
}

Base::Ref<Base::Object> ResolveRowItem(const GridViewRowPresenter& presenter,
    const Base::Ref<Base::Object>& contentValue) noexcept {
    if (contentValue) {
        // Explicit Content binding (WPF ListViewItem template binds
        // Content="{TemplateBinding Content}"). In this engine the container's
        // Content is a placeholder TextBlock for GridView mode, so prefer the
        // DataContext below when Content looks generated. A real data item is
        // rarely a TextBlock created by the generator.
        bool contentIsPlaceholder = false;
        if (contentValue->RuntimeType() == TextBlock::StaticTypeId()) {
            contentIsPlaceholder = true;
        }
        if (!contentIsPlaceholder) {
            return contentValue;
        }
    }
    Value dataContext = presenter.GetDataContext();
    if (dataContext.Kind() == ValueKind::Object && !dataContext.IsNullObject() &&
        dataContext.AsObject()) {
        // Guard against the placeholder TextBlock leaking in as DataContext;
        // DataContext should be the data item (ContentControl in gallery).
        Base::Ref<Base::Object> dc = dataContext.AsObject();
        if (dc->RuntimeType() != TextBlock::StaticTypeId()) {
            return dc;
        }
    }
    // Fall back to the templated ListViewItem's DataContext.
    const DependencyObject* current = &presenter;
    for (std::uint32_t depth = 0U; depth < 16U && current != nullptr; ++depth) {
        DependencyObject* next = GetAncestorOrParent(current);
        if (next == nullptr) break;
        if (const auto* item = ::Aero::TryCast<ListViewItem>(next)) {
            Value itemContext = item->GetDataContext();
            if (itemContext.Kind() == ValueKind::Object && !itemContext.IsNullObject() &&
                itemContext.AsObject()) {
                return itemContext.AsObject();
            }
            break;
        }
        if (next == current) break;
        current = next;
    }
    if (contentValue) return contentValue;
    return {};
}

void AttachPresenterChild(FrameworkElement& presenter, UIElement& child,
    const Base::Ref<Base::Object>& owner) noexcept {
    if (!owner || owner.Get() != &child) return;
    ElementTree* tree = VisualTree(presenter);
    if (tree == nullptr) return;
    if (VisualTree(child) == nullptr && child.GetLogicalParent() == nullptr) {
        (void)tree->AttachElement(presenter, child);
    } else if (child.GetVisualParent() != &presenter || !child.GetIsLayoutAttached()) {
        if (child.GetVisualParent() != nullptr && child.GetVisualParent() != &presenter) {
            return;
        }
        (void)tree->AttachVisualChild(presenter, child);
    }
}

void DetachPresenterChild(FrameworkElement& presenter, UIElement& child) noexcept {
    ElementTree* tree = VisualTree(presenter);
    if (tree == nullptr) return;
    if (VisualTree(child) != tree) return;
    if (child.GetVisualParent() == &presenter) {
        (void)tree->DetachVisual(presenter, child);
    }
    if (child.GetLogicalParent() == &presenter) {
        (void)tree->DetachLogical(presenter, child);
    }
}

double ColumnWidth(const GridViewColumn& column) noexcept {
    const double width = column.GetWidth();
    if (!(width > 0.0) || !std::isfinite(width)) return 100.0;
    return width;
}

} // namespace

// ---- GridViewHeaderRowPresenter ----

GridViewHeaderRowPresenter::~GridViewHeaderRowPresenter() {
    ClearCells();
}

std::uint32_t GridViewHeaderRowPresenter::GetVisualChildrenCount() const noexcept {
    std::uint32_t count = 0U;
    for (const auto& owned : ownedCells_) {
        if (!owned) continue;
        auto* visual = ::Aero::TryCast<::Aero::Media::Visual>(owned.Get());
        if (visual != nullptr && visual->GetVisualParent() == this) ++count;
    }
    return count;
}

::Aero::Media::Visual* GridViewHeaderRowPresenter::GetVisualChild(std::uint32_t index) const noexcept {
    for (const auto& owned : ownedCells_) {
        if (!owned) continue;
        auto* visual = ::Aero::TryCast<::Aero::Media::Visual>(owned.Get());
        if (visual == nullptr || visual->GetVisualParent() != this) continue;
        if (index == 0U) return visual;
        --index;
    }
    return nullptr;
}

std::uint32_t GridViewHeaderRowPresenter::GetLayoutChildrenCount() const noexcept {
    std::uint32_t count = 0U;
    for (const auto& owned : ownedCells_) {
        if (!owned) continue;
        auto* element = ::Aero::TryCast<UIElement>(owned.Get());
        if (element != nullptr && element->GetVisualParent() == this) ++count;
    }
    return count;
}

UIElement* GridViewHeaderRowPresenter::GetLayoutChild(std::uint32_t index) const noexcept {
    for (const auto& owned : ownedCells_) {
        if (!owned) continue;
        auto* element = ::Aero::TryCast<UIElement>(owned.Get());
        if (element == nullptr || element->GetVisualParent() != this) continue;
        if (index == 0U) return element;
        --index;
    }
    return nullptr;
}

void GridViewHeaderRowPresenter::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (args.GetProperty() == ColumnsProperty.Handle() ||
        args.GetProperty() == ColumnHeaderContainerStyleProperty.Handle()) {
        cellsDirty_ = true;
        InvalidateMeasure();
    }
    FrameworkElement::OnPropertyChanged(args);
}

void GridViewHeaderRowPresenter::ClearCells() noexcept {
    for (const auto& owned : ownedCells_) {
        if (!owned) continue;
        if (auto* element = ::Aero::TryCast<UIElement>(owned.Get())) {
            DetachPresenterChild(*this, *element);
        }
    }
    ownedCells_.Clear();
    cellsDirty_ = true;
}

void GridViewHeaderRowPresenter::RebuildIfNeeded() noexcept {
    const GridView* view = ResolvePresenterView(*this, GetValue(ColumnsProperty));
    if (view == nullptr) {
        if (!ownedCells_.Empty()) ClearCells();
        return;
    }
    auto columns = view->GetColumns();
    if (!cellsDirty_ && columns.Size() == ownedCells_.Size()) {
        // First build may have run before the presenter joined a View (no
        // ElementTree), leaving cells stored but unattached. Re-attach them
        // now that layout (hence a tree) is available.
        if (ElementTree* tree = VisualTree(*this)) {
            static_cast<void>(tree);
            for (const auto& owned : ownedCells_) {
                if (!owned) continue;
                auto* element = ::Aero::TryCast<UIElement>(owned.Get());
                if (element != nullptr && element->GetVisualParent() == nullptr &&
                    VisualTree(*element) == nullptr) {
                    AttachPresenterChild(*this, *element, owned);
                }
            }
        }
        return;
    }
    ClearCells();
    cellsDirty_ = false;
    for (const Base::Ref<GridViewColumn>& columnRef : columns) {
        if (!columnRef) continue;
        Base::Result<Base::Ref<GridViewColumnHeader>> made = Base::MakeRef<GridViewColumnHeader>();
        if (!made || !made.Value()) {
            cellsDirty_ = true;
            continue;
        }
        Base::Ref<GridViewColumnHeader> header = std::move(made).Value();
        header->SetContent(columnRef->GetHeader());
        // Per-column style wins, then presenter/GridView ColumnHeaderContainerStyle.
        Base::Ref<Style> columnStyle = columnRef->GetHeaderContainerStyle();
        if (!columnStyle) {
            columnStyle = view->GetColumnHeaderContainerStyle();
        }
        if (!columnStyle) {
            Base::Ref<Base::Object> presenterStyle = GetValue(ColumnHeaderContainerStyleProperty);
            if (presenterStyle && presenterStyle->RuntimeType() == Style::StaticTypeId()) {
                columnStyle = Base::Ref<Style>::FromBorrowed(
                    static_cast<Style&>(*presenterStyle));
            }
        }
        if (columnStyle) {
            header->SetValue(FrameworkElement::StyleProperty, columnStyle);
        }
        Base::Ref<Base::Object> owner(header);
        AttachPresenterChild(*this, *header, owner);
        ownedCells_.PushBack(std::move(owner));
    }
}

Size GridViewHeaderRowPresenter::MeasureOverride(Size availableSize) noexcept {
    RebuildIfNeeded();
    double totalWidth = 0.0;
    double maxHeight = 0.0;
    const GridView* view = ResolvePresenterView(*this, GetValue(ColumnsProperty));
    Base::Span<const Base::Ref<GridViewColumn>> columns =
        view != nullptr ? view->GetColumns() : Base::Span<const Base::Ref<GridViewColumn>>{};
    for (std::uint32_t i = 0U; i < ownedCells_.Size(); ++i) {
        auto* element = ::Aero::TryCast<UIElement>(ownedCells_[i].Get());
        if (element == nullptr) continue;
        double width = 100.0;
        if (i < columns.Size() && columns[i]) width = ColumnWidth(*columns[i]);
        MeasureChild(*element, Size{width, availableSize.height});
        totalWidth += width;
        const double h = element->GetDesiredSize().height;
        if (h > maxHeight) maxHeight = h;
    }
    return Size{totalWidth, maxHeight};
}

Size GridViewHeaderRowPresenter::ArrangeOverride(Size finalSize) noexcept {
    RebuildIfNeeded();
    const GridView* view = ResolvePresenterView(*this, GetValue(ColumnsProperty));
    Base::Span<const Base::Ref<GridViewColumn>> columns =
        view != nullptr ? view->GetColumns() : Base::Span<const Base::Ref<GridViewColumn>>{};
    double x = 0.0;
    for (std::uint32_t i = 0U; i < ownedCells_.Size(); ++i) {
        auto* element = ::Aero::TryCast<UIElement>(ownedCells_[i].Get());
        if (element == nullptr) continue;
        double width = 100.0;
        if (i < columns.Size() && columns[i]) width = ColumnWidth(*columns[i]);
        ArrangeChild(*element, Rect{x, 0.0, width, finalSize.height});
        x += width;
    }
    return finalSize;
}

// ---- GridViewRowPresenter ----

GridViewRowPresenter::~GridViewRowPresenter() {
    ClearCells();
}

std::uint32_t GridViewRowPresenter::GetVisualChildrenCount() const noexcept {
    std::uint32_t count = 0U;
    for (const auto& owned : ownedCells_) {
        if (!owned) continue;
        auto* visual = ::Aero::TryCast<::Aero::Media::Visual>(owned.Get());
        if (visual != nullptr && visual->GetVisualParent() == this) ++count;
    }
    return count;
}

::Aero::Media::Visual* GridViewRowPresenter::GetVisualChild(std::uint32_t index) const noexcept {
    for (const auto& owned : ownedCells_) {
        if (!owned) continue;
        auto* visual = ::Aero::TryCast<::Aero::Media::Visual>(owned.Get());
        if (visual == nullptr || visual->GetVisualParent() != this) continue;
        if (index == 0U) return visual;
        --index;
    }
    return nullptr;
}

std::uint32_t GridViewRowPresenter::GetLayoutChildrenCount() const noexcept {
    std::uint32_t count = 0U;
    for (const auto& owned : ownedCells_) {
        if (!owned) continue;
        auto* element = ::Aero::TryCast<UIElement>(owned.Get());
        if (element != nullptr && element->GetVisualParent() == this) ++count;
    }
    return count;
}

UIElement* GridViewRowPresenter::GetLayoutChild(std::uint32_t index) const noexcept {
    for (const auto& owned : ownedCells_) {
        if (!owned) continue;
        auto* element = ::Aero::TryCast<UIElement>(owned.Get());
        if (element == nullptr || element->GetVisualParent() != this) continue;
        if (index == 0U) return element;
        --index;
    }
    return nullptr;
}

void GridViewRowPresenter::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (args.GetProperty() == ColumnsProperty.Handle() ||
        args.GetProperty() == ContentProperty.Handle()) {
        InvalidateMeasure();
    }
    FrameworkElement::OnPropertyChanged(args);
}

void GridViewRowPresenter::OnDataContextChanged(
    const Value& oldValue, const Value& newValue) noexcept {
    static_cast<void>(oldValue);
    static_cast<void>(newValue);
    InvalidateMeasure();
    FrameworkElement::OnDataContextChanged(oldValue, newValue);
}

void GridViewRowPresenter::ClearCells() noexcept {
    for (const auto& owned : ownedCells_) {
        if (!owned) continue;
        if (auto* element = ::Aero::TryCast<UIElement>(owned.Get())) {
            DetachPresenterChild(*this, *element);
        }
    }
    ownedCells_.Clear();
    lastItem_.Reset();
    lastView_ = nullptr;
    lastColumnCount_ = 0U;
}

void GridViewRowPresenter::RebuildIfNeeded() noexcept {
    const GridView* view = ResolvePresenterView(*this, GetValue(ColumnsProperty));
    Base::Ref<Base::Object> item = ResolveRowItem(*this, GetValue(ContentProperty));
    std::uint32_t columnCount = view != nullptr
        ? static_cast<std::uint32_t>(view->GetColumns().Size()) : 0U;
    if (view == lastView_ && columnCount == lastColumnCount_ &&
        ((lastItem_ && item && lastItem_.Get() == item.Get()) ||
         (!lastItem_ && !item)) &&
        !ownedCells_.Empty()) {
        if (ElementTree* tree = VisualTree(*this)) {
            static_cast<void>(tree);
            for (const auto& owned : ownedCells_) {
                if (!owned) continue;
                auto* element = ::Aero::TryCast<UIElement>(owned.Get());
                if (element != nullptr && element->GetVisualParent() == nullptr &&
                    VisualTree(*element) == nullptr) {
                    AttachPresenterChild(*this, *element, owned);
                }
            }
        }
        return;
    }
    ClearCells();
    if (view == nullptr || !item) {
        return;
    }
    lastView_ = view;
    lastItem_ = item;
    lastColumnCount_ = columnCount;
    Aero::BindingEngine* bindings = ElementTree::BindingsOf(*this);
    for (const Base::Ref<GridViewColumn>& columnRef : view->GetColumns()) {
        if (!columnRef) continue;
        GridViewColumn& column = *columnRef;
        // CellTemplate wins over DisplayMemberBinding (WPF priority is the
        // reverse of the gallery text, but WPF docs state DisplayMemberBinding
        // takes precedence when both are set; honor that here).
        Base::Ref<DataTemplate> cellTemplate = column.GetCellTemplate();
        Base::Ref<Data::Binding> displayBinding = column.GetDisplayMemberBinding();
        auto displayPath = column.GetDisplayMemberPath();
        if (cellTemplate && displayBinding) {
            // WPF: DisplayMemberBinding has priority over CellTemplate.
            cellTemplate.Reset();
        }
        if (cellTemplate) {
            Base::Result<Base::Ref<Base::Object>> created =
                FrameworkTemplateState::Instantiate(*cellTemplate, item, bindings);
            if (!created || !created.Value()) continue;
            Base::Ref<Base::Object> owned = std::move(created).Value();
            auto* element = ::Aero::TryCast<UIElement>(owned.Get());
            if (element == nullptr) continue;
            if (auto* fe = ::Aero::TryCast<FrameworkElement>(element)) {
                // Ensure template content binds against the row item.
                fe->SetDataContext(Value::FromObject(item->RuntimeType(), item));
            }
            AttachPresenterChild(*this, *element, owned);
            ownedCells_.PushBack(std::move(owned));
            continue;
        }
        Base::StringView pathView;
        Base::StringView formatView;
        Base::Ref<Data::IValueConverter> converter;
        Value converterParameter;
        Value fallback;
        Value nullValue;
        Data::BindingMode mode = Data::BindingMode::Default;
        UpdateSourceTrigger trigger = UpdateSourceTrigger::Default;
        if (displayBinding) {
            pathView = displayBinding->GetPathText();
            formatView = displayBinding->GetStringFormat();
            converter = displayBinding->GetConverter();
            converterParameter = displayBinding->GetConverterParameter();
            fallback = displayBinding->GetFallbackValue();
            nullValue = displayBinding->GetTargetNullValue();
            mode = displayBinding->GetMode();
            trigger = displayBinding->GetUpdateSourceTrigger();
        } else if (!displayPath.Empty()) {
            pathView = displayPath;
        } else {
            continue;
        }
        if (pathView.Empty()) continue;
        Base::Result<Base::Ref<TextBlock>> made = Base::MakeRef<TextBlock>();
        if (!made || !made.Value()) continue;
        Base::Ref<TextBlock> text = std::move(made).Value();
        text->SetDataContext(Value::FromObject(item->RuntimeType(), item));
        if (bindings != nullptr && bindings->Metadata() != nullptr) {
            Data::MetadataBindingDescriptor descriptor;
            descriptor.metadata = bindings->Metadata();
            descriptor.source = item.Get();
            descriptor.target = text.Get();
            descriptor.targetProperty = TextBlock::TextProperty.Handle();
            descriptor.path = pathView;
            descriptor.stringFormat = formatView;
            descriptor.mode = Aero::BindingEngine::ResolveBindingMode(
                *text, TextBlock::TextProperty.Handle(), mode);
            descriptor.updateSourceTrigger =
                Aero::BindingEngine::ResolveUpdateSourceTrigger(
                    *text, TextBlock::TextProperty.Handle(), trigger);
            descriptor.converterResource = converter;
            descriptor.converterParameter = converterParameter;
            descriptor.fallbackValue = fallback;
            descriptor.targetNullValue = nullValue;
            (void)bindings->QueueDeferred(descriptor);
            (void)bindings->ActivateDeferredWhenReady(*text);
        }
        Base::Ref<Base::Object> owned(text);
        AttachPresenterChild(*this, *text, owned);
        ownedCells_.PushBack(std::move(owned));
    }
}

Size GridViewRowPresenter::MeasureOverride(Size availableSize) noexcept {
    RebuildIfNeeded();
    double totalWidth = 0.0;
    double maxHeight = 0.0;
    const GridView* view = ResolvePresenterView(*this, GetValue(ColumnsProperty));
    Base::Span<const Base::Ref<GridViewColumn>> columns =
        view != nullptr ? view->GetColumns() : Base::Span<const Base::Ref<GridViewColumn>>{};
    for (std::uint32_t i = 0U; i < ownedCells_.Size(); ++i) {
        auto* element = ::Aero::TryCast<UIElement>(ownedCells_[i].Get());
        if (element == nullptr) continue;
        double width = 100.0;
        if (i < columns.Size() && columns[i]) width = ColumnWidth(*columns[i]);
        MeasureChild(*element, Size{width, availableSize.height});
        totalWidth += width;
        const double h = element->GetDesiredSize().height;
        if (h > maxHeight) maxHeight = h;
    }
    return Size{totalWidth, maxHeight};
}

Size GridViewRowPresenter::ArrangeOverride(Size finalSize) noexcept {
    RebuildIfNeeded();
    const GridView* view = ResolvePresenterView(*this, GetValue(ColumnsProperty));
    Base::Span<const Base::Ref<GridViewColumn>> columns =
        view != nullptr ? view->GetColumns() : Base::Span<const Base::Ref<GridViewColumn>>{};
    double x = 0.0;
    for (std::uint32_t i = 0U; i < ownedCells_.Size(); ++i) {
        auto* element = ::Aero::TryCast<UIElement>(ownedCells_[i].Get());
        if (element == nullptr) continue;
        double width = 100.0;
        if (i < columns.Size() && columns[i]) width = ColumnWidth(*columns[i]);
        ArrangeChild(*element, Rect{x, 0.0, width, finalSize.height});
        x += width;
    }
    return finalSize;
}

namespace {

void AddGridViewColumn(
    Base::Object& owner,
    const Base::Ref<Base::Object>& item,
    void*) noexcept {
    if (!item ||
        item->RuntimeType() !=
            GridViewColumn::StaticTypeId()) {
        return;
    }
    (void)static_cast<GridView&>(
        owner).AddColumn(
            Base::Ref<GridViewColumn>::
                FromBorrowed(
                    static_cast<GridViewColumn&>(
                        *item)));
}

void ClearGridViewColumns(
    Base::Object& owner,
    void*) noexcept {
    static_cast<GridView&>(
        owner).ClearColumns();
}

} // namespace

AERO_DESCRIBE(GridViewColumnHeader) {
    using namespace Aero::Meta;
    Register<GridViewColumnHeader>(context)
        .Property(GridViewColumnHeader::RoleProperty, GridViewColumnHeaderRole::Normal)
        .Factory();
}

AERO_DESCRIBE(GridViewColumn) {
    using namespace Aero::Meta;
    Register<GridViewColumn>(context)
        .Property(GridViewColumn::HeaderProperty, Value::NullObject(TypeOf<Base::Object>()))
        .Property(GridViewColumn::WidthProperty, 100.0, FrameworkPropertyMetadataOptions::None, &Base::Validate::NonNegative<double>)
        .Property(GridViewColumn::CellTemplateProperty, Base::Ref<DataTemplate>{})
        .Property(GridViewColumn::HeaderTemplateProperty, Base::Ref<DataTemplate>{})
        .Property(GridViewColumn::DisplayMemberPathProperty, Base::String{})
        .Property(GridViewColumn::DisplayMemberBindingProperty, Base::Ref<Data::Binding>{})
        .Property(GridViewColumn::HeaderContainerStyleProperty, Base::Ref<Style>{})
        .Factory();
}

AERO_DESCRIBE(ViewBase) {
    using namespace Aero::Meta;
    Register<ViewBase>(context)
        .Factory();
}

AERO_DESCRIBE(GridView) {
    using namespace Aero::Meta;
    Register<GridView>(context)
        .Property(GridView::AllowsColumnReorderProperty, false)
        .Property(GridView::ColumnHeaderContainerStyleProperty, Base::Ref<Style>{})
        .Property(GridView::ColumnHeaderContextMenuProperty, Base::Ref<Base::Object>{})
        .Property(GridView::ColumnHeaderTemplateProperty, Base::Ref<Base::Object>{})
        .Property(GridView::ColumnHeaderTemplateSelectorProperty, Base::Ref<Base::Object>{})
        .Property(GridView::ColumnHeaderToolTipProperty, Base::Ref<Base::Object>{})
        .Property<Base::Ref<Base::Object>, &GridView::GetColumnsObject, &GridView::SetColumnsObject>("Columns", PropertyFlags::Structural)
        .Content<Base::Object>("ColumnItems", ContentKind::Collection, &AddGridViewColumn, &ClearGridViewColumns)
        .Factory();
}

AERO_DESCRIBE(GridViewHeaderRowPresenter) {
    using namespace Aero::Meta;
    Register<GridViewHeaderRowPresenter>(context)
        .Property(GridViewHeaderRowPresenter::AllowsColumnReorderProperty, false)
        .Property(GridViewHeaderRowPresenter::ColumnHeaderContainerStyleProperty, Base::Ref<Base::Object>{})
        .Property(GridViewHeaderRowPresenter::ColumnHeaderContextMenuProperty, Base::Ref<Base::Object>{})
        .Property(GridViewHeaderRowPresenter::ColumnHeaderTemplateProperty, Base::Ref<Base::Object>{})
        .Property(GridViewHeaderRowPresenter::ColumnHeaderTemplateSelectorProperty, Base::Ref<Base::Object>{})
        .Property(GridViewHeaderRowPresenter::ColumnHeaderToolTipProperty, Base::Ref<Base::Object>{})
        .Property(GridViewHeaderRowPresenter::ColumnsProperty, Base::Ref<Base::Object>{})
        .Factory();
}

AERO_DESCRIBE(GridViewRowPresenter) {
    using namespace Aero::Meta;
    Register<GridViewRowPresenter>(context)
        .Property(GridViewRowPresenter::ColumnsProperty, Base::Ref<Base::Object>{})
        .Property(GridViewRowPresenter::ContentProperty, Base::Ref<Base::Object>{})
        .Factory();
}

AERO_DESCRIBE(ListView) {
    using namespace Aero::Meta;
    Register<ListView>(context)
        .Property(ListView::ViewProperty, Base::Ref<ViewBase>{}, AffectsMeasure)
        .Factory();
}

AERO_DESCRIBE(ListViewItem) {
    using namespace Aero::Meta;
    Register<ListViewItem>(context)
        .Override(Aero::UIElement::IsTabStopProperty, true, FrameworkPropertyMetadataOptions::None)
        .Factory();
}

} // namespace Aero::Controls
