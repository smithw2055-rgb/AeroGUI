#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Value.hpp>
#include <Aero/Controls/GridLength.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API ColumnDefinition : public Base::Object {
    AERO_DECLARE_TYPE(ColumnDefinition, Base::Object)
public:
    ColumnDefinition() noexcept = default;
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    GridLength GetWidth() const noexcept { return width_; }
    double GetMaxWidth() const noexcept { return maxWidth_; }
    StringView GetSharedSizeGroup() const noexcept {
        return sharedSizeGroup_.View();
    }
    void SetWidth(GridLength value) noexcept;
    void SetMaxWidth(double value) noexcept;
    void SetSharedSizeGroup(
        StringView value) noexcept;
private:
    GridLength width_ = GridLength::Star();
    double maxWidth_ = 1.0e12;
    String sharedSizeGroup_;
};

} // namespace Aero::Controls
