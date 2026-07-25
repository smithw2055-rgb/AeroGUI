#include <Aero/Platform/Clipboard.hpp>

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

bool TestMemoryClipboard() noexcept {
    MemoryClipboard clipboard;
    String text;
    CHECK(clipboard.ReadText(text));
    CHECK(text.Empty());
    CHECK(clipboard.Generation() == 0U);
    CHECK(clipboard.WriteText(
        StringView(u8"Latin 中文 👩‍💻")));
    CHECK(clipboard.Generation() == 1U);
    CHECK(clipboard.ReadText(text));
    CHECK(text == StringView(
        u8"Latin 中文 👩‍💻"));

    const char malformed[] = {
        static_cast<char>(0xC0U),
        static_cast<char>(0xAFU)};
    CHECK(!clipboard.WriteText(
        StringView(malformed, 2U)));
    CHECK(clipboard.Generation() == 1U);
    CHECK(clipboard.ReadText(text));
    CHECK(text == StringView(
        u8"Latin 中文 👩‍💻"));
    return true;
}

bool TestUnsupportedWin32Surface() noexcept {
#if !defined(_WIN32)
    Win32Clipboard clipboard;
    String text;
    Result<void> read = clipboard.ReadText(text);
    Result<void> write =
        clipboard.WriteText(StringView("text"));
    CHECK(!read);
    CHECK(read.GetStatus().code ==
        ErrorCode::Unsupported);
    CHECK(!write);
    CHECK(write.GetStatus().code ==
        ErrorCode::Unsupported);
#endif
    return true;
}

} // namespace

int main() {
    if (!TestMemoryClipboard()) return 1;
    if (!TestUnsupportedWin32Surface()) return 1;
    std::puts("Clipboard tests passed");
    return 0;
}
