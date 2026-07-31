#pragma once

#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>

namespace Aero::Media {

using namespace Aero::Core;

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

class AERO_API ImageSource : public DependencyObject {
    AERO_DECLARE_TYPE(ImageSource, DependencyObject)
protected:
    explicit ImageSource(TypeId runtimeType) noexcept
        : DependencyObject(runtimeType) {}
    ~ImageSource() override = default;
};

class AERO_API BitmapImage final : public ImageSource {
    AERO_DECLARE_TYPE(BitmapImage, ImageSource)
public:
    BitmapImage() noexcept
        : ImageSource(StaticTypeId()) {}
    ~BitmapImage() override = default;

    Base::ResourceUri UriSource() const noexcept;
    Base::Result<void> SetUriSource(
        const Base::ResourceUri& value) noexcept;

    inline static constexpr Members::Property<
        Base::ResourceUri>
        UriSourceProperty{"UriSource"};
};

} // namespace Aero::Media

namespace Aero::Core {

template<>
struct MetaTypeTraits<Aero::Media::Stretch> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("Stretch");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "Stretch";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Aero::Media::StretchDirection> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("StretchDirection");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "StretchDirection";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core

namespace Aero::Controls {
using Stretch = Aero::Media::Stretch;
using StretchDirection =
    Aero::Media::StretchDirection;
} // namespace Aero::Controls
