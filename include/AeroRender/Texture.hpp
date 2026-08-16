#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>

#include <cstdint>

namespace Aero {

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Base class for 2D textures (reference: NoesisGUI NsRender/Texture.h)
////////////////////////////////////////////////////////////////////////////////////////////////////
class AERO_GUI_API Texture : public Base::Object {
public:
    ~Texture() noexcept override = default;

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    /// Returns the width of the texture, in pixels
    virtual std::uint32_t GetWidth() const noexcept = 0;

    /// Returns the height of the texture, in pixels
    virtual std::uint32_t GetHeight() const noexcept = 0;

    /// Returns true if the texture has mipmaps
    virtual bool HasMipMaps() const noexcept = 0;

    /// Returns true when texture must be vertically inverted when mapped (e.g. OpenGL)
    virtual bool IsInverted() const noexcept = 0;

    /// Returns true if the texture has an alpha channel that is not completely white
    virtual bool HasAlpha() const noexcept = 0;

    /// Stores custom private data
    void SetPrivateData(Ref<Base::Object> data) noexcept {
        privateData_ = data;
    }

    /// Gets custom private data
    Ref<Base::Object> GetPrivateData() const noexcept {
        return privateData_;
    }

protected:
    Texture() noexcept = default;

private:
    Ref<Base::Object> privateData_;
};

} // namespace Aero
