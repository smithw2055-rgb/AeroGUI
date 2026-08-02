#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Value.hpp>

#include <utility>

namespace Aero::Media {
using Point = Base::Point;
using Size = Base::Size;
using Rect = Base::Rect;
using Thickness = Base::Thickness;
using CornerRadius = Base::CornerRadius;
class AERO_API Geometry : public Base::Object {
    AERO_DECLARE_TYPE(Geometry, Base::Object)
public:
    Geometry() noexcept = default;
    ~Geometry() override = default;
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    virtual Rect GetBounds() const noexcept { return {}; }
    Base::Ref<Transform> GetTransform() const noexcept {
        return transform_;
    }
    void SetTransform(
        Base::Ref<Transform> value) noexcept {
        transform_ = std::move(value);
        return;
    }
    inline static constexpr Members::Property<Base::Ref<Transform>> TransformProperty{"Transform"};
private:
    Base::Ref<Transform> transform_;
};

// Streaming geometry is the WPF-shaped geometry value used by path and
// vector controls. The textual value remains accepted for authored XAML while
// the type now has a distinct extension point for incremental path commands.
class AERO_API StreamGeometry : public Geometry {
    AERO_DECLARE_TYPE(StreamGeometry, Geometry)
public:
    StreamGeometry() noexcept = default;
    ~StreamGeometry() override = default;
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::StringView GetData() const noexcept { return data_.View(); }
    void SetData(Base::StringView value) noexcept {
        (void)data_.Assign(value);
    }
    Rect GetBounds() const noexcept override { return bounds_; }
    void SetBounds(Rect value) noexcept {
        bounds_ = value;
        return;
    }
private:
    Base::String data_;
    Rect bounds_{};
};
} // namespace Aero::Media
