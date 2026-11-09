#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Data/NotifyPropertyChanged.hpp>
#include <Aero/RelayCommand.hpp>
#include <Aero/Value.hpp>
#include <Aero/Meta.hpp>

namespace Aero {

using Base::DefaultTypeClassFactoryOrNull;
using Base::ResolveBaseStaticClassType;
using Base::RootObjectClassType;
using Base::TypeClass;
using Base::TypeProperty;

template<class TOwner>
class TypeClassBuilder {
public:
    explicit TypeClassBuilder(TypeClass& typeClass) noexcept
        : type_(typeClass) {}

    TypeClassBuilder& ComponentUri(Base::StringView uri) noexcept {
        type_.SetComponentUri(uri);
        return *this;
    }

    // Compile-time NTTP static read/write thunk generation with zero heap allocation.
    template<auto Getter, auto Setter = nullptr>
    TypeClassBuilder& Property(Base::StringView name) noexcept {
        using RawReturn = std::invoke_result_t<decltype(Getter), const TOwner&>;
        using TValue = Meta::NormalizePropertyValueType<RawReturn>;

        static constexpr TypeProperty::GetterFn kGet = [](const Base::Object* obj, void* out) noexcept {
            if constexpr (std::is_same_v<TValue, Base::String> &&
                          std::is_same_v<std::remove_cv_t<std::remove_reference_t<RawReturn>>, Base::StringView>) {
                (void)static_cast<Base::String*>(out)->Assign(
                    std::invoke(Getter, *static_cast<const TOwner*>(obj)));
            } else {
                *static_cast<TValue*>(out) = std::invoke(Getter, *static_cast<const TOwner*>(obj));
            }
        };
        static constexpr TypeProperty::SetterFn kSet = []() constexpr -> TypeProperty::SetterFn {
            if constexpr (std::is_same_v<decltype(Setter), std::nullptr_t>) {
                return nullptr;
            } else {
                return [](Base::Object* obj, const void* in) noexcept {
                    auto& owner = *static_cast<TOwner*>(obj);
                    const auto& val = *static_cast<const TValue*>(in);
                    if constexpr (std::is_same_v<TValue, Base::String> &&
                                  std::is_invocable_v<decltype(Setter), TOwner&, Base::StringView>) {
                        (void)std::invoke(Setter, owner, val.View());
                    } else {
                        (void)std::invoke(Setter, owner, val);
                    }
                };
            }
        }();
        static constexpr Meta::PropertyGetCallback kMetaGet =
            &Meta::GetOrdinaryProperty<TOwner, TValue, Getter>;
        static constexpr Meta::PropertySetCallback kMetaSet = []() constexpr -> Meta::PropertySetCallback {
            if constexpr (std::is_same_v<decltype(Setter), std::nullptr_t>) {
                return nullptr;
            } else {
                return &Meta::SetOrdinaryProperty<TOwner, TValue, Setter>;
            }
        }();

        static const TypeProperty s_prop{
            name,
            Meta::MakePropertyId(Meta::TypeOf<TOwner>(), name),
            Meta::ValueCodec<TValue>::Type(),
            kGet,
            kSet,
            kMetaGet,
            kMetaSet};
        if (type_.FindProperty(name, false) == nullptr) {
            type_.AddProperty(&s_prop);
        }
        return *this;
    }

    template<auto Getter>
    TypeClassBuilder& PropertyReadOnly(Base::StringView name) noexcept {
        return Property<Getter, nullptr>(name);
    }

    template<auto Member>
    TypeClassBuilder& BindField(Base::StringView name) noexcept {
        Meta::RegisterBoundFieldEntry(Meta::TypeOf<TOwner>(), name, &Meta::BoundFieldInvoker<TOwner, Member>);
        return *this;
    }

private:
    TypeClass& type_;
};

class AERO_GUI_API ViewModelBase : public Base::Object, public Data::NotifyPropertyChanged<ViewModelBase> {
    AERO_DECLARE_TYPE(ViewModelBase, Base::Object)

public:
    ViewModelBase() noexcept = default;
    ~ViewModelBase() override = default;

protected:
    explicit ViewModelBase(Base::MetaTypeId) noexcept {}
};

template<class T>
inline bool MvvmPropertyEquals(const T& a, const T& b) noexcept {
    if constexpr (std::is_same_v<T, ::Aero::Base::String>) {
        return a.View() == b.View();
    } else {
        return a == b;
    }
}

template<class T, class TInit>
inline T MakeMvvmDefaultValue(TInit&& init) noexcept {
    if constexpr (std::is_same_v<T, ::Aero::Base::String> &&
                  std::is_convertible_v<TInit, ::Aero::Base::StringView>) {
        ::Aero::Base::String result; (void)result.Assign(::Aero::Base::StringView(init)); return result;
    } else {
        return T(std::forward<TInit>(init));
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
public: \
    static const ::Aero::TypeProperty* name##Property() noexcept { \
        using OwnerType = Self; \
        using NormType = ::Aero::Meta::NormalizePropertyValueType<type>; \
        static constexpr ::Aero::TypeProperty::GetterFn kGet = [](const ::Aero::Base::Object* obj, void* out) noexcept { \
            *static_cast<NormType*>(out) = static_cast<const OwnerType*>(obj)->Get##name(); \
        }; \
        static constexpr ::Aero::TypeProperty::SetterFn kSet = [](::Aero::Base::Object* obj, const void* in) noexcept { \
            (void)static_cast<OwnerType*>(obj)->Set##name(*static_cast<const NormType*>(in)); \
        }; \
        static constexpr ::Aero::Meta::PropertyGetCallback kMetaGet = \
            &::Aero::Meta::GetOrdinaryProperty<OwnerType, NormType, &OwnerType::Get##name>; \
        static constexpr ::Aero::Meta::PropertySetCallback kMetaSet = \
            &::Aero::Meta::SetOrdinaryProperty<OwnerType, NormType, &OwnerType::Set##name>; \
        static const ::Aero::TypeProperty s_prop{ \
            #name, \
            ::Aero::Meta::MakePropertyId(OwnerType::StaticTypeId(), #name), \
            ::Aero::Meta::ValueCodec<NormType>::Type(), \
            kGet, \
            kSet, \
            kMetaGet, \
            kMetaSet}; \
        auto* cls = const_cast<::Aero::TypeClass*>(OwnerType::StaticClassType()); \
        if (cls != nullptr) { \
            if (const ::Aero::TypeProperty* existing = cls->FindProperty(#name, false)) { \
                return existing; \
            } \
            cls->AddProperty(&s_prop); \
        } \
        return &s_prop; \
    } \
private: \
    type name##_ = (static_cast<void>(name##Property()), ::Aero::MakeMvvmDefaultValue<type>(defaultValue)); \
public: \
    const type& Get##name() const noexcept { return name##_; } \
    bool Set##name(const type& value) noexcept { \
        if (::Aero::MvvmPropertyEquals(name##_, value)) return false; \
        name##_ = value; \
        using EnclosingType = std::remove_cv_t<std::remove_pointer_t<decltype(this)>>; \
        if constexpr (std::is_same_v<typename EnclosingType::Self, EnclosingType>) { \
            const ::Aero::TypeProperty* const prop = name##Property(); \
            if (this->GetClassType() == EnclosingType::StaticClassType()) { \
                this->RaisePropertyChanged(prop); \
            } else { \
                this->RaisePropertyChanged(#name); \
            } \
        } else { \
            this->RaisePropertyChanged(#name); \
        } \
        return true; \
    }

#define AERO_RELAY_COMMAND(name, ...) \
    ::Aero::Base::Ref<::Aero::Input::RelayCommand> name##Command = \
        ::Aero::Base::MakeRef<::Aero::Input::RelayCommand>(__VA_ARGS__)

#define AERO_REFLECT_3(ClassName, BaseClass, QName) \
    inline static constexpr auto __ClrNamespace = ::Aero::ClrNamespaceStorage(QName); \
    AERO_DECLARE_TYPE_NAMED(ClassName, BaseClass, ::Aero::Base::StringView(__ClrNamespace.data, static_cast<std::uint32_t>(__ClrNamespace.size)), #ClassName) \
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

// 4-in-1 unified inline reflection macro (TypeClass* pointer identity + TypeClassBuilder + Metadata).
#define AERO_REFLECT_INLINE_3(ClassName, BaseClass, XmlNs) \
    AERO_DECLARE_TYPE_NAMED_BASE(ClassName, BaseClass, ::Aero::Base::StringView(XmlNs), #ClassName) \
    static const ::Aero::TypeClass* StaticClassType() noexcept { \
        static ::Aero::TypeClass s_type{ \
            StaticTypeId(), \
            StaticMetadataNamespace(), \
            StaticMetadataName(), \
            ::Aero::ResolveBaseStaticClassType<BaseClass>(), \
            ::Aero::DefaultTypeClassFactoryOrNull<ClassName>() \
        }; \
        static const bool s_init = ([]() noexcept { \
            ::Aero::TypeClassBuilder<ClassName> meta(s_type); \
            ClassName::StaticFillClassType(meta); \
            return true; \
        })(); \
        (void)s_init; \
        return &s_type; \
    } \
    const ::Aero::TypeClass* GetClassType() const noexcept { \
        return StaticClassType(); \
    } \
    template<class TBuilder = ::Aero::Meta::TypeBuilder<ClassName>> \
    static void DescribeComponent(TBuilder& builder) noexcept { \
        if constexpr (std::is_base_of_v<::Aero::ViewModelBase, ClassName>) { \
            builder.PropertyChangeNotifications(); \
        } \
        const ::Aero::TypeClass* cls = StaticClassType(); \
        if (!cls->ComponentUri().Empty()) { \
            builder.ComponentUri(cls->ComponentUri()); \
        } \
        for (const ::Aero::TypeProperty* prop : cls->Properties()) { \
            if (prop == nullptr) continue; \
            ::Aero::Meta::PropertyRegistration reg; \
            reg.name = prop->Name(); \
            reg.valueType = prop->ValueTypeId(); \
            reg.flags = prop->IsReadOnly() ? ::Aero::Meta::PropertyFlags::ReadOnly : ::Aero::Meta::PropertyFlags::None; \
            if (prop->ValueTypeId() == ::Aero::Meta::ValueCodec<::Aero::Meta::Value>::Type()) { \
                reg.flags = reg.flags | ::Aero::Meta::PropertyFlags::AnyValue; \
            } \
            reg.access = ::Aero::Meta::PropertyAccessKind::Ordinary; \
            reg.get = prop->MetaGetter(); \
            reg.set = prop->MetaSetter(); \
            builder.Property(reg); \
        } \
    } \
private: \
    static void StaticFillClassType(::Aero::TypeClassBuilder<ClassName>& builder) noexcept

#define AERO_REFLECT_INLINE_2(ClassName, BaseClass) \
    AERO_REFLECT_INLINE_3(ClassName, BaseClass, ::Aero::Meta::AeroNamespaceUri())

#define AERO_REFLECT_INLINE(...) \
    AERO_REFLECT_GET_MACRO(__VA_ARGS__, AERO_REFLECT_INLINE_3, AERO_REFLECT_INLINE_2)(__VA_ARGS__)

#define AERO_BIND_PROPERTY(name, getter, setter) \
    builder.template Property<getter, setter>(#name)

#define AERO_BIND_READONLY_PROPERTY(name, getter) \
    builder.template PropertyReadOnly<getter>(#name)

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
        using EnclosingType = std::remove_cv_t<std::remove_pointer_t<decltype(this)>>; \
        if constexpr (std::is_same_v<typename EnclosingType::Self, EnclosingType>) { \
            static constexpr ::Aero::Meta::MemberId kPropertyId = \
                ::Aero::Meta::MakePropertyId(EnclosingType::StaticTypeId(), #name); \
            if (this->GetClassType() == EnclosingType::StaticClassType()) { \
                this->RaisePropertyChanged(kPropertyId); \
            } else { \
                this->RaisePropertyChanged(#name); \
            } \
        } else { \
            this->RaisePropertyChanged(#name); \
        } \
        return true; \
    }

#define AERO_VM_PROPERTY(type, name, defaultValue) \
    AERO_OBSERVABLE_PROPERTY(type, name, defaultValue)
