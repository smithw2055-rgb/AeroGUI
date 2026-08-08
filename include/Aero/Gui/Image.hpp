#pragma once

#include <Aero/Gui/FrameworkElement.hpp>
#include <Aero/Media/Images.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;
using ::Aero::Media::ImageSource;
class AERO_GUI_API Image : public FrameworkElement {
    AERO_DECLARE_TYPE(Image, FrameworkElement)
public:
    struct Access;

    Image() noexcept
        : FrameworkElement(StaticTypeId()) {}
    ~Image() override = default;

    Base::Ref<ImageSource> GetSource() const noexcept;
    Stretch GetStretch() const noexcept;
    StretchDirection GetStretchDirection() const noexcept;
    void SetSource(
        Base::Ref<ImageSource> value) noexcept;
    void SetStretch(
        Stretch value) noexcept;
    void SetStretchDirection(
        StretchDirection value) noexcept;

    inline static constexpr DependencyProperty<Base::Ref<ImageSource>> SourceProperty{"Source"};
    inline static constexpr DependencyProperty<Stretch> StretchProperty{"Stretch"};
    inline static constexpr DependencyProperty<StretchDirection> StretchDirectionProperty{"StretchDirection"};

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    void OnRender(
        ::Aero::Media::DrawingContext& context) noexcept override;

private:
    friend struct Access;
    std::uint64_t renderImage_ = 0U;
    std::uint32_t pixelWidth_ = 0U;
    std::uint32_t pixelHeight_ = 0U;
};
} // namespace Aero::Controls
