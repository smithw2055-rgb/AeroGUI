#pragma once

#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Transforms.hpp>

namespace Aero::Interactivity {

// Microsoft.Xaml.Behaviors compatible pointer-drag behavior. The authored X/Y
// values are applied through a dedicated TranslateTransform so an existing
// RenderTransform remains intact. Pointer capture keeps dragging stable when
// the pointer leaves the element.
class AERO_GUI_API MouseDragElementBehavior : public Behavior {
    AERO_DECLARE_TYPE(MouseDragElementBehavior, Behavior)
public:
    MouseDragElementBehavior() noexcept;
    ~MouseDragElementBehavior() override = default;

    double GetX() const noexcept { return GetValueOr(XProperty, 0.0); }
    double GetY() const noexcept { return GetValueOr(YProperty, 0.0); }
    bool GetConstrainToParentBounds() const noexcept {
        return GetValueOr(ConstrainToParentBoundsProperty, false);
    }
    void SetX(double value) noexcept { SetValue(XProperty, value); }
    void SetY(double value) noexcept { SetValue(YProperty, value); }
    void SetConstrainToParentBounds(bool value) noexcept {
        SetValue(ConstrainToParentBoundsProperty, value);
    }

    inline static constexpr DependencyProperty<double> XProperty{"X"};
    inline static constexpr DependencyProperty<double> YProperty{"Y"};
    inline static constexpr DependencyProperty<bool> ConstrainToParentBoundsProperty{"ConstrainToParentBounds"};

    static void OnPositionChanged(
        DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs& args) noexcept;

protected:
    Result<void> OnAttached() noexcept override;
    void OnDetaching() noexcept override;

private:
    MouseButtonEventHandler mouseDownHandler_;
    MouseEventHandler mouseMoveHandler_;
    MouseButtonEventHandler mouseUpHandler_;
    Ref<Media::Transform> originalTransform_;
    Ref<Media::TransformGroup> transformGroup_;
    Ref<Media::TranslateTransform> translation_;
    Base::Point dragStartRoot_;
    double dragStartX_ = 0.0;
    double dragStartY_ = 0.0;
    std::uint32_t pointerId_ = UINT32_MAX;
    bool dragging_ = false;
    bool updatingPosition_ = false;

    void OnMouseDown(Base::Object*, MouseButtonEventArgs& args) noexcept;
    void OnMouseMove(Base::Object*, MouseEventArgs& args) noexcept;
    void OnMouseUp(Base::Object*, MouseButtonEventArgs& args) noexcept;
    void SynchronizeTransform() noexcept;
    Base::Point RootPosition(Base::Point local) const noexcept;
};

// Aero.GUI.Extensions background-effect compatibility behavior. It projects
// the Source element's Background brush into the associated shape and applies
// the authored Effect. Image backgrounds are cropped to the associated
// element's location after layout so the blur represents the same backdrop
// region instead of independently fitting the image into each shape.
class AERO_GUI_API BackgroundEffectBehavior : public Behavior {
    AERO_DECLARE_TYPE(BackgroundEffectBehavior, Behavior)
public:
    BackgroundEffectBehavior() noexcept : Behavior(StaticTypeId()) {}
    ~BackgroundEffectBehavior() override = default;

    Ref<FrameworkElement> GetSource() const noexcept;
    void SetSource(Ref<FrameworkElement> value) noexcept {
        SetValue(
            SourceProperty,
            Ref<Base::Object>(std::move(value)));
    }
    Ref<Media::Effect> GetEffect() const noexcept {
        return GetValueOr(EffectProperty, Ref<Media::Effect>{});
    }
    void SetEffect(Ref<Media::Effect> value) noexcept {
        SetValue(EffectProperty, std::move(value));
    }

    inline static constexpr DependencyProperty<Ref<Base::Object>> SourceProperty{"Source"};
    inline static constexpr DependencyProperty<Ref<Media::Effect>> EffectProperty{"Effect"};

    static void OnBehaviorPropertyChanged(
        DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs& args) noexcept;

protected:
    Result<void> OnAttached() noexcept override;
    void OnDetaching() noexcept override;
    void OnLayoutUpdated() noexcept override;

private:
    Ref<Media::Brush> originalFill_;
    Ref<Media::Effect> originalEffect_;
    Ref<Media::ImageBrush> projectedImage_;
    bool updating_ = false;

    Result<void> Refresh() noexcept;
};

} // namespace Aero::Interactivity
