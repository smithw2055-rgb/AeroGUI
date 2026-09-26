#pragma once

// Markup-extension contracts used by the object writer.

#include "gui/markup/XamlSchema.hpp"
#include "gui/markup/MarkupCommon.hpp"

#include <Aero/Media/Animation.hpp>
#include "gui/templates/TemplateInstance.hpp"
// ObjectWriter / extensions / facets / template-compiler contracts
// (formerly MarkupWriterState). Lives beside WriterSupport below.

// ===== Extensions contract =====

// Canonical markup-extension API.

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Value.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Markup/XamlReader.hpp>

#include <cstdint>


namespace Aero::Markup {

class DeferredContentPlan;
class Schema;
struct VisualContentPlan;

enum class ProvidedValueKind : std::uint8_t {
    Value = 0U,
    Handled,
    Expression,
    Deferred
};

using ProvidedPrepareCallback =
    Base::Result<void> (*)(
        void* context,
        const Aero::NameScope& names) noexcept;
using ProvidedCommitCallback =
    Base::Result<std::uint64_t> (*)(void* context) noexcept;
using ProvidedRollbackCallback = void (*)(
    void* context,
    std::uint64_t token) noexcept;
using ProvidedCleanupCallback = void (*)(void* context) noexcept;
using ProvidedBindCallback = Base::Result<void> (*)(
    void* context, const EffectServices& services) noexcept;

struct ProvidedValue {
    ProvidedValueKind kind = ProvidedValueKind::Value;
    Meta::Value value;
    Meta::EffectiveValueEngine* effectiveValues = nullptr;
    Meta::PropertyExpression expression;
    void* rollbackContext = nullptr;
    std::uint64_t rollbackToken = 0U;
    ProvidedRollbackCallback rollback = nullptr;
    ProvidedPrepareCallback prepare = nullptr;
    ProvidedCommitCallback commit = nullptr;
    ProvidedCleanupCallback cleanup = nullptr;
    ProvidedBindCallback bind = nullptr;

    static ProvidedValue FromValue(
        Meta::Value&& provided) noexcept {
        ProvidedValue result;
        result.value = static_cast<Meta::Value&&>(provided);
        return result;
    }
    static ProvidedValue Handled(
        void* context = nullptr,
        std::uint64_t token = 0U,
        ProvidedRollbackCallback rollbackCallback = nullptr) noexcept {
        ProvidedValue result;
        result.kind = ProvidedValueKind::Handled;
        result.rollbackContext = context;
        result.rollbackToken = token;
        result.rollback = rollbackCallback;
        return result;
    }
    static ProvidedValue Expression(
        Meta::EffectiveValueEngine& engine,
        const Meta::PropertyExpression& provided) noexcept {
        ProvidedValue result;
        result.kind = ProvidedValueKind::Expression;
        result.effectiveValues = &engine;
        result.expression = provided;
        return result;
    }
    static ProvidedValue Deferred(
        void* context,
        ProvidedCommitCallback commitCallback,
        ProvidedRollbackCallback rollbackCallback,
        ProvidedCleanupCallback cleanupCallback,
        ProvidedPrepareCallback prepareCallback = nullptr,
        ProvidedBindCallback bindCallback = nullptr) noexcept {
        ProvidedValue result;
        result.kind = ProvidedValueKind::Deferred;
        result.rollbackContext = context;
        result.prepare = prepareCallback;
        result.commit = commitCallback;
        result.rollback = rollbackCallback;
        result.cleanup = cleanupCallback;
        result.bind = bindCallback;
        return result;
    }
    void Discard() noexcept {
        if (kind == ProvidedValueKind::Expression &&
            expression.cleanup != nullptr) {
            expression.cleanup(expression.context);
        } else if (kind == ProvidedValueKind::Handled &&
                   rollback != nullptr) {
            rollback(rollbackContext, rollbackToken);
        } else if (kind == ProvidedValueKind::Deferred &&
                   cleanup != nullptr) {
            cleanup(rollbackContext);
        }
        expression = {};
        rollbackContext = nullptr;
        rollbackToken = 0U;
        rollback = nullptr;
        prepare = nullptr;
        commit = nullptr;
        cleanup = nullptr;
        bind = nullptr;
    }
};

struct ExtensionServices {
    const Schema* schema = nullptr;
    Base::Object* targetObject = nullptr;
    Meta::TypeId targetObjectType = Meta::InvalidTypeId;
    Meta::MemberId targetMember = Meta::InvalidMemberId;
    Meta::TypeId targetValueType = Meta::InvalidTypeId;
    Base::Object* rootObject = nullptr;
    Base::Object* templatedParent = nullptr;
    const Base::ResourceUri* baseUri = nullptr;
    ::Aero::Diagnostics::SourceSpan source;
    const Aero::NameScope* nameScope = nullptr;
    NamespaceScope namespaces;
    ResourceResolver resources;
    Meta::EffectiveValueEngine* effectiveValues = nullptr;
    Aero::BindingEngine* bindings = nullptr;
    Aero::ResourceDictionary* fallbackResources = nullptr;
    Base::Span<const Aero::ResourceDictionary* const>
        ambientResourceChain;
    VisualContentPlan* visualContent = nullptr;
    Base::Object* deferredContentOwner = nullptr;
    DeferredContentPlan* deferredContent = nullptr;
};

struct BindingExtensionOptions {
    BindingExtensionOptions() noexcept = default;
    BindingExtensionOptions(
        Aero::BindingEngine* bindingManager,
        Meta::DependencyPropertyHandle dataContext) noexcept
        : bindings(bindingManager),
          dataContextProperty(dataContext) {}

    Aero::BindingEngine* bindings = nullptr;
    Meta::DependencyPropertyHandle dataContextProperty;
};

// Registers a {Binding ElementName=..., Path=..., Mode=...} provider. Explicit
// ElementName wins over DataContext. Paths are compiled to immutable metadata
// plans; DataContext paths are resolved after tree attachment and recompiled
// only when the concrete source type changes.
class BindingExtension {
public:
    explicit BindingExtension(
        const BindingExtensionOptions& options) noexcept;

    BindingExtension(const BindingExtension&) = delete;
    BindingExtension& operator=(const BindingExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId bindingExtensionType) noexcept;
    void SetDataContextProperty(
        Meta::DependencyPropertyHandle property) noexcept {
        options_.dataContextProperty = property;
    }

private:
    BindingExtensionOptions options_;

    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

class DynamicResource {
public:
    static Base::Result<void> Attach(
        Meta::EffectiveValueEngine& effectiveValues,
        Aero::ResourceDictionary& resources,
        ::Aero::DependencyObject& target,
        Meta::DependencyPropertyHandle property,
        Base::StringView key) noexcept;
    static Base::Result<void> Attach(
        Meta::EffectiveValueEngine& effectiveValues,
        Base::Span<
            const Aero::ResourceDictionary* const> resourceChain,
        Aero::ResourceDictionary* fallbackResources,
        ::Aero::DependencyObject& target,
        Meta::DependencyPropertyHandle property,
        Base::StringView key) noexcept;
    static Base::Result<Meta::PropertyExpression> CreateExpression(
        Meta::EffectiveValueEngine& effectiveValues,
        Base::Span<
            const Aero::ResourceDictionary* const> resourceChain,
        Aero::ResourceDictionary* fallbackResources,
        ::Aero::DependencyObject& target,
        Meta::DependencyPropertyHandle property,
        Base::StringView key) noexcept;
};

struct DynamicResourceExtensionOptions {
    DynamicResourceExtensionOptions() noexcept = default;
    DynamicResourceExtensionOptions(
        Meta::EffectiveValueEngine* effectiveValueEngine,
        Aero::ResourceDictionary* resourceDictionary) noexcept
        : effectiveValues(effectiveValueEngine),
          resources(resourceDictionary) {}

    Meta::EffectiveValueEngine* effectiveValues = nullptr;
    Aero::ResourceDictionary* resources = nullptr;
};

class DynamicResourceExtension {
public:
    explicit DynamicResourceExtension(
        const DynamicResourceExtensionOptions& options) noexcept;

    DynamicResourceExtension(const DynamicResourceExtension&) = delete;
    DynamicResourceExtension& operator=(
        const DynamicResourceExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId dynamicResourceExtensionType) noexcept;

private:
    DynamicResourceExtensionOptions options_;

    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

// Mirrors WPF's {StaticResource key}/StaticResourceExtension. Unlike
// DynamicResourceExtension it resolves the resource dictionary key exactly once
// while the XAML tree is authored and returns the value directly; there is no
// expression and no change tracking once the tree is live.
class StaticResourceExtension {
public:
    StaticResourceExtension() noexcept = default;

    StaticResourceExtension(const StaticResourceExtension&) = delete;
    StaticResourceExtension& operator=(
        const StaticResourceExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId staticResourceExtensionType) noexcept;

private:
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

class TypeExtension {
public:
    TypeExtension() noexcept = default;

    TypeExtension(const TypeExtension&) = delete;
    TypeExtension& operator=(const TypeExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId markupExtensionType) noexcept;

private:
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

// Compatibility provider for AeroGUIExtensions.Loc. Its attached Source is
// registered by the markup schema; the provider supplies values to ordinary
// property attributes such as {aero:Loc TitleLabel}.
class LocExtension {
public:
    LocExtension() noexcept = default;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId markupExtensionType) noexcept;

    // Called by the inheritable aero:Loc.Source compatibility property.  The
    // implementation swaps the source XAML dictionary for all Loc values
    // authored in the same loaded visual tree.
    static void OnSourceChanged(
        ::Aero::DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs& args) noexcept;

private:
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

// Records a WPF-style one-way property mapping while a ControlTemplate
// prototype is authored. The prototype keeps no live expression; its immutable
// runtime plan applies the mapping to every instantiated template tree.
class TemplateBindingExtension {
public:
    TemplateBindingExtension() noexcept = default;

    TemplateBindingExtension(
        const TemplateBindingExtension&) = delete;
    TemplateBindingExtension& operator=(
        const TemplateBindingExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId markupExtensionType) noexcept;

private:
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

// Implements the WPF-compatible {x:Static Type.Member} form for registered
// enum values. The returned Meta::Value retains the enum's concrete metadata
// type so AnyValue members (for example discrete object key frames) can defer
// assignment until their target dependency property is known.
class StaticExtension {
public:
    StaticExtension() noexcept = default;

    StaticExtension(const StaticExtension&) = delete;
    StaticExtension& operator=(const StaticExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId markupExtensionType) noexcept;

private:
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

} // namespace Aero::Markup

