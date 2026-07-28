#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>

#include <cstdint>

namespace Aero::Integration {

struct Source final {
    Base::ResourceUri uri;
    Base::Vector<std::uint8_t> bytes;
    std::uint64_t revision = 0U;

    Base::StringView Text() const noexcept {
        return Base::StringView(
            reinterpret_cast<const char*>(bytes.Data()),
            bytes.Size());
    }
};

class AERO_API ISourceProvider {
public:
    virtual ~ISourceProvider() = default;

    virtual Base::Result<Source> Load(
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

using SourceLoadCallback = Base::Result<Source> (*)(
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
        SourceLoadCallback load,
        void* context = nullptr,
        SourceRevisionCallback revision = nullptr,
        std::uint64_t cacheIdentity = 0U) noexcept
        : load_(load), revision_(revision), context_(context),
          cacheIdentity_(cacheIdentity) {}

    bool IsValid() const noexcept { return load_ != nullptr; }

    Base::Result<Source> Load(
        const Base::ResourceUri& uri) const noexcept override;
    Base::Result<std::uint64_t> Revision(
        const Base::ResourceUri& uri) const noexcept override;
    std::uint64_t CacheIdentity() const noexcept override;

private:
    SourceLoadCallback load_ = nullptr;
    SourceRevisionCallback revision_ = nullptr;
    void* context_ = nullptr;
    std::uint64_t cacheIdentity_ = 0U;
};

} // namespace Aero::Integration
