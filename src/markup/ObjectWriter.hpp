#pragma once

#include "Extensions.hpp"

// Private object materializer used by Loader.

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Diagnostics.hpp>
#include "LoaderResult.hpp"
#include <Aero/Markup/Resources.hpp>
#include <Aero/Markup/Schema.hpp>

#include <cstdint>

namespace Aero {
class UIElement;
class Visual;
}

namespace Aero::Markup {

class CompiledDocument;
class NodeReader;
class NodeCursor;

namespace XamlObjectWriterDiagnosticCodes {
inline constexpr Core::DiagnosticCode UnknownType =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 201U);
inline constexpr Core::DiagnosticCode TypeNotConstructible =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 202U);
inline constexpr Core::DiagnosticCode UnknownMember =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 203U);
inline constexpr Core::DiagnosticCode InvalidAttachedMember =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 204U);
inline constexpr Core::DiagnosticCode UnsupportedMember =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 205U);
inline constexpr Core::DiagnosticCode InvalidValue =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 206U);
inline constexpr Core::DiagnosticCode InvalidWriterState =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 207U);
inline constexpr Core::DiagnosticCode MissingContentProperty =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 208U);
inline constexpr Core::DiagnosticCode DuplicateMemberValue =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 209U);
inline constexpr Core::DiagnosticCode InitializationFailed =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 210U);
inline constexpr Core::DiagnosticCode UnexpectedText =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 211U);
inline constexpr Core::DiagnosticCode TypeMismatch =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 212U);
inline constexpr Core::DiagnosticCode FactoryFailed =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 213U);
inline constexpr Core::DiagnosticCode MissingMemberValue =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 214U);
inline constexpr Core::DiagnosticCode MultipleRootObjects =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 215U);
inline constexpr Core::DiagnosticCode InvalidDirective =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 216U);
inline constexpr Core::DiagnosticCode DuplicateName =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 217U);
inline constexpr Core::DiagnosticCode DuplicateResourceKey =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 218U);
inline constexpr Core::DiagnosticCode StaticResourceNotFound =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 219U);
inline constexpr Core::DiagnosticCode MissingResourceScope =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 220U);
inline constexpr Core::DiagnosticCode NullNotAllowed =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 221U);
inline constexpr Core::DiagnosticCode InvalidMarkupExtension =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 222U);
inline constexpr Core::DiagnosticCode NamespaceState =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 223U);
inline constexpr Core::DiagnosticCode NameRegistrationFailed =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 224U);
inline constexpr Core::DiagnosticCode ResourceRegistrationFailed =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 225U);
inline constexpr Core::DiagnosticCode UnknownMarkupExtension =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 226U);
inline constexpr Core::DiagnosticCode MarkupExtensionFailed =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 227U);
} // namespace XamlObjectWriterDiagnosticCodes

// Immutable writer configuration. Every call creates a fresh one-shot
// private writer state so transaction stacks and document scopes never
// survive a load operation.
class AERO_API ObjectWriter final {
public:
    explicit ObjectWriter(
        Schema& schema,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;

    ObjectWriter(const ObjectWriter&) = delete;
    ObjectWriter& operator=(const ObjectWriter&) = delete;

    Base::Result<LoaderResult> LoadDocument(
        NodeReader& reader) noexcept;
    Base::Result<LoaderResult> LoadDocument(
        const CompiledDocument& document) noexcept;

    Markup::Schema& GetSchema() const noexcept {
        return *schema_;
    }
    Core::IDiagnosticSink* Diagnostics() const noexcept {
        return diagnostics_;
    }

private:
    friend class ObjectBuilder;

    static Base::Result<Aero::Visual*> ResolveVisual(
        Markup::Schema& schema,
        Base::Object& object,
        Core::TypeId type) noexcept;
    static Base::Result<Aero::UIElement*> ResolveUIElement(
        Markup::Schema& schema,
        Base::Object& object,
        Core::TypeId type) noexcept;
    static Base::Result<void> StageContent(
        Markup::Schema& schema,
        Base::Object& object,
        const Core::Value& value,
        const ExtensionServices& services) noexcept;
    Markup::Schema* schema_ = nullptr;
    Core::IDiagnosticSink* diagnostics_ = nullptr;
};

} // namespace Aero::Markup
