#include <Aero/Markup/Resources.hpp>

// Name and resource scope implementation.

namespace Aero::Markup {
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
