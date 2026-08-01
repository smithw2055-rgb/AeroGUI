#include "../render/DisplayList.hpp"
#include "TextRuntime.hpp"
#include "TextResourceContract.hpp"

#include "../render/TextBackendAccess.hpp"

#include <Aero/FrameworkElement.hpp>
#include "../text/FontManager.hpp"
#include "../text/FreeTypeAdapter.hpp"
#include "../text/HarfBuzzAdapter.hpp"
#include "../text/TextLayout.hpp"

#include <cstdio>
#include <cmath>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <new>
#include <string>
#include <utility>

namespace Aero::Detail {
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
    return destination.TryAssign(source);
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
    Base::String& output) noexcept {
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
            prefix > bestPrefix ||
            (prefix == bestPrefix &&
             distance < bestDistance)) {
            best = iterator->path();
            bestPrefix = prefix;
            bestDistance = distance;
        }
    }
    if (error || best.empty() ||
        bestPrefix == 0U) {
        return false;
    }

    const std::string selected =
        best.string();
    Base::Result<void> assigned =
        output.TryAssign(Base::StringView(
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
    Base::StringView searchRoot = {}) noexcept {
    if (!family.Empty() && LooksLikeFontPath(family)) {
        Base::Result<bool> packed =
            SelectPackFontPath(
                family, searchRoot, output);
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
                combined.TryAssign(searchRoot);
            if (joined && !combined.Empty() &&
                combined.View()[
                    combined.SizeBytes() - 1U] != '/' &&
                combined.View()[
                    combined.SizeBytes() - 1U] != '\\') {
                joined = combined.TryAppend(
                    Base::StringView("/"));
            }
            if (joined) {
                joined =
                    combined.TryAppend(family);
            }
            if (!joined) {
                return joined.GetStatus();
            }
            std::string rooted(
                combined.CStr(),
                combined.SizeBytes());
            if (FileExists(rooted.c_str())) {
                return output.TryAssign(
                    combined.View());
            }
        }
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Configured font path does not exist");
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

    if (!family.Empty()) {
        if (configured == nullptr ||
            !FileExists(configured)) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Configured font family is unavailable");
        }
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
        typeface.TrySetFamily(
            family.Empty() ? defaultFamily : family);
    if (!assigned) return assigned.GetStatus();
    if (!language.Empty()) {
        assigned = typeface.TrySetLanguage(language);
    }
    return assigned;
}

class TextLayoutServiceProxy final
    : public Controls::Detail::TextLayoutService {
public:
    using FaceResolver =
        Base::Result<Text::FontFace> (*)(
            void* context,
            Base::StringView family) noexcept;

    void Set(
        Controls::Detail::TextLayoutService* service) noexcept {
        service_ = service;
    }

    void SetFaceResolver(
        void* context,
        FaceResolver resolver) noexcept {
        resolverContext_ = context;
        resolver_ = resolver;
    }

    Base::Result<void> ShapeAndPrepare(
        const Controls::Detail::TextLayoutRequest& request,
        Controls::Detail::TextLayoutResult& output) noexcept override {
        if (service_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "View text resources are unavailable");
        }
        if (request.fontFamily.Empty()) {
            return service_->ShapeAndPrepare(
                request, output);
        }
        if (resolver_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "View text font resolver is unavailable");
        }
        Base::Result<Text::FontFace> resolved =
            resolver_(
                resolverContext_,
                request.fontFamily);
        if (!resolved) {
            return resolved.GetStatus();
        }
        Controls::Detail::TextLayoutRequest selected =
            request;
        selected.face = resolved.Value();
        return service_->ShapeAndPrepare(
            selected, output);
    }

    void ReleaseGlyphRun(
        Render::RenderGlyphRunId glyphRun) noexcept override {
        if (service_ != nullptr) {
            service_->ReleaseGlyphRun(glyphRun);
        }
    }

private:
    Controls::Detail::TextLayoutService* service_ = nullptr;
    void* resolverContext_ = nullptr;
    FaceResolver resolver_ = nullptr;
};

class HeadlessTextLayoutService final
    : public Controls::Detail::TextLayoutService {
public:
    HeadlessTextLayoutService(
        Text::FontManager& fonts,
        const TextRuntimeConfig& config,
        Base::IAllocator* allocator) noexcept
        : fonts_(&fonts),
          allocator_(allocator),
          fallbackFaces_(allocator) {
        config_ = config;
        Base::Result<void> copied =
            fallbackFaces_.TryAppend(
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
        const Controls::Detail::TextLayoutRequest& request,
        Controls::Detail::TextLayoutResult& output) noexcept override {
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
        layoutRequest.fallbackFaces =
            config_.fallbackFaces;
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
        Text::TextLayout layout(allocator_);
        Base::Result<void> shaped =
            layout.ShapeAndMeasure(
                *fonts_, layoutRequest);
        if (!shaped) return shaped.GetStatus();
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
                output.glyphRuns.TryPushBack(
                    nextGlyphRun_++);
            if (!appended) return appended.GetStatus();
        }
        return {};
    }

    void ReleaseGlyphRun(
        Render::RenderGlyphRunId) noexcept override {}

private:
    Text::FontManager* fonts_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    TextRuntimeConfig config_;
    Base::Vector<Text::FontFace> fallbackFaces_;
    Render::RenderGlyphRunId nextGlyphRun_ =
        UINT64_C(1) << 32U;
    bool valid_ = false;
};

} // namespace

struct TextRuntime::Impl final {
    struct LoadedFont final {
        explicit LoadedFont(
            Base::IAllocator* allocator = nullptr) noexcept
            : request(allocator),
              path(allocator) {}

        Base::String request;
        Base::String path;
        Text::FontFace face;
    };

    Impl(
        Base::IAllocator& allocator,
        Render::RenderBackend& selectedBackend) noexcept
        : fontProvider(&allocator),
          shaper(fontProvider),
          fonts(&allocator),
          fallbackFaces(&allocator),
          loadedFonts(&allocator),
          primaryPath(&allocator),
          primaryFamily(&allocator),
          language(&allocator),
          fontSearchRoot(&allocator),
          backend(&selectedBackend),
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
    TextRuntimeConfig config;
    TextLayoutServiceProxy proxy;
    Controls::Detail::TextLayoutService* service = nullptr;
    HeadlessTextLayoutService* headlessService = nullptr;
    Render::RenderBackend* backend = nullptr;
    TextBackendServices* backendServices =
        nullptr;
    std::uint64_t backendGeneration = 0U;
    Base::IAllocator* allocator = nullptr;
};

TextRuntime::TextRuntime(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {}

TextRuntime::~TextRuntime() noexcept {
    Shutdown();
}

Base::Result<void> TextRuntime::Initialize(
    Render::RenderBackend& backend,
    const Integration::TextOptions& options) noexcept {
    if (impl_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "View text runtime is already initialized");
    }
    if (!(options.defaultPixelSize > 0.0F) ||
        !std::isfinite(options.defaultPixelSize)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View text default pixel size must be positive");
    }

    void* memory = allocator_->Allocate({
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::Render});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "View text runtime allocation failed");
    }
    impl_ = new (memory) Impl(*allocator_, backend);
    impl_->allocator = allocator_;
    impl_->backend = &backend;
    impl_->proxy.SetFaceResolver(
        impl_,
        [](void* context,
           Base::StringView family) noexcept
            -> Base::Result<Text::FontFace> {
            auto* state =
                static_cast<Impl*>(context);
            if (state == nullptr ||
                family.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Requested font family is empty");
            }
            for (const Impl::LoadedFont& loaded :
                 state->loadedFonts) {
                if (loaded.request.View() == family) {
                    return loaded.face;
                }
            }

            Base::Result<Impl::LoadedFont*> added =
                state->loadedFonts.TryEmplaceBack(
                    state->allocator);
            if (!added) {
                return added.GetStatus();
            }
            Impl::LoadedFont& loaded =
                *added.Value();
            Base::Result<void> status =
                loaded.request.TryAssign(family);
            if (status) {
                status = SelectFontPath(
                    family, false, loaded.path,
                    state->fontSearchRoot.View());
            }
            Text::Typeface typeface(
                state->allocator);
            if (status) {
                status = ConfigureTypeface(
                    typeface,
                    family,
                    state->language.View(),
                    state->primaryFamily.View());
            }
            if (status) {
                Text::FontSource source;
                source.kind =
                    Text::FontSourceKind::File;
                source.identifier =
                    loaded.path.View();
                status = state->fonts.LoadFace(
                    state->fontProvider.
                        Identity().id,
                    source,
                    typeface,
                    loaded.face);
            }
            if (!status) {
                const Base::Status failure =
                    status.GetStatus();
                state->loadedFonts.PopBack();
                return failure;
            }
            return loaded.face;
        });

    Base::Result<void> status =
        impl_->primaryFamily.TryAssign(
            options.primaryFamily.Empty()
                ? Base::StringView("Segoe UI")
                : options.primaryFamily);
    if (status) {
        status = impl_->language.TryAssign(
            options.language.Empty()
                ? Base::StringView("en-US")
                : options.language);
    }
    if (status) {
        status = impl_->fontSearchRoot.TryAssign(
            options.fontSearchRoot);
    }
    if (status) status = impl_->fontProvider.Initialize();
    if (status) status = impl_->fonts.Initialize();
    if (status) {
        status = impl_->fonts.RegisterProvider({
            &impl_->fontProvider,
            &impl_->shaper,
            &impl_->fontProvider});
    }
    if (status) {
        status = SelectFontPath(
            options.primaryFamily,
            false,
            impl_->primaryPath);
    }
    if (status) {
        Text::Typeface typeface(allocator_);
        status = ConfigureTypeface(
            typeface,
            options.primaryFamily,
            impl_->language.View(),
            impl_->primaryFamily.View());
        if (status) {
            Text::FontSource source;
            source.kind = Text::FontSourceKind::File;
            source.identifier = impl_->primaryPath.View();
            status = impl_->fonts.LoadFace(
                impl_->fontProvider.Identity().id,
                source,
                typeface,
                impl_->primaryFace);
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
                impl_->language.View(),
                Base::StringView("Microsoft YaHei"));
            if (!status) break;
            Text::FontSource source;
            source.kind = Text::FontSourceKind::File;
            source.identifier = path.View();
            Text::FontFace face;
            status = impl_->fonts.LoadFace(
                impl_->fontProvider.Identity().id,
                source,
                typeface,
                face);
            if (status) {
                status = impl_->fallbackFaces.TryPushBack(
                    face);
            }
        }
        if (status &&
            requestedFallbacks != 0U &&
            impl_->fallbackFaces.Empty()) {
            status = Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "No configured fallback font family is available");
        }
    }

    if (status) {
        impl_->config.face = impl_->primaryFace;
        impl_->config.fallbackFaces =
            impl_->fallbackFaces.AsSpan();
        impl_->config.pixelSize =
            options.defaultPixelSize;
        impl_->config.atlas.pageWidth = 1024U;
        impl_->config.atlas.pageHeight = 1024U;
        impl_->config.atlas.maxPages = 4U;
        impl_->backendServices =
            Render::Detail::RenderBackendAccess::
                TextServices(backend);
        if (impl_->backendServices != nullptr) {
            impl_->backendGeneration =
                impl_->backendServices->generation;
            if (impl_->backendServices->createService ==
                nullptr) {
                status = Base::Status::Failure(
                    Base::ErrorCode::NotInitialized,
                    "Render endpoint has no text service factory");
            } else {
                Base::Result<
                    Controls::Detail::TextLayoutService*>
                    created =
                        impl_->backendServices->
                            createService(
                                impl_->backendServices->
                                    context,
                                impl_->fonts,
                                impl_->config,
                                *allocator_);
                if (!created) {
                    status = created.GetStatus();
                } else {
                    impl_->service = created.Value();
                }
            }
        } else {
            void* serviceMemory = allocator_->Allocate({
                sizeof(HeadlessTextLayoutService),
                alignof(HeadlessTextLayoutService),
                Base::MemoryTag::Render});
            if (serviceMemory == nullptr) {
                status = Base::Status::Failure(
                    Base::ErrorCode::OutOfMemory,
                    "Headless text service allocation failed");
            } else {
                impl_->headlessService =
                    new (serviceMemory)
                        HeadlessTextLayoutService(
                            impl_->fonts,
                            impl_->config,
                            allocator_);
                if (!impl_->headlessService->IsValid()) {
                    status = Base::Status::Failure(
                        Base::ErrorCode::OutOfMemory,
                        "Headless text fallback copy failed");
                } else {
                    impl_->service =
                        impl_->headlessService;
                }
            }
        }
    }
    if (!status) {
        Base::Status failure = status.GetStatus();
        Shutdown();
        return failure;
    }
    impl_->proxy.Set(impl_->service);
    return {};
}

Base::Result<bool>
TextRuntime::SynchronizeBackend(
    bool force) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "View text runtime is not initialized");
    }
    TextBackendServices* current =
        Render::Detail::RenderBackendAccess::
            TextServices(*impl_->backend);
    const std::uint64_t generation =
        current != nullptr
        ? current->generation
        : 0U;
    if (!force &&
        current == impl_->backendServices &&
        generation == impl_->backendGeneration) {
        return false;
    }

    impl_->proxy.Set(nullptr);
    if (impl_->headlessService != nullptr) {
        impl_->headlessService->
            ~HeadlessTextLayoutService();
        allocator_->Deallocate(
            impl_->headlessService,
            sizeof(HeadlessTextLayoutService),
            alignof(HeadlessTextLayoutService),
            Base::MemoryTag::Render);
        impl_->headlessService = nullptr;
        impl_->service = nullptr;
    } else if (
        impl_->service != nullptr &&
        impl_->backendServices != nullptr &&
        impl_->backendServices->destroyService != nullptr) {
        impl_->backendServices->destroyService(
            impl_->backendServices->context,
            impl_->service);
        impl_->service = nullptr;
    }
    impl_->service = nullptr;

    Base::Result<void> status;
    if (current != nullptr) {
        if (current->createService == nullptr) {
            status = Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Restored endpoint has no text service factory");
        } else {
            Base::Result<
                Controls::Detail::TextLayoutService*>
                created = current->createService(
                    current->context,
                    impl_->fonts,
                    impl_->config,
                    *allocator_);
            if (!created) {
                status = created.GetStatus();
            } else {
                impl_->service = created.Value();
            }
        }
    } else {
        void* memory = allocator_->Allocate({
            sizeof(HeadlessTextLayoutService),
            alignof(HeadlessTextLayoutService),
            Base::MemoryTag::Render});
        if (memory == nullptr) {
            status = Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Headless text service allocation failed");
        } else {
            impl_->headlessService =
                new (memory)
                    HeadlessTextLayoutService(
                        impl_->fonts,
                        impl_->config,
                        allocator_);
            if (!impl_->headlessService->IsValid()) {
                impl_->headlessService->
                    ~HeadlessTextLayoutService();
                allocator_->Deallocate(
                    impl_->headlessService,
                    sizeof(HeadlessTextLayoutService),
                    alignof(HeadlessTextLayoutService),
                    Base::MemoryTag::Render);
                impl_->headlessService = nullptr;
                status = Base::Status::Failure(
                    Base::ErrorCode::OutOfMemory,
                    "Headless text fallback copy failed");
            } else {
                impl_->service =
                    impl_->headlessService;
            }
        }
    }
    impl_->backendServices = current;
    impl_->backendGeneration = generation;
    if (!status) return status.GetStatus();
    impl_->proxy.Set(impl_->service);
    return true;
}

Base::Result<std::uint32_t>
TextRuntime::CollectGarbage() noexcept {
    if (impl_ == nullptr || impl_->service == nullptr) {
        return 0U;
    }
    if (impl_->headlessService != nullptr ||
        impl_->backendServices == nullptr ||
        impl_->backendServices->collectGarbage ==
            nullptr) {
        return 0U;
    }
    return impl_->backendServices->collectGarbage(
        impl_->backendServices->context,
        impl_->service);
}

void TextRuntime::Shutdown() noexcept {
    if (impl_ == nullptr) return;
    impl_->proxy.Set(nullptr);
    if (impl_->headlessService != nullptr) {
        impl_->headlessService->
            ~HeadlessTextLayoutService();
        allocator_->Deallocate(
            impl_->headlessService,
            sizeof(HeadlessTextLayoutService),
            alignof(HeadlessTextLayoutService),
            Base::MemoryTag::Render);
        impl_->headlessService = nullptr;
    } else if (
        impl_->service != nullptr &&
        impl_->backendServices != nullptr) {
        TextBackendServices* current =
            Render::Detail::RenderBackendAccess::
                TextServices(*impl_->backend);
        if (current == impl_->backendServices &&
            current->generation ==
                impl_->backendGeneration &&
            current->destroyService != nullptr) {
            current->destroyService(
                current->context, impl_->service);
        }
    }
    impl_->service = nullptr;
    for (Impl::LoadedFont& loaded :
         impl_->loadedFonts) {
        if (loaded.face.handle.IsValid()) {
            static_cast<void>(
                impl_->fonts.ReleaseFace(
                    loaded.face.handle));
        }
    }
    impl_->loadedFonts.Clear();
    impl_->fonts.Shutdown();
    impl_->fontProvider.Shutdown();
    impl_->~Impl();
    allocator_->Deallocate(
        impl_,
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::Render);
    impl_ = nullptr;
}

Controls::Detail::TextLayoutService*
TextRuntime::Service() noexcept {
    return impl_ != nullptr
        ? &impl_->proxy
        : nullptr;
}

} // namespace Aero::Detail
