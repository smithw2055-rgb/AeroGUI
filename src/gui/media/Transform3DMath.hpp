#pragma once

// Shared 2.5D helpers for render collapse and hit-test unproject.
// GetLocalVisualTransform stays O(1) / local-only; 3D lives in this context.

#include <Aero/Media/CompositeTransform3D.hpp>
#include <Aero/Media/MatrixTransform3D.hpp>
#include <Aero/Media/PerspectiveTransform3D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/TryCast.hpp>

#include <cmath>

namespace Aero::Media {

inline void OpenPerspective(
    Transform3DContext& ctx,
    double depth,
    Base::Point offset,
    Base::Size renderSize) noexcept {
    ctx.active = true;
    ctx.depth = std::abs(depth) > 0.0 ? depth : Base::DefaultPerspectiveDepth;
    ctx.offset = offset;
    ctx.center = {
        renderSize.width * 0.5 + offset.x,
        renderSize.height * 0.5 + offset.y};
    ctx.accumulated = Base::IdentityTransform3();
}

inline Transform3DContext MakeImplicitViewRootContext(
    Base::Size renderSize) noexcept {
    Transform3DContext ctx;
    OpenPerspective(
        ctx,
        Base::DefaultPerspectiveDepth,
        {},
        renderSize);
    return ctx;
}

inline Base::Transform3 LiftLocalVisual(
    const Base::ProjectiveTransform2D& localVisual) noexcept {
    Base::Transform2D affine;
    if (Base::TryToTransform2D(localVisual, affine)) {
        return Base::ToTransform3(affine);
    }
    return Base::IdentityTransform3();
}

/// Advance 3D state from parent to this element.
/// PerspectiveTransform3D opens/replaces the camera (identity model).
/// Composite/Matrix multiply their Transform3 plus 2D local visual and slot.
inline Transform3DContext AdvanceTransform3DContext(
    Transform3DContext incoming,
    Transform3D* transform,
    const Base::Transform3& localVisual3,
    const Base::Rect& layoutSlot,
    Base::Size renderSize,
    bool overlayRoot) noexcept {
    const PerspectiveTransform3D* perspective =
        TryCast<PerspectiveTransform3D>(transform);
    if (perspective != nullptr) {
        OpenPerspective(
            incoming,
            perspective->GetDepth(),
            {perspective->GetOffsetX(), perspective->GetOffsetY()},
            renderSize);
        return incoming;
    }

    Base::Transform3 model = Base::IdentityTransform3();
    if (transform != nullptr) {
        model = transform->GetTransform3D();
    }
    Base::Transform3 local = Base::Compose(localVisual3, model);
    const double slotX = overlayRoot ? 0.0 : layoutSlot.x;
    const double slotY = overlayRoot ? 0.0 : layoutSlot.y;
    local = Base::Compose(local, Base::MakeTranslate3(slotX, slotY));
    incoming.accumulated = Base::Compose(local, incoming.accumulated);
    return incoming;
}

inline Base::ProjectiveTransform2D MakeTranslateProjective(
    double x,
    double y) noexcept {
    return Base::ToProjective(Base::Transform2D{1.0, 0.0, 0.0, 1.0, x, y});
}

/// Relative render transform stored on the snapshot so FrameEncoder
/// `Combine(renderTransform, Translate(slot)) * parent` equals the collapsed
/// world mapping. Fast path: Z=0 plane unchanged → existing affine local visual.
inline Base::ProjectiveTransform2D CollapseRelativeToParent(
    const Transform3DContext& parent,
    const Transform3DContext& child,
    const Base::Rect& layoutSlot,
    const Base::ProjectiveTransform2D& localVisual,
    bool overlayRoot) noexcept {
    if (overlayRoot) {
        return localVisual;
    }
    if (Base::LeavesZ0PlaneUnchanged(parent.accumulated) &&
        Base::LeavesZ0PlaneUnchanged(child.accumulated)) {
        return localVisual;
    }
    const double depth = child.depth;
    const Base::Point center = child.center;
    const Base::ProjectiveTransform2D childWorld =
        Base::CollapsePerspective(child.accumulated, depth, center);
    const Base::ProjectiveTransform2D parentWorld =
        Base::CollapsePerspective(parent.accumulated, depth, center);
    Base::ProjectiveTransform2D parentInv;
    if (!Base::Invert(parentWorld, parentInv)) {
        return localVisual;
    }
    Base::ProjectiveTransform2D slotInv;
    if (!Base::Invert(
            MakeTranslateProjective(layoutSlot.x, layoutSlot.y),
            slotInv)) {
        return localVisual;
    }
    return Base::Compose(Base::Compose(childWorld, parentInv), slotInv);
}

inline Base::ProjectiveTransform2D CollapseContext(
    const Transform3DContext& ctx) noexcept {
    return Base::CollapsePerspective(ctx.accumulated, ctx.depth, ctx.center);
}

/// Unproject a parent-local 2D point into child-local via the same collapsed
/// mappings render uses. Degenerate w skips the hit (no NaN).
inline bool UnprojectParentToLocal(
    const Transform3DContext& parent,
    const Transform3DContext& child,
    Base::Point parentPosition,
    Base::Point& localPosition) noexcept {
    Base::Point world{};
    if (!Base::TryTransformPoint(
            CollapseContext(parent), parentPosition, world)) {
        return false;
    }
    return Base::TryUnprojectPointToLocalPlane(
        CollapseContext(child), world, localPosition);
}

} // namespace Aero::Media
