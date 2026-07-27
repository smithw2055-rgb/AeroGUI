#include <Aero/Core/Metadata/CoreMetadata.hpp>

#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Metadata.hpp>

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace Aero::Core {
namespace {

bool EqualsAsciiInsensitive(
    Base::StringView value,
    const char* literal) noexcept {
    std::uint32_t size = 0U;
    while (literal[size] != '\0') ++size;
    if (value.SizeBytes() != size) return false;
    for (std::uint32_t index = 0U; index < size; ++index) {
        if (std::tolower(static_cast<unsigned char>(value[index])) !=
            std::tolower(static_cast<unsigned char>(literal[index]))) {
            return false;
        }
    }
    return true;
}

Base::StringView Trim(Base::StringView value) noexcept {
    std::uint32_t begin = 0U;
    std::uint32_t end = value.SizeBytes();
    while (begin < end &&
        std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    while (end > begin &&
        std::isspace(static_cast<unsigned char>(value[end - 1U]))) {
        --end;
    }
    return value.Substr(begin, end - begin);
}

Base::Result<double> ParseDouble(
    Base::StringView text) noexcept {
    Base::String buffer;
    Base::Result<void> assigned =
        buffer.TryAssign(Trim(text));
    if (!assigned) return assigned.GetStatus();
    char* end = nullptr;
    const double value = std::strtod(buffer.CStr(), &end);
    if (end == buffer.CStr() ||
        *end != '\0' ||
        !std::isfinite(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Text is not a finite number");
    }
    return value;
}

Base::Result<Value> ConvertBoolean(
    TypeId type,
    Base::StringView text,
    void*) noexcept {
    const Base::StringView value = Trim(text);
    if (EqualsAsciiInsensitive(value, "true")) {
        return Value::FromBoolean(type, true);
    }
    if (EqualsAsciiInsensitive(value, "false")) {
        return Value::FromBoolean(type, false);
    }
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "Boolean text must be true or false");
}

Base::Result<Value> ConvertUnsigned(
    TypeId type,
    Base::StringView text,
    void*) noexcept {
    Base::String buffer;
    Base::Result<void> assigned =
        buffer.TryAssign(Trim(text));
    if (!assigned) return assigned.GetStatus();
    char* end = nullptr;
    errno = 0;
    const unsigned long long value =
        std::strtoull(buffer.CStr(), &end, 10);
    if (end == buffer.CStr() ||
        *end != '\0' ||
        errno == ERANGE ||
        value > static_cast<unsigned long long>(UINT32_MAX) ||
        (!buffer.Empty() && buffer.View()[0] == '-')) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Text is not an unsigned integer");
    }
    return Value::FromUnsignedInteger(
        type, static_cast<std::uint64_t>(value));
}

Base::Result<Value> ConvertDoubleValue(
    TypeId type,
    Base::StringView text,
    void*) noexcept {
    Base::Result<double> value = ParseDouble(text);
    return value
        ? Base::Result<Value>(
            Value::FromDouble(type, value.Value()))
        : Base::Result<Value>(value.GetStatus());
}

Base::Result<Value> ConvertString(
    TypeId type,
    Base::StringView text,
    void*) noexcept {
    return Value::TryFromString(type, text);
}

} // namespace

Base::Result<void> Detail::PopulateCoreMetadata(
    RegistrationContext& context) noexcept {
    Base::Result<void> status =
        Describe<Base::Object>(context).Finish();
    if (!status) return status.GetStatus();

    status = DescribePrimitive<bool>(context)
        .TextConverter(&ConvertBoolean)
        .Finish();
    if (!status) return status.GetStatus();

    status = DescribePrimitive<std::uint32_t>(context)
        .TextConverter(&ConvertUnsigned)
        .Finish();
    if (!status) return status.GetStatus();

    status = DescribePrimitive<double>(context)
        .TextConverter(&ConvertDoubleValue)
        .Finish();
    if (!status) return status.GetStatus();

    status = DescribePrimitive<Base::String>(context)
        .TextConverter(&ConvertString)
        .Finish();
    if (!status) return status.GetStatus();

    return Describe<DependencyObject>(
        context, TypeFlags::Abstract).Finish();
}

} // namespace Aero::Core
