// Consolidated implementation. Keep sections ordered by dependency.

// ===== CoreMetadata =====

#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include <Aero/TextProperties.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include "gui/core/ElementsFill.hpp"
#include "gui/core/RenderStateCallbacks.hpp"

#include <Aero/Meta.hpp>
#include <Aero/Value.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/Animatable.hpp>
#include <Aero/DispatcherObject.hpp>
#include "gui/core/UiMetadata.hpp"

namespace Aero::Meta {
Base::Result<void> PopulateCoreMetadata(
    Meta::Registration& context) noexcept {

    Register<Base::Object>(context);

    Register<bool>(context)
        .TextConverter<&Base::ValueConversion::ConvertBoolean>();

    Register<::Aero::Nullable<bool>>(context)
        .TextConverter<&Base::ValueConversion::ConvertNullableBoolean>();

    Register<std::int8_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::int8_t>>();
    Register<std::int16_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::int16_t>>();
    Register<std::int32_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::int32_t>>();
    Register<std::int64_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::int64_t>>();
    Register<std::uint8_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::uint8_t>>();
    Register<std::uint16_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::uint16_t>>();
    Register<std::uint32_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::uint32_t>>();
    Register<std::uint64_t>(context)
        .TextConverter<&Base::ValueConversion::ConvertInteger<std::uint64_t>>();

    Register<double>(context)
        .TextConverter<&Base::ValueConversion::ConvertDouble>();

    Register<Base::String>(context)
        .TextConverter<&Base::ValueConversion::ConvertString>();

    Register<Value>(context)
        .ValueSemantics();

    Register<TypeReference>(context);

    Register<Base::ResourceUri>(context)
        .ValueSemantics()
        .TextConverter<&Base::ValueConversion::ConvertResourceUri>();

    Register<Threading::DispatcherObject>(context, TypeFlags::Abstract);

    Register<DependencyObject>(context, TypeFlags::Abstract);

    return Meta::Register<Freezable>(
        context, TypeFlags::Abstract).Result();
}

} // namespace Aero::Meta


// ===== UiMetadata =====

namespace Aero {

Base::Result<void> PopulateUiElements(
    ::Aero::Meta::Registration& context) noexcept {
    ::Aero::Meta::FillVisualMetadata(context);
    ::Aero::Meta::FillContentElementMetadata(context);
    ::Aero::Meta::FillFrameworkContentElementMetadata(context);
    ::Aero::Meta::FillUIElementMetadata(context);
    ::Aero::Meta::FillFrameworkElementMetadata(context);
    return {};
}

Base::Result<void> PopulateUiMetadata(
    ::Aero::Meta::Registration& context) noexcept {
    PopulateEnumMetadata(context);
    PopulateUiInput(context);
    // Media registers foundational value types such as Point. Resources author
    // Geometry dependency-property defaults that consume those values, so keep
    // Media ahead of Resources in the deterministic metadata bootstrap.
    PopulateUiMedia(context);
    PopulateUiResources(context);
    PopulateUiStyling(context);
    PopulateUiAnimation(context);
    PopulateUiElements(context);
    PopulateInputDevices(context);
    return {};
}

} // namespace Aero
