#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Stream.hpp>
#include <Aero/Base/StringView.hpp>

#include <cstdint>

namespace Aero::Markup {

struct StreamResourceInfo {
    Base::ResourceUri uri;
    Base::Ref<Base::Stream> stream;
    std::uint64_t revision = 0U;
};

class AERO_GUI_API XamlProvider {
public:
    virtual ~XamlProvider() = default;

    virtual Base::Result<StreamResourceInfo> Open(
        const Base::ResourceUri& uri) const noexcept = 0;
    virtual Base::Result<std::uint64_t> Revision(
        const Base::ResourceUri&) const noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML provider does not expose revision probes");
    }
    virtual std::uint64_t CacheIdentity() const noexcept {
        return Base::DefaultHash<const XamlProvider*>{}(this);
    }
};

using XamlOpenCallback = Base::Result<StreamResourceInfo> (*)(
    const Base::ResourceUri& uri,
    void* context) noexcept;
using XamlRevisionCallback = Base::Result<std::uint64_t> (*)(
    const Base::ResourceUri& uri,
    void* context) noexcept;

class AERO_GUI_API XamlProviderAdapter : public XamlProvider {
public:
    XamlProviderAdapter() noexcept = default;
    XamlProviderAdapter(
        XamlOpenCallback open,
        void* context = nullptr,
        XamlRevisionCallback revision = nullptr,
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
    XamlOpenCallback open_ = nullptr;
    XamlRevisionCallback revision_ = nullptr;
    void* context_ = nullptr;
    std::uint64_t cacheIdentity_ = 0U;
};

} // namespace Aero::Markup

