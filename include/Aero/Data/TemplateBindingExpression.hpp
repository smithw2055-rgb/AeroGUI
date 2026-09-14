#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero {
class DependencyObject;
}

namespace Aero::Data {

// TemplateBinding does not go through BindingEngine. This type is a
// TemplateEngine facade over an applied TemplateBindingPlan.
class AERO_GUI_API TemplateBindingExpression {
public:
    TemplateBindingExpression() noexcept = default;

    bool IsValid() const noexcept {
        return target_ != nullptr && targetProperty_.IsValid();
    }
    Base::Status UpdateTarget() noexcept;
    DependencyObject* GetTarget() const noexcept { return target_; }
    DependencyPropertyHandle GetTargetProperty() const noexcept {
        return targetProperty_;
    }

private:
    friend class BindingOperations;
    DependencyObject* target_ = nullptr;
    DependencyPropertyHandle targetProperty_{};
};

} // namespace Aero::Data
