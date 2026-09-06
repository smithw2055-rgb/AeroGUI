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
        return GetValue(StartPointProperty);
    }
    void SetStartPoint(Point value) noexcept {
        SetValue(StartPointProperty, value);
    }
    bool GetIsClosed() const noexcept {
        return GetValue(IsClosedProperty);
    }
    void SetIsClosed(bool value) noexcept {
        SetValue(IsClosedProperty, value);
    }
    void AddSegment(Ref<PathSegment> value) noexcept;
    void ClearSegments() noexcept {
        if (!WritePreamble()) return;
        segments_.Clear();
        WritePostscript();
    }
    Span<const Ref<PathSegment>> GetSegments() const noexcept {
        return segments_.AsSpan();
    }
    AERO_DEPENDENCY_PROPERTY(Point, StartPoint);
    AERO_DEPENDENCY_PROPERTY(bool, IsClosed);
private:
    FreezableCollection<PathSegment> segments_;
};
} // namespace Aero::Media
