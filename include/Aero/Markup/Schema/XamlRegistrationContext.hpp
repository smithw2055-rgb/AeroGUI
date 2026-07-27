#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Core/Metadata/Activation.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Markup/Schema/XamlSchemaContext.hpp>

namespace Aero::Markup {

// Services available while a module contributes XAML-specific behavior to a
// schema bundle. The context is registration-scoped and must not be retained.
class AERO_API XamlRegistrationContext final {
public:
    XamlRegistrationContext(
        XamlSchemaContext& schema,
        Core::ActivationProviderRegistry& activation,
        Core::MetadataRuntime& runtime,
        Core::DependencyPropertyRegistry& properties,
        Base::IAllocator& allocator) noexcept
        : schema_(&schema),
          activation_(&activation),
          runtime_(&runtime),
          properties_(&properties),
          allocator_(&allocator) {}

    XamlSchemaContext& Schema() const noexcept { return *schema_; }
    Core::ActivationProviderRegistry& Activation() const noexcept {
        return *activation_;
    }
    Core::MetadataRuntime& Runtime() const noexcept { return *runtime_; }
    Core::DependencyPropertyRegistry& DependencyProperties() const noexcept {
        return *properties_;
    }
    Base::IAllocator& Allocator() const noexcept { return *allocator_; }

    Base::Result<void> TryAdd(const XamlMemberFacet& facet) noexcept {
        return schema_->TryAddFacet(facet);
    }
    Base::Result<void> TryAdd(const XamlMemberProviderFacet& facet) noexcept {
        return schema_->TryAddFacet(facet);
    }
    Base::Result<void> TryAdd(const XamlTypeFacet& facet) noexcept {
        return schema_->TryAddFacet(facet);
    }
    Base::Result<void> TryAdd(const XamlLifecycleFacet& facet) noexcept {
        return schema_->TryAddFacet(facet);
    }
    Base::Result<void> TryAdd(const XamlNameScopeFacet& facet) noexcept {
        return schema_->TryAddFacet(facet);
    }
    Base::Result<void> TryAdd(const XamlResourceScopeFacet& facet) noexcept {
        return schema_->TryAddFacet(facet);
    }
    Base::Result<void> TryAdd(const XamlDeferredContentFacet& facet) noexcept {
        return schema_->TryAddFacet(facet);
    }
    Base::Result<void> TryAdd(const XamlImplicitResourceKeyFacet& facet) noexcept {
        return schema_->TryAddFacet(facet);
    }
    Base::Result<void> TryAdd(const XamlPropertyTargetFacet& facet) noexcept {
        return schema_->TryAddFacet(facet);
    }
    Base::Result<void> TryAdd(const XamlMarkupExtensionFacet& facet) noexcept {
        return schema_->TryAddFacet(facet);
    }

private:
    XamlSchemaContext* schema_ = nullptr;
    Core::ActivationProviderRegistry* activation_ = nullptr;
    Core::MetadataRuntime* runtime_ = nullptr;
    Core::DependencyPropertyRegistry* properties_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
};

} // namespace Aero::Markup
