#pragma once

#include <Aero/Core/Rendering.hpp>

namespace Aero::Core {

enum class Orientation : std::uint8_t { Horizontal = 0U, Vertical };

// First production Panel: lays out visual/layout children sequentially in a
// single axis. Child ownership and attach/detach remain in ObjectTree and
// LayoutManager; StackPanel owns no child objects.
class AERO_API StackPanel final : public LayoutElement {
public:
    StackPanel(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
        TypeId runtimeType, Orientation orientation = Orientation::Vertical,
        Base::IAllocator* allocator = nullptr) noexcept;

    AERO_NODISCARD Orientation GetOrientation() const noexcept { return orientation_; }
    AERO_NODISCARD Base::Result<void> SetOrientation(Orientation value) noexcept;

protected:
    AERO_NODISCARD Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    AERO_NODISCARD Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;

private:
    Orientation orientation_ = Orientation::Vertical;
};

class AERO_API Canvas final : public LayoutElement {
public:
    Canvas(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
        TypeId runtimeType, Base::IAllocator* allocator = nullptr) noexcept;

    AERO_NODISCARD Base::Result<void> SetChildPosition(
        LayoutElement& child, Point position) noexcept;
    AERO_NODISCARD Point ChildPosition(const LayoutElement& child) const noexcept;

protected:
    AERO_NODISCARD Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    AERO_NODISCARD Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;

private:
    struct Position final { LayoutElement* child = nullptr; Point point; };
    Base::Vector<Position> positions_;
};

class AERO_API Border : public RenderElement {
public:
    Border(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
        TypeId runtimeType, Base::IAllocator* allocator = nullptr) noexcept;
    AERO_NODISCARD Base::Result<void> SetBackground(Color value) noexcept;
    AERO_NODISCARD Base::Result<void> SetStroke(Color value, double thickness) noexcept;

protected:
    AERO_NODISCARD Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    AERO_NODISCARD Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;
    AERO_NODISCARD Base::Result<void> BuildDisplayList(DisplayListBuilder& builder) noexcept override;

private:
    Color background_;
    Color stroke_{0.0F, 0.0F, 0.0F, 0.0F};
    double strokeThickness_ = 0.0;
};

} // namespace Aero::Core
