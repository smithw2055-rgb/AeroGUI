#include <Aero/Presentation/Images.hpp>

namespace Aero::Presentation {

Base::ResourceUri BitmapImage::UriSource() const noexcept {
    return GetValueOr(
        UriSourceProperty,
        Base::ResourceUri{});
}

Base::Result<void> BitmapImage::SetUriSource(
    const Base::ResourceUri& value) noexcept {
    return SetValue(UriSourceProperty, value);
}

} // namespace Aero::Presentation
