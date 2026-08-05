#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyObject.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Data.hpp>

namespace Aero { class FrameworkElement; }

namespace Aero::Interactivity {

// Blend-compatible attachable behavior base. A behavior is authored as an
// object, retained by the target element, and attached exactly once when the
// element enters a View. Host controls can derive from this class and subscribe
// to routed input without exposing runtime-specific service objects.
class AERO_API Behavior : public ::Aero::DependencyObject {
    AERO_DECLARE_TYPE(Behavior, ::Aero::DependencyObject)
public:
    struct AuthoredBinding {
        Meta::DependencyPropertyHandle property;
        Base::Ref<Aero::Data::Binding> binding;
    };

    FrameworkElement* GetAssociatedObject() const noexcept {
        return associatedObject_;
    }
    bool GetIsAttached() const noexcept {
        return associatedObject_ != nullptr;
    }
    Base::Result<void> Attach(FrameworkElement& object) noexcept;
    void Detach() noexcept;
    Base::Result<void> AddAuthoredBinding(
        Meta::DependencyPropertyHandle property,
        Base::Ref<Aero::Data::Binding> binding) noexcept;
    Base::Span<const AuthoredBinding> GetAuthoredBindings() const noexcept {
        return authoredBindings_.AsSpan();
    }
    Base::Result<void> CopyAuthoredBindingsTo(
        Behavior& destination) const noexcept;

protected:
    explicit Behavior(Meta::TypeId runtimeType) noexcept
        : DependencyObject(runtimeType) {}
    ~Behavior() override;
    virtual Base::Result<void> OnAttached() noexcept { return {}; }
    virtual void OnDetaching() noexcept {}

private:
    FrameworkElement* associatedObject_ = nullptr;
    Base::Vector<AuthoredBinding> authoredBindings_;
};

class AERO_API StyleBehaviorCollection : public Base::Object {
    AERO_DECLARE_TYPE(StyleBehaviorCollection, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::Result<void> Add(Base::Ref<Base::Object> value) noexcept;
    void Clear() noexcept { items_.Clear(); }
    Base::Span<const Base::Ref<Base::Object>> GetItems() const noexcept {
        return items_.AsSpan();
    }
private:
    Base::Vector<Base::Ref<Base::Object>> items_;
};

class AERO_API StyleTriggerCollection : public Base::Object {
    AERO_DECLARE_TYPE(StyleTriggerCollection, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::Result<void> Add(Base::Ref<Base::Object> value) noexcept;
    void Clear() noexcept { items_.Clear(); }
    Base::Span<const Base::Ref<Base::Object>> GetItems() const noexcept {
        return items_.AsSpan();
    }
private:
    Base::Vector<Base::Ref<Base::Object>> items_;
};

class AERO_API StyleInteraction : public Base::Object {
    AERO_DECLARE_TYPE(StyleInteraction, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    inline static constexpr Members::AttachedProperty<Base::Ref<StyleBehaviorCollection>> BehaviorsProperty{"Behaviors"};
    inline static constexpr Members::AttachedProperty<Base::Ref<StyleTriggerCollection>> TriggersProperty{"Triggers"};

    static void OnBehaviorsChanged(
        DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs& args) noexcept;
    static void OnTriggersChanged(
        DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs& args) noexcept;
};

} // namespace Aero::Interactivity
