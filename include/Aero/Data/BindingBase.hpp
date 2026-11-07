#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Value.hpp>
#include <new>
#include <utility>

namespace Aero {
class DependencyObject;
namespace Meta { struct DependencyPropertyHandle; }
} // namespace Aero
namespace Aero::Data {

class BindingExpression;

class AERO_GUI_API BindingBase : public Base::Object {
    AERO_DECLARE_TYPE(BindingBase, Base::Object)

public:
    ~BindingBase() override { delete rare_; }

    Meta::TypeId RuntimeType() const noexcept override { return runtimeType_; }
    // WPF-style factory: Binding/MultiBinding/PriorityBinding override to
    // create their expression without exposing BindingEngine in headers.
    virtual Ref<BindingExpression> CreateExpression(DependencyObject* target,
        Meta::DependencyPropertyHandle property) const noexcept {
        static_cast<void>(target);
        static_cast<void>(property);
        return {};
    }

    StringView GetStringFormat() const noexcept {
        return rare_ != nullptr ? rare_->stringFormat.View() : StringView{};
    }
    void SetStringFormat(StringView value) noexcept {
        if (value.Empty() && rare_ == nullptr) return;
        if (RareOptions* r = EnsureRare()) {
            (void)r->stringFormat.Assign(value);
        }
    }
    const Value& GetFallbackValue() const noexcept {
        return rare_ != nullptr ? rare_->fallbackValue : EmptyValue();
    }
    void SetFallbackValue(Value value) noexcept {
        if (value.IsUnset() && rare_ == nullptr) return;
        if (RareOptions* r = EnsureRare()) {
            r->fallbackValue = std::move(value);
        }
    }
    const Value& GetTargetNullValue() const noexcept {
        return rare_ != nullptr ? rare_->targetNullValue : EmptyValue();
    }
    void SetTargetNullValue(Value value) noexcept {
        if (value.IsUnset() && rare_ == nullptr) return;
        if (RareOptions* r = EnsureRare()) {
            r->targetNullValue = std::move(value);
        }
    }

protected:
    explicit BindingBase(Meta::TypeId runtimeType) noexcept : runtimeType_(runtimeType) {}

    struct RareOptions {
        String stringFormat{&Base::GetDefaultAllocator()};
        Value fallbackValue;
        Value targetNullValue;
        Value converterParameter;
    };

    RareOptions* EnsureRare() noexcept {
        if (rare_ == nullptr) {
            rare_ = new (std::nothrow) RareOptions();
        }
        return rare_;
    }

    static const Value& EmptyValue() noexcept {
        static const Value kEmpty{};
        return kEmpty;
    }

    Meta::TypeId runtimeType_ = StaticTypeId();
    RareOptions* rare_ = nullptr;
};
} // namespace Aero::Data
