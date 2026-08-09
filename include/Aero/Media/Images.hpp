#pragma once

#include <Aero/Base/ResourceUri.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Media {

using ::Aero::Meta::TypeId;

enum class Stretch : std::uint8_t {
    None = 0U,
    Fill,
    Uniform,
    UniformToFill
};

enum class StretchDirection : std::uint8_t {
    UpOnly = 0U,
    DownOnly,
    Both
};

class AERO_GUI_API ImageSource : public DependencyObject {
    AERO_DECLARE_TYPE(ImageSource, DependencyObject)
protected:
    explicit ImageSource(TypeId runtimeType) noexcept
        : DependencyObject(runtimeType) {}
    ~ImageSource() override = default;
};

class AERO_GUI_API BitmapImage : public ImageSource {
    AERO_DECLARE_TYPE(BitmapImage, ImageSource)
public:
    BitmapImage() noexcept
        : ImageSource(StaticTypeId()) {}
    ~BitmapImage() override = default;

    Base::ResourceUri GetUriSource() const noexcept;
    void SetUriSource(
        const Base::ResourceUri& value) noexcept;

    inline static constexpr DependencyProperty<Base::ResourceUri> UriSourceProperty{"UriSource"};
};

} // namespace Aero::Media

AERO_DECLARE_TYPE_ENUM(Aero::Media::Stretch)

AERO_DECLARE_TYPE_ENUM(Aero::Media::StretchDirection)

namespace Aero::Controls {
using Stretch = Aero::Media::Stretch;
using StretchDirection =
    Aero::Media::StretchDirection;
} // namespace Aero::Controls
