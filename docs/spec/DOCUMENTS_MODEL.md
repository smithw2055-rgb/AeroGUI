# WPF Documents Model

## Public hierarchy

`Aero::Documents` owns `TextElement`, `Inline`, `Run`, `Span`, `Bold`,
`Italic`, `Underline`, `LineBreak` and `Hyperlink`. The declaration hierarchy
follows WPF document semantics. The implementation reuses `Controls::TextBlock`
as the retained visual and layout carrier rather than creating a second text
engine.

## Property identity

TextElement formatting owners reuse the existing TextBlock dependency-property
handles through metadata AddOwner registration. `TextElement.FontWeight`,
`TextElement.Foreground` and `TextElement.FontSize` therefore do not create
duplicate value stores.

## Inline content

TextBlock and Span use the existing owned-inline storage, visual mounting, text
layout and line-break paths. Only objects derived from `Documents::Inline` are
accepted as inline content. Literal XAML text is materialized as
`Documents::Run`.

## Hyperlink

Hyperlink derives from Span, exposes Click, NavigateUri, Command,
CommandParameter and CommandTarget, and is attached to a private runtime
interaction service for pointer, keyboard and command execution. It no longer
derives from ButtonBase and no longer consumes a ControlTemplate.
