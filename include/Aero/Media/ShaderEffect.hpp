#pragma once

#include <Aero/Media/Effect.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <array>

namespace Aero::Media {

class AERO_GUI_API ShaderEffect : public Effect {
    AERO_DECLARE_TYPE(ShaderEffect, Effect)
public:
    ShaderEffect() noexcept : Effect(StaticTypeId()) {}

    StringView GetPixelShader() const noexcept;
    void SetPixelShader(StringView value) noexcept;

    Base::Span<const std::uint8_t> GetBytecode() const noexcept {
        return {bytecode_.Data(), bytecode_.Size()};
    }
    Result<void> SetBytecode(Base::Span<const std::uint8_t> value) noexcept;

    Base::Span<const float> GetUniforms() const noexcept {
        return {uniforms_.data(), uniformCount_};
    }
    void SetUniform(std::uint32_t index, float value) noexcept;

    std::uint32_t GetShaderId() const noexcept { return shaderId_; }
    void SetShaderId(std::uint32_t value) noexcept { shaderId_ = value; }

    inline static constexpr DependencyProperty<String> PixelShaderProperty{"PixelShader"};

    static void OnPixelShaderChanged(
        DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs& args) noexcept;

private:
    mutable String source_;
    Base::Vector<std::uint8_t> bytecode_;
    std::array<float, 16> uniforms_{};
    std::uint32_t uniformCount_ = 0U;
    std::uint32_t shaderId_ = 0U;

    void SynchronizePixelShaderCache() const noexcept;
};

} // namespace Aero::Media
