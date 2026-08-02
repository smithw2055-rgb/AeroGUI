#include <Aero/Media/Images.hpp>

namespace Aero::Media {

Base::ResourceUri BitmapImage::GetUriSource() const noexcept {
    return GetValueOr(
        UriSourceProperty,
        Base::ResourceUri{});
}

void BitmapImage::SetUriSource(
    const Base::ResourceUri& value) noexcept {
    SetValue(UriSourceProperty, value);
}

} // namespace Aero::Media
