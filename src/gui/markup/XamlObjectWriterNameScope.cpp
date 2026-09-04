#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/controls/State.hpp"
#include "gui/templates/TemplateState.hpp"
#include "gui/markup/MarkupState.hpp"
#include "gui/markup/MarkupWriterState.hpp"
#include "gui/markup/MarkupCommon.hpp"
#include "gui/markup/XamlObjectWriterInternal.hpp"
#include "gui/markup/MarkupExtensionHost.hpp"

#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Markup/ServiceProvider.hpp>
#include <Aero/VisualStateManager.hpp>

// ===== ObjectBuilder name-scope / deferrals =====

namespace Aero::Markup {

Base::Result<void> ObjectBuilder::RegisterObjectName(
    std::uint32_t objectIndex,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    CreatedObjectRecord& object = created_[objectIndex];
    if (object.name.Empty() || object.nameRegistered) {
        return {};
    }

    const std::uint32_t scopeIndex =
        FindNameScopeIndexForObject(objectIndex);
    if (scopeIndex == InvalidIndex || scopeIndex >= nameScopes_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    NameScopeRecord& scope = nameScopes_[scopeIndex];
    Base::Result<void> localResult = scope.names.Register(
        object.name.View(),
        *object.object);
    if (!localResult) {
        const bool duplicate =
            localResult.GetStatus().code == Base::ErrorCode::AlreadyExists;
        return Failure(
            localResult.GetStatus(),
            duplicate
                ? XamlObjectWriterDiagnosticCodes::DuplicateName
                : XamlObjectWriterDiagnosticCodes::InvalidDirective,
            duplicate ? MessageDuplicateName : MessageInvalidDirective,
            source);
    }

    if (scope.ownerObjectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }
    CreatedObjectRecord& owner = created_[scope.ownerObjectIndex];
    Base::Result<void> callbackResult = schema_->RegisterName(
        owner.type,
        *owner.object,
        object.name.View(),
        *object.object);
    if (!callbackResult) {
        return Failure(
            callbackResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::NameRegistrationFailed,
            MessageNameRegistrationFailed,
            source);
    }

    object.nameRegistered = true;
    return {};
}

Base::Result<void> ObjectBuilder::ConnectEvent(
    Frame& memberFrame,
    Base::StringView handlerName,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (handlerName.Empty() ||
        memberFrame.targetObjectIndex >= created_.Size() ||
        rootObjectIndex_ >= created_.Size() ||
        memberFrame.member.kind != Meta::MemberKind::Event) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML event attribute requires a handler name"),
            XamlObjectWriterDiagnosticCodes::InvalidValue,
            MessageInvalidValue,
            source);
    }

    CreatedObjectRecord& eventSource =
        created_[memberFrame.targetObjectIndex];
    CreatedObjectRecord& codeBehind =
        created_[rootObjectIndex_];
    if (!eventSource.object || !codeBehind.object) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    Meta::EventHandlerThunk thunk =
        schema_->Metadata()->Types().FindEventHandler(
            codeBehind.type,
            handlerName);
    if (thunk == nullptr) {
        // LoadComponentInto a real code-behind instance must resolve the
        // handler. A pure-XAML host (x:Class omitted or unregistered) stores
        // Click="OnFoo" the same way CommandBinding Executed stores a name:
        // do not abort document construction.
        if (loadContext_ != nullptr && loadContext_->existingRoot) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "XAML event handler is not registered on the x:Class type"),
                XamlObjectWriterDiagnosticCodes::UnknownMember,
                MessageUnknownMember,
                source);
        }
        return {};
    }

    Base::Result<Base::Ref<XamlEventConnection>> connection =
        Base::MakeRef<XamlEventConnection>(
            Base::WeakRef<Base::Object>(root_),
            thunk);
    if (!connection) return connection.GetStatus();
    Base::Delegate<void(Base::Object*, RoutedEventArgs&)> handler(
        XamlEventInvoker{std::move(connection).Value()});
    const RoutedEventHandle routedEvent{memberFrame.member.id};

    Base::Result<void> connected;
    if (schema_->Types().IsDerivedFrom(
            eventSource.type, UIElement::StaticTypeId())) {
        connected = static_cast<UIElement*>(
            eventSource.object.Get())->AddHandlerChecked(
                routedEvent, handler);
    } else if (schema_->Types().IsDerivedFrom(
                   eventSource.type,
                   ContentElement::StaticTypeId())) {
        connected = static_cast<ContentElement*>(
            eventSource.object.Get())->AddHandlerChecked(
                routedEvent, handler);
    } else {
        connected = Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML event source does not support routed handlers");
    }
    return connected
        ? Base::Result<void>{}
        : Base::Result<void>(Failure(
              connected.GetStatus(),
              XamlObjectWriterDiagnosticCodes::UnsupportedMember,
              MessageUnsupportedMember,
              source));
}

Base::Result<bool> ObjectBuilder::RegisterObjectResource(
    std::uint32_t objectIndex,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    CreatedObjectRecord& object = created_[objectIndex];
    if (object.resourceRegistered) {
        return true;
    }

    bool dictionaryContent = false;
    if (!frames_.Empty()) {
        const Frame& parent = frames_.Back();
        std::uint32_t parentObjectIndex = InvalidIndex;
        Meta::MemberId targetMember = Meta::InvalidMemberId;
        if (parent.kind == FrameKind::Object) {
            parentObjectIndex = parent.objectIndex;
        } else if (parent.kind == FrameKind::Member) {
            parentObjectIndex = parent.targetObjectIndex;
            targetMember = parent.member.id;
        }
        if (parentObjectIndex < created_.Size() &&
            created_[parentObjectIndex].type ==
                Aero::ResourceDictionary::StaticTypeId()) {
            const CreatedObjectRecord& dictionary =
                created_[parentObjectIndex];
            dictionaryContent =
                dictionary.hasContentMember &&
                (targetMember == Meta::InvalidMemberId ||
                 targetMember ==
                     dictionary.contentMember.id);
        } else if (
            parentObjectIndex < created_.Size() &&
            targetMember != Meta::InvalidMemberId &&
            object.type !=
                Aero::ResourceDictionary::
                    StaticTypeId() &&
            schema_->CreatesResourceScope(
                created_[parentObjectIndex].type)) {
            dictionaryContent =
                parent.kind == FrameKind::Member &&
                parent.member.valueType ==
                    Aero::ResourceDictionary::
                        StaticTypeId();
        }
    }

    const bool explicitKey = !object.key.Empty();
    if (!dictionaryContent) {
        if (!explicitKey) return false;
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                MessageInvalidDirective.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            source);
    }

    Base::Result<Aero::ResourceKey> resourceKey =
        explicitKey
        ? Aero::ResourceKey::FromString(
              object.key.View())
        : (object.valueElement || !object.object
            ? Base::Result<Aero::ResourceKey>(
                  Base::Status::Failure(
                      Base::ErrorCode::ValidationFailed,
                      "Unkeyed ResourceDictionary entry must be an object"))
            : schema_->ResolveImplicitResourceKey(
                  object.type,
                  *object.object));
    if (!resourceKey) {
        return Failure(
            resourceKey.GetStatus(),
            XamlObjectWriterDiagnosticCodes::ResourceRegistrationFailed,
            MessageResourceRegistrationFailed,
            source);
    }

    const std::uint32_t scopeIndex = FindResourceScopeIndexForParent();
    if (scopeIndex == InvalidIndex || scopeIndex >= resourceScopes_.Size()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                MessageMissingResourceScope.Data()),
            XamlObjectWriterDiagnosticCodes::MissingResourceScope,
            MessageMissingResourceScope,
            source);
    }

    ResourceScopeRecord& scope = resourceScopes_[scopeIndex];
    Meta::Value resourceValue = object.valueElement
        ? object.value
        : Meta::Value::FromObject(object.type, object.object);
    Base::Result<void> localResult = scope.resources.Add(
        resourceKey.Value(),
        resourceValue,
        source);
    if (!localResult) {
        const bool duplicate =
            localResult.GetStatus().code == Base::ErrorCode::AlreadyExists;
        return Failure(
            localResult.GetStatus(),
            duplicate
                ? XamlObjectWriterDiagnosticCodes::DuplicateResourceKey
                : XamlObjectWriterDiagnosticCodes::InvalidDirective,
            duplicate ? MessageDuplicateResourceKey : MessageInvalidDirective,
            source);
    }

    if (scope.ownerObjectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }
    CreatedObjectRecord& owner = created_[scope.ownerObjectIndex];
    Base::Result<void> callbackResult = schema_->AddResource(
        owner.type,
        *owner.object,
        resourceKey.Value(),
        resourceValue);
    if (!callbackResult) {
        return Failure(
            callbackResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::ResourceRegistrationFailed,
            MessageResourceRegistrationFailed,
            source);
    }

    if (!explicitKey &&
        object.type == Aero::Style::StaticTypeId() &&
        object.object) {
        const auto& style = static_cast<const Aero::Style&>(
            *object.object);
        const Meta::TypeInfo* targetType =
            schema_->Types().FindType(
                style.GetTargetType());
        if (targetType != nullptr &&
            !targetType->Name().Empty()) {
            Base::String alias;
            Base::Result<void> named =
                alias.Assign("Style.");
            if (named) {
                named = alias.Append(
                    targetType->Name());
            }
            if (!named) return named.GetStatus();
            if (!scope.resources.Contains(alias.View())) {
                Base::Result<Aero::ResourceKey> aliasKey =
                    Aero::ResourceKey::FromString(
                        alias.View());
                if (!aliasKey) return aliasKey.GetStatus();
                Base::Result<void> localAlias =
                    scope.resources.Add(
                        aliasKey.Value(),
                        resourceValue,
                        source);
                if (!localAlias) {
                    return Failure(
                        localAlias.GetStatus(),
                        XamlObjectWriterDiagnosticCodes::
                            ResourceRegistrationFailed,
                        MessageResourceRegistrationFailed,
                        source);
                }
                Base::Result<void> callbackAlias =
                    schema_->AddResource(
                        owner.type,
                        *owner.object,
                        aliasKey.Value(),
                        resourceValue);
                if (!callbackAlias) {
                    return Failure(
                        callbackAlias.GetStatus(),
                        XamlObjectWriterDiagnosticCodes::
                            ResourceRegistrationFailed,
                        MessageResourceRegistrationFailed,
                        source);
                }
            }
        }
    }

    object.resourceRegistered = true;
    return true;
}

Base::Result<Aero::ResourceValue> ObjectBuilder::LookupResource(
    Base::StringView key) const noexcept {
    constexpr Base::StringView PresentationNamespace(
        "http://schemas.microsoft.com/winfx/2006/xaml/presentation");

    Aero::ResourceKey candidates[4];
    std::uint32_t candidateCount = 0U;
    auto addCandidate = [&](Aero::ResourceKey candidate) noexcept {
        if (!candidate.IsValid() || candidateCount >= 4U) {
            return;
        }
        for (std::uint32_t index = 0U; index < candidateCount; ++index) {
            if (candidates[index] == candidate) {
                return;
            }
        }
        candidates[candidateCount++] = std::move(candidate);
    };
    auto addStringKey = [&](Base::StringView text) noexcept {
        if (text.Empty()) {
            return;
        }
        Base::Result<Aero::ResourceKey> parsed =
            Aero::ResourceKey::FromString(text);
        if (parsed) {
            addCandidate(std::move(parsed).Value());
        }
    };
    auto addTypeCandidates = [&](
        Base::StringView localName,
        Meta::TypeId type) noexcept {
        if (type != Meta::InvalidTypeId) {
            addCandidate(Aero::ResourceKey::FromType(type));
        }
        if (!localName.Empty()) {
            Base::String alias;
            if (alias.Assign("Style.") && alias.Append(localName)) {
                addStringKey(alias.View());
            }
            addStringKey(localName);
        }
    };
    auto splitTypeName = [&](Base::StringView typeName)
        -> Base::Result<std::pair<Base::StringView, Base::StringView>> {
        std::uint32_t colon = typeName.SizeBytes();
        for (std::uint32_t index = 0U;
             index < typeName.SizeBytes();
             ++index) {
            if (typeName[index] != ':') continue;
            if (colon != typeName.SizeBytes()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "StaticResource x:Type key contains multiple namespace prefixes");
            }
            colon = index;
        }
        Base::StringView prefix;
        Base::StringView localName = typeName;
        if (colon != typeName.SizeBytes()) {
            if (colon == 0U || colon + 1U >= typeName.SizeBytes()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "StaticResource x:Type key namespace prefix is malformed");
            }
            prefix = typeName.Substr(0U, colon);
            localName = typeName.Substr(
                colon + 1U,
                typeName.SizeBytes() - colon - 1U);
        }
        return std::make_pair(prefix, localName);
    };
    auto resolveTypeId = [&](Base::StringView prefix, Base::StringView localName)
        -> Base::Result<Meta::TypeId> {
        Base::StringView namespaceUri = PresentationNamespace;
        Base::Result<Base::StringView> bound = LookupNamespace(prefix);
        if (bound) {
            namespaceUri = bound.Value();
        } else if (!prefix.Empty()) {
            return bound.GetStatus();
        }
        Base::Result<const Meta::TypeInfo*> type =
            schema_->ResolveType(namespaceUri, localName);
        if (!type && prefix.Empty() &&
            namespaceUri != PresentationNamespace) {
            type = schema_->ResolveType(PresentationNamespace, localName);
        }
        if (!type) {
            return type.GetStatus();
        }
        return type.Value()->Id();
    };
    auto looksLikeTypeName = [&](Base::StringView text) noexcept -> bool {
        if (text.Empty() || text[0] == '{') {
            return false;
        }
        bool sawColon = false;
        for (std::uint32_t index = 0U; index < text.SizeBytes(); ++index) {
            const char character = text[index];
            if (character == ':') {
                if (sawColon || index == 0U ||
                    index + 1U >= text.SizeBytes()) {
                    return false;
                }
                sawColon = true;
                continue;
            }
            const bool letter =
                (character >= 'A' && character <= 'Z') ||
                (character >= 'a' && character <= 'z') ||
                character == '_';
            const bool digit =
                character >= '0' && character <= '9';
            if (!(letter || (index > 0U && digit))) {
                return false;
            }
        }
        return true;
    };

    Base::StringView extensionName;
    Base::StringView typeArgument;
    const MarkupValueKind markup =
        ParseMarkupValue(key, extensionName, typeArgument);
    if (markup == MarkupValueKind::Extension &&
        extensionName == Base::StringView("x:Type")) {
        Base::Result<std::pair<Base::StringView, Base::StringView>> parts =
            splitTypeName(typeArgument);
        if (!parts) {
            return parts.GetStatus();
        }
        Base::Result<Meta::TypeId> typeId =
            resolveTypeId(parts.Value().first, parts.Value().second);
        if (typeId) {
            addTypeCandidates(parts.Value().second, typeId.Value());
        }
        addStringKey(key);
    } else {
        addStringKey(key);
        if (looksLikeTypeName(key)) {
            Base::Result<std::pair<Base::StringView, Base::StringView>> parts =
                splitTypeName(key);
            if (parts) {
                Base::Result<Meta::TypeId> typeId =
                    resolveTypeId(parts.Value().first, parts.Value().second);
                if (typeId) {
                    addTypeCandidates(parts.Value().second, typeId.Value());
                }
            }
        }
    }
    if (candidateCount == 0U) {
        addStringKey(key);
        if (candidateCount == 0U) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "String resource key cannot be empty");
        }
    }

    auto lookupOne = [&](const Aero::ResourceKey& resourceKey)
        -> Base::Result<Aero::ResourceValue> {
        for (std::uint32_t index = frames_.Size(); index > 0U; --index) {
            const Frame& frame = frames_[index - 1U];
            if (frame.kind != FrameKind::Object ||
                frame.resourceScopeIndex == InvalidIndex ||
                frame.resourceScopeIndex >= resourceScopes_.Size()) {
                continue;
            }
            Base::Result<Aero::ResourceValue> value =
                (resourceScopes_[frame.resourceScopeIndex].external != nullptr
                    ? resourceScopes_[frame.resourceScopeIndex].external
                    : &resourceScopes_[frame.resourceScopeIndex].resources)
                    ->Lookup(resourceKey);
            if (value) {
                return value;
            }
            if (value.GetStatus().code != Base::ErrorCode::NotFound) {
                return value.GetStatus();
            }
        }
        if (Base::Object* deferredOwner = FindDeferredContentOwner()) {
            Aero::ResourceDictionary* templateResources = nullptr;
            const Meta::TypeId ownerType = deferredOwner->RuntimeType();
            if (ownerType == ::Aero::DataTemplate::StaticTypeId() ||
                ownerType == ::Aero::HierarchicalDataTemplate::StaticTypeId()) {
                templateResources =
                    &static_cast<::Aero::DataTemplate&>(
                        *deferredOwner).GetResources();
            } else if (
                ownerType == Controls::ItemsPanelTemplate::StaticTypeId()) {
                templateResources =
                    &static_cast<Controls::ItemsPanelTemplate&>(
                        *deferredOwner).GetResources();
            } else if (
                ownerType == Controls::ControlTemplate::StaticTypeId()) {
                templateResources =
                    &static_cast<Controls::ControlTemplate&>(
                        *deferredOwner).GetResources();
            }
            if (templateResources != nullptr) {
                Base::Result<Aero::ResourceValue> value =
                    templateResources->Lookup(resourceKey);
                if (value) return value;
                if (value.GetStatus().code != Base::ErrorCode::NotFound) {
                    return value.GetStatus();
                }
            }
        }
        // WPF: a later merged dictionary's StaticResource sees keys from earlier
        // application merged dictionaries AND from nested MergedDictionaries, even
        // while this document still has an object/member frame on the stack.
        for (std::uint32_t index = resourceScopes_.Size();
             index > 0U; --index) {
            const ResourceScopeRecord& scope =
                resourceScopes_[index - 1U];
            const Aero::ResourceDictionary* dictionary =
                scope.external != nullptr
                    ? scope.external
                    : &scope.resources;
            Base::Result<Aero::ResourceValue> value =
                dictionary->Lookup(resourceKey);
            if (value) return value;
            if (value.GetStatus().code != Base::ErrorCode::NotFound) {
                return value.GetStatus();
            }
        }
        {
            Base::Result<Aero::ResourceValue> committed =
                committedResources_.Lookup(resourceKey);
            if (committed) return committed;
            if (committed.GetStatus().code != Base::ErrorCode::NotFound) {
                return committed.GetStatus();
            }
        }
        if (loadContext_ != nullptr && loadContext_->resources != nullptr) {
            Base::Result<Aero::ResourceValue> value =
                loadContext_->resources->Lookup(resourceKey);
            if (value) {
                return value;
            }
            if (value.GetStatus().code != Base::ErrorCode::NotFound) {
                return value.GetStatus();
            }
        }
        if (loadContext_ != nullptr &&
            loadContext_->fallbackResources != nullptr &&
            loadContext_->fallbackResources != loadContext_->resources) {
            Base::Result<Aero::ResourceValue> value =
                loadContext_->fallbackResources->Lookup(resourceKey);
            if (value) {
                return value;
            }
            if (value.GetStatus().code != Base::ErrorCode::NotFound) {
                return value.GetStatus();
            }
        }
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            MessageStaticResourceNotFound.Data());
    };

    Base::Result<Aero::ResourceValue> last =
        Base::Status::Failure(
            Base::ErrorCode::NotFound,
            MessageStaticResourceNotFound.Data());
    for (std::uint32_t index = 0U; index < candidateCount; ++index) {
        Base::Result<Aero::ResourceValue> value =
            lookupOne(candidates[index]);
        if (value) {
            return value;
        }
        if (value.GetStatus().code != Base::ErrorCode::NotFound) {
            return value.GetStatus();
        }
        last = std::move(value);
    }
    return last;
}

Base::Result<void> ObjectBuilder::CreateScopesForObject(
    std::uint32_t objectIndex,
    Frame& frame,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    const bool documentRoot = objectIndex == rootObjectIndex_;
    if (documentRoot || schema_->CreatesNameScope(created_[objectIndex].type)) {
        NameScopeRecord scope;
        scope.ownerObjectIndex = objectIndex;
        const std::uint32_t index = nameScopes_.Size();
        Base::Result<void> appendResult =
            nameScopes_.PushBack(std::move(scope));
        if (!appendResult) {
            return appendResult.GetStatus();
        }
        frame.nameScopeIndex = index;
        if (documentRoot) {
            documentNameScopeIndex_ = index;
        }
    }

    if (documentRoot ||
        schema_->CreatesResourceScope(created_[objectIndex].type)) {
        ResourceScopeRecord scope;
        scope.ownerObjectIndex = objectIndex;
        scope.external = schema_->ResolveResourceScope(
            created_[objectIndex].type,
            *created_[objectIndex].object);
        const std::uint32_t index = resourceScopes_.Size();
        Base::Result<void> appendResult =
            resourceScopes_.PushBack(std::move(scope));
        if (!appendResult) {
            return appendResult.GetStatus();
        }
        frame.resourceScopeIndex = index;
        if (documentRoot) {
            documentResourceScopeIndex_ = index;
        }
    }
    return {};
}

Base::Result<void> ObjectBuilder::ActivatePendingNamespaces(
    std::uint32_t& bindingStart) noexcept {
    bindingStart = namespaceBindings_.Size();
    for (PendingNamespaceRecord& pending : pendingNamespaces_) {
        NamespaceBindingRecord binding;
        Base::Result<void> prefixResult = binding.prefix.AssignUnchecked(
            pending.prefix.View());
        if (!prefixResult) {
            return prefixResult.GetStatus();
        }
        Base::Result<void> uriResult = binding.uri.AssignUnchecked(
            pending.uri.View());
        if (!uriResult) {
            return uriResult.GetStatus();
        }
        Base::Result<void> appendResult =
            namespaceBindings_.PushBack(std::move(binding));
        if (!appendResult) {
            return appendResult.GetStatus();
        }
    }
    pendingNamespaces_.Clear();
    return {};
}

void ObjectBuilder::PopNamespaceBindings(
    std::uint32_t bindingStart) noexcept {
    if (bindingStart == InvalidIndex) {
        return;
    }
    while (namespaceBindings_.Size() > bindingStart) {
        namespaceBindings_.PopBack();
    }
}

Base::Result<Base::StringView> ObjectBuilder::LookupNamespace(
    Base::StringView prefix) const noexcept {
    if (prefix == XmlPrefix) {
        return XmlNamespaceUri;
    }
    for (std::uint32_t index = namespaceBindings_.Size();
         index > 0U;
         --index) {
        const NamespaceBindingRecord& binding =
            namespaceBindings_[index - 1U];
        if (binding.prefix.View() == prefix) {
            return binding.uri.View();
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "XAML namespace prefix is not bound in the active scope");
}

ExtensionServices ObjectBuilder::BuildExtensionServices(
    std::uint32_t targetObjectIndex,
    const ResolvedMember& member,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    ExtensionServices services;
    services.schema = schema_;
    if (targetObjectIndex < created_.Size()) {
        services.targetObject = created_[targetObjectIndex].object.Get();
        services.targetObjectType = created_[targetObjectIndex].type;
    }
    services.targetMember = member.id;
    services.targetValueType = member.valueType;
    if (rootObjectIndex_ < created_.Size()) {
        services.rootObject = created_[rootObjectIndex_].object.Get();
    }
    if (loadContext_ != nullptr) {
        services.templatedParent = loadContext_->templatedParent;
        services.baseUri = loadContext_->baseUri;
        services.effectiveValues = loadContext_->effectiveValues;
        services.bindings = loadContext_->bindings;
        services.fallbackResources = loadContext_->fallbackResources;
    }
    services.source = source;
    services.nameScope = FindActiveNameScope();
    services.namespaces = NamespaceScope(
        &ObjectBuilder::NamespaceLookupCallback,
        this);
    services.resources = ResourceResolver(
        &ObjectBuilder::ResourceLookupCallback,
        this);
    serviceResourceChain_.Clear();
    for (std::uint32_t index = frames_.Size();
         index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind != FrameKind::Object ||
            frame.resourceScopeIndex == InvalidIndex ||
            frame.resourceScopeIndex >= resourceScopes_.Size()) {
            continue;
        }
        const Aero::ResourceDictionary* dictionary =
            resourceScopes_[frame.resourceScopeIndex].external;
        if (dictionary == nullptr) {
            dictionary =
                &resourceScopes_[
                    frame.resourceScopeIndex].resources;
        }
        bool duplicate = false;
        for (const Aero::ResourceDictionary* existing :
             serviceResourceChain_) {
            if (existing == dictionary) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            Base::Result<void> added =
                serviceResourceChain_.PushBack(dictionary);
            if (!added) {
                serviceResourceChain_.Clear();
                break;
            }
        }
    }
    if (loadContext_ != nullptr &&
        loadContext_->resources != nullptr) {
        bool duplicate = false;
        for (const Aero::ResourceDictionary* existing :
             serviceResourceChain_) {
            if (existing == loadContext_->resources) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            Base::Result<void> added =
                serviceResourceChain_.PushBack(
                    loadContext_->resources);
            if (!added) {
                serviceResourceChain_.Clear();
            }
        }
    }
    services.ambientResourceChain = {
        serviceResourceChain_.Data(),
        serviceResourceChain_.Size()};
    services.visualContent = &resultVisualContent_;
    services.deferredContentOwner =
        FindDeferredContentOwner();
    services.deferredContent =
        &deferredContent_;
    return services;
}

const Aero::NameScope*
ObjectBuilder::FindActiveNameScope() const noexcept {
    for (std::uint32_t index = frames_.Size(); index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind == FrameKind::Object &&
            frame.nameScopeIndex != InvalidIndex &&
            frame.nameScopeIndex < nameScopes_.Size()) {
            return &nameScopes_[frame.nameScopeIndex].names;
        }
    }
    return nullptr;
}

Base::Object*
ObjectBuilder::FindDeferredContentOwner() const noexcept {
    for (std::uint32_t index = frames_.Size();
         index > 0U;
         --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind != FrameKind::Object ||
            frame.objectIndex >= created_.Size()) {
            continue;
        }
        const CreatedObjectRecord& record =
            created_[frame.objectIndex];
        if (schema_->DefersVisualContent(
                record.type)) {
            return record.object.Get();
        }
    }
    return nullptr;
}

std::uint32_t ObjectBuilder::FindNameScopeIndexForObject(
    std::uint32_t objectIndex) const noexcept {
    const std::uint32_t objectFrame = FindObjectFrameIndex(objectIndex);
    if (objectFrame == InvalidIndex) {
        return InvalidIndex;
    }
    for (std::uint32_t index = objectFrame + 1U; index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind != FrameKind::Object ||
            frame.nameScopeIndex == InvalidIndex) {
            continue;
        }
        // UserControl/Page own a nested NameScope for names inside their
        // content. Their own x:Name belongs to the enclosing namescope so
        // sibling ElementName bindings (MultiBinding ElementName=BgR) resolve.
        if (frame.objectIndex == objectIndex &&
            frame.nameScopeIndex != documentNameScopeIndex_) {
            continue;
        }
        return frame.nameScopeIndex;
    }
    return documentNameScopeIndex_;
}

std::uint32_t ObjectBuilder::FindResourceScopeIndexForParent() const noexcept {
    for (std::uint32_t index = frames_.Size(); index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind == FrameKind::Object &&
            frame.resourceScopeIndex != InvalidIndex) {
            return frame.resourceScopeIndex;
        }
    }
    return InvalidIndex;
}

std::uint32_t ObjectBuilder::FindObjectFrameIndex(
    std::uint32_t objectIndex) const noexcept {
    for (std::uint32_t index = frames_.Size(); index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind == FrameKind::Object &&
            frame.objectIndex == objectIndex) {
            return index - 1U;
        }
    }
    return InvalidIndex;
}

bool ObjectBuilder::IsXamlDirective(
    const QualifiedName& name,
    Base::StringView localName) const noexcept {
    return name.NamespaceUri() == LanguageNamespaceUri() &&
        name.LocalName() == localName;
}

bool ObjectBuilder::IsXamlNullObject(
    const QualifiedName& name) const noexcept {
    return IsXamlDirective(name, DirectiveNull);
}

bool ObjectBuilder::HasPropertyElementSyntax(
    const QualifiedName& name) const noexcept {
    for (char character : name.LocalName()) {
        if (character == '.') {
            return true;
        }
    }
    return false;
}

bool ObjectBuilder::IsWhitespaceOnly(
    Base::StringView value) const noexcept {
    for (char character : value) {
        if (!IsAsciiWhitespace(character)) {
            return false;
        }
    }
    return true;
}

ObjectBuilder::AssignmentRecord* ObjectBuilder::FindAssignment(
    std::uint32_t objectIndex,
    Meta::MemberId member) noexcept {
    for (AssignmentRecord& assignment : assignments_) {
        if (assignment.objectIndex == objectIndex &&
            assignment.member == member) {
            return &assignment;
        }
    }
    return nullptr;
}

void ObjectBuilder::CommitDocumentScopes() noexcept {
    committedNames_.Clear();
    committedResources_.Clear();
    if (documentNameScopeIndex_ < nameScopes_.Size()) {
        committedNames_ = std::move(
            nameScopes_[documentNameScopeIndex_].names);
    }
    if (documentResourceScopeIndex_ < resourceScopes_.Size()) {
        committedResources_ = std::move(
            resourceScopes_[documentResourceScopeIndex_].resources);
    }
}

void ObjectBuilder::AbortTransaction() noexcept {
    root_.Reset();
    resultVisualContent_.ReleaseContent();
    resultVisualContent_.Clear();
    deferredContent_.ReleaseAll();
    for (std::uint32_t index = extensionEffects_.Size();
         index > 0U; --index) {
        extensionEffects_[index - 1U].Rollback();
    }
    extensionEffects_.Clear();
    for (std::uint32_t index = created_.Size(); index > 0U; --index) {
        CreatedObjectRecord& record = created_[index - 1U];
        if (record.beginCalled && record.object) {
            schema_->AbortInit(record.type, *record.object);
        }
    }
    ClearTransaction();
}

void ObjectBuilder::ClearTransaction() noexcept {
    deferredContent_.ReleaseAll();
    frames_.Clear();
    assignments_.Clear();
    deferredStaticResources_.Clear();
    extensionEffects_.Clear();
    nameScopes_.Clear();
    resourceScopes_.Clear();
    serviceResourceChain_.Clear();
    namespaceBindings_.Clear();
    pendingNamespaces_.Clear();
    created_.Clear();
    root_.Reset();
    rootObjectIndex_ = InvalidIndex;
    documentNameScopeIndex_ = InvalidIndex;
    documentResourceScopeIndex_ = InvalidIndex;
    ended_ = false;
    hasDeferredStaticResources_ = false;
}

Base::Status ObjectBuilder::Failure(
    Base::Status status,
    ::Aero::Diagnostics::DiagnosticCode diagnostic,
    Base::StringView message,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (diagnostics_ != nullptr) {
        Base::Result<::Aero::Diagnostics::Diagnostic> item = ::Aero::Diagnostics::Diagnostic::Create(
            diagnostic,
            ::Aero::Diagnostics::DiagnosticSeverity::Error,
            message,
            source,
            ::Aero::Diagnostics::InvalidDiagnosticObjectId,
            Meta::InvalidMemberId);
        if (!item) {
            return item.GetStatus();
        }
        Base::Result<void> reportResult = diagnostics_->Report(
            std::move(item).Value());
        if (!reportResult) {
            return reportResult.GetStatus();
        }
    }
    return status;
}

Base::Result<Base::StringView> ObjectBuilder::NamespaceLookupCallback(
    void* context,
    Base::StringView prefix) noexcept {
    if (context == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageNamespaceState.Data());
    }
    return static_cast<ObjectBuilder*>(context)->LookupNamespace(prefix);
}

Base::Result<Aero::ResourceValue> ObjectBuilder::ResourceLookupCallback(
    void* context,
    Base::StringView key) noexcept {
    if (context == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageStaticResourceNotFound.Data());
    }
    return static_cast<ObjectBuilder*>(context)->LookupResource(key);
}

} // namespace Aero::Markup
