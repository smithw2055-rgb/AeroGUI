# Documents Content and Navigation Model

## Collections

`InlineCollection` is a mutable projection over the existing TextBlock-owned
inline storage. `InlineCollectionView` is the immutable projection. Collection
mutation is accepted while detached; mounted edits remain a MountService
transaction so ownership and visual/layout edges cannot diverge.

## Text positions

`TextPointer` binds a root TextBlock and a UTF-8 byte offset in its flattened
inline content. `TextRange` requires two ordered pointers from the same root.
Public offset operations reject positions inside a multi-byte UTF-8 sequence.
The offset model matches shaping cluster offsets and supports point-to-position
and position-to-character-rectangle mapping after a valid measure pass.

## Navigation

Hyperlink activation raises Click, executes Command when configured, and then
raises RequestNavigate when NavigateUri is non-empty. `NavigationService` is a
host policy adapter attached to a routed-event root; AeroGUI does not choose an
OS browser or external navigation policy.

## Accessibility

The diagnostics accessibility projection recognizes document text and links,
publishes flattened text, and exposes Invoke/Focus/Navigate actions for links.
Logical and visual-only inline children are both traversed. Block, Paragraph and
FlowDocument remain outside this stage.
