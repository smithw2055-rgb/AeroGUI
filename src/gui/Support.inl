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
        ::Aero::Base::Detail::ValueConversion::Trim(text);
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
    const Base::StringView value = ::Aero::Base::Detail::ValueConversion::Trim(text);
    Length length = Length::Auto();
    if (!::Aero::Base::Detail::ValueConversion::EqualsAsciiInsensitive(value, "auto")) {
        Base::Result<double> parsed =
            ::Aero::Base::Detail::ValueConversion::ParseDouble(value);
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
    Base::Result<void> assigned = text.Assign(input);
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
    Base::Result<void> assigned = text.Assign(input);
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
        text.Assign(input);
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

Base::Result<Base::Transform2D> ConvertMatrix(
    Base::StringView input) noexcept {
    Base::String text;
    Base::Result<void> assigned = text.Assign(input);
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
    return Base::Transform2D{
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
    const Base::StringView value = ::Aero::Base::Detail::ValueConversion::Trim(text);
    struct NamedColor {
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
        {"BurlyWood", 222, 184, 135, 255},
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
        if (!::Aero::Base::Detail::ValueConversion::EqualsAsciiInsensitive(
                value, candidate.name)) {
            continue;
        }
        return Color{
            candidate.red / 255.0F,
            candidate.green / 255.0F,
            candidate.blue / 255.0F,
            candidate.alpha / 255.0F};
    }
    if ((value.SizeBytes() != 4U && value.SizeBytes() != 5U &&
         value.SizeBytes() != 7U && value.SizeBytes() != 9U) ||
        value[0] != '#') {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "Color requires #RGB, #ARGB, #RRGGBB, or #AARRGGBB");
    }
    if (value.SizeBytes() == 4U || value.SizeBytes() == 5U) {
        const bool alpha = value.SizeBytes() == 5U;
        std::uint8_t components[4]{255U, 0U, 0U, 0U};
        const std::uint32_t count = alpha ? 4U : 3U;
        for (std::uint32_t index = 0U; index < count; ++index) {
            const int digit = Hex(value[1U + index]);
            if (digit < 0) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Color contains a non-hex digit");
            }
            components[index] = static_cast<std::uint8_t>(
                (digit << 4) | digit);
        }
        return alpha
            ? Color{
                  components[1] / 255.0F,
                  components[2] / 255.0F,
                  components[3] / 255.0F,
                  components[0] / 255.0F}
            : Color{
                  components[0] / 255.0F,
                  components[1] / 255.0F,
                  components[2] / 255.0F,
                  1.0F};
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
bool ValidateMarginValue(const Thickness& t) noexcept {
    // WPF permits negative margins for overlap and shared-border layouts.
    return IsFinite(t);
}
TypeReference GetStyleTargetType(
    const Style& style) noexcept {
    return {style.GetTargetType()};
}

void SetStyleTargetType(
    Style& style,
    TypeReference value) noexcept {
    // TargetType is authored as a TypeReference by the XAML schema.  Keep the
    // resolved runtime TypeId on the Style so implicit style keys remain
    // distinct (for example Label, ComboBox, and ComboBoxItem).
    (void)style.SetTargetType(value.type);
}

void SetStyleBasedOn(
    Style& style,
    Base::Ref<Style> value) noexcept {
    (void)style.SetBasedOn(Base::Ref<Base::Object>(
        std::move(value)));
}

void AddMergedDictionary(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value ||
        value->RuntimeType() != ResourceDictionary::StaticTypeId()) {
        return;
    }
    static_cast<void>(
        static_cast<ResourceDictionary&>(owner).AddMerged(
            static_cast<ResourceDictionary&>(*value)));
}

void ClearMergedDictionaries(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ResourceDictionary&>(owner).ClearMergedDictionaries();
    return;
}

void AddStyleSetter(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() != Setter::StaticTypeId()) {
        return;
    }
    Base::Ref<Setter> retained =
        Base::Ref<Setter>::TryFromBorrowed(
            static_cast<Setter&>(*value));
    if (!retained) {
        return;
    }
    static_cast<void>(static_cast<Style&>(owner).AddAuthoredSetter(
        std::move(retained)));
}

void ClearStyleSetters(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Style&>(owner).ClearAuthoredSetters();
    return;
}

void AddStyleTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) {
        return;
    }
    Base::Ref<TriggerBase> retained =
        Base::Ref<TriggerBase>::TryFromBorrowed(
            static_cast<TriggerBase&>(*value));
    if (!retained) {
        return;
    }
    static_cast<void>(static_cast<Style&>(owner).AddAuthoredTrigger(
        std::move(retained)));
}

void ClearStyleTriggers(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Style&>(owner).ClearAuthoredTriggers();
    return;
}

void AddTriggerSetter(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() != Setter::StaticTypeId()) {
        return;
    }
    Base::Ref<Setter> retained =
        Base::Ref<Setter>::TryFromBorrowed(
            static_cast<Setter&>(*value));
    if (!retained) {
        return;
    }
    static_cast<void>(static_cast<Trigger&>(owner).AddAuthoredSetter(
        std::move(retained)));
}

void ClearTriggerSetters(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Trigger&>(owner).ClearAuthoredSetters();
    return;
}

void AddTriggerEnterAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    static_cast<void>(
        static_cast<TriggerBase&>(owner).AddEnterAction(value));
}

void ClearTriggerEnterActions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<TriggerBase&>(owner)
        .ClearEnterActions();
    return;
}

void AddTriggerExitAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    static_cast<void>(
        static_cast<TriggerBase&>(owner).AddExitAction(value));
}

void ClearTriggerExitActions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<TriggerBase&>(owner)
        .ClearExitActions();
    return;
}


void AddDataTriggerContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    auto& trigger = static_cast<DataTrigger&>(owner);
    if (value->RuntimeType() == Setter::StaticTypeId()) {
        Base::Ref<Setter> setter = Base::Ref<Setter>::TryFromBorrowed(
            static_cast<Setter&>(*value));
        if (setter) static_cast<void>(trigger.AddAuthoredSetter(std::move(setter)));
        return;
    }
    static_cast<void>(trigger.AddEnterAction(value));
}

void ClearDataTriggerContent(Base::Object& owner, void*) noexcept {
    auto& trigger = static_cast<DataTrigger&>(owner);
    trigger.ClearAuthoredSetters();
    trigger.ClearEnterActions();
}
void AddMultiDataCondition(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Condition> retained =
        Base::Ref<Condition>::TryFromBorrowed(
            static_cast<Condition&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<MultiDataTrigger&>(owner).AddCondition(
            std::move(retained)));
}

void ClearMultiDataConditions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<MultiDataTrigger&>(owner)
        .ClearConditions();
    return;
}

void AddMultiDataSetter(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Setter> retained =
        Base::Ref<Setter>::TryFromBorrowed(
            static_cast<Setter&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<MultiDataTrigger&>(owner).AddAuthoredSetter(
            std::move(retained)));
}

void ClearMultiDataSetters(
    Base::Object& owner,
    void*) noexcept {
    static_cast<MultiDataTrigger&>(owner)
        .ClearAuthoredSetters();
    return;
}

void AddMultiTriggerCondition(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Condition> retained =
        Base::Ref<Condition>::TryFromBorrowed(
            static_cast<Condition&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<MultiTrigger&>(owner).AddCondition(
            std::move(retained)));
}

void ClearMultiTriggerConditions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<MultiTrigger&>(owner).ClearConditions();
    return;
}

void AddMultiTriggerSetter(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Setter> retained =
        Base::Ref<Setter>::TryFromBorrowed(
            static_cast<Setter&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<MultiTrigger&>(owner).AddAuthoredSetter(
            std::move(retained)));
}

void ClearMultiTriggerSetters(
    Base::Object& owner,
    void*) noexcept {
    static_cast<MultiTrigger&>(owner).ClearAuthoredSetters();
    return;
}

void AddFrameworkEventTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Media::Animation::EventTrigger> retained =
        Base::Ref<Media::Animation::EventTrigger>::TryFromBorrowed(
            static_cast<Media::Animation::EventTrigger&>(*value));
    if (!retained) {
        return;
    }
    static_cast<void>(
        ::Aero::Visual::Impl::AddAuthoredTrigger(
            static_cast<FrameworkElement&>(owner),
            Base::Ref<Base::Object>(std::move(retained))));
}

void ClearFrameworkEventTriggers(
    Base::Object& owner,
    void*) noexcept {
    static_cast<void>(
        ::Aero::Visual::Impl::ClearAuthoredTriggers(
            static_cast<FrameworkElement&>(owner)));
}

void AddStoryboardTimeline(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Media::Animation::Timeline> retained =
        Base::Ref<Media::Animation::Timeline>::TryFromBorrowed(
            static_cast<Media::Animation::Timeline&>(*value));
    if (!retained) {
        return;
    }
    static_cast<void>(
        static_cast<Media::Animation::Storyboard&>(owner)
            .AddTimeline(std::move(retained)));
}

void ClearStoryboardTimelines(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Media::Animation::Storyboard&>(owner).ClearTimelines();
    return;
}

void AddGradientStop(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            GradientStop::StaticTypeId()) {
        return;
    }
    Base::Ref<GradientStop> retained =
        Base::Ref<GradientStop>::TryFromBorrowed(
            static_cast<GradientStop&>(*value));
    if (!retained) {
        return;
    }
    static_cast<void>(
        static_cast<GradientBrush&>(owner).AddGradientStop(
            std::move(retained)));
}

void ClearGradientStops(
    Base::Object& owner,
    void*) noexcept {
    static_cast<GradientBrush&>(owner)
        .ClearGradientStops();
    return;
}

void AddGradientStopCollectionItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            GradientStop::StaticTypeId()) {
        return;
    }
    static_cast<void>(
        static_cast<GradientStopCollection&>(owner).Add(
            Base::Ref<GradientStop>::FromBorrowed(
                static_cast<GradientStop&>(*value))));
}

void ClearGradientStopCollectionItems(
    Base::Object& owner,
    void*) noexcept {
    static_cast<GradientStopCollection&>(owner).Clear();
    return;
}

void AddPathFigureSegment(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Media::PathSegment> retained =
        Base::Ref<Media::PathSegment>::TryFromBorrowed(
            static_cast<Media::PathSegment&>(*value));
    if (retained) {
        static_cast<void>(
            static_cast<Media::PathFigure&>(owner)
                .AddSegment(std::move(retained)));
    }
}

void ClearPathFigureSegments(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Media::PathFigure&>(owner).ClearSegments();
}

void AddPathGeometryFigure(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Media::PathFigure> retained =
        Base::Ref<Media::PathFigure>::TryFromBorrowed(
            static_cast<Media::PathFigure&>(*value));
    if (retained) {
        static_cast<void>(
            static_cast<Media::PathGeometry&>(owner)
                .AddFigure(std::move(retained)));
    }
}

void ClearPathGeometryFigures(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Media::PathGeometry&>(owner).ClearFigures();
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
    RegistrationValues values =
        ::Aero::Meta::Detail::MakeRegistrationValues(context);
    Base::Result<Value> converted =
        values.TryConvertText(
            Meta::TypeOf<Color>(), text);
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
    made.Value()->SetColor(color.Value());
    return Value::FromObject(
        Brush::StaticTypeId(),
        Base::Ref<Base::Object>(
            made.Value()));
}

Base::Result<Value> ConvertGeometryText(
    TypeId targetType,
    Base::StringView text,
    void*) noexcept {
    const bool streamGeometry =
        targetType == Media::StreamGeometry::StaticTypeId();
    if (targetType != Media::Geometry::StaticTypeId() &&
        !streamGeometry) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Geometry text conversion received an invalid target");
    }
    Base::Result<Base::Ref<Media::StreamGeometry>> made =
        Base::MakeRef<Media::StreamGeometry>();
    if (!made) return made.GetStatus();
    Base::Ref<Media::Geometry> geometry =
        Base::Ref<Media::Geometry>(std::move(made).Value());
    static_cast<Media::StreamGeometry*>(geometry.Get())->SetData(text);
    return Value::FromObject(
        targetType,
        Base::Ref<Base::Object>(
            std::move(geometry)));
}

Base::Result<Value> ConvertFontFamilyText(
    TypeId targetType, Base::StringView text, void*) noexcept {
    if (targetType != Media::FontFamily::StaticTypeId()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "FontFamily text conversion received an invalid target");
    }
    Base::Result<Base::Ref<Media::FontFamily>> family = Base::MakeRef<Media::FontFamily>();
    if (!family) return family.GetStatus();
    family.Value()->SetSource(text);
    return Value::FromObject(Media::FontFamily::StaticTypeId(),
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
    image.Value()->SetUriSource(uri.Value());
    return Value::FromObject(
        ImageSource::StaticTypeId(),
        Base::Ref<Base::Object>(
            image.Value()));
}

void AddDoubleKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Media::Animation::DoubleKeyFrame> retained =
        Base::Ref<Media::Animation::DoubleKeyFrame>::TryFromBorrowed(
            static_cast<Media::Animation::DoubleKeyFrame&>(*value));
    if (!retained) {
        return;
    }
    static_cast<void>(
        static_cast<Media::Animation::DoubleAnimationUsingKeyFrames&>(
            owner).AddKeyFrame(std::move(retained)));
}

void ClearDoubleKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Media::Animation::DoubleAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddPointKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Media::Animation::PointKeyFrame> retained =
        Base::Ref<Media::Animation::PointKeyFrame>::TryFromBorrowed(
            static_cast<Media::Animation::PointKeyFrame&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<Media::Animation::PointAnimationUsingKeyFrames&>(owner)
            .AddKeyFrame(std::move(retained)));
}

void ClearPointKeyFrames(Base::Object& owner, void*) noexcept {
    static_cast<Media::Animation::PointAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
}

void AddThicknessKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Media::Animation::ThicknessKeyFrame> retained =
        Base::Ref<Media::Animation::ThicknessKeyFrame>::
            TryFromBorrowed(
                static_cast<
                    Media::Animation::ThicknessKeyFrame&>(
                        *value));
    if (!retained) {
        return;
    }
    static_cast<void>(
        static_cast<Media::Animation::ThicknessAnimationUsingKeyFrames&>(
            owner).AddKeyFrame(std::move(retained)));
}

void ClearThicknessKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Media::Animation::ThicknessAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddColorKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Media::Animation::ColorKeyFrame> retained =
        Base::Ref<Media::Animation::ColorKeyFrame>::TryFromBorrowed(
            static_cast<Media::Animation::ColorKeyFrame&>(*value));
    if (!retained) {
        return;
    }
    static_cast<void>(
        static_cast<Media::Animation::ColorAnimationUsingKeyFrames&>(
            owner).AddKeyFrame(std::move(retained)));
}

void ClearColorKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Media::Animation::ColorAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddObjectKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Media::Animation::DiscreteObjectKeyFrame> retained =
        Base::Ref<Media::Animation::DiscreteObjectKeyFrame>::TryFromBorrowed(
            static_cast<Media::Animation::DiscreteObjectKeyFrame&>(*value));
    if (!retained) {
        return;
    }
    static_cast<void>(
        static_cast<Media::Animation::ObjectAnimationUsingKeyFrames&>(
            owner).AddKeyFrame(std::move(retained)));
}

void ClearObjectKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Media::Animation::ObjectAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddBooleanKeyFrame(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Media::Animation::DiscreteBooleanKeyFrame> retained =
        Base::Ref<Media::Animation::DiscreteBooleanKeyFrame>::TryFromBorrowed(
            static_cast<Media::Animation::DiscreteBooleanKeyFrame&>(*value));
    if (!retained) {
        return;
    }
    static_cast<void>(
        static_cast<Media::Animation::BooleanAnimationUsingKeyFrames&>(
            owner).AddKeyFrame(std::move(retained)));
}

void ClearBooleanKeyFrames(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Media::Animation::BooleanAnimationUsingKeyFrames&>(owner)
        .ClearKeyFrames();
    return;
}

void AddEventTriggerAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Media::Animation::TriggerAction> retained =
        Base::Ref<Media::Animation::TriggerAction>::TryFromBorrowed(
            static_cast<Media::Animation::TriggerAction&>(*value));
    if (!retained) {
        return;
    }
    static_cast<void>(
        static_cast<Media::Animation::EventTrigger&>(owner)
            .AddAction(std::move(retained)));
}

void ClearEventTriggerActions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Media::Animation::EventTrigger&>(owner).ClearActions();
    return;
}

void AddPropertyChangedTriggerAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Media::Animation::TriggerAction> retained =
        Base::Ref<Media::Animation::TriggerAction>::TryFromBorrowed(
            static_cast<Media::Animation::TriggerAction&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<Media::Animation::PropertyChangedTrigger&>(owner)
            .AddAction(std::move(retained)));
}

void ClearPropertyChangedTriggerActions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Media::Animation::PropertyChangedTrigger&>(owner)
        .ClearActions();
}

void AddKeyTriggerAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Media::Animation::TriggerAction> retained =
        Base::Ref<Media::Animation::TriggerAction>::TryFromBorrowed(
            static_cast<Media::Animation::TriggerAction&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<Media::Animation::KeyTrigger&>(owner)
            .AddAction(std::move(retained)));
}

void ClearKeyTriggerActions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Media::Animation::KeyTrigger&>(owner).ClearActions();
}

void AddInteractionBehavior(
    Base::Object& owner, const Base::Ref<Base::Object>& value, void*) noexcept {
    if (!value) return;
    if (owner.RuntimeType() == DataTrigger::StaticTypeId()) {
        static_cast<void>(static_cast<DataTrigger&>(owner).AddBehavior(value));
    } else if (owner.RuntimeType() ==
               Media::Animation::StoryboardCompletedTrigger::StaticTypeId()) {
        static_cast<void>(static_cast<Media::Animation::StoryboardCompletedTrigger&>(owner)
            .AddConditionBehavior(value));
    } else if (owner.RuntimeType() ==
               Media::Animation::EventTrigger::StaticTypeId()) {
        static_cast<void>(static_cast<Media::Animation::EventTrigger&>(owner)
            .AddConditionBehavior(value));
    } else {
        static_cast<void>(::Aero::Visual::Impl::AddAuthoredBehavior(
            static_cast<FrameworkElement&>(owner), value));
    }
}
void ClearInteractionBehaviors(Base::Object& owner, void*) noexcept {
    if (owner.RuntimeType() == DataTrigger::StaticTypeId()) {
        static_cast<DataTrigger&>(owner).ClearBehaviors();
    } else if (owner.RuntimeType() ==
               Media::Animation::StoryboardCompletedTrigger::StaticTypeId()) {
        static_cast<Media::Animation::StoryboardCompletedTrigger&>(owner)
            .ClearConditionBehaviors();
    } else if (owner.RuntimeType() ==
               Media::Animation::EventTrigger::StaticTypeId()) {
        static_cast<Media::Animation::EventTrigger&>(owner)
            .ClearConditionBehaviors();
    } else {
        static_cast<void>(::Aero::Visual::Impl::ClearAuthoredBehaviors(
            static_cast<FrameworkElement&>(owner)));
    }
}

void AddStyleBehaviorItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    static_cast<void>(
        static_cast<Interactivity::StyleBehaviorCollection&>(owner)
            .Add(value));
}
void ClearStyleBehaviorItems(Base::Object& owner, void*) noexcept {
    static_cast<Interactivity::StyleBehaviorCollection&>(owner).Clear();
}
void SetBackgroundEffectBehaviorEffect(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    auto& behavior =
        static_cast<Interactivity::BackgroundEffectBehavior&>(owner);
    if (!value) {
        behavior.SetEffect({});
        return;
    }
    Base::Ref<Media::Effect> effect =
        Base::Ref<Media::Effect>::TryFromBorrowed(
            static_cast<Media::Effect&>(*value));
    if (effect) behavior.SetEffect(std::move(effect));
}
void ClearBackgroundEffectBehaviorEffect(
    Base::Object& owner, void*) noexcept {
    static_cast<Interactivity::BackgroundEffectBehavior&>(owner)
        .SetEffect({});
}
void AddStyleTriggerItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    static_cast<void>(
        static_cast<Interactivity::StyleTriggerCollection&>(owner)
            .Add(value));
}
void ClearStyleTriggerItems(Base::Object& owner, void*) noexcept {
    static_cast<Interactivity::StyleTriggerCollection&>(owner).Clear();
}
void AddConditionalComparison(
    Base::Object& owner, const Base::Ref<Base::Object>& value, void*) noexcept {
    if (!value) return;
    Base::Ref<Media::Animation::ComparisonCondition> retained =
        Base::Ref<Media::Animation::ComparisonCondition>::TryFromBorrowed(
            static_cast<Media::Animation::ComparisonCondition&>(*value));
    if (!retained) return;
    static_cast<void>(
        static_cast<Media::Animation::ConditionalExpression&>(owner)
            .AddCondition(std::move(retained)));
}
void ClearConditionalComparisons(Base::Object& owner, void*) noexcept {
    static_cast<Media::Animation::ConditionalExpression&>(owner).ClearConditions(); return;
}
void SetConditionBehaviorExpression(
    Base::Object& owner, const Base::Ref<Base::Object>& value, void*) noexcept {
    static_cast<Media::Animation::ConditionBehavior&>(owner).SetExpression(
        Base::Ref<Media::Animation::ConditionalExpression>::FromBorrowed(
            static_cast<Media::Animation::ConditionalExpression&>(*value)));
    return;
}
void ClearConditionBehaviorExpression(Base::Object& owner, void*) noexcept {
    static_cast<Media::Animation::ConditionBehavior&>(owner).SetExpression({});
    return;
}

void AddStoryboardCompletedTriggerAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Media::Animation::TriggerAction> retained =
        Base::Ref<Media::Animation::TriggerAction>::TryFromBorrowed(
            static_cast<Media::Animation::TriggerAction&>(*value));
    if (!retained) {
        return;
    }
    static_cast<void>(
        static_cast<Media::Animation::StoryboardCompletedTrigger&>(owner)
            .AddAction(std::move(retained)));
}

void ClearStoryboardCompletedTriggerActions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Media::Animation::StoryboardCompletedTrigger&>(owner)
        .ClearActions();
    return;
}

void AddInteractionTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    auto& dependencyObject =
        static_cast<DependencyObject&>(owner);
    const Meta::TypeRegistry& types =
        dependencyObject.PropertyRegistry().Types();
    if (types.IsDerivedFrom(
            owner.RuntimeType(), FrameworkElement::StaticTypeId())) {
        static_cast<void>(
            ::Aero::Visual::Impl::AddAuthoredTrigger(
                static_cast<FrameworkElement&>(owner), value));
    } else if (types.IsDerivedFrom(
                   owner.RuntimeType(),
                   FrameworkContentElement::StaticTypeId())) {
        static_cast<void>(
            ::Aero::Visual::Impl::AddAuthoredTrigger(
                static_cast<FrameworkContentElement&>(owner), value));
    }
}

void ClearInteractionTriggers(
    Base::Object& owner,
    void*) noexcept {
    auto& dependencyObject =
        static_cast<DependencyObject&>(owner);
    const Meta::TypeRegistry& types =
        dependencyObject.PropertyRegistry().Types();
    if (types.IsDerivedFrom(
            owner.RuntimeType(), FrameworkElement::StaticTypeId())) {
        static_cast<void>(
            ::Aero::Visual::Impl::ClearAuthoredTriggers(
                static_cast<FrameworkElement&>(owner)));
    } else if (types.IsDerivedFrom(
                   owner.RuntimeType(),
                   FrameworkContentElement::StaticTypeId())) {
        static_cast<void>(
            ::Aero::Visual::Impl::ClearAuthoredTriggers(
                static_cast<FrameworkContentElement&>(owner)));
    }
}

void SetBeginStoryboardContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    Base::Ref<Media::Animation::Storyboard> retained =
        Base::Ref<Media::Animation::Storyboard>::TryFromBorrowed(
            static_cast<Media::Animation::Storyboard&>(*value));
    if (!retained) {
        return;
    }
    static_cast<Media::Animation::BeginStoryboard&>(owner)
        .SetStoryboard(std::move(retained));
    return;
}

void ClearBeginStoryboardContent(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Media::Animation::BeginStoryboard&>(owner)
        .SetStoryboard({});
    return;
}

void AddTransformGroupChild(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Transform> retained =
        Base::Ref<Transform>::TryFromBorrowed(
            static_cast<Transform&>(*value));
    if (!retained) {
        return;
    }
    static_cast<void>(
        static_cast<TransformGroup&>(owner)
            .AddChild(std::move(retained)));
}

void ClearTransformGroupChildren(
    Base::Object& owner,
    void*) noexcept {
    static_cast<TransformGroup&>(owner).ClearChildren();
    return;
}

void OnRenderStateChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept {
    auto& visual =
        static_cast<UIElement&>(object);
    static_cast<void>(
        Aero::GuiPrivate::Detail::ElementPrivate::
            InvalidateRenderState(visual));
}

void OnOpacityMaskChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept {
    FrameworkElement* owner =
        static_cast<UIElement&>(object).AsFrameworkElement();
    if (owner == nullptr) return;
    static_cast<void>(
        Aero::GuiPrivate::Detail::ElementPrivate::
            InvalidateRenderState(*owner));
}

void OnRenderTransformChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept {
    FrameworkElement* owner =
        static_cast<UIElement&>(object).AsFrameworkElement();
    if (owner == nullptr) return;
    static_cast<void>(
        Aero::GuiPrivate::Detail::ElementPrivate::
            InvalidateRenderState(*owner));
}

void OnLayoutTransformChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&) noexcept {
}

void OnEffectChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept {
    FrameworkElement* owner =
        static_cast<UIElement&>(object).AsFrameworkElement();
    if (owner == nullptr) return;
    static_cast<void>(
        Aero::GuiPrivate::Detail::ElementPrivate::
            InvalidateRenderState(*owner));
}
