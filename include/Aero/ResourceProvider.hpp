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

namespace Aero {

struct ResourceStreamInfo {
    Base::ResourceUri uri;
    Base::Ref<Base::Stream> stream;
    std::uint64_t revision = 0U;
};

using ResourceChangedHandler = Base::Delegate<void(const Base::ResourceUri&)>;

// Unified ResourceProvider (Virtual File System / Asset Provider facade).
// Hosts can implement this single interface to supply .xaml, images, fonts, and binaries
// to the AeroGUI runtime, avoiding the need to implement multiple distinct providers.
class AERO_GUI_API ResourceProvider : public Base::Object {
    AERO_DECLARE_TYPE(ResourceProvider, Base::Object)

public:
    ResourceProvider() noexcept = default;
    ~ResourceProvider() override = default;

    virtual Base::Result<ResourceStreamInfo> Open(const Base::ResourceUri& uri) const noexcept = 0;

    virtual bool Exists(const Base::ResourceUri& uri) const noexcept {
        Base::Result<ResourceStreamInfo> res = Open(uri);
        return res && res.Value().stream != nullptr;
    }

    virtual Base::Result<std::uint64_t> Revision(const Base::ResourceUri&) const noexcept {
        return 0U;
    }

    void AddChangedHandler(const ResourceChangedHandler& handler) noexcept {
        changed_.Add(handler);
    }

    bool RemoveChangedHandler(const ResourceChangedHandler& handler) noexcept {
        return changed_.Remove(handler);
    }

protected:
    void RaiseChanged(const Base::ResourceUri& uri = {}) noexcept {
        if (changed_) changed_(uri);
    }

private:
    ResourceChangedHandler changed_;
};

} // namespace Aero
