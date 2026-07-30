// Shared implementation helpers for the semantic metadata units.
constexpr double DefaultMaximum = 1.0e12;

Base::Result<Base::Ref<Base::Object>>
CreateFrameworkElementPlaceholder() noexcept {
    Base::Result<Base::Ref<FrameworkElement>> created =
        Base::MakeRef<FrameworkElement>(
            FrameworkElement::StaticTypeId());
    return created
        ? Base::Result<Base::Ref<Base::Object>>(
            Base::Ref<Base::Object>(
                std::move(created).Value()))
        : Base::Result<Base::Ref<Base::Object>>(
            created.GetStatus());
}

Base::Result<Value> ConvertRoutedCommandReference(
    TypeId targetType,
    Base::StringView text,
    void*) noexcept {
    const Base::StringView name =
        Core::ValueConversion::Trim(text);
    if (targetType != ICommand::StaticTypeId() ||
        name.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Command reference requires a non-empty routed command name");
    }
    Base::Result<Base::Ref<RoutedCommand>> command =
        Base::MakeRef<RoutedCommand>(name);
    if (!command) return command.GetStatus();
    return Value::FromObject(
        targetType,
        Base::Ref<Base::Object>(
            std::move(command).Value()));
}

bool ValidateUnitDouble(
    const double& value) noexcept {
    return std::isfinite(value) &&
        value >= 0.0 && value <= 1.0;
}

Base::Result<Length> ConvertLength(
    Base::StringView text) noexcept {
    const Base::StringView value = ValueConversion::Trim(text);
    Length length = Length::Auto();
    if (!ValueConversion::EqualsAsciiInsensitive(value, "auto")) {
        Base::Result<double> parsed =
            ValueConversion::ParseDouble(value);
        if (!parsed || parsed.Value() < 0.0) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
                "Length must be Auto or a nonnegative number");
        }
        length = Length::Pixels(parsed.Value());
    }
    return length;
}

Base::Result<Thickness> ParseThickness(Base::StringView input) noexcept {
    Base::String text;
    Base::Result<void> assigned = text.TryAssign(input);
    if (!assigned) return assigned.GetStatus();
    const char* cursor = text.CStr();
    double values[4]{};
    std::uint32_t count = 0U;
    bool valid = true;
    while (*cursor != '\0') {
        while (std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
        if (*cursor == '\0') break;
        if (count == 4U) {
            valid = false;
            break;
        }
        char* end = nullptr;
        values[count] = std::strtod(cursor, &end);
        if (end == cursor || !std::isfinite(values[count])) {
            valid = false;
            break;
        }
        ++count;
        cursor = end;
        const char* whitespace = cursor;
        while (std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
        if (*cursor == '\0') break;
        if (*cursor == ',') {
            ++cursor;
            while (std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
            if (*cursor == '\0') {
                valid = false;
                break;
            }
        } else if (cursor == whitespace) {
            valid = false;
            break;
        }
    }
    Thickness result;
    if (!valid) return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
        "Thickness contains invalid text");
    if (count == 1U) result = {values[0], values[0], values[0], values[0]};
    else if (count == 2U) result = {values[0], values[1], values[0], values[1]};
    else if (count == 4U) result = {values[0], values[1], values[2], values[3]};
    else return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
        "Thickness accepts one, two, or four numbers");
    return result;
}

Base::Result<Thickness> ConvertThickness(
    Base::StringView text) noexcept {
    return ParseThickness(text);
}

Base::Result<CornerRadius> ConvertCornerRadius(
    Base::StringView text) noexcept {
    Base::Result<Thickness> parsed =
        ParseThickness(text);
    if (!parsed) return parsed.GetStatus();
    const Thickness& values = parsed.Value();
    return CornerRadius{
        values.left,
        values.top,
        values.right,
        values.bottom};
}

Base::Result<Point> ConvertPoint(
    Base::StringView input) noexcept {
    Base::String text;
    Base::Result<void> assigned = text.TryAssign(input);
    if (!assigned) return assigned.GetStatus();
    const char* cursor = text.CStr();
    char* end = nullptr;
    const double x = std::strtod(cursor, &end);
    if (end == cursor || !std::isfinite(x)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Point requires two finite coordinates");
    }
    cursor = end;
    while (*cursor == ' ' || *cursor == ',') ++cursor;
    const double y = std::strtod(cursor, &end);
    if (end == cursor || !std::isfinite(y)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Point requires two finite coordinates");
    }
    cursor = end;
    while (*cursor == ' ' || *cursor == ',') ++cursor;
    if (*cursor != '\0') {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Point contains trailing text");
    }
    return Point{x, y};
}

Base::Result<Rect> ConvertRect(
    Base::StringView input) noexcept {
    Base::String text;
    Base::Result<void> assigned =
        text.TryAssign(input);
    if (!assigned) return assigned.GetStatus();
    const char* cursor = text.CStr();
    double values[4]{};
    for (std::uint32_t index = 0U;
         index < 4U; ++index) {
        while (*cursor == ' ' ||
               *cursor == ',') {
            ++cursor;
        }
        char* end = nullptr;
        values[index] =
            std::strtod(cursor, &end);
        if (end == cursor ||
            !std::isfinite(values[index])) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Rect requires four finite values");
        }
        cursor = end;
    }
    while (std::isspace(
        static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
    if (*cursor != '\0' ||
        values[2] < 0.0 ||
        values[3] < 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Rect requires x,y,width,height with non-negative size");
    }
    return Rect{
        values[0], values[1],
        values[2], values[3]};
}

Base::Result<Transform2D> ConvertMatrix(
    Base::StringView input) noexcept {
    Base::String text;
    Base::Result<void> assigned = text.TryAssign(input);
    if (!assigned) return assigned.GetStatus();
    const char* cursor = text.CStr();
    double values[6]{};
    for (std::uint32_t index = 0U; index < 6U; ++index) {
        while (*cursor == ' ' || *cursor == ',') ++cursor;
        char* end = nullptr;
        values[index] = std::strtod(cursor, &end);
        if (end == cursor || !std::isfinite(values[index])) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Matrix requires six finite values");
        }
        cursor = end;
    }
    while (*cursor == ' ' || *cursor == ',') ++cursor;
    if (*cursor != '\0') {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Matrix contains trailing text");
    }
    return Transform2D{
        values[0], values[1], values[2],
        values[3], values[4], values[5]};
}

int Hex(char value) noexcept {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

Base::Result<Color> ConvertColor(
    Base::StringView text) noexcept {
    const Base::StringView value = ValueConversion::Trim(text);
    struct NamedColor final {
        Base::StringView name;
        std::uint8_t red;
        std::uint8_t green;
        std::uint8_t blue;
        std::uint8_t alpha;
    };
    static constexpr NamedColor named[] = {
        {"Aquamarine", 127, 255, 212, 255},
        {"Black", 0, 0, 0, 255},
        {"Blue", 0, 0, 255, 255},
        {"CadetBlue", 95, 158, 160, 255},
        {"Cyan", 0, 255, 255, 255},
        {"DodgerBlue", 30, 144, 255, 255},
        {"Gold", 255, 215, 0, 255},
        {"Gray", 128, 128, 128, 255},
        {"Green", 0, 128, 0, 255},
        {"GreenYellow", 173, 255, 47, 255},
        {"LightGray", 211, 211, 211, 255},
        {"LightSeaGreen", 32, 178, 170, 255},
        {"Lime", 0, 255, 0, 255},
        {"Magenta", 255, 0, 255, 255},
        {"Moccasin", 255, 228, 181, 255},
        {"Orange", 255, 165, 0, 255},
        {"OrangeRed", 255, 69, 0, 255},
        {"PaleTurquoise", 175, 238, 238, 255},
        {"Purple", 128, 0, 128, 255},
        {"Red", 255, 0, 0, 255},
        {"Salmon", 250, 128, 114, 255},
        {"Silver", 192, 192, 192, 255},
        {"Teal", 0, 128, 128, 255},
        {"Thistle", 216, 191, 216, 255},
        {"Transparent", 255, 255, 255, 0},
        {"Turquoise", 64, 224, 208, 255},
        {"White", 255, 255, 255, 255},
        {"WhiteSmoke", 245, 245, 245, 255},
        {"Yellow", 255, 255, 0, 255},
        {"YellowGreen", 154, 205, 50, 255}};
    for (const NamedColor& candidate : named) {
        if (!ValueConversion::EqualsAsciiInsensitive(
                value, candidate.name)) {
            continue;
        }
        return Color{
            candidate.red / 255.0F,
            candidate.green / 255.0F,
            candidate.blue / 255.0F,
            candidate.alpha / 255.0F};
    }
    if ((value.SizeBytes() != 7U && value.SizeBytes() != 9U) || value[0] != '#') {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "Color requires #RRGGBB or #AARRGGBB");
    }
    std::uint8_t bytes[4]{255U, 0U, 0U, 0U};
    const std::uint32_t count = value.SizeBytes() == 9U ? 4U : 3U;
    for (std::uint32_t index = 0U; index < count; ++index) {
        const int high = Hex(value[1U + index * 2U]);
        const int low = Hex(value[2U + index * 2U]);
        if (high < 0 || low < 0) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
                "Color contains a non-hex digit");
        }
        bytes[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    Color color = count == 3U
        ? Color{bytes[0] / 255.0F, bytes[1] / 255.0F, bytes[2] / 255.0F, 1.0F}
        : Color{bytes[1] / 255.0F, bytes[2] / 255.0F,
            bytes[3] / 255.0F, bytes[0] / 255.0F};
    return color;
}

bool EqualLength(const void* left, const void* right, void*) noexcept {
    const Length& a = *static_cast<const Length*>(left);
    const Length& b = *static_cast<const Length*>(right);
    return a.isAuto == b.isAuto && (a.isAuto || a.value == b.value);
}
bool EqualThickness(const void* left, const void* right, void*) noexcept {
    const Thickness& a = *static_cast<const Thickness*>(left);
    const Thickness& b = *static_cast<const Thickness*>(right);
    return a.left == b.left && a.top == b.top &&
        a.right == b.right && a.bottom == b.bottom;
}
bool EqualColor(const void* left, const void* right, void*) noexcept {
    const Color& a = *static_cast<const Color*>(left);
    const Color& b = *static_cast<const Color*>(right);
    return a.red == b.red && a.green == b.green &&
        a.blue == b.blue && a.alpha == b.alpha;
}
bool EqualCornerRadius(
    const void* left,
    const void* right,
    void*) noexcept {
    const CornerRadius& a =
        *static_cast<const CornerRadius*>(left);
    const CornerRadius& b =
        *static_cast<const CornerRadius*>(right);
    return a.topLeft == b.topLeft &&
        a.topRight == b.topRight &&
        a.bottomRight == b.bottomRight &&
        a.bottomLeft == b.bottomLeft;
}
bool ValidateLength(const Length& length) noexcept {
    return length.isAuto || (std::isfinite(length.value) && length.value >= 0.0);
}
bool ValidateThicknessValue(const Thickness& t) noexcept {
    return IsFinite(t) && t.left >= 0.0 && t.top >= 0.0 &&
        t.right >= 0.0 && t.bottom >= 0.0;
}
bool ValidateMarginValue(const Thickness& t) noexcept {
    // WPF permits negative margins for overlap and shared-border layouts.
    return IsFinite(t);
}
template<class TProperty>
Base::Result<double> CheckMinimum(
    DependencyObject& object,
    const double& value,
    const TProperty& maximum) noexcept {
    Base::Result<double> other = object.GetValue(maximum);
    if (!other || value > other.Value()) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "Minimum layout size exceeds maximum layout size");
    }
    return value;
}
template<class TProperty>
Base::Result<double> CheckMaximum(
    DependencyObject& object,
    const double& value,
    const TProperty& minimum) noexcept {
    Base::Result<double> other = object.GetValue(minimum);
    if (!other || value < other.Value()) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "Maximum layout size is below minimum layout size");
    }
    return value;
}
Base::Result<double> CoerceMinWidth(
    DependencyObject& object,
    const DependencyProperty&,
    const double& value) noexcept {
    return CheckMinimum(
        object, value, FrameworkElement::MaxWidthProperty);
}
Base::Result<double> CoerceMaxWidth(
    DependencyObject& object,
    const DependencyProperty&,
    const double& value) noexcept {
    return CheckMaximum(
        object, value, FrameworkElement::MinWidthProperty);
}
Base::Result<double> CoerceMinHeight(
    DependencyObject& object,
    const DependencyProperty&,
    const double& value) noexcept {
    return CheckMinimum(
        object, value, FrameworkElement::MaxHeightProperty);
}
Base::Result<double> CoerceMaxHeight(
    DependencyObject& object,
    const DependencyProperty&,
    const double& value) noexcept {
    return CheckMaximum(
        object, value, FrameworkElement::MinHeightProperty);
}

TypeReference GetStyleTargetType(
    const Style& style) noexcept {
    return {style.TargetType()};
}

Base::Result<void> SetStyleTargetType(
    Style& style,
    TypeReference value) noexcept {
    return style.TrySetTargetType(value.type);
}

Base::Result<void> SetStyleBasedOn(
    Style& style,
    Base::Ref<Style> value) noexcept {
    return style.TrySetBasedOn(
        Base::Ref<Base::Object>(
            std::move(value)));
}

Base::Result<void> AddMergedDictionary(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    return static_cast<ResourceDictionary&>(owner)
        .TryAddMerged(
            static_cast<ResourceDictionary&>(*value));
}

Base::Result<void> ClearMergedDictionaries(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<ResourceDictionary&>(owner)
        .ClearMergedDictionaries();
}

Base::Result<void> AddStyleSetter(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Setter> retained =
        Base::Ref<Setter>::TryFromBorrowed(
            static_cast<Setter&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Style Setter cannot be retained");
    }
    return static_cast<Style&>(owner)
        .TryAddAuthoredSetter(
            std::move(retained));
}

Base::Result<void> ClearStyleSetters(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<Style&>(owner)
        .ClearAuthoredSetters();
}

Base::Result<void> AddStyleTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<PropertyTrigger> retained =
        Base::Ref<PropertyTrigger>::TryFromBorrowed(
            static_cast<PropertyTrigger&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Style Trigger cannot be retained");
    }
    return static_cast<Style&>(owner)
        .TryAddAuthoredTrigger(
            std::move(retained));
}

Base::Result<void> ClearStyleTriggers(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<Style&>(owner)
        .ClearAuthoredTriggers();
}

Base::Result<void> AddTriggerSetter(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Setter> retained =
        Base::Ref<Setter>::TryFromBorrowed(
            static_cast<Setter&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Trigger Setter cannot be retained");
    }
    return static_cast<PropertyTrigger&>(owner)
        .TryAddAuthoredSetter(
            std::move(retained));
}

Base::Result<void> ClearTriggerSetters(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<PropertyTrigger&>(owner)
        .ClearAuthoredSetters();
}

Base::Result<void> AddTriggerEnterAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    return static_cast<TriggerBase&>(owner)
        .TryAddEnterAction(value);
}

Base::Result<void> ClearTriggerEnterActions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<TriggerBase&>(owner)
        .ClearEnterActions();
    return {};
}

Base::Result<void> AddTriggerExitAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    return static_cast<TriggerBase&>(owner)
        .TryAddExitAction(value);
}

Base::Result<void> ClearTriggerExitActions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<TriggerBase&>(owner)
        .ClearExitActions();
    return {};
}

Base::Result<void> AddDataTriggerSetter(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Setter> retained =
        Base::Ref<Setter>::TryFromBorrowed(
            static_cast<Setter&>(*value));
    return retained
        ? static_cast<DataTrigger&>(owner)
              .TryAddAuthoredSetter(
                  std::move(retained))
        : Base::Result<void>(
              Base::Status::Failure(
                  Base::ErrorCode::InvalidArgument,
                  "DataTrigger Setter cannot be retained"));
}

Base::Result<void> ClearDataTriggerSetters(
    Base::Object& owner,
    void*) noexcept {
    static_cast<DataTrigger&>(owner)
        .ClearAuthoredSetters();
    return {};
}

Base::Result<void> AddMultiDataCondition(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Condition> retained =
        Base::Ref<Condition>::TryFromBorrowed(
            static_cast<Condition&>(*value));
    return retained
        ? static_cast<MultiDataTrigger&>(owner)
              .TryAddCondition(
                  std::move(retained))
        : Base::Result<void>(
              Base::Status::Failure(
                  Base::ErrorCode::InvalidArgument,
                  "MultiDataTrigger Condition cannot be retained"));
}

Base::Result<void> ClearMultiDataConditions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<MultiDataTrigger&>(owner)
        .ClearConditions();
    return {};
}

Base::Result<void> AddMultiDataSetter(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Setter> retained =
        Base::Ref<Setter>::TryFromBorrowed(
            static_cast<Setter&>(*value));
    return retained
        ? static_cast<MultiDataTrigger&>(owner)
              .TryAddAuthoredSetter(
                  std::move(retained))
        : Base::Result<void>(
              Base::Status::Failure(
                  Base::ErrorCode::InvalidArgument,
                  "MultiDataTrigger Setter cannot be retained"));
}

Base::Result<void> ClearMultiDataSetters(
    Base::Object& owner,
    void*) noexcept {
    static_cast<MultiDataTrigger&>(owner)
        .ClearAuthoredSetters();
    return {};
}

Base::Result<void> AddMultiTriggerCondition(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Condition> retained =
        Base::Ref<Condition>::TryFromBorrowed(
            static_cast<Condition&>(*value));
    return retained
        ? static_cast<MultiTrigger&>(owner)
              .TryAddCondition(std::move(retained))
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::InvalidArgument,
              "MultiTrigger Condition cannot be retained"));
}

Base::Result<void> ClearMultiTriggerConditions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<MultiTrigger&>(owner).ClearConditions();
    return {};
}

Base::Result<void> AddMultiTriggerSetter(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Setter> retained =
        Base::Ref<Setter>::TryFromBorrowed(
            static_cast<Setter&>(*value));
    return retained
        ? static_cast<MultiTrigger&>(owner)
              .TryAddAuthoredSetter(std::move(retained))
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::InvalidArgument,
              "MultiTrigger Setter cannot be retained"));
}

Base::Result<void> ClearMultiTriggerSetters(
    Base::Object& owner,
    void*) noexcept {
    static_cast<MultiTrigger&>(owner).ClearAuthoredSetters();
    return {};
}

Base::Result<void> AddFrameworkEventTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Animation::EventTrigger> retained =
        Base::Ref<Animation::EventTrigger>::TryFromBorrowed(
            static_cast<Animation::EventTrigger&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "FrameworkElement EventTrigger cannot be retained");
    }
    return static_cast<FrameworkElement&>(owner)
        .TryAddAuthoredTrigger(
            Base::Ref<Base::Object>(std::move(retained)));
}

Base::Result<void> ClearFrameworkEventTriggers(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<FrameworkElement&>(owner)
        .ClearAuthoredTriggers();
}

Base::Result<void> AddStoryboardTimeline(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Animation::Timeline> retained =
        Base::Ref<Animation::Timeline>::TryFromBorrowed(
            static_cast<Animation::Timeline&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Storyboard Timeline cannot be retained");
    }
    return static_cast<Animation::Storyboard&>(owner)
        .TryAddTimeline(std::move(retained));
}

Base::Result<void> ClearStoryboardTimelines(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<Animation::Storyboard&>(owner)
        .ClearTimelines();
}

Base::Result<void> AddGradientStop(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "GradientStop content cannot be null");
    }
    Base::Ref<GradientStop> retained =
        Base::Ref<GradientStop>::TryFromBorrowed(
            static_cast<GradientStop&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "GradientStop content cannot be retained");
    }
    return static_cast<GradientBrush&>(owner)
        .AddGradientStop(std::move(retained));
}

Base::Result<void> ClearGradientStops(
    Base::Object& owner,
    void*) noexcept {
    static_cast<GradientBrush&>(owner)
        .ClearGradientStops();
    return {};
}

Base::Result<void> AddGradientStopCollectionItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            GradientStop::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "GradientStopCollection content must be a GradientStop");
    }
    return static_cast<GradientStopCollection&>(owner)
        .Add(Base::Ref<GradientStop>::FromBorrowed(
            static_cast<GradientStop&>(*value)));
}

Base::Result<void> ClearGradientStopCollectionItems(
    Base::Object& owner,
    void*) noexcept {
    static_cast<GradientStopCollection&>(owner).Clear();
    return {};
}

Base::Result<Value> ConvertBrushText(
    TypeId targetType,
    Base::StringView text,
    void* context) noexcept {
    if (targetType != Brush::StaticTypeId() ||
        context == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Brush text conversion received invalid metadata");
    }
    MetadataRegistrationValues values =
        Core::Detail::MakeRegistrationValues(
            context);
    Base::Result<Value> converted =
        values.TryConvertText(
            Core::TypeOf<Color>(), text);
    if (!converted) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Brush text could not be converted to Color");
    }
    Base::Result<Color> color =
        ValueCodec<Color>::Decode(
            values, converted.Value());
    if (!color) return color.GetStatus();
    Base::Result<Base::Ref<SolidColorBrush>> made =
        Base::MakeRef<SolidColorBrush>();
    if (!made) return made.GetStatus();
    Base::Result<void> assigned =
        made.Value()->SetColor(color.Value());
    if (!assigned) return assigned.GetStatus();
    return Value::FromObject(
        Brush::StaticTypeId(),
        Base::Ref<Base::Object>(
            made.Value()));
}

Base::Result<Value> ConvertGeometryText(
    TypeId targetType,
    Base::StringView text,
    void*) noexcept {
    if (targetType != Geometry::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Geometry text conversion received an invalid target");
    }
    Base::Result<Base::Ref<Geometry>> geometry =
        Base::MakeRef<Geometry>();
    if (!geometry) return geometry.GetStatus();
    Base::Result<void> assigned =
        geometry.Value()->SetValue(text);
    if (!assigned) return assigned.GetStatus();
    return Value::FromObject(
        Geometry::StaticTypeId(),
        Base::Ref<Base::Object>(
            geometry.Value()));
}

Base::Result<Value> ConvertFontFamilyText(
    TypeId targetType, Base::StringView text, void*) noexcept {
    if (targetType != FontFamily::StaticTypeId()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "FontFamily text conversion received an invalid target");
    }
    Base::Result<Base::Ref<FontFamily>> family = Base::MakeRef<FontFamily>();
    if (!family) return family.GetStatus();
    Base::Result<void> assigned = family.Value()->SetSource(text);
    if (!assigned) return assigned.GetStatus();
    return Value::FromObject(FontFamily::StaticTypeId(),
        Base::Ref<Base::Object>(std::move(family).Value()));
}

Base::Result<Value> ConvertImageSourceText(
    TypeId targetType,
    Base::StringView text,
    void*) noexcept {
    if (targetType !=
        ImageSource::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ImageSource text conversion received an invalid target");
    }
    Base::Result<Base::ResourceUri> uri =
        Base::ResourceUri::Parse(text);
    if (!uri || uri.Value().Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "ImageSource requires a non-empty resource URI");
    }
    Base::Result<Base::Ref<BitmapImage>> image =
        Base::MakeRef<BitmapImage>();
    if (!image) return image.GetStatus();
    Base::Result<void> assigned =
        image.Value()->SetUriSource(
            uri.Value());
    if (!assigned) return assigned.GetStatus();
    return Value::FromObject(
        ImageSource::StaticTypeId(),
        Base::Ref<Base::Object>(
            image.Value()));
}

Base::Result<void> AddDoubleKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Animation::DoubleKeyFrame> retained =
        Base::Ref<Animation::DoubleKeyFrame>::TryFromBorrowed(
            static_cast<Animation::DoubleKeyFrame&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Double key frame cannot be retained");
    }
    return static_cast<
        Animation::DoubleAnimationUsingKeyFrames&>(owner)
            .TryAddKeyFrame(std::move(retained));
}

Base::Result<void> ClearDoubleKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<
        Animation::DoubleAnimationUsingKeyFrames&>(owner)
            .ClearKeyFrames();
}

Base::Result<void> AddThicknessKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Animation::ThicknessKeyFrame> retained =
        Base::Ref<Animation::ThicknessKeyFrame>::
            TryFromBorrowed(
                static_cast<
                    Animation::ThicknessKeyFrame&>(
                        *value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Thickness key frame cannot be retained");
    }
    return static_cast<
        Animation::ThicknessAnimationUsingKeyFrames&>(
            owner)
        .TryAddKeyFrame(std::move(retained));
}

Base::Result<void> ClearThicknessKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<
        Animation::ThicknessAnimationUsingKeyFrames&>(
            owner)
        .ClearKeyFrames();
}

Base::Result<void> AddColorKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Animation::ColorKeyFrame> retained =
        Base::Ref<Animation::ColorKeyFrame>::TryFromBorrowed(
            static_cast<Animation::ColorKeyFrame&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Color key frame cannot be retained");
    }
    return static_cast<
        Animation::ColorAnimationUsingKeyFrames&>(owner)
            .TryAddKeyFrame(std::move(retained));
}

Base::Result<void> ClearColorKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<
        Animation::ColorAnimationUsingKeyFrames&>(owner)
            .ClearKeyFrames();
}

Base::Result<void> AddObjectKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Animation::DiscreteObjectKeyFrame> retained =
        Base::Ref<Animation::DiscreteObjectKeyFrame>::TryFromBorrowed(
            static_cast<Animation::DiscreteObjectKeyFrame&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Object key frame cannot be retained");
    }
    return static_cast<
        Animation::ObjectAnimationUsingKeyFrames&>(owner)
            .TryAddKeyFrame(std::move(retained));
}

Base::Result<void> ClearObjectKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<
        Animation::ObjectAnimationUsingKeyFrames&>(owner)
            .ClearKeyFrames();
}

Base::Result<void> AddBooleanKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Animation::DiscreteBooleanKeyFrame> retained =
        Base::Ref<Animation::DiscreteBooleanKeyFrame>::TryFromBorrowed(
            static_cast<Animation::DiscreteBooleanKeyFrame&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Boolean key frame cannot be retained");
    }
    return static_cast<
        Animation::BooleanAnimationUsingKeyFrames&>(owner)
            .TryAddKeyFrame(std::move(retained));
}

Base::Result<void> ClearBooleanKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<
        Animation::BooleanAnimationUsingKeyFrames&>(owner)
            .ClearKeyFrames();
}

Base::Result<void> AddEventTriggerAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Animation::TriggerAction> retained =
        Base::Ref<Animation::TriggerAction>::TryFromBorrowed(
            static_cast<Animation::TriggerAction&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "EventTrigger action cannot be retained");
    }
    return static_cast<Animation::EventTrigger&>(owner)
        .TryAddAction(std::move(retained));
}

Base::Result<void> ClearEventTriggerActions(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<Animation::EventTrigger&>(owner)
        .ClearActions();
}

Base::Result<void> AddEventTriggerBehavior(
    Base::Object& owner, const Base::Ref<Base::Object>& value, void*) noexcept {
    return static_cast<Animation::EventTrigger&>(owner).TryAddConditionBehavior(value);
}
Base::Result<void> ClearEventTriggerBehaviors(Base::Object& owner, void*) noexcept {
    static_cast<Animation::EventTrigger&>(owner).ClearConditionBehaviors(); return {};
}
Base::Result<void> AddConditionalComparison(
    Base::Object& owner, const Base::Ref<Base::Object>& value, void*) noexcept {
    return static_cast<Animation::ConditionalExpression&>(owner).AddCondition(
        Base::Ref<Animation::ComparisonCondition>::FromBorrowed(
            static_cast<Animation::ComparisonCondition&>(*value)));
}
Base::Result<void> ClearConditionalComparisons(Base::Object& owner, void*) noexcept {
    static_cast<Animation::ConditionalExpression&>(owner).ClearConditions(); return {};
}
Base::Result<void> SetConditionBehaviorExpression(
    Base::Object& owner, const Base::Ref<Base::Object>& value, void*) noexcept {
    return static_cast<Animation::ConditionBehavior&>(owner).SetExpression(
        Base::Ref<Animation::ConditionalExpression>::FromBorrowed(
            static_cast<Animation::ConditionalExpression&>(*value)));
}
Base::Result<void> ClearConditionBehaviorExpression(Base::Object& owner, void*) noexcept {
    return static_cast<Animation::ConditionBehavior&>(owner).SetExpression({});
}

Base::Result<void> AddStoryboardCompletedTriggerAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Animation::TriggerAction> retained =
        Base::Ref<Animation::TriggerAction>::TryFromBorrowed(
            static_cast<Animation::TriggerAction&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "StoryboardCompletedTrigger action cannot be retained");
    }
    return static_cast<
        Animation::StoryboardCompletedTrigger&>(owner)
            .TryAddAction(std::move(retained));
}

Base::Result<void> ClearStoryboardCompletedTriggerActions(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<
        Animation::StoryboardCompletedTrigger&>(owner)
            .ClearActions();
}

Base::Result<void> AddInteractionTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Interaction trigger cannot be null");
    }
    return static_cast<FrameworkElement&>(owner)
        .TryAddAuthoredTrigger(value);
}

Base::Result<void> ClearInteractionTriggers(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<FrameworkElement&>(owner)
        .ClearAuthoredTriggers();
}

Base::Result<void> SetBeginStoryboardContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Animation::Storyboard> retained =
        Base::Ref<Animation::Storyboard>::TryFromBorrowed(
            static_cast<Animation::Storyboard&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "BeginStoryboard Storyboard cannot be retained");
    }
    return static_cast<Animation::BeginStoryboard&>(owner)
        .SetStoryboard(std::move(retained));
}

Base::Result<void> ClearBeginStoryboardContent(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<Animation::BeginStoryboard&>(owner)
        .SetStoryboard({});
}

Base::Result<void> AddTransformGroupChild(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Transform> retained =
        Base::Ref<Transform>::TryFromBorrowed(
            static_cast<Transform&>(*value));
    if (!retained) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TransformGroup child cannot be retained");
    }
    return static_cast<TransformGroup&>(owner)
        .TryAddChild(std::move(retained));
}

Base::Result<void> ClearTransformGroupChildren(
    Base::Object& owner,
    void*) noexcept {
    return static_cast<TransformGroup&>(owner)
        .ClearChildren();
}

void OnRenderTransformChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    FrameworkElement* owner =
        static_cast<UIElement&>(object).AsFrameworkElement();
    if (owner == nullptr) return;
    Base::Result<Base::Ref<Transform>> oldTransform =
        ValueCodec<Base::Ref<Transform>>::Decode(args.oldValue);
    if (oldTransform && oldTransform.Value() &&
        oldTransform.Value()->Owner() == owner) {
        oldTransform.Value()->DetachOwner(
            owner,
            TransformOwnerRole::Render);
    }
    Base::Result<Base::Ref<Transform>> newTransform =
        ValueCodec<Base::Ref<Transform>>::Decode(args.newValue);
    if (newTransform && newTransform.Value()) {
        newTransform.Value()->AttachOwner(
            owner,
            TransformOwnerRole::Render);
    }
}

void OnLayoutTransformChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    FrameworkElement* owner =
        static_cast<UIElement&>(object).
            AsFrameworkElement();
    if (owner == nullptr) return;
    Base::Result<Base::Ref<Transform>> oldTransform =
        ValueCodec<Base::Ref<Transform>>::Decode(
            args.oldValue);
    if (oldTransform &&
        oldTransform.Value() &&
        oldTransform.Value()->Owner() == owner) {
        oldTransform.Value()->DetachOwner(
            owner,
            TransformOwnerRole::Layout);
    }
    Base::Result<Base::Ref<Transform>> newTransform =
        ValueCodec<Base::Ref<Transform>>::Decode(
            args.newValue);
    if (newTransform && newTransform.Value()) {
        newTransform.Value()->AttachOwner(
            owner,
            TransformOwnerRole::Layout);
    }
}

void OnEffectChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    FrameworkElement* owner =
        static_cast<UIElement&>(object).AsFrameworkElement();
    if (owner == nullptr) return;
    Base::Result<Base::Ref<Effect>> oldEffect =
        ValueCodec<Base::Ref<Effect>>::Decode(
            args.oldValue);
    if (oldEffect && oldEffect.Value() &&
        oldEffect.Value()->Owner() == owner) {
        oldEffect.Value()->SetOwner(nullptr);
    }
    Base::Result<Base::Ref<Effect>> newEffect =
        ValueCodec<Base::Ref<Effect>>::Decode(
            args.newValue);
    if (newEffect && newEffect.Value()) {
        newEffect.Value()->SetOwner(owner);
    }
}
