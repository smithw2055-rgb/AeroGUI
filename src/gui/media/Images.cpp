#include <Aero/Media/Images.hpp>
#include "gui/core/Describe.hpp"
#include <utility>

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

// Metadata registration for the types implemented in this file.
namespace Aero::MetadataSupport {
using namespace ::Aero::Meta;
using namespace ::Aero::Media;
namespace {

Base::Result<Value> ConvertImageSourceText(
    TypeId targetType,
    Base::StringView text,
    void*) noexcept {
    if (targetType !=
        ImageSource::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ImageSource text conversion received an invalid target");
    }
    Base::Result<Base::ResourceUri> uri =
        Base::ResourceUri::Parse(text);
    if (!uri || uri.Value().Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "ImageSource requires a non-empty resource URI");
    }
    Base::Result<Base::Ref<BitmapImage>> image =
        Base::MakeRef<BitmapImage>();
    if (!image) return image.GetStatus();
    image.Value()->SetUriSource(uri.Value());
    return Value::FromObject(
        ImageSource::StaticTypeId(),
        Base::Ref<Base::Object>(
            image.Value()));
}
} // namespace
} // namespace Aero::MetadataSupport

namespace Aero::Media {

AERO_DESCRIBE(ImageSource) {
    using namespace Aero::Meta;
    Register<ImageSource>(context, TypeFlags::Abstract)
            .TextConverter(&::Aero::MetadataSupport::ConvertImageSourceText);
}

AERO_DESCRIBE(BitmapImage) {
    using namespace Aero::Meta;
    Register<BitmapImage>(context)
            .Property<Base::ResourceUri, &BitmapImage::GetUriSource, &BitmapImage::SetUriSource>("UriSource")
            .Factory();
}

AERO_DESCRIBE(CroppedBitmap) {
    using namespace Aero::Meta;
    Register<CroppedBitmap>(context)
            .Property<Base::Ref<ImageSource>, &CroppedBitmap::GetSource, &CroppedBitmap::SetSource>("Source")
            .Property<Base::Rect, &CroppedBitmap::GetSourceRect, &CroppedBitmap::SetSourceRect>("SourceRect")
            .Factory();
}

} // namespace Aero::Media

