#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>

namespace Aero::Audio {

// Public audio boundary. Backends and decoder types deliberately stay private
// so applications do not acquire a dependency on a particular audio library.
class AERO_AUDIO_API Engine {
public:
    Engine() noexcept;
    ~Engine() noexcept;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    Result<void> Initialize() noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept;

    void SetMusicVolume(float value) noexcept;
    void SetEffectsVolume(float value) noexcept;
    Result<void> PlayMusic(
        StringView filePath) noexcept;
    Result<void> StopMusic() noexcept;
    Result<void> PlayEffect(
        StringView filePath) noexcept;

private:
    void* state_ = nullptr;
};

} // namespace Aero::Audio
