#include <Aero/Media/Images.hpp>
#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/MediaActions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/ApplicationCommands.hpp>
#include <Aero/InputGesture.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/Animatable.hpp>
#include <cctype>
#include <cmath>
#include <cstdlib>
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

