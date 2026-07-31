#include "CoreMetadata.hpp"

#include <Aero/Core/Metadata/Describe.hpp>
#include <Aero/Core/Metadata/ValueConversion.hpp>
#include "../property/EffectiveValueEngine.hpp"

namespace Aero::Core {
Base::Result<void> Detail::PopulateCoreMetadata(
    MetadataContext& context) noexcept {
    Base::Result<void> status;

    status = Describe<Base::Object>(context).Result();
    if (!status) return status.GetStatus();

    status = Describe<bool>(context)
        .TextConverter<&ValueConversion::ConvertBoolean>()
        .Result();
    if (!status) return status.GetStatus();

    status = Describe<std::int8_t>(context)
        .TextConverter<
            &ValueConversion::ConvertInteger<std::int8_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Describe<std::int16_t>(context)
        .TextConverter<
            &ValueConversion::ConvertInteger<std::int16_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Describe<std::int32_t>(context)
        .TextConverter<
            &ValueConversion::ConvertInteger<std::int32_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Describe<std::int64_t>(context)
        .TextConverter<
            &ValueConversion::ConvertInteger<std::int64_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Describe<std::uint8_t>(context)
        .TextConverter<
            &ValueConversion::ConvertInteger<std::uint8_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Describe<std::uint16_t>(context)
        .TextConverter<
            &ValueConversion::ConvertInteger<std::uint16_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Describe<std::uint32_t>(context)
        .TextConverter<
            &ValueConversion::ConvertInteger<std::uint32_t>>()
        .Result();
    if (!status) return status.GetStatus();
    status = Describe<std::uint64_t>(context)
        .TextConverter<
            &ValueConversion::ConvertInteger<std::uint64_t>>()
        .Result();
    if (!status) return status.GetStatus();

    status = Describe<double>(context)
        .TextConverter<&ValueConversion::ConvertDouble>()
        .Result();
    if (!status) return status.GetStatus();

    status = Describe<Base::String>(context)
        .TextConverter<&ValueConversion::ConvertString>()
        .Result();
    if (!status) return status.GetStatus();

    status = Describe<Value>(context).Result();
    if (!status) return status.GetStatus();

    status = Describe<TypeReference>(context).Result();
    if (!status) return status.GetStatus();

    status = Describe<Base::ResourceUri>(context)
        .ValueSemantics()
        .TextConverter<&ValueConversion::ConvertResourceUri>()
        .Result();
    if (!status) return status.GetStatus();

    return Describe<DependencyObject>(
        context, TypeFlags::Abstract).Result();
}

} // namespace Aero::Core
