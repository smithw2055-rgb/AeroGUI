#include <Aero/Media/Geometry.hpp>

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

} // namespace Aero::Media
