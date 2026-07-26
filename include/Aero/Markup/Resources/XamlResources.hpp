#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>

namespace Aero::Markup {

class XamlSchemaContext;

// Installs the ResourceDictionary schema adapters shared by runtime, compiled,
// application and theme XAML.
class AERO_API XamlResourceExtension final {
public:
    Base::Result<void> Register(
        XamlSchemaContext& schema) noexcept;

private:
    XamlSchemaContext* schema_ = nullptr;
};

} // namespace Aero::Markup
