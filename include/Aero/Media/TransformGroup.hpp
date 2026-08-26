#pragma once

#include <Aero/Base/Span.hpp>
#include <Aero/Media/FreezableCollection.hpp>
#include <Aero/Media/Transform.hpp>

namespace Aero::Media {

class AERO_GUI_API TransformGroup : public Transform {
    AERO_DECLARE_TYPE(TransformGroup, Transform)
public:
    TransformGroup() noexcept : Transform(StaticTypeId()) {}
    ~TransformGroup() override;
    Result<void> AddChild(
        Ref<Transform> value) noexcept;
    void ClearChildren() noexcept;
    Span<const Ref<Transform>>
    GetChildren() const noexcept {
        return children_.AsSpan();
    }
    Base::Transform2D GetMatrix() const noexcept override;

private:
    bool FreezeCore(bool isChecking) noexcept override;
    void OnChildChanged(Freezable&) noexcept;
    FreezableCollection<Transform> children_;
    FreezableChangedHandler childChangedHandler_;
};
} // namespace Aero::Media
