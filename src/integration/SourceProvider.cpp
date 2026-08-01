#include <Aero/Integration/SourceProvider.hpp>

namespace Aero::Integration {

Base::Result<Source>
SourceProviderAdapter::Load(
    const Base::ResourceUri& uri) const noexcept {
    if (load_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Source provider has no load callback");
    }
    return load_(uri, context_);
}

Base::Result<std::uint64_t>
SourceProviderAdapter::Revision(
    const Base::ResourceUri& uri) const noexcept {
    return revision_ != nullptr
        ? revision_(uri, context_)
        : ISourceProvider::Revision(uri);
}

std::uint64_t
SourceProviderAdapter::CacheIdentity() const noexcept {
    return cacheIdentity_ != 0U
        ? cacheIdentity_
        : ISourceProvider::CacheIdentity();
}


} // namespace Aero::Integration
