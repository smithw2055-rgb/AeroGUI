#pragma once

#include <Aero/FrameworkContentElement.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/FontFamily.hpp>
#include <Aero/TextFormatting.hpp>

namespace Aero::Documents {

class AERO_GUI_API TextElement : public FrameworkContentElement {
    AERO_DECLARE_TYPE(TextElement, FrameworkContentElement)
public:
    ~TextElement() override = default;

    Ref<Media::FontFamily> GetFontFamily() const noexcept {
        return GetValue(FontFamilyProperty);
    }
    double GetFontSize() const noexcept {
        return GetValue(FontSizeProperty);
    }
    FontWeight GetFontWeight() const noexcept {
        return GetValue(FontWeightProperty);
    }
    FontStyle GetFontStyle() const noexcept {
        return GetValue(FontStyleProperty);
    }
    Ref<Media::Brush> GetForeground() const noexcept {
        return GetValue(ForegroundProperty);
    }
    Controls::TextDecorations GetTextDecorations() const noexcept {
        return GetValue(TextDecorationsProperty);
    }

    void SetFontFamily(Ref<Media::FontFamily> value) noexcept {
        SetValue(FontFamilyProperty, std::move(value));
    }
    Result<void> SetFontFamily(StringView value) noexcept {
        Result<Ref<Media::FontFamily>> family =
            Base::MakeRef<Media::FontFamily>();
        if (!family) return family.GetStatus();
        family.Value()->SetSource(value);
        SetFontFamily(std::move(family).Value());
        return {};
    }
    void SetFontSize(double value) noexcept {
        SetValue(FontSizeProperty, value);
    }
    void SetFontWeight(FontWeight value) noexcept {
        SetValue(FontWeightProperty, value);
    }
    void SetFontStyle(FontStyle value) noexcept {
        SetValue(FontStyleProperty, value);
    }
    void SetForeground(Ref<Media::Brush> value) noexcept {
        SetValue(ForegroundProperty, std::move(value));
    }
    void SetTextDecorations(
        Controls::TextDecorations value) noexcept {
        SetValue(TextDecorationsProperty, value);
    }

    inline static constexpr AttachedProperty<Ref<Media::FontFamily>> FontFamilyProperty{"FontFamily"};
    inline static constexpr AttachedProperty<double> FontSizeProperty{"FontSize"};
    inline static constexpr AttachedProperty<FontWeight> FontWeightProperty{"FontWeight"};
    inline static constexpr AttachedProperty<FontStyle> FontStyleProperty{"FontStyle"};
    inline static constexpr AttachedProperty<Ref<Media::Brush>> ForegroundProperty{"Foreground"};
    inline static constexpr AttachedProperty<Controls::TextDecorations> TextDecorationsProperty{"TextDecorations"};

protected:
    explicit TextElement(Meta::TypeId runtimeType) noexcept
        : FrameworkContentElement(runtimeType) {}
};

} // namespace Aero::Documents
