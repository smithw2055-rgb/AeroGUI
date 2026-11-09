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
        EnsureTypeClassRegistered<T>();
        if (context.ContainsType(TypeTraits<T>::Id())) {
            return;
        }
        if constexpr (HasBaseType<T>::value) {
            using BaseT = typename T::BaseType;
            if constexpr (!std::is_same_v<BaseT, NoMetadataBase> &&
                          !std::is_same_v<BaseT, void> &&
                          !std::is_same_v<BaseT, Base::Object>) {
                if constexpr (HasDescribeMetadata<BaseT>::value || HasComponentDescription<BaseT>::value) {
                    if (!context.ContainsType(TypeTraits<BaseT>::Id())) {
                        DescribeHook<BaseT>::Run(context);
                    }
                }
            }
        }
        if constexpr (HasComponentDescription<T>::value) {
            static_cast<void>(RegisterComponentType<T>(context));
        } else {
            T::DescribeMetadata(context);
        }
    }
};

template<class... Ts>
inline void DescribeTypes(Registration& context) noexcept {
    (DescribeHook<Ts>::Run(context), ...);
}

struct AutoDescribeNode {
    using Callback = void (*)(Registration&) noexcept;
    Callback run = nullptr;
    AutoDescribeNode* next = nullptr;

    static AutoDescribeNode*& Head() noexcept {
        static AutoDescribeNode* s_head = nullptr;
        return s_head;
    }

    explicit AutoDescribeNode(Callback cb) noexcept
        : run(cb), next(Head()) {
        Head() = this;
    }

    static void RunAll(Registration& context) noexcept {
        for (const AutoDescribeNode* cur = Head(); cur != nullptr; cur = cur->next) {
            if (cur->run != nullptr) {
                cur->run(context);
            }
        }
    }
};

} // namespace Aero::Meta

// Defines the private DescribeMetadata declared by AERO_DECLARE_TYPE.
// The function is a class member, so it can name protected handlers.
#define AERO_DESCRIBE(Type) \
    void Type::DescribeMetadata( \
        ::Aero::Meta::Registration& context) noexcept


#define AERO_DESCRIBE_DEFAULT(Type) \
    AERO_DESCRIBE(Type) { \
        ::Aero::Meta::TypeBuilder<Type> type(context); \
        type.Factory(); \
    }
