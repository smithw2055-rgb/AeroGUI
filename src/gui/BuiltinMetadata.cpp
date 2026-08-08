// Consolidated implementation. Keep sections ordered by dependency.

// ===== CoreMetadata =====

#include "gui/MetadataInternal.hpp"
#include "gui/PropertyInternal.hpp"
#include "gui/FreezableInternal.hpp"
#include "gui/ElementInternal.hpp"
#include "gui/RoutedEventInternal.hpp"
#include "gui/InputInternal.hpp"
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/AnimationInternal.hpp"
#include "gui/StyleInternal.hpp"
#include <Aero/Triggers/Behavior.hpp>
#include <Aero/Triggers/BlendBehaviors.hpp>
#include "gui/MetadataInternal.hpp"
#include "gui/PropertyInternal.hpp"
#include "gui/FreezableInternal.hpp"
#include "gui/ElementInternal.hpp"
#include "gui/RoutedEventInternal.hpp"
#include "gui/InputInternal.hpp"
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/AnimationInternal.hpp"
#include "gui/StyleInternal.hpp"
#include "media/AnimationInternal.hpp"
#include "media/BrushInternal.hpp"
#include "media/EffectInternal.hpp"
#include "media/TransformInternal.hpp"

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
#include <Aero/Gui/Storyboard.hpp>
#include <Aero/Gui/BindingBase.hpp>
#include <Aero/Gui/Brush.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Gui/FrameworkElement.hpp>
#include <Aero/Gui/FrameworkContentElement.hpp>
#include <Aero/Gui/ResourceDictionary.hpp>
#include <Aero/Gui/ControlTemplate.hpp>
#include <Aero/Gui/Transform.hpp>
#include "gui/MetadataInternal.hpp"
#include "gui/PropertyInternal.hpp"
#include "gui/FreezableInternal.hpp"
#include "gui/ElementInternal.hpp"
#include "gui/RoutedEventInternal.hpp"
#include "gui/InputInternal.hpp"
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/AnimationInternal.hpp"
#include "gui/StyleInternal.hpp"
#include "media/AnimationInternal.hpp"
#include "media/BrushInternal.hpp"
#include "media/EffectInternal.hpp"
#include "media/TransformInternal.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace Aero::GuiPrivate::Detail {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Input;
using namespace Aero::Media;
using namespace Aero::Data;
using namespace Aero::Media::Detail::Animation;
namespace {
#include "gui/Support.inl"
#include "gui/Resources.inl"
#include "gui/Styling.inl"
#include "gui/Input.inl"
#include "gui/Media.inl"
#include "gui/Animation.inl"
#include "gui/Elements.inl"
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

} // namespace Aero::GuiPrivate::Detail
