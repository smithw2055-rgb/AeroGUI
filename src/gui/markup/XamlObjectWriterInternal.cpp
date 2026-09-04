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

namespace Aero::Markup {
namespace WriterDetail {

Base::Result<Base::String> StaticResourceNotFoundMessage(
    Base::StringView key) noexcept {
    Base::String message;
    Base::Result<void> appended = message.Assign(
        "StaticResource key '");
    if (appended) appended = message.Append(key);
    if (appended) appended = message.Append(
        "' is not available; forward references are not supported");
    return appended
        ? Base::Result<Base::String>(std::move(message))
        : Base::Result<Base::String>(appended.GetStatus());
}

ResolvedMember ResolveCompiledMember(
    const CompiledMemberBinding& binding) noexcept {
    ResolvedMember member;
    member.id = binding.id;
    member.kind = binding.kind;
    member.ownerType = binding.ownerType;
    member.valueType = binding.valueType;
    member.propertyFlags = binding.propertyFlags;
    member.eventFlags = binding.eventFlags;
    member.attached = binding.attached;
    return member;
}

MemberWritePolicy ResolveCompiledMemberPolicy(
    const CompiledMemberBinding& binding) noexcept {
    MemberWritePolicy policy;
    policy.mode = binding.writeMode ==
            static_cast<std::uint8_t>(
                MemberWriteMode::Collection)
        ? MemberWriteMode::Collection
        : MemberWriteMode::SetOnce;
    if (binding.id ==
        VisualStateManager::VisualStateGroupsProperty.Handle().value) {
        policy.mode = MemberWriteMode::Collection;
    }
    policy.acceptsAnyValue =
        binding.acceptsAnyValue;
    policy.writable = binding.writable;
    return policy;
}

bool IsCompiledMemberCompatible(
    const Schema& schema,
    Meta::TypeId targetType,
    const ResolvedMember& member) noexcept {
    return member.attached ||
        schema.Types().IsDerivedFrom(
            targetType, member.ownerType);
}

} // namespace WriterDetail

// ===== Scopes =====

namespace {

constexpr const char* MessageNamespaceUnavailable =
    "XAML namespace scope is not available";
constexpr const char* MessageResourceResolverUnavailable =
    "XAML resource resolver is not available";

} // namespace

Base::Result<Base::StringView> NamespaceScope::Lookup(
    Base::StringView prefix) const noexcept {
    if (lookup_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            MessageNamespaceUnavailable);
    }
    return lookup_(context_, prefix);
}

Base::Result<Aero::ResourceValue> ResourceResolver::Lookup(
    Base::StringView key) const noexcept {
    if (lookup_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            MessageResourceResolverUnavailable);
    }
    return lookup_(context_, key);
}

} // namespace Aero::Markup
