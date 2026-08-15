#include "Aero/Media/MediaElement.hpp"

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
