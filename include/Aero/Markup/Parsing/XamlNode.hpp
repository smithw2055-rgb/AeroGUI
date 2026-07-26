#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/Diagnostics.hpp>

#include <cstdint>

namespace Aero::Markup {

class XamlCompiledDocument;
class XamlNodeReader;

enum class XamlNodeKind : std::uint8_t {
    None = 0U,
    NamespaceDeclaration,
    StartObject,
    EndObject,
    StartMember,
    EndMember,
    Value,
    EndOfDocument
};

namespace XamlNodeDiagnosticCodes {
inline constexpr Core::DiagnosticCode UnboundNamespacePrefix =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 101U);
inline constexpr Core::DiagnosticCode InvalidNamespaceDeclaration =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 102U);
inline constexpr Core::DiagnosticCode DuplicateNamespacePrefix =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 103U);
inline constexpr Core::DiagnosticCode InvalidNodeStreamState =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 104U);
} // namespace XamlNodeDiagnosticCodes

class AERO_API XamlQualifiedName final {
public:
    XamlQualifiedName() noexcept = default;

    XamlQualifiedName(XamlQualifiedName&&) noexcept = default;
    XamlQualifiedName& operator=(XamlQualifiedName&&) noexcept = default;

    XamlQualifiedName(const XamlQualifiedName&) = delete;
    XamlQualifiedName& operator=(const XamlQualifiedName&) = delete;

    Base::StringView Prefix() const noexcept {
        return prefix_.View();
    }
    Base::StringView LocalName() const noexcept {
        return localName_.View();
    }
    Base::StringView NamespaceUri() const noexcept {
        return namespaceUri_.View();
    }
    bool HasNamespace() const noexcept {
        return !namespaceUri_.Empty();
    }

private:
    friend class XamlNode;
    friend class XamlNodeReader;
    friend class XamlCompiledDocument;

    void Clear() noexcept;

    Base::String prefix_;
    Base::String localName_;
    Base::String namespaceUri_;
};

class AERO_API XamlNode final {
public:
    XamlNode() noexcept = default;

    XamlNode(XamlNode&&) noexcept = default;
    XamlNode& operator=(XamlNode&&) noexcept = default;

    XamlNode(const XamlNode&) = delete;
    XamlNode& operator=(const XamlNode&) = delete;

    void Clear() noexcept;
    static Base::Result<XamlNode> TryClone(
        const XamlNode& source) noexcept;

    XamlNodeKind Kind() const noexcept { return kind_; }
    const XamlQualifiedName& Name() const noexcept {
        return name_;
    }
    Base::StringView NamespacePrefix() const noexcept {
        return namespacePrefix_.View();
    }
    Base::StringView NamespaceUri() const noexcept {
        return namespaceUri_.View();
    }
    Base::StringView Value() const noexcept {
        return value_.View();
    }
    Core::SourceSpan Source() const noexcept {
        return source_;
    }
    bool IsFromAttribute() const noexcept {
        return fromAttribute_;
    }

private:
    friend class XamlNodeReader;
    friend class XamlCompiledDocument;

    XamlNodeKind kind_ = XamlNodeKind::None;
    XamlQualifiedName name_;
    Base::String namespacePrefix_;
    Base::String namespaceUri_;
    Base::String value_;
    Core::SourceSpan source_;
    bool fromAttribute_ = false;
};

} // namespace Aero::Markup
