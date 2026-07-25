#include <Aero/Platform/Ime.hpp>

#include <Aero/Base/String.hpp>

#include <cstdio>

namespace {

using namespace Aero::Base;
using namespace Aero::Platform;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf( \
                stderr, \
                "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

class CompositionClient final
    : public ITextCompositionClient {
public:
    Result<void> BeginComposition() noexcept override {
        ++beginCount;
        composing = true;
        return {};
    }

    Result<void> UpdateComposition(
        StringView text) noexcept override {
        ++updateCount;
        return lastText.TryAssign(text);
    }

    Result<void> CommitComposition(
        StringView text) noexcept override {
        ++commitCount;
        composing = false;
        return lastText.TryAssign(text);
    }

    Result<void> CancelComposition() noexcept override {
        ++cancelCount;
        composing = false;
        return {};
    }

    String lastText;
    std::uint32_t beginCount = 0U;
    std::uint32_t updateCount = 0U;
    std::uint32_t commitCount = 0U;
    std::uint32_t cancelCount = 0U;
    bool composing = false;
};

bool TestPlatformContract() noexcept {
    Win32ImeAdapter adapter;
    CompositionClient client;

#if defined(_WIN32)
    Result<void> beforeAttach =
        adapter.SetClient(&client);
    CHECK(!beforeAttach);
    CHECK(beforeAttach.GetStatus().code ==
        ErrorCode::NotInitialized);

    // These lifecycle messages do not dereference the opaque window.
    CHECK(adapter.Attach(
        reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(1U))));
    CHECK(adapter.SetClient(&client));
    CHECK(adapter.HandleMessage(
        0x010DU, 0U, 0).Value());
    CHECK(adapter.IsComposing());
    CHECK(client.beginCount == 1U);
    CHECK(adapter.HandleMessage(
        0x010EU, 0U, 0).Value());
    CHECK(!adapter.IsComposing());
    CHECK(client.cancelCount == 1U);
    CHECK(adapter.SetClient(nullptr));
    CHECK(adapter.Detach().Value());
    CHECK(!adapter.IsAttached());
#else
    Result<void> attached =
        adapter.Attach(
            reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(1U)));
    CHECK(!attached);
    CHECK(attached.GetStatus().code ==
        ErrorCode::Unsupported);
    Result<void> clientResult =
        adapter.SetClient(&client);
    CHECK(!clientResult);
    CHECK(clientResult.GetStatus().code ==
        ErrorCode::Unsupported);
    Result<bool> message =
        adapter.HandleMessage(
            0x010DU, 0U, 0);
    CHECK(!message);
    CHECK(message.GetStatus().code ==
        ErrorCode::Unsupported);
    CHECK(!adapter.Detach().Value());
#endif
    return true;
}

} // namespace

int main() {
    if (!TestPlatformContract()) {
        return 1;
    }
    std::puts("IME tests passed");
    return 0;
}
