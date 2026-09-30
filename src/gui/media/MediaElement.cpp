#include "Aero/Media/MediaElement.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryDetail.hpp"
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
#include <Aero/Media/Images.hpp>
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

MediaElement::~MediaElement() = default;

void MediaElement::SetSource(StringView value) noexcept {
    String source;
    if (!source.Assign(value)) return;
    SetValue(SourceProperty, std::move(source));
}

void MediaElement::Play() noexcept {
}

void MediaElement::Pause() noexcept {
}

void MediaElement::Stop() noexcept {
}

void MediaElement::Close() noexcept {
}

} // namespace Aero::Media

// Metadata registration for the types implemented in this file.
AERO_DESCRIBE(::Aero::Media::MediaElement) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Threading;
    using namespace ::Aero::Input;
    using namespace ::Aero::Media;
    using namespace ::Aero::Data;
    using namespace ::Aero::Interactivity;
    Register<MediaElement>(context)
            .Event(MediaElement::BufferingEndedEvent, RoutingStrategy::Direct)
            .Event(MediaElement::BufferingStartedEvent, RoutingStrategy::Direct)
            .Event(MediaElement::MediaEndedEvent, RoutingStrategy::Direct)
            .Event(MediaElement::MediaFailedEvent, RoutingStrategy::Direct)
            .Event(MediaElement::MediaOpenedEvent, RoutingStrategy::Direct)
            .Property(MediaElement::SourceProperty, Base::String{})
            .Property(MediaElement::StretchProperty, Stretch::Uniform, AffectsMeasure | AffectsRender)
            .Property(MediaElement::StretchDirectionProperty, StretchDirection::Both, AffectsMeasure | AffectsRender)
            .Property(MediaElement::LoadedBehaviorProperty, MediaState::Play)
            .Property(MediaElement::UnloadedBehaviorProperty, MediaState::Close)
            .Property(MediaElement::IsMutedProperty, false)
            .Property(MediaElement::VolumeProperty, 0.5)
            .Property(MediaElement::BalanceProperty, 0.0)
            .Property(MediaElement::ScrubbingEnabledProperty, false)
            .Factory();
}
