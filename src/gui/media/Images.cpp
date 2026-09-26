#include <Aero/Media/Images.hpp>

namespace Aero::Media {

void BitmapImage::SetUriSource(
    const Base::ResourceUri& value) noexcept {
    uri_ = value;
    BumpRevision();
}

void CroppedBitmap::SetSource(
    Ref<ImageSource> value) noexcept {
    source_ = std::move(value);
    BumpRevision();
}

void CroppedBitmap::SetSourceRect(
    Base::Rect value) noexcept {
    sourceRect_ = value;
    BumpRevision();
}

} // namespace Aero::Media
