// Consolidated implementation. Keep sections ordered by dependency.

// ===== CoreMetadata =====

#include "gui/metadata/MetadataRuntime.hpp"
#include "gui/property/PropertyRuntime.hpp"
#include "gui/base/FreezableRuntime.hpp"
#include "gui/base/ElementRuntime.hpp"
#include "gui/base/RoutedEventRuntime.hpp"
#include "gui/input/InputRuntime.hpp"
#include "gui/layout/LayoutRuntime.hpp"
#include "gui/binding/BindingRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/resources/StyleRuntime.hpp"
#include <Aero/Triggers/Behavior.hpp>
#include <Aero/Triggers/BlendBehaviors.hpp>
#include "gui/metadata/MetadataRuntime.hpp"
#include "gui/property/PropertyRuntime.hpp"
#include "gui/base/FreezableRuntime.hpp"
#include "gui/base/ElementRuntime.hpp"
#include "gui/base/RoutedEventRuntime.hpp"
#include "gui/input/InputRuntime.hpp"
#include "gui/layout/LayoutRuntime.hpp"
#include "gui/binding/BindingRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/resources/StyleRuntime.hpp"
#include "gui/media/AnimationRuntime.hpp"
#include "gui/media/BrushRuntime.hpp"
#include "gui/media/EffectRuntime.hpp"
#include "gui/media/TransformRuntime.hpp"

#include <Aero/Meta.hpp>
#include <Aero/Value.hpp>
#include <Aero/Freezable.hpp>

namespace Aero::Meta {
Base::Result<void> Detail::PopulateCoreMetadata(
    Meta::Registration& context) noexcept {
    Base::Result<void> status;

    status = Meta::Register<Base::Object>(context).Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<bool>(context)
        .TextConverter<&::Aero::Base::Detail::ValueConversion::ConvertBoolean>()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<::Aero::Nullable<bool>>(context)
        .TextConverter<
            &::Aero::Base::Detail::ValueConversion::
                ConvertNullableBoolean>()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<std::int8_t>(context)
        .TextConverter<
            &::Aero::Base::Detail::ValueConversion::ConvertInteger<std::int8_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<std::int16_t>(context)
        .TextConverter<
            &::Aero::Base::Detail::ValueConversion::ConvertInteger<std::int16_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<std::int32_t>(context)
        .TextConverter<
            &::Aero::Base::Detail::ValueConversion::ConvertInteger<std::int32_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<std::int64_t>(context)
        .TextConverter<
            &::Aero::Base::Detail::ValueConversion::ConvertInteger<std::int64_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<std::uint8_t>(context)
        .TextConverter<
            &::Aero::Base::Detail::ValueConversion::ConvertInteger<std::uint8_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<std::uint16_t>(context)
        .TextConverter<
            &::Aero::Base::Detail::ValueConversion::ConvertInteger<std::uint16_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<std::uint32_t>(context)
        .TextConverter<
            &::Aero::Base::Detail::ValueConversion::ConvertInteger<std::uint32_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<std::uint64_t>(context)
        .TextConverter<
            &::Aero::Base::Detail::ValueConversion::ConvertInteger<std::uint64_t>>()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<double>(context)
        .TextConverter<&::Aero::Base::Detail::ValueConversion::ConvertDouble>()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<Base::String>(context)
        .TextConverter<&::Aero::Base::Detail::ValueConversion::ConvertString>()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<Value>(context)
        .ValueSemantics()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<TypeReference>(context).Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<Base::ResourceUri>(context)
        .ValueSemantics()
        .TextConverter<&::Aero::Base::Detail::ValueConversion::ConvertResourceUri>()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<DependencyObject>(
        context, TypeFlags::Abstract).Result();
    if (!status) return status.GetStatus();

    return Meta::Register<Freezable>(
        context, TypeFlags::Abstract).Result();
}

} // namespace Aero::Meta


// ===== UiMetadata =====


#include <Aero/Input.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Media/Transforms.hpp>
#include "gui/metadata/MetadataRuntime.hpp"
#include "gui/property/PropertyRuntime.hpp"
#include "gui/base/FreezableRuntime.hpp"
#include "gui/base/ElementRuntime.hpp"
#include "gui/base/RoutedEventRuntime.hpp"
#include "gui/input/InputRuntime.hpp"
#include "gui/layout/LayoutRuntime.hpp"
#include "gui/binding/BindingRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/resources/StyleRuntime.hpp"
#include "gui/media/AnimationRuntime.hpp"
#include "gui/media/BrushRuntime.hpp"
#include "gui/media/EffectRuntime.hpp"
#include "gui/media/TransformRuntime.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Input;
using namespace Aero::Media;
using namespace Aero::Data;
using namespace Aero::Media::Animation::Runtime;
namespace {
#include "gui/metadata/Support.inl"
#include "gui/metadata/Resources.inl"
#include "gui/metadata/Styling.inl"
#include "gui/metadata/Input.inl"
#include "gui/metadata/Media.inl"
#include "gui/metadata/Animation.inl"
#include "gui/metadata/Elements.inl"
} // namespace

Base::Result<void> PopulateUiMetadata(
    ::Aero::Meta::Registration& context) noexcept {
    Base::Result<void> status;
    status = PopulateEnumMetadata(context);
    if (!status) return status.GetStatus();
    status = PopulateUiInput(context);
    if (!status) return status.GetStatus();
    // Media registers foundational value types such as Point. Resources author
    // Geometry dependency-property defaults that consume those values, so keep
    // Media ahead of Resources in the deterministic metadata bootstrap.
    status = PopulateUiMedia(context);
    if (!status) return status.GetStatus();
    status = PopulateUiResources(context);
    if (!status) return status.GetStatus();
    status = PopulateUiStyling(context);
    if (!status) return status.GetStatus();
    status = PopulateUiAnimation(context);
    if (!status) return status.GetStatus();
    status = PopulateUiElements(context);
    if (!status) return status.GetStatus();
    return {};
}

} // namespace Aero
