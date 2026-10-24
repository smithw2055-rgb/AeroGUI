#pragma once

// Internal builtin-metadata helpers. Public controls do not declare
// RegisterMetadata; a type with extra metadata defines
// AERO_DESCRIBE(Type) in its cpp, and Populate walks BaseType so a derived
// type is registered after its base.

#include <Aero/ClassHandler.hpp>
#include <Aero/Meta.hpp>

namespace Aero::Meta {

template<class T, typename = void>
struct HasDescribeMetadata : std::false_type {};

template<class T>
struct HasDescribeMetadata<T, std::void_t<decltype(T::DescribeMetadata(std::declval<Registration&>()))>> : std::true_type {};

template<class T, typename = void>
struct HasBaseType : std::false_type {};

template<class T>
struct HasBaseType<T, std::void_t<typename T::BaseType>> : std::true_type {};

template<class T>
struct DescribeHook {
    static void Run(Registration& context) noexcept {
        if (context.ContainsType(TypeTraits<T>::Id())) {
            return;
        }
        if constexpr (HasBaseType<T>::value) {
            using BaseT = typename T::BaseType;
            if constexpr (!std::is_same_v<BaseT, NoMetadataBase> &&
                          !std::is_same_v<BaseT, void> &&
                          !std::is_same_v<BaseT, Base::Object>) {
                if constexpr (HasDescribeMetadata<BaseT>::value) {
                    if (!context.ContainsType(TypeTraits<BaseT>::Id())) {
                        DescribeHook<BaseT>::Run(context);
                    }
                }
            }
        }
        T::DescribeMetadata(context);
    }
};

} // namespace Aero::Meta

// Defines the private DescribeMetadata declared by AERO_DECLARE_TYPE.
// The function is a class member, so it can name protected handlers.
#define AERO_DESCRIBE(Type) \
    void Type::DescribeMetadata( \
        ::Aero::Meta::Registration& context) noexcept
