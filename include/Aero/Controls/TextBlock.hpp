#pragma once

#include <Aero/Controls/Control.hpp>
#include <Aero/TextFormatting.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Media/Brushes.hpp>

namespace Aero::Documents {
class InlineCollection;
class InlineCollectionView;
class TextPointer;
}
namespace Aero::Controls {
using ::Aero::Meta::TypeId;
using ::Aero::Media::Brush;
using ::Aero::Media::FrameworkElementForegroundProperty;

class AERO_GUI_API TextBlock : public FrameworkElement {
    AERO_DECLARE_TYPE(TextBlock, FrameworkElement)

public:
    // Source-retained formatting produced by RichText markup. Text layout
    // keeps byte offsets for hit testing, so the same ranges can tint shaped
    // glyphs without replacing the public Documents inline model.
    struct RichTextStyleRange {
        std::uint32_t start = 0U;
        std::uint32_t length = 0U;
        Base::Color foreground{};
        bool hasForeground = false;
        bool bold = false;
        bool italic = false;
    };

    TextBlock() noexcept;
    ~TextBlock() override;

    void AttachTextLayout(void* service, bool invalidate = false) noexcept;
    void InvalidateDocumentText() noexcept;
    void AddOwnedInline(const Ref<Base::Object>& inlineObject) noexcept;

    StringView GetText() const noexcept;
    void SetText(StringView value) noexcept;
    Ref<Brush> GetForeground() const noexcept;
    void SetForeground(Ref<Brush> value) noexcept;
    Ref<Brush> GetBackground() const noexcept;
    void SetBackground(Ref<Brush> value) noexcept;
    double GetFontSize() const noexcept;
    void SetFontSize(double value) noexcept;
    Ref<Media::FontFamily> GetFontFamily() const noexcept;
    void SetFontFamily(Ref<Media::FontFamily> value) noexcept;
    void SetFontFamily(StringView value) noexcept;
    FontWeight GetFontWeight() const noexcept;
    void SetFontWeight(FontWeight value) noexcept;
    FontStyle GetFontStyle() const noexcept;
    void SetFontStyle(FontStyle value) noexcept;
    TextDecorations GetTextDecorations() const noexcept;
    void SetTextDecorations(TextDecorations value) noexcept;
    TextWrapping GetTextWrapping() const noexcept;
    void SetTextWrapping(TextWrapping value) noexcept;
    TextTrimming GetTextTrimming() const noexcept;
    void SetTextTrimming(TextTrimming value) noexcept;
    TextAlignment GetTextAlignment() const noexcept;
    void SetTextAlignment(TextAlignment value) noexcept;
    double GetLineHeight() const noexcept;
    void SetLineHeight(double value) noexcept;
    std::uint32_t GetInlineCount() const noexcept { return ownedInlines_.Size(); }
    Documents::InlineCollection GetInlines() noexcept;
    Documents::InlineCollectionView GetInlines() const noexcept;
    Documents::TextPointer GetContentStart() noexcept;
    Documents::TextPointer GetContentEnd() noexcept;
    Value GetMetadataInlines() const noexcept;
    void SetInlineValue(Value value) noexcept;
    void SetRichTextStyleRanges(Base::Span<const RichTextStyleRange> ranges) noexcept;
    void ClearOwnedInlines() noexcept;

    AERO_DEPENDENCY_PROPERTY(String, Text);
    inline static constexpr auto ForegroundProperty = FrameworkElementForegroundProperty;
    AERO_DEPENDENCY_PROPERTY(Ref<Aero::Media::Brush>, Background);
    AERO_DEPENDENCY_PROPERTY(Ref<Aero::Media::Brush>, Stroke);
    // WPF exposes the same inheritable text formatting property through
    // Control and TextBlock owners. Sharing the handle here gives generated
    // text content the ComboBoxItem/Control FontSize instead of falling back
    // to an unrelated TextBlock default.
    inline static constexpr auto FontSizeProperty = Control::FontSizeProperty;
    inline static constexpr auto FontFamilyProperty = FrameworkElement::FontFamilyProperty;
    AERO_DEPENDENCY_PROPERTY(FontWeight, FontWeight);
    AERO_DEPENDENCY_PROPERTY(FontStyle, FontStyle);
    AERO_DEPENDENCY_PROPERTY(TextDecorations, TextDecorations);
    AERO_DEPENDENCY_PROPERTY(double, StrokeThickness);
    AERO_DEPENDENCY_PROPERTY(TextWrapping, TextWrapping);
    AERO_DEPENDENCY_PROPERTY(TextTrimming, TextTrimming);
    AERO_DEPENDENCY_PROPERTY(TextAlignment, TextAlignment);
    AERO_DEPENDENCY_PROPERTY(double, LineHeight);
    AERO_DEPENDENCY_PROPERTY(Thickness, Padding);

protected:
    explicit TextBlock(TypeId runtimeType) noexcept;

    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;
    void OnPropertyInvalidated(PropertyInvalidationFlags flags) noexcept override;
    std::uint32_t GetVisualChildrenCount() const noexcept override;
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override;

private:
    friend struct TextBlockDocumentHelper;

    StringView EffectiveFontFamily() const noexcept;
    void ReleaseServiceGlyphRun() noexcept;

    void SetGlyphRun(std::uint64_t glyphRun, Size size) noexcept;

    Base::Vector<std::uint64_t> glyphRuns_;
    Base::Vector<TextHitRegion> textHitRegions_;
    Base::Vector<Ref<Base::Object>> ownedInlines_;
    mutable Base::String flatText_;
    mutable bool flatTextValid_ = false;
    Base::Vector<RichTextStyleRange> richTextStyleRanges_;
    Ref<Base::Object> pendingInline_;
    Size glyphRunSize_;
    bool serviceOwnsGlyphRun_ = false;
    bool arrangingText_ = false;
};
} // namespace Aero::Controls
