#pragma once

#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Freezable.hpp>

namespace Aero::Media {

class AERO_GUI_API DashStyle : public Freezable {
    AERO_DECLARE_TYPE(DashStyle, Freezable)
public:
    DashStyle() noexcept : Freezable(StaticTypeId()) {}

    Span<const double> GetDashes() const noexcept {
        return {dashes_.Data(), dashes_.Size()};
    }
    double GetOffset() const noexcept { return offset_; }

    Result<void> SetDashes(Span<const double> value) noexcept;
    void SetOffset(double value) noexcept;

private:
    Base::Vector<double> dashes_;
    double offset_ = 0.0;
};

} // namespace Aero::Media
