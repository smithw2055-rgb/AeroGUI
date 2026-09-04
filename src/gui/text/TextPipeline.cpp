#include "render/DisplayList.hpp"
#include "TextPipeline.hpp"
#include "render/RenderResources.hpp"

#include <AeroRender/RenderDevice.hpp>
#include <Aero/FrameworkElement.hpp>
#include "gui/text/FontManager.hpp"
#include "gui/text/FreeTypeAdapter.hpp"
#include "gui/text/HarfBuzzAdapter.hpp"
#include "gui/text/TextLayout.hpp"

#include <cstdio>
#include <cmath>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <new>
#include <string>
#include <utility>

namespace Aero::Text {
using TextConfig = ::Aero::Render::TextConfig;
using TextResources = ::Aero::Render::TextResources;
namespace {

bool FileExists(const char* path) noexcept {
    if (path == nullptr || path[0] == '\0') return false;
    std::FILE* file = nullptr;
#if defined(_WIN32)
    if (fopen_s(&file, path, "rb") != 0) return false;
#else
    file = std::fopen(path, "rb");
    if (file == nullptr) return false;
#endif
    std::fclose(file);
    return true;
}

bool LooksLikeFontPath(Base::StringView value) noexcept {
    for (std::uint32_t index = 0U;
         index < value.SizeBytes(); ++index) {
        const char character = value.Data()[index];
        if (character == '/' ||
            character == '\\' ||
            character == '.') {
            return true;
        }
    }
    return false;
}

Base::Result<void> Assign(
    Base::String& destination,
    Base::StringView source) noexcept {
    return destination.Assign(source);
}

Base::StringView TrimFontFamilyCandidate(
    Base::StringView value) noexcept {
    std::uint32_t begin = 0U;
    std::uint32_t end = value.SizeBytes();
    while (begin < end &&
        std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    while (end > begin &&
        std::isspace(static_cast<unsigned char>(value[end - 1U])) != 0) {
        --end;
    }
    return value.Substr(begin, end - begin);
}

std::string NormalizedFontName(
    const std::string& value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const char rawCharacter : value) {
        const auto character =
            static_cast<unsigned char>(rawCharacter);
        if (std::isalnum(character) != 0) {
            normalized.push_back(
                static_cast<char>(
                    std::tolower(character)));
        }
    }
    return normalized;
}

std::string AbbreviatedPackFaceName(
    const std::string& normalized) {
    struct FaceSuffix {
        const char* name;
        const char* abbreviation;
    };
    constexpr FaceSuffix suffixes[] = {
        {"semilight", "l"},
        {"semibold", "b"}};
    for (const FaceSuffix& suffix : suffixes) {
        const std::size_t length = std::strlen(suffix.name);
        if (normalized.size() < length ||
            normalized.compare(
                normalized.size() - length,
                length,
                suffix.name) != 0) {
            continue;
        }
        std::string abbreviated =
            normalized.substr(0U, normalized.size() - length);
        abbreviated += suffix.abbreviation;
        return abbreviated;
    }
    return {};
}

bool IsSupportedFontFile(
    const std::filesystem::path& path) {
    std::string extension =
        path.extension().string();
    for (char& character : extension) {
        character = static_cast<char>(
            std::tolower(
                static_cast<unsigned char>(
                    character)));
    }
    return extension == ".ttf" ||
        extension == ".ttc" ||
        extension == ".otf";
}

Base::Result<bool> SelectPackFontPath(
    Base::StringView family,
    Base::StringView searchRoot,
    Base::String& output,
    bool bold = false,
    bool italic = false) noexcept {
    std::uint32_t hash = UINT32_MAX;
    for (std::uint32_t index = 0U;
         index < family.SizeBytes();
         ++index) {
        if (family[index] == '#') {
            hash = index;
            break;
        }
    }
    if (hash == UINT32_MAX ||
        hash + 1U >= family.SizeBytes()) {
        return false;
    }

    std::string directoryText(
        family.Data(), hash);
    while (!directoryText.empty() &&
        (directoryText.back() == '/' ||
         directoryText.back() == '\\')) {
        directoryText.pop_back();
    }
    const std::string requested(
        family.Data() + hash + 1U,
        family.SizeBytes() - hash - 1U);
    const std::string normalizedRequest =
        NormalizedFontName(requested);
    const std::string abbreviatedRequest =
        AbbreviatedPackFaceName(normalizedRequest);
    if (directoryText.empty() ||
        normalizedRequest.empty()) {
        return false;
    }

    std::filesystem::path directory =
        std::filesystem::u8path(directoryText);
    if (directory.is_relative() &&
        !searchRoot.Empty()) {
        directory =
            std::filesystem::u8path(std::string(
                searchRoot.Data(),
                searchRoot.SizeBytes())) /
            directory;
    }

    std::error_code error;
    if (!std::filesystem::is_directory(
            directory, error) ||
        error) {
        return false;
    }

    std::filesystem::path best;
    bool bestIsRegular = false;
    std::size_t bestPrefix = 0U;
    std::size_t bestDistance = SIZE_MAX;
    for (std::filesystem::directory_iterator iterator(
             directory, error), end;
         !error && iterator != end;
         iterator.increment(error)) {
        if (!iterator->is_regular_file(error) ||
            error ||
            !IsSupportedFontFile(
                iterator->path())) {
            continue;
        }
        const std::string candidate =
            NormalizedFontName(
                iterator->path().
                    stem().string());
        // A package FontFamily such as "./#PT Sans" names the family, not a
        // face. Pick its Regular face deterministically before using the
        // legacy closest-file fallback; otherwise PTSans-Bold happens to win
        // merely because its filename is shorter than PTSans-Regular.
        std::string preferred = normalizedRequest;
        if (bold && italic) preferred += "bolditalic";
        else if (bold) preferred += "bold";
        else if (italic) preferred += "italic";
        else preferred += "regular";
        const bool candidateIsRegular =
            candidate == preferred ||
            (!bold && !italic &&
             (candidate == normalizedRequest ||
              (!abbreviatedRequest.empty() &&
               candidate == abbreviatedRequest)));
        std::size_t prefix = 0U;
        while (prefix < candidate.size() &&
            prefix < normalizedRequest.size() &&
            candidate[prefix] ==
                normalizedRequest[prefix]) {
            ++prefix;
        }
        const std::size_t distance =
            candidate.size() >
                    normalizedRequest.size()
            ? candidate.size() -
                normalizedRequest.size()
            : normalizedRequest.size() -
                candidate.size();
        if (best.empty() ||
            (candidateIsRegular && !bestIsRegular) ||
            (candidateIsRegular == bestIsRegular &&
             (prefix > bestPrefix ||
              (prefix == bestPrefix &&
               distance < bestDistance)))) {
            best = iterator->path();
            bestIsRegular = candidateIsRegular;
            bestPrefix = prefix;
            bestDistance = distance;
        }
    }
    if (error || best.empty()) {
        return false;
    }
    // No filename prefix match (Fonts/#PT Root UI vs Muli/Caladea) still
    // yields a face from that directory so text measure is non-zero.
    static_cast<void>(bestPrefix);

    const std::string selected =
        best.string();
    Base::Result<void> assigned =
        output.Assign(Base::StringView(
            selected.data(),
            static_cast<std::uint32_t>(
                selected.size())));
    return assigned
        ? Base::Result<bool>(true)
        : Base::Result<bool>(
              assigned.GetStatus());
}

Base::Result<void> SelectFontPath(
    Base::StringView family,
    bool fallback,
    Base::String& output,
    Base::StringView searchRoot = {},
    bool bold = false,
    bool italic = false) noexcept {
    if (!family.Empty() && LooksLikeFontPath(family)) {
        Base::Result<bool> packed =
            SelectPackFontPath(
                family, searchRoot, output,
                bold, italic);
        if (!packed) return packed.GetStatus();
        if (packed.Value()) return {};

        std::string candidate(
            family.Data(), family.SizeBytes());
        if (FileExists(candidate.c_str())) {
            return Assign(output, family);
        }
        const bool absolute =
            family[0] == '/' ||
            family[0] == '\\' ||
            (family.SizeBytes() >= 2U &&
             family[1] == ':');
        if (!absolute && !searchRoot.Empty()) {
            Base::String combined(
                &output.Allocator());
            Base::Result<void> joined =
                combined.Assign(searchRoot);
            if (joined && !combined.Empty() &&
                combined.View()[
                    combined.SizeBytes() - 1U] != '/' &&
                combined.View()[
                    combined.SizeBytes() - 1U] != '\\') {
                joined = combined.Append(
                    Base::StringView("/"));
            }
            if (joined) {
                joined =
                    combined.Append(family);
            }
            if (!joined) {
                return joined.GetStatus();
            }
            std::string rooted(
                combined.CStr(),
                combined.SizeBytes());
            if (FileExists(rooted.c_str())) {
                return output.Assign(
                    combined.View());
            }
        }
        // Pack FontFamily such as Fonts/#PT Root UI is not a file path.
        // Fall through to family-name / system UI face substitution.
    }

#if defined(_WIN32)
    static constexpr const char* PrimaryCandidates[] = {
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf"};
    static constexpr const char* FallbackCandidates[] = {
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\msyhbd.ttc",
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\segoeui.ttf"};

    const char* configured = nullptr;
    if (!family.Empty()) {
        if (family == Base::StringView("Segoe UI")) {
            configured = "C:\\Windows\\Fonts\\segoeui.ttf";
        } else if (
            family == Base::StringView("Segoe UI Bold")) {
            configured = "C:\\Windows\\Fonts\\segoeuib.ttf";
        } else if (
            family == Base::StringView("Segoe UI Italic")) {
            configured = "C:\\Windows\\Fonts\\segoeuii.ttf";
        } else if (
            family ==
                Base::StringView("Segoe UI Bold Italic")) {
            configured = "C:\\Windows\\Fonts\\segoeuiz.ttf";
        } else if (family == Base::StringView("Arial")) {
            configured = "C:\\Windows\\Fonts\\arial.ttf";
        } else if (
            family == Base::StringView("Microsoft YaHei")) {
            configured = "C:\\Windows\\Fonts\\msyh.ttc";
        } else if (family == Base::StringView("SimHei")) {
            configured = "C:\\Windows\\Fonts\\simhei.ttf";
        }
    }
#else
    static constexpr const char* PrimaryCandidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        "/System/Library/Fonts/Helvetica.ttc"};
    static constexpr const char* FallbackCandidates[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/System/Library/Fonts/PingFang.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"};

    const char* configured = nullptr;
    if (!family.Empty()) {
        if (family == Base::StringView("DejaVu Sans")) {
            configured =
                "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
        } else if (
            family == Base::StringView("Segoe UI Bold")) {
            configured =
                "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf";
        } else if (
            family == Base::StringView("Segoe UI Italic")) {
            configured =
                "/usr/share/fonts/truetype/dejavu/DejaVuSans-Oblique.ttf";
        } else if (
            family ==
                Base::StringView("Segoe UI Bold Italic")) {
            configured =
                "/usr/share/fonts/truetype/dejavu/DejaVuSans-BoldOblique.ttf";
        } else if (
            family == Base::StringView("Liberation Sans")) {
            configured =
                "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf";
        } else if (
            family == Base::StringView("Noto Sans CJK")) {
            configured =
                "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc";
        } else if (
            family == Base::StringView("Helvetica")) {
            configured =
                "/System/Library/Fonts/Helvetica.ttc";
        } else if (
            family == Base::StringView("PingFang")) {
            configured =
                "/System/Library/Fonts/PingFang.ttc";
        }
    }
#endif

    if (!family.Empty() &&
        configured != nullptr &&
        FileExists(configured)) {
        return Assign(
            output,
            Base::StringView(
                configured,
                static_cast<std::uint32_t>(
                    std::strlen(configured))));
    }

    const char* const* candidates = fallback
        ? FallbackCandidates
        : PrimaryCandidates;
    const std::size_t count = fallback
        ? sizeof(FallbackCandidates) /
            sizeof(FallbackCandidates[0])
        : sizeof(PrimaryCandidates) /
            sizeof(PrimaryCandidates[0]);
    for (std::size_t index = 0U; index < count; ++index) {
        if (FileExists(candidates[index])) {
            return Assign(
                output,
                Base::StringView(
                    candidates[index],
                    static_cast<std::uint32_t>(
                        std::strlen(candidates[index]))));
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        fallback
            ? "No installed fallback font was found"
            : "No installed primary font was found");
}

Base::Result<void> ConfigureTypeface(
    Text::Typeface& typeface,
    Base::StringView family,
    Base::StringView language,
    Base::StringView defaultFamily) noexcept {
    Base::Result<void> assigned =
        typeface.SetFamily(
            family.Empty() ? defaultFamily : family);
    if (!assigned) return assigned.GetStatus();
    if (!language.Empty()) {
        assigned = typeface.SetLanguage(language);
    }
    return assigned;
}

class TextBlockLayoutProxy
    : public ::Aero::Controls::TextBlockLayout {
public:
    using FaceResolver =
        Base::Result<void> (*)(
            void* context,
            Base::StringView family,
            FontWeight weight,
            FontStyle style,
            Text::FontFace& primary,
            Base::Vector<Text::FontFace>& fallbacks) noexcept;

    void Set(
        ::Aero::Controls::TextBlockLayout* layout) noexcept {
        layout_ = layout;
    }

    void SetFaceResolver(
        void* context,
        FaceResolver resolver) noexcept {
        resolverContext_ = context;
        resolver_ = resolver;
    }

    Base::Result<void> ShapeAndPrepare(
        const ::Aero::Controls::TextLayoutRequest& request,
        ::Aero::Controls::TextLayoutResult& output) noexcept override {
        if (layout_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "View text resources are unavailable");
        }
        if (request.fontFamily.Empty()) {
            return layout_->ShapeAndPrepare(
                request, output);
        }
        if (resolver_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "View text font resolver is unavailable");
        }
        ::Aero::Controls::TextLayoutRequest selected = request;
        Base::Vector<Text::FontFace> fallbacks;
        Base::Result<void> resolved =
            resolver_(
                resolverContext_,
                request.fontFamily,
                request.fontWeight,
                request.fontStyle,
                selected.face,
                fallbacks);
        if (!resolved) {
            return resolved.GetStatus();
        }
        selected.fallbackFaces = fallbacks.AsSpan();
        return layout_->ShapeAndPrepare(
            selected, output);
    }

    void ReleaseGlyphRun(
        Render::RenderGlyphRunId glyphRun) noexcept override {
        if (layout_ != nullptr) {
            layout_->ReleaseGlyphRun(glyphRun);
        }
    }

private:
    ::Aero::Controls::TextBlockLayout* layout_ = nullptr;
    void* resolverContext_ = nullptr;
    FaceResolver resolver_ = nullptr;
};

class HeadlessTextBlockLayout
    : public ::Aero::Controls::TextBlockLayout {
public:
    HeadlessTextBlockLayout(
        Text::FontManager& fonts,
        const TextConfig& config,
        Base::IAllocator* allocator) noexcept
        : fonts_(&fonts),
          allocator_(allocator),
          fallbackFaces_(allocator) {
        config_ = config;
        Base::Result<void> copied =
            fallbackFaces_.Append(
                config.fallbackFaces);
        valid_ = static_cast<bool>(copied);
        config_.fallbackFaces =
            fallbackFaces_.AsSpan();
        nextGlyphRun_ = config.firstGlyphRunId;
    }

    bool IsValid() const noexcept {
        return valid_;
    }

    Base::Result<void> ShapeAndPrepare(
        const ::Aero::Controls::TextLayoutRequest& request,
        ::Aero::Controls::TextLayoutResult& output) noexcept override {
        if (fonts_ == nullptr ||
            !Aero::IsValidLayoutSize(
                request.availableSize) ||
            !std::isfinite(request.dpiScale) ||
            request.dpiScale <= 0.0 ||
            !std::isfinite(request.lineHeight) ||
            request.lineHeight < 0.0F) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Headless text layout request is invalid");
        }
        Text::TextLayoutRequest layoutRequest;
        layoutRequest.face = request.face.handle.IsValid()
            ? request.face
            : config_.face;
        layoutRequest.fallbackFaces = !request.fallbackFaces.Empty()
            ? request.fallbackFaces
            : config_.fallbackFaces;
        layoutRequest.text = request.text;
        layoutRequest.pixelSize =
            request.pixelSize;
        layoutRequest.maxWidth =
            static_cast<float>(
                request.availableSize.width);
        layoutRequest.lineHeight =
            request.lineHeight > 0.0F
            ? request.lineHeight
            : config_.lineHeight;
        layoutRequest.wrapping = request.wrapping;
        layoutRequest.trimming = request.trimming;
        layoutRequest.alignment = request.alignment;
        layoutRequest.direction = request.direction;
        Text::TextLayout layout(allocator_);
        Base::Result<void> shaped =
            layout.ShapeAndMeasure(
                *fonts_, layoutRequest);
        if (!shaped) return shaped.GetStatus();
        if (request.arrangeToAvailableWidth) {
            shaped = layout.Arrange(
                static_cast<float>(request.availableSize.width));
            if (!shaped) return shaped.GetStatus();
        }
        output.glyphRuns.Clear();
        output.desiredSize = {
            static_cast<double>(
                layout.NaturalSize().width),
            static_cast<double>(
                layout.NaturalSize().height)};
        if (!layout.Runs().Empty()) {
            if (nextGlyphRun_ ==
                Render::InvalidRenderGlyphRunId) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Headless glyph-run ID space is exhausted");
            }
            Base::Result<void> appended =
                output.glyphRuns.PushBack(
                    nextGlyphRun_++);
            if (!appended) return appended.GetStatus();
        }
        output.hitRegions.Clear();
        for (const Text::TextLine& line : layout.Lines()) {
            const float lineHeight = (line.ascent + line.descent > 0.0F)
                ? (line.ascent + line.descent)
                : (request.pixelSize > 0.0F ? request.pixelSize : 16.0F);
            for (std::uint32_t r = 0U; r < line.runCount; ++r) {
                const Text::GlyphRun& run = layout.Runs()[line.firstRun + r];
                for (std::uint32_t g = 0U; g < run.glyphs.Size(); ++g) {
                    const Text::PositionedGlyph& glyph = run.glyphs[g];
                    TextHitRegion region;
                    region.textOffset = glyph.cluster;
                    region.textLength = (g + 1 < run.glyphs.Size() && run.glyphs[g + 1].cluster > glyph.cluster)
                        ? run.glyphs[g + 1].cluster - glyph.cluster
                        : 1U;
                    region.x = glyph.x;
                    region.y = line.y;
                    region.width = glyph.advanceX;
                    region.height = lineHeight;
                    (void)output.hitRegions.PushBack(region);
                }
            }
        }
        return {};
    }

    void ReleaseGlyphRun(
        Render::RenderGlyphRunId) noexcept override {}

private:
    Text::FontManager* fonts_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    TextConfig config_;
    Base::Vector<Text::FontFace> fallbackFaces_;
    Render::RenderGlyphRunId nextGlyphRun_ =
        UINT64_C(1) << 32U;
    bool valid_ = false;
};

} // namespace

struct TextPipelineState {
    struct LoadedFont {
        explicit LoadedFont(
            Base::IAllocator* allocator = nullptr) noexcept
            : request(allocator),
              path(allocator) {}

        Base::String request;
        Base::String path;
        Text::FontFace face;
        FontWeight weight = FontWeight::Normal;
        FontStyle style = FontStyle::Normal;
    };

    TextPipelineState(
        Base::IAllocator& allocator,
        RenderDevice& selectedDevice) noexcept
        : fontProvider(&allocator),
          shaper(fontProvider),
          fonts(&allocator),
          fallbackFaces(&allocator),
          loadedFonts(&allocator),
          primaryPath(&allocator),
          primaryFamily(&allocator),
          language(&allocator),
          fontSearchRoot(&allocator),
          device(&selectedDevice),
          allocator(&allocator) {}

    Text::FreeTypeAdapter fontProvider;
    Text::HarfBuzzAdapter shaper;
    Text::FontManager fonts;
    Base::Vector<Text::FontFace> fallbackFaces;
    Base::Vector<LoadedFont> loadedFonts;
    Base::String primaryPath;
    Base::String primaryFamily;
    Base::String language;
    Base::String fontSearchRoot;
    Text::FontFace primaryFace;
    TextConfig config;
    TextBlockLayoutProxy proxy;
    ::Aero::Controls::TextBlockLayout* layout = nullptr;
    alignas(HeadlessTextBlockLayout) std::uint8_t headlessStorage[sizeof(HeadlessTextBlockLayout)]{};
    HeadlessTextBlockLayout* headlessLayout = nullptr;
    RenderDevice* device = nullptr;
    TextResources* resources = nullptr;
    std::uint64_t resourceGeneration = 0U;
    Base::IAllocator* allocator = nullptr;

    HeadlessTextBlockLayout* CreateHeadless() noexcept {
        DestroyHeadless();
        headlessLayout = new (headlessStorage)
            HeadlessTextBlockLayout(fonts, config, allocator);
        if (!headlessLayout->IsValid()) {
            DestroyHeadless();
            return nullptr;
        }
        layout = headlessLayout;
        return headlessLayout;
    }

    void DestroyHeadless() noexcept {
        if (headlessLayout == nullptr) return;
        headlessLayout->~HeadlessTextBlockLayout();
        headlessLayout = nullptr;
        if (layout != nullptr) layout = nullptr;
    }
};

static_assert(sizeof(TextPipelineState) <= 16384U,
    "TextPipeline inline state storage is too small");
static_assert(alignof(TextPipelineState) <= alignof(std::max_align_t),
    "TextPipeline inline state alignment is insufficient");

TextPipeline::TextPipeline(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {}

TextPipeline::~TextPipeline() noexcept {
    Shutdown();
}

Base::Result<void> TextPipeline::Initialize(
    RenderDevice& device,
    TextResources* resources,
    const TextOptions& options) noexcept {
    if (state_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "View text pipeline is already initialized");
    }
    if (!(options.defaultPixelSize > 0.0F) ||
        !std::isfinite(options.defaultPixelSize)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View text default pixel size must be positive");
    }

    state_ = new (stateStorage_)
        TextPipelineState(*allocator_, device);
    state_->allocator = allocator_;
    state_->device = &device;
    state_->proxy.SetFaceResolver(
        state_,
        [](void* context,
           Base::StringView family,
           FontWeight weight,
           FontStyle style,
           Text::FontFace& primary,
           Base::Vector<Text::FontFace>& fallbacks) noexcept
            -> Base::Result<void> {
            auto* state =
                static_cast<TextPipelineState*>(context);
            if (state == nullptr ||
                family.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Requested font family is empty");
            }

            auto loadFace = [&](Base::StringView candidate,
                                Text::FontFace& outFace) noexcept -> bool {
                for (const TextPipelineState::LoadedFont& loaded :
                     state->loadedFonts) {
                    if (loaded.request.View() == candidate &&
                        loaded.weight == weight &&
                        loaded.style == style) {
                        outFace = loaded.face;
                        return true;
                    }
                }
                Base::String path(state->allocator);
                Base::Result<void> selected = SelectFontPath(
                    candidate, false, path,
                    state->fontSearchRoot.View(),
                    weight == FontWeight::Bold ||
                        weight == FontWeight::SemiBold,
                    style != FontStyle::Normal);
                if (!selected) return false;

                Text::Typeface typeface(state->allocator);
                Base::Result<void> status = ConfigureTypeface(
                    typeface,
                    candidate,
                    state->language.View(),
                    state->primaryFamily.View());
                if (!status) return false;

                Text::FontSource source;
                source.kind = Text::FontSourceKind::File;
                source.identifier = path.View();
                Text::FontFace face;
                status = state->fonts.LoadFace(
                    state->fontProvider.Identity().id,
                    source,
                    typeface,
                    face);
                if (!status) return false;

                Base::Result<TextPipelineState::LoadedFont*> added =
                    state->loadedFonts.EmplaceBack(state->allocator);
                if (added) {
                    TextPipelineState::LoadedFont& loaded = *added.Value();
                    loaded.weight = weight;
                    loaded.style = style;
                    (void)loaded.request.Assign(candidate);
                    (void)loaded.path.Assign(path.View());
                    loaded.face = face;
                }
                outFace = face;
                return true;
            };

            std::uint32_t begin = 0U;
            while (begin <= family.SizeBytes()) {
                std::uint32_t end = begin;
                while (end < family.SizeBytes() && family[end] != ',') {
                    ++end;
                }
                const Base::StringView candidate =
                    TrimFontFamilyCandidate(
                        family.Substr(begin, end - begin));
                if (!candidate.Empty()) {
                    Text::FontFace loadedFace{};
                    if (loadFace(candidate, loadedFace) &&
                        loadedFace.handle.IsValid()) {
                        if (!primary.handle.IsValid()) {
                            primary = loadedFace;
                        } else {
                            (void)fallbacks.PushBack(loadedFace);
                        }
                    }
                }
                if (end == family.SizeBytes()) break;
                begin = end + 1U;
            }

            for (const Text::FontFace& globalFallback : state->fallbackFaces) {
                (void)fallbacks.PushBack(globalFallback);
            }
            if (!primary.handle.IsValid()) {
                primary = state->primaryFace;
            }
            return {};
        });

    Base::Result<void> status =
        state_->primaryFamily.Assign(
            options.primaryFamily.Empty()
                ? Base::StringView("Segoe UI")
                : options.primaryFamily);
    if (status) {
        status = state_->language.Assign(
            options.language.Empty()
                ? Base::StringView("en-US")
                : options.language);
    }
    if (status) {
        status = state_->fontSearchRoot.Assign(
            options.fontSearchRoot);
    }
    if (status) status = state_->fontProvider.Initialize();
    if (status) status = state_->fonts.Initialize();
    if (status) {
        status = state_->fonts.RegisterProvider({
            &state_->fontProvider,
            &state_->shaper,
            &state_->fontProvider});
    }
    if (status) {
        status = SelectFontPath(
            options.primaryFamily,
            false,
            state_->primaryPath);
    }
    if (status) {
        Text::Typeface typeface(allocator_);
        status = ConfigureTypeface(
            typeface,
            options.primaryFamily,
            state_->language.View(),
            state_->primaryFamily.View());
        if (status) {
            Text::FontSource source;
            source.kind = Text::FontSourceKind::File;
            source.identifier = state_->primaryPath.View();
            status = state_->fonts.LoadFace(
                state_->fontProvider.Identity().id,
                source,
                typeface,
                state_->primaryFace);
        }
    }

    if (status) {
        const std::uint32_t requestedFallbacks =
            options.fallbackFamilies.Size();
        const std::uint32_t fallbackCount =
            requestedFallbacks == 0U
            ? 1U
            : requestedFallbacks;
        for (std::uint32_t index = 0U;
             status && index < fallbackCount; ++index) {
            const Base::StringView family =
                requestedFallbacks == 0U
                ? Base::StringView("Microsoft YaHei")
                : options.fallbackFamilies[index];
            Base::String path(allocator_);
            Base::Result<void> selected =
                SelectFontPath(family, true, path);
            if (!selected) {
                continue;
            }
            Text::Typeface typeface(allocator_);
            status = ConfigureTypeface(
                typeface,
                family,
                state_->language.View(),
                Base::StringView("Microsoft YaHei"));
            if (!status) break;
            Text::FontSource source;
            source.kind = Text::FontSourceKind::File;
            source.identifier = path.View();
            Text::FontFace face;
            status = state_->fonts.LoadFace(
                state_->fontProvider.Identity().id,
                source,
                typeface,
                face);
            if (status) {
                status = state_->fallbackFaces.PushBack(
                    face);
            }
        }
        if (status &&
            requestedFallbacks != 0U &&
            state_->fallbackFaces.Empty()) {
            status = Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "No configured fallback font family is available");
        }
    }

    if (status) {
        state_->config.face = state_->primaryFace;
        state_->config.fallbackFaces =
            state_->fallbackFaces.AsSpan();
        state_->config.pixelSize =
            options.defaultPixelSize;
        state_->config.atlas.pageWidth = 1024U;
        state_->config.atlas.pageHeight = 1024U;
        state_->config.atlas.maxPages = 4U;
        state_->resources = resources;
        if (state_->resources != nullptr) {
            state_->resourceGeneration =
                state_->resources->generation;
            if (state_->resources->create ==
                nullptr) {
                status = Base::Status::Failure(
                    Base::ErrorCode::NotInitialized,
                    "Render device has no text layout factory");
            } else {
                Base::Result<
                    ::Aero::Controls::TextBlockLayout*>
                    created =
                        state_->resources->
                            create(
                                state_->resources->
                                    context,
                                state_->fonts,
                                state_->config,
                                *allocator_);
                if (!created) {
                    status = created.GetStatus();
                } else {
                    state_->layout = created.Value();
                }
            }
        } else if (state_->CreateHeadless() == nullptr) {
            status = Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Headless text fallback copy failed");
        }
    }
    if (!status) {
        Base::Status failure = status.GetStatus();
        Shutdown();
        return failure;
    }
    state_->proxy.Set(state_->layout);
    return {};
}

Base::Result<bool>
TextPipeline::SynchronizeBackend(
    RenderDevice& device,
    TextResources* resources,
    bool force) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "View text pipeline is not initialized");
    }
    state_->device = &device;
    TextResources* current = resources;
    const std::uint64_t generation =
        current != nullptr
        ? current->generation
        : 0U;
    if (!force &&
        current == state_->resources &&
        generation == state_->resourceGeneration) {
        return false;
    }

    state_->proxy.Set(nullptr);
    if (state_->headlessLayout != nullptr) {
        state_->DestroyHeadless();
    } else if (
        state_->layout != nullptr &&
        state_->resources != nullptr &&
        state_->resources->destroy != nullptr) {
        state_->resources->destroy(
            state_->resources->context,
            state_->layout);
        state_->layout = nullptr;
    }
    state_->layout = nullptr;

    Base::Result<void> status;
    if (current != nullptr) {
        if (current->create == nullptr) {
            status = Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Restored device has no text layout factory");
        } else {
            Base::Result<
                ::Aero::Controls::TextBlockLayout*>
                created = current->create(
                    current->context,
                    state_->fonts,
                    state_->config,
                    *allocator_);
            if (!created) {
                status = created.GetStatus();
            } else {
                state_->layout = created.Value();
            }
        }
    } else if (state_->CreateHeadless() == nullptr) {
        status = Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Headless text fallback copy failed");
    }
    state_->resources = current;
    state_->resourceGeneration = generation;
    if (!status) return status.GetStatus();
    state_->proxy.Set(state_->layout);
    return true;
}

Base::Result<std::uint32_t>
TextPipeline::CollectGarbage() noexcept {
    if (state_ == nullptr || state_->layout == nullptr) {
        return 0U;
    }
    if (state_->headlessLayout != nullptr ||
        state_->resources == nullptr ||
        state_->resources->collect ==
            nullptr) {
        return 0U;
    }
    return state_->resources->collect(
        state_->resources->context,
        state_->layout);
}

void TextPipeline::Shutdown() noexcept {
    if (state_ == nullptr) return;
    state_->proxy.Set(nullptr);
    if (state_->headlessLayout != nullptr) {
        state_->DestroyHeadless();
    } else if (
        state_->layout != nullptr &&
        state_->resources != nullptr) {
        TextResources* current = state_->resources;
        if (current == state_->resources &&
            current->generation ==
                state_->resourceGeneration &&
            current->destroy != nullptr) {
            current->destroy(
                current->context, state_->layout);
        }
    }
    state_->layout = nullptr;
    for (TextPipelineState::LoadedFont& loaded :
         state_->loadedFonts) {
        if (loaded.face.handle.IsValid()) {
            static_cast<void>(
                state_->fonts.ReleaseFace(
                    loaded.face.handle));
        }
    }
    state_->loadedFonts.Clear();
    state_->fonts.Shutdown();
    state_->fontProvider.Shutdown();
    state_->~TextPipelineState();
    state_ = nullptr;
}

::Aero::Controls::TextBlockLayout*
TextPipeline::Layout() noexcept {
    return state_ != nullptr
        ? &state_->proxy
        : nullptr;
}

} // namespace Aero::Text
