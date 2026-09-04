#pragma once

#include <Aero/Freezable.hpp>
#include <Aero/Media/Geometry.hpp>

namespace Aero::Media {

class AERO_GUI_API PathSegment : public Freezable {
    AERO_DECLARE_TYPE(PathSegment, Freezable)
public:
    virtual Result<void> Flatten(
        FlattenSink& sink,
        Point& currentPoint) const noexcept {
        (void)sink;
        (void)currentPoint;
        return {};
    }
protected:
    explicit PathSegment(Meta::TypeId runtimeType) noexcept
        : Freezable(runtimeType) {}
    ~PathSegment() override = default;
};
} // namespace Aero::Media
