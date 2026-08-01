#pragma once

// Built-in identifiers, metadata context glue and routed-event catalog.

#include <Aero/Base/StringView.hpp>
#include <Aero/Meta/TypeRegistry.hpp>

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
inline constexpr TypeId RepeatButton =
    MakeTypeId(Base::StringView("RepeatButton"));
inline constexpr TypeId ToggleButton =
    MakeTypeId(Base::StringView("ToggleButton"));
inline constexpr TypeId CheckBox =
    MakeTypeId(Base::StringView("CheckBox"));
inline constexpr TypeId RadioButton =
    MakeTypeId(Base::StringView("RadioButton"));
inline constexpr TypeId ScrollContentPresenter =
    MakeTypeId(Base::StringView("ScrollContentPresenter"));
inline constexpr TypeId ScrollViewer =
    MakeTypeId(Base::StringView("ScrollViewer"));
inline constexpr TypeId ScrollBar =
    MakeTypeId(Base::StringView("ScrollBar"));
inline constexpr TypeId Track =
    MakeTypeId(Base::StringView("Track"));
inline constexpr TypeId Thumb =
    MakeTypeId(Base::StringView("Thumb"));
inline constexpr TypeId ItemContainer =
    MakeTypeId(Base::StringView("ItemContainer"));
inline constexpr TypeId ItemsControl =
    MakeTypeId(Base::StringView("ItemsControl"));
inline constexpr TypeId ItemsPresenter =
    MakeTypeId(Base::StringView("ItemsPresenter"));
inline constexpr TypeId Selector =
    MakeTypeId(Base::StringView("Selector"));
inline constexpr TypeId ListBox =
    MakeTypeId(Base::StringView("ListBox"));
inline constexpr TypeId ListBoxItem =
    MakeTypeId(Base::StringView("ListBoxItem"));
inline constexpr TypeId VirtualizingStackPanel =
    MakeTypeId(Base::StringView("VirtualizingStackPanel"));

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
inline constexpr TypeId MouseWheelEventArgs =
    MakeTypeId(Base::StringView("MouseWheelEventArgs"));
inline constexpr TypeId KeyEventArgs =
    MakeTypeId(Base::StringView("KeyEventArgs"));
inline constexpr TypeId TextCompositionEventArgs =
    MakeTypeId(Base::StringView("TextCompositionEventArgs"));
inline constexpr TypeId KeyboardFocusChangedEventArgs =
    MakeTypeId(Base::StringView("KeyboardFocusChangedEventArgs"));
inline constexpr TypeId ScrollChangedEventArgs =
    MakeTypeId(Base::StringView("ScrollChangedEventArgs"));

} // namespace Aero::Core::BuiltinTypes

#include <Aero/Meta/MetadataDomain.hpp>

namespace Aero::Core {

namespace Detail {

// Module population is an implementation callback; hosts register through the
// MetadataDomain overload below.
AERO_API Base::Result<void> PopulateCoreMetadata(
    MetadataContext& context) noexcept;

} // namespace Detail

inline constexpr Base::StringView CoreMetadataModuleName() noexcept {
    return "Aero.Core";
}

inline Base::Result<void> TryRegisterCoreMetadata(
    MetadataDomain& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 1U;
    const Base::StringView name = CoreMetadataModuleName();
    return domain.TryRegisterModule({
        MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &Detail::PopulateCoreMetadata,
        nullptr,
        nullptr});
}

} // namespace Aero::Core

#include <Aero/Base/Result.hpp>

namespace Aero::Detail {

// Registers the complete built-in UI schema through the typed
// Fluent metadata DSL. The function leaves all stores mutable for host modules.
namespace Detail {

// Module population is an implementation callback; hosts register through the
// MetadataDomain overload below.
AERO_API Base::Result<void> PopulateUiMetadata(
    Core::MetadataContext& context) noexcept;

} // namespace Detail

inline constexpr Base::StringView UiMetadataModuleName() noexcept {
    return "Aero.UI";
}

inline Base::Result<void> TryRegisterUiMetadata(
    Core::MetadataDomain& domain) noexcept {
    constexpr std::uint32_t SchemaVersion = 11U;
    const Base::StringView name = UiMetadataModuleName();
    return domain.TryRegisterModule({
        Core::MakeMetadataModuleId(name),
        name,
        SchemaVersion,
        &Detail::PopulateUiMetadata,
        nullptr,
        nullptr});
}

} // namespace Aero::Detail


namespace Aero::Core::Detail {

class MetadataDomainAccess final {
public:
    static DependencyPropertyRegistry& DependencyProperties(
        MetadataDomain& domain) noexcept {
        return domain.DependencyProperties();
    }

    static void* RoutedEventState(
        MetadataDomain& domain) noexcept {
        return domain.RoutedEventState();
    }
};

} // namespace Aero::Core::Detail

#include <Aero/Base/Config.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include "MetadataBehaviorRegistrationStore.hpp"
#include <Aero/RoutedEvent.hpp>

namespace Aero::Core {

struct RoutedEventRegistration final {
    Base::StringView name;
    TypeId ownerType = InvalidTypeId;
    TypeId eventArgsType = InvalidTypeId;
    RoutingStrategy strategy = RoutingStrategy::Bubble;
};

class AERO_API RoutedEventCatalog final {
public:
    struct Definition final {
        RoutedEventHandle handle;
        TypeId ownerType = InvalidTypeId;
        TypeId eventArgsType = InvalidTypeId;
        RoutingStrategy strategy = RoutingStrategy::Bubble;
        Base::String name;

        Definition() noexcept : name() {}
    };

    RoutedEventCatalog(
        TypeRegistry& types,
        MetadataBehaviorRegistrationStore& behaviors) noexcept;

    RoutedEventCatalog(const RoutedEventCatalog&) = delete;
    RoutedEventCatalog& operator=(const RoutedEventCatalog&) = delete;

    Base::Result<RoutedEventHandle> TryRegister(
        const RoutedEventRegistration& registration) noexcept;
    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    const TypeRegistry& Types() const noexcept { return *types_; }
    const Definition* Find(RoutedEventHandle event) const noexcept;

private:
    TypeRegistry* types_ = nullptr;
    MetadataBehaviorRegistrationStore* behaviorRegistrations_ = nullptr;
    Base::Vector<Definition> definitions_;
    bool frozen_ = false;
};

} // namespace Aero::Core

#include <Aero/Meta/MetadataRegistrationValues.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Core::Detail {

struct MetadataContextState final {
    TypeRegistry* types = nullptr;
    MetadataBehaviorRegistrationStore* behaviors = nullptr;
    MetadataValueRegistrationStore* values = nullptr;
    DependencyPropertyRegistry* properties = nullptr;
    RoutedEventCatalog* events = nullptr;
};

} // namespace Aero::Core::Detail

// Private helpers for sealing value behavior into MetadataRuntimeData.

#include <Aero/Base/Hash.hpp>
#include "MetadataRuntimeData.hpp"

namespace Aero::Core::Detail {

// Computes the deterministic structural contribution of value-semantics and
// text-converter facets. Callback and context addresses are never included.
Base::Result<Base::HashCode> ComputeMetadataValueFacetHash(
    const MetadataFacetStore& facets,
    const TypeRegistry& descriptors) noexcept;

} // namespace Aero::Core::Detail
