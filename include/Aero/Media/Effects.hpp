#pragma once

// Effect family: Effect plus the concrete effects.
#include <Aero/Animatable.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyProperty.hpp>

#include <array>
#include <cstdint>

namespace Aero::Media {

class AERO_GUI_API Effect : public ::Aero::Animatable {
    AERO_DECLARE_TYPE(Effect, ::Aero::Animatable)

public:
    // Freezable content revision (render cache invalidation).
    std::uint64_t GetRevision() const noexcept;

protected:
    explicit Effect(Meta::TypeId runtimeType) noexcept : Animatable(runtimeType) {}
};

class AERO_GUI_API BlurEffect : public Effect {
    AERO_DECLARE_TYPE(BlurEffect, Effect)

public:
    BlurEffect() noexcept : Effect(StaticTypeId()) {}

    AERO_DEPENDENCY_PROPERTY2(double, Radius);
};

class AERO_GUI_API DropShadowEffect : public Effect {
    AERO_DECLARE_TYPE(DropShadowEffect, Effect)

public:
    DropShadowEffect() noexcept : Effect(StaticTypeId()) {}

    AERO_DEPENDENCY_PROPERTY2(double, BlurRadius);
    AERO_DEPENDENCY_PROPERTY2(double, Direction);
    AERO_DEPENDENCY_PROPERTY2(double, ShadowDepth);
    AERO_DEPENDENCY_PROPERTY2(double, Opacity);
    AERO_DEPENDENCY_PROPERTY2(Base::Color, Color);
};

class AERO_GUI_API PixelateEffect : public Effect {
    AERO_DECLARE_TYPE(PixelateEffect, Effect)

public:
    PixelateEffect() noexcept : Effect(StaticTypeId()) {}

    AERO_DEPENDENCY_PROPERTY2(double, Size);
};

class AERO_GUI_API TintEffect : public Effect {
    AERO_DECLARE_TYPE(TintEffect, Effect)

public:
    TintEffect() noexcept : Effect(StaticTypeId()) {}

    AERO_DEPENDENCY_PROPERTY2(Base::Color, Color);
};

class AERO_GUI_API DirectionalBlurEffect : public Effect {
    AERO_DECLARE_TYPE(DirectionalBlurEffect, Effect)

public:
    DirectionalBlurEffect() noexcept : Effect(StaticTypeId()) {}

    AERO_DEPENDENCY_PROPERTY2(double, Radius);
    AERO_DEPENDENCY_PROPERTY2(double, Angle);
};

class AERO_GUI_API ShaderEffect : public Effect {
    AERO_DECLARE_TYPE(ShaderEffect, Effect)

public:
    ShaderEffect() noexcept : Effect(StaticTypeId()) {}

    StringView GetPixelShader() const noexcept;
    void SetPixelShader(StringView value) noexcept;

    Base::Span<const std::uint8_t> GetBytecode() const noexcept { return {bytecode_.Data(), bytecode_.Size()}; }
    void SetBytecode(Base::Span<const std::uint8_t> value) noexcept;

    Base::Span<const float> GetUniforms() const noexcept { return {uniforms_.data(), uniformCount_}; }
    void SetUniform(std::uint32_t index, float value) noexcept;

    std::uint32_t GetShaderId() const noexcept { return shaderId_; }
    void SetShaderId(std::uint32_t value) noexcept { shaderId_ = value; }

    AERO_DEPENDENCY_PROPERTY(String, PixelShader);

    static void OnPixelShaderChanged(DependencyObject& object,
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
