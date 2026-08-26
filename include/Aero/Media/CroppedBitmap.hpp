#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Media/ImageSource.hpp>

namespace Aero::Media {

class AERO_GUI_API CroppedBitmap : public ImageSource {
    AERO_DECLARE_TYPE(CroppedBitmap, ImageSource)
public:
    CroppedBitmap() noexcept
        : ImageSource(StaticTypeId()) {}
    ~CroppedBitmap() override = default;

    Ref<ImageSource> GetSource() const noexcept;
    void SetSource(
        Ref<ImageSource> value) noexcept;
    Base::Rect GetSourceRect() const noexcept;
    void SetSourceRect(
        Base::Rect value) noexcept;

    inline static constexpr DependencyProperty<Ref<ImageSource>> SourceProperty{"Source"};
    inline static constexpr DependencyProperty<Base::Rect> SourceRectProperty{"SourceRect"};
};

} // namespace Aero::Media
