#include <AeroAudio/Audio.hpp>
#include <Aero/Base/String.hpp>

#include <new>

#if AERO_AUDIO_MINIAUDIO
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#include <miniaudio.h>
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

struct EngineState final {
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
    if (state_ != nullptr) return {};
    EngineState* created = new (std::nothrow) EngineState();
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
    state_ = created;
    return {};
#else
    delete created;
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "AeroGUI was built without the miniAudio provider");
#endif
}

void Engine::Shutdown() noexcept {
    auto* state = static_cast<EngineState*>(state_);
    if (state == nullptr) return;
#if AERO_AUDIO_MINIAUDIO
    if (state->musicInitialized) {
        ma_sound_uninit(&state->music);
    }
    if (state->effectsGroupInitialized) {
        ma_sound_group_uninit(&state->effectsGroup);
    }
    if (state->musicGroupInitialized) {
        ma_sound_group_uninit(&state->musicGroup);
    }
    if (state->engineInitialized) {
        ma_engine_uninit(&state->engine);
    }
#endif
    delete state;
    state_ = nullptr;
}

bool Engine::IsInitialized() const noexcept {
    return state_ != nullptr;
}

void Engine::SetMusicVolume(float value) noexcept {
    Base::Result<void> valid = ValidateVolume(value);
    auto* state = static_cast<EngineState*>(state_);
    if (!valid || state == nullptr) return;
#if AERO_AUDIO_MINIAUDIO
    ma_sound_group_set_volume(&state->musicGroup, value);
#else
    (void)value;
#endif
}

void Engine::SetEffectsVolume(float value) noexcept {
    Base::Result<void> valid = ValidateVolume(value);
    auto* state = static_cast<EngineState*>(state_);
    if (!valid || state == nullptr) return;
#if AERO_AUDIO_MINIAUDIO
    ma_sound_group_set_volume(&state->effectsGroup, value);
#else
    (void)value;
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
    auto& state = *static_cast<EngineState*>(state_);
#if AERO_AUDIO_MINIAUDIO
    if (state.musicInitialized) {
        ma_sound_uninit(&state.music);
        state.musicInitialized = false;
    }
    Base::Result<void> copied = state.musicPath.Assign(filePath);
    if (!copied) return copied.GetStatus();
    if (ma_sound_init_from_file(
            &state.engine, state.musicPath.CStr(),
            MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION,
            &state.musicGroup, nullptr, &state.music) != MA_SUCCESS) {
        return AudioFailure("Music file could not be opened or decoded");
    }
    state.musicInitialized = true;
    ma_sound_set_looping(&state.music, MA_TRUE);
    if (ma_sound_start(&state.music) != MA_SUCCESS) {
        ma_sound_uninit(&state.music);
        state.musicInitialized = false;
        return AudioFailure("Music playback could not be started");
    }
    return {};
#else
    return Base::Status::Failure(Base::ErrorCode::Unsupported,
        "AeroGUI was built without the miniAudio provider");
#endif
}

Base::Result<void> Engine::StopMusic() noexcept {
    auto* state = static_cast<EngineState*>(state_);
    if (state == nullptr) return {};
#if AERO_AUDIO_MINIAUDIO
    if (state->musicInitialized) {
        ma_sound_stop(&state->music);
        ma_sound_uninit(&state->music);
        state->musicInitialized = false;
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
    auto& state = *static_cast<EngineState*>(state_);
#if AERO_AUDIO_MINIAUDIO
    Base::String copiedPath;
    Base::Result<void> copied = copiedPath.Assign(filePath);
    if (!copied) return copied.GetStatus();
    return ma_engine_play_sound(
        &state.engine, copiedPath.CStr(),
        &state.effectsGroup) == MA_SUCCESS
        ? Base::Result<void>()
        : Base::Result<void>(AudioFailure(
            "Effect file could not be opened or decoded"));
#else
    return Base::Status::Failure(Base::ErrorCode::Unsupported,
        "AeroGUI was built without the miniAudio provider");
#endif
}

} // namespace Aero::Audio
