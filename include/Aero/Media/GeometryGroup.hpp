#pragma once

#include <Aero/Media/FreezableCollection.hpp>
#include <Aero/Media/Geometry.hpp>

namespace Aero::Media {

class AERO_GUI_API GeometryGroup : public Geometry {
    AERO_DECLARE_TYPE(GeometryGroup, Geometry)
public:
    GeometryGroup() noexcept : Geometry(StaticTypeId()) {}
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Result<void> Add(Ref<Geometry> value) noexcept;
    void Clear() noexcept {
        if (!WritePreamble()) return;
        children_.Clear();
        WritePostscript();
    }
    Span<const Ref<Geometry>> GetChildren() const noexcept {
        return children_.AsSpan();
    }
protected:
    Result<void> FlattenCore(FlattenSink& sink) const noexcept override;
private:
    FreezableCollection<Geometry> children_;
};
} // namespace Aero::Media
