#include <Aero/Base/Ref.hpp>
#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Metadata.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/CoreMetadata.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Presentation/Binding.hpp>
#include <Aero/Presentation/Metadata.hpp>
#include <Aero/Text/GlyphAtlas.hpp>
#include <Aero/Text/Text.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>

namespace {

using namespace Aero::Base;
using namespace Aero::Controls;
using namespace Aero::Core;
using namespace Aero::Presentation;
using namespace Aero::Text;

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

class BenchObject final : public Object {
public:
    TypeId RuntimeType()
        const noexcept override {
        return BuiltinTypes::Object;
    }
};

struct RuntimeFixture final {
    MetadataDomain metadata;
    std::unique_ptr<MetadataRuntime> runtime;
    Dispatcher dispatcher;
    std::unique_ptr<ObjectServicesScope>
        services;
    std::unique_ptr<EffectiveValueEngine>
        values;

    bool Build() {
        CHECK(TryRegisterCoreMetadata(metadata));
        CHECK(TryRegisterPresentationMetadata(
            metadata));
        CHECK(TryRegisterControlsMetadata(
            metadata));
        CHECK(metadata.Seal());
        runtime =
            std::make_unique<MetadataRuntime>(
                metadata);
        CHECK(
            TryRegisterDependencyPropertyRuntimeProvider(
                *runtime,
                metadata.DependencyProperties(),
                BuiltinTypes::DependencyObject));
        CHECK(runtime->Freeze());
        services =
            std::make_unique<
                ObjectServicesScope>(
                    dispatcher,
                    metadata.
                        DependencyProperties(),
                    *runtime);
        values =
            std::make_unique<
                EffectiveValueEngine>(
                    dispatcher,
                    metadata.
                        DependencyProperties());
        CHECK(values->Initialize());
        return true;
    }
};

double Microseconds(
    std::chrono::steady_clock::time_point begin,
    std::chrono::steady_clock::time_point end)
    noexcept {
    return std::chrono::duration<
        double, std::micro>(end - begin).count();
}

bool Report(
    const char* name,
    std::uint32_t iterations,
    std::chrono::steady_clock::time_point begin,
    std::chrono::steady_clock::time_point end) {
    const double elapsed =
        Microseconds(begin, end);
    std::printf(
        "%s iterations=%u total=%.3fus "
        "mean=%.3fus\n",
        name,
        iterations,
        elapsed,
        elapsed /
            static_cast<double>(iterations));
    return elapsed >= 0.0 &&
        elapsed < 10000000.0;
}

bool EmptyFrameBenchmark() {
    Dispatcher dispatcher;
    constexpr std::uint32_t Iterations = 5000U;
    const DispatcherFramePhase phases[] = {
        DispatcherFramePhase::BeginFrame,
        DispatcherFramePhase::Input,
        DispatcherFramePhase::PropertyChanges,
        DispatcherFramePhase::DataBind,
        DispatcherFramePhase::Animation,
        DispatcherFramePhase::Layout,
        DispatcherFramePhase::Lifecycle,
        DispatcherFramePhase::RenderCommit,
        DispatcherFramePhase::EndFrame};
    const auto begin =
        std::chrono::steady_clock::now();
    for (std::uint32_t frame = 0U;
         frame < Iterations;
         ++frame) {
        for (DispatcherFramePhase phase :
             phases) {
            CHECK(dispatcher.RunFramePhase(
                phase));
        }
    }
    const auto end =
        std::chrono::steady_clock::now();
    CHECK(dispatcher.FrameTimings().
        frameSequence == Iterations);
    return Report(
        "EmptyFrameBenchmark",
        Iterations,
        begin,
        end);
}

class BenchFontProvider final
    : public IFontProvider,
      public ITextShaper,
      public IGlyphRasterizer {
public:
    FontProviderIdentity Identity()
        const noexcept override {
        return {991U, 1U};
    }

    Result<void> LoadFace(
        const FontSource&,
        const Typeface&,
        FontFace& output) noexcept override {
        output.handle =
            {Identity(), 1U, 1U};
        output.metrics.unitsPerEm =
            1000.0F;
        output.metrics.ascent = 800.0F;
        output.metrics.descent = -200.0F;
        return {};
    }

    Result<void> ResolveFace(
        const FontQuery&,
        FontFace& output) noexcept override {
        output.handle =
            {Identity(), 1U, 1U};
        output.metrics.unitsPerEm =
            1000.0F;
        output.metrics.ascent = 800.0F;
        output.metrics.descent = -200.0F;
        return {};
    }

    Result<bool> HasCodePoint(
        FontFaceHandle,
        std::uint32_t codePoint)
        noexcept override {
        return codePoint <= 0x10FFFFU;
    }

    void ReleaseFace(
        FontFaceHandle) noexcept override {}

    bool Supports(
        FontProviderIdentity provider)
        const noexcept override {
        return provider == Identity();
    }

    Result<void> Shape(
        const ShapingRequest& request,
        ShapedTextRun& output)
        noexcept override {
        output.face = request.face;
        output.direction =
            TextDirection::LeftToRight;
        output.script = Script::Common;
        for (std::uint32_t index = 0U;
             index <
                request.text.SizeBytes();
             ++index) {
            ShapedGlyph glyph;
            glyph.glyph =
                static_cast<std::uint8_t>(
                    request.text.Data()[index]);
            glyph.cluster = index;
            glyph.advanceX = 8.0F;
            Result<void> appended =
                output.glyphs.TryPushBack(
                    glyph);
            if (!appended) {
                return appended.GetStatus();
            }
        }
        return {};
    }

    Result<void> GetMetrics(
        const GlyphRequest&,
        GlyphMetrics& output)
        noexcept override {
        output.width = 4.0F;
        output.height = 4.0F;
        output.advanceX = 5.0F;
        return {};
    }

    Result<void> Rasterize(
        const GlyphRequest& request,
        GlyphBitmap& output)
        noexcept override {
        output.width = 4U;
        output.height = 4U;
        output.strideBytes = 4U;
        Result<void> resized =
            output.pixels.TryResize(16U);
        if (!resized) {
            return resized.GetStatus();
        }
        for (std::uint8_t& pixel :
             output.pixels) {
            pixel =
                static_cast<std::uint8_t>(
                    request.glyph);
        }
        return {};
    }

    Result<void> ExtractOutline(
        const GlyphRequest&,
        GlyphOutline&) noexcept override {
        return Status::Failure(
            ErrorCode::Unsupported,
            "outline is not used");
    }
};

bool PrepareFont(
    BenchFontProvider& provider,
    FontManager& fonts,
    FontFace& face) {
    CHECK(fonts.Initialize());
    CHECK(fonts.RegisterProvider(
        {&provider, &provider, &provider}));
    Typeface typeface;
    CHECK(typeface.TrySetFamily(
        "Quality Benchmark"));
    FontSource source;
    source.identifier = "quality-benchmark";
    CHECK(fonts.LoadFace(
        provider.Identity().id,
        source,
        typeface,
        face));
    return true;
}

bool TextLayoutBenchmark() {
    BenchFontProvider provider;
    FontManager fonts;
    FontFace face;
    CHECK(PrepareFont(
        provider, fonts, face));
    TextLayoutRequest request;
    request.face = face;
    request.text =
        "AeroGUI text layout benchmark "
        "wraps deterministic content.";
    request.pixelSize = 16.0F;
    request.maxWidth = 240.0F;
    request.wrapping =
        TextWrapping::Wrap;
    TextLayout layout;
    constexpr std::uint32_t Iterations = 2000U;
    const auto begin =
        std::chrono::steady_clock::now();
    for (std::uint32_t index = 0U;
         index < Iterations;
         ++index) {
        CHECK(layout.ShapeAndMeasure(
            fonts, request));
    }
    const auto end =
        std::chrono::steady_clock::now();
    CHECK(!layout.Lines().Empty());
    return Report(
        "TextLayoutBenchmark",
        Iterations,
        begin,
        end);
}

bool BindingUpdateBenchmark() {
    RuntimeFixture fixture;
    CHECK(fixture.Build());
    FrameworkElement source(
        BuiltinTypes::FrameworkElement);
    FrameworkElement target(
        BuiltinTypes::FrameworkElement);
    BindingManager bindings(
        fixture.dispatcher);
    CHECK(bindings.Initialize());
    BindingDescriptor descriptor;
    descriptor.source = &source;
    descriptor.sourceProperty =
        FrameworkElement::WidthProperty;
    descriptor.target = &target;
    descriptor.targetProperty =
        FrameworkElement::WidthProperty;
    descriptor.mode = BindingMode::OneWay;
    Result<BindingHandle> attached =
        bindings.Attach(descriptor);
    CHECK(attached);
    constexpr std::uint32_t Iterations = 3000U;
    const auto begin =
        std::chrono::steady_clock::now();
    for (std::uint32_t index = 0U;
         index < Iterations;
         ++index) {
        CHECK(source.SetWidth(
            static_cast<double>(
                index % 512U)));
        CHECK(fixture.dispatcher.RunFramePhase(
            DispatcherFramePhase::
                PropertyChanges));
        CHECK(fixture.dispatcher.RunFramePhase(
            DispatcherFramePhase::
                DataBind));
    }
    const auto end =
        std::chrono::steady_clock::now();
    CHECK(target.Width() ==
        static_cast<double>(
            (Iterations - 1U) % 512U));
    CHECK(bindings.Detach(
        attached.Value()).Value());
    bindings.Shutdown();
    CHECK(fixture.values->DetachObject(
        target));
    CHECK(fixture.values->DetachObject(
        source));
    return Report(
        "BindingUpdateBenchmark",
        Iterations,
        begin,
        end);
}

bool ButtonInteractionBenchmark() {
    RuntimeFixture fixture;
    CHECK(fixture.Build());
    CheckBox button;
    constexpr std::uint32_t Iterations = 10000U;
    const auto begin =
        std::chrono::steady_clock::now();
    for (std::uint32_t index = 0U;
         index < Iterations;
         ++index) {
        CHECK(button.SetIsChecked(
            (index & 1U) != 0U));
    }
    const auto end =
        std::chrono::steady_clock::now();
    CHECK(button.IsChecked());
    CHECK(fixture.values->DetachObject(
        button));
    return Report(
        "ButtonInteractionBenchmark",
        Iterations,
        begin,
        end);
}

Result<Ref<Object>> MakeBenchItem(
    const Ref<Object>&,
    void*) noexcept {
    Result<Ref<BenchObject>> made =
        MakeRef<BenchObject>();
    if (!made) {
        return made.GetStatus();
    }
    return Ref<Object>(
        std::move(made).Value());
}

bool ItemsGenerationBenchmark() {
    ItemsCollection items;
    Result<Ref<BenchObject>> made =
        MakeRef<BenchObject>();
    CHECK(made);
    Ref<Object> item(
        std::move(made).Value());
    DataTemplate itemTemplate(
        &MakeBenchItem);
    constexpr std::uint32_t Iterations = 5000U;
    const auto begin =
        std::chrono::steady_clock::now();
    for (std::uint32_t index = 0U;
         index < Iterations;
         ++index) {
        CHECK(items.Add(item));
        Result<Ref<Object>> visual =
            itemTemplate.Instantiate(item);
        CHECK(visual);
    }
    while (items.Count() != 0U) {
        CHECK(items.RemoveAt(
            items.Count() - 1U));
    }
    const auto end =
        std::chrono::steady_clock::now();
    return Report(
        "ItemsGenerationBenchmark",
        Iterations,
        begin,
        end);
}

bool RenderPlanTranslationBenchmark() {
    RuntimeFixture fixture;
    CHECK(fixture.Build());
    ObjectTree tree(
        fixture.dispatcher,
        *fixture.values);
    CHECK(tree.Initialize());
    LayoutManager layout(
        fixture.dispatcher);
    CHECK(layout.Initialize());
    NullRenderBackend backend;
    RenderManager renderer(
        fixture.dispatcher,
        backend);
    CHECK(renderer.Initialize());
    Border root;
    CHECK(root.SetBackground(
        {0.2F, 0.4F, 0.6F, 1.0F}));
    CHECK(tree.SetRoot(&root));
    CHECK(layout.SetRoot(
        &root, {320.0, 200.0}));
    CHECK(renderer.SetRoot(&root));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::
            RenderCommit));
    const RenderPlan& plan =
        renderer.CurrentPlan();
    CHECK(!plan.Nodes().Empty());
    CHECK(!plan.Commands().Empty());
    constexpr std::uint32_t Iterations = 10000U;
    const auto begin =
        std::chrono::steady_clock::now();
    for (std::uint32_t index = 0U;
         index < Iterations;
         ++index) {
        CHECK(backend.Submit(plan));
    }
    const auto end =
        std::chrono::steady_clock::now();
    CHECK(renderer.SetRoot(nullptr));
    CHECK(layout.SetRoot(nullptr, {}));
    CHECK(tree.SetRoot(nullptr));
    CHECK(fixture.values->DetachObject(
        root));
    return Report(
        "RenderPlanTranslationBenchmark",
        Iterations,
        begin,
        end);
}

bool GlyphAtlasBenchmark() {
    BenchFontProvider provider;
    FontManager fonts;
    FontFace face;
    CHECK(PrepareFont(
        provider, fonts, face));
    GlyphAtlas atlas;
    GlyphAtlasConfig config;
    config.pageWidth = 256U;
    config.pageHeight = 256U;
    config.maxPages = 2U;
    config.padding = 1U;
    CHECK(atlas.Initialize(config));
    constexpr std::uint32_t Iterations = 512U;
    GlyphAtlasPlacement placement;
    const auto begin =
        std::chrono::steady_clock::now();
    for (std::uint32_t index = 0U;
         index < Iterations;
         ++index) {
        GlyphRequest request;
        request.face = face.handle;
        request.glyph =
            static_cast<GlyphId>(
                index % 96U);
        request.pixelSize = 16.0F;
        request.dpiScale = 1.0F;
        CHECK(atlas.EnsureGlyph(
            fonts,
            request,
            index + 1U,
            index,
            placement));
        atlas.ClearPendingUploads();
    }
    const auto end =
        std::chrono::steady_clock::now();
    CHECK(atlas.EntryCount() == 96U);
    return Report(
        "GlyphAtlasBenchmark",
        Iterations,
        begin,
        end);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(
            stderr,
            "quality benchmark name is required\n");
        return 2;
    }
    const char* name = argv[1];
    bool passed = false;
    if (std::strcmp(
            name,
            "EmptyFrameBenchmark") == 0) {
        passed = EmptyFrameBenchmark();
    } else if (std::strcmp(
                   name,
                   "TextLayoutBenchmark") == 0) {
        passed = TextLayoutBenchmark();
    } else if (std::strcmp(
                   name,
                   "BindingUpdateBenchmark") == 0) {
        passed = BindingUpdateBenchmark();
    } else if (std::strcmp(
                   name,
                   "ButtonInteractionBenchmark") == 0) {
        passed = ButtonInteractionBenchmark();
    } else if (std::strcmp(
                   name,
                   "ItemsGenerationBenchmark") == 0) {
        passed = ItemsGenerationBenchmark();
    } else if (std::strcmp(
                   name,
                   "RenderPlanTranslationBenchmark") == 0) {
        passed =
            RenderPlanTranslationBenchmark();
    } else if (std::strcmp(
                   name,
                   "GlyphAtlasBenchmark") == 0) {
        passed = GlyphAtlasBenchmark();
    }
    return passed ? 0 : 1;
}
