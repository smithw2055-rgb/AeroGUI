#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Value.hpp>

namespace Aero::Meta {
class TypeInfo;
}

namespace Aero::Markup {

// Context provided to MarkupExtension::ProvideValue, exposing target object,
// target property, base URI, and type resolution.
class MarkupExtensionContext {
public:
    using TypeResolverCallback =
        Base::Result<const Meta::TypeInfo*> (*)(void* context, Base::StringView qualifiedTypeName) noexcept;

    MarkupExtensionContext() noexcept = default;
    MarkupExtensionContext(
        Base::Object* targetObject,
        Meta::MemberId targetMember,
        const Base::ResourceUri* baseUri,
        void* resolverContext = nullptr,
        TypeResolverCallback typeResolver = nullptr) noexcept
        : targetObject_(targetObject),
          targetMember_(targetMember),
          baseUri_(baseUri),
          resolverContext_(resolverContext),
          typeResolver_(typeResolver) {}

    Base::Object* TargetObject() const noexcept { return targetObject_; }
    Meta::MemberId TargetMember() const noexcept { return targetMember_; }
    Meta::DependencyPropertyHandle TargetProperty() const noexcept {
        return Meta::DependencyPropertyHandle{targetMember_};
    }
    const Base::ResourceUri* BaseUri() const noexcept { return baseUri_; }

    Base::Result<const Meta::TypeInfo*> ResolveType(Base::StringView qualifiedTypeName) const noexcept {
        if (typeResolver_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Type resolution is unavailable in this context");
        }
        return typeResolver_(resolverContext_, qualifiedTypeName);
    }

private:
    Base::Object* targetObject_ = nullptr;
    Meta::MemberId targetMember_ = Meta::InvalidMemberId;
    const Base::ResourceUri* baseUri_ = nullptr;
    void* resolverContext_ = nullptr;
    TypeResolverCallback typeResolver_ = nullptr;
};

// WPF MarkupExtension. User types inherit this, override ProvideValue, and
// register with TypeFlags::MarkupExtension plus Factory(). XAML object-element
// usage constructs the extension then replaces the property value with the
// result of ProvideValue. Attribute syntax `{local:MyExt}` constructs via the
// factory and calls ProvideValue with no constructor arguments parsed yet.
class AERO_GUI_API MarkupExtension : public Base::Object {
    AERO_DECLARE_TYPE(MarkupExtension, Base::Object)

public:
    TypeId RuntimeType() const noexcept override { return runtimeType_; }
    virtual Result<Value> ProvideValue(const MarkupExtensionContext& context) noexcept {
        (void)context;
        return ProvideValue();
    }
    virtual Result<Value> ProvideValue() noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "ProvideValue not implemented");
    }

protected:
    explicit MarkupExtension(TypeId runtimeType) noexcept : runtimeType_(runtimeType) {}

private:
    TypeId runtimeType_ = StaticTypeId();
};

} // namespace Aero::Markup
