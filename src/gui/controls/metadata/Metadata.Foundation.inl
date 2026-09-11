// Controls metadata: foundation (helpers + values).
// ---- From Support.inl (merged; registration order preserved) ----
// Shared implementation helpers for the semantic metadata units.
// The Gallery uses a bare Control as a style/template host. Keep the public
// Control base class extensible while providing a concrete runtime instance
// for that XAML form.
class BasicControl : public Control {
public:
    BasicControl() noexcept : Control(Control::StaticTypeId()) {}
};

class BasicContentControl : public ContentControl {
public:
    BasicContentControl() noexcept
        : ContentControl(ContentControl::StaticTypeId()) {}
};

class BasicHeaderedContentControl : public HeaderedContentControl {
public:
    BasicHeaderedContentControl() noexcept
        : HeaderedContentControl(
              HeaderedContentControl::StaticTypeId()) {}
};
void AddTemplateTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) {
        return;
    }
    (void)::Aero::Controls::FrameworkTemplateState::AddAuthoredTrigger(
        static_cast<FrameworkTemplate&>(owner), value);
}

void ClearTemplateTriggers(
    Base::Object& owner,
    void*) noexcept {
    ::Aero::Controls::FrameworkTemplateState::ClearAuthoredTriggers(
        static_cast<FrameworkTemplate&>(owner));
}

template<class T>
void SetDeferredTemplateVisualTree(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if constexpr (std::is_same_v<T, ControlTemplate>) {
        (void)::Aero::Controls::FrameworkTemplateState::SetAuthoredVisualTree(
            static_cast<ControlTemplate&>(object), value);
    } else if constexpr (std::is_same_v<T, DataTemplate>) {
        (void)::Aero::Controls::FrameworkTemplateState::SetAuthoredVisualTree(
            static_cast<DataTemplate&>(object), value);
    } else {
        (void)::Aero::Controls::FrameworkTemplateState::SetAuthoredVisualTree(
            static_cast<ItemsPanelTemplate&>(object), value);
    }
}

template<class T>
void ClearDeferredTemplateVisualTree(
    Base::Object& object,
    void*) noexcept {
    if constexpr (std::is_same_v<T, ControlTemplate>) {
        ::Aero::Controls::FrameworkTemplateState::ClearAuthoredVisualTree(
            static_cast<ControlTemplate&>(object));
    } else if constexpr (std::is_same_v<T, DataTemplate>) {
        ::Aero::Controls::FrameworkTemplateState::ClearAuthoredVisualTree(
            static_cast<DataTemplate&>(object));
    } else {
        ::Aero::Controls::FrameworkTemplateState::ClearAuthoredVisualTree(
            static_cast<ItemsPanelTemplate&>(object));
    }
}

void AddTemplateVisualStateGroup(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    (void)::Aero::Controls::FrameworkTemplateState::AddAuthoredVisualStateGroup(
        static_cast<ControlTemplate&>(object), value);
}

void ClearTemplateVisualStateGroups(
    Base::Object& object,
    void*) noexcept {
    ::Aero::Controls::FrameworkTemplateState::ClearAuthoredVisualStateGroups(
        static_cast<ControlTemplate&>(object));
}

Meta::TypeReference GetControlTemplateTargetType(
    const ControlTemplate& value) noexcept {
    return {value.GetTargetType()};
}

void SetControlTemplateTargetType(
    ControlTemplate& target,
    Meta::TypeReference value) noexcept {
    (void)::Aero::Controls::FrameworkTemplateState::SetTargetType(target, value.type);
}

Meta::TypeReference GetDataTemplateType(
    const DataTemplate& value) noexcept {
    return {value.GetDataType()};
}

void SetDataTemplateType(
    DataTemplate& target,
    Meta::TypeReference value) noexcept {
    target.SetDataType(value.type);
}

bool ValidateThicknessValue(
    const Aero::Thickness& thickness) noexcept {
    return Aero::IsFinite(thickness) &&
        thickness.left >= 0.0 && thickness.top >= 0.0 &&
        thickness.right >= 0.0 && thickness.bottom >= 0.0;
}

[[maybe_unused]] bool ValidateColorValue(
    const Base::Color& color) noexcept {
    return std::isfinite(color.red) && std::isfinite(color.green) &&
        std::isfinite(color.blue) && std::isfinite(color.alpha) &&
        color.red >= 0.0F && color.red <= 1.0F &&
        color.green >= 0.0F && color.green <= 1.0F &&
        color.blue >= 0.0F && color.blue <= 1.0F &&
        color.alpha >= 0.0F && color.alpha <= 1.0F;
}

bool ValidateCornerRadiusValue(
    const Aero::CornerRadius& radius) noexcept {
    return std::isfinite(radius.topLeft) &&
        std::isfinite(radius.topRight) &&
        std::isfinite(radius.bottomRight) &&
        std::isfinite(radius.bottomLeft) &&
        radius.topLeft >= 0.0 &&
        radius.topRight >= 0.0 &&
        radius.bottomRight >= 0.0 &&
        radius.bottomLeft >= 0.0;
}

bool ValidatePositiveFiniteDouble(const double& value) noexcept {
    return std::isfinite(value) && value > 0.0;
}

Base::Result<GridLength> ConvertGridLength(
    Base::StringView text) noexcept {
    return AeroGuiInternal::ConvertGridLength(text);
}

bool EqualGridLength(
    const void* left,
    const void* right,
    void*) noexcept {
    const auto& a =
        *static_cast<const GridLength*>(left);
    const auto& b =
        *static_cast<const GridLength*>(right);
    return a.unit == b.unit && a.value == b.value;
}

void AddDataTemplateTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Aero::TriggerBase> retained =
        Base::Ref<Aero::TriggerBase>::
            TryFromBorrowed(
                static_cast<
                    Aero::TriggerBase&>(
                        *value));
    if (!retained) {
        return;
    }
    (void)::Aero::Controls::FrameworkTemplateState::AddAuthoredTrigger(
        static_cast<DataTemplate&>(owner), std::move(retained));
}

void ClearDataTemplateTriggers(
    Base::Object& owner,
    void*) noexcept {
    ::Aero::Controls::FrameworkTemplateState::ClearAuthoredTriggers(
        static_cast<DataTemplate&>(owner));
}

void AddGridColumnDefinition(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<ColumnDefinition> retained =
        Base::Ref<ColumnDefinition>::TryFromBorrowed(
            static_cast<ColumnDefinition&>(*value));
    if (!retained) {
        return;
    }
    (void)static_cast<Grid&>(owner)
        .AddColumnDefinition(std::move(retained));
}

void ClearGridColumnDefinitions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Grid&>(owner).ClearColumnDefinitionObjects();
}

void AddGridRowDefinition(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<RowDefinition> retained =
        Base::Ref<RowDefinition>::TryFromBorrowed(
            static_cast<RowDefinition&>(*value));
    if (!retained) {
        return;
    }
    (void)static_cast<Grid&>(owner)
        .AddRowDefinition(std::move(retained));
}

void ClearGridRowDefinitions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Grid&>(owner).ClearRowDefinitionObjects();
}

bool ValidateNormalizedDouble(
    const double& value) noexcept {
    return std::isfinite(value) &&
        value >= 0.0 && value <= 1.0;
}

void SetPanelContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    AeroGuiInternal::PanelAddChild(
        static_cast<Panel&>(owner), child, *static_cast<Aero::UIElement*>(child.Get()));
}

void ClearPanelContent(
    Base::Object& owner,
    void*) noexcept {
    AeroGuiInternal::PanelClearChildren(static_cast<Panel&>(owner));
}

void SetDecoratorContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    (void)AeroGuiInternal::DecoratorSetOwnedChild(
        static_cast<Decorator&>(owner), child, *static_cast<Aero::UIElement*>(child.Get()));
}

void ClearDecoratorContent(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Decorator&>(owner).SetChild(nullptr);
}

void AddBulletDecoratorContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) return;
    auto& decorator = static_cast<BulletDecorator&>(owner);
    Base::Ref<UIElement> retained =
        Base::Ref<UIElement>::TryFromBorrowed(
            *static_cast<UIElement*>(child.Get()));
    if (!retained) return;
    if (decorator.GetBullet() == nullptr) {
        decorator.SetBullet(std::move(retained));
    } else {
        decorator.SetChild(std::move(retained));
    }
}

void ClearBulletDecoratorContent(
    Base::Object& owner,
    void*) noexcept {
    auto& decorator = static_cast<BulletDecorator&>(owner);
    decorator.SetBullet({});
    decorator.SetChild({});
}

void SetContentControlContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    (void)AeroGuiInternal::SetContentValue(
        static_cast<ContentControl&>(owner), child);
}

void ClearContentControlContent(
    Base::Object& owner,
    void*) noexcept {
    (void)AeroGuiInternal::SetContentValue(
        static_cast<ContentControl&>(owner), Meta::Value::NullObject(Meta::TypeOf<Base::Object>()));
}

void SetContentPresenterContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    static_cast<ContentPresenter&>(owner).SetOwnedContent(
        child, *static_cast<Aero::UIElement*>(child.Get()));
}

void ClearContentPresenterContent(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ContentPresenter&>(owner).SetContent(nullptr);
}

void AddTextBlockInline(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    auto& text = static_cast<TextBlock&>(owner);
    if (!AeroGuiInternal::PropertyRegistry(text).Types().IsDerivedFrom(
            child->RuntimeType(),
            Aero::Documents::Inline::StaticTypeId())) {
        return;
    }
    text.AddOwnedInline(child);
}

void ClearTextBlockInlines(
    Base::Object& owner,
    void*) noexcept {
    static_cast<TextBlock&>(owner).ClearOwnedInlines();
}

void AddSpanInline(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    auto& span = static_cast<Documents::Span&>(owner);
    if (!AeroGuiInternal::PropertyRegistry(span).Types().IsDerivedFrom(
            child->RuntimeType(),
            Documents::Inline::StaticTypeId())) {
        return;
    }
    span.AddOwnedInline(
        Base::Ref<Documents::Inline>::FromBorrowed(
            *static_cast<Documents::Inline*>(child.Get())));
}

void ClearSpanInlines(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Documents::Span&>(owner).ClearOwnedInlines();
}

void AddItemsControlItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& item,
    void*) noexcept {
    if (!item) {
        return;
    }
    static_cast<ItemsControl&>(owner).GetItems().Add(item);
}

void ClearItemsControlItems(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ItemsControl&>(owner).GetItems().Reset();
}

void AddTreeViewItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& item,
    void*) noexcept {
    if (!item) {
        return;
    }
    static_cast<TreeViewItem&>(owner).GetItems().Add(item);
}

void ClearTreeViewItems(
    Base::Object& owner,
    void*) noexcept {
    static_cast<TreeViewItem&>(
        owner).GetItems().Reset();
}

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
// ---- From Values.inl (merged; registration order preserved) ----
// Generated by the guarded Phase 6 migration from the original ordered
// metadata bootstrap. Keep registration order stable within this unit.
Base::Result<void> PopulateControlsValues(
    ::Aero::Meta::Registration& context) noexcept {
    Register<GridLength>(context)
        .ValueSemantics({sizeof(GridLength), alignof(GridLength), nullptr, nullptr, &EqualGridLength, nullptr, true})
        .TextConverter<&ConvertGridLength>();
    Register<ScrollChangedEventArgs>(context);
    Register<RangeValueChangedEventArgs>(context);
    return {};
}
