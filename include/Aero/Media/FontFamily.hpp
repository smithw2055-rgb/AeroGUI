#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Value.hpp>

namespace Aero::Media {

class AERO_GUI_API FontFamily : public Base::Object {
    AERO_DECLARE_TYPE(FontFamily, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    StringView GetSource() const noexcept {
        return source_.View();
    }
    void SetSource(StringView value) noexcept {
        (void)source_.Assign(value);
    }

private:
    String source_;
};

} // namespace Aero::Media
