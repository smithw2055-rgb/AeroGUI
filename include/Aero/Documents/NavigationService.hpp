#pragma once

#include <Aero/Documents/Hyperlink.hpp>
#include <Aero/UIElement.hpp>

#include <utility>

namespace Aero::Documents {

using NavigationHandler = Base::Delegate<bool(
    StringView, Hyperlink&)>;

class AERO_GUI_API NavigationService {
public:
    explicit NavigationService(
        NavigationHandler handler = {}) noexcept;
    NavigationService(const NavigationService&) = delete;
    NavigationService& operator=(const NavigationService&) = delete;
    ~NavigationService() noexcept;

    void SetHandler(NavigationHandler handler) noexcept {
        handler_ = std::move(handler);
    }
    Result<void> Attach(Aero::UIElement& root) noexcept;
    bool Detach() noexcept;
    bool GetIsAttached() const noexcept { return root_ != nullptr; }

private:
    void OnRequestNavigate(
        Base::Object* sender,
        RequestNavigateEventArgs& args) noexcept;

    NavigationHandler handler_;
    Aero::UIElement* root_ = nullptr;
    RequestNavigateEventHandler requestHandler_;
};

} // namespace Aero::Documents
