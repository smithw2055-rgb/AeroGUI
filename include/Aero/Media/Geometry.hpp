#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/Value.hpp>

#include <utility>

namespace Aero::Media {
using Point = Base::Point;
using Size = Base::Size;
using Rect = Base::Rect;
using Thickness = Base::Thickness;
using CornerRadius = Base::CornerRadius;
class AERO_API Geometry : public Freezable {
    AERO_DECLARE_TYPE(Geometry, Freezable)
public:
    Geometry() noexcept : Freezable(StaticTypeId()) {}
    ~Geometry() override;
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    virtual Rect GetBounds() const noexcept { return {}; }
    Base::Ref<Transform> GetTransform() const noexcept {
        return transform_;
    }
    void SetTransform(Base::Ref<Transform> value) noexcept;
    inline static constexpr Members::Property<Base::Ref<Transform>> TransformProperty{"Transform"};
private:
    void OnTransformChanged(Freezable&) noexcept;
    Base::Ref<Transform> transform_;
    FreezableChangedHandler transformChangedHandler_;

protected:
    explicit Geometry(Meta::TypeId runtimeType) noexcept
        : Freezable(runtimeType) {}
    bool FreezeCore(bool isChecking) noexcept override;
};

// Streaming geometry is the WPF-shaped geometry value used by path and
// vector controls. The textual value remains accepted for authored XAML while
// the type now has a distinct extension point for incremental path commands.
class AERO_API StreamGeometry : public Geometry {
    AERO_DECLARE_TYPE(StreamGeometry, Geometry)
public:
    StreamGeometry() noexcept : Geometry(StaticTypeId()) {}
    ~StreamGeometry() override = default;
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::StringView GetData() const noexcept { return data_.View(); }
    void SetData(Base::StringView value) noexcept {
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
    Base::String data_;
    Rect bounds_{};
};
} // namespace Aero::Media
