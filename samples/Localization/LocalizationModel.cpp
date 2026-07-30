#include "LocalizationModel.hpp"

#include <Aero/Core/Metadata/TypeRegistry.hpp>

#include <cmath>

namespace Aero::Samples::Localization {
namespace {

Base::Result<void> Configure(Language& language, Base::StringView name, Base::StringView select, Base::StringView sound, Base::StringView music, Base::StringView title, Base::StringView description) noexcept {
    Base::Result<void> status = language.SetName(name);
    if (!status) return status.GetStatus();
    status = language.SetSelectLabel(select); if (!status) return status.GetStatus();
    status = language.SetSoundLabel(sound); if (!status) return status.GetStatus();
    status = language.SetMusicLabel(music); if (!status) return status.GetStatus();
    status = language.SetTitleLabel(title); if (!status) return status.GetStatus();
    return language.SetDescription(description);
}

Base::Result<Base::Ref<Language>> MakeLanguage(Base::StringView name, Base::StringView select, Base::StringView sound, Base::StringView music, Base::StringView title, Base::StringView description) noexcept {
    Base::Result<Base::Ref<Language>> made = Base::MakeRef<Language>();
    if (!made) return made.GetStatus();
    Base::Ref<Language> language = std::move(made).Value();
    Base::Result<void> status = Configure(*language, name, select, sound, music, title, description);
    return status ? Base::Result<Base::Ref<Language>>(std::move(language)) : Base::Result<Base::Ref<Language>>(status.GetStatus());
}

Base::Result<void> RegisterMetadata(Core::MetadataContext& context) noexcept {
    auto language = Describe<Language>(context);
    language.Property("Name", &Language::Name, &Language::SetName)
        .Property("SelectLabel", &Language::SelectLabel, &Language::SetSelectLabel)
        .Property("SoundLabel", &Language::SoundLabel, &Language::SetSoundLabel)
        .Property("MusicLabel", &Language::MusicLabel, &Language::SetMusicLabel)
        .Property("TitleLabel", &Language::TitleLabel, &Language::SetTitleLabel)
        .Property("Description", &Language::Description, &Language::SetDescription);
    Base::Result<void> status = language.Result();
    if (!status) return status.GetStatus();
    auto viewModel = Describe<ViewModel>(context);
    viewModel.Property("Languages", &ViewModel::Languages, &ViewModel::SetLanguages)
        .Property("SelectedLanguage", &ViewModel::SelectedLanguage, &ViewModel::SetSelectedLanguage)
        .Property("SoundLevel", &ViewModel::SoundLevel, &ViewModel::SetSoundLevel)
        .Property("MusicLevel", &ViewModel::MusicLevel, &ViewModel::SetMusicLevel)
        .PropertyChangeNotifications(&ViewModel::SubscribePropertyChanged, &ViewModel::UnsubscribePropertyChanged);
    return viewModel.Result();
}

} // namespace

void ViewModel::SetLanguages(Base::Ref<Controls::ObjectItemsSource> value) noexcept {
    languages_ = std::move(value); Notify("Languages");
}
void ViewModel::SetSelectedLanguage(Base::Ref<Language> value) noexcept {
    if (selectedLanguage_.Get() == value.Get()) return;
    selectedLanguage_ = std::move(value); Notify("SelectedLanguage");
}
void ViewModel::SetSoundLevel(double value) noexcept {
    if (std::fabs(soundLevel_ - value) < 0.0001) return;
    soundLevel_ = value; Notify("SoundLevel");
}
void ViewModel::SetMusicLevel(double value) noexcept {
    if (std::fabs(musicLevel_ - value) < 0.0001) return;
    musicLevel_ = value; Notify("MusicLevel");
}
Base::Result<std::uint64_t> ViewModel::Subscribe(Core::MetadataPropertyChangedCallback callback, void* context) noexcept {
    if (callback == nullptr) return UINT64_C(0);
    const std::uint64_t token = nextSubscriberToken_++;
    Base::Result<void> status = subscribers_.TryPushBack({token, callback, context});
    return status ? Base::Result<std::uint64_t>(token) : Base::Result<std::uint64_t>(status.GetStatus());
}
Base::Result<bool> ViewModel::Unsubscribe(std::uint64_t token) noexcept {
    for (Subscriber& subscriber : subscribers_) if (subscriber.token == token && subscriber.callback != nullptr) { subscriber.callback = nullptr; subscriber.context = nullptr; return true; }
    return false;
}
void ViewModel::Notify(Base::StringView property) noexcept {
    const Core::MemberId member = Core::MakeMemberId(StaticTypeId(), Core::MemberKind::Property, property);
    for (Subscriber& subscriber : subscribers_) if (subscriber.callback != nullptr) subscriber.callback(*this, member, subscriber.context);
}
Base::Result<std::uint64_t> ViewModel::SubscribePropertyChanged(Base::Object& object, Core::MetadataPropertyChangedCallback callback, void* callbackContext, void*) noexcept {
    if (object.RuntimeType() != StaticTypeId()) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Localization property subscription has the wrong object type");
    return static_cast<ViewModel&>(object).Subscribe(callback, callbackContext);
}
Base::Result<bool> ViewModel::UnsubscribePropertyChanged(Base::Object& object, std::uint64_t subscription, void*) noexcept {
    return object.RuntimeType() == StaticTypeId() ? static_cast<ViewModel&>(object).Unsubscribe(subscription) : Base::Result<bool>(false);
}

Base::Result<Base::Ref<ViewModel>> CreateLocalizationViewModel() noexcept {
    Base::Result<Base::Ref<ViewModel>> madeModel = Base::MakeRef<ViewModel>();
    if (!madeModel) return madeModel.GetStatus();
    Base::Ref<ViewModel> model = std::move(madeModel).Value();
    Base::Result<Base::Ref<Controls::ObjectItemsSource>> madeLanguages = Base::MakeRef<Controls::ObjectItemsSource>();
    if (!madeLanguages) return madeLanguages.GetStatus();
    Base::Ref<Controls::ObjectItemsSource> languages = std::move(madeLanguages).Value();
    const struct Text { Base::StringView name, select, sound, music, title, description; } texts[] = {
        {"English", "Select language", "Sound", "Music", "LOCALIZATION SAMPLE", "This sample uses a ViewModel, bindings, and a selected language object to refresh localized content at runtime."},
        {"Fran\xC3\xA7" "ais", "Choisir la langue", "Son", "Musique", "EXEMPLE DE LOCALISATION", "Cet exemple utilise un ViewModel, des liaisons et la langue sélectionnée pour actualiser le contenu pendant l'exécution."},
        {u8"日本語", u8"言語を選択する", u8"音", u8"音楽", u8"ローカリゼーション見本", u8"このサンプルは ViewModel とバインディングを使い、選択した言語に応じて実行中に表示内容を更新します。"},
        {u8"عربي", u8"اختار اللغة", u8"الصوت", u8"موسيقى", u8"نموذج الترجمة", u8"يستخدم هذا النموذج ViewModel والربط واللغة المختارة لتحديث المحتوى المترجم أثناء التشغيل。"},
    };
    Base::Ref<Language> first;
    for (const Text& text : texts) {
        Base::Result<Base::Ref<Language>> made = MakeLanguage(text.name, text.select, text.sound, text.music, text.title, text.description);
        if (!made) return made.GetStatus();
        Base::Ref<Language> language = std::move(made).Value();
        if (!first) first = language;
        Base::Result<void> added = languages->Add(Base::Ref<Base::Object>(language));
        if (!added) return added.GetStatus();
    }
    model->SetLanguages(std::move(languages));
    model->SetSelectedLanguage(std::move(first));
    return model;
}

ModuleRegistration MakeLocalizationModuleManifest() noexcept {
    return DefineModule("Aero.Samples.Localization", &RegisterMetadata);
}

} // namespace Aero::Samples::Localization
