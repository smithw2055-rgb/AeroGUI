#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Base/Stream.hpp>

#include <cstdint>

namespace Aero::Integration {

struct StreamResourceInfo final {
    Base::ResourceUri uri;
    Base::Ref<Base::Stream> stream;
    std::uint64_t revision = 0U;
};

class AERO_API ISourceProvider {
public:
    virtual ~ISourceProvider() = default;

    virtual Base::Result<StreamResourceInfo> Open(
        const Base::ResourceUri& uri) const noexcept = 0;
    virtual Base::Result<std::uint64_t> Revision(
        const Base::ResourceUri&) const noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Source provider does not expose revision probes");
    }
    virtual std::uint64_t CacheIdentity() const noexcept {
        return Base::DefaultHash<const ISourceProvider*>{}(this);
    }
};

using SourceOpenCallback = Base::Result<StreamResourceInfo> (*)(
    const Base::ResourceUri& uri,
    void* context) noexcept;
using SourceRevisionCallback = Base::Result<std::uint64_t> (*)(
    const Base::ResourceUri& uri,
    void* context) noexcept;

class AERO_API SourceProviderAdapter final
    : public ISourceProvider {
public:
    SourceProviderAdapter() noexcept = default;
    SourceProviderAdapter(
        SourceOpenCallback open,
        void* context = nullptr,
        SourceRevisionCallback revision = nullptr,
        std::uint64_t cacheIdentity = 0U) noexcept
        : open_(open), revision_(revision), context_(context),
          cacheIdentity_(cacheIdentity) {}

    bool IsValid() const noexcept { return open_ != nullptr; }

    Base::Result<StreamResourceInfo> Open(
        const Base::ResourceUri& uri) const noexcept override;
    Base::Result<std::uint64_t> Revision(
        const Base::ResourceUri& uri) const noexcept override;
    std::uint64_t CacheIdentity() const noexcept override;

private:
    SourceOpenCallback open_ = nullptr;
    SourceRevisionCallback revision_ = nullptr;
    void* context_ = nullptr;
    std::uint64_t cacheIdentity_ = 0U;
};

} // namespace Aero::Integration
