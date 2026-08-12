// Shared implementation helpers for the semantic metadata units.
// The Gallery uses a bare Control as a style/template host. Keep the public
// Control base class extensible while providing a concrete runtime instance
// for that XAML form.
class BasicControl : public Control {
public:
    BasicControl() noexcept : Control(Control::StaticTypeId()) {}
};

Base::Result<Base::Ref<Base::Object>>
CreateBasicControl() noexcept {
    Base::Result<Base::Ref<BasicControl>> created =
        Base::MakeRef<BasicControl>();
    return created
        ? Base::Result<Base::Ref<Base::Object>>(
            Base::Ref<Base::Object>(
                std::move(created).Value()))
        : Base::Result<Base::Ref<Base::Object>>(
            created.GetStatus());
}


class BasicContentControl : public ContentControl {
public:
    BasicContentControl() noexcept
        : ContentControl(ContentControl::StaticTypeId()) {}
};

Base::Result<Base::Ref<Base::Object>>
CreateBasicContentControl() noexcept {
    Base::Result<Base::Ref<BasicContentControl>> created =
        Base::MakeRef<BasicContentControl>();
    return created
        ? Base::Result<Base::Ref<Base::Object>>(
              Base::Ref<Base::Object>(std::move(created).Value()))
        : Base::Result<Base::Ref<Base::Object>>(created.GetStatus());
}

class BasicHeaderedContentControl : public HeaderedContentControl {
public:
    BasicHeaderedContentControl() noexcept
        : HeaderedContentControl(
              HeaderedContentControl::StaticTypeId()) {}
};

Base::Result<Base::Ref<Base::Object>>
CreateBasicHeaderedContentControl() noexcept {
    Base::Result<Base::Ref<BasicHeaderedContentControl>> created =
        Base::MakeRef<BasicHeaderedContentControl>();
    return created
        ? Base::Result<Base::Ref<Base::Object>>(
              Base::Ref<Base::Object>(std::move(created).Value()))
        : Base::Result<Base::Ref<Base::Object>>(created.GetStatus());
}
void AddTemplateTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) {
        return;
    }
    (void)::Aero::Controls::TemplatePrivate::AddAuthoredTrigger(
        static_cast<FrameworkTemplate&>(owner), value);
}

void ClearTemplateTriggers(
    Base::Object& owner,
    void*) noexcept {
    ::Aero::Controls::TemplatePrivate::ClearAuthoredTriggers(
        static_cast<FrameworkTemplate&>(owner));
}

template<class T>
void SetDeferredTemplateVisualTree(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if constexpr (std::is_same_v<T, ControlTemplate>) {
        (void)::Aero::Controls::TemplatePrivate::SetAuthoredVisualTree(
            static_cast<ControlTemplate&>(object), value);
    } else if constexpr (std::is_same_v<T, DataTemplate>) {
        (void)::Aero::Controls::TemplatePrivate::SetAuthoredVisualTree(
            static_cast<DataTemplate&>(object), value);
    } else {
        (void)::Aero::Controls::TemplatePrivate::SetAuthoredVisualTree(
            static_cast<ItemsPanelTemplate&>(object), value);
    }
}

template<class T>
void ClearDeferredTemplateVisualTree(
    Base::Object& object,
    void*) noexcept {
    if constexpr (std::is_same_v<T, ControlTemplate>) {
        ::Aero::Controls::TemplatePrivate::ClearAuthoredVisualTree(
            static_cast<ControlTemplate&>(object));
    } else if constexpr (std::is_same_v<T, DataTemplate>) {
        ::Aero::Controls::TemplatePrivate::ClearAuthoredVisualTree(
            static_cast<DataTemplate&>(object));
    } else {
        ::Aero::Controls::TemplatePrivate::ClearAuthoredVisualTree(
            static_cast<ItemsPanelTemplate&>(object));
    }
}

void AddTemplateVisualStateGroup(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    (void)::Aero::Controls::TemplatePrivate::AddAuthoredVisualStateGroup(
        static_cast<ControlTemplate&>(object), value);
}

void ClearTemplateVisualStateGroups(
    Base::Object& object,
    void*) noexcept {
    ::Aero::Controls::TemplatePrivate::ClearAuthoredVisualStateGroups(
        static_cast<ControlTemplate&>(object));
}

Meta::TypeReference GetControlTemplateTargetType(
    const ControlTemplate& value) noexcept {
    return {value.GetTargetType()};
}

void SetControlTemplateTargetType(
    ControlTemplate& target,
    Meta::TypeReference value) noexcept {
    (void)::Aero::Controls::TemplatePrivate::SetTargetType(target, value.type);
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

bool ValidateSliderTicks(
    const Base::String& value) noexcept {
    const Base::StringView text = value.View();
    std::uint32_t start = 0U;
    while (start < text.SizeBytes()) {
        while (start < text.SizeBytes() &&
            (text[start] == ' ' ||
             text[start] == '\t' ||
             text[start] == ',' ||
             text[start] == ';')) {
            ++start;
        }
        if (start >= text.SizeBytes()) break;
        std::uint32_t end = start;
        while (end < text.SizeBytes() &&
            text[end] != ' ' &&
            text[end] != '\t' &&
            text[end] != ',' &&
            text[end] != ';') {
            ++end;
        }
        Base::Result<double> parsed =
            ::Aero::Base::Detail::ValueConversion::ParseDouble(
                text.Substr(start, end - start));
        if (!parsed ||
            !std::isfinite(parsed.Value())) {
            return false;
        }
        start = end;
    }
    return true;
}

Base::Result<GridLength> ConvertGridLength(
    Base::StringView text) noexcept {
    const Base::StringView value =
        ::Aero::Base::Detail::ValueConversion::Trim(text);
    if (::Aero::Base::Detail::ValueConversion::EqualsAsciiInsensitive(
            value, "auto")) {
        return GridLength::Auto();
    }
    if (!value.Empty() &&
        value[value.SizeBytes() - 1U] == '*') {
        const Base::StringView weightText =
            value.Substr(0U, value.SizeBytes() - 1U);
        double weight = 1.0;
        if (!weightText.Empty()) {
            Base::Result<double> parsed =
                ::Aero::Base::Detail::ValueConversion::ParseDouble(weightText);
            if (!parsed) return parsed.GetStatus();
            weight = parsed.Value();
        }
        if (!std::isfinite(weight) || weight <= 0.0) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "GridLength star weight must be positive and finite");
        }
        return GridLength::Star(weight);
    }
    Base::Result<double> pixels =
        ::Aero::Base::Detail::ValueConversion::ParseDouble(value);
    if (!pixels || pixels.Value() < 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "GridLength must be Auto, a nonnegative pixel value, or a star weight");
    }
    return GridLength::Pixel(pixels.Value());
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

Base::Result<void> ParseGridDefinitions(
    Base::StringView text,
    Base::Vector<GridLength>& output) noexcept {
    output.Clear();
    const Base::StringView value =
        ::Aero::Base::Detail::ValueConversion::Trim(text);
    if (value.Empty()) return {};
    std::uint32_t start = 0U;
    while (start <= value.SizeBytes()) {
        std::uint32_t end = start;
        while (end < value.SizeBytes() &&
            value[end] != ',') {
            ++end;
        }
        const Base::StringView token =
            ::Aero::Base::Detail::ValueConversion::Trim(
                value.Substr(start, end - start));
        if (token.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Grid definitions contain an empty track");
        }
        Base::Result<GridLength> parsed =
            ConvertGridLength(token);
        if (!parsed) return parsed.GetStatus();
        Base::Result<void> added =
            output.PushBack(parsed.Value());
        if (!added) return added.GetStatus();
        if (end == value.SizeBytes()) break;
        start = end + 1U;
    }
    return {};
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
    (void)::Aero::Controls::TemplatePrivate::AddAuthoredTrigger(
        static_cast<DataTemplate&>(owner), std::move(retained));
}

void ClearDataTemplateTriggers(
    Base::Object& owner,
    void*) noexcept {
    ::Aero::Controls::TemplatePrivate::ClearAuthoredTriggers(
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

void AddGridInputBinding(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Input::KeyBinding> retained =
        Base::Ref<Input::KeyBinding>::TryFromBorrowed(
            static_cast<Input::KeyBinding&>(*value));
    if (retained) {
        (void)static_cast<Grid&>(owner).AddInputBinding(std::move(retained));
    }
}

void ClearGridInputBindings(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Grid&>(owner).ClearInputBindings();
}

bool ValidateGridDefinitionsText(
    const Base::String& value) noexcept {
    Base::Vector<GridLength> parsed;
    Base::Result<void> result =
        ParseGridDefinitions(
            value.View(), parsed);
    return static_cast<bool>(result);
}

void OnGridColumnsChanged(
    ::Aero::DependencyObject& object,
    const Base::String&,
    const Base::String& value) noexcept {
    Base::Vector<GridLength> parsed;
    if (ParseGridDefinitions(
            value.View(), parsed)) {
        static_cast<void>(
            static_cast<Grid&>(object).
                SetColumnDefinitions(
                    parsed.AsSpan()));
    }
}

void OnGridRowsChanged(
    ::Aero::DependencyObject& object,
    const Base::String&,
    const Base::String& value) noexcept {
    Base::Vector<GridLength> parsed;
    if (ParseGridDefinitions(
            value.View(), parsed)) {
        static_cast<void>(
            static_cast<Grid&>(object).
                SetRowDefinitions(
                    parsed.AsSpan()));
    }
}

void OnPathDataChanged(
    ::Aero::DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs&) noexcept {
    ::Aero::Controls::ControlPrivate::InvalidateGeometry(
        static_cast<Path&>(object));
}

void OnPathColorChanged(
    ::Aero::DependencyObject& object,
    const Base::Ref<Media::Brush>&,
    const Base::Ref<Media::Brush>&) noexcept {
    ::Aero::Controls::ControlPrivate::InvalidateGeometry(
        static_cast<Path&>(object));
}

void OnPathDoubleChanged(
    ::Aero::DependencyObject& object,
    const double&,
    const double&) noexcept {
    if (object.PropertyRegistry().Types().IsDerivedFrom(
            object.RuntimeType(), Path::StaticTypeId())) {
        ::Aero::Controls::ControlPrivate::InvalidateGeometry(
            static_cast<Path&>(object));
    } else if (object.PropertyRegistry().Types().IsDerivedFrom(
                   object.RuntimeType(), FrameworkElement::StaticTypeId())) {
        static_cast<void>(
            static_cast<FrameworkElement&>(object).InvalidateVisual());
    }
}

void OnPathLineJoinChanged(
    ::Aero::DependencyObject& object,
    const PenLineJoin&,
    const PenLineJoin&) noexcept {
    ::Aero::Controls::ControlPrivate::InvalidateGeometry(
        static_cast<Path&>(object));
}

void OnPathLineCapChanged(
    ::Aero::DependencyObject& object,
    const PenLineCap&,
    const PenLineCap&) noexcept {
    ::Aero::Controls::ControlPrivate::InvalidateGeometry(
        static_cast<Path&>(object));
}

void OnShapeFillChanged(
    ::Aero::DependencyObject&,
    const Meta::DependencyPropertyChangedEventArgs&) noexcept {
}

void OnScrollViewerVisibilityChanged(
    ::Aero::DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs&) noexcept {
    if (!object.PropertyRegistry().Types().IsDerivedFrom(
            object.RuntimeType(),
            ScrollViewer::StaticTypeId())) {
        return;
    }
    auto& viewer = static_cast<ScrollViewer&>(object);
    // Re-enter through the instance setters with the already committed
    // values. SetValue is then a no-op, while the setters refresh the
    // read-only computed visibility against the current extent/viewport.
    static_cast<void>(
        viewer.SetHorizontalScrollBarVisibility(
            viewer.GetHorizontalScrollBarVisibility()));
    static_cast<void>(
        viewer.SetVerticalScrollBarVisibility(
            viewer.GetVerticalScrollBarVisibility()));
}

bool ValidateNormalizedDouble(
    const double& value) noexcept {
    return std::isfinite(value) &&
        value >= 0.0 && value <= 1.0;
}

Base::Result<Base::Ref<Base::Object>> CoerceSelectedObject(
    ::Aero::DependencyObject& object,
    const Meta::DependencyProperty&,
    const Base::Ref<Base::Object>& value) noexcept {
    const auto& selector =
        static_cast<const Selector&>(object);
    if (value && selector.GetCount() != 0U &&
        selector.GetIndexOfItem(value.Get()) == UINT32_MAX) {
        return Base::Ref<Base::Object>{};
    }
    return value;
}

Base::Result<bool> CoerceButtonEnabled(
    ::Aero::DependencyObject& object,
    const Meta::DependencyProperty&,
    const bool& value) noexcept {
    return value &&
        static_cast<ButtonBase&>(object).GetIsCommandEnabled();
}

Base::Result<Base::String> CoerceTextBoxText(
    ::Aero::DependencyObject& object,
    const Meta::DependencyProperty&,
    const Base::String& value) noexcept {
    ::Aero::Text::EditableTextModel validation;
    static_cast<void>(object);
    Base::Result<void> limited =
        validation.SetText(value.View());
    if (!limited) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "TextBox text is not valid UTF-8");
    }
    return value;
}

bool ValidatePasswordChar(
    const Base::String& value) noexcept {
    ::Aero::Text::EditableTextModel validation;
    Base::Result<void> text =
        validation.SetText(value.View());
    return text &&
        validation.GraphemeCount() == 1U;
}

Base::Result<double> CoerceRangeMinimum(
    ::Aero::DependencyObject& object,
    const Meta::DependencyProperty&,
    const double& value) noexcept {
    const double maximum =
        static_cast<RangeBase&>(object).GetMaximum();
    return std::min(value, maximum);
}

Base::Result<double> CoerceRangeMaximum(
    ::Aero::DependencyObject& object,
    const Meta::DependencyProperty&,
    const double& value) noexcept {
    const double minimum =
        static_cast<RangeBase&>(object).GetMinimum();
    return std::max(value, minimum);
}

Base::Result<double> CoerceRangeValue(
    ::Aero::DependencyObject& object,
    const Meta::DependencyProperty&,
    const double& value) noexcept {
    const auto& range =
        static_cast<const RangeBase&>(object);
    return std::clamp(
        value,
        range.GetMinimum(),
        range.GetMaximum());
}

void SetPanelContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    (void)::Aero::Controls::ControlPrivate::Add(
        static_cast<Panel&>(owner), child, *static_cast<Aero::UIElement*>(child.Get()));
}

void ClearPanelContent(
    Base::Object& owner,
    void*) noexcept {
    (void)::Aero::Controls::ControlPrivate::Clear(static_cast<Panel&>(owner));
}

void SetDecoratorContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    (void)::Aero::Controls::ControlPrivate::SetOwnedChild(
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
    (void)::Aero::Controls::ControlPrivate::SetContentValue(
        static_cast<ContentControl&>(owner), child);
}

void ClearContentControlContent(
    Base::Object& owner,
    void*) noexcept {
    (void)::Aero::Controls::ControlPrivate::SetContentValue(
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
    if (!text.PropertyRegistry().Types().IsDerivedFrom(
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
    if (!span.PropertyRegistry().Types().IsDerivedFrom(
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
    (void)static_cast<ItemsControl&>(owner).GetItems().Add(item);
}

void ClearItemsControlItems(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ItemsControl&>(owner).GetItems().Reset();
}

void OnItemsSourceChanged(
    ::Aero::DependencyObject& object,
    const Base::Ref<Base::Object>&,
    const Base::Ref<Base::Object>& value) noexcept {
    Collections::IItemsSource* source = nullptr;
    if (value) {
        if (value->RuntimeType() ==
            Collections::ObservableCollection::StaticTypeId()) {
            source = static_cast<Collections::ObservableCollection*>(value.Get());
        } else if (value->RuntimeType() ==
                   Media::GradientStopCollection::StaticTypeId()) {
            source = static_cast<Media::GradientStopCollection*>(
                value.Get());
        }
    }
    ItemsControl::Access::SetItemsSource(
        static_cast<ItemsControl&>(object), source);
}

void OnItemTemplateChanged(
    ::Aero::DependencyObject& object,
    const Base::Ref<DataTemplate>&,
    const Base::Ref<DataTemplate>& value) noexcept {
    ItemsControl::Access::SetItemTemplate(
        static_cast<ItemsControl&>(object), value.Get());
}

void OnDisplayMemberPathChanged(
    ::Aero::DependencyObject& object,
    const Base::String&,
    const Base::String&) noexcept {
    ItemsControl::Access::RefreshDisplayMemberPath(
        static_cast<ItemsControl&>(object));
}

void OnItemsPanelChanged(
    ::Aero::DependencyObject& object,
    const Base::Ref<ItemsPanelTemplate>&,
    const Base::Ref<ItemsPanelTemplate>& value) noexcept {
    ItemsControl::Access::SetItemsPanel(
        static_cast<ItemsControl&>(object), value.Get());
}

void OnItemContainerStyleChanged(
    ::Aero::DependencyObject& object,
    const Base::Ref<Style>&,
    const Base::Ref<Style>& value) noexcept {
    ItemsControl::Access::SetItemContainerStyle(
        static_cast<ItemsControl&>(object), value.Get());
}

void AddTreeViewItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& item,
    void*) noexcept {
    if (!item) {
        return;
    }
    (void)static_cast<TreeViewItem&>(owner).GetItems().Add(item);
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

void AddTabControlItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& item,
    void*) noexcept {
    if (!item) {
        return;
    }
    (void)static_cast<TabControl&>(owner).
        AddOwnedTab(
            Base::Ref<TabItem>::FromBorrowed(
                static_cast<TabItem&>(*item)));
}

void ClearTabControlItems(
    Base::Object& owner,
    void*) noexcept {
    static_cast<TabControl&>(owner).ClearOwnedTabs();
}
