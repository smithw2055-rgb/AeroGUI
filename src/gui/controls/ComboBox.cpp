#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/input/InputManager.hpp" 
#include "gui/controls/ItemsContainers.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include <Aero/VisualStateManager.hpp>
#include <Aero/Controls.hpp>
#include <Aero/ClassHandler.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/Popup.hpp>
#include <Aero/Data/CollectionView.hpp>


#include <Aero/Base/String.hpp>
#include "gui/core/ValueConversion.hpp"
#include <algorithm>
#include <utility>
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero::Controls {

using namespace Primitives;

ComboBox::ComboBox() noexcept
    : Selector(StaticTypeId()),
      popupIsOpenChangedHandler_(
          this,
          &ComboBox::OnPopupIsOpenChanged),
      selectedProjectionChangedHandler_(
          this,
          &ComboBox::OnSelectedProjectionChanged),
      editableTextChangedHandler_(
          this,
          &ComboBox::OnEditableTextChanged) {}

ComboBox::~ComboBox() {
    ObserveSelectedProjection(nullptr);
}

bool ComboBox::GetIsDropDownOpen() const noexcept {
    return GetValue(IsDropDownOpenProperty);
}

void ComboBox::SetIsDropDownOpen(
    bool value) noexcept {
    SetValue(IsDropDownOpenProperty, value);
}

double ComboBox::GetMaxDropDownHeight() const noexcept {
    return GetValue(MaxDropDownHeightProperty);
}

void ComboBox::SetMaxDropDownHeight(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) return;
    SetValue(MaxDropDownHeightProperty, value);
}

bool ComboBox::GetIsEditable() const noexcept {
    return GetValue(IsEditableProperty);
}

bool ComboBox::GetIsReadOnly() const noexcept {
    return GetValue(IsReadOnlyProperty);
}

void ComboBox::SetIsReadOnly(
    bool value) noexcept {
    SetValue(IsReadOnlyProperty, value);
}

void ComboBox::SetIsEditable(
    bool value) noexcept {
    SetValue(IsEditableProperty, value);
}

Base::StringView ComboBox::GetText() const noexcept {
    return GetValue(TextProperty);
}

void ComboBox::SetText(
    Base::StringView value) noexcept {
    SetValue(TextProperty, value);
}

Base::StringView ComboBox::GetSelectionBoxText() const noexcept {
    return GetValue(SelectionBoxTextProperty);
}

Base::Result<Base::Ref<FrameworkElement>>
ComboBox::GetContainerForItemOverride() const noexcept {
    Base::Result<Base::Ref<ComboBoxItem>> made =
        Base::MakeRef<ComboBoxItem>();
    if (!made) return made.GetStatus();
    return Base::Ref<FrameworkElement>(
        std::move(made).Value());
}

Base::Result<void> ComboBox::PrepareContainerForItemOverride(
    FrameworkElement& container,
    const Base::Ref<Base::Object>& item,
    std::uint32_t index) noexcept {
    Base::Result<void> prepared =
        Selector::PrepareContainerForItemOverride(
            container, item, index);
    if (!prepared) return prepared.GetStatus();
    if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            container.RuntimeType(),
            ComboBoxItem::StaticTypeId())) {
        auto& comboItem =
            static_cast<ComboBoxItem&>(
                container);
        comboItem.SetIsSelected(GetIsSelected(index));
        comboItem.SetIsTabStop(true);
        return {};
    }
    return {};
}

void ComboBox::ClearContainerForItemOverride(
    FrameworkElement& container) noexcept {
    if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            container.RuntimeType(),
            ComboBoxItem::StaticTypeId())) {
        static_cast<ComboBoxItem&>(container).SetIsSelected(false);
    }
    Selector::ClearContainerForItemOverride(container);
}

void ComboBox::SynchronizeContainers() noexcept {
    ItemContainerGenerator* generator =
        AttachedGenerator();
    if (generator == nullptr) return;
    for (std::uint32_t index =
             generator->GetFirstGeneratedIndex();
         index <
             generator->GetFirstGeneratedIndex() +
                 generator->GetGeneratedCount();
         ++index) {
        FrameworkElement* container =
            generator->ContainerFromIndex(index);
        if (container == nullptr ||
            !DependencyObjectAccess::PropertyRegistry((*this)).Types().
                IsDerivedFrom(
                    container->RuntimeType(),
                    ComboBoxItem::StaticTypeId())) {
            continue;
        }
        static_cast<ComboBoxItem&>(*container).SetIsSelected(GetIsSelected(index));
    }
}

void ComboBox::OnContainersChanged() noexcept {
    Selector::OnContainersChanged();
    SynchronizeContainers();
    // Popup item containers are often generated after the closed presenter.
    // Once the full template exists, reuse their ItemTemplate projection for
    // the selected model item.
    if (popup_ != nullptr) {
        static_cast<void>(UpdateSelectionBox());
    }
}

void ComboBox::OnApplyTemplate()
    noexcept {
    Selector::OnApplyTemplate();

    DependencyObject* selection =
        GetTemplateChild("SelectionBox");
    selectionBox_ =
        selection != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().
            IsDerivedFrom(
                selection->RuntimeType(),
                TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(selection)
        : nullptr;
    DependencyObject* contentSite =
        GetTemplateChild("ContentSite");
    selectionPresenter_ =
        contentSite != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().
            IsDerivedFrom(
                contentSite->RuntimeType(),
                ContentPresenter::StaticTypeId())
        ? static_cast<ContentPresenter*>(
              contentSite)
        : nullptr;
    if (selectionBox_ == nullptr &&
        selectionPresenter_ != nullptr &&
        selectionPresenter_->GetContent() != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().
            IsDerivedFrom(
        selectionPresenter_->GetContent()->
                    RuntimeType(),
                TextBlock::StaticTypeId())) {
        selectionBox_ =
            static_cast<TextBlock*>(
                selectionPresenter_->GetContent());
    }
    if (selectionBox_ != nullptr) {
        selectionBox_->SetForeground(GetForeground());
    }
    DependencyObject* editable =
        GetTemplateChild("PART_EditableTextBox");
    editableTextBox_ =
        editable != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().
            IsDerivedFrom(
                editable->RuntimeType(),
                TextBox::StaticTypeId())
        ? static_cast<TextBox*>(editable)
        : nullptr;
    DependencyObject* border =
        GetTemplateChild("DropDownBorder");
    dropDownBorder_ =
        border != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().
            IsDerivedFrom(
                border->RuntimeType(),
                FrameworkElement::StaticTypeId())
        ? static_cast<FrameworkElement*>(border)
        : nullptr;
    DependencyObject* popup =
        GetTemplateChild("PART_Popup");
    popup_ =
        popup != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().
            IsDerivedFrom(
                popup->RuntimeType(),
                Popup::StaticTypeId())
        ? static_cast<Popup*>(popup)
        : nullptr;
    if (editableTextBox_ != nullptr) {
        editableTextBox_->AddHandler(
            TextBox::TextChangedEvent,
            editableTextChangedHandler_);
    }
    if (popup_ != nullptr) {
        static_cast<void>(popup_->AddValueChangedHandler(
            Popup::IsOpenProperty,
            popupIsOpenChangedHandler_));
        popup_->SetPlacementTarget(
            Base::Ref<UIElement>::TryFromBorrowed(*this));
        popup_->SetStaysOpen(false);
        popup_->SetIsOpen(GetIsDropDownOpen());
        popup_->SetMatchPlacementTargetWidth(true);
    }
    if (dropDownBorder_ != nullptr) {
        dropDownBorder_->SetMaxSize({1.0e12, GetMaxDropDownHeight()});
    }
    Base::Result<void> selectionUpdated =
        UpdateSelectionBox();
    if (!selectionUpdated) {
        return;
    }
    static_cast<void>(UpdateEditableVisualState());
}

void ComboBox::OnTemplateDetached() noexcept {
    ObserveSelectedProjection(nullptr);
    if (editableTextBox_ != nullptr) {
        static_cast<void>(
            editableTextBox_->RemoveHandler(
                TextBox::TextChangedEvent,
                editableTextChangedHandler_));
    }
    if (popup_ != nullptr) {
        static_cast<void>(popup_->RemoveValueChangedHandler(
            Popup::IsOpenProperty,
            popupIsOpenChangedHandler_));
        popup_->SetPlacementTarget({});
    }
    selectionBox_ = nullptr;
    selectionPresenter_ = nullptr;
    editableTextBox_ = nullptr;
    popup_ = nullptr;
    dropDownBorder_ = nullptr;
    Selector::OnTemplateDetached();
}

void ComboBox::OnSelectionChanged(
    const SelectionChangedEvent& event) {
    Selector::OnSelectionChanged(event);
    static_cast<void>(UpdateSelectionBox());
    SynchronizeContainers();
    if (GetIsDropDownOpen()) {
        SetIsDropDownOpen(false);
    }
}

void ComboBox::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    Selector::OnPropertyChanged(args);
    const DependencyPropertyHandle prop = args.GetProperty();
    if (prop == IsDropDownOpenProperty) {
        if (popup_ != nullptr && popup_->GetIsOpen() != args.GetNewValue().AsBoolean()) {
            static_cast<void>(
                popup_->SetIsOpen(
                    args.GetNewValue().AsBoolean()));
        }
        RoutedEventArgs eventArgs;
        RaiseEvent(
            args.GetNewValue().AsBoolean()
                ? DropDownOpenedEvent
                : DropDownClosedEvent,
            &eventArgs);
    } else if (prop == MaxDropDownHeightProperty) {
        if (dropDownBorder_ != nullptr) {
            static_cast<void>(
                dropDownBorder_->SetMaxSize(
                    {1.0e12,
                     GetMaxDropDownHeight()}));
        }
    } else if (prop == IsEditableProperty) {
        UpdateEditableVisualState();
    } else if (prop == TextProperty) {
        if (editableTextBox_ == nullptr ||
            synchronizingEditableText_ ||
            editableTextBox_->GetText() == GetText()) {
            return;
        }
        synchronizingEditableText_ = true;
        editableTextBox_->SetText(GetText());
        synchronizingEditableText_ = false;
    } else if (prop == Control::ForegroundProperty) {
        if (selectionBox_ != nullptr) {
            selectionBox_->SetForeground(GetForeground());
        }
        if (editableTextBox_ != nullptr) {
            editableTextBox_->SetForeground(GetForeground());
        }
    } else if (prop == Selector::SelectedIndexProperty ||
               prop == Selector::SelectedItemProperty) {
        static_cast<void>(UpdateSelectionBox());
    } else if (prop == UIElement::IsMouseOverProperty ||
               prop == UIElement::IsEnabledProperty) {
        UpdateVisualState(true);
    }
}

void ComboBox::OnPopupIsOpenChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    const bool open = args.GetNewValue().AsBoolean();
    if (open != GetIsDropDownOpen()) {
        SetIsDropDownOpen(open);
    }
}

void ComboBox::OnSelectedProjectionChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept {
    if (&object == selectedProjection_) {
        static_cast<void>(UpdateSelectionBox());
    }
}

void ComboBox::ObserveSelectedProjection(
    TextBlock* projection) noexcept {
    if (selectedProjection_ == projection) return;
    if (selectedProjection_ != nullptr) {
        static_cast<void>(selectedProjection_->RemoveValueChangedHandler(
            TextBlock::TextProperty,
            selectedProjectionChangedHandler_));
    }
    selectedProjection_ = projection;
    if (selectedProjection_ != nullptr) {
        selectedProjection_->AddValueChangedHandler(
            TextBlock::TextProperty,
            selectedProjectionChangedHandler_);
    }
}

void ComboBox::OnEditableTextChanged(
    Base::Object* sender,
    RoutedEventArgs&) noexcept {
    if (sender != editableTextBox_ ||
        editableTextBox_ == nullptr ||
        synchronizingEditableText_) {
        return;
    }
    Base::String edited;
    Base::Result<void> copied =
        edited.Assign(
            editableTextBox_->GetText());
    if (!copied) {
        return;
    }
    synchronizingEditableText_ = true;
    static_cast<void>(
        SetCurrentValue(
            TextProperty,
            std::move(edited)));
    synchronizingEditableText_ = false;
}

Base::Result<void>
ComboBox::UpdateSelectionBox() noexcept {
    Base::StringView text;
    TextBlock* selectedProjection = nullptr;
    Base::Ref<Base::Object> selected =
        GetSelectedItem();
    if (selected &&
        selected->RuntimeType() ==
            BoxedItemValue::StaticTypeId()) {
        const Meta::Value& value =
            static_cast<const BoxedItemValue&>(
                *selected).Value();
        if (value.Kind() ==
                Meta::ValueKind::String) {
            text = value.AsString();
        }
    } else if (selected &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().
            IsDerivedFrom(
                selected->RuntimeType(),
                TextBlock::StaticTypeId())) {
        text = static_cast<TextBlock*>(
            selected.Get())->GetText();
    } else if (
        selected &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().
            IsDerivedFrom(
                selected->RuntimeType(),
                ContentControl::StaticTypeId())) {
        UIElement* content =
            (*static_cast<ContentControl*>(
                selected.Get())).GetContentElement();
        if (content != nullptr &&
            DependencyObjectAccess::PropertyRegistry((*this)).Types().
                IsDerivedFrom(
                    content->RuntimeType(),
                    TextBlock::StaticTypeId())) {
            text = static_cast<TextBlock*>(
                content)->GetText();
        }
    }
    if (text.Empty() && selected) {
        // Data items are displayed through their existing ItemTemplate. This
        // is the same presentation the popup list uses and avoids a
        // Localization-specific model branch in ComboBox.
        ItemContainerGenerator* generator = AttachedGenerator();
        const std::uint32_t index = GetSelectedIndex();
        FrameworkElement* container =
            generator != nullptr && index != UINT32_MAX
            ? generator->ContainerFromIndex(index)
            : nullptr;
        if (container != nullptr &&
            DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                container->RuntimeType(),
                ContentControl::StaticTypeId())) {
            UIElement* content = (*static_cast<ContentControl*>(container)).GetContentElement();
            if (content != nullptr &&
                DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                    content->RuntimeType(), TextBlock::StaticTypeId())) {
                selectedProjection = static_cast<TextBlock*>(content);
                text = selectedProjection->GetText();
            }
        }
    }
    ObserveSelectedProjection(selectedProjection);
    if (text.Empty() && selected) {
        // When an ItemTemplate is realized after the initial selection, use
        // its conventional display property as a generic closed-state
        // fallback. This keeps model objects out of ComboBox while matching
        // WPF's selected-item presentation timing.
        Meta::ObjectFactoryState services = Meta::CurrentObjectFactory();
        const Meta::PropertyInfo* name = services.metadata != nullptr
            ? services.metadata->Types().FindProperty(
                selected->RuntimeType(), Base::StringView("Name"), true)
            : nullptr;
        if (name != nullptr) {
            Base::Result<Meta::Value> displayed =
                services.metadata->GetProperty(*selected, name->Id());
            if (displayed &&
                displayed.Value().Kind() == Meta::ValueKind::String) {
                text = displayed.Value().AsString();
            }
        }
    }
    Base::String value;
    Base::Result<void> assigned =
        value.Assign(text);
    if (!assigned) return assigned.GetStatus();
    SetReadOnlyCurrentValue(SelectionBoxTextProperty, value);
    Base::Result<Meta::Value> itemValue =
        Meta::Value::TryFromString(
            Meta::TypeOf<Base::String>(),
            text);
    if (!itemValue) {
        return itemValue.GetStatus();
    }
    Meta::Value selectionItem = std::move(itemValue).Value();
    SetReadOnlyCurrentValue(SelectionBoxItemProperty, selectionItem);
    // ContentSource is compiled into a TemplateBinding, but the closed
    // presenter is constructed before ItemsSource has materialized its first
    // selection.  Feed its current content at the same point as the
    // read-only source update so the initial selection is visible without
    // waiting for another template application or user selection change.
    if (selectionPresenter_ != nullptr) {
        selectionPresenter_->SetContentValue(selectionItem);
    }
    if (selectionBox_ != nullptr) {
        selectionBox_->SetText(text);
    }
    if (!text.Empty()) {
        SetCurrentValue(TextProperty, value);
    }
    UpdateEditableVisualState();
    return {};
}

void ComboBox::UpdateEditableVisualState() noexcept {
    if (selectionBox_ != nullptr) {
        selectionBox_->SetVisibility(GetIsEditable()
            ? Visibility::Collapsed : Visibility::Visible);
    }
    if (editableTextBox_ == nullptr) {
        return;
    }
    editableTextBox_->SetVisibility(GetIsEditable()
        ? Visibility::Visible : Visibility::Collapsed);
    if (editableTextBox_->GetText() == GetText()) {
        return;
    }
    synchronizingEditableText_ = true;
    editableTextBox_->SetText(GetText());
    synchronizingEditableText_ = false;
}

std::uint32_t ComboBox::FindContainerIndex(
    Base::Object* source) const noexcept {
    if (source == nullptr ||
        !DependencyObjectAccess::PropertyRegistry((*this)).Types().
            IsDerivedFrom(
                source->RuntimeType(),
                UIElement::StaticTypeId())) {
        return UINT32_MAX;
    }
    ::Aero::Media::Visual* visual =
        static_cast<UIElement*>(source);
    while (visual != nullptr &&
        visual != this) {
        UIElement* element =
            ::Aero::TryCast<::Aero::UIElement>(visual);
        if (element != nullptr &&
            DependencyObjectAccess::PropertyRegistry((*this)).Types().
                IsDerivedFrom(
                    element->RuntimeType(),
                    ComboBoxItem::StaticTypeId())) {
            ItemContainerGenerator* generator =
                AttachedGenerator();
            return generator != nullptr
                ? generator->IndexFromContainer(
                    static_cast<ComboBoxItem&>(
                        *element))
                : UINT32_MAX;
        }
        visual = visual->GetVisualParent();
    }
    return UINT32_MAX;
}


void ComboBox::UpdateVisualState(bool useTransitions) noexcept {
    Base::StringView comboCommon = "Normal";
    if (!GetIsEnabled()) {
        comboCommon = "Disabled";
    } else if (GetIsMouseOver()) {
        comboCommon = "MouseOver";
    }
    static_cast<void>(
        VisualStateManager::GoToState(
            *this,
            comboCommon,
            useTransitions));
}

void ComboBox::OnMouseLeftButtonDown(MouseButtonEventArgs& args) {
    if (args.GetHandled()) return;
    if (args.GetChangedButton() != MouseButton::Left) {
        return;
    }
    if (!GetIsEnabled()) return;
    const std::uint32_t index = FindContainerIndex(args.GetOriginalSource());
    if (index != UINT32_MAX) {
        SetSelectedIndex(index);
        SetIsDropDownOpen(false);
    } else {
        SetIsDropDownOpen(!GetIsDropDownOpen());
    }
    static_cast<void>(Focus());
    args.SetHandled(true);
}

void ComboBox::OnKeyDown(KeyEventArgs& args) {
    if (!GetIsEnabled()) return;
    if (args.GetKey() == KeyboardKeyEscape) {
        if (!GetIsDropDownOpen()) return;
        SetIsDropDownOpen(false);
        args.SetHandled(true);
        return;
    }
    if (args.GetKey() == KeyboardKeyEnter ||
        args.GetKey() == KeyboardKeySpace) {
        SetIsDropDownOpen(!GetIsDropDownOpen());
        args.SetHandled(true);
        return;
    }
    if (args.GetKey() != KeyboardKeyUp &&
        args.GetKey() != KeyboardKeyDown) {
        return;
    }
    if (GetCount() == 0U) return;
    std::uint32_t selected = GetSelectedIndex();
    if (selected == UINT32_MAX) {
        selected = 0U;
    } else if (
        args.GetKey() == KeyboardKeyDown &&
        selected + 1U < GetCount()) {
        ++selected;
    } else if (
        args.GetKey() == KeyboardKeyUp &&
        selected > 0U) {
        --selected;
    }
    SetSelectedIndex(selected);
    args.SetHandled(true);
}

bool ComboBoxItem::GetIsSelected() const noexcept {
    return GetValue(IsSelectedProperty);
}

void ComboBoxItem::SetIsSelected(
    bool value) noexcept {
    SetCurrentValue(IsSelectedProperty, value);
}

AERO_DESCRIBE(ComboBox) {
    using namespace Aero::Meta;
    Register<ComboBox>(context)
        .Event(ComboBox::DropDownOpenedEvent)
        .Event(ComboBox::DropDownClosedEvent)
        .Property(ComboBox::IsDropDownOpenProperty, false, AffectsMeasure | AffectsRender | BindsTwoWayByDefault)
        .Property(ComboBox::MaxDropDownHeightProperty, 240.0, AffectsMeasure, &Base::Validate::Positive<double>)
        .Property(ComboBox::IsEditableProperty, false, AffectsMeasure | AffectsRender)
        .Property(ComboBox::IsReadOnlyProperty, false, AffectsRender)
        .Property(ComboBox::TextProperty, Base::String{}, AffectsMeasure | BindsTwoWayByDefault)
        .Property(ComboBox::PlaceholderProperty, Base::String{}, AffectsMeasure | AffectsRender)
        .Property(ComboBox::SelectionBoxTextProperty, Base::String{})
        .Property(ComboBox::SelectionBoxItemProperty, Meta::Value::NullObject(Meta::TypeOf<Base::Object>()))
        .Override(Aero::UIElement::IsTabStopProperty, true, FrameworkPropertyMetadataOptions::None)
        .TemplatePart("PART_EditableTextBox", TypeOf<TextBox>())
        .Factory();
    AERO_ON(ComboBox, &ComboBox::OnMouseLeftButtonDown, UIElement::MouseLeftButtonDownEvent);
    AERO_ON(ComboBox, &ComboBox::OnKeyDown, UIElement::KeyDownEvent);
}

AERO_DESCRIBE(ComboBoxItem) {
    using namespace Aero::Meta;
    Register<ComboBoxItem>(context)
        .Property(ComboBoxItem::IsSelectedProperty, false, AffectsRender | BindsTwoWayByDefault)
        .Override(Aero::UIElement::IsTabStopProperty, true, FrameworkPropertyMetadataOptions::None)
        .Factory();
}

} // namespace Aero::Controls
