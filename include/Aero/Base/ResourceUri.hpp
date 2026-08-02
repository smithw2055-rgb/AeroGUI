#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>

namespace Aero::Base {

// Canonical identifier for resources loaded by higher-level subsystems.
// ResourceUri performs syntax normalization only. Security policy (for
// example, whether http/https are allowed) belongs to the caller.
class AERO_API ResourceUri  {
public:
    ResourceUri() noexcept = default;

    ResourceUri(const ResourceUri&) = default;
    ResourceUri& operator=(const ResourceUri&) = default;
    ResourceUri(ResourceUri&&) noexcept = default;
    ResourceUri& operator=(ResourceUri&&) noexcept = default;

    static Result<ResourceUri> Parse(StringView text) noexcept;
    static Result<ResourceUri> Resolve(
        const ResourceUri& baseUri,
        StringView reference) noexcept;

    StringView Canonical() const noexcept {
        return canonical_.View();
    }
    StringView Scheme() const noexcept {
        return scheme_.View();
    }
    StringView Assembly() const noexcept {
        return assembly_.View();
    }
    StringView Path() const noexcept {
        return path_.View();
    }

    bool Empty() const noexcept {
        return canonical_.Empty();
    }
    bool IsAbsolute() const noexcept {
        return absolute_;
    }
    bool IsNetwork() const noexcept {
        return network_;
    }

private:
    static Result<void> Build(
        ResourceUri& uri,
        StringView scheme,
        StringView path,
        StringView prefix) noexcept;

    String canonical_;
    String scheme_;
    String assembly_;
    String path_;
    bool absolute_ = false;
    bool network_ = false;
};

inline bool operator==(
    const ResourceUri& left,
    const ResourceUri& right) noexcept {
    return left.Canonical() == right.Canonical();
}

inline bool operator!=(
    const ResourceUri& left,
    const ResourceUri& right) noexcept {
    return !(left == right);
}

} // namespace Aero::Base
