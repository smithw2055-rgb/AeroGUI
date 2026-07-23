#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/EffectiveValueEngine.hpp>

namespace Aero::Core {

struct StyleSetter final {
    DependencyPropertyHandle property;
    PropertyValue value;
};

// Host-owned immutable style plan. Styles are authored through setters and
// sealed only after DependencyProperty metadata is frozen. BasedOn setters are
// flattened deterministically; a derived style replaces a base setter for the
// same property.
class AERO_API Style final {
public:
    explicit Style(
        TypeId targetType,
        const Style* basedOn = nullptr,
        Base::IAllocator* allocator = nullptr) noexcept;

    Style(const Style&) = delete;
    Style& operator=(const Style&) = delete;

    Base::Result<void> TryAddSetter(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
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

private:
    TypeId targetType_ = InvalidTypeId;
    const Style* basedOn_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<StyleSetter> authored_;
    Base::Vector<StyleSetter> flattened_;
    bool sealed_ = false;
};

// Applies sealed style setters through EffectiveValueEngine, thereby retaining
// the existing precedence contract: local values and local expressions remain
// above Style values, and template/animation layers remain independent.
class AERO_API StyleManager final {
public:
    explicit StyleManager(
        EffectiveValueEngine& values,
        DependencyPropertyRegistry& properties,
        Base::IAllocator* allocator = nullptr) noexcept
        : values_(&values), properties_(&properties),
          applications_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()) {}

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

    Base::Result<void> VerifyTarget(
        const DependencyObject& object,
        const Style& style) const noexcept;
    std::uint32_t FindApplication(
        const DependencyObject& object) const noexcept;
    Base::Result<void> ClearSetters(
        DependencyObject& object,
        const Style& style) noexcept;
};

} // namespace Aero::Core
