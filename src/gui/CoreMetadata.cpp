#include "MetaInternals.hpp"

#include <Aero/Meta/Describe.hpp>
#include <Aero/Meta/ValueConversion.hpp>
#include "PropertyInternal.hpp"

namespace Aero::Core {
Base::Result<void> Detail::PopulateCoreMetadata(
    Meta::Registration& context) noexcept {
    Base::Result<void> status;

    status = Meta::Describe<Base::Object>(context).Result();
    if (!status) return status.GetStatus();

    status = Meta::Describe<bool>(context)
        .TextConverter<&ValueConversion::ConvertBoolean>()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Describe<std::int8_t>(context)
        .TextConverter<
            &ValueConversion::ConvertInteger<std::int8_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Describe<std::int16_t>(context)
        .TextConverter<
            &ValueConversion::ConvertInteger<std::int16_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Describe<std::int32_t>(context)
        .TextConverter<
            &ValueConversion::ConvertInteger<std::int32_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Describe<std::int64_t>(context)
        .TextConverter<
            &ValueConversion::ConvertInteger<std::int64_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Describe<std::uint8_t>(context)
        .TextConverter<
            &ValueConversion::ConvertInteger<std::uint8_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Describe<std::uint16_t>(context)
        .TextConverter<
            &ValueConversion::ConvertInteger<std::uint16_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Describe<std::uint32_t>(context)
        .TextConverter<
            &ValueConversion::ConvertInteger<std::uint32_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Meta::Describe<std::uint64_t>(context)
        .TextConverter<
            &ValueConversion::ConvertInteger<std::uint64_t>>()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Describe<double>(context)
        .TextConverter<&ValueConversion::ConvertDouble>()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Describe<Base::String>(context)
        .TextConverter<&ValueConversion::ConvertString>()
        .Result();
    if (!status) return status.GetStatus();

    status = Meta::Describe<Value>(context).Result();
    if (!status) return status.GetStatus();

    status = Meta::Describe<TypeReference>(context).Result();
    if (!status) return status.GetStatus();

    status = Meta::Describe<Base::ResourceUri>(context)
        .ValueSemantics()
        .TextConverter<&ValueConversion::ConvertResourceUri>()
        .Result();
    if (!status) return status.GetStatus();

    return Meta::Describe<DependencyObject>(
        context, TypeFlags::Abstract).Result();
}

} // namespace Aero::Core
