#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Value.hpp>
#include <Aero/Controls/GridLength.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API RowDefinition : public Base::Object {
    AERO_DECLARE_TYPE(RowDefinition, Base::Object)
public:
    RowDefinition() noexcept = default;
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    GridLength GetHeight() const noexcept { return height_; }
    double GetMaxHeight() const noexcept { return maxHeight_; }
    StringView GetSharedSizeGroup() const noexcept {
        return sharedSizeGroup_.View();
    }
    void SetHeight(GridLength value) noexcept;
    void SetMaxHeight(double value) noexcept;
    void SetSharedSizeGroup(
        StringView value) noexcept;
private:
    GridLength height_ = GridLength::Star();
    double maxHeight_ = 1.0e12;
    String sharedSizeGroup_;
};

} // namespace Aero::Controls
