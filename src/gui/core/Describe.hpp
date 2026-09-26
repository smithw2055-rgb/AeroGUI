#pragma once

// Internal builtin-metadata helpers. Public controls do not declare
// RegisterMetadata; a type with extra metadata defines
// AERO_DESCRIBE(Type) in its cpp, and Populate walks BaseType so a derived
// type is registered after its base.

#include <Aero/Base/Assert.hpp>
#include <Aero/ClassHandler.hpp>
#include <Aero/Meta.hpp>

#include <cstdint>
#include <type_traits>

namespace Aero::Meta {

using DescribeFn = void (*)(Registration&) noexcept;

struct DescribeBinding {
    TypeId type = InvalidTypeId;
    DescribeFn run = nullptr;
};

inline DescribeBinding* DescribeTable() noexcept {
    static DescribeBinding table[160];
    return table;
}

inline std::uint32_t& DescribeTableCount() noexcept {
    static std::uint32_t count = 0U;
    return count;
}

inline void AddDescribe(TypeId type, DescribeFn run) noexcept {
    std::uint32_t& count = DescribeTableCount();
    DescribeBinding* table = DescribeTable();
    AERO_ASSERT(count < 160U);
    if (count >= 160U) return;
    for (std::uint32_t index = 0U; index < count; ++index) {
        if (table[index].type == type) {
            table[index].run = run;
            return;
        }
    }
    table[count].type = type;
    table[count].run = run;
    ++count;
}

template<class T>
void AddDescribe(DescribeFn run) noexcept {
    AddDescribe(T::StaticTypeId(), run);
}

inline DescribeFn FindDescribe(TypeId type) noexcept {
    const std::uint32_t count = DescribeTableCount();
    const DescribeBinding* table = DescribeTable();
    for (std::uint32_t index = 0U; index < count; ++index) {
        if (table[index].type == type) return table[index].run;
    }
    return nullptr;
}

template<class T, class = void>
struct HasMetadataBaseType : std::false_type {};

template<class T>
struct HasMetadataBaseType<T, std::void_t<typename T::BaseType>>
    : std::true_type {};

template<class T, class = void>
struct HasStaticTypeIdMethod : std::false_type {};

template<class T>
struct HasStaticTypeIdMethod<
    T,
    std::void_t<decltype(T::StaticTypeId())>> : std::true_type {};

template<class T, int Depth = 0>
void EnsureRegistered(Registration& registration) noexcept {
    static_assert(Depth < 32, "metadata base chain is too deep");
    if (registration.ContainsType(T::StaticTypeId())) return;
    if constexpr (HasMetadataBaseType<T>::value) {
        using Base = typename T::BaseType;
        if constexpr (!std::is_same_v<Base, T> &&
                      !std::is_same_v<Base, NoMetadataBase> &&
                      HasStaticTypeIdMethod<Base>::value) {
            EnsureRegistered<Base, Depth + 1>(registration);
        }
    }
    if (DescribeFn run = FindDescribe(T::StaticTypeId())) {
        run(registration);
        return;
    }
    // Factory() is only instantiated for concrete default-constructible
    // types. Abstract types and types with a protected constructor supply
    // AERO_DESCRIBE instead.
    if constexpr (std::is_abstract_v<T> ||
                  !std::is_default_constructible_v<T>) {
        AERO_ASSERT(false);
    } else {
        Register<T>(registration).Factory();
    }
}

template<class... Ts>
void EnsureRegisteredPack(Registration& registration) noexcept {
    (EnsureRegistered<Ts>(registration), ...);
}

template<class T>
struct DescribeHook {
    static void Run(Registration& context) noexcept {
        T::DescribeMetadata(context);
    }
};

} // namespace Aero::Meta

// Defines the private DescribeMetadata declared by AERO_DECLARE_TYPE.
// The function is a class member, so it can name protected handlers.
#define AERO_DESCRIBE(Type) \
    void Type::DescribeMetadata( \
        ::Aero::Meta::Registration& context) noexcept
