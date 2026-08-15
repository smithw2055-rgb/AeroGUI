#pragma once

#include <Aero/DependencyProperty.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Images.hpp>

namespace Aero::Media {

using ::Aero::Meta::TypeId;

enum class MediaState : std::uint8_t {
    Manual = 0U,
    Play,
    Close,
    Pause,
    Stop
};

class AERO_GUI_API MediaElement : public FrameworkElement {
    AERO_DECLARE_TYPE(MediaElement, FrameworkElement)
public:
    MediaElement() noexcept : FrameworkElement(StaticTypeId()) {}
    ~MediaElement() override;

    String GetSource() const noexcept {
        return GetValueOr(SourceProperty, String{});
    }
    void SetSource(StringView value) noexcept;
    Stretch GetStretch() const noexcept {
        return GetValueOr(StretchProperty, Stretch::Uniform);
    }
    void SetStretch(Stretch value) noexcept {
        SetValue(StretchProperty, value);
    }
    StretchDirection GetStretchDirection() const noexcept {
        return GetValueOr(StretchDirectionProperty, StretchDirection::Both);
    }
    void SetStretchDirection(StretchDirection value) noexcept {
        SetValue(StretchDirectionProperty, value);
    }
    MediaState GetLoadedBehavior() const noexcept {
        return GetValueOr(LoadedBehaviorProperty, MediaState::Play);
    }
    void SetLoadedBehavior(MediaState value) noexcept {
        SetValue(LoadedBehaviorProperty, value);
    }
    MediaState GetUnloadedBehavior() const noexcept {
        return GetValueOr(UnloadedBehaviorProperty, MediaState::Close);
    }
    void SetUnloadedBehavior(MediaState value) noexcept {
        SetValue(UnloadedBehaviorProperty, value);
    }
    bool GetIsMuted() const noexcept {
        return GetValueOr(IsMutedProperty, false);
    }
    void SetIsMuted(bool value) noexcept {
        SetValue(IsMutedProperty, value);
    }
    double GetVolume() const noexcept {
        return GetValueOr(VolumeProperty, 0.5);
    }
    void SetVolume(double value) noexcept {
        SetValue(VolumeProperty, value);
    }
    double GetBalance() const noexcept {
        return GetValueOr(BalanceProperty, 0.0);
    }
    void SetBalance(double value) noexcept {
        SetValue(BalanceProperty, value);
    }
    bool GetScrubbingEnabled() const noexcept {
        return GetValueOr(ScrubbingEnabledProperty, false);
    }
    void SetScrubbingEnabled(bool value) noexcept {
        SetValue(ScrubbingEnabledProperty, value);
    }

    // Media playback control. Video decoding is not wired to a media
    // provider yet, so the operations are accepted without playback.
    void Play() noexcept;
    void Pause() noexcept;
    void Stop() noexcept;
    void Close() noexcept;

    inline static constexpr RoutedEvent<RoutedEventArgs> BufferingEndedEvent{"BufferingEnded"};
    inline static constexpr RoutedEvent<RoutedEventArgs> BufferingStartedEvent{"BufferingStarted"};
    inline static constexpr RoutedEvent<RoutedEventArgs> MediaEndedEvent{"MediaEnded"};
    inline static constexpr RoutedEvent<RoutedEventArgs> MediaFailedEvent{"MediaFailed"};
    inline static constexpr RoutedEvent<RoutedEventArgs> MediaOpenedEvent{"MediaOpened"};

    inline static constexpr DependencyProperty<String> SourceProperty{"Source"};
    inline static constexpr DependencyProperty<Stretch> StretchProperty{"Stretch"};
    inline static constexpr DependencyProperty<StretchDirection> StretchDirectionProperty{"StretchDirection"};
    inline static constexpr DependencyProperty<MediaState> LoadedBehaviorProperty{"LoadedBehavior"};
    inline static constexpr DependencyProperty<MediaState> UnloadedBehaviorProperty{"UnloadedBehavior"};
    inline static constexpr DependencyProperty<bool> IsMutedProperty{"IsMuted"};
    inline static constexpr DependencyProperty<double> VolumeProperty{"Volume"};
    inline static constexpr DependencyProperty<double> BalanceProperty{"Balance"};
    inline static constexpr DependencyProperty<bool> ScrubbingEnabledProperty{"ScrubbingEnabled"};
};

} // namespace Aero::Media

AERO_DECLARE_TYPE_ENUM(Aero::Media::MediaState)
