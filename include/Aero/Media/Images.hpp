#pragma once

// Image source family plus Stretch. BitmapImage and CroppedBitmap live here.
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero::Media {

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

using ::Aero::Meta::TypeId;

// Decoded images are immutable resources. A generation counter replaces the
// dependency-property store; XAML still sets UriSource and SourceRect by name.
class AERO_GUI_API ImageSource : public Base::Object {
    AERO_DECLARE_TYPE(ImageSource, Base::Object)

public:
    TypeId RuntimeType() const noexcept override { return runtimeType_; }
    std::uint64_t GetRevision() const noexcept { return revision_; }

protected:
    explicit ImageSource(TypeId runtimeType) noexcept : runtimeType_(runtimeType) {}
    ~ImageSource() override = default;
    void BumpRevision() noexcept { ++revision_; }

private:
    TypeId runtimeType_;
    std::uint64_t revision_ = 0U;
};

class AERO_GUI_API BitmapImage : public ImageSource {
    AERO_DECLARE_TYPE(BitmapImage, ImageSource)

public:
    BitmapImage() noexcept : ImageSource(StaticTypeId()) {}
    ~BitmapImage() override = default;

    Base::ResourceUri GetUriSource() const noexcept { return uri_; }
    void SetUriSource(const Base::ResourceUri& value) noexcept;

private:
    Base::ResourceUri uri_{};
};

class AERO_GUI_API CroppedBitmap : public ImageSource {
    AERO_DECLARE_TYPE(CroppedBitmap, ImageSource)

public:
    CroppedBitmap() noexcept : ImageSource(StaticTypeId()) {}
    ~CroppedBitmap() override = default;

    Ref<ImageSource> GetSource() const noexcept { return source_; }
    void SetSource(Ref<ImageSource> value) noexcept;
    Base::Rect GetSourceRect() const noexcept { return sourceRect_; }
    void SetSourceRect(Base::Rect value) noexcept;

private:
    Ref<ImageSource> source_{};
    Base::Rect sourceRect_{};
};

} // namespace Aero::Media

AERO_DECLARE_TYPE_ENUM(Aero::Media::Stretch)

AERO_DECLARE_TYPE_ENUM(Aero::Media::StretchDirection)

namespace Aero::Controls {
using Stretch = Aero::Media::Stretch;
using StretchDirection = Aero::Media::StretchDirection;
} // namespace Aero::Controls
