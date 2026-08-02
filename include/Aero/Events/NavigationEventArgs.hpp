#pragma once

#include <Aero/Base/Delegate.hpp>
#include <Aero/Events/EventArgs.hpp>

namespace Aero::Documents {

class Hyperlink;

struct RequestNavigateEventArgs : Aero::RoutedEventArgs {
    AERO_DECLARE_TYPE(RequestNavigateEventArgs, Aero::RoutedEventArgs)
public:
    RequestNavigateEventArgs() noexcept
        : Aero::RoutedEventArgs(StaticTypeId()) {}
    RequestNavigateEventArgs(
        Base::StringView uri,
        Hyperlink* hyperlink) noexcept
        : Aero::RoutedEventArgs(StaticTypeId()),
          uri_(uri), hyperlink_(hyperlink) {}

    Base::StringView GetUri() const noexcept { return uri_; }
    Hyperlink* GetHyperlink() const noexcept { return hyperlink_; }

private:
    Base::StringView uri_;
    Hyperlink* hyperlink_ = nullptr;
};

using RequestNavigateEventHandler = Base::Delegate<void(
    Base::Object*, RequestNavigateEventArgs&)>;

} // namespace Aero::Documents

