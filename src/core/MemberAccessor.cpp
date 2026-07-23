#include <Aero/Core/MemberAccessor.hpp>

namespace Aero::Core {

Base::Result<void> MemberAccessor::Freeze() noexcept {
    if (frozen_) return {};
    if (runtime_ == nullptr || !runtime_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MetadataRuntime must be frozen before MemberAccessor");
    }
    frozen_ = true;
    return {};
}

Base::Result<Value> MemberAccessor::GetProperty(
    const Base::Object& object,
    const PropertyInfo& property) const noexcept {
    if (!frozen_ || runtime_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MemberAccessor must be frozen before use");
    }
    return runtime_->GetProperty(object, property.Id());
}

Base::Result<void> MemberAccessor::SetProperty(
    Base::Object& object,
    const PropertyInfo& property,
    const Value& value) const noexcept {
    if (!frozen_ || runtime_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MemberAccessor must be frozen before use");
    }
    return runtime_->SetProperty(object, property.Id(), value);
}

Base::Result<Value> MemberAccessor::InvokeMethod(
    Base::Object& object,
    const MethodInfo& method,
    Base::Span<const Value> arguments) const noexcept {
    if (!frozen_ || runtime_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MemberAccessor must be frozen before use");
    }
    return runtime_->InvokeMethod(object, method.Id(), arguments);
}

} // namespace Aero::Core
