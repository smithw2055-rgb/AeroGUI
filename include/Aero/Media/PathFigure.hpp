#pragma once

#include <Aero/Media/FreezableCollection.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/PathSegment.hpp>

namespace Aero::Media {

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
    Result<void> AddSegment(Ref<PathSegment> value) noexcept;
    void ClearSegments() noexcept {
        if (!WritePreamble()) return;
        segments_.Clear();
        WritePostscript();
    }
    Span<const Ref<PathSegment>> GetSegments() const noexcept {
        return segments_.AsSpan();
    }
    inline static constexpr DependencyProperty<Point> StartPointProperty{"StartPoint"};
    inline static constexpr DependencyProperty<bool> IsClosedProperty{"IsClosed"};
private:
    FreezableCollection<PathSegment> segments_;
};
} // namespace Aero::Media
