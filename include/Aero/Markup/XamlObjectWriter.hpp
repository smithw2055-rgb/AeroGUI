#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

#include <cstdint>

namespace Aero::Markup {

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
} // namespace XamlObjectWriterDiagnosticCodes

class AERO_API XamlObjectWriter final {
public:
    explicit XamlObjectWriter(
        XamlSchemaContext& schema,
        Core::IDiagnosticSink* diagnostics = nullptr,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~XamlObjectWriter() noexcept;

    XamlObjectWriter(const XamlObjectWriter&) = delete;
    XamlObjectWriter& operator=(const XamlObjectWriter&) = delete;

    AERO_NODISCARD Base::Result<Base::Ref<Base::Object>> Load(
        XamlNodeReader& reader) noexcept;
    void Reset() noexcept;

private:
    static constexpr std::uint32_t InvalidIndex = UINT32_MAX;

    enum class FrameKind : std::uint8_t {
        Object = 0U,
        Member
    };

    struct Frame final {
        FrameKind kind = FrameKind::Object;
        std::uint32_t objectIndex = InvalidIndex;
        std::uint32_t targetObjectIndex = InvalidIndex;
        XamlResolvedMember member;
        Core::SourceSpan source;
        std::uint32_t valuesWritten = 0U;
        bool propertyElement = false;
    };

    struct CreatedObjectRecord final {
        Base::Ref<Base::Object> object;
        Core::TypeId type = Core::InvalidTypeId;
        bool beginCalled = false;
        bool endCalled = false;
    };

    struct AssignmentRecord final {
        std::uint32_t objectIndex = InvalidIndex;
        Core::MemberId member = Core::InvalidMemberId;
        std::uint32_t count = 0U;
    };

    XamlSchemaContext* schema_ = nullptr;
    Core::IDiagnosticSink* diagnostics_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<Frame> frames_;
    Base::Vector<CreatedObjectRecord> created_;
    Base::Vector<AssignmentRecord> assignments_;
    Base::Ref<Base::Object> root_;
    bool loading_ = false;
    bool ended_ = false;

    AERO_NODISCARD Base::Result<void> ProcessNode(
        const XamlNode& node) noexcept;
    AERO_NODISCARD Base::Result<void> StartObject(
        const XamlNode& node) noexcept;
    AERO_NODISCARD Base::Result<void> EndObject(
        const XamlNode& node) noexcept;
    AERO_NODISCARD Base::Result<void> StartMember(
        const XamlNode& node) noexcept;
    AERO_NODISCARD Base::Result<void> EndMember(
        const XamlNode& node) noexcept;
    AERO_NODISCARD Base::Result<void> WriteText(
        const XamlNode& node) noexcept;

    AERO_NODISCARD Base::Result<void> StartPropertyElement(
        const XamlNode& node,
        std::uint32_t targetFrameIndex) noexcept;
    AERO_NODISCARD Base::Result<void> CompleteObject(
        const XamlNode& node) noexcept;
    AERO_NODISCARD Base::Result<void> WriteObjectToParent(
        std::uint32_t objectIndex,
        Core::SourceSpan source) noexcept;
    AERO_NODISCARD Base::Result<void> WriteObjectToContent(
        std::uint32_t parentObjectIndex,
        std::uint32_t childObjectIndex,
        Core::SourceSpan source) noexcept;
    AERO_NODISCARD Base::Result<void> WriteValueToMember(
        Frame& memberFrame,
        XamlValue&& value,
        Core::SourceSpan source) noexcept;
    AERO_NODISCARD Base::Result<void> WriteValue(
        std::uint32_t targetObjectIndex,
        const XamlResolvedMember& member,
        XamlValue&& value,
        Core::SourceSpan source) noexcept;

    AERO_NODISCARD std::uint32_t CurrentObjectFrameIndex() const noexcept;
    AERO_NODISCARD AssignmentRecord* FindAssignment(
        std::uint32_t objectIndex,
        Core::MemberId member) noexcept;
    AERO_NODISCARD bool HasPropertyElementSyntax(
        const XamlQualifiedName& name) const noexcept;
    AERO_NODISCARD bool IsWhitespaceOnly(Base::StringView value) const noexcept;

    void AbortTransaction() noexcept;
    void ClearTransaction() noexcept;
    AERO_NODISCARD Base::Status Failure(
        Base::Status status,
        Core::DiagnosticCode diagnostic,
        Base::StringView message,
        Core::SourceSpan source) noexcept;
};

} // namespace Aero::Markup
