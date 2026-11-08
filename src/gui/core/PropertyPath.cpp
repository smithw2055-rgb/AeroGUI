#include "gui/core/ValueConversion.hpp"
#include <Aero/PropertyPath.hpp>

namespace Aero {
namespace {

Base::StringView StripXmlNamespacePrefix(Base::StringView token) noexcept {
    for (std::uint32_t i = 0U; i < token.SizeBytes(); ++i) {
        if (token[i] == ':') {
            return token.Substr(i + 1U, token.SizeBytes() - i - 1U);
        }
    }
    return token;
}

} // namespace

Base::Result<void> PropertyPath::ParseTokens(
    Base::StringView path,
    Base::Vector<PropertyPathToken>& outTokens) noexcept {
    outTokens.Clear();

    path = Base::ValueConversion::Trim(path);
    const std::uint32_t len = path.SizeBytes();
    if (len == 0U) return {};

    std::uint32_t cursor = 0U;
    while (cursor < len) {
        // Skip dots or whitespace between segments
        while (cursor < len && (path[cursor] == '.' || path[cursor] == ' ' || path[cursor] == '\t')) {
            ++cursor;
        }
        if (cursor >= len) break;

        PropertyPathToken token;
        const std::uint32_t segmentStart = cursor;

        if (path[cursor] == '(') {
            // Attached or type-qualified property segment: (OwnerType.PropertyName)
            ++cursor;
            const std::uint32_t innerStart = cursor;
            while (cursor < len && path[cursor] != ')') {
                ++cursor;
            }
            if (cursor >= len) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Attached property segment missing closing ')'");
            }
            const Base::StringView inner = path.Substr(innerStart, cursor - innerStart);
            ++cursor; // skip ')'

            token.isTypeQualified = true;
            std::uint32_t dot = UINT32_MAX;
            for (std::uint32_t i = 0U; i < inner.SizeBytes(); ++i) {
                if (inner[i] == '.') dot = i;
            }
            if (dot != UINT32_MAX) {
                token.typeQualifier = StripXmlNamespacePrefix(inner.Substr(0U, dot));
                token.propertyName = inner.Substr(dot + 1U, inner.SizeBytes() - dot - 1U);
            } else {
                token.propertyName = inner;
            }
        } else {
            // Standard property name until '.', '[', or '('
            const std::uint32_t nameStart = cursor;
            while (cursor < len && path[cursor] != '.' && path[cursor] != '[' && path[cursor] != '(') {
                ++cursor;
            }
            token.propertyName = path.Substr(nameStart, cursor - nameStart);
        }

        // Optional collection indexer: [index]
        if (cursor < len && path[cursor] == '[') {
            ++cursor;
            const std::uint32_t idxStart = cursor;
            while (cursor < len && path[cursor] != ']') {
                if (path[cursor] < '0' || path[cursor] > '9') {
                    return Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        "Collection index must be numeric");
                }
                ++cursor;
            }
            if (cursor >= len) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Collection index missing closing ']'");
            }
            std::uint64_t idxVal = 0U;
            for (std::uint32_t i = idxStart; i < cursor; ++i) {
                idxVal = idxVal * 10U + static_cast<std::uint64_t>(path[i] - '0');
                if (idxVal > UINT32_MAX) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Collection index is out of range");
                }
            }
            token.hasIndex = true;
            token.collectionIndex = static_cast<std::uint32_t>(idxVal);
            ++cursor; // skip ']'
        }

        token.raw = path.Substr(segmentStart, cursor - segmentStart);
        outTokens.PushBack(token);
    }

    return {};
}

} // namespace Aero
