#pragma once

#include <Aero/Media/Images.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Rendering.hpp>

namespace Aero::Detail {
class ImageControlAccess;
}

namespace Aero::Controls {

using namespace Aero::Core;
using namespace Aero;
using namespace Aero::Media;
using namespace Aero::Render;

class AERO_API Image final : public FrameworkElement {
    AERO_DECLARE_TYPE(Image, FrameworkElement)
public:
    Image() noexcept
        : FrameworkElement(StaticTypeId()) {}
    ~Image() override = default;

    Base::Ref<ImageSource> Source() const noexcept;
    Stretch GetStretch() const noexcept;
    StretchDirection GetStretchDirection() const noexcept;
    Base::Result<void> SetSource(
        Base::Ref<ImageSource> value) noexcept;
    Base::Result<void> SetStretch(
        Stretch value) noexcept;
    Base::Result<void> SetStretchDirection(
        StretchDirection value) noexcept;

    inline static constexpr Members::Property<
        Base::Ref<ImageSource>>
        SourceProperty{"Source"};
    inline static constexpr Members::Property<Stretch>
        StretchProperty{"Stretch"};
    inline static constexpr Members::Property<
        StretchDirection>
        StretchDirectionProperty{"StretchDirection"};

protected:
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<void> OnRender(
        DrawingContext& context) noexcept override;

private:
    friend class Aero::Detail::ImageControlAccess;
    std::uint64_t renderImage_ = 0U;
    std::uint32_t pixelWidth_ = 0U;
    std::uint32_t pixelHeight_ = 0U;
};

} // namespace Aero::Controls
