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
#if defined(AERO_GUI_IMPLEMENTATION)
public:
#else
private:
#endif
    struct Access;

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
    StringView GetText() const noexcept;
    Ref<Brush> GetForeground() const noexcept;
    Ref<Brush> GetBackground() const noexcept;
    double GetFontSize() const noexcept;
    Ref<Media::FontFamily> GetFontFamily() const noexcept;
    FontWeight GetFontWeight() const noexcept;
    FontStyle GetFontStyle() const noexcept;
    TextDecorations GetTextDecorations() const noexcept;
    TextWrapping GetTextWrapping() const noexcept;
    TextTrimming GetTextTrimming() const noexcept;
    TextAlignment GetTextAlignment() const noexcept;
    double GetLineHeight() const noexcept;
    std::uint32_t GetInlineCount() const noexcept {
        return ownedInlines_.Size();
    }
    Documents::InlineCollection GetInlines() noexcept;
    Documents::InlineCollectionView GetInlines() const noexcept;
    Documents::TextPointer GetContentStart() noexcept;
    Documents::TextPointer GetContentEnd() noexcept;
    Value GetMetadataInlines() const noexcept;
    void SetText(StringView value) noexcept;
    void SetForeground(
        Ref<Brush> value) noexcept;
    void SetBackground(
        Ref<Brush> value) noexcept;
    void SetFontSize(double value) noexcept;
    void SetFontFamily(Ref<Media::FontFamily> value) noexcept;
    Result<void> SetFontFamily(StringView value) noexcept;
    void SetFontWeight(
        FontWeight value) noexcept;
    void SetFontStyle(
        FontStyle value) noexcept;
    void SetTextDecorations(
        TextDecorations value) noexcept;
    void SetTextWrapping(
        TextWrapping value) noexcept;
    void SetTextTrimming(
        TextTrimming value) noexcept;
    void SetTextAlignment(
        TextAlignment value) noexcept;
    void SetLineHeight(double value) noexcept;
    void SetInlineValue(
        Value value) noexcept;
    void SetRichTextStyleRanges(
        Base::Span<const RichTextStyleRange> ranges) noexcept;
    Result<void> AddOwnedInline(
        const Ref<Base::Object>& inlineObject) noexcept;
    void ClearOwnedInlines() noexcept;
    inline static constexpr DependencyProperty<String> TextProperty{"Text"};
    inline static constexpr auto ForegroundProperty = FrameworkElementForegroundProperty;
    inline static constexpr DependencyProperty<Ref<Aero::Media::Brush>> BackgroundProperty{"Background"};
    inline static constexpr DependencyProperty<Ref<Aero::Media::Brush>> StrokeProperty{"Stroke"};
    // WPF exposes the same inheritable text formatting property through
    // Control and TextBlock owners. Sharing the handle here gives generated
    // text content the ComboBoxItem/Control FontSize instead of falling back
    // to an unrelated TextBlock default.
    inline static constexpr auto FontSizeProperty = Control::FontSizeProperty;
    inline static constexpr auto FontFamilyProperty = FrameworkElement::FontFamilyProperty;
    inline static constexpr DependencyProperty<FontWeight> FontWeightProperty{"FontWeight"};
    inline static constexpr DependencyProperty<FontStyle> FontStyleProperty{"FontStyle"};
    inline static constexpr DependencyProperty<TextDecorations> TextDecorationsProperty{"TextDecorations"};
    inline static constexpr DependencyProperty<double> StrokeThicknessProperty{"StrokeThickness"};
    inline static constexpr DependencyProperty<TextWrapping> TextWrappingProperty{"TextWrapping"};
    inline static constexpr DependencyProperty<TextTrimming> TextTrimmingProperty{"TextTrimming"};
    inline static constexpr DependencyProperty<TextAlignment> TextAlignmentProperty{"TextAlignment"};
    inline static constexpr DependencyProperty<double> LineHeightProperty{"LineHeight"};
    inline static constexpr DependencyProperty<Thickness> PaddingProperty{"Padding"};
protected:
    explicit TextBlock(TypeId runtimeType) noexcept;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;
private:
#if defined(AERO_GUI_IMPLEMENTATION)
#if defined(AERO_GUI_IMPLEMENTATION)
    friend struct ::Aero::Controls::Control::Access;
#endif
#endif
    friend struct Access;

    StringView EffectiveFontFamily() const noexcept;
    void ReleaseServiceGlyphRun() noexcept;
    void SetGlyphRun(
        std::uint64_t glyphRun, Size size) noexcept;

    Base::Vector<std::uint64_t> glyphRuns_;
    Base::Vector<TextHitRegion> textHitRegions_;
    Base::Vector<Ref<Base::Object>> ownedInlines_;
    Base::Vector<RichTextStyleRange> richTextStyleRanges_;
    Ref<Base::Object> pendingInline_;
    Size glyphRunSize_;
    bool serviceOwnsGlyphRun_ = false;
    bool arrangingText_ = false;
};
} // namespace Aero::Controls
