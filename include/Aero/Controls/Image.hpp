#pragma once

#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Images.hpp>


namespace Aero::Controls {
using ::Aero::Meta::TypeId;
using ::Aero::Media::ImageSource;
class AERO_GUI_API Image : public FrameworkElement {
    AERO_DECLARE_TYPE(Image, FrameworkElement)
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
#endif
public:

    Image() noexcept
        : FrameworkElement(StaticTypeId()) {}
    ~Image() override = default;

    Ref<ImageSource> GetSource() const noexcept;
    Stretch GetStretch() const noexcept;
    StretchDirection GetStretchDirection() const noexcept;
    void SetSource(
        Ref<ImageSource> value) noexcept;
    void SetStretch(
        Stretch value) noexcept;
    void SetStretchDirection(
        StretchDirection value) noexcept;

    inline static constexpr DependencyProperty<Ref<ImageSource>> SourceProperty{"Source"};
    inline static constexpr DependencyProperty<Stretch> StretchProperty{"Stretch"};
    inline static constexpr DependencyProperty<StretchDirection> StretchDirectionProperty{"StretchDirection"};

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    void OnRender(
        ::Aero::Media::DrawingContext& context) noexcept override;

private:
    std::uint64_t renderImage_ = 0U;
    std::uint32_t pixelWidth_ = 0U;
    std::uint32_t pixelHeight_ = 0U;
};
} // namespace Aero::Controls
