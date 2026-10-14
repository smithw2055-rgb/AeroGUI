#include <Aero/Media/MediaElement.hpp>
#include "gui/core/Describe.hpp"
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
namespace Aero::Media {

AERO_DESCRIBE(MediaElement) {
    using namespace Aero::Meta;
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

} // namespace Aero::Media

