#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>

namespace Aero::Audio {

// Public audio boundary. Backends and decoder types deliberately stay private
// so applications do not acquire a dependency on a particular audio library.
class AERO_API Engine final {
public:
    Engine() noexcept;
    ~Engine() noexcept;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept;

    void SetMusicVolume(float value) noexcept;
    void SetEffectsVolume(float value) noexcept;
    Base::Result<void> PlayMusic(
        Base::StringView filePath) noexcept;
    Base::Result<void> StopMusic() noexcept;
    Base::Result<void> PlayEffect(
        Base::StringView filePath) noexcept;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Audio
