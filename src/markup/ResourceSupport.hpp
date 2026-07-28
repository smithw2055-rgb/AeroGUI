#pragma once

// Private XAML resource-scope registration.

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>

namespace Aero::Markup {

class Schema;

// Installs the ResourceDictionary schema adapters shared by runtime, compiled,
// application and theme XAML.
class AERO_API ResourceExtension final {
public:
    Base::Result<void> Register(
        Schema& schema) noexcept;

private:
    Schema* schema_ = nullptr;
};

} // namespace Aero::Markup
