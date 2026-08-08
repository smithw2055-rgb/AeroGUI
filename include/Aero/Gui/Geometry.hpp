#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Gui/Transform.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/Value.hpp>

#include <utility>

namespace Aero::Media {
using Point = Base::Point;
using Size = Base::Size;
using Rect = Base::Rect;
using Thickness = Base::Thickness;
using CornerRadius = Base::CornerRadius;
class AERO_GUI_API Geometry : public Freezable {
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
    inline static constexpr DependencyProperty<Base::Ref<Transform>> TransformProperty{"Transform"};
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
class AERO_GUI_API StreamGeometry : public Geometry {
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

class AERO_GUI_API PathSegment : public Freezable {
    AERO_DECLARE_TYPE(PathSegment, Freezable)
protected:
    explicit PathSegment(Meta::TypeId runtimeType) noexcept
        : Freezable(runtimeType) {}
    ~PathSegment() override = default;
};

class AERO_GUI_API LineSegment : public PathSegment {
    AERO_DECLARE_TYPE(LineSegment, PathSegment)
public:
    LineSegment() noexcept : PathSegment(StaticTypeId()) {}
    Point GetPoint() const noexcept {
        return GetValueOr(PointProperty, Point{});
    }
    void SetPoint(Point value) noexcept {
        SetValue(PointProperty, value);
    }
    inline static constexpr DependencyProperty<Point> PointProperty{"Point"};
};

class AERO_GUI_API PathFigure : public Freezable {
    AERO_DECLARE_TYPE(PathFigure, Freezable)
public:
    PathFigure() noexcept : Freezable(StaticTypeId()) {}
    Point GetStartPoint() const noexcept {
        return GetValueOr(StartPointProperty, Point{});
    }
    void SetStartPoint(Point value) noexcept {
        SetValue(StartPointProperty, value);
    }
    bool GetIsClosed() const noexcept {
        return GetValueOr(IsClosedProperty, false);
    }
    void SetIsClosed(bool value) noexcept {
        SetValue(IsClosedProperty, value);
    }
    Base::Result<void> AddSegment(Base::Ref<PathSegment> value) noexcept;
    void ClearSegments() noexcept {
        if (!WritePreamble()) return;
        segments_.Clear();
        WritePostscript();
    }
    Base::Span<const Base::Ref<PathSegment>> GetSegments() const noexcept {
        return segments_.AsSpan();
    }
    inline static constexpr DependencyProperty<Point> StartPointProperty{"StartPoint"};
    inline static constexpr DependencyProperty<bool> IsClosedProperty{"IsClosed"};
private:
    Base::Vector<Base::Ref<PathSegment>> segments_;
};

class AERO_GUI_API PathGeometry : public Geometry {
    AERO_DECLARE_TYPE(PathGeometry, Geometry)
public:
    PathGeometry() noexcept : Geometry(StaticTypeId()) {}
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Result<void> AddFigure(Base::Ref<PathFigure> value) noexcept;
    void ClearFigures() noexcept {
        if (!WritePreamble()) return;
        figures_.Clear();
        WritePostscript();
    }
    Base::Span<const Base::Ref<PathFigure>> GetFigures() const noexcept {
        return figures_.AsSpan();
    }
    Base::Result<Base::String> ToStreamData() const noexcept;
private:
    Base::Vector<Base::Ref<PathFigure>> figures_;
};
} // namespace Aero::Media
