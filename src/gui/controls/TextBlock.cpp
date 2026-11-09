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

struct TextBlock::DocumentRare {
    Base::Vector<std::uint64_t> extraGlyphRuns;
    Base::Vector<TextHitRegion> textHitRegions;
    Base::Vector<Ref<Documents::Inline>> ownedInlines;
    Base::Vector<UIElement*> scratchVisualChildren;
    Base::String flatText;
    Base::Vector<RichTextStyleRange> richTextStyleRanges;
    Ref<Documents::Inline> pendingInline;
    bool hasBuiltHitRegions = false;
};

TextBlock::DocumentRare* TextBlock::EnsureDocumentRare() const noexcept {
    if (docRare_ == nullptr) {
        docRare_ = new (std::nothrow) DocumentRare();
    }
    return docRare_;
}

std::uint32_t TextBlock::GetInlineCount() const noexcept {
    return docRare_ != nullptr ? docRare_->ownedInlines.Size() : 0U;
}

Base::Vector<Ref<Documents::Inline>>* TextBlock::EnsureOwnedInlinesVector() noexcept {
    DocumentRare* rare = EnsureDocumentRare();
    return rare != nullptr ? &rare->ownedInlines : nullptr;
}

const Base::Vector<Ref<Documents::Inline>>* TextBlock::OwnedInlinesVector() const noexcept {
    return docRare_ != nullptr ? &docRare_->ownedInlines : nullptr;
}

void TextBlock::SetPendingInline(Ref<Documents::Inline> value) noexcept {
    if (!value && docRare_ == nullptr) return;
    if (DocumentRare* rare = EnsureDocumentRare()) {
        rare->pendingInline = std::move(value);
    }
}

bool TextBlock::AccessDocumentFlattenState(
    Base::String*& outFlatText,
    Base::Vector<RichTextStyleRange>*& outStyleRanges) const noexcept {
    if (docRare_ == nullptr || docRare_->ownedInlines.Empty()) {
        return false;
    }
    outFlatText = &docRare_->flatText;
    outStyleRanges = &docRare_->richTextStyleRanges;
    return true;
}

TextBlock::TextBlock() noexcept
    : TextBlock(StaticTypeId()) {}

TextBlock::TextBlock(TypeId runtimeType) noexcept
    : FrameworkElement(runtimeType) {}

void TextBlock::InvalidateDocumentText() noexcept {
    flatTextValid_ = false;
    glyphRunValid_ = false;
    if (docRare_ != nullptr) {
        docRare_->hasBuiltHitRegions = false;
        if (docRare_->ownedInlines.Empty() && GetValue(RichText::TextProperty).Empty()) {
            docRare_->richTextStyleRanges.Clear();
        }
    }
    InvalidateMeasure();
}

void TextBlock::OnPropertyInvalidated(
    PropertyInvalidationFlags flags) noexcept {
    flatTextValid_ = false;
    glyphRunValid_ = false;
    if (docRare_ != nullptr) {
        docRare_->hasBuiltHitRegions = false;
    }
    FrameworkElement::OnPropertyInvalidated(flags);
}

TextBlock::~TextBlock() {
    ClearOwnedInlines();
    ReleaseServiceGlyphRun();
    delete docRare_;
    docRare_ = nullptr;
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
    if (docRare_ != nullptr) {
        docRare_->textHitRegions.Clear();
        docRare_->hasBuiltHitRegions = false;
    }
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
    if (ranges.Empty() && (docRare_ == nullptr || docRare_->richTextStyleRanges.Empty())) {
        return;
    }
    DocumentRare* rare = EnsureDocumentRare();
    if (rare == nullptr) return;
    bool changed = rare->richTextStyleRanges.Size() != ranges.Size();
    if (!changed) {
        for (std::uint32_t index = 0U; index < ranges.Size(); ++index) {
            const RichTextStyleRange& left = rare->richTextStyleRanges[index];
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
    rare->richTextStyleRanges = std::move(next);
    InvalidateVisual();
}
Meta::Value TextBlock::GetMetadataInlines() const noexcept {
    if (docRare_ != nullptr && docRare_->pendingInline) {
        return Meta::Value::FromObject(
            docRare_->pendingInline->RuntimeType(),
            docRare_->pendingInline);
    }
    return Meta::Value::NullObject(
        Meta::TypeOf<Base::Object>());
}
void TextBlock::SetInlineValue(
    Meta::Value value) noexcept {
    if (!VerifyAccess()) return;
    if (value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() &&
        value.AsObject()) {
        auto* inlinePtr = ::Aero::TryCast<Documents::Inline>(value.AsObject().Get());
        if (inlinePtr == nullptr) {
            return;
        }
        AddOwnedInline(Base::Ref<Documents::Inline>::FromBorrowed(*inlinePtr));
        return;
    }
    if (value.Kind() != Meta::ValueKind::String) {
        return;
    }
    Base::Ref<Documents::Run> created = Base::MakeRef<Documents::Run>();
    if (!created) return;
    created->SetText(value.AsString());
    AddOwnedInline(std::move(created));
}
void TextBlock::AddOwnedInline(
    Base::Ref<Documents::Inline> inlineObject) noexcept {
    if (!inlineObject) { AERO_ASSERT(false); return; }
    if (!VerifyAccess()) { AERO_ASSERT(false); return; }
    DocumentRare* rare = EnsureDocumentRare();
    if (rare == nullptr) return;
    for (const Base::Ref<Documents::Inline>& owned : rare->ownedInlines) {
        if (owned.Get() == inlineObject.Get()) { AERO_ASSERT(false); return; }
    }
    rare->pendingInline = inlineObject;
    rare->ownedInlines.PushBack(std::move(inlineObject));
    InvalidateDocumentText();
}
void TextBlock::ClearOwnedInlines() noexcept {
    if (!VerifyAccess()) return;
    if (docRare_ == nullptr) return;
    for (Base::Ref<Documents::Inline>& item : docRare_->ownedInlines) {
        if (item) {
            if (Documents::InlineUIContainer* container =
                    ::Aero::TryCast<Documents::InlineUIContainer>(item.Get())) {
                if (UIElement* child = container->GetChild()) {
                    RemoveVisualChild(child);
                }
            }
            item->Detach();
        }
    }
    docRare_->ownedInlines.Clear();
    docRare_->pendingInline.Reset();
    if (GetValue(RichText::TextProperty).Empty()) {
        docRare_->richTextStyleRanges.Clear();
    }
    flatTextValid_ = false;
    glyphRunValid_ = false;
    docRare_->hasBuiltHitRegions = false;
    InvalidateMeasure();
}

namespace {

void CollectInlineUiChildren(
    const Documents::Inline& value,
    Base::Vector<UIElement*>& children,
    std::uint32_t depth = 0U) noexcept {
    if (depth >= 1024U) return;
    if (const auto* container = ::Aero::TryCast<Documents::InlineUIContainer>(&value)) {
        UIElement* child = container->GetChild();
        if (child != nullptr) {
            static_cast<void>(children.PushBack(child));
        }
        return;
    }
    if (const auto* span = ::Aero::TryCast<Documents::Span>(&value)) {
        const Documents::InlineCollectionView inlines = span->GetInlines();
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
    if (docRare_ == nullptr || docRare_->ownedInlines.Empty()) return 0U;
    Base::Vector<UIElement*>& children = docRare_->scratchVisualChildren;
    children.Clear();
    for (const Base::Ref<Documents::Inline>& item : docRare_->ownedInlines) {
        if (!item) continue;
        CollectInlineUiChildren(*item, children);
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
    if (docRare_ == nullptr || docRare_->ownedInlines.Empty()) return nullptr;
    Base::Vector<UIElement*>& children = docRare_->scratchVisualChildren;
    children.Clear();
    for (const Base::Ref<Documents::Inline>& item : docRare_->ownedInlines) {
        if (!item) continue;
        CollectInlineUiChildren(*item, children);
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
    if (!VerifyAccess()) return;
    if (!IsValidTextSize(size) ||
        (glyphRun == InvalidRenderGlyphRunId &&
            (size.width != 0.0 || size.height != 0.0))) {
        return;
    }
    const bool hasExtra = docRare_ != nullptr && !docRare_->extraGlyphRuns.Empty();
    if (!hasExtra && primaryGlyphRun_ == glyphRun &&
        glyphRunSize_.width == size.width &&
        glyphRunSize_.height == size.height &&
        !serviceOwnsGlyphRun_) return;
    ReleaseServiceGlyphRun();
    primaryGlyphRun_ = glyphRun;
    glyphRunValid_ = false;
    if (docRare_ != nullptr) {
        docRare_->extraGlyphRuns.Clear();
        docRare_->textHitRegions.Clear();
        docRare_->hasBuiltHitRegions = false;
    }
    glyphRunSize_ = size;
    InvalidateMeasure();
    InvalidateVisual();
}

Span<const TextHitRegion> TextBlock::HitRegionsSpan() const noexcept {
    if (docRare_ != nullptr && docRare_->hasBuiltHitRegions) {
        return docRare_->textHitRegions.AsSpan();
    }
    if (primaryGlyphRun_ == InvalidRenderGlyphRunId) {
        return {};
    }
    auto* layoutService = TextLayoutFor(*this);
    if (layoutService == nullptr) return {};
    DocumentRare* rare = EnsureDocumentRare();
    if (rare == nullptr) return {};
    Base::String flattened;
    const bool hasInlines = !rare->ownedInlines.Empty() || !GetValue(RichText::TextProperty).Empty();
    Base::StringView text = GetText();
    if (hasInlines) {
        Documents::CopyText(*this, flattened);
        text = flattened.View();
    }
    if (text.Empty()) return {};
    Size textAvailable{};
    if (lastShapedAvailableWidth_ >= 0.0f) {
        textAvailable.width = lastShapedAvailableWidth_;
    } else {
        const Thickness padding = GetValue(PaddingProperty);
        const Size renderSize = GetRenderSize();
        if (renderSize.width > 0.0f) {
            textAvailable.width = std::max(0.0f, renderSize.width - padding.left - padding.right);
        }
    }
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
    request.arrangeToAvailableWidth = lastShapedArrangeMode_;
    request.includeHitRegions = true;
    request.direction = GetFlowDirection() == FlowDirection::RightToLeft
        ? Text::TextDirection::RightToLeft
        : Text::TextDirection::LeftToRight;
    ::Aero::Controls::TextLayoutResult output;
    if (layoutService->ShapeAndPrepare(request, output)) {
        for (RenderGlyphRunId run : output.glyphRuns) {
            if (run != InvalidRenderGlyphRunId) {
                layoutService->ReleaseGlyphRun(run);
            }
        }
        rare->textHitRegions = std::move(output.hitRegions);
        rare->hasBuiltHitRegions = true;
    }
    return rare->textHitRegions.AsSpan();
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

    Size cachedTextAvailable = availableSize;
    if (std::isfinite(cachedTextAvailable.width) && cachedTextAvailable.width > 0.0f) {
        cachedTextAvailable.width = std::max(
            0.0f, cachedTextAvailable.width - padding.left - padding.right);
    }
    const bool widthMatchesCache =
        (GetTextWrapping() == TextWrapping::NoWrap &&
         GetTextTrimming() == TextTrimming::None &&
         !arrangingText_ && !lastShapedArrangeMode_) ||
        (std::isfinite(cachedTextAvailable.width) &&
         std::isfinite(lastShapedAvailableWidth_) &&
         std::fabs(cachedTextAvailable.width - lastShapedAvailableWidth_) <= 0.01f &&
         arrangingText_ == lastShapedArrangeMode_);
    if (glyphRunValid_ &&
        widthMatchesCache &&
        primaryGlyphRun_ != InvalidRenderGlyphRunId &&
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

    const bool hasInlines = docRare_ != nullptr && !docRare_->ownedInlines.Empty();
    Base::String flattened;
    Base::StringView shapedText;
    if (hasInlines) {
        Documents::CopyText(*this, flattened);
        shapedText = flattened.View();
    } else {
        shapedText = GetText();
    }

    Size textAvailable = availableSize;
    if (std::isfinite(textAvailable.width) && textAvailable.width > 0.0f) {
        textAvailable.width = std::max(
            0.0f, textAvailable.width - padding.left - padding.right);
    }
    if (std::isfinite(textAvailable.height) && textAvailable.height > 0.0f) {
        textAvailable.height = std::max(
            0.0f, textAvailable.height - padding.top - padding.bottom);
    }

    auto* layoutService = TextLayoutFor(*this);
    const auto fallbackTextSize = [&]() noexcept -> Size {
        if (shapedText.Empty()) return {};
        const double fontSize = std::max(1.0, GetFontSize());
        const double units =
            static_cast<double>(std::max(1U, shapedText.SizeBytes()));
        return Size{fontSize * 0.55 * units, fontSize * 1.25};
    };
    if (layoutService != nullptr) {
        const Base::StringView text = shapedText;
        if (text.Empty()) {
            const bool changed =
                primaryGlyphRun_ != InvalidRenderGlyphRunId ||
                glyphRunSize_.width != 0.0 ||
                glyphRunSize_.height != 0.0;
            ReleaseServiceGlyphRun();
            primaryGlyphRun_ = InvalidRenderGlyphRunId;
            glyphRunValid_ = false;
            glyphRunSize_ = {};
            if (docRare_ != nullptr) {
                docRare_->extraGlyphRuns.Clear();
                docRare_->textHitRegions.Clear();
                docRare_->hasBuiltHitRegions = false;
            }
            if (changed) {
                InvalidateVisual();
            }
        } else {
            const bool needsHitRegions = docRare_ != nullptr &&
                (!docRare_->ownedInlines.Empty() || !docRare_->richTextStyleRanges.Empty());
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
            request.includeHitRegions = needsHitRegions;
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
                    const std::uint32_t oldCount =
                        (primaryGlyphRun_ != InvalidRenderGlyphRunId ? 1U : 0U) +
                        (docRare_ != nullptr ? docRare_->extraGlyphRuns.Size() : 0U);
                    bool changed =
                        oldCount != output.glyphRuns.Size() ||
                        glyphRunSize_.width != output.desiredSize.width ||
                        glyphRunSize_.height != output.desiredSize.height ||
                        !serviceOwnsGlyphRun_;
                    if (!changed && !output.glyphRuns.Empty()) {
                        if (primaryGlyphRun_ != output.glyphRuns[0]) {
                            changed = true;
                        } else if (docRare_ != nullptr) {
                            for (std::uint32_t i = 1U; i < output.glyphRuns.Size(); ++i) {
                                if (docRare_->extraGlyphRuns[i - 1U] != output.glyphRuns[i]) {
                                    changed = true;
                                    break;
                                }
                            }
                        }
                    }
                    ReleaseServiceGlyphRun();
                    if (output.glyphRuns.Empty()) {
                        primaryGlyphRun_ = InvalidRenderGlyphRunId;
                        if (docRare_ != nullptr) docRare_->extraGlyphRuns.Clear();
                    } else {
                        primaryGlyphRun_ = output.glyphRuns[0];
                        if (output.glyphRuns.Size() > 1U) {
                            DocumentRare* rare = EnsureDocumentRare();
                            bool storedExtra = false;
                            if (rare != nullptr) {
                                rare->extraGlyphRuns.Clear();
                                rare->extraGlyphRuns.Append(
                                    output.glyphRuns.AsSpan().Subspan(1U, output.glyphRuns.Size() - 1U));
                                storedExtra = rare->extraGlyphRuns.Size() == output.glyphRuns.Size() - 1U;
                            }
                            if (!storedExtra) {
                                for (std::uint32_t i = 1U; i < output.glyphRuns.Size(); ++i) {
                                    layoutService->ReleaseGlyphRun(output.glyphRuns[i]);
                                }
                            }
                        } else if (docRare_ != nullptr) {
                            docRare_->extraGlyphRuns.Clear();
                        }
                    }
                    if (needsHitRegions) {
                        if (DocumentRare* rare = EnsureDocumentRare()) {
                            rare->textHitRegions = std::move(output.hitRegions);
                            rare->hasBuiltHitRegions = true;
                        }
                    } else if (docRare_ != nullptr) {
                        docRare_->textHitRegions.Clear();
                        docRare_->hasBuiltHitRegions = false;
                    }
                    glyphRunSize_ = output.desiredSize;
                    lastShapedAvailableWidth_ = textAvailable.width;
                    lastShapedArrangeMode_ = arrangingText_;
                    serviceOwnsGlyphRun_ = primaryGlyphRun_ != InvalidRenderGlyphRunId;
                    glyphRunValid_ = true;
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
    if (needsAlignment && finalSize.width != GetRenderSize().width) {
        InvalidateVisual();
    }
    const float contentWidth = std::max(
        0.0f, finalSize.width - padding.left - padding.right);
    const float extraWidth = std::max(0.0f, contentWidth - glyphRunSize_.width);
    float alignOffsetX = 0.0f;
    if (GetTextAlignment() == TextAlignment::Center) {
        alignOffsetX = extraWidth * 0.5f;
    } else if (GetTextAlignment() == TextAlignment::Right ||
               (GetTextAlignment() == TextAlignment::Left &&
                GetFlowDirection() == FlowDirection::RightToLeft)) {
        alignOffsetX = extraWidth;
    }
    const std::uint32_t uiCount = GetVisualChildrenCount();
    const Span<const TextHitRegion> hitRegions = uiCount > 0U ? HitRegionsSpan() : Span<const TextHitRegion>{};
    std::uint32_t placeholder = 0U;
    for (std::uint32_t index = 0U; index < uiCount; ++index) {
        auto* child = static_cast<UIElement*>(GetVisualChild(index));
        if (child == nullptr) continue;
        Rect slot{padding.left + alignOffsetX, padding.top, child->GetDesiredSize().width,
            child->GetDesiredSize().height};
        while (placeholder < hitRegions.Size()) {
            const TextHitRegion& region = hitRegions[placeholder];
            ++placeholder;
            if (region.textLength == 3U) {
                slot = Rect{
                    static_cast<float>(region.x) + padding.left + alignOffsetX,
                    static_cast<float>(region.y) + padding.top,
                    std::max(static_cast<float>(region.width),
                        child->GetDesiredSize().width),
                    std::max(static_cast<float>(region.height),
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
    const float contentWidth = std::max(
        0.0f, renderSize.width - padding.left - padding.right);
    const float extraWidth = std::max(0.0f, contentWidth - glyphRunSize_.width);
    float alignOffsetX = 0.0f;
    if (GetTextAlignment() == TextAlignment::Center) {
        alignOffsetX = extraWidth * 0.5f;
    } else if (GetTextAlignment() == TextAlignment::Right ||
               (GetTextAlignment() == TextAlignment::Left &&
                GetFlowDirection() == FlowDirection::RightToLeft)) {
        alignOffsetX = extraWidth;
    }
    const float originX = padding.left + alignOffsetX;
    const float originY = padding.top;
    const bool shiftGlyphs =
        originX != 0.0f || originY != 0.0f;
    if (shiftGlyphs) {
        Base::Transform2D origin;
        origin.dx = originX;
        origin.dy = originY;
        builder.PushTransform(origin);
    }
    struct RichTextLineClip {
        float x = 0.0F;
        float y = 0.0F;
        float right = 0.0F;
        float height = 0.0F;
    };

    const auto emitAllGlyphRuns = [&](Color color) noexcept {
        if (primaryGlyphRun_ != InvalidRenderGlyphRunId) {
            builder.DrawGlyphRun(primaryGlyphRun_, color);
        }
        if (docRare_ != nullptr) {
            for (RenderGlyphRunId glyphRun : docRare_->extraGlyphRuns) {
                builder.DrawGlyphRun(glyphRun, color);
            }
        }
    };

    bool hasStyledRanges = false;
    if (docRare_ != nullptr) {
        for (const RichTextStyleRange& range : docRare_->richTextStyleRanges) {
            if (range.length > 0U &&
                (range.hasForeground || range.italic ||
                 (range.bold && GetFontWeight() != FontWeight::Bold))) {
                hasStyledRanges = true;
                break;
            }
        }
    }

    const Span<const TextHitRegion> hitRegions = hasStyledRanges ? HitRegionsSpan() : Span<const TextHitRegion>{};
    if (!hasStyledRanges || hitRegions.Empty()) {
        emitAllGlyphRuns(::Aero::Media::SampleBrush(GetForeground(), 0.5,
            Color{0.0F, 0.0F, 0.0F, 1.0F}));
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
        for (const TextHitRegion& region : hitRegions) {
            maxTextEnd = std::max(
                maxTextEnd,
                static_cast<std::uint32_t>(region.textOffset + region.textLength));
        }
        for (const RichTextStyleRange& range : docRare_->richTextStyleRanges) {
            maxTextEnd = std::max(maxTextEnd, range.start + range.length);
        }

        Base::Vector<StyleSpan> spans;
        const auto appendSpan = [&spans](const StyleSpan& candidate) noexcept {
            if (candidate.length == 0U) return;
            if (!spans.Empty()) {
                StyleSpan& prev = spans[spans.Size() - 1U];
                const bool sameColor = !prev.hasForeground ||
                    (prev.foreground.red == candidate.foreground.red &&
                     prev.foreground.green == candidate.foreground.green &&
                     prev.foreground.blue == candidate.foreground.blue &&
                     prev.foreground.alpha == candidate.foreground.alpha);
                if (prev.start + prev.length == candidate.start &&
                    prev.hasForeground == candidate.hasForeground &&
                    sameColor &&
                    prev.bold == candidate.bold &&
                    prev.italic == candidate.italic) {
                    prev.length += candidate.length;
                    return;
                }
            }
            spans.PushBack(candidate);
        };
        std::uint32_t currentOffset = 0U;
        for (const RichTextStyleRange& range : docRare_->richTextStyleRanges) {
            if (range.length == 0U) continue;
            if (range.start > currentOffset) {
                StyleSpan unstyled;
                unstyled.start = currentOffset;
                unstyled.length = range.start - currentOffset;
                appendSpan(unstyled);
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
                appendSpan(styled);
            } else {
                StyleSpan unstyled;
                unstyled.start = range.start;
                unstyled.length = range.length;
                appendSpan(unstyled);
            }
            currentOffset = std::max(currentOffset, range.start + range.length);
        }
        if (currentOffset < maxTextEnd) {
            StyleSpan unstyled;
            unstyled.start = currentOffset;
            unstyled.length = maxTextEnd - currentOffset;
            appendSpan(unstyled);
        }

        if (spans.Size() == 1U && !spans[0].italic) {
            const StyleSpan& span = spans[0];
            const Color tint = span.hasForeground
                ? span.foreground
                : ::Aero::Media::SampleBrush(
                    GetForeground(), 0.5,
                    Color{0.0F, 0.0F, 0.0F, 1.0F});
            emitAllGlyphRuns(tint);
            if (span.bold) {
                Base::Transform2D embolden;
                embolden.dx = 0.4;
                builder.PushTransform(embolden);
                emitAllGlyphRuns(tint);
                builder.PopTransform();
            }
        } else {
            for (const StyleSpan& span : spans) {
            if (span.length == 0U) continue;
            Base::Vector<RichTextLineClip> clips;
            const std::uint64_t spanEnd =
                static_cast<std::uint64_t>(span.start) + span.length;
            for (const TextHitRegion& region : hitRegions) {
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
                emitAllGlyphRuns(tint);
                if (span.bold) {
                    Base::Transform2D embolden;
                    embolden.dx = 0.4;
                    builder.PushTransform(embolden);
                    emitAllGlyphRuns(tint);
                    builder.PopTransform();
                }
                if (skewed) builder.PopTransform();
                builder.PopClip();
            }
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
        if (primaryGlyphRun_ != InvalidRenderGlyphRunId) {
            layoutService->ReleaseGlyphRun(primaryGlyphRun_);
        }
        if (docRare_ != nullptr) {
            for (RenderGlyphRunId glyphRun : docRare_->extraGlyphRuns) {
                layoutService->ReleaseGlyphRun(glyphRun);
            }
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
    auto* inlinePtr = ::Aero::TryCast<Aero::Documents::Inline>(child.Get());
    if (inlinePtr == nullptr) {
        return;
    }
    auto& text = static_cast<TextBlock&>(owner);
    text.AddOwnedInline(Base::Ref<Aero::Documents::Inline>::FromBorrowed(*inlinePtr));
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
        .Property(TextBlock::TextAlignmentProperty, TextAlignment::Left, AffectsMeasure | AffectsArrange | AffectsRender)
        .Property(TextBlock::LineHeightProperty, 0.0, AffectsMeasure, &Base::Validate::NonNegative<double>)
        .Property(TextBlock::PaddingProperty, Aero::Thickness{}, AffectsMeasure | AffectsArrange, &ValidateThicknessValue)
        .Property<&TextBlock::GetMetadataInlines, &TextBlock::SetInlineValue>("Inlines", PropertyFlags::Collection | PropertyFlags::Structural)
        .ContentAccessor("Inlines", ContentKind::Collection, &AddTextBlockInline, &ClearTextBlockInlines, ContentFlags::None)
        .Factory();

    ::Aero::Documents::PopulateDocumentsMetadata(context);
}

} // namespace Aero::Controls

