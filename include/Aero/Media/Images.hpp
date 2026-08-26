#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/DependencyObject.hpp>

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

class AERO_GUI_API CroppedBitmap : public ImageSource {
    AERO_DECLARE_TYPE(CroppedBitmap, ImageSource)
public:
    CroppedBitmap() noexcept
        : ImageSource(StaticTypeId()) {}
    ~CroppedBitmap() override = default;

    Ref<ImageSource> GetSource() const noexcept;
    void SetSource(
        Ref<ImageSource> value) noexcept;
    Base::Rect GetSourceRect() const noexcept;
    void SetSourceRect(
        Base::Rect value) noexcept;

    inline static constexpr DependencyProperty<Ref<ImageSource>> SourceProperty{"Source"};
    inline static constexpr DependencyProperty<Base::Rect> SourceRectProperty{"SourceRect"};
};

} // namespace Aero::Media

AERO_DECLARE_TYPE_ENUM(Aero::Media::Stretch)

AERO_DECLARE_TYPE_ENUM(Aero::Media::StretchDirection)

namespace Aero::Controls {
using Stretch = Aero::Media::Stretch;
using StretchDirection =
    Aero::Media::StretchDirection;
} // namespace Aero::Controls
