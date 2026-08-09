#include <Aero/Markup/XamlProvider.hpp>

namespace Aero::Markup {

void XamlProvider::AddChangedHandler(
    const XamlProviderChangedHandler& handler) noexcept {
    changed_.Add(handler);
}

bool XamlProvider::RemoveChangedHandler(
    const XamlProviderChangedHandler& handler) noexcept {
    return changed_.Remove(handler);
}

void XamlProvider::RaiseChanged(
    const Base::ResourceUri& uri) noexcept {
    if (changed_) changed_(uri);
}

Base::Result<StreamResourceInfo>
XamlProviderAdapter::Open(
    const Base::ResourceUri& uri) const noexcept {
    if (open_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML provider has no open callback");
    }
    return open_(uri, context_);
}

Base::Result<std::uint64_t>
XamlProviderAdapter::Revision(
    const Base::ResourceUri& uri) const noexcept {
    return revision_ != nullptr
        ? revision_(uri, context_)
        : XamlProvider::Revision(uri);
}

} // namespace Aero::Markup
