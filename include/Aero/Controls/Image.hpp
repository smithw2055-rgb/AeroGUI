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
    void SetSource(Ref<ImageSource> value) noexcept;
    void SetStretch(Stretch value) noexcept;
    void SetStretchDirection(StretchDirection value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Ref<ImageSource>, Source);
    AERO_DEPENDENCY_PROPERTY(Stretch, Stretch);
    AERO_DEPENDENCY_PROPERTY(StretchDirection, StretchDirection);

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
