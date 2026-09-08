// Shared contour-recording FlattenSink implementations (P4.4).
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

    void BeginFigure(Point start, bool isClosed) noexcept override {
        Flush();
        closed_ = isClosed;
        contour_.Clear();
        contour_.PushBack(start);
    }
    void AddPoint(Point point) noexcept override {
        if (contour_.Empty()) {
            BeginFigure(point, closed_);
            return;
        }
        contour_.PushBack(point);
    }
    void EndFigure(bool isClosed) noexcept override {
        closed_ = isClosed;
        Flush();
    }
    void Finish() noexcept { Flush(); }

private:
    void Flush() noexcept {
        if (contour_.Size() > 1U &&
            FlattenSinksSamePoint(contour_.Front(), contour_.Back())) {
            contour_.PopBack();
        }
        if (contour_.Size() < 3U) {
            contour_.Clear();
            return;
        }
        FillContour record{points_->Size(), contour_.Size()};
        points_->Append(contour_.AsSpan());
        contours_->PushBack(record);
        contour_.Clear();
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

    void BeginFigure(Point start, bool isClosed) noexcept override {
        Flush(closedFlag_);
        closedFlag_ = isClosed;
        contour_.Clear();
        contour_.PushBack(start);
    }
    void AddPoint(Point point) noexcept override {
        if (contour_.Empty()) {
            BeginFigure(point, closedFlag_);
            return;
        }
        contour_.PushBack(point);
    }
    void EndFigure(bool isClosed) noexcept override {
        closedFlag_ = isClosed;
        Flush(isClosed);
    }
    void Finish() noexcept { Flush(closedFlag_); }

private:
    void Flush(bool closed) noexcept {
        if (contour_.Size() < 2U) {
            contour_.Clear();
            return;
        }
        starts_->PushBack(points_->Size());
        counts_->PushBack(contour_.Size());
        closed_->PushBack(closed ? std::uint8_t{1} : std::uint8_t{0});
        points_->Append(contour_.AsSpan());
        contour_.Clear();
    }

    Base::Vector<Point> contour_;
    Base::Vector<Point>* points_ = nullptr;
    Base::Vector<std::uint32_t>* starts_ = nullptr;
    Base::Vector<std::uint32_t>* counts_ = nullptr;
    Base::Vector<std::uint8_t>* closed_ = nullptr;
    bool closedFlag_ = false;
};

} // namespace Aero::Media
