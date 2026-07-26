from pathlib import Path

path = Path('.github/patches/apply_theme_objectwriter_closure.py')
text = path.read_text(encoding='utf-8')
old = '''replace_once(
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
new = '''replace_once(
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
if text.count(old) != 1:
    raise RuntimeError(f'expected one ambiguous EndObject transform, found {text.count(old)}')
path.write_text(text.replace(old, new, 1), encoding='utf-8')
