#include <Aero/Integration/Providers/XamlProvider.hpp>

namespace Aero::Integration {

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

std::uint64_t
XamlProviderAdapter::CacheIdentity() const noexcept {
    return cacheIdentity_ != 0U
        ? cacheIdentity_
        : XamlProvider::CacheIdentity();
}

} // namespace Aero::Integration
