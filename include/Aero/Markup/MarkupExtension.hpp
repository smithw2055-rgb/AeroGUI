#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Value.hpp>

namespace Aero::Markup {

// WPF MarkupExtension. User types inherit this, override ProvideValue, and
// register with TypeFlags::MarkupExtension plus Factory(). XAML object-element
// usage constructs the extension then replaces the property value with the
// result of ProvideValue. Attribute syntax `{local:MyExt}` constructs via the
// factory and calls ProvideValue with no constructor arguments parsed yet.
class AERO_GUI_API MarkupExtension : public Base::Object {
    AERO_DECLARE_TYPE(MarkupExtension, Base::Object)
public:
    TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    virtual Result<Value> ProvideValue() noexcept = 0;

protected:
    explicit MarkupExtension(TypeId runtimeType) noexcept
        : runtimeType_(runtimeType) {}

private:
    TypeId runtimeType_ = StaticTypeId();
};

} // namespace Aero::Markup
