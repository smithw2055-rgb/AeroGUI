#include "GalleryRuntime.hpp"

#include <cstdio>
#include <cstring>

namespace {

using namespace Aero::Base;
using namespace Aero::Samples::ControlGallery;

struct Options final {
    const char* assetDirectory =
        AERO_CONTROL_GALLERY_ASSET_DIR;
    const char* backend = "null";
    bool runtime = true;
    bool compiled = true;
    bool light = true;
    bool dark = true;
    bool contextLoss = false;
    bool interactive = false;
};

bool Equal(
    const char* left,
    const char* right) noexcept {
    return std::strcmp(left, right) == 0;
}

bool StartsWith(
    const char* value,
    const char* prefix) noexcept {
    return std::strncmp(
        value,
        prefix,
        std::strlen(prefix)) == 0;
}

bool Parse(
    int argc,
    char** argv,
    Options& options) noexcept {
    for (int index = 1;
         index < argc;
         ++index) {
        const char* argument = argv[index];
        if (StartsWith(
                argument, "--assets=")) {
            options.assetDirectory =
                argument + 9;
        } else if (StartsWith(
                       argument,
                       "--backend=")) {
            options.backend =
                argument + 10;
        } else if (Equal(
                       argument,
                       "--xaml=runtime")) {
            options.runtime = true;
            options.compiled = false;
        } else if (Equal(
                       argument,
                       "--xaml=compiled")) {
            options.runtime = false;
            options.compiled = true;
        } else if (Equal(
                       argument,
                       "--xaml=both")) {
            options.runtime = true;
            options.compiled = true;
        } else if (Equal(
                       argument,
                       "--theme=light")) {
            options.light = true;
            options.dark = false;
        } else if (Equal(
                       argument,
                       "--theme=dark")) {
            options.light = false;
            options.dark = true;
        } else if (Equal(
                       argument,
                       "--theme=both")) {
            options.light = true;
            options.dark = true;
        } else if (Equal(
                       argument,
                       "--simulate-context-loss")) {
            options.contextLoss = true;
        } else if (Equal(
                       argument,
                       "--interactive")) {
            options.interactive = true;
        } else {
            return false;
        }
    }
    return options.assetDirectory != nullptr &&
        options.assetDirectory[0] != '\0' &&
        options.backend != nullptr &&
        options.backend[0] != '\0';
}

int Fail(Status status) noexcept {
    std::fprintf(
        stderr,
        "AeroControlGallery: %s\n",
        status.message != nullptr
            ? status.message
            : "operation failed");
    return 1;
}

int RunCombination(
    const Options& options,
    GalleryLoadMode mode,
    GalleryTheme theme,
    GallerySnapshot& output) noexcept {
    GalleryRuntime runtime;
    Result<void> initialized =
        runtime.Initialize(
            StringView(
                options.assetDirectory,
                static_cast<std::uint32_t>(
                    std::strlen(
                        options.assetDirectory))),
            mode,
            theme);
    if (!initialized) {
        return Fail(
            initialized.GetStatus());
    }
    Result<void> presented =
        RunNativeGallery(
            runtime.Plan(),
            StringView(
                options.backend,
                static_cast<std::uint32_t>(
                    std::strlen(
                        options.backend))),
            options.contextLoss,
            options.interactive);
    if (!presented) {
        return Fail(
            presented.GetStatus());
    }
    output = runtime.Snapshot();
    std::printf(
        "ControlGallery xaml=%s theme=%s "
        "backend=%s hash=0x%016llX "
        "nodes=%u commands=%u "
        "names=%u items=%u realized=%u "
        "created=%u\n",
        mode == GalleryLoadMode::Runtime
            ? "runtime"
            : "compiled",
        theme == GalleryTheme::Light
            ? "light"
            : "dark",
        options.backend,
        static_cast<unsigned long long>(
            output.planHash),
        output.nodeCount,
        output.commandCount,
        output.namedObjectCount,
        output.itemCount,
        output.realizedItemCount,
        output.createdContainerCount);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!Parse(argc, argv, options)) {
        std::fprintf(
            stderr,
            "usage: AeroControlGallery "
            "[--assets=DIR] "
            "[--backend=null|d3d11|opengl] "
            "[--xaml=runtime|compiled|both] "
            "[--theme=light|dark|both] "
            "[--simulate-context-loss] "
            "[--interactive]\n");
        return 2;
    }

    const GalleryTheme themes[] = {
        GalleryTheme::Light,
        GalleryTheme::Dark};
    for (GalleryTheme theme : themes) {
        if ((theme == GalleryTheme::Light &&
             !options.light) ||
            (theme == GalleryTheme::Dark &&
             !options.dark)) {
            continue;
        }
        GallerySnapshot runtimeSnapshot;
        GallerySnapshot compiledSnapshot;
        if (options.runtime) {
            const int result = RunCombination(
                options,
                GalleryLoadMode::Runtime,
                theme,
                runtimeSnapshot);
            if (result != 0) {
                return result;
            }
        }
        if (options.compiled) {
            const int result = RunCombination(
                options,
                GalleryLoadMode::Compiled,
                theme,
                compiledSnapshot);
            if (result != 0) {
                return result;
            }
        }
        if (options.runtime &&
            options.compiled &&
            (runtimeSnapshot.planHash !=
                 compiledSnapshot.planHash ||
             runtimeSnapshot.nodeCount !=
                 compiledSnapshot.nodeCount ||
             runtimeSnapshot.commandCount !=
                 compiledSnapshot.commandCount ||
             runtimeSnapshot.
                     realizedItemCount !=
                 compiledSnapshot.
                     realizedItemCount)) {
            std::fprintf(
                stderr,
                "AeroControlGallery: runtime "
                "and compiled XAML diverged\n");
            return 1;
        }
    }
    std::puts(
        "Aero ControlGallery completed");
    return 0;
}
