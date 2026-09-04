// Shared contour-recording FlattenSink implementations (P4.4).
// Included by GeometryFlatten.cpp (fill/scanline) and DrawingContext.cpp
// (stroke) so the single-flatten fan-out observes exactly the same contour
// streams as the former per-purpose flatten passes. Single source: any
// semantic change here is guarded by the geometry conformance suite plus
// TestDrawGeometryFillStrokeScene.
#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Media/Geometry.hpp>

#include <cmath>
#include <cstdint>

namespace Aero::Media {

inline bool FlattenSinksSamePoint(Point left, Point right) noexcept {
    constexpr double Epsilon = 1.0e-9;
    return std::abs(left.x - right.x) <= Epsilon &&
        std::abs(left.y - right.y) <= Epsilon;
}

class GeometryFillSink final : public FlattenSink {
public:
    GeometryFillSink(
        Base::Vector<Point>& points,
        Base::Vector<FillContour>& contours) noexcept
        : points_(&points), contours_(&contours) {}

    Result<void> BeginFigure(Point start, bool isClosed) noexcept override {
        Result<void> finished = Flush();
        if (!finished) return finished.GetStatus();
        closed_ = isClosed;
        contour_.Clear();
        return contour_.PushBack(start);
    }
    Result<void> AddPoint(Point point) noexcept override {
        if (contour_.Empty()) {
            return BeginFigure(point, closed_);
        }
        return contour_.PushBack(point);
    }
    Result<void> EndFigure(bool isClosed) noexcept override {
        closed_ = isClosed;
        return Flush();
    }
    Result<void> Finish() noexcept { return Flush(); }

private:
    Result<void> Flush() noexcept {
        if (contour_.Size() > 1U &&
            FlattenSinksSamePoint(contour_.Front(), contour_.Back())) {
            contour_.PopBack();
        }
        if (contour_.Size() < 3U) {
            contour_.Clear();
            return {};
        }
        FillContour record{points_->Size(), contour_.Size()};
        Result<void> appended = points_->Append(contour_.AsSpan());
        if (!appended) return appended.GetStatus();
        appended = contours_->PushBack(record);
        contour_.Clear();
        return appended;
    }

    Base::Vector<Point> contour_;
    Base::Vector<Point>* points_ = nullptr;
    Base::Vector<FillContour>* contours_ = nullptr;
    bool closed_ = true;
};

class StrokeContourSink final : public FlattenSink {
public:
    StrokeContourSink(
        Base::Vector<Point>& points,
        Base::Vector<std::uint32_t>& starts,
        Base::Vector<std::uint32_t>& counts,
        Base::Vector<std::uint8_t>& closed) noexcept
        : points_(&points),
          starts_(&starts),
          counts_(&counts),
          closed_(&closed) {}

    Result<void> BeginFigure(Point start, bool isClosed) noexcept override {
        Result<void> finished = Flush(closedFlag_);
        if (!finished) return finished.GetStatus();
        closedFlag_ = isClosed;
        contour_.Clear();
        return contour_.PushBack(start);
    }
    Result<void> AddPoint(Point point) noexcept override {
        if (contour_.Empty()) {
            return BeginFigure(point, closedFlag_);
        }
        return contour_.PushBack(point);
    }
    Result<void> EndFigure(bool isClosed) noexcept override {
        closedFlag_ = isClosed;
        return Flush(isClosed);
    }
    Result<void> Finish() noexcept { return Flush(closedFlag_); }

private:
    Result<void> Flush(bool closed) noexcept {
        if (contour_.Size() < 2U) {
            contour_.Clear();
            return {};
        }
        Result<void> added = starts_->PushBack(points_->Size());
        if (added) added = counts_->PushBack(contour_.Size());
        if (added) added = closed_->PushBack(closed ? std::uint8_t{1} : std::uint8_t{0});
        if (added) added = points_->Append(contour_.AsSpan());
        contour_.Clear();
        return added;
    }

    Base::Vector<Point> contour_;
    Base::Vector<Point>* points_ = nullptr;
    Base::Vector<std::uint32_t>* starts_ = nullptr;
    Base::Vector<std::uint32_t>* counts_ = nullptr;
    Base::Vector<std::uint8_t>* closed_ = nullptr;
    bool closedFlag_ = false;
};

} // namespace Aero::Media
