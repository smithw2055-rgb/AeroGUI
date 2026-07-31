# Documents Selection and Caret Model

## Selection ownership

`TextBlock` retains only two UTF-8 offsets: the selection anchor and caret.
`Documents::TextSelection` is a borrowed value projection over those positions;
it does not introduce a second document, text buffer or ownership graph.

Interactive selection is enabled through `IsTextSelectionEnabled`. The private
runtime interaction manager attaches only to root TextBlock instances, never to
nested Inline carriers. Programmatic selection remains available independently
of runtime input services.

## Input and clipboard

Pointer press and drag use the Phase 10 point-to-position mapping. Routed input
positions are translated from originalSource-local coordinates to the root
TextBlock through the existing visual transforms and layout slots. Shift extends
from the existing anchor. Keyboard Left, Right, Up, Down, Home and End update the
caret, while Shift extends the selection. Ctrl+A selects all and Ctrl+C writes
the selected flattened text through the existing `Platform::IClipboard` API.
Documents are read-only in this phase; Cut, Paste and text mutation are excluded.

## Caret and rendering

Selection rectangles and caret geometry are derived from retained shaping hit
regions and nested Inline layout slots. Selection offsets are coerced during
layout rather than rendering. The runtime's host-driven `AdvanceTime` clock
toggles the focused collapsed caret at a 500 ms interval. No background thread
or platform timer is introduced.

## Accessibility

Accessibility nodes expose selection start, selection end and caret UTF-8
offsets, plus Focus, Select and Copy actions where applicable. Inline links keep
the independent Link projection introduced in Phase 10.

TextRange formatting, edit transactions, Paragraph, Block and FlowDocument are
outside this stage.
