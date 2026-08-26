// Consolidated implementation. Keep sections ordered by dependency.

// ===== CoreMetadata =====

#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp" 
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include "gui/media/MediaState.hpp"

#include <Aero/Meta.hpp>
#include <Aero/Value.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/DispatcherObject.hpp>

namespace Aero::Meta {
Base::Result<void> PopulateCoreMetadata(
    Meta::Registration& context) noexcept {
    Base::Result<void> status;

    status = Meta::Register<Base::Object>(context).Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<bool>(context)
        .TextConverter<&::Aero::Base::ValueConversion::ConvertBoolean>()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<::Aero::Nullable<bool>>(context)
        .TextConverter<
            &::Aero::Base::ValueConversion::
                ConvertNullableBoolean>()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<std::int8_t>(context)
        .TextConverter<
            &::Aero::Base::ValueConversion::ConvertInteger<std::int8_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<std::int16_t>(context)
        .TextConverter<
            &::Aero::Base::ValueConversion::ConvertInteger<std::int16_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<std::int32_t>(context)
        .TextConverter<
            &::Aero::Base::ValueConversion::ConvertInteger<std::int32_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<std::int64_t>(context)
        .TextConverter<
            &::Aero::Base::ValueConversion::ConvertInteger<std::int64_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<std::uint8_t>(context)
        .TextConverter<
            &::Aero::Base::ValueConversion::ConvertInteger<std::uint8_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<std::uint16_t>(context)
        .TextConverter<
            &::Aero::Base::ValueConversion::ConvertInteger<std::uint16_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<std::uint32_t>(context)
        .TextConverter<
            &::Aero::Base::ValueConversion::ConvertInteger<std::uint32_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Register<std::uint64_t>(context)
        .TextConverter<
            &::Aero::Base::ValueConversion::ConvertInteger<std::uint64_t>>()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<double>(context)
        .TextConverter<&::Aero::Base::ValueConversion::ConvertDouble>()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<Base::String>(context)
        .TextConverter<&::Aero::Base::ValueConversion::ConvertString>()
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
        .TextConverter<&::Aero::Base::ValueConversion::ConvertResourceUri>()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<Threading::DispatcherObject>(
        context, TypeFlags::Abstract).Result();
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
using namespace Aero::Media::Animation::Model;
namespace {
#include "gui/meta/Support.inl"
#include "gui/meta/Resources.inl"
#include "gui/meta/Styling.inl"
#include "gui/meta/Input.inl"
#include "gui/meta/Media.inl"
#include "gui/meta/Animation.inl"
#include "gui/meta/Elements.inl"
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
