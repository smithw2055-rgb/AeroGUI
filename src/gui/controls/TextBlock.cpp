#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/media/BrushRendering.hpp"
#include "RichText.hpp"
#include "TextBlockLayout.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Documents.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/TryCast.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>
#include "gui/core/DependencyObjectAccess.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/ValueConversion.hpp"
#include "ControlPropertyValidators.hpp"

namespace Aero::Controls {
using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Render;

namespace {

bool IsValidTextSize(Size value) noexcept {
    return IsFinite(value) && value.width >= 0.0 && value.height >= 0.0;
}
::Aero::Controls::TextBlockLayout* TextLayoutFor(
    const ::Aero::Media::Visual& visual) noexcept {
    return static_cast<::Aero::Controls::TextBlockLayout*>(ElementTree::TextLayoutOf(visual));
}

} // namespace

TextBlock::TextBlock() noexcept
    : TextBlock(StaticTypeId()) {}

TextBlock::TextBlock(TypeId runtimeType) noexcept
    : FrameworkElement(runtimeType),
      textHitRegions_(),
      ownedInlines_(),
      richTextStyleRanges_(),
      pendingInline_() {}

void TextBlock::InvalidateDocumentText() noexcept {
    flatTextValid_ = false;
    InvalidateMeasure();
}

void TextBlock::OnPropertyInvalidated(
    PropertyInvalidationFlags flags) noexcept {
    flatTextValid_ = false;
    FrameworkElement::OnPropertyInvalidated(flags);
}

TextBlock::~TextBlock() {
    ClearOwnedInlines();
    ReleaseServiceGlyphRun();
}
Base::StringView TextBlock::GetText() const noexcept {
    return GetValue(TextProperty);
}
Base::Ref<Brush> TextBlock::GetForeground() const noexcept {
    const auto authoredForeground =
        [](const DependencyObject& object) noexcept -> Base::Ref<Brush> {
            const Meta::PropertyValueSourceInfo source =
                object.GetValueSourceInfo(
                    FrameworkElementForegroundProperty.Handle());
            if (source.isInherited ||
                source.rank <= Meta::PropertyValueRank::Inherited) {
                return {};
            }
            return object.GetValue(FrameworkElementForegroundProperty);
        };
    if (Base::Ref<Brush> local = authoredForeground(*this)) {
        return local;
    }
    const ::Aero::Media::Visual* current =
        GetVisualParent() != nullptr
        ? GetVisualParent()
        : ::Aero::TryCast<::Aero::Media::Visual>(GetLogicalParent());
    if (current == nullptr && GetTemplatedParent() != nullptr) {
        current = ::Aero::TryCast<::Aero::Media::Visual>(GetTemplatedParent());
    }
    for (std::uint32_t depth = 0U;
         current != nullptr && depth < 64U;
         ++depth) {
        if (Base::Ref<Brush> authored = authoredForeground(*current)) {
            return authored;
        }
        const ::Aero::Media::Visual* next = current->GetVisualParent() != nullptr
            ? current->GetVisualParent()
            : ::Aero::TryCast<::Aero::Media::Visual>(
                  current->GetLogicalParent());
        if (next == nullptr && ::Aero::TryCast<::Aero::FrameworkElement>(current) != nullptr) {
            next = ::Aero::TryCast<::Aero::Media::Visual>(
                ::Aero::TryCast<::Aero::FrameworkElement>(current)->GetTemplatedParent());
        }
        current = next;
    }
    return GetValue(ForegroundProperty);
}
Base::Ref<Brush> TextBlock::GetBackground() const noexcept {
    return GetValue(BackgroundProperty);
}
double TextBlock::GetFontSize() const noexcept {
    const Meta::PropertyValueSourceInfo attached =
        GetValueSourceInfo(
            Documents::TextElement::FontSizeProperty.Handle());
    const Meta::PropertyValueSourceInfo owned =
        GetValueSourceInfo(FontSizeProperty.Handle());
    const double attachedSize = GetValue(Documents::TextElement::FontSizeProperty);
    const double ownedSize = GetValue(FontSizeProperty);
    const auto attachedRank =
        static_cast<std::uint8_t>(attached.rank);
    const auto ownedRank = static_cast<std::uint8_t>(owned.rank);

    if (attachedRank > ownedRank) return attachedSize;
    if (ownedRank > attachedRank) return ownedSize;
    constexpr double kDefaultSize = 15.0;
    if (attachedSize != kDefaultSize && ownedSize == kDefaultSize) {
        return attachedSize;
    }
    if (ownedSize != kDefaultSize && attachedSize == kDefaultSize) {
        return ownedSize;
    }
    return ownedSize;
}
Base::Ref<Media::FontFamily> TextBlock::GetFontFamily() const noexcept {
    return FrameworkElement::GetFontFamily();
}
FontWeight TextBlock::GetFontWeight() const noexcept {
    if (RuntimeType() == Documents::Bold::StaticTypeId()) {
        return FontWeight::Bold;
    }
    return GetValue(FontWeightProperty);
}
FontStyle TextBlock::GetFontStyle() const noexcept {
    if (RuntimeType() == Documents::Italic::StaticTypeId()) {
        return FontStyle::Italic;
    }
    return GetValue(FontStyleProperty);
}
TextDecorations TextBlock::GetTextDecorations() const noexcept {
    if (RuntimeType() == Documents::Underline::StaticTypeId()) {
        return TextDecorations::Underline;
    }
    return GetValue(TextDecorationsProperty);
}
TextWrapping TextBlock::GetTextWrapping() const noexcept {
    return GetValue(TextWrappingProperty);
}
TextTrimming TextBlock::GetTextTrimming() const noexcept {
    return GetValue(TextTrimmingProperty);
}
TextAlignment TextBlock::GetTextAlignment() const noexcept {
    return GetValue(TextAlignmentProperty);
}
double TextBlock::GetLineHeight() const noexcept {
    return GetValue(LineHeightProperty);
}
void TextBlock::SetText(Base::StringView value) noexcept {
    SetValue(TextProperty, value);
    textHitRegions_.Clear();
}
void TextBlock::SetForeground(
    Base::Ref<Brush> value) noexcept {
    SetValue(
        ForegroundProperty, std::move(value));
}
void TextBlock::SetBackground(
    Base::Ref<Brush> value) noexcept {
    SetValue(
        BackgroundProperty, std::move(value));
}
void TextBlock::SetFontSize(double value) noexcept {
    SetValue(FontSizeProperty, value);
}
void TextBlock::SetFontFamily(
    Base::Ref<Media::FontFamily> value) noexcept {
    FrameworkElement::SetFontFamily(std::move(value));
}
void TextBlock::SetFontFamily(
    Base::StringView value) noexcept {
    FrameworkElement::SetFontFamily(value);
}
void TextBlock::SetFontWeight(
    FontWeight value) noexcept {
    SetValue(FontWeightProperty, value);
}
void TextBlock::SetFontStyle(
    FontStyle value) noexcept {
    SetValue(FontStyleProperty, value);
}
void TextBlock::SetTextDecorations(
    TextDecorations value) noexcept {
    SetValue(TextDecorationsProperty, value);
}
void TextBlock::SetTextWrapping(
    TextWrapping value) noexcept {
    SetValue(TextWrappingProperty, value);
}
void TextBlock::SetTextTrimming(
    TextTrimming value) noexcept {
    SetValue(TextTrimmingProperty, value);
}
void TextBlock::SetTextAlignment(
    TextAlignment value) noexcept {
    SetValue(TextAlignmentProperty, value);
}
void TextBlock::SetLineHeight(double value) noexcept {
    SetValue(LineHeightProperty, value);
}
void TextBlock::SetRichTextStyleRanges(
    Base::Span<const RichTextStyleRange> ranges) noexcept {
    bool changed = richTextStyleRanges_.Size() != ranges.Size();
    if (!changed) {
        for (std::uint32_t index = 0U; index < ranges.Size(); ++index) {
            const RichTextStyleRange& left = richTextStyleRanges_[index];
            const RichTextStyleRange& right = ranges[index];
            if (left.start != right.start || left.length != right.length ||
                left.hasForeground != right.hasForeground ||
                left.bold != right.bold || left.italic != right.italic ||
                left.foreground.red != right.foreground.red ||
                left.foreground.green != right.foreground.green ||
                left.foreground.blue != right.foreground.blue ||
                left.foreground.alpha != right.foreground.alpha) {
                changed = true;
                break;
            }
        }
    }
    if (!changed) return;
    Base::Vector<RichTextStyleRange> next;
    next.Append(ranges);
    richTextStyleRanges_ = std::move(next);
    InvalidateVisual();
}
Meta::Value TextBlock::GetMetadataInlines() const noexcept {
    if (pendingInline_) {
        return Meta::Value::FromObject(
            pendingInline_->RuntimeType(),
            pendingInline_);
    }
    return Meta::Value::NullObject(
        Meta::TypeOf<Base::Object>());
}
void TextBlock::SetInlineValue(
    Meta::Value value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    if (value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() &&
        value.AsObject()) {
        Base::Ref<Base::Object> inlineObject = value.AsObject();
        if (!DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                inlineObject->RuntimeType(),
                Documents::Inline::StaticTypeId())) {
            return;
        }
        AddOwnedInline(inlineObject);
        return;
    }
    if (value.Kind() != Meta::ValueKind::String) {
        return;
    }
    Base::Result<Base::Ref<Documents::Run>> created =
        Base::MakeRef<Documents::Run>();
    if (!created) return;
    created->SetText(value.AsString());
    AddOwnedInline(Base::Ref<Base::Object>(created.Value()));
}
void TextBlock::AddOwnedInline(
    const Base::Ref<Base::Object>& inlineObject) noexcept {
    if (!inlineObject) { AERO_ASSERT(false); return; }
    Base::Result<void> access = VerifyAccess();
    if (!access) { AERO_ASSERT(false); return; }
    const TypeRegistry& types = DependencyObjectAccess::PropertyRegistry((*this)).Types();
    const TypeId type = inlineObject->RuntimeType();
    const bool supported = types.IsDerivedFrom(
        type, Documents::Inline::StaticTypeId());
    if (!supported) { AERO_ASSERT(false); return; }
    for (const Base::Ref<Base::Object>& owned :
         ownedInlines_) {
        if (owned.Get() == inlineObject.Get()) { AERO_ASSERT(false); return; }
    }
    ownedInlines_.PushBack(inlineObject);
    pendingInline_ = inlineObject;
    InvalidateDocumentText();
}
void TextBlock::ClearOwnedInlines() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    for (Base::Ref<Base::Object>& item : ownedInlines_) {
        if (item) {
            if (Documents::InlineUIContainer* container =
                    ::Aero::TryCast<Documents::InlineUIContainer>(item.Get())) {
                if (UIElement* child = container->GetChild()) {
                    RemoveVisualChild(child);
                }
            }
            (*static_cast<Documents::Inline*>(item.Get())).Detach();
        }
    }
    ownedInlines_.Clear();
    pendingInline_.Reset();
    flatTextValid_ = false;
    InvalidateMeasure();
}

namespace {

void CollectInlineUiChildren(
    const Documents::Inline& value,
    Base::Vector<UIElement*>& children,
    std::uint32_t depth = 0U) noexcept {
    if (depth >= 1024U) return;
    const Meta::TypeRegistry& types = DependencyObjectAccess::PropertyRegistry((value)).Types();
    if (types.IsDerivedFrom(
            value.RuntimeType(),
            Documents::InlineUIContainer::StaticTypeId())) {
        UIElement* child =
            static_cast<const Documents::InlineUIContainer&>(value).GetChild();
        if (child != nullptr) {
            static_cast<void>(children.PushBack(child));
        }
        return;
    }
    if (types.IsDerivedFrom(
            value.RuntimeType(), Documents::Span::StaticTypeId())) {
        const auto& span = static_cast<const Documents::Span&>(value);
        const Documents::InlineCollectionView inlines = span.GetInlines();
        for (std::uint32_t index = 0U; index < inlines.GetCount(); ++index) {
            const Documents::Inline* nested = inlines.GetItem(index);
            if (nested != nullptr) {
                CollectInlineUiChildren(*nested, children, depth + 1U);
            }
        }
    }
}

} // namespace

std::uint32_t TextBlock::GetVisualChildrenCount() const noexcept {
    Base::Vector<UIElement*> children;
    for (const Base::Ref<Base::Object>& item : ownedInlines_) {
        if (!item) continue;
        CollectInlineUiChildren(
            *static_cast<const Documents::Inline*>(item.Get()), children);
    }
    std::uint32_t count = 0U;
    for (UIElement* child : children) {
        if (child != nullptr && child->GetVisualParent() == this) {
            ++count;
        }
    }
    return count;
}

::Aero::Media::Visual* TextBlock::GetVisualChild(
    std::uint32_t index) const noexcept {
    Base::Vector<UIElement*> children;
    for (const Base::Ref<Base::Object>& item : ownedInlines_) {
        if (!item) continue;
        CollectInlineUiChildren(
            *static_cast<const Documents::Inline*>(item.Get()), children);
    }
    std::uint32_t current = 0U;
    for (UIElement* child : children) {
        if (child == nullptr || child->GetVisualParent() != this) continue;
        if (current == index) return child;
        ++current;
    }
    return nullptr;
}

Base::StringView TextBlock::EffectiveFontFamily() const noexcept {
    const Base::Ref<Media::FontFamily> family = GetFontFamily();
    const Base::StringView configured = family
        ? family->GetSource()
        : Base::StringView{};
    const bool defaultFamily =
        configured.Empty() ||
        configured == Base::StringView("Segoe UI");
    if (!defaultFamily) return configured;
    const bool bold =
        GetFontWeight() == FontWeight::Bold ||
        GetFontWeight() == FontWeight::SemiBold;
    const bool italic =
        GetFontStyle() != FontStyle::Normal;
    if (bold && italic) {
        return Base::StringView("Segoe UI Bold Italic");
    }
    if (bold) return Base::StringView("Segoe UI Bold");
    if (italic) return Base::StringView("Segoe UI Italic");
    return configured;
}
void TextBlock::SetGlyphRun(
    RenderGlyphRunId glyphRun, Size size) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    if (!IsValidTextSize(size) ||
        (glyphRun == InvalidRenderGlyphRunId &&
            (size.width != 0.0 || size.height != 0.0))) {
        return;
    }
    if (glyphRuns_.Size() == 1U && glyphRuns_[0] == glyphRun &&
        glyphRunSize_.width == size.width &&
        glyphRunSize_.height == size.height &&
        !serviceOwnsGlyphRun_) return;
    ReleaseServiceGlyphRun();
    glyphRuns_.Clear();
    textHitRegions_.Clear();
    if (glyphRun != InvalidRenderGlyphRunId) {
        glyphRuns_.PushBack(glyphRun);
    }
    glyphRunSize_ = size;
    InvalidateMeasure();
    InvalidateVisual();
}
Size TextBlock::MeasureOverride(Size availableSize) noexcept {
    if (!GetValue(RichText::TextProperty).Empty()) {
        // RichText can be populated while a template is still detached from
        // its inherited DataContext. Refresh at the first real measure so
        // inline bindings such as MusicLevel observe the mounted model.
        ApplyRichText(*this);
    }

    const Thickness padding =
        GetValue(PaddingProperty);

    if (!arrangingText_ &&
        flatTextValid_ &&
        GetTextWrapping() == TextWrapping::NoWrap &&
        GetTextTrimming() == TextTrimming::None &&
        !glyphRuns_.Empty() &&
        GetVisualChildrenCount() == 0U) {
        Size desired{
            glyphRunSize_.width + padding.left + padding.right,
            glyphRunSize_.height + padding.top + padding.bottom};
        if (std::isfinite(availableSize.width) && availableSize.width > 0.0) {
            desired.width = std::min(desired.width, availableSize.width);
        }
        if (std::isfinite(availableSize.height) && availableSize.height > 0.0) {
            desired.height = std::min(desired.height, availableSize.height);
        }
        return desired;
    }

    Base::String flattened;
    Documents::CopyText(*this, flattened);

    Size textAvailable = availableSize;
    if (std::isfinite(textAvailable.width) && textAvailable.width > 0.0) {
        textAvailable.width = std::max(
            0.0, textAvailable.width - padding.left - padding.right);
    }
    if (std::isfinite(textAvailable.height) && textAvailable.height > 0.0) {
        textAvailable.height = std::max(
            0.0, textAvailable.height - padding.top - padding.bottom);
    }

    auto* layoutService = TextLayoutFor(*this);
    const Base::StringView shapedText = flattened.View();
    const auto fallbackTextSize = [&]() noexcept -> Size {
        if (shapedText.Empty()) return {};
        const double fontSize = std::max(1.0, GetFontSize());
        const double units =
            static_cast<double>(std::max(1U, shapedText.SizeBytes()));
        return Size{fontSize * 0.55 * units, fontSize * 1.25};
    };
    if (layoutService != nullptr) {
        const Base::StringView text = flattened.View();
        if (text.Empty()) {
            const bool changed =
                !glyphRuns_.Empty() ||
                glyphRunSize_.width != 0.0 ||
                glyphRunSize_.height != 0.0;
            ReleaseServiceGlyphRun();
            glyphRuns_.Clear();
            glyphRunSize_ = {};
            textHitRegions_.Clear();
            if (changed) {
                InvalidateVisual();
            }
        } else {
            ::Aero::Controls::TextLayoutRequest request;
            request.text = text;
            request.availableSize = textAvailable;
            request.dpiScale = GetDpiScale();
            request.pixelSize = static_cast<float>(GetFontSize());
            request.lineHeight = static_cast<float>(GetLineHeight());
            request.fontFamily = EffectiveFontFamily();
            request.fontWeight = GetFontWeight();
            request.fontStyle = GetFontStyle();
            request.wrapping = GetTextWrapping();
            request.trimming = GetTextTrimming();
            request.alignment = GetTextAlignment();
            request.arrangeToAvailableWidth = arrangingText_;
            request.direction = GetFlowDirection() == FlowDirection::RightToLeft
                ? Text::TextDirection::RightToLeft
                : Text::TextDirection::LeftToRight;
            ::Aero::Controls::TextLayoutResult output;
            Base::Result<void> prepared =
                layoutService->ShapeAndPrepare(request, output);
            if (!prepared) {
                glyphRunSize_ = fallbackTextSize();
            } else {
                bool validGlyphRuns = true;
                for (RenderGlyphRunId glyphRun : output.glyphRuns) {
                    if (glyphRun == InvalidRenderGlyphRunId) {
                        validGlyphRuns = false;
                        break;
                    }
                }
                if (!IsValidTextSize(output.desiredSize) || !validGlyphRuns) {
                    for (RenderGlyphRunId glyphRun : output.glyphRuns) {
                        if (glyphRun != InvalidRenderGlyphRunId) {
                            layoutService->ReleaseGlyphRun(glyphRun);
                        }
                    }
                    glyphRunSize_ = fallbackTextSize();
                } else {
                    bool changed =
                        glyphRuns_.Size() != output.glyphRuns.Size() ||
                        glyphRunSize_.width != output.desiredSize.width ||
                        glyphRunSize_.height != output.desiredSize.height ||
                        !serviceOwnsGlyphRun_;
                    if (!changed) {
                        for (std::uint32_t index = 0U;
                             index < glyphRuns_.Size(); ++index) {
                            if (glyphRuns_[index] != output.glyphRuns[index]) {
                                changed = true;
                                break;
                            }
                        }
                    }
                    ReleaseServiceGlyphRun();
                    glyphRuns_ = std::move(output.glyphRuns);
                    textHitRegions_ = std::move(output.hitRegions);
                    glyphRunSize_ = output.desiredSize;
                    serviceOwnsGlyphRun_ = !glyphRuns_.Empty();
                    flatTextValid_ = true;
                    if (changed) {
                        InvalidateVisual();
                    }
                }
            }
        }
    } else {
        glyphRunSize_ = fallbackTextSize();
    }
    Size desired{
        glyphRunSize_.width + padding.left + padding.right,
        glyphRunSize_.height + padding.top + padding.bottom};
    const std::uint32_t uiCount = GetVisualChildrenCount();
    for (std::uint32_t index = 0U; index < uiCount; ++index) {
        auto* child = static_cast<UIElement*>(GetVisualChild(index));
        if (child == nullptr) continue;
        MeasureChild(*child, availableSize);
        const Size childSize = child->GetDesiredSize();
        desired.width = std::max(desired.width, childSize.width);
        desired.height = std::max(desired.height, childSize.height);
    }
    return desired;
}
Size TextBlock::ArrangeOverride(Size finalSize) noexcept {
    const Thickness padding =
        GetValue(PaddingProperty);
    const bool needsAlignment = GetTextAlignment() != TextAlignment::Left ||
        GetFlowDirection() == FlowDirection::RightToLeft;
    if (needsAlignment && finalSize.width > 0.0) {
        arrangingText_ = true;
        static_cast<void>(MeasureOverride(finalSize));
        arrangingText_ = false;
    }
    const std::uint32_t uiCount = GetVisualChildrenCount();
    std::uint32_t placeholder = 0U;
    for (std::uint32_t index = 0U; index < uiCount; ++index) {
        auto* child = static_cast<UIElement*>(GetVisualChild(index));
        if (child == nullptr) continue;
        Rect slot{padding.left, padding.top, child->GetDesiredSize().width,
            child->GetDesiredSize().height};
        while (placeholder < textHitRegions_.Size()) {
            const TextHitRegion& region = textHitRegions_[placeholder];
            ++placeholder;
            if (region.textLength == 3U) {
                slot = Rect{
                    static_cast<double>(region.x) + padding.left,
                    static_cast<double>(region.y) + padding.top,
                    std::max(static_cast<double>(region.width),
                        child->GetDesiredSize().width),
                    std::max(static_cast<double>(region.height),
                        child->GetDesiredSize().height)};
                break;
            }
        }
        ArrangeChild(*child, slot);
    }
    return finalSize;
}
void TextBlock::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    const Size renderSize = GetRenderSize();
    if (renderSize.width <= 0.0 || renderSize.height <= 0.0) return;
    auto& builder = Aero::Render::DrawingBridge::Builder(context);
    const Color background = ::Aero::Media::SampleBrush(GetBackground());
    if (background.alpha > 0.0F) {
        builder.FillRect(
            {0.0, 0.0,
             GetRenderSize().width,
             GetRenderSize().height},
            background);
    }
    const Thickness padding =
        GetValue(PaddingProperty);
    const bool shiftGlyphs =
        padding.left != 0.0 || padding.top != 0.0;
    if (shiftGlyphs) {
        Base::Transform2D origin;
        origin.dx = padding.left;
        origin.dy = padding.top;
        builder.PushTransform(origin);
    }
    struct RichTextLineClip {
        float x = 0.0F;
        float y = 0.0F;
        float right = 0.0F;
        float height = 0.0F;
    };

    bool hasStyledRanges = false;
    for (const RichTextStyleRange& range : richTextStyleRanges_) {
        if (range.length > 0U &&
            (range.hasForeground || range.italic ||
             (range.bold && GetFontWeight() != FontWeight::Bold))) {
            hasStyledRanges = true;
            break;
        }
    }

    if (!hasStyledRanges || textHitRegions_.Empty()) {
        for (RenderGlyphRunId glyphRun : glyphRuns_) {
            builder.DrawGlyphRun(glyphRun, ::Aero::Media::SampleBrush(GetForeground(), 0.5,
                Color{0.0F, 0.0F, 0.0F, 1.0F}));
        }
    } else {
        struct StyleSpan {
            std::uint32_t start = 0U;
            std::uint32_t length = 0U;
            Color foreground{};
            bool hasForeground = false;
            bool bold = false;
            bool italic = false;
        };

        std::uint32_t maxTextEnd = 0U;
        for (const TextHitRegion& region : textHitRegions_) {
            maxTextEnd = std::max(
                maxTextEnd,
                static_cast<std::uint32_t>(region.textOffset + region.textLength));
        }
        for (const RichTextStyleRange& range : richTextStyleRanges_) {
            maxTextEnd = std::max(maxTextEnd, range.start + range.length);
        }

        Base::Vector<StyleSpan> spans;
        std::uint32_t currentOffset = 0U;
        for (const RichTextStyleRange& range : richTextStyleRanges_) {
            if (range.length == 0U) continue;
            if (range.start > currentOffset) {
                StyleSpan unstyled;
                unstyled.start = currentOffset;
                unstyled.length = range.start - currentOffset;
                spans.PushBack(unstyled);
            }
            const bool isStyled = range.hasForeground || range.italic ||
                (range.bold && GetFontWeight() != FontWeight::Bold);
            if (isStyled) {
                StyleSpan styled;
                styled.start = range.start;
                styled.length = range.length;
                styled.foreground = range.foreground;
                styled.hasForeground = range.hasForeground;
                styled.bold = range.bold && GetFontWeight() != FontWeight::Bold;
                styled.italic = range.italic;
                spans.PushBack(styled);
            } else {
                StyleSpan unstyled;
                unstyled.start = range.start;
                unstyled.length = range.length;
                spans.PushBack(unstyled);
            }
            currentOffset = std::max(currentOffset, range.start + range.length);
        }
        if (currentOffset < maxTextEnd) {
            StyleSpan unstyled;
            unstyled.start = currentOffset;
            unstyled.length = maxTextEnd - currentOffset;
            spans.PushBack(unstyled);
        }

        for (const StyleSpan& span : spans) {
            if (span.length == 0U) continue;
            Base::Vector<RichTextLineClip> clips;
            const std::uint64_t spanEnd =
                static_cast<std::uint64_t>(span.start) + span.length;
            for (const TextHitRegion& region : textHitRegions_) {
                const std::uint64_t regionEnd =
                    static_cast<std::uint64_t>(region.textOffset) +
                    region.textLength;
                if (region.textLength == 0U ||
                    regionEnd <= span.start ||
                    region.textOffset >= spanEnd) {
                    continue;
                }
                RichTextLineClip* line = nullptr;
                for (RichTextLineClip& candidate : clips) {
                    if (std::fabs(candidate.y - region.y) < 0.01F) {
                        line = &candidate;
                        break;
                    }
                }
                if (line == nullptr) {
                    RichTextLineClip added;
                    added.x = region.x;
                    added.y = region.y;
                    added.right = region.x + region.width;
                    added.height = region.height;
                    clips.PushBack(added);
                } else {
                    line->x = std::min(line->x, region.x);
                    line->right = std::max(
                        line->right, region.x + region.width);
                    line->height = std::max(line->height, region.height);
                }
            }
            if (clips.Empty()) continue;

            const Color tint = span.hasForeground
                ? span.foreground
                : ::Aero::Media::SampleBrush(
                    GetForeground(), 0.5,
                    Color{0.0F, 0.0F, 0.0F, 1.0F});

            for (const RichTextLineClip& line : clips) {
                const double extraRight = span.italic
                    ? std::max(3.0, 0.25 * line.height + 2.0)
                    : (span.bold ? 1.0 : 0.5);
                const double extraLeft = span.italic ? 1.0 : 0.0;
                const Rect clip{
                    std::max(0.0, static_cast<double>(line.x) - extraLeft),
                    std::max(0.0, static_cast<double>(line.y)),
                    std::max(0.0, static_cast<double>(line.right - line.x) + extraLeft + extraRight),
                    std::max(0.0, static_cast<double>(line.height))};
                builder.PushClip(clip);
                bool skewed = false;
                if (span.italic) {
                    Base::Transform2D italic;
                    italic.m21 = -0.16;
                    italic.dx = 0.16 * (line.y + line.height);
                    builder.PushTransform(italic);
                    skewed = true;
                }
                for (RenderGlyphRunId glyphRun : glyphRuns_) {
                    builder.DrawGlyphRun(glyphRun, tint);
                }
                if (span.bold) {
                    Base::Transform2D embolden;
                    embolden.dx = 0.4;
                    builder.PushTransform(embolden);
                    for (RenderGlyphRunId glyphRun : glyphRuns_) {
                        builder.DrawGlyphRun(glyphRun, tint);
                    }
                    builder.PopTransform();
                }
                if (skewed) builder.PopTransform();
                builder.PopClip();
            }
        }
    }
    if (GetTextDecorations() ==
            TextDecorations::Underline &&
        glyphRunSize_.width > 0.0) {
        const double thickness =
            std::max(1.0, GetFontSize() * 0.06);
        const double y = std::max(
            0.0,
            glyphRunSize_.height - thickness * 1.5);
        builder.FillRect(
            {0.0, y, glyphRunSize_.width, thickness},
            ::Aero::Media::SampleBrush(GetForeground(), 0.5,
                Color{0.0F, 0.0F, 0.0F, 1.0F}));
    }
    if (shiftGlyphs) {
        builder.PopTransform();
    }
    return;
}
void TextBlock::ReleaseServiceGlyphRun() noexcept {
    auto* layoutService = TextLayoutFor(*this);
    if (serviceOwnsGlyphRun_ && layoutService != nullptr) {
        for (RenderGlyphRunId glyphRun : glyphRuns_) {
            layoutService->ReleaseGlyphRun(glyphRun);
        }
    }
    serviceOwnsGlyphRun_ = false;
}

namespace {

void AddTextBlockInline(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    auto& text = static_cast<TextBlock&>(owner);
    if (!DependencyObjectAccess::PropertyRegistry((text)).Types().IsDerivedFrom(
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

} // namespace

AERO_DESCRIBE(TextBlock) {
    using namespace Aero::Meta;
    using namespace Aero::Media;

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

    ::Aero::Documents::PopulateDocumentsMetadata(context);
}

} // namespace Aero::Controls

