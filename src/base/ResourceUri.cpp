#include <Aero/Base/ResourceUri.hpp>

#include <Aero/Base/Utf8.hpp>
#include <Aero/Base/Vector.hpp>

#include <cstdint>

namespace Aero::Base {
namespace {

struct PathSegment  {
    std::uint32_t offset = 0U;
    std::uint32_t length = 0U;
};

bool IsAsciiAlpha(char value) noexcept {
    return (value >= 'A' && value <= 'Z') ||
        (value >= 'a' && value <= 'z');
}

bool IsSchemeCharacter(char value) noexcept {
    return IsAsciiAlpha(value) ||
        (value >= '0' && value <= '9') ||
        value == '+' || value == '-' || value == '.';
}

char ToLowerAscii(char value) noexcept {
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value - 'A' + 'a')
        : value;
}

bool EqualsAsciiInsensitive(
    StringView value,
    StringView expected) noexcept {
    if (value.SizeBytes() != expected.SizeBytes()) {
        return false;
    }
    for (std::uint32_t index = 0U;
         index < value.SizeBytes();
         ++index) {
        if (ToLowerAscii(value[index]) !=
            ToLowerAscii(expected[index])) {
            return false;
        }
    }
    return true;
}

Result<void> AppendCharacter(
    String& output,
    char value) noexcept {
    return output.AppendUnchecked(StringView(&value, 1U));
}

Result<void> AssignLowerAscii(
    String& output,
    StringView value) noexcept {
    String replacement(&output.Allocator());
    Result<void> reserve = replacement.Reserve(value.SizeBytes());
    if (!reserve) {
        return reserve.GetStatus();
    }
    for (char character : value) {
        const char lower = ToLowerAscii(character);
        Result<void> append = AppendCharacter(replacement, lower);
        if (!append) {
            return append.GetStatus();
        }
    }
    output = std::move(replacement);
    return {};
}

bool IsWindowsDrivePath(StringView text) noexcept {
    return text.SizeBytes() >= 2U &&
        IsAsciiAlpha(text[0]) &&
        text[1] == ':';
}

std::uint32_t FindSchemeColon(StringView text) noexcept {
    if (text.Empty() || !IsAsciiAlpha(text[0])) {
        return UINT32_MAX;
    }
    for (std::uint32_t index = 1U;
         index < text.SizeBytes();
         ++index) {
        const char value = text[index];
        if (value == ':') {
            return index;
        }
        if (value == '/' || value == '\\' ||
            !IsSchemeCharacter(value)) {
            return UINT32_MAX;
        }
    }
    return UINT32_MAX;
}

bool SegmentEquals(
    StringView path,
    const PathSegment& segment,
    StringView expected) noexcept {
    return path.Substr(segment.offset, segment.length) == expected;
}

Result<String> NormalizePath(
    StringView path,
    bool preserveDoubleLeadingSlash = false) noexcept {
    String normalized;
    Vector<PathSegment> segments;
    const bool absolute =
        !path.Empty() && (path[0] == '/' || path[0] == '\\');
    const bool doubleLeading = preserveDoubleLeadingSlash &&
        path.SizeBytes() >= 2U &&
        (path[0] == '/' || path[0] == '\\') &&
        (path[1] == '/' || path[1] == '\\');
    const bool trailingSlash =
        path.SizeBytes() > 1U &&
        (path[path.SizeBytes() - 1U] == '/' ||
         path[path.SizeBytes() - 1U] == '\\');

    std::uint32_t cursor = 0U;
    while (cursor < path.SizeBytes()) {
        while (cursor < path.SizeBytes() &&
               (path[cursor] == '/' || path[cursor] == '\\')) {
            ++cursor;
        }
        if (cursor >= path.SizeBytes()) {
            break;
        }
        const std::uint32_t start = cursor;
        while (cursor < path.SizeBytes() &&
               path[cursor] != '/' && path[cursor] != '\\') {
            ++cursor;
        }
        const PathSegment segment{start, cursor - start};
        if (SegmentEquals(path, segment, StringView("."))) {
            continue;
        }
        if (SegmentEquals(path, segment, StringView(".."))) {
            if (!segments.Empty() &&
                !SegmentEquals(path, segments.Back(), StringView(".."))) {
                segments.PopBack();
                continue;
            }
            if (absolute) {
                continue;
            }
        }
        Result<void> append = segments.PushBack(segment);
        if (!append) {
            return append.GetStatus();
        }
    }

    Result<void> reserve = normalized.Reserve(path.SizeBytes());
    if (!reserve) {
        return reserve.GetStatus();
    }
    if (absolute) {
        Result<void> slash = normalized.AppendUnchecked(
            doubleLeading ? StringView("//") : StringView("/"));
        if (!slash) {
            return slash.GetStatus();
        }
    }
    for (std::uint32_t index = 0U;
         index < segments.Size();
         ++index) {
        if (!normalized.Empty() &&
            normalized.View()[normalized.SizeBytes() - 1U] != '/') {
            Result<void> slash = AppendCharacter(normalized, '/');
            if (!slash) {
                return slash.GetStatus();
            }
        }
        const PathSegment& segment = segments[index];
        Result<void> append = normalized.AppendUnchecked(
            path.Substr(segment.offset, segment.length));
        if (!append) {
            return append.GetStatus();
        }
    }
    if (trailingSlash && !normalized.Empty() &&
        normalized.View()[normalized.SizeBytes() - 1U] != '/') {
        Result<void> slash = AppendCharacter(normalized, '/');
        if (!slash) {
            return slash.GetStatus();
        }
    }
    if (absolute && normalized.Empty()) {
        Result<void> slash = AppendCharacter(normalized, '/');
        if (!slash) {
            return slash.GetStatus();
        }
    }
    return normalized;
}

std::uint32_t FindComponentMarker(StringView path) noexcept {
    constexpr StringView marker(";component/");
    if (path.SizeBytes() < marker.SizeBytes()) {
        return UINT32_MAX;
    }
    for (std::uint32_t offset = 0U;
         offset + marker.SizeBytes() <= path.SizeBytes();
         ++offset) {
        if (EqualsAsciiInsensitive(
                path.Substr(offset, marker.SizeBytes()),
                marker)) {
            return offset;
        }
    }
    return UINT32_MAX;
}

Result<void> AssignAssembly(
    String& assembly,
    StringView path) noexcept {
    const std::uint32_t marker = FindComponentMarker(path);
    if (marker == UINT32_MAX) {
        return {};
    }
    std::uint32_t start = marker;
    while (start > 0U && path[start - 1U] != '/') {
        --start;
    }
    if (start == marker) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Resource URI component qualifier has no assembly name");
    }
    return assembly.Assign(path.Substr(start, marker - start));
}

Result<void> AppendCombined(
    String& output,
    StringView prefix,
    StringView reference) noexcept {
    Result<void> reserve = output.Reserve(
        prefix.SizeBytes() + 1U + reference.SizeBytes());
    if (!reserve) {
        return reserve.GetStatus();
    }
    Result<void> first = output.AppendUnchecked(prefix);
    if (!first) {
        return first.GetStatus();
    }
    if (!output.Empty() &&
        output.View()[output.SizeBytes() - 1U] != '/') {
        Result<void> slash = AppendCharacter(output, '/');
        if (!slash) {
            return slash.GetStatus();
        }
    }
    return output.AppendUnchecked(reference);
}

} // namespace

Result<void> ResourceUri::Build(
    ResourceUri& uri,
    StringView scheme,
    StringView path,
    StringView prefix) noexcept {
    Result<void> schemeResult = AssignLowerAscii(uri.scheme_, scheme);
    if (!schemeResult) {
        return schemeResult.GetStatus();
    }
    Result<void> pathResult = uri.path_.Assign(path);
    if (!pathResult) {
        return pathResult.GetStatus();
    }
    Result<void> reserve = uri.canonical_.Reserve(
        prefix.SizeBytes() + path.SizeBytes());
    if (!reserve) {
        return reserve.GetStatus();
    }
    Result<void> prefixResult =
        uri.canonical_.AppendUnchecked(prefix);
    if (!prefixResult) {
        return prefixResult.GetStatus();
    }
    Result<void> canonicalPath =
        uri.canonical_.AppendUnchecked(path);
    if (!canonicalPath) {
        return canonicalPath.GetStatus();
    }
    Result<void> assemblyResult =
        AssignAssembly(uri.assembly_, uri.path_.View());
    if (!assemblyResult) {
        return assemblyResult.GetStatus();
    }
    uri.absolute_ = !uri.scheme_.Empty();
    uri.network_ =
        uri.scheme_ == StringView("http") ||
        uri.scheme_ == StringView("https");
    return {};
}

Result<ResourceUri> ResourceUri::Parse(StringView text) noexcept {
    if (text.Empty()) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Resource URI cannot be empty");
    }
    const Utf8Validation utf8 = ValidateUtf8(text);
    if (!utf8.valid) {
        return Status::Failure(
            ErrorCode::InvalidUtf8,
            "Resource URI is not valid UTF-8");
    }
    for (char value : text) {
        if (value == '\0') {
            return Status::Failure(
                ErrorCode::InvalidArgument,
                "Resource URI contains a null character");
        }
    }

    ResourceUri uri;
    if (IsWindowsDrivePath(text)) {
        Result<String> path = NormalizePath(text);
        if (!path) {
            return path.GetStatus();
        }
        Result<void> built = Build(
            uri,
            StringView("file"),
            path.Value().View(),
            StringView("file:///"));
        if (!built) {
            return built.GetStatus();
        }
        return uri;
    }

    const std::uint32_t schemeColon = FindSchemeColon(text);
    if (schemeColon == UINT32_MAX) {
        Result<String> path = NormalizePath(text);
        if (!path) {
            return path.GetStatus();
        }
        Result<void> canonical =
            uri.canonical_.Assign(path.Value().View());
        if (!canonical) {
            return canonical.GetStatus();
        }
        Result<void> resourcePath =
            uri.path_.Assign(path.Value().View());
        if (!resourcePath) {
            return resourcePath.GetStatus();
        }
        Result<void> assembly =
            AssignAssembly(uri.assembly_, uri.path_.View());
        if (!assembly) {
            return assembly.GetStatus();
        }
        uri.absolute_ =
            !uri.path_.Empty() && uri.path_.View()[0] == '/';
        return uri;
    }

    String scheme;
    Result<void> schemeResult =
        AssignLowerAscii(scheme, text.Substr(0U, schemeColon));
    if (!schemeResult) {
        return schemeResult.GetStatus();
    }
    const StringView remainder = text.Substr(schemeColon + 1U);

    if (scheme == StringView("pack")) {
        constexpr StringView standardPrefix("//application:,,,/");
        constexpr StringView compactPrefix("/application/");
        StringView payload = remainder;
        if (remainder.SizeBytes() >= standardPrefix.SizeBytes() &&
            EqualsAsciiInsensitive(
                remainder.Substr(0U, standardPrefix.SizeBytes()),
                standardPrefix)) {
            payload = remainder.Substr(standardPrefix.SizeBytes());
        } else if (remainder.SizeBytes() >= compactPrefix.SizeBytes() &&
                   EqualsAsciiInsensitive(
                       remainder.Substr(0U, compactPrefix.SizeBytes()),
                       compactPrefix)) {
            payload = remainder.Substr(compactPrefix.SizeBytes());
        } else {
            return Status::Failure(
                ErrorCode::InvalidArgument,
                "Only pack application resource URIs are supported");
        }
        Result<String> path = NormalizePath(payload);
        if (!path) {
            return path.GetStatus();
        }
        Result<void> built = Build(
            uri,
            StringView("pack"),
            path.Value().View(),
            StringView("pack://application:,,,/"));
        if (!built) {
            return built.GetStatus();
        }
        return uri;
    }

    if (scheme == StringView("file") &&
        remainder.SizeBytes() >= 3U &&
        remainder[0] == '/' &&
        remainder[1] == '/' &&
        remainder[2] == '/') {
        Result<String> path =
            NormalizePath(remainder.Substr(3U));
        if (!path) {
            return path.GetStatus();
        }
        Result<void> built = Build(
            uri,
            StringView("file"),
            path.Value().View(),
            StringView("file:///"));
        if (!built) {
            return built.GetStatus();
        }
        return uri;
    }

    Result<String> path = NormalizePath(
        remainder,
        remainder.SizeBytes() >= 2U &&
            (remainder[0] == '/' || remainder[0] == '\\') &&
            (remainder[1] == '/' || remainder[1] == '\\'));
    if (!path) {
        return path.GetStatus();
    }
    String prefix;
    Result<void> prefixScheme = prefix.Assign(scheme.View());
    if (!prefixScheme) {
        return prefixScheme.GetStatus();
    }
    Result<void> colon = AppendCharacter(prefix, ':');
    if (!colon) {
        return colon.GetStatus();
    }
    Result<void> built = Build(
        uri,
        scheme.View(),
        path.Value().View(),
        prefix.View());
    if (!built) {
        return built.GetStatus();
    }
    return uri;
}

Result<ResourceUri> ResourceUri::Resolve(
    const ResourceUri& baseUri,
    StringView reference) noexcept {
    if (reference.Empty()) {
        if (baseUri.Empty()) {
            return Status::Failure(
                ErrorCode::InvalidArgument,
                "Cannot resolve an empty resource URI");
        }
        return baseUri;
    }
    if (IsWindowsDrivePath(reference) ||
        FindSchemeColon(reference) != UINT32_MAX) {
        return Parse(reference);
    }
    if (baseUri.Empty()) {
        return Parse(reference);
    }

    if (reference[0] == '/' || reference[0] == '\\') {
        String rooted;
        if (baseUri.Scheme() == StringView("pack")) {
            Result<void> append = AppendCombined(
                rooted,
                StringView("pack://application:,,,"),
                reference.Substr(1U));
            if (!append) {
                return append.GetStatus();
            }
        } else if (!baseUri.Scheme().Empty()) {
            Result<void> prefix =
                rooted.AppendUnchecked(baseUri.Scheme());
            if (!prefix) {
                return prefix.GetStatus();
            }
            Result<void> colon = AppendCharacter(rooted, ':');
            if (!colon) {
                return colon.GetStatus();
            }
            Result<void> append =
                rooted.AppendUnchecked(reference);
            if (!append) {
                return append.GetStatus();
            }
        } else {
            Result<void> append =
                rooted.Assign(reference);
            if (!append) {
                return append.GetStatus();
            }
        }
        return Parse(rooted.View());
    }

    StringView canonical = baseUri.Canonical();
    std::uint32_t slash = canonical.SizeBytes();
    while (slash > 0U && canonical[slash - 1U] != '/') {
        --slash;
    }
    String combined;
    Result<void> append = AppendCombined(
        combined,
        canonical.Substr(0U, slash),
        reference);
    if (!append) {
        return append.GetStatus();
    }
    return Parse(combined.View());
}

} // namespace Aero::Base
