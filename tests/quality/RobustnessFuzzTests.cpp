#include <Aero/Base/Ref.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Metadata.hpp>
#include <Aero/Core/Metadata/BindingPath.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/CoreMetadata.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Markup/XamlCompiledDocument.hpp>
#include <Aero/Presentation/Metadata.hpp>
#include <Aero/Rhi/OpenGL33Backend.hpp>
#include <Aero/Text/Text.hpp>

#include <cstdio>
#include <cstring>
#include <memory>

namespace {

using namespace Aero::Base;
using namespace Aero::Controls;
using namespace Aero::Core;
using namespace Aero::Markup;
using namespace Aero::Presentation;
using namespace Aero::Rhi;
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

class Random final {
public:
    std::uint32_t Next() noexcept {
        state_ ^= state_ << 13U;
        state_ ^= state_ >> 17U;
        state_ ^= state_ << 5U;
        return state_;
    }

    std::uint32_t Bounded(
        std::uint32_t limit) noexcept {
        return limit == 0U
            ? 0U
            : Next() % limit;
    }

private:
    std::uint32_t state_ = 0xA3E0F17DU;
};

class FuzzObject final : public Object {
public:
    TypeId RuntimeType()
        const noexcept override {
        return BuiltinTypes::Object;
    }
};

struct MetadataFixture final {
    MetadataDomain metadata;
    std::unique_ptr<MetadataRuntime> runtime;

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
        CHECK(runtime->Freeze());
        return true;
    }
};

bool BindingPathFuzz() {
    MetadataFixture fixture;
    CHECK(fixture.Build());
    Random random;
    char path[65]{};
    constexpr char Alphabet[] =
        "Width.Height.DataContext[]()_0123456789";
    for (std::uint32_t iteration = 0U;
         iteration < 1000U;
         ++iteration) {
        const std::uint32_t size =
            random.Bounded(64U);
        for (std::uint32_t index = 0U;
             index < size;
             ++index) {
            path[index] =
                Alphabet[random.Bounded(
                    static_cast<std::uint32_t>(
                        sizeof(Alphabet) - 1U))];
        }
        path[size] = '\0';
        BindingPathCompileError error;
        Result<BindingPathPlan> compiled =
            BindingPathPlan::Compile(
                *fixture.runtime,
                BuiltinTypes::FrameworkElement,
                StringView(path, size),
                &error);
        if (compiled) {
            CHECK(compiled.Value().IsValid());
            CHECK(!compiled.Value().
                Segments().Empty());
        } else {
            CHECK(!compiled.GetStatus().IsOk());
        }
    }
    return true;
}

bool CompiledXamlDecoderFuzz() {
    MetadataFixture fixture;
    CHECK(fixture.Build());
    Random random;
    std::uint8_t bytes[512]{};
    XamlCompiledDocumentLimits limits;
    limits.maxNodes = 128U;
    limits.maxStringBytes = 4096U;
    for (std::uint32_t iteration = 0U;
         iteration < 1000U;
         ++iteration) {
        const std::uint32_t size =
            random.Bounded(512U);
        for (std::uint32_t index = 0U;
             index < size;
             ++index) {
            bytes[index] =
                static_cast<std::uint8_t>(
                    random.Next());
        }
        Result<XamlCompiledDocument> decoded =
            XamlCompiledDocument::Deserialize(
                {bytes, size},
                fixture.metadata,
                limits);
        if (decoded) {
            CHECK(decoded.Value().IsValid());
            CHECK(decoded.Value().Nodes().
                Size() <= limits.maxNodes);
        } else {
            CHECK(!decoded.GetStatus().IsOk());
        }
    }
    return true;
}

class FuzzFontProvider final
    : public IFontProvider,
      public ITextShaper,
      public IGlyphRasterizer {
public:
    FontProviderIdentity Identity()
        const noexcept override {
        return {993U, 1U};
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
        const FontQuery& query,
        FontFace& output) noexcept override {
        if (query.requireCodePoint &&
            query.codePoint > 0x10FFFFU) {
            return Status::Failure(
                ErrorCode::NotFound,
                "code point is outside Unicode");
        }
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
            glyph.advanceX =
                4.0F +
                static_cast<float>(
                    glyph.glyph % 8U);
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
        const GlyphRequest& request,
        GlyphMetrics& output)
        noexcept override {
        if (request.pixelSize <= 0.0F ||
            request.dpiScale <= 0.0F) {
            return Status::Failure(
                ErrorCode::InvalidArgument,
                "invalid glyph scale");
        }
        output.width = request.pixelSize;
        output.height = request.pixelSize;
        output.advanceX = request.pixelSize;
        return {};
    }

    Result<void> Rasterize(
        const GlyphRequest& request,
        GlyphBitmap& output)
        noexcept override {
        GlyphMetrics metrics;
        Result<void> valid =
            GetMetrics(request, metrics);
        if (!valid) {
            return valid.GetStatus();
        }
        output.width = 1U;
        output.height = 1U;
        output.strideBytes = 1U;
        Result<void> resized =
            output.pixels.TryResize(1U);
        if (!resized) {
            return resized.GetStatus();
        }
        output.pixels[0U] = 255U;
        return {};
    }

    Result<void> ExtractOutline(
        const GlyphRequest&,
        GlyphOutline&) noexcept override {
        return Status::Failure(
            ErrorCode::Unsupported,
            "outline is unavailable");
    }
};

bool PrepareFonts(
    FuzzFontProvider& provider,
    FontManager& fonts,
    FontFace& face) {
    CHECK(fonts.Initialize());
    CHECK(fonts.RegisterProvider(
        {&provider, &provider, &provider}));
    Typeface typeface;
    CHECK(typeface.TrySetFamily(
        "Fuzz Font"));
    FontSource source;
    source.identifier = "fuzz-font";
    CHECK(fonts.LoadFace(
        provider.Identity().id,
        source,
        typeface,
        face));
    return true;
}

bool TextLayoutFuzz() {
    FuzzFontProvider provider;
    FontManager fonts;
    FontFace face;
    CHECK(PrepareFonts(
        provider, fonts, face));
    Random random;
    char text[129]{};
    TextLayout layout;
    for (std::uint32_t iteration = 0U;
         iteration < 1000U;
         ++iteration) {
        const std::uint32_t size =
            random.Bounded(128U);
        for (std::uint32_t index = 0U;
             index < size;
             ++index) {
            text[index] =
                static_cast<char>(
                    0x20U +
                    random.Bounded(0x5FU));
        }
        TextLayoutRequest request;
        request.face = face;
        request.text =
            StringView(text, size);
        request.pixelSize =
            1.0F +
            static_cast<float>(
                random.Bounded(127U));
        request.maxWidth =
            static_cast<float>(
                random.Bounded(1024U));
        request.wrapping =
            static_cast<TextWrapping>(
                random.Bounded(2U));
        Result<void> shaped =
            layout.ShapeAndMeasure(
                fonts, request);
        if (shaped) {
            CHECK(layout.Size().width >=
                0.0F);
            CHECK(layout.Size().height >=
                0.0F);
        }
    }
    return true;
}

bool FontProviderBoundaryFuzz() {
    FuzzFontProvider provider;
    FontManager fonts;
    FontFace face;
    CHECK(PrepareFonts(
        provider, fonts, face));
    Random random;
    Typeface typeface;
    CHECK(typeface.TrySetFamily(
        "Boundary Font"));
    for (std::uint32_t iteration = 0U;
         iteration < 1000U;
         ++iteration) {
        FontQuery query;
        query.typeface =
            (iteration & 1U) != 0U
            ? &typeface
            : nullptr;
        query.codePoint = random.Next();
        query.requireCodePoint = true;
        query.preferredProvider =
            (iteration % 3U) == 0U
            ? provider.Identity().id
            : InvalidFontProviderId;
        FontFace resolved;
        Result<void> result =
            fonts.ResolveFace(
                query, resolved);
        if (result) {
            CHECK(resolved.handle.IsValid());
            CHECK(query.codePoint <=
                0x10FFFFU);
        }

        GlyphRequest glyph;
        glyph.face = face.handle;
        glyph.glyph = random.Next();
        glyph.pixelSize =
            (iteration & 1U) != 0U
            ? 16.0F
            : -1.0F;
        glyph.dpiScale =
            (iteration & 2U) != 0U
            ? 1.0F
            : 0.0F;
        GlyphMetrics metrics;
        Result<void> measured =
            fonts.GetGlyphMetrics(
                glyph, metrics);
        if (measured) {
            CHECK(glyph.pixelSize > 0.0F);
            CHECK(glyph.dpiScale > 0.0F);
        }
    }
    return true;
}

bool ItemsCollectionDeltaFuzz() {
    Random random;
    ItemsCollection items;
    Result<Ref<FuzzObject>> made =
        MakeRef<FuzzObject>();
    CHECK(made);
    Ref<Object> item(
        std::move(made).Value());
    std::uint32_t expected = 0U;
    for (std::uint32_t iteration = 0U;
         iteration < 5000U;
         ++iteration) {
        switch (random.Bounded(5U)) {
        case 0U:
            CHECK(items.Add(item));
            ++expected;
            break;
        case 1U:
            if (expected != 0U) {
                const std::uint32_t index =
                    random.Bounded(expected);
                CHECK(items.RemoveAt(index));
                --expected;
            }
            break;
        case 2U:
            if (expected != 0U) {
                CHECK(items.Replace(
                    random.Bounded(expected),
                    item));
            }
            break;
        case 3U:
            if (expected > 1U) {
                CHECK(items.Move(
                    random.Bounded(expected),
                    random.Bounded(expected)));
            }
            break;
        default:
            if (expected > 128U) {
                items.Reset();
                expected = 0U;
            }
            break;
        }
        CHECK(items.Count() == expected);
        if (expected != 0U) {
            CHECK(items.ItemAt(
                random.Bounded(expected)).
                    Get() == item.Get());
        }
    }
    return true;
}

Result<Ref<Object>> InstantiateTemplate(
    const Ref<Object>& item,
    void* context) noexcept {
    auto* count =
        static_cast<std::uint32_t*>(
            context);
    if (!item || count == nullptr) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "template input is invalid");
    }
    ++*count;
    Result<Ref<FuzzObject>> made =
        MakeRef<FuzzObject>();
    if (!made) {
        return made.GetStatus();
    }
    return Ref<Object>(
        std::move(made).Value());
}

bool TemplateInstantiationFuzz() {
    std::uint32_t count = 0U;
    DataTemplate valid(
        &InstantiateTemplate, &count);
    DataTemplate invalid(nullptr);
    Result<Ref<FuzzObject>> made =
        MakeRef<FuzzObject>();
    CHECK(made);
    Ref<Object> item(
        std::move(made).Value());
    for (std::uint32_t iteration = 0U;
         iteration < 1000U;
         ++iteration) {
        Result<Ref<Object>> result =
            (iteration % 3U) == 0U
            ? invalid.Instantiate(item)
            : valid.Instantiate(
                (iteration % 7U) == 0U
                ? Ref<Object>()
                : item);
        if (result) {
            CHECK(result.Value());
        } else {
            CHECK(!result.GetStatus().IsOk());
        }
    }
    CHECK(count != 0U);
    return true;
}

bool GlShaderMetadataFuzz() {
    Random random;
    const std::uint8_t vertex[] =
        "#version 330 core\nvoid main(){}";
    const std::uint8_t fragment[] =
        "#version 330 core\nout vec4 c;"
        "void main(){c=vec4(1);}";
    for (std::uint32_t iteration = 0U;
         iteration < 2000U;
         ++iteration) {
        PipelineDescriptor descriptor;
        descriptor.vertexShader.stage =
            (random.Bounded(4U) == 0U)
            ? ShaderStage::Fragment
            : ShaderStage::Vertex;
        descriptor.vertexShader.language =
            (random.Bounded(4U) == 0U)
            ? ShaderLanguage::SpirV
            : ShaderLanguage::Glsl330;
        descriptor.vertexShader.bytecode =
            (random.Bounded(5U) == 0U)
            ? nullptr
            : vertex;
        descriptor.vertexShader.bytecodeSize =
            (random.Bounded(5U) == 0U)
            ? 0U
            : static_cast<std::uint32_t>(
                sizeof(vertex) - 1U);
        descriptor.vertexShader.entryPoint =
            (random.Bounded(5U) == 0U)
            ? StringView()
            : StringView("main");
        descriptor.vertexShader.stableId =
            random.Bounded(5U) == 0U
            ? 0U
            : 1001U;

        descriptor.fragmentShader.stage =
            (random.Bounded(4U) == 0U)
            ? ShaderStage::Vertex
            : ShaderStage::Fragment;
        descriptor.fragmentShader.language =
            (random.Bounded(4U) == 0U)
            ? ShaderLanguage::Dxbc
            : ShaderLanguage::Glsl330;
        descriptor.fragmentShader.bytecode =
            (random.Bounded(5U) == 0U)
            ? nullptr
            : fragment;
        descriptor.fragmentShader.bytecodeSize =
            (random.Bounded(5U) == 0U)
            ? 0U
            : static_cast<std::uint32_t>(
                sizeof(fragment) - 1U);
        descriptor.fragmentShader.entryPoint =
            (random.Bounded(5U) == 0U)
            ? StringView()
            : StringView("main");
        descriptor.fragmentShader.stableId =
            random.Bounded(5U) == 0U
            ? 0U
            : 1002U;
        if (random.Bounded(8U) == 0U) {
            descriptor.fragmentShader.
                stableId =
                    descriptor.vertexShader.
                        stableId;
        }

        Result<void> valid =
            ValidateOpenGL33PipelineDescriptor(
                descriptor);
        if (valid) {
            CHECK(descriptor.vertexShader.
                stage == ShaderStage::Vertex);
            CHECK(descriptor.fragmentShader.
                stage ==
                    ShaderStage::Fragment);
            CHECK(descriptor.vertexShader.
                stableId !=
                    descriptor.fragmentShader.
                        stableId);
        } else {
            CHECK(!valid.GetStatus().IsOk());
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    const char* name = argv[1];
    bool passed = false;
    if (std::strcmp(
            name, "BindingPath") == 0) {
        passed = BindingPathFuzz();
    } else if (std::strcmp(
                   name,
                   "CompiledXamlDecoder") == 0) {
        passed =
            CompiledXamlDecoderFuzz();
    } else if (std::strcmp(
                   name,
                   "TextLayout") == 0) {
        passed = TextLayoutFuzz();
    } else if (std::strcmp(
                   name,
                   "FontProviderBoundary") == 0) {
        passed =
            FontProviderBoundaryFuzz();
    } else if (std::strcmp(
                   name,
                   "ItemsCollectionDelta") == 0) {
        passed =
            ItemsCollectionDeltaFuzz();
    } else if (std::strcmp(
                   name,
                   "TemplateInstantiation") == 0) {
        passed =
            TemplateInstantiationFuzz();
    } else if (std::strcmp(
                   name,
                   "GlShaderMetadata") == 0) {
        passed = GlShaderMetadataFuzz();
    }
    if (passed) {
        std::printf(
            "%s deterministic fuzz passed\n",
            name);
    }
    return passed ? 0 : 1;
}
