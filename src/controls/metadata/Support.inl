// Shared implementation helpers for the semantic metadata units.
// The Gallery uses a bare Control as a style/template host. Keep the public
// Control base class extensible while providing a concrete runtime instance
// for that XAML form.
class BasicControl final : public Control {
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

Base::Result<void> AddTemplateTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Template Trigger cannot be retained");
    }
    return Detail::FrameworkTemplateAccess::TryAddAuthoredTrigger(
        static_cast<FrameworkTemplate&>(owner), value);
}

Base::Result<void> ClearTemplateTriggers(
    Base::Object& owner,
    void*) noexcept {
    Detail::FrameworkTemplateAccess::ClearAuthoredTriggers(
        static_cast<FrameworkTemplate&>(owner));
    return {};
}

template<class T>
Base::Result<void> SetDeferredTemplateVisualTree(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if constexpr (std::is_same_v<T, ControlTemplate>) {
        return Detail::FrameworkTemplateAccess::SetAuthoredVisualTree(
            static_cast<ControlTemplate&>(object), value);
    } else if constexpr (std::is_same_v<T, DataTemplate>) {
        return Detail::DeferredTemplateAccess::SetAuthoredVisualTree(
            static_cast<DataTemplate&>(object), value);
    } else {
        return Detail::DeferredTemplateAccess::SetAuthoredVisualTree(
            static_cast<ItemsPanelTemplate&>(object), value);
    }
}

template<class T>
Base::Result<void> ClearDeferredTemplateVisualTree(
    Base::Object& object,
    void*) noexcept {
    if constexpr (std::is_same_v<T, ControlTemplate>) {
        Detail::FrameworkTemplateAccess::ClearAuthoredVisualTree(
            static_cast<ControlTemplate&>(object));
    } else if constexpr (std::is_same_v<T, DataTemplate>) {
        Detail::DeferredTemplateAccess::ClearAuthoredVisualTree(
            static_cast<DataTemplate&>(object));
    } else {
        Detail::DeferredTemplateAccess::ClearAuthoredVisualTree(
            static_cast<ItemsPanelTemplate&>(object));
    }
    return {};
}

Base::Result<void> AddTemplateVisualStateGroup(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    return Detail::FrameworkTemplateAccess::TryAddAuthoredVisualStateGroup(
        static_cast<ControlTemplate&>(object), value);
}

Base::Result<void> ClearTemplateVisualStateGroups(
    Base::Object& object,
    void*) noexcept {
    Detail::FrameworkTemplateAccess::ClearAuthoredVisualStateGroups(
        static_cast<ControlTemplate&>(object));
    return {};
}

Core::TypeReference GetControlTemplateTargetType(
    const ControlTemplate& value) noexcept {
    return {value.GetTargetType()};
}

Base::Result<void> SetControlTemplateTargetType(
    ControlTemplate& target,
    Core::TypeReference value) noexcept {
    return Detail::FrameworkTemplateAccess::TrySetTargetType(target, value.type);
}

Core::TypeReference GetDataTemplateType(
    const DataTemplate& value) noexcept {
    return {value.GetDataType()};
}

Base::Result<void> SetDataTemplateType(
    DataTemplate& target,
    Core::TypeReference value) noexcept {
    return target.SetDataType(value.type);
}

bool ValidateThicknessValue(
    const Aero::Thickness& thickness) noexcept {
    return Aero::IsFinite(thickness) &&
        thickness.left >= 0.0 && thickness.top >= 0.0 &&
        thickness.right >= 0.0 && thickness.bottom >= 0.0;
}

bool ValidateColorValue(
    const Render::Color& color) noexcept {
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
            Core::ValueConversion::ParseDouble(
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
        Core::ValueConversion::Trim(text);
    if (Core::ValueConversion::EqualsAsciiInsensitive(
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
                Core::ValueConversion::ParseDouble(weightText);
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
        Core::ValueConversion::ParseDouble(value);
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
        Core::ValueConversion::Trim(text);
    if (value.Empty()) return {};
    std::uint32_t start = 0U;
    while (start <= value.SizeBytes()) {
        std::uint32_t end = start;
        while (end < value.SizeBytes() &&
            value[end] != ',') {
            ++end;
        }
        const Base::StringView token =
            Core::ValueConversion::Trim(
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
            output.TryPushBack(parsed.Value());
        if (!added) return added.GetStatus();
        if (end == value.SizeBytes()) break;
        start = end + 1U;
    }
    return {};
}

Base::Result<void> AddDataTemplateTrigger(
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
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "DataTemplate Trigger cannot be retained");
    }
    return Detail::DeferredTemplateAccess::TryAddAuthoredTrigger(
        static_cast<DataTemplate&>(owner), std::move(retained));
}

Base::Result<void> ClearDataTemplateTriggers(
    Base::Object& owner,
    void*) noexcept {
    Detail::DeferredTemplateAccess::ClearAuthoredTriggers(
        static_cast<DataTemplate&>(owner));
    return {};
}

Base::Result<void> AddGridColumnDefinition(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<ColumnDefinition> retained =
        Base::Ref<ColumnDefinition>::TryFromBorrowed(
            static_cast<ColumnDefinition&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Grid ColumnDefinition cannot be retained");
    }
    return static_cast<Grid&>(owner)
        .AddColumnDefinition(std::move(retained));
}

Base::Result<void> ClearGridColumnDefinitions(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<Grid&>(owner)
        .ClearColumnDefinitionObjects();
}

Base::Result<void> AddGridRowDefinition(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<RowDefinition> retained =
        Base::Ref<RowDefinition>::TryFromBorrowed(
            static_cast<RowDefinition&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Grid RowDefinition cannot be retained");
    }
    return static_cast<Grid&>(owner)
        .AddRowDefinition(std::move(retained));
}

Base::Result<void> ClearGridRowDefinitions(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<Grid&>(owner)
        .ClearRowDefinitionObjects();
}

Base::Result<void> AddGridInputBinding(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Input::KeyBinding> retained =
        Base::Ref<Input::KeyBinding>::TryFromBorrowed(
            static_cast<Input::KeyBinding&>(*value));
    return retained
        ? static_cast<Grid&>(owner).AddInputBinding(std::move(retained))
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::InvalidArgument,
              "Grid InputBindings expects KeyBinding"));
}

Base::Result<void> ClearGridInputBindings(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Grid&>(owner).ClearInputBindings();
    return {};
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
    Core::DependencyObject& object,
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
    Core::DependencyObject& object,
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
    Core::DependencyObject& object,
    const Core::DependencyPropertyChangedEventArgs&) noexcept {
    Detail::PathServicesAccess::InvalidateGeometry(
        static_cast<Path&>(object));
}

void OnPathColorChanged(
    Core::DependencyObject& object,
    const Base::Ref<Media::Brush>& oldBrush,
    const Base::Ref<Media::Brush>& newBrush) noexcept {
    auto* owner = static_cast<Path&>(object).AsFrameworkElement();
    if (owner != nullptr) {
        if (oldBrush && oldBrush->Owner() == owner) {
            oldBrush->SetOwner(nullptr);
        }
        if (newBrush) {
            newBrush->SetOwner(owner);
        }
    }
    Detail::PathServicesAccess::InvalidateGeometry(
        static_cast<Path&>(object));
}

void OnPathDoubleChanged(
    Core::DependencyObject& object,
    const double&,
    const double&) noexcept {
    Detail::PathServicesAccess::InvalidateGeometry(
        static_cast<Path&>(object));
}

void OnPathLineJoinChanged(
    Core::DependencyObject& object,
    const PenLineJoin&,
    const PenLineJoin&) noexcept {
    Detail::PathServicesAccess::InvalidateGeometry(
        static_cast<Path&>(object));
}

void OnPathLineCapChanged(
    Core::DependencyObject& object,
    const PenLineCap&,
    const PenLineCap&) noexcept {
    Detail::PathServicesAccess::InvalidateGeometry(
        static_cast<Path&>(object));
}

void OnShapeFillChanged(
    Core::DependencyObject& object,
    const Core::DependencyPropertyChangedEventArgs&
        args) noexcept {
    auto* owner =
        static_cast<Shape&>(object).
            AsFrameworkElement();
    if (owner == nullptr) return;
    Base::Result<Base::Ref<Brush>> oldBrush =
        ValueCodec<Base::Ref<Brush>>::Decode(
            args.oldValue);
    if (oldBrush && oldBrush.Value() &&
        oldBrush.Value()->Owner() == owner) {
        oldBrush.Value()->SetOwner(nullptr);
    }
    Base::Result<Base::Ref<Brush>> newBrush =
        ValueCodec<Base::Ref<Brush>>::Decode(
            args.newValue);
    if (newBrush && newBrush.Value()) {
        newBrush.Value()->SetOwner(owner);
    }
}

void OnScrollViewerVisibilityChanged(
    Core::DependencyObject& object,
    const Core::DependencyPropertyChangedEventArgs&) noexcept {
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
            viewer.HorizontalScrollBarVisibility()));
    static_cast<void>(
        viewer.SetVerticalScrollBarVisibility(
            viewer.VerticalScrollBarVisibility()));
}

bool ValidateNormalizedDouble(
    const double& value) noexcept {
    return std::isfinite(value) &&
        value >= 0.0 && value <= 1.0;
}

Base::Result<Base::Ref<Base::Object>> CoerceSelectedObject(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const Base::Ref<Base::Object>& value) noexcept {
    const auto& selector =
        static_cast<const Selector&>(object);
    if (value &&
        selector.IndexOfItem(value.Get()) == UINT32_MAX) {
        return Base::Ref<Base::Object>{};
    }
    return value;
}

Base::Result<bool> CoerceButtonEnabled(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const bool& value) noexcept {
    return value &&
        static_cast<ButtonBase&>(object).IsCommandEnabled();
}

Base::Result<Base::String> CoerceTextBoxText(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const Base::String& value) noexcept {
    Text::EditableTextModel validation;
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
    Text::EditableTextModel validation;
    Base::Result<void> text =
        validation.SetText(value.View());
    return text &&
        validation.GraphemeCount() == 1U;
}

Base::Result<double> CoerceRangeMinimum(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const double& value) noexcept {
    const double maximum =
        static_cast<RangeBase&>(object).Maximum();
    return std::min(value, maximum);
}

Base::Result<double> CoerceRangeMaximum(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const double& value) noexcept {
    const double minimum =
        static_cast<RangeBase&>(object).Minimum();
    return std::max(value, minimum);
}

Base::Result<double> CoerceRangeValue(
    Core::DependencyObject& object,
    const Core::DependencyProperty&,
    const double& value) noexcept {
    const auto& range =
        static_cast<const RangeBase&>(object);
    return std::clamp(
        value,
        range.Minimum(),
        range.Maximum());
}

Base::Result<void> SetPanelContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Panel content child is null");
    }
    return Detail::PanelAccess::Add(
        static_cast<Panel&>(owner), child, *static_cast<Aero::UIElement*>(child.Get()));
}

Base::Result<void> ClearPanelContent(
    Base::Object& owner,
    void*) noexcept {
    return Detail::PanelAccess::Clear(static_cast<Panel&>(owner));
}

Base::Result<void> SetDecoratorContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Decorator content child is null");
    }
    return Detail::DecoratorAccess::SetOwnedChild(
        static_cast<Decorator&>(owner), child, *static_cast<Aero::UIElement*>(child.Get()));
}

Base::Result<void> ClearDecoratorContent(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<Decorator&>(owner).SetChild(nullptr);
}

Base::Result<void> SetContentControlContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ContentControl content child is null");
    }
    return Detail::ContentControlAccess::SetContentValue(
        static_cast<ContentControl&>(owner), child);
}

Base::Result<void> ClearContentControlContent(
    Base::Object& owner,
    void*) noexcept {
    return Detail::ContentControlAccess::SetContentValue(
        static_cast<ContentControl&>(owner), Core::Value::NullObject(Core::TypeOf<Base::Object>()));
}

Base::Result<void> SetContentPresenterContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ContentPresenter content child is null");
    }
    return static_cast<ContentPresenter&>(owner).SetOwnedContent(
        child, *static_cast<Aero::UIElement*>(child.Get()));
}

Base::Result<void> ClearContentPresenterContent(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<ContentPresenter&>(owner).SetContent(nullptr);
}

Base::Result<void> AddTextBlockInline(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextBlock inline child is null");
    }
    auto& text = static_cast<TextBlock&>(owner);
    if (!text.PropertyRegistry().Types().IsDerivedFrom(
            child->RuntimeType(),
            Aero::UIElement::StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextBlock inline content must be a UIElement");
    }
    return text.AddOwnedInline(
        child,
        *static_cast<Aero::UIElement*>(
            child.Get()));
}

Base::Result<void> ClearTextBlockInlines(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<TextBlock&>(owner)
        .ClearOwnedInlines();
}

Base::Result<void> AddItemsControlItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& item,
    void*) noexcept {
    if (!item) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ItemsControl content item is null");
    }
    return static_cast<ItemsControl&>(owner).GetItems().Add(item);
}

Base::Result<void> ClearItemsControlItems(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ItemsControl&>(owner).GetItems().Reset();
    return {};
}

void OnItemsSourceChanged(
    Core::DependencyObject& object,
    const Base::Ref<Base::Object>&,
    const Base::Ref<Base::Object>& value) noexcept {
    Core::IItemsSource* source = nullptr;
    if (value) {
        if (value->RuntimeType() ==
            ObjectItemsSource::StaticTypeId()) {
            source = static_cast<ObjectItemsSource*>(value.Get());
        } else if (value->RuntimeType() ==
                   Media::GradientStopCollection::StaticTypeId()) {
            source = static_cast<Media::GradientStopCollection*>(
                value.Get());
        }
    }
    static_cast<void>(
        static_cast<ItemsControl&>(object)
            .SetItemsSource(source));
}

void OnItemTemplateChanged(
    Core::DependencyObject& object,
    const Base::Ref<DataTemplate>&,
    const Base::Ref<DataTemplate>& value) noexcept {
    static_cast<ItemsControl&>(object)
        .SetItemTemplate(value.Get());
}

void OnItemsPanelChanged(
    Core::DependencyObject& object,
    const Base::Ref<ItemsPanelTemplate>&,
    const Base::Ref<ItemsPanelTemplate>& value) noexcept {
    static_cast<ItemsControl&>(object)
        .SetItemsPanel(value.Get());
}

void OnItemContainerStyleChanged(
    Core::DependencyObject& object,
    const Base::Ref<Style>&,
    const Base::Ref<Style>& value) noexcept {
    static_cast<ItemsControl&>(object)
        .SetItemContainerStyle(value.Get());
}

Base::Result<void> AddTreeViewItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& item,
    void*) noexcept {
    if (!item) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TreeViewItem content item is null");
    }
    return static_cast<TreeViewItem&>(
        owner).GetItems().Add(item);
}

Base::Result<void> ClearTreeViewItems(
    Base::Object& owner,
    void*) noexcept {
    static_cast<TreeViewItem&>(
        owner).GetItems().Reset();
    return {};
}

Base::Result<void> AddGridViewColumn(
    Base::Object& owner,
    const Base::Ref<Base::Object>& item,
    void*) noexcept {
    if (!item ||
        item->RuntimeType() !=
            GridViewColumn::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "GridView content must be a GridViewColumn");
    }
    return static_cast<GridView&>(
        owner).AddColumn(
            Base::Ref<GridViewColumn>::
                FromBorrowed(
                    static_cast<GridViewColumn&>(
                        *item)));
}

Base::Result<void> ClearGridViewColumns(
    Base::Object& owner,
    void*) noexcept {
    static_cast<GridView&>(
        owner).ClearColumns();
    return {};
}

Base::Result<void> AddTabControlItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& item,
    void*) noexcept {
    if (!item) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TabControl item is null");
    }
    return static_cast<TabControl&>(owner).
        AddOwnedTab(
            Base::Ref<TabItem>::FromBorrowed(
                static_cast<TabItem&>(*item)));
}

Base::Result<void> ClearTabControlItems(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<TabControl&>(owner).
        ClearOwnedTabs();
}
