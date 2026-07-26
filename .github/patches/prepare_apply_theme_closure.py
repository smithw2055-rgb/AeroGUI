from pathlib import Path

path = Path('.github/patches/apply_theme_objectwriter_closure.py')
text = path.read_text(encoding='utf-8')

ambiguous_end = '''replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "    Frame& frame = frames_.Back();\\n"
    "    if (frame.kind == FrameKind::NullObject) {\\n",
    "    Frame& frame = frames_.Back();\\n"
    "    if (frame.kind == FrameKind::ValueObject) {\\n"
    "        return CompleteValueObject(node);\\n"
    "    }\\n"
    "    if (frame.kind == FrameKind::NullObject) {\\n",
)
'''
unique_end = '''replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "Base::Result<void> XamlObjectWriter::EndObject(\\n"
    "    const XamlNode& node) noexcept {\\n"
    "    if (frames_.Empty()) {\\n"
    "        return Failure(\\n"
    "            InvalidStateStatus(),\\n"
    "            XamlObjectWriterDiagnosticCodes::InvalidWriterState,\\n"
    "            MessageInvalidWriterState,\\n"
    "            node.Source());\\n"
    "    }\\n\\n"
    "    Frame& frame = frames_.Back();\\n"
    "    if (frame.kind == FrameKind::NullObject) {\\n",
    "Base::Result<void> XamlObjectWriter::EndObject(\\n"
    "    const XamlNode& node) noexcept {\\n"
    "    if (frames_.Empty()) {\\n"
    "        return Failure(\\n"
    "            InvalidStateStatus(),\\n"
    "            XamlObjectWriterDiagnosticCodes::InvalidWriterState,\\n"
    "            MessageInvalidWriterState,\\n"
    "            node.Source());\\n"
    "    }\\n\\n"
    "    Frame& frame = frames_.Back();\\n"
    "    if (frame.kind == FrameKind::ValueObject) {\\n"
    "        return CompleteValueObject(node);\\n"
    "    }\\n"
    "    if (frame.kind == FrameKind::NullObject) {\\n",
)
'''
if text.count(ambiguous_end) != 1:
    raise RuntimeError(
        f'expected one ambiguous EndObject transform, found {text.count(ambiguous_end)}')
text = text.replace(ambiguous_end, unique_end, 1)

noop = '''replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "    const Frame& objectFrame = frames_.Back();\\n"
    "    if (objectFrame.objectIndex >= created_.Size()) {\\n",
    "    const Frame& objectFrame = frames_.Back();\\n"
    "    if (objectFrame.objectIndex >= created_.Size()) {\\n",
)
'''
if text.count(noop) != 1:
    raise RuntimeError(f'expected one writer no-op transform, found {text.count(noop)}')
text = text.replace(noop, '', 1)

path.write_text(text, encoding='utf-8')
