#pragma once

#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>

namespace Aero::Data {

class AERO_GUI_API PropertyPath {
public:
    PropertyPath() noexcept = default;
    explicit PropertyPath(StringView path) noexcept { static_cast<void>(path_.Assign(path)); }

    StringView GetPath() const noexcept { return path_.View(); }
    bool GetIsEmpty() const noexcept { return path_.Empty(); }
    void SetPath(StringView value) noexcept { (void)path_.Assign(value); }

private:
    String path_;
};
} // namespace Aero::Data
