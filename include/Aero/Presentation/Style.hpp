#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>

namespace Aero::Presentation {

using namespace Aero::Core;

struct StyleSetter final {
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct StyleTriggerSetter final {
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct StylePropertyTrigger final {
    DependencyPropertyHandle property;
    PropertyValue value;
    Base::Vector<StyleTriggerSetter> setters;
};

// Host-owned immutable style plan. Styles are authored through setters and
// sealed only after DependencyProperty metadata is frozen. BasedOn setters are
// flattened deterministically; a derived style replaces a base setter for the
// same property.
class AERO_API Style final {
public:
    explicit Style(
        TypeId targetType,
        const Style* basedOn = nullptr) noexcept;

    Style(const Style&) = delete;
    Style& operator=(const Style&) = delete;

    Base::Result<void> TryAddSetter(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    Base::Result<void> TryAddPropertyTrigger(
        StylePropertyTrigger trigger) noexcept;
    // Builder configuration is intentionally available only before Seal().
    // XAML object construction supplies TargetType and BasedOn as members,
    // whereas native callers commonly provide both to the constructor.
    Base::Result<void> TrySetTargetType(
        TypeId targetType) noexcept;
    Base::Result<void> TrySetBasedOn(
        const Style* basedOn) noexcept;
    Base::Result<void> Seal(
        const DependencyPropertyRegistry& properties) noexcept;

    TypeId TargetType() const noexcept { return targetType_; }
    const Style* BasedOn() const noexcept { return basedOn_; }
    bool IsSealed() const noexcept { return sealed_; }
    Base::Span<const StyleSetter> Setters() const noexcept {
        return {flattened_.Data(), flattened_.Size()};
    }
    Base::Span<const StylePropertyTrigger> Triggers() const noexcept {
        return {
            flattenedTriggers_.Data(),
            flattenedTriggers_.Size()};
    }

private:
    TypeId targetType_ = InvalidTypeId;
    const Style* basedOn_ = nullptr;
    Base::Vector<StyleSetter> authored_;
    Base::Vector<StyleSetter> flattened_;
    Base::Vector<StylePropertyTrigger> authoredTriggers_;
    Base::Vector<StylePropertyTrigger> flattenedTriggers_;
    bool sealed_ = false;
};

// Applies sealed style setters through EffectiveValueEngine, thereby retaining
// the existing precedence contract: local values and local expressions remain
// above Style values, and template/animation layers remain independent.
class AERO_API StyleManager final {
public:
    explicit StyleManager(
        EffectiveValueEngine& values,
        DependencyPropertyRegistry& properties) noexcept
        : values_(&values), properties_(&properties),
          applications_(),
          propertyChangedHandler_(
              this, &StyleManager::OnPropertyChanged) {}

    Base::Result<void> Apply(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> Clear(
        DependencyObject& object,
        const Style& style) noexcept;
    // Tree/object ownership code calls this before destroying an object.
    Base::Result<bool> DetachObject(
        DependencyObject& object) noexcept;

private:
    EffectiveValueEngine* values_ = nullptr;
    DependencyPropertyRegistry* properties_ = nullptr;
    struct Application final {
        DependencyObject* object = nullptr;
        const Style* style = nullptr;
    };
    Base::Vector<Application> applications_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;

    Base::Result<void> VerifyTarget(
        const DependencyObject& object,
        const Style& style) const noexcept;
    std::uint32_t FindApplication(
        const DependencyObject& object) const noexcept;
    Base::Result<void> ClearSetters(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> SubscribeTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
    void UnsubscribeTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> EvaluateTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> ClearTriggerSetters(
        DependencyObject& object,
        const Style& style) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
};

// Type-keyed default styles are resolved through the registered base-type
// chain and occupy the ThemeStyle provider below explicit Style values.
class AERO_API ThemeStyleRegistry final {
public:
    explicit ThemeStyleRegistry(
        const DependencyPropertyRegistry& properties) noexcept
        : properties_(&properties) {}

    Base::Result<void> TryRegister(
        TypeId controlType,
        const Style& style) noexcept;
    const Style* Find(TypeId controlType) const noexcept;

private:
    struct Entry final {
        TypeId controlType = InvalidTypeId;
        const Style* style = nullptr;
    };
    const DependencyPropertyRegistry* properties_ = nullptr;
    Base::Vector<Entry> entries_;
};

class AERO_API ThemeStyleManager final {
public:
    ThemeStyleManager(
        EffectiveValueEngine& values,
        const ThemeStyleRegistry& registry) noexcept
        : values_(&values), registry_(&registry) {}

    Base::Result<bool> ApplyDefault(
        DependencyObject& object) noexcept;
    Base::Result<bool> Clear(
        DependencyObject& object) noexcept;

private:
    struct Application final {
        DependencyObject* object = nullptr;
        const Style* style = nullptr;
    };
    EffectiveValueEngine* values_ = nullptr;
    const ThemeStyleRegistry* registry_ = nullptr;
    Base::Vector<Application> applications_;

    std::uint32_t FindApplication(
        const DependencyObject& object) const noexcept;
    Base::Result<void> ClearSetters(
        DependencyObject& object,
        const Style& style) noexcept;
};

} // namespace Aero::Presentation
