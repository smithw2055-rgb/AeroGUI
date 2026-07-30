#pragma once

#include <Aero/Detail/RuntimeManagersFwd.hpp>

// Canonical markup-extension API.

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Core/Metadata/Value.hpp>
#include <Aero/Presentation/Binding.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Markup/Resources.hpp>

#include <cstdint>

namespace Aero::Presentation {
}

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

using ProvidedCommitCallback =
    Base::Result<std::uint64_t> (*)(void* context) noexcept;
using ProvidedRollbackCallback = void (*)(
    void* context,
    std::uint64_t token) noexcept;
using ProvidedCleanupCallback = void (*)(void* context) noexcept;

struct ProvidedValue final {
    ProvidedValueKind kind = ProvidedValueKind::Value;
    Core::Value value;
    Core::EffectiveValueEngine* effectiveValues = nullptr;
    Core::PropertyExpression expression;
    void* rollbackContext = nullptr;
    std::uint64_t rollbackToken = 0U;
    ProvidedRollbackCallback rollback = nullptr;
    ProvidedCommitCallback commit = nullptr;
    ProvidedCleanupCallback cleanup = nullptr;

    static ProvidedValue FromValue(
        Core::Value&& provided) noexcept {
        ProvidedValue result;
        result.value = static_cast<Core::Value&&>(provided);
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
        Core::EffectiveValueEngine& engine,
        const Core::PropertyExpression& provided) noexcept {
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
        ProvidedCleanupCallback cleanupCallback) noexcept {
        ProvidedValue result;
        result.kind = ProvidedValueKind::Deferred;
        result.rollbackContext = context;
        result.commit = commitCallback;
        result.rollback = rollbackCallback;
        result.cleanup = cleanupCallback;
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
        commit = nullptr;
        cleanup = nullptr;
    }
};

struct ExtensionContext final {
    const Schema* schema = nullptr;
    Base::Object* targetObject = nullptr;
    Core::TypeId targetObjectType = Core::InvalidTypeId;
    Core::MemberId targetMember = Core::InvalidMemberId;
    Core::TypeId targetValueType = Core::InvalidTypeId;
    Base::Object* rootObject = nullptr;
    Base::Object* templatedParent = nullptr;
    const Base::ResourceUri* baseUri = nullptr;
    Core::SourceSpan source;
    const Presentation::NameScope* nameScope = nullptr;
    NamespaceScope namespaces;
    ResourceResolver resources;
    Core::EffectiveValueEngine* effectiveValues = nullptr;
    Presentation::BindingManager* bindings = nullptr;
    Presentation::ResourceDictionary* fallbackResources = nullptr;
    Base::Span<const Presentation::ResourceDictionary* const>
        ambientResourceChain;
    VisualContentPlan* visualContent = nullptr;
    Base::Object* deferredContentOwner = nullptr;
    DeferredContentPlan* deferredContent = nullptr;
};

struct BindingExtensionOptions final {
    BindingExtensionOptions() noexcept = default;
    BindingExtensionOptions(
        Presentation::BindingManager* bindingManager,
        Core::DependencyPropertyHandle dataContext) noexcept
        : bindings(bindingManager),
          dataContextProperty(dataContext) {}

    Presentation::BindingManager* bindings = nullptr;
    Core::DependencyPropertyHandle dataContextProperty;
};

// Registers a {Binding ElementName=..., Path=..., Mode=...} provider. Explicit
// ElementName wins over DataContext. Paths are compiled to immutable metadata
// plans; DataContext paths are resolved after tree attachment and recompiled
// only when the concrete source type changes.
class AERO_API BindingExtension final {
public:
    explicit BindingExtension(
        const BindingExtensionOptions& options) noexcept;

    BindingExtension(const BindingExtension&) = delete;
    BindingExtension& operator=(const BindingExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Core::TypeId bindingExtensionType) noexcept;
    void SetDataContextProperty(
        Core::DependencyPropertyHandle property) noexcept {
        options_.dataContextProperty = property;
    }

private:
    BindingExtensionOptions options_;

    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionContext& services,
        void* context) noexcept;
};

class AERO_API DynamicResource final {
public:
    static Base::Result<void> Attach(
        Core::EffectiveValueEngine& effectiveValues,
        Presentation::ResourceDictionary& resources,
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        Base::StringView key) noexcept;
    static Base::Result<void> Attach(
        Core::EffectiveValueEngine& effectiveValues,
        Base::Span<
            const Presentation::ResourceDictionary* const> resourceChain,
        Presentation::ResourceDictionary* fallbackResources,
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        Base::StringView key) noexcept;
    static Base::Result<Core::PropertyExpression> CreateExpression(
        Core::EffectiveValueEngine& effectiveValues,
        Base::Span<
            const Presentation::ResourceDictionary* const> resourceChain,
        Presentation::ResourceDictionary* fallbackResources,
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        Base::StringView key) noexcept;
};

struct DynamicResourceExtensionOptions final {
    DynamicResourceExtensionOptions() noexcept = default;
    DynamicResourceExtensionOptions(
        Core::EffectiveValueEngine* effectiveValueEngine,
        Presentation::ResourceDictionary* resourceDictionary) noexcept
        : effectiveValues(effectiveValueEngine),
          resources(resourceDictionary) {}

    Core::EffectiveValueEngine* effectiveValues = nullptr;
    Presentation::ResourceDictionary* resources = nullptr;
};

class AERO_API DynamicResourceExtension final {
public:
    explicit DynamicResourceExtension(
        const DynamicResourceExtensionOptions& options) noexcept;

    DynamicResourceExtension(const DynamicResourceExtension&) = delete;
    DynamicResourceExtension& operator=(
        const DynamicResourceExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Core::TypeId dynamicResourceExtensionType) noexcept;

private:
    DynamicResourceExtensionOptions options_;

    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionContext& services,
        void* context) noexcept;
};

class AERO_API TypeExtension final {
public:
    TypeExtension() noexcept = default;

    TypeExtension(const TypeExtension&) = delete;
    TypeExtension& operator=(const TypeExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Core::TypeId markupExtensionType) noexcept;

private:
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionContext& services,
        void* context) noexcept;
};

// Records a WPF-style one-way property mapping while a ControlTemplate
// prototype is authored. The prototype keeps no live expression; its immutable
// runtime plan applies the mapping to every instantiated template tree.
class AERO_API TemplateBindingExtension final {
public:
    TemplateBindingExtension() noexcept = default;

    TemplateBindingExtension(
        const TemplateBindingExtension&) = delete;
    TemplateBindingExtension& operator=(
        const TemplateBindingExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Core::TypeId markupExtensionType) noexcept;

private:
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionContext& services,
        void* context) noexcept;
};

// Implements the WPF-compatible {x:Static Type.Member} form for registered
// enum values. The returned Core::Value retains the enum's concrete metadata
// type so AnyValue members (for example discrete object key frames) can defer
// assignment until their target dependency property is known.
class AERO_API StaticExtension final {
public:
    StaticExtension() noexcept = default;

    StaticExtension(const StaticExtension&) = delete;
    StaticExtension& operator=(const StaticExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Core::TypeId markupExtensionType) noexcept;

private:
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionContext& services,
        void* context) noexcept;
};

} // namespace Aero::Markup
