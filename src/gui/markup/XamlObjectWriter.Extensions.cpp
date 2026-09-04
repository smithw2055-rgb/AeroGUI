#include "gui/markup/XamlObjectWriterInternal.hpp"

// Markup-extension evaluation for the object writer node stream.
// Concrete extension providers compile from dedicated .cpp wrappers.

namespace Aero::Markup {

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

ObjectBuilder::MarkupValueKind ObjectBuilder::ParseMarkupValue(
    Base::StringView text,
    Base::StringView& extensionName,
    Base::StringView& argument) const noexcept {
    extensionName = {};
    argument = {};
    const Base::StringView value = TrimAscii(text);
    if (value.Empty() || value[0] != '{') {
        return MarkupValueKind::Literal;
    }
    if (value.SizeBytes() >= 2U && value[1] == '}') {
        argument = value.Substr(2U, value.SizeBytes() - 2U);
        return MarkupValueKind::EscapedLiteral;
    }
    if (value.SizeBytes() < 2U ||
        value[value.SizeBytes() - 1U] != '}') {
        return MarkupValueKind::Invalid;
    }

    const Base::StringView inner = TrimAscii(value.Substr(
        1U,
        value.SizeBytes() - 2U));
    if (inner == NullMarkup) {
        return MarkupValueKind::Null;
    }

    std::uint32_t nestedDepth = 0U;
    char quote = '\0';
    for (char character : inner) {
        if (quote != '\0') {
            if (character == quote) quote = '\0';
            continue;
        }
        if (character == '\'' || character == '"') {
            quote = character;
        } else if (character == '{') {
            ++nestedDepth;
        } else if (character == '}') {
            if (nestedDepth == 0U) {
                return MarkupValueKind::Invalid;
            }
            --nestedDepth;
        }
    }
    if (nestedDepth != 0U || quote != '\0') {
        return MarkupValueKind::Invalid;
    }

    std::uint32_t nameEnd = 0U;
    while (nameEnd < inner.SizeBytes() &&
           !IsAsciiWhitespace(inner[nameEnd]) && inner[nameEnd] != ',') {
        ++nameEnd;
    }
    if (nameEnd == 0U) {
        return MarkupValueKind::Invalid;
    }
    extensionName = inner.Substr(0U, nameEnd);

    argument = TrimAscii(inner.Substr(nameEnd, inner.SizeBytes() - nameEnd));
    if (!argument.Empty() && argument[0] == ',') {
        argument = TrimAscii(argument.Substr(1U, argument.SizeBytes() - 1U));
    }
    if (extensionName == StaticResourceMarkup) {
        if (argument.Empty()) {
            return MarkupValueKind::Invalid;
        }
        constexpr Base::StringView resourceKeyPrefix("ResourceKey=");
        if (argument.SizeBytes() > resourceKeyPrefix.SizeBytes() &&
            argument.Substr(0U, resourceKeyPrefix.SizeBytes()) ==
                resourceKeyPrefix) {
            argument = TrimAscii(argument.Substr(
                resourceKeyPrefix.SizeBytes(),
                argument.SizeBytes() - resourceKeyPrefix.SizeBytes()));
            if (argument.Empty()) return MarkupValueKind::Invalid;
        }
        return MarkupValueKind::StaticResource;
    }
    return MarkupValueKind::Extension;
}

Base::Result<ProvidedValue> ObjectBuilder::EvaluateMarkupExtension(
    std::uint32_t targetObjectIndex,
    const ResolvedMember& member,
    Base::StringView extensionName,
    Base::StringView arguments,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    std::uint32_t colon = extensionName.SizeBytes();
    for (std::uint32_t index = 0U;
         index < extensionName.SizeBytes();
         ++index) {
        if (extensionName[index] != ':') {
            continue;
        }
        if (colon != extensionName.SizeBytes()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageInvalidMarkupExtension.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
                MessageInvalidMarkupExtension,
                source);
        }
        colon = index;
    }

    Base::StringView prefix;
    Base::StringView localName = extensionName;
    if (colon != extensionName.SizeBytes()) {
        if (colon == 0U || colon + 1U >= extensionName.SizeBytes()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageInvalidMarkupExtension.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
                MessageInvalidMarkupExtension,
                source);
        }
        prefix = extensionName.Substr(0U, colon);
        localName = extensionName.Substr(
            colon + 1U,
            extensionName.SizeBytes() - colon - 1U);
    }

    Base::Result<Base::StringView> namespaceResult = LookupNamespace(prefix);
    if (!namespaceResult) {
        return Failure(
            namespaceResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension,
            MessageUnknownMarkupExtension,
            source);
    }
    Base::Result<const Meta::TypeInfo*> typeResult =
        schema_->ResolveType(
            namespaceResult.Value(),
            localName);
    if (!typeResult) {
        return Failure(
            typeResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension,
            MessageUnknownMarkupExtension,
            source);
    }

    const ExtensionServices services = BuildExtensionServices(
        targetObjectIndex,
        member,
        source);
    Base::Result<ProvidedValue> provided =
        schema_->ProvideMarkupExtensionValue(
            typeResult.Value()->Id(),
            arguments,
            services);
    if (!provided) {
        const bool missing =
            provided.GetStatus().code == Base::ErrorCode::Unsupported ||
            provided.GetStatus().code == Base::ErrorCode::NotFound;
        return Failure(
            provided.GetStatus(),
            missing
                ? XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension
                : XamlObjectWriterDiagnosticCodes::MarkupExtensionFailed,
            missing
                ? MessageUnknownMarkupExtension
                : MessageMarkupExtensionFailed,
            source);
    }
    return provided;
}

} // namespace Aero::Markup
