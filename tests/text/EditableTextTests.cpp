#include <Aero/Text/EditableText.hpp>

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

bool SnapshotEquals(
    const EditableTextModel& model,
    StringView expected) noexcept {
    String snapshot;
    return model.Snapshot(snapshot) &&
        snapshot.View() == expected;
}

bool TestUtf8AndGraphemeIndexes() noexcept {
    EditableTextModel model;
    CHECK(model.LineCount() == 1U);
    CHECK(model.SetText(StringView(
        u8"Ae\u0301中👩‍💻🇨🇳")));
    CHECK(model.CodePointCount() == 9U);
    CHECK(model.GraphemeCount() == 5U);
    CHECK(model.Caret() == 5U);
    CHECK(model.SizeBytes() == 26U);

    const std::uint32_t expectedOffsets[] = {
        0U, 1U, 4U, 7U, 18U, 26U};
    for (std::uint32_t index = 0U; index < 6U; ++index) {
        Result<std::uint32_t> offset =
            model.ByteOffsetForGrapheme(index);
        CHECK(offset);
        CHECK(offset.Value() == expectedOffsets[index]);
        Result<std::uint32_t> grapheme =
            model.GraphemeIndexForByteOffset(
                expectedOffsets[index]);
        CHECK(grapheme);
        CHECK(grapheme.Value() == index);
    }
    CHECK(!model.GraphemeIndexForByteOffset(2U));

    const char invalidBytes[] = {
        static_cast<char>(0xC0U),
        static_cast<char>(0xAFU)};
    CHECK(!model.SetText(
        StringView(invalidBytes, sizeof(invalidBytes))));
    CHECK(SnapshotEquals(
        model,
        StringView(u8"Ae\u0301中👩‍💻🇨🇳")));
    return true;
}

bool TestSelectionReplacementAndHistory() noexcept {
    EditableTextModel model;
    CHECK(model.SetText(StringView(u8"hello中")));
    const std::uint64_t initialRevision = model.Revision();
    CHECK(model.SetSelection(4U, 1U));
    CHECK(model.Selection().Start() == 1U);
    CHECK(model.Selection().Length() == 3U);
    CHECK(model.ReplaceSelection(StringView(u8"é")));
    CHECK(SnapshotEquals(model, StringView(u8"héo中")));
    CHECK(model.Caret() == 2U);
    CHECK(model.Revision() == initialRevision + 1U);
    CHECK(model.CanUndo());
    CHECK(!model.CanRedo());

    CHECK(model.DeleteBackward());
    CHECK(SnapshotEquals(model, StringView(u8"ho中")));
    CHECK(model.Caret() == 1U);
    CHECK(model.Undo());
    CHECK(SnapshotEquals(model, StringView(u8"héo中")));
    CHECK(model.Selection().anchor == 2U);
    CHECK(model.Undo());
    CHECK(SnapshotEquals(model, StringView(u8"hello中")));
    CHECK(model.Selection().anchor == 4U);
    CHECK(model.Selection().caret == 1U);
    CHECK(model.CanRedo());
    CHECK(model.Redo());
    CHECK(SnapshotEquals(model, StringView(u8"héo中")));

    CHECK(model.SetSelection(1U, 1U));
    CHECK(model.ReplaceSelection(StringView("a")));
    CHECK(!model.CanRedo());
    CHECK(SnapshotEquals(model, StringView(u8"haéo中")));
    CHECK(model.SetSelection(0U, model.GraphemeCount()));
    CHECK(model.DeleteForward());
    CHECK(SnapshotEquals(model, StringView()));
    CHECK(model.Undo());
    CHECK(SnapshotEquals(model, StringView(u8"haéo中")));
    return true;
}

bool TestMaximumLengthAndReadOnly() noexcept {
    EditableTextModel model;
    CHECK(model.SetMaximumLength(3U));
    CHECK(model.SetText(StringView("abc")));
    CHECK(!model.ReplaceRange({3U, 0U}, StringView("d")));
    CHECK(!model.SetMaximumLength(2U));
    CHECK(model.SetMaximumLength(4U));
    CHECK(model.ReplaceRange({3U, 0U}, StringView("d")));
    CHECK(SnapshotEquals(model, StringView("abcd")));

    CHECK(model.SetReadOnly(true));
    CHECK(model.IsReadOnly());
    CHECK(!model.DeleteBackward());
    CHECK(!model.Undo());
    CHECK(SnapshotEquals(model, StringView("abcd")));
    CHECK(model.SetText(StringView("xy")));
    CHECK(SnapshotEquals(model, StringView("xy")));
    CHECK(model.SetReadOnly(false));
    CHECK(model.DeleteBackward());
    CHECK(SnapshotEquals(model, StringView("x")));
    return true;
}

bool TestLineModel() noexcept {
    EditableTextModel model;
    CHECK(model.SetText(StringView("one\r\ntwo\n")));
    CHECK(model.GraphemeCount() == 8U);
    CHECK(model.LineCount() == 3U);
    Result<TextRange> first = model.LineRange(0U);
    Result<TextRange> second = model.LineRange(1U);
    Result<TextRange> third = model.LineRange(2U);
    CHECK(first && second && third);
    CHECK(first.Value().start == 0U);
    CHECK(first.Value().length == 3U);
    CHECK(second.Value().start == 4U);
    CHECK(second.Value().length == 3U);
    CHECK(third.Value().start == 8U);
    CHECK(third.Value().length == 0U);
    CHECK(!model.LineRange(3U));
    return true;
}

bool TestGapMovementAndHistoryBound() noexcept {
    EditableTextModel model;
    CHECK(model.SetText(StringView()));
    for (std::uint32_t index = 0U; index < 512U; ++index) {
        const std::uint32_t insertion =
            index % 2U == 0U
            ? 0U
            : model.GraphemeCount();
        CHECK(model.ReplaceRange(
            {insertion, 0U}, StringView("x")));
    }
    CHECK(model.GraphemeCount() == 512U);
    CHECK(model.SizeBytes() == 512U);
    CHECK(model.SetSelection(100U, 400U));
    CHECK(model.ReplaceSelection(StringView(u8"中")));
    CHECK(model.GraphemeCount() == 213U);
    CHECK(model.Caret() == 101U);
    CHECK(model.Undo());
    CHECK(model.GraphemeCount() == 512U);

    std::uint32_t undoCount = 0U;
    while (model.CanUndo()) {
        CHECK(model.Undo());
        ++undoCount;
    }
    CHECK(undoCount == 127U);
    CHECK(!model.Undo());
    model.ClearHistory();
    CHECK(!model.CanUndo());
    CHECK(!model.CanRedo());
    return true;
}

} // namespace

int main() {
    if (!TestUtf8AndGraphemeIndexes()) return 1;
    if (!TestSelectionReplacementAndHistory()) return 1;
    if (!TestMaximumLengthAndReadOnly()) return 1;
    if (!TestLineModel()) return 1;
    if (!TestGapMovementAndHistoryBound()) return 1;
    std::puts("Editable text model tests passed");
    return 0;
}
