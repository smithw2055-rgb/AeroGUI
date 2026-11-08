#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Data/NotifyPropertyChanged.hpp>
#include <Aero/RelayCommand.hpp>
#include <Aero/Value.hpp>
#include <Aero/Meta.hpp>

namespace Aero {

class AERO_GUI_API ViewModelBase : public Base::Object, public Data::NotifyPropertyChanged<ViewModelBase> {
    AERO_DECLARE_TYPE(ViewModelBase, Base::Object)

public:
    ViewModelBase() noexcept : ViewModelBase(StaticTypeId()) {}
    ~ViewModelBase() override = default;

protected:
    explicit ViewModelBase(Base::MetaTypeId runtimeType) noexcept {
        SetContextSlot(Meta::InternCompactTypeId(runtimeType));
    }

public:
    Base::MetaTypeId RuntimeType() const noexcept override {
        return Meta::ResolveCompactTypeId(ContextSlot());
    }
};

template<class T>
inline bool MvvmPropertyEquals(const T& a, const T& b) noexcept {
    if constexpr (std::is_same_v<T, ::Aero::Base::String>) {
        return a.View() == b.View();
    } else {
        return a == b;
    }
}

template<std::size_t N>
struct ClrNamespaceStorage {
    char data[N + 16]{};
    std::size_t size = 0;
    constexpr explicit ClrNamespaceStorage(const char (&qname)[N]) noexcept {
        const char prefix[] = "clr-namespace:";
        std::size_t p = 0;
        for (; p < sizeof(prefix) - 1; ++p) {
            data[p] = prefix[p];
        }
        std::size_t dot = N - 1;
        for (std::size_t i = 0; i < N - 1; ++i) {
            if (qname[i] == '.') {
                dot = i;
                break;
            }
        }
        for (std::size_t i = 0; i < dot; ++i) {
            data[p++] = qname[i];
        }
        data[p] = '\0';
        size = p;
    }
};

using RelayCommand = Input::RelayCommand;

} // namespace Aero

#define AERO_VIEWMODEL(TClass) \
    AERO_DECLARE_TYPE(TClass, ::Aero::ViewModelBase)

#define AERO_OBSERVABLE_PROPERTY(type, name, defaultValue) \
private: \
    type name##_ = (defaultValue); \
public: \
    const type& Get##name() const noexcept { return name##_; } \
    bool Set##name(const type& value) noexcept { \
        if (::Aero::MvvmPropertyEquals(name##_, value)) return false; \
        name##_ = value; \
        constexpr ::Aero::Meta::MemberId kMemberId = \
            ::Aero::Meta::MakePropertyId(Self::StaticTypeId(), #name); \
        this->RaisePropertyChanged(kMemberId); \
        return true; \
    }

#define AERO_RELAY_COMMAND(name, ...) \
    ::Aero::Base::Ref<::Aero::Input::RelayCommand> name##Command = \
        ::Aero::Base::MakeRef<::Aero::Input::RelayCommand>(__VA_ARGS__)

#define AERO_REFLECT_3(ClassName, BaseClass, QName) \
    inline static constexpr auto __ClrNamespace = ::Aero::ClrNamespaceStorage(QName); \
    AERO_DECLARE_TYPE_NAMED(ClassName, BaseClass, std::string_view(__ClrNamespace.data), #ClassName) \
    static void DescribeComponent(::Aero::Meta::TypeBuilder<ClassName>& builder) noexcept { \
        if constexpr (std::is_base_of_v<::Aero::ViewModelBase, ClassName>) { \
            builder.PropertyChangeNotifications(); \
        } \
        __ReflectMembers(builder); \
    } \
    static void __ReflectMembers(::Aero::Meta::TypeBuilder<ClassName>& builder) noexcept

#define AERO_REFLECT_2(ClassName, BaseClass) \
    AERO_REFLECT_3(ClassName, BaseClass, #ClassName)

#define AERO_REFLECT_GET_MACRO(_1, _2, _3, NAME, ...) NAME
#define AERO_REFLECT(...) \
    AERO_REFLECT_GET_MACRO(__VA_ARGS__, AERO_REFLECT_3, AERO_REFLECT_2)(__VA_ARGS__)

#define AERO_BIND_PROPERTY(name, getter, setter) \
    builder.Property(#name, getter, setter)

#define AERO_BIND_READONLY_PROPERTY(name, getter) \
    builder.PropertyReadOnly(#name, getter)

#define AERO_BIND_FIELD(name, memberPtr) \
    builder.template BindField<memberPtr>(name)

#define AERO_VM_FIELD(type, name, memberName) \
private: \
    type memberName{}; \
public: \
    const type& Get##name() const noexcept { return memberName; } \
    bool Set##name(type value) noexcept { \
        if (::Aero::MvvmPropertyEquals(memberName, value)) return false; \
        memberName = std::move(value); \
        this->RaisePropertyChanged(#name); \
        return true; \
    }

#define AERO_VM_PROPERTY(type, name, memberName) \
    AERO_VM_FIELD(type, name, memberName)
