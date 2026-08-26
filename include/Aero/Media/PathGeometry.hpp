#pragma once

#include <Aero/Media/FreezableCollection.hpp>
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
protected:
    Result<void> FlattenCore(FlattenSink& sink) const noexcept override;
private:
    FreezableCollection<PathFigure> figures_;
};
} // namespace Aero::Media
