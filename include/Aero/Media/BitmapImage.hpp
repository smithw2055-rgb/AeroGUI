#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Media/ImageSource.hpp>

namespace Aero::Media {

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
