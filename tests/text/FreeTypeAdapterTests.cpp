#include <Aero/Text/FontManager.hpp>
#include <Aero/Text/FreeTypeAdapter.hpp>

#include <cstdio>

namespace {

using namespace Aero::Base;
using namespace Aero::Text;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

bool TestFreeTypeOnlyPipelineAndCache() {
    FreeTypeAdapter adapter;
    CHECK(adapter.Initialize());

    FontManager manager;
    CHECK(manager.Initialize());
    CHECK(manager.RegisterProvider({&adapter, &adapter, &adapter}));

    Typeface typeface;
    CHECK(typeface.TrySetFamily("Roboto"));
    FontSource source;
    source.kind = FontSourceKind::File;
    source.identifier = AERO_TEXT_TEST_FONT;

    FontFace first;
    FontFace cached;
    CHECK(manager.LoadFace(
        adapter.Identity().id, source, typeface, first));
    CHECK(manager.LoadFace(
        adapter.Identity().id, source, typeface, cached));
    CHECK(first.handle == cached.handle);

    ShapingRequest shaping;
    shaping.face = first.handle;
    shaping.text = "Aero 123";
    shaping.pixelSize = 20.0F;
    shaping.direction = TextDirection::LeftToRight;
    shaping.script = Script::Latin;
    ShapedTextRun shaped;
    CHECK(manager.Shape(shaping, shaped));
    CHECK(shaped.glyphs.Size() == 8U);
    CHECK(shaped.glyphs[0].advanceX > 0.0F);

    ShapingRequest complex = shaping;
    complex.direction = TextDirection::RightToLeft;
    complex.script = Script::Arabic;
    Result<void> unsupported = manager.Shape(complex, shaped);
    CHECK(!unsupported);
    CHECK(unsupported.GetStatus().code == ErrorCode::Unsupported);

    CHECK(manager.ReleaseFace(first.handle));
    shaped.glyphs.Clear();
    CHECK(manager.Shape(shaping, shaped));
    CHECK(manager.ReleaseFace(cached.handle));
    Result<void> stale = manager.Shape(shaping, shaped);
    CHECK(!stale);
    CHECK(stale.GetStatus().code == ErrorCode::NotFound);

    manager.Shutdown();
    adapter.Shutdown();
    return true;
}

} // namespace

int main() {
    if (!TestFreeTypeOnlyPipelineAndCache()) return 1;
    std::puts("Aero FreeType adapter tests passed");
    return 0;
}
