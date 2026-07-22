#include <Aero/Core/Style.hpp>

namespace Aero::Core {
namespace {

Base::Result<void> InvalidStyle(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

bool IsTargetCompatible(
    const TypeRegistry& types,
    TypeId derived,
    TypeId expectedBase) noexcept {
    return derived == expectedBase || types.IsDerivedFrom(derived, expectedBase);
}

} // namespace

Style::Style(
    TypeId targetType,
    const Style* basedOn,
    Base::IAllocator* allocator) noexcept
    : targetType_(targetType),
      basedOn_(basedOn),
      allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()),
      authored_(allocator_),
      flattened_(allocator_) {}

Base::Result<void> Style::TryAddSetter(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    if (sealed_) {
        return InvalidStyle("Cannot modify a sealed Style");
    }
    if (!property.IsValid() || value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Style setter requires a property and concrete value");
    }
    for (const StyleSetter& setter : authored_) {
        if (setter.property == property) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Style already has a setter for this property");
        }
    }
    return authored_.TryPushBack({property, value});
}

Base::Result<void> Style::Seal(
    const DependencyPropertyRegistry& properties) noexcept {
    if (sealed_) {
        return {};
    }
    if (!properties.IsFrozen() || targetType_ == InvalidTypeId ||
        properties.Types().FindType(targetType_) == nullptr) {
        return InvalidStyle("Style requires a frozen registry and registered target type");
    }

    const Style* ancestor = basedOn_;
    while (ancestor != nullptr) {
        if (ancestor == this) {
            return Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "Style BasedOn graph contains a cycle");
        }
        if (!ancestor->sealed_) {
            return InvalidStyle("BasedOn style must be sealed before its derived style");
        }
        if (!IsTargetCompatible(
                properties.Types(), targetType_, ancestor->targetType_)) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Derived Style target type is incompatible with BasedOn target type");
        }
        ancestor = ancestor->basedOn_;
    }

    Base::Vector<StyleSetter> next(allocator_);
    if (basedOn_ != nullptr) {
        Base::Result<void> inherited = next.TryAppend(basedOn_->Setters());
        if (!inherited) {
            return inherited.GetStatus();
        }
    }
    for (const StyleSetter& setter : authored_) {
        const DependencyProperty* property = properties.Find(setter.property);
        if (property == nullptr || property->MetadataFor(targetType_) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Style setter does not apply to its target type");
        }
        Base::Result<void> validValue = properties.ValidateValueFor(
            setter.property, targetType_, setter.value);
        if (!validValue) {
            return validValue.GetStatus();
        }
        bool replaced = false;
        for (StyleSetter& inherited : next) {
            if (inherited.property == setter.property) {
                inherited.value = setter.value;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            Base::Result<void> appended = next.TryPushBack(setter);
            if (!appended) {
                return appended.GetStatus();
            }
        }
    }
    flattened_ = std::move(next);
    sealed_ = true;
    return {};
}

Base::Result<void> StyleManager::VerifyTarget(
    const DependencyObject& object,
    const Style& style) const noexcept {
    if (values_ == nullptr || properties_ == nullptr || !style.IsSealed()) {
        return InvalidStyle("StyleManager requires a sealed Style");
    }
    if (!IsTargetCompatible(
            properties_->Types(), object.RuntimeType(), style.TargetType())) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Style target type is incompatible with the object type");
    }
    return {};
}

Base::Result<void> StyleManager::Apply(
    DependencyObject& object,
    const Style& style) noexcept {
    Base::Result<void> verified = VerifyTarget(object, style);
    if (!verified) {
        return verified.GetStatus();
    }
    for (const StyleSetter& setter : style.Setters()) {
        Base::Result<void> applied = values_->SetStyleValue(
            object, setter.property, setter.value);
        if (!applied) {
            return applied.GetStatus();
        }
    }
    return {};
}

Base::Result<void> StyleManager::Clear(
    DependencyObject& object,
    const Style& style) noexcept {
    Base::Result<void> verified = VerifyTarget(object, style);
    if (!verified) {
        return verified.GetStatus();
    }
    for (const StyleSetter& setter : style.Setters()) {
        Base::Result<void> cleared = values_->ClearStyleValue(object, setter.property);
        if (!cleared) {
            return cleared.GetStatus();
        }
    }
    return {};
}

} // namespace Aero::Core
