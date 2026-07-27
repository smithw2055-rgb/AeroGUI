from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def replace_function(text: str, pattern: str, replacement: str) -> str:
    match = re.search(pattern, text, re.MULTILINE)
    if match is None:
        raise RuntimeError(f"function not found: {pattern}")
    brace = text.find("{", match.start())
    if brace < 0:
        raise RuntimeError(f"function body not found: {pattern}")
    depth = 0
    end = brace
    while end < len(text):
        if text[end] == "{":
            depth += 1
        elif text[end] == "}":
            depth -= 1
            if depth == 0:
                end += 1
                break
        end += 1
    return text[:match.start()] + replacement + text[end:]


metadata = ROOT / "include/Aero/Metadata.hpp"
text = metadata.read_text(encoding="utf-8")
if "AccessType GetOr(" not in text:
    anchor = "    Base::Result<void> Set(\n"
    position = text.find(anchor)
    if position < 0:
        raise RuntimeError("DependencyPropertyRef::Set anchor not found")
    get_or = """    AccessType GetOr(\n        const DependencyObject& object,\n        AccessType fallback = {}) const noexcept {\n        Base::Result<AccessType> value = Get(object);\n        return value ? std::move(value).Value()\n                     : std::move(fallback);\n    }\n\n"""
    text = text[:position] + get_or + text[position:]
metadata.write_text(text, encoding="utf-8")

rendering = ROOT / "include/Aero/Presentation/Rendering.hpp"
text = rendering.read_text(encoding="utf-8")
text = text.replace('Members::Property<double>{"Width"}', 'Members::Property<Length>{"Width"}')
text = text.replace('Members::Property<double>{"Height"}', 'Members::Property<Length>{"Height"}')
rendering.write_text(text, encoding="utf-8")

buttons = ROOT / "src/controls/Buttons.cpp"
text = buttons.read_text(encoding="utf-8")
replacements = [
(r"ClickMode ButtonBase::GetClickMode\(\) const noexcept \{", """ClickMode ButtonBase::GetClickMode() const noexcept {\n    return ClickModeProperty.GetOr(*this, ClickMode::Release);\n}"""),
(r"ICommand\* ButtonBase::Command\(\) const noexcept \{", """ICommand* ButtonBase::Command() const noexcept {\n    Base::Result<Base::Ref<ICommand>> value =\n        CommandProperty.Get(*this);\n    return value ? value.Value().Get() : nullptr;\n}"""),
(r"Base::Ref<Base::Object> ButtonBase::CommandParameter\(\) const noexcept \{", """Base::Ref<Base::Object> ButtonBase::CommandParameter() const noexcept {\n    return CommandParameterProperty.GetOr(*this);\n}"""),
(r"UIElement\* ButtonBase::CommandTarget\(\) const noexcept \{", """UIElement* ButtonBase::CommandTarget() const noexcept {\n    Base::Result<Base::Ref<UIElement>> value =\n        CommandTargetProperty.Get(*this);\n    return value ? value.Value().Get() : nullptr;\n}"""),
(r"Base::Result<void> ButtonBase::SetClickMode\(\s*ClickMode value\) noexcept \{", """Base::Result<void> ButtonBase::SetClickMode(\n    ClickMode value) noexcept {\n    if (value > ClickMode::Hover) {\n        return Base::Status::Failure(\n            Base::ErrorCode::InvalidArgument,\n            \"ButtonBase ClickMode is invalid\");\n    }\n    return ClickModeProperty.Set(*this, value);\n}"""),
(r"Base::Result<void> ButtonBase::SetCommand\(\s*Base::Ref<ICommand> command\) noexcept \{", """Base::Result<void> ButtonBase::SetCommand(\n    Base::Ref<ICommand> command) noexcept {\n    return CommandProperty.Set(*this, command);\n}"""),
(r"Base::Result<void> ButtonBase::SetCommandParameter\(\s*Base::Ref<Base::Object> parameter\) noexcept \{", """Base::Result<void> ButtonBase::SetCommandParameter(\n    Base::Ref<Base::Object> parameter) noexcept {\n    return CommandParameterProperty.Set(*this, parameter);\n}"""),
(r"Base::Result<void> ButtonBase::SetCommandTarget\(\s*Base::Ref<UIElement> target\) noexcept \{", """Base::Result<void> ButtonBase::SetCommandTarget(\n    Base::Ref<UIElement> target) noexcept {\n    return CommandTargetProperty.Set(*this, target);\n}"""),
(r"std::uint32_t RepeatButton::Delay\(\) const noexcept \{", """std::uint32_t RepeatButton::Delay() const noexcept {\n    return DelayProperty.GetOr(*this, 400U);\n}"""),
(r"std::uint32_t RepeatButton::Interval\(\) const noexcept \{", """std::uint32_t RepeatButton::Interval() const noexcept {\n    return IntervalProperty.GetOr(*this, 100U);\n}"""),
(r"Base::Result<void> RepeatButton::SetDelay\(\s*std::uint32_t value\) noexcept \{", """Base::Result<void> RepeatButton::SetDelay(\n    std::uint32_t value) noexcept {\n    return DelayProperty.Set(*this, value);\n}"""),
(r"Base::Result<void> RepeatButton::SetInterval\(\s*std::uint32_t value\) noexcept \{", """Base::Result<void> RepeatButton::SetInterval(\n    std::uint32_t value) noexcept {\n    if (value == 0U) {\n        return Base::Status::Failure(\n            Base::ErrorCode::InvalidArgument,\n            \"RepeatButton interval must be positive\");\n    }\n    return IntervalProperty.Set(*this, value);\n}"""),
(r"bool ToggleButton::IsChecked\(\) const noexcept \{", """bool ToggleButton::IsChecked() const noexcept {\n    return IsCheckedProperty.GetOr(*this, false);\n}"""),
(r"bool ToggleButton::IsThreeState\(\) const noexcept \{", """bool ToggleButton::IsThreeState() const noexcept {\n    return IsThreeStateProperty.GetOr(*this, false);\n}"""),
(r"bool ToggleButton::IsIndeterminate\(\) const noexcept \{", """bool ToggleButton::IsIndeterminate() const noexcept {\n    return IsIndeterminateProperty.GetOr(*this, false);\n}"""),
]
for pattern, replacement in replacements:
    text = replace_function(text, pattern, replacement)
buttons.write_text(text, encoding="utf-8")

controls = ROOT / "src/controls/Controls.cpp"
text = controls.read_text(encoding="utf-8")
replacements = [
(r"Orientation StackPanel::GetOrientation\(\) const noexcept \{", """Orientation StackPanel::GetOrientation() const noexcept {\n    return OrientationProperty.GetOr(*this, Orientation::Vertical);\n}"""),
(r"Base::Result<void> StackPanel::SetOrientation\(Orientation value\) noexcept \{", """Base::Result<void> StackPanel::SetOrientation(Orientation value) noexcept {\n    if (value > Orientation::Vertical) {\n        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,\n            \"StackPanel orientation is invalid\");\n    }\n    return OrientationProperty.Set(*this, value);\n}"""),
(r"Color Border::Background\(\) const noexcept \{", """Color Border::Background() const noexcept {\n    return BackgroundProperty.GetOr(*this, Color{});\n}"""),
(r"Color Border::BorderBrush\(\) const noexcept \{", """Color Border::BorderBrush() const noexcept {\n    return BorderBrushProperty.GetOr(*this, Color{});\n}"""),
(r"double Border::BorderThickness\(\) const noexcept \{", """double Border::BorderThickness() const noexcept {\n    return BorderThicknessProperty.GetOr(*this, 0.0);\n}"""),
(r"Thickness Border::Padding\(\) const noexcept \{", """Thickness Border::Padding() const noexcept {\n    return PaddingProperty.GetOr(*this, Thickness{});\n}"""),
]
for pattern, replacement in replacements:
    text = replace_function(text, pattern, replacement)
controls.write_text(text, encoding="utf-8")

layout = ROOT / "src/presentation/Layout.cpp"
text = layout.read_text(encoding="utf-8")
replacements = [
(r"bool UIElement::ClipToBounds\(\) const noexcept \{", """bool UIElement::ClipToBounds() const noexcept {\n    return ClipToBoundsProperty.GetOr(*this, false);\n}"""),
(r"bool UIElement::IsHitTestVisible\(\) const noexcept \{", """bool UIElement::IsHitTestVisible() const noexcept {\n    return IsHitTestVisibleProperty.GetOr(*this, true);\n}"""),
(r"bool UIElement::IsMouseOver\(\) const noexcept \{", """bool UIElement::IsMouseOver() const noexcept {\n    return IsMouseOverProperty.GetOr(*this, false);\n}"""),
(r"bool UIElement::IsPressed\(\) const noexcept \{", """bool UIElement::IsPressed() const noexcept {\n    return IsPressedProperty.GetOr(*this, false);\n}"""),
(r"bool UIElement::IsKeyboardFocused\(\) const noexcept \{", """bool UIElement::IsKeyboardFocused() const noexcept {\n    return IsKeyboardFocusedProperty.GetOr(*this, false);\n}"""),
(r"bool UIElement::IsTabStop\(\) const noexcept \{", """bool UIElement::IsTabStop() const noexcept {\n    return IsTabStopProperty.GetOr(*this, false);\n}"""),
(r"std::uint32_t UIElement::TabIndex\(\) const noexcept \{", """std::uint32_t UIElement::TabIndex() const noexcept {\n    return TabIndexProperty.GetOr(*this, 0U);\n}"""),
(r"bool UIElement::IsFocusScope\(\) const noexcept \{", """bool UIElement::IsFocusScope() const noexcept {\n    return IsFocusScopeProperty.GetOr(*this, false);\n}"""),
(r"bool FrameworkElement::UseLayoutRounding\(\) const noexcept \{", """bool FrameworkElement::UseLayoutRounding() const noexcept {\n    return UseLayoutRoundingProperty.GetOr(*this, false);\n}"""),
(r"Thickness FrameworkElement::Margin\(\) const noexcept \{", """Thickness FrameworkElement::Margin() const noexcept {\n    return MarginProperty.GetOr(*this, Thickness{});\n}"""),
(r"HorizontalAlignment FrameworkElement::GetHorizontalAlignment\(\) const noexcept \{", """HorizontalAlignment FrameworkElement::GetHorizontalAlignment() const noexcept {\n    return HorizontalAlignmentProperty.GetOr(\n        *this, HorizontalAlignment::Stretch);\n}"""),
(r"VerticalAlignment FrameworkElement::GetVerticalAlignment\(\) const noexcept \{", """VerticalAlignment FrameworkElement::GetVerticalAlignment() const noexcept {\n    return VerticalAlignmentProperty.GetOr(\n        *this, VerticalAlignment::Stretch);\n}"""),
(r"Base::Result<void> UIElement::SetClipToBounds\(bool value\) noexcept \{", """Base::Result<void> UIElement::SetClipToBounds(bool value) noexcept {\n    return ClipToBoundsProperty.Set(*this, value);\n}"""),
(r"Base::Result<void> UIElement::SetHitTestVisible\(bool value\) noexcept \{", """Base::Result<void> UIElement::SetHitTestVisible(bool value) noexcept {\n    return IsHitTestVisibleProperty.Set(*this, value);\n}"""),
(r"Base::Result<void> UIElement::SetEnabled\(bool value\) noexcept \{", """Base::Result<void> UIElement::SetEnabled(bool value) noexcept {\n    return IsEnabledProperty.Set(*this, value);\n}"""),
(r"Base::Result<void> UIElement::SetTabStop\(bool value\) noexcept \{", """Base::Result<void> UIElement::SetTabStop(bool value) noexcept {\n    return IsTabStopProperty.Set(*this, value);\n}"""),
(r"Base::Result<void> UIElement::SetTabIndex\(std::uint32_t value\) noexcept \{", """Base::Result<void> UIElement::SetTabIndex(std::uint32_t value) noexcept {\n    return TabIndexProperty.Set(*this, value);\n}"""),
(r"Base::Result<void> UIElement::SetFocusScope\(bool value\) noexcept \{", """Base::Result<void> UIElement::SetFocusScope(bool value) noexcept {\n    return IsFocusScopeProperty.Set(*this, value);\n}"""),
]
for pattern, replacement in replacements:
    text = replace_function(text, pattern, replacement)
layout.write_text(text, encoding="utf-8")
