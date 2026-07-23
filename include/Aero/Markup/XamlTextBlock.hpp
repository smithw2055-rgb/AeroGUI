#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Controls.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

namespace Aero::Markup {

using XamlAsTextBlockCallback = Core::TextBlock* (*)(
    Base::Object& object, void* context) noexcept;

struct XamlTextBlockTypeRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    XamlAsTextBlockCallback cast = nullptr;
    void* context = nullptr;
};

// Writes the Text property of a core TextBlock from the XAML string scalar.
// Glyph shaping remains independent: a text provider later selects and
// registers the glyph run consumed by TextBlock::SetGlyphRun().
class [[deprecated("Use XamlDependencyPropertyBridge")]] AERO_API XamlTextBlockExtension final {
public:
    XamlTextBlockExtension() noexcept;

    XamlTextBlockExtension(const XamlTextBlockExtension&) = delete;
    XamlTextBlockExtension& operator=(const XamlTextBlockExtension&) = delete;

    Base::Result<void> TryRegisterType(
        const XamlTextBlockTypeRegistration& registration) noexcept;
    Base::Result<std::uint32_t> Register(
        XamlSchemaContext& schema) noexcept;

private:
    XamlSchemaContext* schema_ = nullptr;
    Base::Vector<XamlTextBlockTypeRegistration> types_;
    Base::Vector<Core::MemberId> textMembers_;

    const XamlTextBlockTypeRegistration* FindTypeRegistration(
        Core::TypeId type) const noexcept;
    Base::Result<Core::TextBlock*> ResolveTextBlock(
        Base::Object& object,
        const XamlServiceProvider& services) const noexcept;
    bool IsTextMember(Core::MemberId member) const noexcept;

    static Base::Result<void> SetTextMember(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services,
        void* context) noexcept;
};

} // namespace Aero::Markup
