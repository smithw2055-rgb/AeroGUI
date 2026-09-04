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

inline bool InvertAffine2D(
    const Base::Transform2D& value,
    Base::Transform2D& inverse) noexcept {
    const double det = value.m11 * value.m22 - value.m12 * value.m21;
    if (std::abs(det) <= 1.0e-18 || !std::isfinite(det)) {
        return false;
    }
    inverse.m11 = value.m22 / det;
    inverse.m12 = -value.m12 / det;
    inverse.m21 = -value.m21 / det;
    inverse.m22 = value.m11 / det;
    inverse.dx = -(value.dx * inverse.m11 + value.dy * inverse.m21);
    inverse.dy = -(value.dx * inverse.m12 + value.dy * inverse.m22);
    return true;
}

// Viewbox stretch lives on the Viewbox (AeroGUI wrapper Decorator). If an
// older path still stored the scale on the child, 3D must happen in unscaled
// local space (CompositeTransform3D CenterX/Y) and the Viewbox scale wraps
// that result; otherwise a 90° RotationY collapses around the pre-scale
// origin and the board disappears instead of flipping.
inline void LiftLocalVisualWithViewbox(
    const Base::ProjectiveTransform2D& localVisual,
    const Base::Transform2D* viewbox,
    Base::Transform3& inner,
    Base::Transform3& outer) noexcept {
    inner = LiftLocalVisual(localVisual);
    outer = Base::IdentityTransform3();
    if (viewbox == nullptr) return;
    Base::Transform2D affine;
    Base::Transform2D inverse;
    if (!Base::TryToTransform2D(localVisual, affine) ||
        !InvertAffine2D(*viewbox, inverse)) {
        return;
    }
    outer = Base::ToTransform3(*viewbox);
    Base::Transform2D stripped;
    stripped.m11 = affine.m11 * inverse.m11 + affine.m12 * inverse.m21;
    stripped.m12 = affine.m11 * inverse.m12 + affine.m12 * inverse.m22;
    stripped.m21 = affine.m21 * inverse.m11 + affine.m22 * inverse.m21;
    stripped.m22 = affine.m21 * inverse.m12 + affine.m22 * inverse.m22;
    stripped.dx =
        affine.dx * inverse.m11 + affine.dy * inverse.m21 + inverse.dx;
    stripped.dy =
        affine.dx * inverse.m12 + affine.dy * inverse.m22 + inverse.dy;
    inner = Base::ToTransform3(stripped);
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
    bool overlayRoot,
    const Base::Transform3& outerVisual3 = Base::IdentityTransform3()) noexcept {
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
    if (!Base::IsIdentityTransform3(outerVisual3)) {
        local = Base::Compose(local, outerVisual3);
    }
    const double slotX = overlayRoot ? 0.0 : layoutSlot.x;
    const double slotY = overlayRoot ? 0.0 : layoutSlot.y;
    local = Base::Compose(local, Base::MakeTranslate3(slotX, slotY));
    incoming.accumulated = Base::Compose(local, incoming.accumulated);
    return incoming;
}

inline Base::ProjectiveTransform2D ProjectiveFromAffine3(
    const Base::Transform3& value) noexcept {
    Base::Transform2D affine;
    affine.m11 = value.m11;
    affine.m12 = value.m12;
    affine.m21 = value.m21;
    affine.m22 = value.m22;
    affine.dx = value.dx;
    affine.dy = value.dy;
    return Base::ToProjective(affine);
}

/// Collapse this element's own CompositeTransform3D in local pixels, then wrap
/// with the Viewbox scale. Used when the accumulated 3D chain would hide a
/// RotationY card-flip (Viewbox scale on the child, window-sized camera).
inline Base::ProjectiveTransform2D CollapseLocalTransform3D(
    Transform3D* transform,
    Base::Size renderSize,
    const Base::Transform3& innerVisual,
    const Base::Transform3& outerVisual,
    double depth) noexcept {
    if (transform == nullptr ||
        TryCast<PerspectiveTransform3D>(transform) != nullptr) {
        return Base::IdentityProjective();
    }
    const Base::Transform3 model = transform->GetTransform3D();
    if (Base::LeavesZ0PlaneUnchanged(model)) {
        return Base::IdentityProjective();
    }
    Base::Point center{
        renderSize.width * 0.5,
        renderSize.height * 0.5};
    if (const CompositeTransform3D* composite =
            TryCast<CompositeTransform3D>(transform)) {
        const double cx = composite->GetCenterX();
        const double cy = composite->GetCenterY();
        if (std::abs(cx) > 1.0e-9 || std::abs(cy) > 1.0e-9) {
            center = {cx, cy};
        }
    }
    const double camera =
        std::abs(depth) > 0.0 ? depth : Base::DefaultPerspectiveDepth;
    const Base::ProjectiveTransform2D collapsed =
        Base::CollapsePerspective(model, camera, center);
    Base::ProjectiveTransform2D result = Base::Compose(
        ProjectiveFromAffine3(innerVisual), collapsed);
    if (!Base::IsIdentityTransform3(outerVisual)) {
        result = Base::Compose(result, ProjectiveFromAffine3(outerVisual));
    }
    return result;
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
    // Match CollapseRelativeToParent: collapse both worlds with the child's
    // camera so hit-testing inverts the same perspective the renderer uses.
    const double depth = child.depth;
    const Base::Point center = child.center;
    const Base::ProjectiveTransform2D parentWorld =
        Base::CollapsePerspective(parent.accumulated, depth, center);
    const Base::ProjectiveTransform2D childWorld =
        Base::CollapsePerspective(child.accumulated, depth, center);
    Base::Point world{};
    if (!Base::TryTransformPoint(parentWorld, parentPosition, world)) {
        return false;
    }
    return Base::TryUnprojectPointToLocalPlane(
        childWorld, world, localPosition);
}

} // namespace Aero::Media
