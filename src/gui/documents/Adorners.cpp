#include <Aero/Documents.hpp>
#include <Aero/TryCast.hpp>
#include "gui/core/State.hpp"

#include <algorithm>
#include <cmath>

namespace Aero::Documents {

Size Adorner::MeasureOverride(Size availableSize) noexcept {
    if (adorned_ != nullptr) {
        const Size render = adorned_->GetRenderSize();
        if (render.width > 0.0 && render.height > 0.0) {
            return render;
        }
        return adorned_->GetDesiredSize();
    }
    return Size{
        std::isfinite(availableSize.width) ? availableSize.width : 0.0,
        std::isfinite(availableSize.height) ? availableSize.height : 0.0};
}

Size Adorner::ArrangeOverride(Size finalSize) noexcept {
    return finalSize;
}

Base::Result<void> AdornerLayer::Add(Base::Ref<Adorner> adorner) noexcept {
    if (!adorner) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument, "Adorner cannot be null");
    }
    for (const Base::Ref<Adorner>& owned : adorners_) {
        if (owned.Get() == adorner.Get()) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists, "Adorner already added");
        }
    }
    Adorner* child = adorner.Get();
    Base::Result<void> appended = adorners_.PushBack(std::move(adorner));
    if (!appended) return appended.GetStatus();
    AddVisualChild(child);
    return InvalidateMeasure();
}

Base::Result<void> AdornerLayer::Remove(Adorner& adorner) noexcept {
    for (std::uint32_t index = 0U; index < adorners_.Size(); ++index) {
        if (adorners_[index].Get() == &adorner) {
            RemoveVisualChild(&adorner);
            for (std::uint32_t shift = index;
                 shift + 1U < adorners_.Size(); ++shift) {
                adorners_[shift] = std::move(adorners_[shift + 1U]);
            }
            adorners_.PopBack();
            return InvalidateMeasure();
        }
    }
    return {};
}

AdornerLayer* AdornerLayer::GetAdornerLayer(UIElement* element) noexcept {
    ::Aero::Media::Visual* current = element;
    while (current != nullptr) {
        if (AdornerLayer* layer = ::Aero::TryCast<AdornerLayer>(current)) {
            return layer;
        }
        if (AdornerDecorator* decorator =
                ::Aero::TryCast<AdornerDecorator>(current)) {
            return decorator->GetAdornerLayer();
        }
        current = current->GetVisualParent();
    }
    return nullptr;
}

std::uint32_t AdornerLayer::GetVisualChildrenCount() const noexcept {
    return adorners_.Size();
}

::Aero::Media::Visual* AdornerLayer::GetVisualChild(
    std::uint32_t index) const noexcept {
    return index < adorners_.Size() ? adorners_[index].Get() : nullptr;
}

Size AdornerLayer::MeasureOverride(Size availableSize) noexcept {
    Size desired{};
    for (const Base::Ref<Adorner>& adorner : adorners_) {
        if (!adorner) continue;
        Base::Result<void> measured = MeasureChild(*adorner, availableSize);
        if (!measured) return Size{};
        const Size child = adorner->GetDesiredSize();
        desired.width = std::max(desired.width, child.width);
        desired.height = std::max(desired.height, child.height);
    }
    return desired;
}

Size AdornerLayer::ArrangeOverride(Size finalSize) noexcept {
    for (const Base::Ref<Adorner>& adorner : adorners_) {
        if (!adorner) continue;
        Point origin{};
        if (UIElement* adorned = adorner->GetAdornedElement()) {
            origin = Point{adorned->GetLayoutSlot().x, adorned->GetLayoutSlot().y};
        }
        const Size size = adorner->GetDesiredSize();
        Base::Result<void> arranged = ArrangeChild(
            *adorner,
            Rect{origin.x, origin.y, size.width, size.height});
        if (!arranged) return finalSize;
    }
    return finalSize;
}

AdornerDecorator::AdornerDecorator() noexcept
    : Controls::Decorator(StaticTypeId()) {
    Base::Result<Base::Ref<AdornerLayer>> created =
        Base::MakeRef<AdornerLayer>();
    if (created) {
        layer_ = std::move(created.Value());
        AddVisualChild(layer_.Get());
    }
}

std::uint32_t AdornerDecorator::GetVisualChildrenCount() const noexcept {
    std::uint32_t count = Controls::Decorator::GetVisualChildrenCount();
    if (layer_ && layer_->GetVisualParent() == this) {
        ++count;
    }
    return count;
}

::Aero::Media::Visual* AdornerDecorator::GetVisualChild(
    std::uint32_t index) const noexcept {
    const std::uint32_t contentCount =
        Controls::Decorator::GetVisualChildrenCount();
    if (index < contentCount) {
        return Controls::Decorator::GetVisualChild(index);
    }
    if (index == contentCount && layer_ && layer_->GetVisualParent() == this) {
        return layer_.Get();
    }
    return nullptr;
}

Size AdornerDecorator::MeasureOverride(Size availableSize) noexcept {
    Size desired = Controls::Decorator::MeasureOverride(availableSize);
    if (layer_) {
        Base::Result<void> measured = MeasureChild(*layer_, availableSize);
        if (!measured) return Size{};
        const Size layerSize = layer_->GetDesiredSize();
        desired.width = std::max(desired.width, layerSize.width);
        desired.height = std::max(desired.height, layerSize.height);
    }
    return desired;
}

Size AdornerDecorator::ArrangeOverride(Size finalSize) noexcept {
    static_cast<void>(Controls::Decorator::ArrangeOverride(finalSize));
    if (layer_) {
        Base::Result<void> arranged = ArrangeChild(
            *layer_, {0.0, 0.0, finalSize.width, finalSize.height});
        if (!arranged) return finalSize;
    }
    return finalSize;
}

} // namespace Aero::Documents
