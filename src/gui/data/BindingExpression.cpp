#include <Aero/Data/BindingExpression.hpp>
#include <Aero/Data/BindingOperations.hpp>
#include <Aero/UIElement.hpp>

#include "gui/data/BindingEngine.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/controls/ControlBehavior.hpp"

namespace Aero::Data {

BindingEngine* BindingExpression::EngineOf(const BindingHandle& handle) noexcept {
    return handle.IsValid()
        ? static_cast<BindingEngine*>(handle.engine_)
        : nullptr;
}

namespace {

Base::Status InvalidExpression() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "BindingExpression handle is invalid");
}

} // namespace

bool BindingExpression::IsValid() const noexcept {
    BindingEngine* engine = BindingExpression::EngineOf(handle_);
    return engine != nullptr && engine->Contains(handle_);
}

BindingStatus BindingExpression::Status() const noexcept {
    BindingEngine* engine = BindingExpression::EngineOf(handle_);
    if (engine == nullptr) return BindingStatus::Unattached;
    return engine->QueryStatus(handle_);
}

Base::Status BindingExpression::UpdateSource() noexcept {
    BindingEngine* engine = BindingExpression::EngineOf(handle_);
    if (engine == nullptr || !engine->Contains(handle_)) {
        return InvalidExpression();
    }
    Base::Result<bool> updated = engine->UpdateSource(handle_);
    if (!updated) return updated.GetStatus();
    if (!updated.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "BindingExpression cannot update the source");
    }
    Base::Result<std::uint32_t> flushed = engine->Flush();
    return flushed ? Base::Status{} : flushed.GetStatus();
}

Base::Status BindingExpression::UpdateTarget() noexcept {
    BindingEngine* engine = BindingExpression::EngineOf(handle_);
    if (engine == nullptr || !engine->Contains(handle_)) {
        return InvalidExpression();
    }
    Base::Result<bool> updated = engine->UpdateTarget(handle_);
    if (!updated) return updated.GetStatus();
    if (!updated.Value()) return InvalidExpression();
    return {};
}

bool MultiBindingExpression::IsValid() const noexcept {
    if (handles_.Empty()) return false;
    for (std::uint32_t index = 0U; index < handles_.Size(); ++index) {
        BindingEngine* engine = BindingExpression::EngineOf(handles_[index]);
        if (engine == nullptr || !engine->Contains(handles_[index])) {
            return false;
        }
    }
    return true;
}

BindingStatus MultiBindingExpression::Status() const noexcept {
    if (!IsValid()) return BindingStatus::Unattached;
    BindingStatus status = BindingStatus::Active;
    for (std::uint32_t index = 0U; index < handles_.Size(); ++index) {
        BindingEngine* engine = BindingExpression::EngineOf(handles_[index]);
        const BindingStatus child = engine->QueryStatus(handles_[index]);
        if (child == BindingStatus::UpdateSourceError ||
            child == BindingStatus::UpdateTargetError) {
            return child;
        }
        if (child == BindingStatus::Inactive) {
            status = BindingStatus::Inactive;
        }
    }
    return status;
}

Base::Status MultiBindingExpression::UpdateSource() noexcept {
    if (!IsValid()) return InvalidExpression();
    for (std::uint32_t index = 0U; index < handles_.Size(); ++index) {
        BindingExpression child(handles_[index]);
        Base::Status status = child.UpdateSource();
        if (!status.IsOk() &&
            status.code != Base::ErrorCode::InvalidState) {
            return status;
        }
    }
    return {};
}

Base::Status MultiBindingExpression::UpdateTarget() noexcept {
    if (!IsValid()) return InvalidExpression();
    for (std::uint32_t index = 0U; index < handles_.Size(); ++index) {
        BindingExpression child(handles_[index]);
        Base::Status status = child.UpdateTarget();
        if (!status.IsOk()) return status;
    }
    return {};
}

Base::Status TemplateBindingExpression::UpdateTarget() noexcept {
    if (!IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TemplateBindingExpression is invalid");
    }
    UIElement* element = TryCast<UIElement>(target_);
    if (element == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TemplateBindingExpression target is not a UIElement");
    }
    Controls::TemplateEngine* templates =
        AeroGuiInternal::TemplatesOf(*element);
    if (templates == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "TemplateEngine is unavailable");
    }
    Base::Result<void> refreshed =
        templates->RefreshTemplateBinding(*target_, targetProperty_);
    return refreshed ? Base::Status{} : refreshed.GetStatus();
}

BindingExpression BindingOperations::GetBindingExpression(
    DependencyObject* target,
    DependencyPropertyHandle property) noexcept {
    if (target == nullptr || !property.IsValid()) return {};
    BindingEngine* engine = AeroGuiInternal::BindingEngineOf(*target);
    if (engine == nullptr) return {};
    BindingHandle handle = engine->FindBinding(*target, property);
    if (!handle.IsValid()) return {};
    return BindingExpression(handle);
}

MultiBindingExpression BindingOperations::GetMultiBindingExpression(
    DependencyObject* target,
    DependencyPropertyHandle property) noexcept {
    if (target == nullptr || !property.IsValid()) return {};
    BindingEngine* engine = AeroGuiInternal::BindingEngineOf(*target);
    if (engine == nullptr) return {};
    return engine->FindMultiBinding(*target, property);
}

TemplateBindingExpression BindingOperations::GetTemplateBindingExpression(
    DependencyObject* target,
    DependencyPropertyHandle property) noexcept {
    TemplateBindingExpression expression;
    if (target == nullptr || !property.IsValid()) return expression;
    UIElement* element = TryCast<UIElement>(target);
    if (element == nullptr) return expression;
    Controls::TemplateEngine* templates =
        AeroGuiInternal::TemplatesOf(*element);
    if (templates == nullptr ||
        !templates->HasTemplateBinding(*target, property)) {
        return expression;
    }
    expression.target_ = target;
    expression.targetProperty_ = property;
    return expression;
}

} // namespace Aero::Data
