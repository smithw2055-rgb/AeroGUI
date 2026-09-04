#pragma once

#include <Aero/DependencyObject.hpp>

namespace Aero::Media {

using ::Aero::Meta::TypeId;

class AERO_GUI_API ImageSource : public DependencyObject {
    AERO_DECLARE_TYPE(ImageSource, DependencyObject)
protected:
    explicit ImageSource(TypeId runtimeType) noexcept
        : DependencyObject(runtimeType) {}
    ~ImageSource() override = default;
};

} // namespace Aero::Media
