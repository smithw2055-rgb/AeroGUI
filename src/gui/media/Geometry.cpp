#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/StreamGeometry.hpp>
#include <Aero/Media/PathFigure.hpp>
#include <Aero/Media/PathGeometry.hpp>
#include <Aero/Media/LineSegment.hpp>
#include <Aero/Media/PathSegment.hpp>

#include <cstdio>
#include <utility>

namespace Aero::Media {

Geometry::~Geometry() {
    if (transform_ && !transform_->IsFrozen() &&
        !transformChangedHandler_.Empty()) {
        static_cast<void>(transform_->RemoveChangedHandler(
            transformChangedHandler_));
    }
}

void Geometry::SetTransform(Base::Ref<Transform> value) noexcept {
    if (!WritePreamble() || transform_.Get() == value.Get()) return;
    if (transformChangedHandler_.Empty()) {
        transformChangedHandler_ = FreezableChangedHandler(
            this, &Geometry::OnTransformChanged);
    }
    Transform* next = value.Get();
    if (next != nullptr && !next->IsFrozen()) {
        Base::Result<void> subscribed =
            next->AddChangedHandlerChecked(transformChangedHandler_);
        if (!subscribed) return;
    }
    Base::Ref<Transform> previous = std::move(transform_);
    transform_ = std::move(value);
    if (previous && !previous->IsFrozen()) {
        static_cast<void>(previous->RemoveChangedHandler(
            transformChangedHandler_));
    }
    WritePostscript();
}

void Geometry::OnTransformChanged(Freezable&) noexcept {
    WritePostscript();
}

bool Geometry::FreezeCore(bool isChecking) noexcept {
    if (transform_) {
        if (isChecking) {
            if (!transform_->CanFreeze()) return false;
        } else {
            static_cast<void>(transform_->Freeze());
        }
    }
    return Freezable::FreezeCore(isChecking);
}

Base::Result<void> PathFigure::AddSegment(
    Base::Ref<PathSegment> value) noexcept {
    Base::Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "PathFigure segment cannot be null");
    }
    Base::Result<void> added = segments_.PushBack(std::move(value));
    if (added) WritePostscript();
    return added;
}

Base::Result<void> PathGeometry::AddFigure(
    Base::Ref<PathFigure> value) noexcept {
    Base::Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "PathGeometry figure cannot be null");
    }
    Base::Result<void> added = figures_.PushBack(std::move(value));
    if (added) WritePostscript();
    return added;
}

namespace {
Base::Result<void> AppendPoint(
    Base::String& output,
    char command,
    Base::Point point) noexcept {
    char text[96]{};
    const int length = std::snprintf(
        text, sizeof(text), "%c%.17g,%.17g", command, point.x, point.y);
    if (length <= 0 || static_cast<std::size_t>(length) >= sizeof(text)) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "PathGeometry coordinate text is too large");
    }
    return output.Append(Base::StringView(
        text, static_cast<std::uint32_t>(length)));
}
} // namespace

Base::Result<Base::String> PathGeometry::ToStreamData() const noexcept {
    Base::String result;
    for (const Base::Ref<PathFigure>& figure : figures_) {
        if (!figure) continue;
        Base::Result<void> appended =
            AppendPoint(result, 'M', figure->GetStartPoint());
        if (!appended) return appended.GetStatus();
        for (const Base::Ref<PathSegment>& segment : figure->GetSegments()) {
            if (!segment || segment->RuntimeType() != LineSegment::StaticTypeId()) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "PathGeometry currently supports LineSegment content");
            }
            appended = AppendPoint(
                result, 'L',
                static_cast<const LineSegment&>(*segment).GetPoint());
            if (!appended) return appended.GetStatus();
        }
        if (figure->GetIsClosed()) {
            appended = result.Append(Base::StringView("Z"));
            if (!appended) return appended.GetStatus();
        }
    }
    return result;
}

} // namespace Aero::Media
