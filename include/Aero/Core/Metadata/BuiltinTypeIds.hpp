#pragma once

#include <Aero/Base/StringView.hpp>
#include <Aero/Core/Metadata/TypeRegistry.hpp>

namespace Aero::Core::BuiltinTypes {

inline constexpr TypeId Object = MakeTypeId(Base::StringView("Object"));
inline constexpr TypeId DependencyObject =
    MakeTypeId(Base::StringView("DependencyObject"));
inline constexpr TypeId Visual = MakeTypeId(Base::StringView("Visual"));
inline constexpr TypeId UIElement =
    MakeTypeId(Base::StringView("UIElement"));
inline constexpr TypeId FrameworkElement =
    MakeTypeId(Base::StringView("FrameworkElement"));

inline constexpr TypeId Panel = MakeTypeId(Base::StringView("Panel"));
inline constexpr TypeId Decorator = MakeTypeId(Base::StringView("Decorator"));
inline constexpr TypeId Control = MakeTypeId(Base::StringView("Control"));
inline constexpr TypeId ContentControl =
    MakeTypeId(Base::StringView("ContentControl"));
inline constexpr TypeId UserControl =
    MakeTypeId(Base::StringView("UserControl"));
inline constexpr TypeId ButtonBase =
    MakeTypeId(Base::StringView("ButtonBase"));
inline constexpr TypeId Button =
    MakeTypeId(Base::StringView("Button"));

inline constexpr TypeId StackPanel = MakeTypeId(Base::StringView("StackPanel"));
inline constexpr TypeId Canvas = MakeTypeId(Base::StringView("Canvas"));
inline constexpr TypeId Grid = MakeTypeId(Base::StringView("Grid"));
inline constexpr TypeId Border = MakeTypeId(Base::StringView("Border"));
inline constexpr TypeId TextBlock = MakeTypeId(Base::StringView("TextBlock"));
inline constexpr TypeId ContentPresenter =
    MakeTypeId(Base::StringView("ContentPresenter"));

inline constexpr TypeId Boolean = MakeTypeId(Base::StringView("Boolean"));
inline constexpr TypeId UnsignedInteger = MakeTypeId(Base::StringView("UInt32"));
inline constexpr TypeId Double = MakeTypeId(Base::StringView("Double"));
inline constexpr TypeId String = MakeTypeId(Base::StringView("String"));
inline constexpr TypeId Length = MakeTypeId(Base::StringView("Length"));
inline constexpr TypeId Thickness = MakeTypeId(Base::StringView("Thickness"));
inline constexpr TypeId Color = MakeTypeId(Base::StringView("Color"));
inline constexpr TypeId HorizontalAlignment =
    MakeTypeId(Base::StringView("HorizontalAlignment"));
inline constexpr TypeId VerticalAlignment =
    MakeTypeId(Base::StringView("VerticalAlignment"));
inline constexpr TypeId Orientation = MakeTypeId(Base::StringView("Orientation"));

inline constexpr TypeId EventArgs = MakeTypeId(Base::StringView("EventArgs"));
inline constexpr TypeId RoutedEventArgs =
    MakeTypeId(Base::StringView("RoutedEventArgs"));
inline constexpr TypeId InputEventArgs =
    MakeTypeId(Base::StringView("InputEventArgs"));
inline constexpr TypeId MouseEventArgs =
    MakeTypeId(Base::StringView("MouseEventArgs"));
inline constexpr TypeId MouseButtonEventArgs =
    MakeTypeId(Base::StringView("MouseButtonEventArgs"));
inline constexpr TypeId KeyEventArgs =
    MakeTypeId(Base::StringView("KeyEventArgs"));
inline constexpr TypeId TextCompositionEventArgs =
    MakeTypeId(Base::StringView("TextCompositionEventArgs"));
inline constexpr TypeId KeyboardFocusChangedEventArgs =
    MakeTypeId(Base::StringView("KeyboardFocusChangedEventArgs"));

} // namespace Aero::Core::BuiltinTypes
