#include <Aero/Media/StreamGeometry.hpp>
#include "gui/media/GeometryFlatten.hpp"

#include <algorithm>

namespace Aero::Media {
namespace {

class BoundsSink final : public FlattenSink {
public:
    Result<void> AddPoint(Point point) noexcept override {
        Include(point);
        return {};
    }
    Rect Bounds() const noexcept { return bounds_; }
    bool HasBounds() const noexcept { return hasBounds_; }
private:
    void Include(Point point) noexcept {
        if (!hasBounds_) {
            bounds_ = {point.x, point.y, 0.0, 0.0};
            hasBounds_ = true;
            return;
        }
        const double right = std::max(bounds_.x + bounds_.width, point.x);
        const double bottom = std::max(bounds_.y + bounds_.height, point.y);
        bounds_.x = std::min(bounds_.x, point.x);
        bounds_.y = std::min(bounds_.y, point.y);
        bounds_.width = right - bounds_.x;
        bounds_.height = bottom - bounds_.y;
    }
    Rect bounds_{};
    bool hasBounds_ = false;
};

} // namespace

void StreamGeometry::SetData(StringView value) noexcept {
    if (!WritePreamble() || data_.View() == value) return;
    commands_.Clear();
    InvalidateBounds();
    if (data_.Assign(value)) WritePostscript();
}

Rect StreamGeometry::GetBounds() const noexcept {
    if (boundsValid_) return bounds_;
    BoundsSink sink;
    if (FlattenCore(sink) && sink.HasBounds()) {
        bounds_ = sink.Bounds();
        boundsValid_ = true;
    }
    return bounds_;
}

StreamGeometryContext StreamGeometry::Open() noexcept {
    StreamGeometryContext context;
    if (!WritePreamble()) return context;
    data_.Clear();
    commands_.Clear();
    InvalidateBounds();
    WritePostscript();
    return StreamGeometryContext(this);
}

Result<void> StreamGeometry::AppendCommand(const Command& command) noexcept {
    Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    data_.Clear();
    InvalidateBounds();
    Result<void> added = commands_.PushBack(command);
    if (added) WritePostscript();
    return added;
}

Result<void> StreamGeometry::ReplayCommands(FlattenSink& sink) const noexcept {
    Point current{};
    bool figureOpen = false;
    bool figureClosed = false;
    for (std::uint32_t index = 0U; index < commands_.Size(); ++index) {
        const Command& command = commands_[index];
        switch (command.kind) {
        case CommandKind::BeginFigure: {
            if (figureOpen) {
                Result<void> ended = sink.EndFigure(figureClosed);
                if (!ended) return ended.GetStatus();
            }
            figureClosed = command.closed;
            Result<void> started =
                sink.BeginFigure(command.p0, command.closed);
            if (!started) return started.GetStatus();
            current = command.p0;
            figureOpen = true;
            break;
        }
        case CommandKind::LineTo: {
            Result<void> added = sink.AddPoint(command.p0);
            if (!added) return added.GetStatus();
            current = command.p0;
            break;
        }
        case CommandKind::BezierTo: {
            Result<void> flattened = FlattenCubicBezier(
                sink, current, command.p0, command.p1, command.p2);
            if (!flattened) return flattened.GetStatus();
            current = command.p2;
            break;
        }
        case CommandKind::QuadraticBezierTo: {
            Result<void> flattened = FlattenQuadraticBezier(
                sink, current, command.p0, command.p1);
            if (!flattened) return flattened.GetStatus();
            current = command.p1;
            break;
        }
        case CommandKind::ArcTo: {
            Result<void> flattened = FlattenArc(
                sink,
                current,
                command.size,
                command.rotation,
                command.largeArc,
                command.sweepClockwise,
                command.p0);
            if (!flattened) return flattened.GetStatus();
            current = command.p0;
            break;
        }
        case CommandKind::Close: {
            if (!figureOpen) break;
            Result<void> ended = sink.EndFigure(figureClosed);
            if (!ended) return ended.GetStatus();
            figureOpen = false;
            figureClosed = false;
            break;
        }
        }
    }
    if (figureOpen) {
        return sink.EndFigure(figureClosed);
    }
    return {};
}

Result<void> StreamGeometry::FlattenCore(FlattenSink& sink) const noexcept {
    if (!commands_.Empty()) {
        return ReplayCommands(sink);
    }
    return FlattenPathData(data_.View(), sink);
}

StreamGeometryContext::StreamGeometryContext(StreamGeometry* owner) noexcept
    : owner_(owner), closed_(false) {}

StreamGeometryContext::StreamGeometryContext(
    StreamGeometryContext&& other) noexcept
    : owner_(other.owner_), closed_(other.closed_) {
    other.owner_ = nullptr;
    other.closed_ = true;
}

StreamGeometryContext& StreamGeometryContext::operator=(
    StreamGeometryContext&& other) noexcept {
    if (this == &other) return *this;
    if (!closed_) {
        static_cast<void>(Close());
    }
    owner_ = other.owner_;
    closed_ = other.closed_;
    other.owner_ = nullptr;
    other.closed_ = true;
    return *this;
}

StreamGeometryContext::~StreamGeometryContext() {
    if (!closed_) {
        static_cast<void>(Close());
    }
}

Result<void> StreamGeometryContext::BeginFigure(
    Point startPoint,
    bool isFilled,
    bool isClosed) noexcept {
    if (owner_ == nullptr || closed_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "StreamGeometryContext is not open");
    }
    StreamGeometry::Command command;
    command.kind = StreamGeometry::CommandKind::BeginFigure;
    command.p0 = startPoint;
    command.filled = isFilled;
    command.closed = isClosed;
    return owner_->AppendCommand(command);
}

Result<void> StreamGeometryContext::LineTo(
    Point point,
    bool isStroked,
    bool isSmoothJoin) noexcept {
    if (owner_ == nullptr || closed_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "StreamGeometryContext is not open");
    }
    StreamGeometry::Command command;
    command.kind = StreamGeometry::CommandKind::LineTo;
    command.p0 = point;
    command.isStroked = isStroked;
    command.isSmoothJoin = isSmoothJoin;
    return owner_->AppendCommand(command);
}

Result<void> StreamGeometryContext::BezierTo(
    Point controlPoint1,
    Point controlPoint2,
    Point endPoint,
    bool isStroked,
    bool isSmoothJoin) noexcept {
    if (owner_ == nullptr || closed_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "StreamGeometryContext is not open");
    }
    StreamGeometry::Command command;
    command.kind = StreamGeometry::CommandKind::BezierTo;
    command.p0 = controlPoint1;
    command.p1 = controlPoint2;
    command.p2 = endPoint;
    command.isStroked = isStroked;
    command.isSmoothJoin = isSmoothJoin;
    return owner_->AppendCommand(command);
}

Result<void> StreamGeometryContext::QuadraticBezierTo(
    Point controlPoint,
    Point endPoint,
    bool isStroked,
    bool isSmoothJoin) noexcept {
    if (owner_ == nullptr || closed_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "StreamGeometryContext is not open");
    }
    StreamGeometry::Command command;
    command.kind = StreamGeometry::CommandKind::QuadraticBezierTo;
    command.p0 = controlPoint;
    command.p1 = endPoint;
    command.isStroked = isStroked;
    command.isSmoothJoin = isSmoothJoin;
    return owner_->AppendCommand(command);
}

Result<void> StreamGeometryContext::ArcTo(
    Point point,
    Size size,
    double rotationAngle,
    bool isLargeArc,
    SweepDirection sweepDirection,
    bool isStroked,
    bool isSmoothJoin) noexcept {
    if (owner_ == nullptr || closed_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "StreamGeometryContext is not open");
    }
    StreamGeometry::Command command;
    command.kind = StreamGeometry::CommandKind::ArcTo;
    command.p0 = point;
    command.size = size;
    command.rotation = rotationAngle;
    command.largeArc = isLargeArc;
    command.sweepClockwise =
        sweepDirection == SweepDirection::Clockwise;
    command.isStroked = isStroked;
    command.isSmoothJoin = isSmoothJoin;
    return owner_->AppendCommand(command);
}

Result<void> StreamGeometryContext::PolyLineTo(
    const Point* points,
    std::uint32_t count,
    bool isStroked,
    bool isSmoothJoin) noexcept {
    if (points == nullptr && count != 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "StreamGeometryContext PolyLineTo requires a point buffer");
    }
    for (std::uint32_t index = 0U; index < count; ++index) {
        Result<void> added = LineTo(points[index], isStroked, isSmoothJoin);
        if (!added) return added.GetStatus();
    }
    return {};
}

Result<void> StreamGeometryContext::Close() noexcept {
    if (owner_ == nullptr || closed_) {
        closed_ = true;
        owner_ = nullptr;
        return {};
    }
    StreamGeometry::Command command;
    command.kind = StreamGeometry::CommandKind::Close;
    Result<void> added = owner_->AppendCommand(command);
    closed_ = true;
    owner_ = nullptr;
    return added;
}

} // namespace Aero::Media
