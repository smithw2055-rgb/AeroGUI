#pragma once

// Shared helper for built-in enum metadata registration.
// Families own AERO_REGISTER_ENUM bodies; BuiltinModules only orchestrates.

#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Meta.hpp>
#include <type_traits>
#include <utility>
#include <cstdint>
#include <limits>

namespace Aero::Meta {

struct EnumReflector {
    template <class E, E V>
    static constexpr Base::StringView RawEnumValueName() noexcept {
#if defined(_MSC_VER)
        constexpr Base::StringView sig(__FUNCSIG__, sizeof(__FUNCSIG__) - 1U);
        std::uint32_t end = sig.SizeBytes();
        while (end > 0U && sig[end - 1U] != '>') --end;
        if (end == 0U) return {};
        --end;
        std::uint32_t start = end;
        while (start > 0U) {
            char c = sig[start - 1U];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
                break;
            }
            --start;
        }
        if (start >= end) return {};
        char first = sig[start];
        if (!((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z') || first == '_')) {
            return {};
        }
        return sig.Substr(start, end - start);
#elif defined(__clang__) || defined(__GNUC__)
        constexpr Base::StringView sig(__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__) - 1U);
        std::uint32_t end = sig.SizeBytes();
        while (end > 0U && sig[end - 1U] != ']') --end;
        if (end == 0U) return {};
        --end;
        std::uint32_t start = end;
        while (start > 0U) {
            char c = sig[start - 1U];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
                break;
            }
            --start;
        }
        if (start >= end) return {};
        char first = sig[start];
        if (!((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z') || first == '_')) {
            return {};
        }
        return sig.Substr(start, end - start);
#else
        return {};
#endif
    }

    template <class E, std::int64_t... Is>
    static void RegisterEnumSequence(TypeBuilder<E>& builder, std::integer_sequence<std::int64_t, Is...>) noexcept {
        auto tryOne = [&builder](auto ic) noexcept {
            constexpr std::int64_t raw = decltype(ic)::value;
            using U = std::underlying_type_t<E>;
            if constexpr (raw >= static_cast<std::int64_t>(std::numeric_limits<U>::min()) &&
                          raw <= static_cast<std::int64_t>(std::numeric_limits<U>::max())) {
                constexpr E val = static_cast<E>(static_cast<U>(raw));
                constexpr Base::StringView name = RawEnumValueName<E, val>();
                if constexpr (!name.Empty()) {
                    builder.Value(name, val);
                }
            }
        };
        (tryOne(std::integral_constant<std::int64_t, Is>{}), ...);
    }

    template <class E, std::size_t... Is>
    static void RegisterEnumContiguous(TypeBuilder<E>& builder, std::index_sequence<Is...>) noexcept {
        RegisterEnumSequence(
            builder,
            std::integer_sequence<std::int64_t, (static_cast<std::int64_t>(Is) - 1)...>{});
    }

    template <class E>
    static void RegisterEnumProbes(TypeBuilder<E>& builder) noexcept {
        // 1. 扫描常用连续整数区间 [-1, 64]
        RegisterEnumContiguous(builder, std::make_index_sequence<66>{});

        // 2. 扫描常用位掩码 (1<<6 到 1<<30)
        RegisterEnumSequence(builder, std::integer_sequence<std::int64_t,
            (1LL << 6),  (1LL << 7),  (1LL << 8),  (1LL << 9),
            (1LL << 10), (1LL << 11), (1LL << 12), (1LL << 13),
            (1LL << 14), (1LL << 15), (1LL << 16), (1LL << 20),
            (1LL << 24), (1LL << 28), (1LL << 30)>{});
    }
};

template <class E>
Base::Result<void> AutoRegisterEnum(
    Registration& context,
    Base::StringView customName = {}) noexcept {
    static_assert(std::is_enum_v<E>, "AutoRegisterEnum requires an enum type");
    if (context.ContainsType(TypeOf<E>())) {
        return {};
    }
    const Base::StringView name = customName.Empty() ? TypeTraits<E>::Name() : customName;
    auto builder = Register<E>(context, name);
    EnumReflector::RegisterEnumProbes(builder);
    return builder.Result();
}

} // namespace Aero::Meta

namespace Aero {

template<class T, class TPopulate>
inline Base::Result<void> RegisterEnum(
    Meta::Registration& context,
    Base::StringView name,
    TPopulate&& populate) noexcept {
    auto description = Meta::Register<T>(context, name);
    populate(description);
    return description.Result();
}

#define AERO_REGISTER_ENUM(Type, Name, Values) \
    static_cast<void>(::Aero::RegisterEnum<Type>(context, Name, \
        [](auto& description) { Values }))

#define AERO_AUTO_REGISTER_ENUM(Type) \
    static_cast<void>(::Aero::Meta::AutoRegisterEnum<Type>(context, #Type))

#define AERO_AUTO_REGISTER_NAMED_ENUM(Type, Name) \
    static_cast<void>(::Aero::Meta::AutoRegisterEnum<Type>(context, Name))

void PopulateInputEnums(
    Meta::Registration& context) noexcept;
void PopulateAnimationEnums(
    Meta::Registration& context) noexcept;
void PopulateInteractivityEnums(
    Meta::Registration& context) noexcept;
void PopulateElementEnums(
    Meta::Registration& context) noexcept;
void PopulateMediaEnums(
    Meta::Registration& context) noexcept;
void PopulateShapesEnums(
    Meta::Registration& context) noexcept;
void PopulateDataEnums(
    Meta::Registration& context) noexcept;
void PopulateTextEnums(
    Meta::Registration& context) noexcept;
void PopulateControlsEnums(
    Meta::Registration& context) noexcept;
Base::Result<void> PopulateMediaValueTypes(
    Meta::Registration& context) noexcept;
Base::Result<void> PopulateAnimationTypes(
    Meta::Registration& context) noexcept;
Base::Result<void> PopulateVisualStateMetadata(
    Meta::Registration& context) noexcept;

} // namespace Aero
