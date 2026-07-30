#pragma once

#include <Aero/Controls/Items.hpp>
#include <Aero/Metadata.hpp>
#include <Aero/Module.hpp>

namespace Aero::Samples::Localization {

class Language final : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(Language, Base::Object, "clr-namespace:Localization", "Language")
public:
    Language() noexcept = default;
    Core::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::StringView Name() const noexcept { return name_.View(); }
    Base::Result<void> SetName(Base::StringView value) noexcept { return name_.TryAssign(value); }
    Base::StringView SelectLabel() const noexcept { return selectLabel_.View(); }
    Base::Result<void> SetSelectLabel(Base::StringView value) noexcept { return selectLabel_.TryAssign(value); }
    Base::StringView SoundLabel() const noexcept { return soundLabel_.View(); }
    Base::Result<void> SetSoundLabel(Base::StringView value) noexcept { return soundLabel_.TryAssign(value); }
    Base::StringView MusicLabel() const noexcept { return musicLabel_.View(); }
    Base::Result<void> SetMusicLabel(Base::StringView value) noexcept { return musicLabel_.TryAssign(value); }
    Base::StringView TitleLabel() const noexcept { return titleLabel_.View(); }
    Base::Result<void> SetTitleLabel(Base::StringView value) noexcept { return titleLabel_.TryAssign(value); }
    Base::StringView Description() const noexcept { return description_.View(); }
    Base::Result<void> SetDescription(Base::StringView value) noexcept { return description_.TryAssign(value); }
private:
    Base::String name_, selectLabel_, soundLabel_, musicLabel_, titleLabel_, description_;
};

class ViewModel final : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(ViewModel, Base::Object, "clr-namespace:Localization", "ViewModel")
public:
    ViewModel() noexcept = default;
    Core::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::Ref<Controls::ObjectItemsSource> Languages() const noexcept { return languages_; }
    void SetLanguages(Base::Ref<Controls::ObjectItemsSource> value) noexcept;
    Base::Ref<Language> SelectedLanguage() const noexcept { return selectedLanguage_; }
    void SetSelectedLanguage(Base::Ref<Language> value) noexcept;
    double SoundLevel() const noexcept { return soundLevel_; }
    void SetSoundLevel(double value) noexcept;
    double MusicLevel() const noexcept { return musicLevel_; }
    void SetMusicLevel(double value) noexcept;
    Base::Result<std::uint64_t> Subscribe(Core::MetadataPropertyChangedCallback callback, void* context) noexcept;
    Base::Result<bool> Unsubscribe(std::uint64_t token) noexcept;
private:
    struct Subscriber final { std::uint64_t token = 0U; Core::MetadataPropertyChangedCallback callback = nullptr; void* context = nullptr; };
    void Notify(Base::StringView property) noexcept;
    static Base::Result<std::uint64_t> SubscribePropertyChanged(Base::Object& object, Core::MetadataPropertyChangedCallback callback, void* callbackContext, void*) noexcept;
    static Base::Result<bool> UnsubscribePropertyChanged(Base::Object& object, std::uint64_t subscription, void*) noexcept;
    Base::Ref<Controls::ObjectItemsSource> languages_;
    Base::Ref<Language> selectedLanguage_;
    Base::Vector<Subscriber> subscribers_;
    double soundLevel_ = 100.0;
    double musicLevel_ = 70.0;
    std::uint64_t nextSubscriberToken_ = 1U;
    friend ModuleRegistration MakeLocalizationModuleManifest() noexcept;
};

Base::Result<Base::Ref<ViewModel>> CreateLocalizationViewModel() noexcept;
ModuleRegistration MakeLocalizationModuleManifest() noexcept;

} // namespace Aero::Samples::Localization
