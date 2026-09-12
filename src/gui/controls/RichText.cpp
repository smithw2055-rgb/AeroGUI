#include "gui/meta/TypeRegistryDetail.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/ListBox.hpp>
#include <Aero/Controls/TreeView.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Media/Transforms.hpp>
#include "gui/media/BrushRendering.hpp"
#include <Aero/Documents.hpp>
#include <Aero/Media/SolidColorBrush.hpp>
#include <Aero/Base/String.hpp>
#include "gui/meta/ValueConversion.hpp"
#include "ControlsMetadata.hpp"

#include "TextBlockLayout.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>


#include "RichText.hpp"

namespace Aero {
namespace {

Base::StringView TrimRichTextToken(Base::StringView value) noexcept {
    std::uint32_t begin = 0U;
    std::uint32_t end = value.SizeBytes();
    while (begin < end &&
        std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    while (end > begin &&
        std::isspace(static_cast<unsigned char>(value[end - 1U])) != 0) {
        --end;
    }
    return value.Substr(begin, end - begin);
}

bool StartsWithRichTextToken(
    Base::StringView value,
    Base::StringView prefix) noexcept {
    return value.SizeBytes() >= prefix.SizeBytes() &&
        value.Substr(0U, prefix.SizeBytes()) == prefix;
}

std::uint32_t FindRichTextToken(
    Base::StringView source,
    Base::StringView token,
    std::uint32_t begin) noexcept {
    if (token.Empty() || begin > source.SizeBytes()) return UINT32_MAX;
    for (std::uint32_t index = begin;
         index + token.SizeBytes() <= source.SizeBytes(); ++index) {
        if (source.Substr(index, token.SizeBytes()) == token) return index;
    }
    return UINT32_MAX;
}

Base::StringView RichTextFormatAttribute(
    Base::StringView tag) noexcept {
    const Base::StringView key("format=");
    const std::uint32_t keyAt = FindRichTextToken(tag, key, 0U);
    if (keyAt == UINT32_MAX) return {};
    const std::uint32_t valueAt = keyAt + key.SizeBytes();
    if (valueAt >= tag.SizeBytes()) return {};
    const char quote = tag[valueAt];
    if (quote != '\'' && quote != '"') return {};
    for (std::uint32_t end = valueAt + 1U;
         end < tag.SizeBytes(); ++end) {
        if (tag[end] == quote) {
            return tag.Substr(valueAt + 1U, end - valueAt - 1U);
        }
    }
    return {};
}

Base::Result<void> AppendRichTextValue(
    Base::String& output,
    const Meta::Value& value,
    Base::StringView format) noexcept {
    char raw[128]{};
    bool numeric = false;
    double number = 0.0;
    switch (value.Kind()) {
    case Meta::ValueKind::String:
        return output.Append(value.AsString());
    case Meta::ValueKind::Boolean:
        return output.Append(value.AsBoolean()
            ? Base::StringView("True")
            : Base::StringView("False"));
    case Meta::ValueKind::SignedInteger:
        numeric = true;
        number = static_cast<double>(value.AsSignedInteger());
        break;
    case Meta::ValueKind::UnsignedInteger:
        numeric = true;
        number = static_cast<double>(value.AsUnsignedInteger());
        break;
    case Meta::ValueKind::Double:
        numeric = true;
        number = value.AsDouble();
        break;
    default:
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "RichText binding value has no text conversion");
    }

    if (!numeric) return {};
    std::uint32_t precision = 15U;
    Base::StringView prefix;
    Base::StringView suffix;
    const std::uint32_t placeholder = FindRichTextToken(
        format, Base::StringView("{0"), 0U);
    if (placeholder != UINT32_MAX) {
        const std::uint32_t close = FindRichTextToken(
            format, Base::StringView("}"), placeholder + 2U);
        if (close != UINT32_MAX) {
            prefix = format.Substr(0U, placeholder);
            suffix = format.Substr(close + 1U);
            if (placeholder + 3U < close &&
                format[placeholder + 2U] == ':') {
                const Base::StringView specifier = format.Substr(
                    placeholder + 3U,
                    close - placeholder - 3U);
                bool allZero = !specifier.Empty();
                for (std::uint32_t index = 0U;
                     index < specifier.SizeBytes(); ++index) {
                    allZero = allZero && specifier[index] == '0';
                }
                if (allZero) precision = 0U;
            }
        }
    }
    const int length = std::snprintf(
        raw, sizeof(raw), "%.*f",
        static_cast<int>(precision), number);
    if (length <= 0) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "RichText numeric binding formatting failed");
    }
    Base::Result<void> appended = output.Append(prefix);
    if (appended) {
        appended = output.Append(Base::StringView(
            raw, static_cast<std::uint32_t>(
                std::min<int>(length, sizeof(raw) - 1U))));
    }
    if (appended) appended = output.Append(suffix);
    return appended;
}

Base::Result<bool> AppendRichTextBinding(
    DependencyObject& object,
    Base::StringView path,
    Base::StringView format,
    Base::String& output) noexcept {
    if (!AeroGuiInternal::PropertyRegistry(object).Types().IsDerivedFrom(
            object.RuntimeType(),
            FrameworkElement::StaticTypeId())) {
        return false;
    }
    const Meta::Value source =
        static_cast<FrameworkElement&>(object).GetDataContext();
    const Meta::TypeId sourceType =
        source.Kind() == Meta::ValueKind::Object &&
            !source.IsNullObject() && source.AsObject()
        ? source.AsObject()->RuntimeType()
        : source.Type();
    Meta::ObjectFactoryState services = Meta::CurrentObjectFactory();
    if (services.metadata == nullptr) return false;
    Meta::Registry& runtime = *services.metadata;
    Base::Result<Meta::BindingPathPlan> compiled =
        Meta::BindingPathPlan::Compile(
            runtime, sourceType, TrimRichTextToken(path));
    if (!compiled) return false;
    Base::Result<Meta::Value> resolved =
        compiled.Value().Get(runtime, source);
    if (!resolved) return false;
    Base::Result<void> appended = AppendRichTextValue(
        output, resolved.Value(), format);
    return appended
        ? Base::Result<bool>(true)
        : Base::Result<bool>(appended.GetStatus());
}

struct RichTextParseState {
    Base::Color foreground{};
    bool hasForeground = false;
    bool bold = false;
    bool italic = false;
};

bool SameRichTextState(
    const Controls::TextBlock::RichTextStyleRange& range,
    const RichTextParseState& state) noexcept {
    return range.hasForeground == state.hasForeground &&
        range.bold == state.bold &&
        range.italic == state.italic &&
        (!state.hasForeground ||
         (range.foreground.red == state.foreground.red &&
          range.foreground.green == state.foreground.green &&
          range.foreground.blue == state.foreground.blue &&
          range.foreground.alpha == state.foreground.alpha));
}

} // namespace

void ApplyRichText(DependencyObject& object) noexcept {
    if (!AeroGuiInternal::PropertyRegistry(object).Types().IsDerivedFrom(
            object.RuntimeType(),
            Controls::TextBlock::StaticTypeId())) return;
    const Base::StringView source = object.GetValue(RichText::TextProperty);
    Base::String plain;
    Base::Vector<RichTextParseState> states;
    Base::Vector<Controls::TextBlock::RichTextStyleRange> ranges;
    states.PushBack({});

    const auto recordRange = [&ranges, &states](
        std::uint32_t start,
        std::uint32_t end) noexcept -> bool {
        if (end <= start || states.Empty()) return true;
        const RichTextParseState& state =
            states[states.Size() - 1U];
        if (!state.hasForeground && !state.bold && !state.italic) {
            return true;
        }
        if (!ranges.Empty()) {
            auto& previous = ranges[ranges.Size() - 1U];
            if (previous.start + previous.length == start &&
                SameRichTextState(previous, state)) {
                previous.length += end - start;
                return true;
            }
        }
        Controls::TextBlock::RichTextStyleRange range;
        range.start = start;
        range.length = end - start;
        range.foreground = state.foreground;
        range.hasForeground = state.hasForeground;
        range.bold = state.bold;
        range.italic = state.italic;
        ranges.PushBack(range);
        return true;
    };
    const auto appendText = [&plain, &recordRange](
        Base::StringView value) noexcept -> bool {
        const std::uint32_t start = plain.SizeBytes();
        if (!plain.Append(value)) return false;
        return recordRange(start, plain.SizeBytes());
    };

    std::uint32_t index = 0U;
    while (index < source.SizeBytes()) {
        if (source[index] != '[') {
            const std::uint32_t next = FindRichTextToken(
                source, Base::StringView("["), index);
            const std::uint32_t end = next == UINT32_MAX
                ? source.SizeBytes() : next;
            if (!appendText(source.Substr(index, end - index))) return;
            index = end;
            continue;
        }
        const std::uint32_t tagEnd = FindRichTextToken(
            source, Base::StringView("]"), index + 1U);
        if (tagEnd == UINT32_MAX) {
            if (!appendText(source.Substr(index))) return;
            break;
        }
        const Base::StringView tag = TrimRichTextToken(
            source.Substr(index + 1U, tagEnd - index - 1U));
        if (StartsWithRichTextToken(tag, Base::StringView("bind"))) {
            const Base::StringView closeToken("[/bind]");
            const std::uint32_t close = FindRichTextToken(
                source, closeToken, tagEnd + 1U);
            if (close != UINT32_MAX) {
                const Base::StringView path = source.Substr(
                    tagEnd + 1U, close - tagEnd - 1U);
                const std::uint32_t rangeStart = plain.SizeBytes();
                Base::Result<bool> bound = AppendRichTextBinding(
                    object, path, RichTextFormatAttribute(tag), plain);
                if (!bound) return;
                if (!bound.Value() && !plain.Append(path)) return;
                if (!recordRange(rangeStart, plain.SizeBytes())) return;
                index = close + closeToken.SizeBytes();
                continue;
            }
        }

        bool stateTag = false;
        if (!tag.Empty() && tag[0] == '/') {
            const Base::StringView closing = TrimRichTextToken(
                tag.Substr(1U));
            stateTag = closing == Base::StringView("b") ||
                closing == Base::StringView("i") ||
                closing == Base::StringView("size") ||
                closing == Base::StringView("style") ||
                closing == Base::StringView("color");
            if (stateTag && states.Size() > 1U) states.PopBack();
        } else {
            RichTextParseState nextState =
                states[states.Size() - 1U];
            if (tag == Base::StringView("b")) {
                nextState.bold = true;
                stateTag = true;
            } else if (tag == Base::StringView("i")) {
                nextState.italic = true;
                stateTag = true;
            } else if (StartsWithRichTextToken(
                    tag, Base::StringView("size="))) {
                // Font-size ranges are retained structurally even though the
                // current glyph pipeline shapes a TextBlock at one size.
                stateTag = true;
            } else if (StartsWithRichTextToken(
                    tag, Base::StringView("style="))) {
                const Base::StringView styleName = TrimRichTextToken(
                    tag.Substr(Base::StringView("style=").SizeBytes()));
                if (styleName == Base::StringView("ColoredText")) {
                    nextState.foreground =
                        Base::Color{0.1647F, 0.6510F, 0.8863F, 1.0F};
                    nextState.hasForeground = true;
                    nextState.bold = true;
                } else if (styleName ==
                           Base::StringView("ColoredNumber")) {
                    nextState.foreground =
                        Base::Color{0.7F, 1.0F, 0.0F, 1.0F};
                    nextState.hasForeground = true;
                }
                stateTag = true;
            }
            if (stateTag) states.PushBack(nextState);
        }
        index = tagEnd + 1U;
    }
    auto& text = static_cast<Controls::TextBlock&>(object);
    const Base::StringView trimmed = TrimRichTextToken(source);
    bool hasSize = false;
    if (StartsWithRichTextToken(trimmed, Base::StringView("[size="))) {
        const std::uint32_t sizeEnd = FindRichTextToken(
            trimmed, Base::StringView("]"), 6U);
        if (sizeEnd != UINT32_MAX) {
            const Base::StringView sizeStr = trimmed.Substr(6U, sizeEnd - 6U);
            double fontSize = 0.0;
            for (std::uint32_t i = 0U; i < sizeStr.SizeBytes(); ++i) {
                const char c = sizeStr[i];
                if (c >= '0' && c <= '9') {
                    fontSize = fontSize * 10.0 + (c - '0');
                }
            }
            if (fontSize > 0.0) {
                hasSize = true;
                if (text.GetFontSize() != fontSize) {
                    text.SetFontSize(fontSize);
                }
            }
        }
    }
    if (!hasSize) {
        text.ClearValue(Controls::TextBlock::FontSizeProperty);
    }
    const bool isEnclosedBold = (StartsWithRichTextToken(trimmed, Base::StringView("[b]")) &&
        trimmed.SizeBytes() >= 7U &&
        trimmed.Substr(trimmed.SizeBytes() - 4U) == Base::StringView("[/b]")) ||
        (StartsWithRichTextToken(trimmed, Base::StringView("[size=")) &&
         FindRichTextToken(trimmed, Base::StringView("[b]"), 0U) != UINT32_MAX &&
         FindRichTextToken(trimmed, Base::StringView("[/b]"), 0U) != UINT32_MAX);
    if (isEnclosedBold) {
        if (text.GetFontWeight() != FontWeight::Bold) {
            text.SetFontWeight(FontWeight::Bold);
        }
    } else {
        text.ClearValue(Controls::TextBlock::FontWeightProperty);
    }
    if (text.GetText() != plain.View()) {
        text.SetText(plain.View());
    }
    text.SetRichTextStyleRanges(ranges.AsSpan());
}

void RichText::OnTextChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept {
    ApplyRichText(object);
}

namespace Controls {
namespace {

void AddTextBlockInline(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    auto& text = static_cast<TextBlock&>(owner);
    if (!AeroGuiInternal::PropertyRegistry(text).Types().IsDerivedFrom(
            child->RuntimeType(),
            Aero::Documents::Inline::StaticTypeId())) {
        return;
    }
    text.AddOwnedInline(child);
}

void ClearTextBlockInlines(
    Base::Object& owner,
    void*) noexcept {
    static_cast<TextBlock&>(owner).ClearOwnedInlines();
}

void AddSpanInline(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    auto& span = static_cast<Documents::Span&>(owner);
    if (!AeroGuiInternal::PropertyRegistry(span).Types().IsDerivedFrom(
            child->RuntimeType(),
            Documents::Inline::StaticTypeId())) {
        return;
    }
    span.AddOwnedInline(
        Base::Ref<Documents::Inline>::FromBorrowed(
            *static_cast<Documents::Inline*>(child.Get())));
}

void ClearSpanInlines(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Documents::Span&>(owner).ClearOwnedInlines();
}

} // namespace

void TextBlock::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    namespace Docs = Aero::Documents;
    using namespace Aero::Media;
    using namespace Aero::Meta;

    const auto makeBrush = [](Base::Color color) noexcept {
        Base::Result<Base::Ref<Brush>> made = MakeSolidColorBrush(color);
        return made ? std::move(made).Value() : Base::Ref<Brush>{};
    };
    const Base::Ref<Brush> black = makeBrush({0.0F, 0.0F, 0.0F, 1.0F});

    Register<TextBlock>(context)
        .Property(TextBlock::TextProperty, Base::String{}, AffectsMeasure)
        .Property(TextBlock::BackgroundProperty, Base::Ref<Brush>{}, AffectsRender)
        .Property(TextBlock::StrokeProperty, Base::Ref<Brush>{}, AffectsRender)
        .AddOwner(TextBlock::FontSizeProperty, 15.0, Inherits | AffectsMeasure, &ValidatePositiveFiniteDouble)
        .Property(TextBlock::FontWeightProperty, FontWeight::Normal, AffectsMeasure)
        .Property(TextBlock::FontStyleProperty, FontStyle::Normal, AffectsMeasure)
        .Property(TextBlock::TextDecorationsProperty, TextDecorations::None, AffectsRender)
        .Property(TextBlock::StrokeThicknessProperty, 0.0, AffectsMeasure | AffectsRender, &Base::Validate::NonNegative<double>)
        .Property(TextBlock::TextWrappingProperty, TextWrapping::NoWrap, AffectsMeasure)
        .Property(TextBlock::TextTrimmingProperty, TextTrimming::None, AffectsMeasure)
        .Property(TextBlock::TextAlignmentProperty, TextAlignment::Left, AffectsMeasure)
        .Property(TextBlock::LineHeightProperty, 0.0, AffectsMeasure, &Base::Validate::NonNegative<double>)
        .Property(TextBlock::PaddingProperty, Aero::Thickness{}, AffectsMeasure | AffectsArrange, &ValidateThicknessValue)
        .Property<Value, &TextBlock::GetMetadataInlines, &TextBlock::SetInlineValue>("Inlines", PropertyFlags::AnyValue | PropertyFlags::Collection | PropertyFlags::Structural)
        .ContentAccessor(MakeMemberId(TextBlock::StaticTypeId(), MemberKind::Property, "Inlines"), ContentKind::Collection, &AddTextBlockInline, &ClearTextBlockInlines, ContentFlags::None)
        .Factory();

    Register<Docs::TextElement>(context, TypeFlags::Abstract)
        .AddOwner(Docs::TextElement::FontFamilyProperty, Aero::FrameworkElement::FontFamilyProperty, Base::Ref<FontFamily>{}, Inherits | AffectsMeasure)
        .Property(Docs::TextElement::FontWeightProperty, FontWeight::Normal, Inherits | AffectsMeasure)
        .AddOwner(Docs::TextElement::ForegroundProperty, Aero::FrameworkElement::ForegroundProperty, black, Inherits)
        .Property(Docs::TextElement::FontSizeProperty, 15.0, Inherits, &ValidatePositiveFiniteDouble)
        .Property(Docs::TextElement::FontStyleProperty, FontStyle::Normal, Inherits)
        .Property(Docs::TextElement::TextDecorationsProperty, TextDecorations::None, Inherits);

    Register<Docs::Inline>(context, TypeFlags::Abstract);

    Register<Docs::Run>(context)
        .Property(Docs::Run::TextProperty, FrameworkPropertyMetadata(Base::String{}).Structural())
        .Content(Docs::Run::TextProperty.Id())
        .Factory();

    Register<Docs::Span>(context)
        .Property<Value, &Docs::Span::GetMetadataInlines, &Docs::Span::SetInlineValue>("Inlines", PropertyFlags::AnyValue | PropertyFlags::Collection | PropertyFlags::Structural)
        .ContentAccessor(MakeMemberId(Docs::Span::StaticTypeId(), MemberKind::Property, "Inlines"), ContentKind::Collection, &AddSpanInline, &ClearSpanInlines, ContentFlags::None)
        .Factory();

    Register<Docs::Bold>(context)
        .Override(Docs::TextElement::FontWeightProperty, FontWeight::Bold, AffectsMeasure)
        .Factory();

    Register<Docs::Italic>(context)
        .Override(Docs::TextElement::FontStyleProperty, FontStyle::Italic, AffectsMeasure)
        .Factory();

    Register<Docs::Underline>(context)
        .Override(Docs::TextElement::TextDecorationsProperty, TextDecorations::Underline, AffectsRender)
        .Factory();

    Register<Docs::LineBreak>(context)
        .Factory();

    Register<Docs::RequestNavigateEventArgs>(context);

    Register<Docs::Hyperlink>(context)
        .Event(Docs::Hyperlink::ClickEvent)
        .Event(Docs::Hyperlink::RequestNavigateEvent)
        .Property(Docs::Hyperlink::NavigateUriProperty, Base::String{})
        .Property(Docs::Hyperlink::CommandProperty, Base::Ref<ICommand>{})
        .Property(Docs::Hyperlink::CommandParameterProperty, Value::NullObject(TypeOf<Base::Object>()))
        .Property(Docs::Hyperlink::CommandTargetProperty, Base::Ref<UIElement>{})
        .Override(Docs::TextElement::TextDecorationsProperty, TextDecorations::Underline, AffectsRender)
        .Factory();

    Register<Docs::InlineUIContainer>(context)
        .Property(Docs::InlineUIContainer::ChildProperty, Base::Ref<UIElement>{})
        .Factory();

    Register<Docs::Adorner>(context)
        .Factory();

    Register<Docs::AdornerLayer>(context)
        .Factory();

    Register<Docs::AdornerDecorator>(context)
        .Factory();
}

} // namespace Controls

} // namespace Aero
