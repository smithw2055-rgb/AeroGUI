#pragma once

#include <Aero/Audio/Audio.hpp>
#include <Aero/Base/Result.hpp>

#include <new>

namespace Aero::App {

// Optional application-framework services. These services belong to the App
// layer rather than the WPF Application object, so embedded/headless runtimes
// do not acquire platform devices merely by constructing Application.
class AERO_API Services final {
public:
    Services() noexcept = default;

    ~Services() noexcept {
        delete audio_;
        audio_ = nullptr;
    }

    Services(const Services&) = delete;
    Services& operator=(const Services&) = delete;

    Base::Result<Audio::Engine*> AudioEngine() noexcept {
        if (audio_ != nullptr) return audio_;

        Audio::Engine* created =
            new (std::nothrow) Audio::Engine();
        if (created == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Audio engine allocation failed");
        }
        audio_ = created;
        return audio_;
    }

private:
    Audio::Engine* audio_ = nullptr;
};

} // namespace Aero::App
