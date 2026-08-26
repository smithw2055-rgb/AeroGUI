#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/DependencyProperty.hpp>
#include <cstdint>

namespace Aero::Data {

enum class RelativeSourceMode : std::uint8_t {
    PreviousData = 0U,
    TemplatedParent,
    Self,
    FindAncestor
};

class AERO_GUI_API RelativeSource : public Base::Object {
    AERO_DECLARE_TYPE(RelativeSource, Base::Object)
public:
    RelativeSource() noexcept = default;
    explicit RelativeSource(RelativeSourceMode mode) noexcept : mode_(mode) {}

    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    RelativeSourceMode GetMode() const noexcept { return mode_; }
    void SetMode(RelativeSourceMode value) noexcept { mode_ = value; }
    StringView GetAncestorType() const noexcept { return ancestorType_.View(); }
    void SetAncestorType(StringView value) noexcept { (void)ancestorType_.Assign(value); }
    std::uint32_t GetAncestorLevel() const noexcept { return ancestorLevel_; }
    void SetAncestorLevel(std::uint32_t value) noexcept { ancestorLevel_ = value == 0U ? 1U : value; }

    static Ref<RelativeSource> ForSelf() noexcept;
    static Ref<RelativeSource> ForTemplatedParent() noexcept;

private:
    RelativeSourceMode mode_ = RelativeSourceMode::Self;
    String ancestorType_;
    std::uint32_t ancestorLevel_ = 1U;
};
} // namespace Aero::Data
