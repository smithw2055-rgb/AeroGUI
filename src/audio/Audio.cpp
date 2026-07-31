#include <Aero/Audio/Audio.hpp>
#include <Aero/Base/String.hpp>

#include <new>

#if AERO_AUDIO_MINIAUDIO
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <miniaudio.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif

namespace Aero::Audio {
namespace {

Base::Status AudioFailure(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

Base::Result<void> ValidateVolume(float value) noexcept {
    if (value < 0.0F || value > 1.0F) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Audio volume must be between zero and one");
    }
    return {};
}

} // namespace

struct Engine::Impl final {
#if AERO_AUDIO_MINIAUDIO
    ma_engine engine{};
    ma_sound_group musicGroup{};
    ma_sound_group effectsGroup{};
    ma_sound music{};
    bool engineInitialized = false;
    bool musicGroupInitialized = false;
    bool effectsGroupInitialized = false;
    bool musicInitialized = false;
#endif
    Base::String musicPath;
};

Engine::Engine() noexcept = default;

Engine::~Engine() noexcept {
    Shutdown();
}

Base::Result<void> Engine::Initialize() noexcept {
    if (impl_ != nullptr) return {};
    Impl* created = new (std::nothrow) Impl();
    if (created == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Audio engine allocation failed");
    }
#if AERO_AUDIO_MINIAUDIO
    if (ma_engine_init(nullptr, &created->engine) != MA_SUCCESS) {
        delete created;
        return AudioFailure("Audio device initialization failed");
    }
    created->engineInitialized = true;
    if (ma_sound_group_init(
            &created->engine, 0U, nullptr,
            &created->musicGroup) != MA_SUCCESS) {
        ma_engine_uninit(&created->engine);
        delete created;
        return AudioFailure("Music sound-group initialization failed");
    }
    created->musicGroupInitialized = true;
    if (ma_sound_group_init(
            &created->engine, 0U, nullptr,
            &created->effectsGroup) != MA_SUCCESS) {
        ma_sound_group_uninit(&created->musicGroup);
        ma_engine_uninit(&created->engine);
        delete created;
        return AudioFailure("Effects sound-group initialization failed");
    }
    created->effectsGroupInitialized = true;
    impl_ = created;
    return {};
#else
    delete created;
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "AeroGUI was built without the miniAudio provider");
#endif
}

void Engine::Shutdown() noexcept {
    if (impl_ == nullptr) return;
#if AERO_AUDIO_MINIAUDIO
    if (impl_->musicInitialized) {
        ma_sound_uninit(&impl_->music);
    }
    if (impl_->effectsGroupInitialized) {
        ma_sound_group_uninit(&impl_->effectsGroup);
    }
    if (impl_->musicGroupInitialized) {
        ma_sound_group_uninit(&impl_->musicGroup);
    }
    if (impl_->engineInitialized) {
        ma_engine_uninit(&impl_->engine);
    }
#endif
    delete impl_;
    impl_ = nullptr;
}

bool Engine::IsInitialized() const noexcept {
    return impl_ != nullptr;
}

Base::Result<void> Engine::SetMusicVolume(float value) noexcept {
    Base::Result<void> valid = ValidateVolume(value);
    if (!valid) return valid.GetStatus();
    if (impl_ == nullptr) return AudioFailure("Audio engine is not initialized");
#if AERO_AUDIO_MINIAUDIO
    ma_sound_group_set_volume(&impl_->musicGroup, value);
    return {};
#else
    return Base::Status::Failure(Base::ErrorCode::Unsupported,
        "AeroGUI was built without the miniAudio provider");
#endif
}

Base::Result<void> Engine::SetEffectsVolume(float value) noexcept {
    Base::Result<void> valid = ValidateVolume(value);
    if (!valid) return valid.GetStatus();
    if (impl_ == nullptr) return AudioFailure("Audio engine is not initialized");
#if AERO_AUDIO_MINIAUDIO
    ma_sound_group_set_volume(&impl_->effectsGroup, value);
    return {};
#else
    return Base::Status::Failure(Base::ErrorCode::Unsupported,
        "AeroGUI was built without the miniAudio provider");
#endif
}

Base::Result<void> Engine::PlayMusic(
    Base::StringView filePath) noexcept {
    if (filePath.Empty()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Music file path is empty");
    }
    Base::Result<void> initialized = Initialize();
    if (!initialized) return initialized.GetStatus();
#if AERO_AUDIO_MINIAUDIO
    if (impl_->musicInitialized) {
        ma_sound_uninit(&impl_->music);
        impl_->musicInitialized = false;
    }
    Base::Result<void> copied = impl_->musicPath.TryAssign(filePath);
    if (!copied) return copied.GetStatus();
    if (ma_sound_init_from_file(
            &impl_->engine, impl_->musicPath.CStr(),
            MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION,
            &impl_->musicGroup, nullptr, &impl_->music) != MA_SUCCESS) {
        return AudioFailure("Music file could not be opened or decoded");
    }
    impl_->musicInitialized = true;
    ma_sound_set_looping(&impl_->music, MA_TRUE);
    if (ma_sound_start(&impl_->music) != MA_SUCCESS) {
        ma_sound_uninit(&impl_->music);
        impl_->musicInitialized = false;
        return AudioFailure("Music playback could not be started");
    }
    return {};
#else
    return Base::Status::Failure(Base::ErrorCode::Unsupported,
        "AeroGUI was built without the miniAudio provider");
#endif
}

Base::Result<void> Engine::StopMusic() noexcept {
    if (impl_ == nullptr) return {};
#if AERO_AUDIO_MINIAUDIO
    if (impl_->musicInitialized) {
        ma_sound_stop(&impl_->music);
        ma_sound_uninit(&impl_->music);
        impl_->musicInitialized = false;
    }
#endif
    return {};
}

Base::Result<void> Engine::PlayEffect(
    Base::StringView filePath) noexcept {
    if (filePath.Empty()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Effect file path is empty");
    }
    Base::Result<void> initialized = Initialize();
    if (!initialized) return initialized.GetStatus();
#if AERO_AUDIO_MINIAUDIO
    Base::String copiedPath;
    Base::Result<void> copied = copiedPath.TryAssign(filePath);
    if (!copied) return copied.GetStatus();
    return ma_engine_play_sound(
        &impl_->engine, copiedPath.CStr(),
        &impl_->effectsGroup) == MA_SUCCESS
        ? Base::Result<void>()
        : Base::Result<void>(AudioFailure(
            "Effect file could not be opened or decoded"));
#else
    return Base::Status::Failure(Base::ErrorCode::Unsupported,
        "AeroGUI was built without the miniAudio provider");
#endif
}

} // namespace Aero::Audio
