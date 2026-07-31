#include <Aero/Media/Images.hpp>

namespace Aero::Media {

Base::ResourceUri BitmapImage::UriSource() const noexcept {
    return GetValueOr(
        UriSourceProperty,
        Base::ResourceUri{});
}

Base::Result<void> BitmapImage::SetUriSource(
    const Base::ResourceUri& value) noexcept {
    return SetValue(UriSourceProperty, value);
}

} // namespace Aero::Media
