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

Ref<ImageSource> CroppedBitmap::GetSource() const noexcept {
    return GetValueOr(
        SourceProperty,
        Ref<ImageSource>{});
}

void CroppedBitmap::SetSource(
    Ref<ImageSource> value) noexcept {
    SetValue(SourceProperty, std::move(value));
}

Base::Rect CroppedBitmap::GetSourceRect() const noexcept {
    return GetValueOr(
        SourceRectProperty,
        Base::Rect{});
}

void CroppedBitmap::SetSourceRect(
    Base::Rect value) noexcept {
    SetValue(SourceRectProperty, value);
}

} // namespace Aero::Media
