#include <Aero/RuntimeHost.hpp>
#include <Aero/Markup/XamlVisualTree.hpp>

namespace Aero::Markup {

Base::Result<void> XamlVisualTreeHost::Resize(
    Presentation::Size availableSize) noexcept {
    if (!mounted_ || rootLayout_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "XAML visual tree resize requires a mounted layout root");
    }
    if (!Presentation::IsValidLayoutSize(availableSize)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML visual tree resize dimensions are invalid");
    }
    Base::Result<void> resized =
        layout_->SetRoot(rootLayout_, availableSize);
    if (!resized) {
        return resized.GetStatus();
    }
    if (renderer_ != nullptr && rootRender_ != nullptr) {
        return renderer_->Invalidate(*rootRender_);
    }
    return {};
}

} // namespace Aero::Markup
