#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/PathFigure.hpp>

namespace Aero::Media {

class AERO_GUI_API PathGeometry : public Geometry {
    AERO_DECLARE_TYPE(PathGeometry, Geometry)
public:
    PathGeometry() noexcept : Geometry(StaticTypeId()) {}
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Result<void> AddFigure(Ref<PathFigure> value) noexcept;
    void ClearFigures() noexcept {
        if (!WritePreamble()) return;
        figures_.Clear();
        WritePostscript();
    }
    Span<const Ref<PathFigure>> GetFigures() const noexcept {
        return figures_.AsSpan();
    }
    Result<String> ToStreamData() const noexcept;
private:
    Base::Vector<Ref<PathFigure>> figures_;
};
} // namespace Aero::Media
