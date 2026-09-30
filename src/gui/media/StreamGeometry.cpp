#include <Aero/Media/Geometries.hpp>
#include "gui/media/GeometryFlatten.hpp"

#include <algorithm>
#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryDetail.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/MediaActions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/ApplicationCommands.hpp>
#include <Aero/InputGesture.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/Animatable.hpp>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace Aero::Media {
namespace {

class BoundsSink final : public FlattenSink {
public:
    void AddPoint(Point point) noexcept override {
        Include(point);
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
    FlattenCore(sink);
    if (sink.HasBounds()) {
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
    commands_.PushBack(command);
    WritePostscript();
    return {};
}

void StreamGeometry::ReplayCommands(FlattenSink& sink) const noexcept {
    Point current{};
    bool figureOpen = false;
    bool figureClosed = false;
    for (std::uint32_t index = 0U; index < commands_.Size(); ++index) {
        const Command& command = commands_[index];
        switch (command.kind) {
        case CommandKind::BeginFigure: {
            if (figureOpen) {
                sink.EndFigure(figureClosed);
            }
            figureClosed = command.closed;
            sink.BeginFigure(command.p0, command.closed);
            current = command.p0;
            figureOpen = true;
            break;
        }
        case CommandKind::LineTo: {
            sink.AddPoint(command.p0);
            current = command.p0;
            break;
        }
        case CommandKind::BezierTo: {
            FlattenCubicBezier(
                sink, current, command.p0, command.p1, command.p2);
            current = command.p2;
            break;
        }
        case CommandKind::QuadraticBezierTo: {
            FlattenQuadraticBezier(
                sink, current, command.p0, command.p1);
            current = command.p1;
            break;
        }
        case CommandKind::ArcTo: {
            FlattenArc(
                sink,
                current,
                command.size,
                command.rotation,
                command.largeArc,
                command.sweepClockwise,
                command.p0);
            current = command.p0;
            break;
        }
        case CommandKind::Close: {
            if (!figureOpen) break;
            sink.EndFigure(figureClosed);
            figureOpen = false;
            figureClosed = false;
            break;
        }
        }
    }
    if (figureOpen) {
        sink.EndFigure(figureClosed);
    }
}

void StreamGeometry::FlattenCore(FlattenSink& sink) const noexcept {
    if (!commands_.Empty()) {
        ReplayCommands(sink);
        return;
    }
    if (!FlattenPathData(data_.View(), sink)) {
        AERO_ASSERT(false);
    }
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

// Metadata registration for the types implemented in this file.
namespace Aero::MetadataSupport {
using namespace ::Aero::Meta;
using namespace ::Aero::Threading;
using namespace ::Aero::Input;
using namespace ::Aero::Media;
using namespace ::Aero::Data;
using namespace ::Aero::Interactivity;
    using namespace Interactivity;
    using Media::Animation::BeginStoryboard;
    using Media::Animation::BooleanAnimationUsingKeyFrames;
    using Media::Animation::BooleanKeyFrame;
    using Media::Animation::ColorAnimationUsingKeyFrames;
    using Media::Animation::ColorKeyFrame;
    using Media::Animation::DoubleAnimationUsingKeyFrames;
    using Media::Animation::DoubleKeyFrame;
    using Media::Animation::EventTrigger;
    using Media::Animation::Int16AnimationUsingKeyFrames;
    using Media::Animation::Int16KeyFrame;
    using Media::Animation::Int32AnimationUsingKeyFrames;
    using Media::Animation::Int32KeyFrame;
    using Media::Animation::Int64AnimationUsingKeyFrames;
    using Media::Animation::Int64KeyFrame;
    using Media::Animation::MatrixAnimationUsingKeyFrames;
    using Media::Animation::MatrixKeyFrame;
    using Media::Animation::ObjectAnimationUsingKeyFrames;
    using Media::Animation::ObjectKeyFrame;
    using Media::Animation::PointAnimationUsingKeyFrames;
    using Media::Animation::PointKeyFrame;
    using Media::Animation::SizeAnimationUsingKeyFrames;
    using Media::Animation::SizeKeyFrame;
    using Media::Animation::Storyboard;
    using Media::Animation::StoryboardCompletedTrigger;
    using Media::Animation::StringAnimationUsingKeyFrames;
    using Media::Animation::StringKeyFrame;
    using Media::Animation::ThicknessAnimationUsingKeyFrames;
    using Media::Animation::ThicknessKeyFrame;
    using Media::Animation::Timeline;
    using Media::Animation::TimelineGroup;
    using Media::Effect;
    using Media::FontFamily;
    using Media::Geometry;
    using Media::GeometryGroup;
    using Media::PathFigure;
    using Media::PathGeometry;
    using Media::PathSegment;
    using Media::StreamGeometry;
namespace {

Base::Result<Value> ConvertGeometryText(
    TypeId targetType,
    Base::StringView text,
    void*) noexcept {
    const bool streamGeometry =
        targetType == StreamGeometry::StaticTypeId();
    if (targetType != Geometry::StaticTypeId() &&
        !streamGeometry) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Geometry text conversion received an invalid target");
    }
    Base::Result<Base::Ref<StreamGeometry>> made =
        Base::MakeRef<StreamGeometry>();
    if (!made) return made.GetStatus();
    Base::Ref<Geometry> geometry =
        Base::Ref<Geometry>(std::move(made).Value());
    static_cast<StreamGeometry*>(geometry.Get())->SetData(text);
    return Value::FromObject(
        targetType,
        Base::Ref<Base::Object>(
            std::move(geometry)));
}
} // namespace
} // namespace Aero::MetadataSupport

AERO_DESCRIBE(::Aero::Media::StreamGeometry) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    using namespace Media;
    Register<StreamGeometry>(context)
            .Property("Data", &StreamGeometry::GetData, &StreamGeometry::SetData, PropertyFlags::Structural)
            .Content(MakeMemberId(StreamGeometry::StaticTypeId(), MemberKind::Property, "Data"))
            .TextConverter(&::Aero::MetadataSupport::ConvertGeometryText)
            .Factory();
}
