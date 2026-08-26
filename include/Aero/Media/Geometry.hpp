#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/Media/Transform.hpp>
#include <Aero/Value.hpp>

namespace Aero::Media {
using Point = Base::Point;
using Size = Base::Size;
using Rect = Base::Rect;
using Thickness = Base::Thickness;
using CornerRadius = Base::CornerRadius;

struct FlattenSink {
    virtual ~FlattenSink() = default;
    virtual Result<void> AddPoint(Point point) noexcept = 0;
    virtual Result<void> BeginFigure(Point start, bool isClosed) noexcept {
        (void)isClosed;
        return AddPoint(start);
    }
    virtual Result<void> EndFigure(bool isClosed) noexcept {
        (void)isClosed;
        return {};
    }
};

class AERO_GUI_API Geometry : public Freezable {
    AERO_DECLARE_TYPE(Geometry, Freezable)
public:
    Geometry() noexcept : Freezable(StaticTypeId()) {}
    ~Geometry() override;
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    virtual Rect GetBounds() const noexcept { return {}; }
    // Applies Geometry.Transform, then FlattenCore. Rendering must Flatten
    // rather than round-trip PathGeometry through ToStreamData.
    Result<void> Flatten(FlattenSink& sink) const noexcept;
    Ref<Transform> GetTransform() const noexcept {
        return transform_;
    }
    void SetTransform(Ref<Transform> value) noexcept;
    inline static constexpr DependencyProperty<Ref<Transform>> TransformProperty{"Transform"};
private:
    void OnTransformChanged(Freezable&) noexcept;
    Ref<Transform> transform_;
    FreezableChangedHandler transformChangedHandler_;

protected:
    explicit Geometry(Meta::TypeId runtimeType) noexcept
        : Freezable(runtimeType) {}
    virtual Result<void> FlattenCore(FlattenSink& sink) const noexcept;
    bool FreezeCore(bool isChecking) noexcept override;
};
} // namespace Aero::Media
