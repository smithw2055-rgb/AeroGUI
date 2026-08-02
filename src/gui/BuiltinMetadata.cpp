// Consolidated implementation. Keep sections ordered by dependency.

// ===== CoreMetadata =====

#include "MetadataInternal.hpp"
#include "../media/TransformInternals.hpp"

#include <Aero/Meta.hpp>
#include <Aero/Value.hpp>
#include "PropertyInternal.hpp"

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

    status = Meta::Register<Value>(context).Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<TypeReference>(context).Result();
    if (!status) return status.GetStatus();

    status = Meta::Register<Base::ResourceUri>(context)
        .ValueSemantics()
        .TextConverter<&::Aero::Base::Detail::ValueConversion::ConvertResourceUri>()
        .Result();
    if (!status) return status.GetStatus();

    return Meta::Register<DependencyObject>(
        context, TypeFlags::Abstract).Result();
}

} // namespace Aero::Meta


// ===== UiMetadata =====

#include "gui/MetadataInternal.hpp"
#include "gui/StyleInternal.hpp"

#include "gui/MetadataInternal.hpp"
#include <Aero/Meta.hpp>
#include <Aero/Value.hpp>
#include <Aero/Input.hpp>
#include <Aero/Animation.hpp>
#include <Aero/Data.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Layout.hpp>
#include "gui/ElementInternal.hpp"
#include <Aero/FrameworkElement.hpp>
#include <Aero/ContentElement.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Styling.hpp>
#include <Aero/Media/Transforms.hpp>
#include "media/TransformInternals.hpp"
#include "media/EffectInternals.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace Aero::Internal {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Input;
using namespace Aero::Media;
using namespace Aero::Data;
using namespace Aero::Internal::Animation;
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
    status = PopulateUiResources(context);
    if (!status) return status.GetStatus();
    status = PopulateUiStyling(context);
    if (!status) return status.GetStatus();
    status = PopulateUiInput(context);
    if (!status) return status.GetStatus();
    status = PopulateUiMedia(context);
    if (!status) return status.GetStatus();
    status = PopulateUiAnimation(context);
    if (!status) return status.GetStatus();
    status = PopulateUiElements(context);
    if (!status) return status.GetStatus();
    return {};
}

} // namespace Aero::Internal
