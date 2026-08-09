#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Stream.hpp>
#include <Aero/Base/StringView.hpp>

#include <cstdint>

namespace Aero::Markup {

struct StreamResourceInfo {
    Base::ResourceUri uri;
    Ref<Base::Stream> stream;
    std::uint64_t revision = 0U;
};

using XamlProviderChangedHandler =
    Base::Delegate<void(const Base::ResourceUri&)>;

class AERO_GUI_API XamlProvider : public Base::Object {
public:
    virtual ~XamlProvider() = default;

    virtual Result<StreamResourceInfo> Open(
        const Base::ResourceUri& uri) const noexcept = 0;
    virtual Result<std::uint64_t> Revision(
        const Base::ResourceUri&) const noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "XAML provider does not expose revision probes");
    }

    void AddChangedHandler(
        const XamlProviderChangedHandler& handler) noexcept;
    bool RemoveChangedHandler(
        const XamlProviderChangedHandler& handler) noexcept;

protected:
    // Providers configured on a Gui must raise notifications on that Gui's
    // dispatcher thread. An empty URI invalidates every source from this
    // provider.
    void RaiseChanged(const Base::ResourceUri& uri = {}) noexcept;

private:
    XamlProviderChangedHandler changed_;
};

using XamlOpenCallback = Result<StreamResourceInfo> (*)(
    const Base::ResourceUri& uri,
    void* context) noexcept;
using XamlRevisionCallback = Result<std::uint64_t> (*)(
    const Base::ResourceUri& uri,
    void* context) noexcept;

class AERO_GUI_API XamlProviderAdapter : public XamlProvider {
public:
    XamlProviderAdapter() noexcept = default;
    XamlProviderAdapter(
        XamlOpenCallback open,
        void* context = nullptr,
        XamlRevisionCallback revision = nullptr) noexcept
        : open_(open), revision_(revision), context_(context) {}

    bool IsValid() const noexcept { return open_ != nullptr; }

    Result<StreamResourceInfo> Open(
        const Base::ResourceUri& uri) const noexcept override;
    Result<std::uint64_t> Revision(
        const Base::ResourceUri& uri) const noexcept override;

private:
    XamlOpenCallback open_ = nullptr;
    XamlRevisionCallback revision_ = nullptr;
    void* context_ = nullptr;
};

} // namespace Aero::Markup

