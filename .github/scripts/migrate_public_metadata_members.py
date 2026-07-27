from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]

PROPERTY_TYPES = {
    "include/Aero/Controls/ControlPrimitives.hpp": {
        "TemplateProperty": "ControlTemplate",
    },
    "include/Aero/Controls/Controls.hpp": {
        "OrientationProperty": "Orientation",
        "LeftProperty": "double",
        "TopProperty": "double",
        "RowProperty": "std::uint32_t",
        "ColumnProperty": "std::uint32_t",
        "BackgroundProperty": "Color",
        "BorderBrushProperty": "Color",
        "BorderThicknessProperty": "double",
        "PaddingProperty": "Thickness",
        "TextProperty": "Base::String",
        "ForegroundProperty": "Color",
    },
    "include/Aero/Controls/Items.hpp": {
        "ItemCountProperty": "std::uint32_t",
    },
    "include/Aero/Controls/Scroll.hpp": {
        "HorizontalOffsetProperty": "double",
        "VerticalOffsetProperty": "double",
        "ExtentWidthProperty": "double",
        "ExtentHeightProperty": "double",
        "ViewportWidthProperty": "double",
        "ViewportHeightProperty": "double",
        "CanHorizontallyScrollProperty": "bool",
        "CanVerticallyScrollProperty": "bool",
        "CanContentScrollProperty": "bool",
        "OrientationProperty": "Orientation",
        "MinimumProperty": "double",
        "MaximumProperty": "double",
        "ValueProperty": "double",
        "ViewportSizeProperty": "double",
        "SmallChangeProperty": "double",
        "LargeChangeProperty": "double",
    },
    "include/Aero/Controls/Selection.hpp": {
        "IsSelectedProperty": "bool",
        "SelectionModeProperty": "SelectionMode",
        "SelectedIndexProperty": "std::uint32_t",
        "SelectedItemProperty": "Base::Object",
        "SelectedValueProperty": "Base::Object",
    },
    "include/Aero/Controls/TextBox.hpp": {
        "TextProperty": "Base::String",
        "IsReadOnlyProperty": "bool",
        "MaximumLengthProperty": "std::uint32_t",
        "AcceptsReturnProperty": "bool",
        "ForegroundProperty": "Color",
        "SelectionBrushProperty": "Color",
        "CaretBrushProperty": "Color",
    },
    "include/Aero/Controls/Virtualization.hpp": {
        "OrientationProperty": "Orientation",
        "OverscanCountProperty": "std::uint32_t",
        "EstimatedItemExtentProperty": "double",
    },
    "include/Aero/Presentation/Layout.hpp": {
        "ClipToBoundsProperty": "bool",
        "IsHitTestVisibleProperty": "bool",
        "IsEnabledProperty": "bool",
        "IsMouseOverProperty": "bool",
        "IsPressedProperty": "bool",
        "IsKeyboardFocusedProperty": "bool",
        "IsTabStopProperty": "bool",
        "TabIndexProperty": "std::uint32_t",
        "IsFocusScopeProperty": "bool",
    },
    "include/Aero/Presentation/Rendering.hpp": {
        "DataContextProperty": "Base::Object",
        "StyleProperty": "Style",
        "WidthProperty": "double",
        "HeightProperty": "double",
        "MinWidthProperty": "double",
        "MaxWidthProperty": "double",
        "MinHeightProperty": "double",
        "MaxHeightProperty": "double",
        "MarginProperty": "Thickness",
        "HorizontalAlignmentProperty": "HorizontalAlignment",
        "VerticalAlignmentProperty": "VerticalAlignment",
        "UseLayoutRoundingProperty": "bool",
    },
}

EVENT_TYPES = {
    "include/Aero/Controls/Scroll.hpp": {
        "ScrollChangedEvent": "ScrollChangedEventArgs",
    },
    "include/Aero/Presentation/Layout.hpp": {
        "MouseMoveEvent": "MouseEventArgs",
        "MouseDownEvent": "MouseButtonEventArgs",
        "MouseUpEvent": "MouseButtonEventArgs",
        "MouseWheelEvent": "MouseWheelEventArgs",
        "GotKeyboardFocusEvent": "KeyboardFocusChangedEventArgs",
        "LostKeyboardFocusEvent": "KeyboardFocusChangedEventArgs",
        "KeyDownEvent": "KeyEventArgs",
        "KeyUpEvent": "KeyEventArgs",
        "TextInputEvent": "TextCompositionEventArgs",
    },
}

EXPECTED_PROPERTY_COUNTS = {
    ("include/Aero/Controls/Scroll.hpp", "OrientationProperty"): 2,
    ("include/Aero/Controls/Scroll.hpp", "MinimumProperty"): 2,
    ("include/Aero/Controls/Scroll.hpp", "MaximumProperty"): 2,
    ("include/Aero/Controls/Scroll.hpp", "ValueProperty"): 2,
    ("include/Aero/Controls/Scroll.hpp", "ViewportSizeProperty"): 2,
}


def add_type_include(text: str) -> str:
    if "#include <Aero/Type.hpp>" in text:
        return text
    lines = text.splitlines()
    last_include = max(i for i, line in enumerate(lines) if line.startswith("#include"))
    lines.insert(last_include + 1, "#include <Aero/Type.hpp>")
    return "\n".join(lines) + "\n"


def replace_property(text: str, path: str, name: str, value_type: str) -> str:
    literal = name.removesuffix("Property")
    pattern = re.compile(
        r"(?m)^\s*inline static constexpr (?:Aero::Core::)?DependencyPropertyHandle\s+"
        + re.escape(name)
        + r"\s*=\s*(?:Aero::Core::)?MakeDependencyPropertyHandle\s*\(\s*"
        + r"StaticTypeIdValue_\s*,\s*\""
        + re.escape(literal)
        + r"\"\s*\);"
    )
    replacement = (
        f"    inline static constexpr auto {name} =\n"
        f"        Members::Property<{value_type}>{{\"{literal}\"}};"
    )
    text, count = pattern.subn(replacement, text)
    expected = EXPECTED_PROPERTY_COUNTS.get((path, name), 1)
    if count != expected:
        raise RuntimeError(f"{path}: expected {expected} replacements for {name}, got {count}")
    return text


def replace_event(text: str, path: str, name: str, args_type: str) -> str:
    literal = name.removesuffix("Event")
    pattern = re.compile(
        r"(?m)^\s*inline static constexpr (?:Aero::Core::)?RoutedEventHandle\s+"
        + re.escape(name)
        + r"\s*=\s*(?:Aero::Core::)?MakeRoutedEventHandle\s*\(\s*"
        + r"StaticTypeIdValue_\s*,\s*\""
        + re.escape(literal)
        + r"\"\s*\);"
    )
    replacement = (
        f"    inline static constexpr auto {name} =\n"
        f"        Members::RoutedEvent<{args_type}>{{\"{literal}\"}};"
    )
    text, count = pattern.subn(replacement, text)
    if count != 1:
        raise RuntimeError(f"{path}: expected one replacement for {name}, got {count}")
    return text


def migrate(path: str) -> None:
    file = ROOT / path
    text = file.read_text(encoding="utf-8")
    text = add_type_include(text)
    text = text.replace("AERO_TYPED_META_NAMED(", "AERO_DECLARE_TYPE_NAMED(")
    text = text.replace("AERO_TYPED_META(", "AERO_DECLARE_TYPE(")
    for name, value_type in PROPERTY_TYPES[path].items():
        text = replace_property(text, path, name, value_type)
    for name, args_type in EVENT_TYPES.get(path, {}).items():
        text = replace_event(text, path, name, args_type)
    if "MakeDependencyPropertyHandle" in text or "MakeRoutedEventHandle" in text:
        raise RuntimeError(f"{path}: raw public member handle declaration remains")
    file.write_text(text, encoding="utf-8")


for source_path in PROPERTY_TYPES:
    migrate(source_path)
