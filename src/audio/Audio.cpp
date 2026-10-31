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

bool Engine::Initialize() noexcept {
    if (state_ != nullptr) return true;
    EngineState* created = new (std::nothrow) EngineState();
    if (created == nullptr) {
        return false;
    }
#if AERO_AUDIO_MINIAUDIO
    if (ma_engine_init(nullptr, &created->engine) != MA_SUCCESS) {
        delete created;
        return false;
    }
    created->engineInitialized = true;
    if (ma_sound_group_init(
            &created->engine, 0U, nullptr,
            &created->musicGroup) != MA_SUCCESS) {
        ma_engine_uninit(&created->engine);
        delete created;
        return false;
    }
    created->musicGroupInitialized = true;
    if (ma_sound_group_init(
            &created->engine, 0U, nullptr,
            &created->effectsGroup) != MA_SUCCESS) {
        ma_sound_group_uninit(&created->musicGroup);
        ma_engine_uninit(&created->engine);
        delete created;
        return false;
    }
    created->effectsGroupInitialized = true;
    state_ = created;
    return true;
#else
    delete created;
    return false;
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

void Engine::PlayMusic(
    Base::StringView filePath) noexcept {
    if (filePath.Empty()) {
        return;
    }
    if (!Initialize()) return;
    auto& state = *static_cast<EngineState*>(state_);
#if AERO_AUDIO_MINIAUDIO
    if (state.musicInitialized) {
        ma_sound_uninit(&state.music);
        state.musicInitialized = false;
    }
    if (!state.musicPath.Assign(filePath)) return;
    if (ma_sound_init_from_file(
            &state.engine, state.musicPath.CStr(),
            MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION,
            &state.musicGroup, nullptr, &state.music) != MA_SUCCESS) {
        return;
    }
    state.musicInitialized = true;
    ma_sound_set_looping(&state.music, MA_TRUE);
    if (ma_sound_start(&state.music) != MA_SUCCESS) {
        ma_sound_uninit(&state.music);
        state.musicInitialized = false;
        return;
    }
#else
    (void)state;
#endif
}

void Engine::StopMusic() noexcept {
    auto* state = static_cast<EngineState*>(state_);
    if (state == nullptr) return;
#if AERO_AUDIO_MINIAUDIO
    if (state->musicInitialized) {
        ma_sound_stop(&state->music);
        ma_sound_uninit(&state->music);
        state->musicInitialized = false;
    }
#endif
}

void Engine::PlayEffect(
    Base::StringView filePath) noexcept {
    if (filePath.Empty()) {
        return;
    }
    if (!Initialize()) return;
    auto& state = *static_cast<EngineState*>(state_);
#if AERO_AUDIO_MINIAUDIO
    Base::String copiedPath;
    if (!copiedPath.Assign(filePath)) return;
    static_cast<void>(ma_engine_play_sound(
        &state.engine, copiedPath.CStr(),
        &state.effectsGroup));
#else
    (void)state;
#endif
}

} // namespace Aero::Audio
