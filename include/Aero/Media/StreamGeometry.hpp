#pragma once

#include <Aero/Media/Geometry.hpp>

namespace Aero::Media {

class AERO_GUI_API StreamGeometry : public Geometry {
    AERO_DECLARE_TYPE(StreamGeometry, Geometry)
public:
    StreamGeometry() noexcept : Geometry(StaticTypeId()) {}
    ~StreamGeometry() override = default;
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    StringView GetData() const noexcept { return data_.View(); }
    void SetData(StringView value) noexcept {
        if (!WritePreamble() || data_.View() == value) return;
        if (data_.Assign(value)) WritePostscript();
    }
    Rect GetBounds() const noexcept override { return bounds_; }
    void SetBounds(Rect value) noexcept {
        if (!WritePreamble() ||
            (bounds_.x == value.x && bounds_.y == value.y &&
             bounds_.width == value.width &&
             bounds_.height == value.height)) return;
        bounds_ = value;
        WritePostscript();
    }
private:
    String data_;
    Rect bounds_{};
};
} // namespace Aero::Media
