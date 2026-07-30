#include "GalleryRuntime.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

using namespace Aero::Base;
using namespace Aero::Samples::ControlGallery;

struct Options final {
    const char* assetDirectory =
        AERO_CONTROL_GALLERY_ASSET_DIR;
    const char* page = "home";
    const char* report = nullptr;
    GalleryScenario scenario =
        GalleryScenario::Smoke;
    std::uint32_t frameCount = 1U;
    std::uint32_t cycles = 1U;
    bool runtime = true;
    bool compiled = true;
    bool light = true;
    bool dark = true;
    bool batchingEnabled = true;
};

struct RunRecord final {
    GallerySnapshot snapshot;
    std::string page;
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

bool IsPage(const char* value) noexcept {
    const char* pages[] = {
        "all", "home", "layout", "canvas", "stackpanel",
        "wrappanel", "dockpanel", "grid", "uniformgrid",
        "content", "buttons",
        "button", "repeatbutton", "togglebutton", "checkbox",
        "radiobutton", "text", "textblock", "textbox",
        "passwordbox", "hyperlink", "range", "slider",
        "progressbar", "groupbox", "expander", "items",
        "itemscontrol", "combobox", "listbox", "listview",
        "treeview", "tabcontrol", "scrolling", "scrollviewer",
        "menu", "contextmenu", "toolbar", "statusbar",
        "tooltip", "image", "brushes", "effects", "blending",
        "animation"};
    for (const char* page : pages) {
        if (Equal(value, page)) return true;
    }
    return false;
}

bool ParseScenario(
    const char* value,
    GalleryScenario& output) noexcept {
    if (Equal(value, "smoke")) {
        output = GalleryScenario::Smoke;
    } else if (Equal(value, "interaction")) {
        output = GalleryScenario::Interaction;
    } else if (Equal(value, "scroll")) {
        output = GalleryScenario::Scroll;
    } else if (Equal(value, "batch")) {
        output = GalleryScenario::Batch;
    } else {
        return false;
    }
    return true;
}

const char* ScenarioName(
    GalleryScenario scenario) noexcept {
    switch (scenario) {
    case GalleryScenario::Interaction:
        return "interaction";
    case GalleryScenario::Scroll:
        return "scroll";
    case GalleryScenario::Batch:
        return "batch";
    case GalleryScenario::Smoke:
    default:
        return "smoke";
    }
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
                       "--page=")) {
            options.page = argument + 7;
        } else if (StartsWith(
                       argument,
                       "--scenario=")) {
            if (!ParseScenario(
                    argument + 11,
                    options.scenario)) {
                return false;
            }
        } else if (StartsWith(
                       argument,
                       "--frames=")) {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(
                argument + 9, &end, 10);
            if (end == argument + 9 ||
                *end != '\0' ||
                parsed == 0UL ||
                parsed > UINT32_MAX) {
                return false;
            }
            options.frameCount =
                static_cast<std::uint32_t>(parsed);
        } else if (StartsWith(
                       argument,
                       "--cycles=")) {
            char* end = nullptr;
            const unsigned long parsed =
                std::strtoul(
                    argument + 9, &end, 10);
            if (end == argument + 9 ||
                *end != '\0' ||
                parsed == 0UL ||
                parsed > UINT32_MAX) {
                return false;
            }
            options.cycles =
                static_cast<std::uint32_t>(
                    parsed);
        } else if (StartsWith(
                       argument,
                       "--report=")) {
            options.report = argument + 9;
        } else if (Equal(
                       argument,
                       "--batching=on")) {
            options.batchingEnabled = true;
        } else if (Equal(
                       argument,
                       "--batching=off")) {
            options.batchingEnabled = false;
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
        } else {
            return false;
        }
    }
    return options.assetDirectory != nullptr &&
        options.assetDirectory[0] != '\0' &&
        options.page != nullptr &&
        IsPage(options.page) &&
        (options.report == nullptr ||
         options.report[0] != '\0');
}

int Fail(Status status) noexcept {
    std::fprintf(
        stderr,
        "AeroControlGallery: %s\n",
        status.message != nullptr
            ? status.message
            : "operation failed");
    return status.code == ErrorCode::Unsupported
        ? 77
        : 1;
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
    Result<void> batching =
        runtime.SetBatchingEnabledForTesting(
            options.batchingEnabled);
    if (!batching) {
        return Fail(batching.GetStatus());
    }
    Result<void> page = runtime.SelectPage(
        StringView(
            options.page,
            static_cast<std::uint32_t>(
                std::strlen(options.page))));
    if (!page) {
        return Fail(page.GetStatus());
    }
    Result<void> scenario = runtime.RunScenario(
        options.scenario,
        options.frameCount);
    if (!scenario) {
        return Fail(scenario.GetStatus());
    }
    output = runtime.Snapshot();
    std::printf(
        "ControlGallery xaml=%s theme=%s hash=0x%016llX "
        "nodes=%u commands=%u "
        "glyph=%u "
        "names=%u items=%u realized=%u "
        "created=%u frames=%u page=%s scenario=%s\n",
        mode == GalleryLoadMode::Runtime
            ? "runtime"
            : "compiled",
        theme == GalleryTheme::Light
            ? "light"
            : "dark",
        static_cast<unsigned long long>(
            output.planHash),
        output.nodeCount,
        output.commandCount,
        output.textCommandCount,
        output.namedObjectCount,
        output.itemCount,
        output.realizedItemCount,
        output.createdContainerCount,
        output.executedFrameCount,
        options.page,
        ScenarioName(options.scenario));
    return 0;
}

bool WriteReport(
    const Options& options,
    const std::vector<RunRecord>& records) {
    if (options.report == nullptr) return true;
    std::ofstream stream(
        options.report,
        std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    stream << "{\"schemaVersion\":1,\"cycles\":"
           << options.cycles
           << ",\"runs\":[";
    for (std::size_t index = 0U;
         index < records.size();
         ++index) {
        if (index != 0U) stream << ',';
        const RunRecord& record = records[index];
        const GallerySnapshot& snapshot =
            record.snapshot;
        stream
            << "{\"page\":\"" << record.page
            << "\",\"scenario\":\""
            << ScenarioName(snapshot.scenario)
            << "\",\"xaml\":\""
            << (snapshot.loadMode ==
                    GalleryLoadMode::Runtime
                ? "runtime"
                : "compiled")
            << "\",\"theme\":\""
            << (snapshot.theme == GalleryTheme::Light
                ? "light"
                : "dark")
            << "\",\"planHash\":"
            << snapshot.planHash
            << ",\"nodeCount\":"
            << snapshot.nodeCount
            << ",\"commandCount\":"
            << snapshot.commandCount
            << ",\"glyphCommandCount\":"
            << snapshot.textCommandCount
            << ",\"renderFrameStatistics\":{"
            << "\"sourceCommandCount\":"
            << snapshot.commandCount
            << ",\"drawPacketCount\":"
            << snapshot.drawPacketCount
            << ",\"batchCount\":"
            << snapshot.batchCount
            << ",\"drawCallCount\":"
            << snapshot.drawCallCount
            << ",\"mergedPacketCount\":"
            << snapshot.mergedPacketCount
            << ",\"barrierCount\":"
            << snapshot.barrierCount
            << ",\"instanceCount\":"
            << snapshot.instanceCount
            << ",\"stateBindingCount\":"
            << snapshot.stateBindingCount
            << ",\"batchingEnabled\":"
            << (snapshot.batchingEnabled
                ? "true" : "false")
            << '}'
            << ",\"layout\":{"
            << "\"passVersion\":"
            << snapshot.layoutPassVersion
            << ",\"measuredCount\":"
            << snapshot.measuredCount
            << ",\"arrangedCount\":"
            << snapshot.arrangedCount
            << ",\"pendingMeasureCount\":"
            << snapshot.pendingMeasureCount
            << ",\"pendingArrangeCount\":"
            << snapshot.pendingArrangeCount
            << '}'
            << ",\"namedObjectCount\":"
            << snapshot.namedObjectCount
            << ",\"itemCount\":"
            << snapshot.itemCount
            << ",\"realizedItemCount\":"
            << snapshot.realizedItemCount
            << ",\"createdContainerCount\":"
            << snapshot.createdContainerCount
            << ",\"recycledContainerUseCount\":"
            << snapshot.recycledContainerUseCount
            << ",\"executedFrameCount\":"
            << snapshot.executedFrameCount
            << '}';
    }
    stream << "]}\n";
    return static_cast<bool>(stream);
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!Parse(argc, argv, options)) {
        std::fprintf(
            stderr,
            "usage: AeroControlGallery "
            "[--assets=DIR] "
        "[--page=all|home|layout|canvas|stackpanel|wrappanel|dockpanel|grid|uniformgrid|content|buttons|button|repeatbutton|togglebutton|checkbox|radiobutton|text|textblock|textbox|passwordbox|hyperlink|range|slider|progressbar|groupbox|expander|items|itemscontrol|combobox|listbox|listview|treeview|tabcontrol|scrolling|scrollviewer|menu|contextmenu|toolbar|statusbar|tooltip|image|brushes|effects|blending|animation] "
            "[--scenario=smoke|interaction|scroll|batch] "
            "[--frames=N] "
            "[--cycles=N] "
            "[--report=FILE] "
            "[--batching=on|off] "
            "[--xaml=runtime|compiled|both] "
            "[--theme=light|dark|both]\n");
        return 2;
    }

    std::vector<RunRecord> records;
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
            std::uint64_t stableHash = 0U;
            for (std::uint32_t cycle = 0U;
                 cycle < options.cycles;
                 ++cycle) {
                const int result =
                    RunCombination(
                        options,
                        GalleryLoadMode::Runtime,
                        theme,
                        runtimeSnapshot);
                if (result != 0) {
                    return result;
                }
                if (cycle == 0U) {
                    stableHash =
                        runtimeSnapshot.planHash;
                } else if (
                    stableHash !=
                    runtimeSnapshot.planHash) {
                    std::fprintf(
                        stderr,
                        "AeroControlGallery: runtime lifecycle cycle diverged\n");
                    return 1;
                }
            }
            records.push_back(
                {runtimeSnapshot,
                 options.page});
        }
        if (options.compiled) {
            std::uint64_t stableHash = 0U;
            for (std::uint32_t cycle = 0U;
                 cycle < options.cycles;
                 ++cycle) {
                const int result =
                    RunCombination(
                        options,
                        GalleryLoadMode::Compiled,
                        theme,
                        compiledSnapshot);
                if (result != 0) {
                    return result;
                }
                if (cycle == 0U) {
                    stableHash =
                        compiledSnapshot.planHash;
                } else if (
                    stableHash !=
                    compiledSnapshot.planHash) {
                    std::fprintf(
                        stderr,
                        "AeroControlGallery: compiled lifecycle cycle diverged\n");
                    return 1;
                }
            }
            records.push_back(
                {compiledSnapshot,
                 options.page});
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
    if (!WriteReport(options, records)) {
        std::fprintf(
            stderr,
            "AeroControlGallery: failed to write report\n");
        return 1;
    }
    std::puts(
        "Aero ControlGallery completed");
    return 0;
}
