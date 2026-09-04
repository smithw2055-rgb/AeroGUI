#include "gui/markup/XamlObjectWriterInternal.hpp"

// Name scopes, resource scopes, and deferred content / StaticResource.

namespace Aero::Markup {

Base::Result<void> ObjectBuilder::ResolveDeferredStaticResources() noexcept {
    for (DeferredStaticResourceRecord& deferred :
         deferredStaticResources_) {
        Base::Result<Aero::ResourceValue> resource =
            LookupResource(deferred.key.View());
        if (!resource) {
            Base::Result<Base::String> message =
                StaticResourceNotFoundMessage(
                    deferred.key.View());
            if (!message) return message.GetStatus();
            return Failure(
                resource.GetStatus(),
                XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,
                message.Value().View(),
                deferred.source);
        }
        Base::Result<void> written = WriteValue(
            deferred.targetObjectIndex,
            deferred.member,
            std::move(resource).Value(),
            deferred.source,
            deferred.hasPolicy
                ? &deferred.policy
                : nullptr);
        if (!written) return written.GetStatus();
    }
    deferredStaticResources_.Clear();
    hasDeferredStaticResources_ = false;
    return {};
}

Base::Result<void> ObjectBuilder::FinalizeDeferredStyles() noexcept {
    for (std::uint32_t index = 0U; index < created_.Size(); ++index) {
        CreatedObjectRecord& record = created_[index];
        if (!record.endCalled || !record.object) {
            continue;
        }
        if (record.type == Aero::Style::StaticTypeId()) {
            auto& style = static_cast<Aero::Style&>(*record.object);
            if (style.GetIsSealed()) {
                continue;
            }
            Base::Result<void> endResult = schema_->EndInit(
                record.type,
                *record.object,
                BuildExtensionServices(index, {}, {}));
            if (!endResult) {
                return endResult.GetStatus();
            }
        }
    }
    return {};
}

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

Base::Result<void> DeferredContentPlan::Stage(
    Base::Object& owner,
    Base::Object& parent,
    const Base::Ref<Base::Object>& child,
    ::Aero::Meta::Registry& metadata,
    Meta::MemberId member) noexcept {
    if (!child || member == Meta::InvalidMemberId ||
        !metadata.IsReady()) {
        return InvalidContentState(
            "Deferred XAML content edge is invalid");
    }
    Base::Result<void> retained =
        edges_.PushBack({
            &owner,
            &parent,
            child,
            &metadata,
            member,
            false});
    if (!retained) return retained.GetStatus();
    Base::Result<void> written =
        metadata.WriteContent(parent, member, child);
    if (!written) {
        edges_.PopBack();
        return written.GetStatus();
    }
    return {};
}

Base::Result<void> DeferredContentPlan::StageProperty(
    Base::Object& owner,
    Base::Object& parent,
    const Base::Ref<Base::Object>& child,
    ::Aero::Meta::Registry& metadata,
    Meta::MemberId member) noexcept {
    if (!child ||
        member == Meta::InvalidMemberId) {
        return InvalidContentState(
            "Deferred XAML structural property edge is invalid");
    }
    const Meta::PropertyInfo* property =
        metadata.Types().FindProperty(member);
    if (property == nullptr) {
        return InvalidContent(
            "Deferred XAML structural property was not found");
    }
    Base::Result<void> retained =
        edges_.PushBack({
            &owner,
            &parent,
            child,
            &metadata,
            member,
            true});
    if (!retained) return retained.GetStatus();
    const Meta::Value value =
        Meta::Value::FromObject(
            property->ValueType(), child);
    Base::Result<void> written =
        metadata.SetProperty(
            parent, member, value);
    if (!written) {
        edges_.PopBack();
        return written.GetStatus();
    }
    return {};
}

Base::Result<void> DeferredContentPlan::CopyForOwner(
    const Base::Object& owner,
    Base::Vector<DeferredContentEdge>& output) const noexcept {
    output.Clear();
    for (const DeferredContentEdge& edge : edges_) {
        if (edge.owner != &owner) continue;
        Base::Result<void> copied =
            output.PushBack(edge);
        if (!copied) {
            output.Clear();
            return copied.GetStatus();
        }
    }
    return {};
}

Base::Result<void> DeferredContentPlan::StageBinding(
    Base::Object& owner,
    Base::Object* source,
    Base::StringView sourceName,
    Base::StringView relativeAncestorType,
    std::uint32_t relativeAncestorLevel,
    ::Aero::DependencyObject& target,
    ::Aero::Meta::Registry& metadata,
    Meta::DependencyPropertyHandle targetProperty,
    Meta::DependencyPropertyHandle dataContextProperty,
    Base::StringView path,
    Base::StringView stringFormat,
    Data::BindingMode mode,
    Meta::UpdateSourceTrigger updateSourceTrigger,
    bool bindsToSource,
    const Base::Ref<Data::IValueConverter>& converter,
    const Meta::PropertyValue& converterParameter) noexcept {
    if (!targetProperty.IsValid() ||
        (path.Empty() && !bindsToSource) ||
        !metadata.IsReady()) {
        return InvalidContentState(
            "Deferred XAML Binding declaration is invalid");
    }
    DeferredBindingEdge edge;
    edge.owner = &owner;
    edge.source = source;
    Base::Result<void> sourceAssigned =
        edge.sourceName.Assign(sourceName);
    if (!sourceAssigned) return sourceAssigned.GetStatus();
    sourceAssigned = edge.relativeAncestorType.Assign(
        relativeAncestorType);
    if (!sourceAssigned) return sourceAssigned.GetStatus();
    edge.relativeAncestorLevel = relativeAncestorLevel;
    edge.target = &target;
    edge.metadata = &metadata;
    edge.targetProperty = targetProperty;
    edge.dataContextProperty = dataContextProperty;
    edge.mode = mode;
    edge.bindsToSource = bindsToSource;
    edge.updateSourceTrigger = updateSourceTrigger;
    edge.converter = converter;
    edge.converterParameter = converterParameter;
    Base::Result<void> assigned =
        edge.path.Assign(path);
    if (!assigned) return assigned.GetStatus();
    assigned = edge.stringFormat.Assign(
        stringFormat);
    if (!assigned) return assigned.GetStatus();
    return bindings_.PushBack(std::move(edge));
}

Base::Result<void>
DeferredContentPlan::CopyBindingsForOwner(
    const Base::Object& owner,
    Base::Vector<DeferredBindingEdge>& output) const noexcept {
    output.Clear();
    for (const DeferredBindingEdge& edge : bindings_) {
        if (edge.owner != &owner) continue;
        Base::Result<void> copied =
            output.PushBack(edge);
        if (!copied) {
            output.Clear();
            return copied.GetStatus();
        }
    }
    return {};
}

void DeferredContentPlan::ReleaseOwner(
    Base::Object& owner) noexcept {
    for (std::uint32_t index = 0U;
         index < edges_.Size();
         ++index) {
        DeferredContentEdge& edge = edges_[index];
        if (edge.owner != &owner) continue;
        bool firstForParent = true;
        for (std::uint32_t earlier = 0U;
             earlier < index;
             ++earlier) {
            firstForParent =
                firstForParent &&
                (edges_[earlier].owner != &owner ||
                 edges_[earlier].parent !=
                     edge.parent ||
                 (edge.property &&
                  edges_[earlier].member !=
                      edge.member));
        }
        if (firstForParent &&
            edge.parent != nullptr &&
            edge.metadata != nullptr) {
            if (edge.property) {
                const Meta::PropertyInfo* property =
                    edge.metadata->Types().
                        FindProperty(edge.member);
                if (property != nullptr) {
                    (void)edge.metadata->SetProperty(
                        *edge.parent,
                        edge.member,
                        Meta::Value::NullObject(
                            property->ValueType()));
                }
            } else {
                (void)edge.metadata->ClearContent(
                    *edge.parent,
                    edge.member);
            }
        }
    }

    std::uint32_t output = 0U;
    for (std::uint32_t index = 0U;
         index < edges_.Size();
         ++index) {
        DeferredContentEdge& edge = edges_[index];
        if (edge.owner == &owner) continue;
        if (output != index) {
            edges_[output] = std::move(edge);
        }
        ++output;
    }
    (void)edges_.Resize(output);

    output = 0U;
    for (std::uint32_t index = 0U;
         index < bindings_.Size();
         ++index) {
        DeferredBindingEdge& edge = bindings_[index];
        if (edge.owner == &owner) continue;
        if (output != index) {
            bindings_[output] = std::move(edge);
        }
        ++output;
    }
    (void)bindings_.Resize(output);
}

void DeferredContentPlan::ReleaseAll() noexcept {
    while (!edges_.Empty() || !bindings_.Empty()) {
        Base::Object* owner = !edges_.Empty()
            ? edges_.Front().owner
            : bindings_.Front().owner;
        if (owner == nullptr) {
            edges_.Clear();
            return;
        }
        ReleaseOwner(*owner);
    }
}

} // namespace Aero::Markup
