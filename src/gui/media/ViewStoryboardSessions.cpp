#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

using namespace ::Aero;
namespace MediaAnimation = ::Aero::Media::Animation;

ViewState::StoryboardSession::StoryboardSession(
            Base::IAllocator* allocator) noexcept
    : handles(allocator) {}

ViewState::StoryboardCompletionSession::StoryboardCompletionSession(
            Base::IAllocator* allocator) noexcept
    : handles(allocator) {}

Base::Result<Base::StringView> ViewState::AnimationAttachedString(
        MediaAnimation::Timeline& timeline,
        Meta::DependencyPropertyHandle property) noexcept {
        Base::Result<Meta::PropertyValue> value =
            timeline.GetValue(property);
        if (!value) return value.GetStatus();
        if (value.Value().Kind() != Meta::ValueKind::String) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Storyboard attached property must be a string");
        }
        return value.Value().AsString();
    }

Base::Result<ViewState::ResolvedAnimationProperty>
ViewState::ResolveAnimationProperty(
        ::Aero::DependencyObject& target,
        Base::StringView authoredPath) noexcept {
        // Object-model geometry uses two indexed collection hops. Resolve the
        // exact WPF path before the generic collection cases below.
        const Base::StringView figuresToken("PathGeometry.Figures");
        const Base::StringView segmentsToken("PathFigure.Segments");
        const auto findText = [](
            Base::StringView text,
            Base::StringView token) noexcept {
            for (std::uint32_t index = 0U;
                 index + token.SizeBytes() <= text.SizeBytes();
                 ++index) {
                if (text.Substr(index, token.SizeBytes()) == token) {
                    return index;
                }
            }
            return UINT32_MAX;
        };
        if (findText(authoredPath, figuresToken) != UINT32_MAX &&
            findText(authoredPath, segmentsToken) != UINT32_MAX) {
            std::uint32_t indices[2]{};
            std::uint32_t found = 0U;
            for (std::uint32_t cursor = 0U;
                 cursor < authoredPath.SizeBytes() && found < 2U;
                 ++cursor) {
                if (authoredPath[cursor] != '[') continue;
                std::uint32_t value = 0U;
                ++cursor;
                bool digit = false;
                while (cursor < authoredPath.SizeBytes() &&
                       authoredPath[cursor] != ']') {
                    if (authoredPath[cursor] < '0' ||
                        authoredPath[cursor] > '9') {
                        return Base::Status::Failure(
                            Base::ErrorCode::ValidationFailed,
                            "PathGeometry Storyboard index must be numeric");
                    }
                    digit = true;
                    value = value * 10U +
                        static_cast<std::uint32_t>(
                            authoredPath[cursor] - '0');
                    ++cursor;
                }
                if (!digit) {
                    return Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        "PathGeometry Storyboard index is empty");
                }
                indices[found++] = value;
            }
            const Meta::DependencyProperty* dataProperty =
                ::Aero::MetadataPrivate::
                    DependencyProperties(*metadata).Find(
                        target.RuntimeType(), "Data");
            if (found != 2U || dataProperty == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "PathGeometry Storyboard Data property was not found");
            }
            Base::Result<Meta::PropertyValue> data =
                target.GetValue(dataProperty->Handle());
            if (!data ||
                data.Value().Kind() != Meta::ValueKind::Object ||
                !data.Value().AsObject() ||
                data.Value().AsObject()->RuntimeType() !=
                    Media::PathGeometry::StaticTypeId()) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Storyboard target Data is not a PathGeometry");
            }
            auto& geometry = static_cast<Media::PathGeometry&>(
                *data.Value().AsObject());
            const auto figures = geometry.GetFigures();
            if (indices[0] >= figures.Size() || !figures[indices[0]]) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Storyboard PathGeometry figure index is invalid");
            }
            const auto segments = figures[indices[0]]->GetSegments();
            if (indices[1] >= segments.Size() || !segments[indices[1]] ||
                segments[indices[1]]->RuntimeType() !=
                    Media::LineSegment::StaticTypeId()) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Storyboard PathGeometry segment index is invalid");
            }
            auto* line = static_cast<Media::LineSegment*>(
                segments[indices[1]].Get());
            return ResolvedAnimationProperty{
                line, Media::LineSegment::PointProperty.Handle()};
        }

        const auto findDependencyProperty =
            [this](::Aero::DependencyObject& object,
                   Base::StringView authored) noexcept
            -> const Meta::DependencyProperty* {
            while (!authored.Empty() &&
                   (authored[0] == ' ' || authored[0] == '\t' ||
                    authored[0] == '(')) {
                authored = authored.Substr(
                    1U, authored.SizeBytes() - 1U);
            }
            while (!authored.Empty() &&
                   (authored[authored.SizeBytes() - 1U] == ' ' ||
                    authored[authored.SizeBytes() - 1U] == '\t' ||
                    authored[authored.SizeBytes() - 1U] == ')')) {
                authored = authored.Substr(
                    0U, authored.SizeBytes() - 1U);
            }
            std::uint32_t separator = UINT32_MAX;
            for (std::uint32_t index = 0U;
                 index < authored.SizeBytes(); ++index) {
                if (authored[index] == '.') separator = index;
            }
            auto& properties =
                ::Aero::MetadataPrivate::
                    DependencyProperties(*metadata);
            if (separator == UINT32_MAX) {
                return properties.Find(
                    object.RuntimeType(), authored);
            }
            Base::StringView ownerName = authored.Substr(0U, separator);
            const Base::StringView propertyName = authored.Substr(
                separator + 1U,
                authored.SizeBytes() - separator - 1U);
            // WPF owner-qualified paths may name a different owner of the
            // same dependency property (for example TextElement.Foreground
            // targeting a TextBlock). Prefer the property exposed by the
            // concrete target before falling back to a true attached owner.
            if (const Meta::DependencyProperty* targetProperty =
                    properties.Find(object.RuntimeType(), propertyName)) {
                return targetProperty;
            }
            for (std::uint32_t index = 0U;
                 index < ownerName.SizeBytes(); ++index) {
                if (ownerName[index] == ':') {
                    ownerName = ownerName.Substr(
                        index + 1U,
                        ownerName.SizeBytes() - index - 1U);
                }
            }
            for (const Meta::TypeInfo& type : metadata->Types().Types()) {
                if (type.Name() != ownerName) continue;
                const Meta::DependencyProperty* property =
                    properties.Find(type.Id(), propertyName);
                if (property != nullptr &&
                    (property->IsAttached() ||
                     metadata->Types().IsDerivedFrom(
                         object.RuntimeType(), type.Id()))) {
                    return property;
                }
            }
            return properties.Find(object.RuntimeType(), propertyName);
        };
        Base::StringView path = authoredPath;
        while (!path.Empty() &&
               (path[0] == ' ' || path[0] == '\t')) {
            path = path.Substr(1U, path.SizeBytes() - 1U);
        }
        while (!path.Empty() &&
               (path[path.SizeBytes() - 1U] == ' ' ||
                path[path.SizeBytes() - 1U] == '\t')) {
            path = path.Substr(0U, path.SizeBytes() - 1U);
        }
        bool compoundParenthesizedPath = false;
        for (std::uint32_t index = 0U;
             index + 1U < path.SizeBytes(); ++index) {
            if (path[index] == ')' &&
                (path[index + 1U] == '.' ||
                 path[index + 1U] == '[')) {
                compoundParenthesizedPath = true;
                break;
            }
        }
        if (!compoundParenthesizedPath &&
            path.SizeBytes() >= 2U &&
            path[0] == '(' &&
            path[path.SizeBytes() - 1U] == ')') {
            path = path.Substr(1U, path.SizeBytes() - 2U);
        }
        std::uint32_t indexedOpen = UINT32_MAX;
        std::uint32_t indexedClose = UINT32_MAX;
        for (std::uint32_t index = 0U;
             index < path.SizeBytes(); ++index) {
            if (path[index] == '[' &&
                indexedOpen == UINT32_MAX) {
                indexedOpen = index;
            } else if (path[index] == ']' &&
                       indexedOpen != UINT32_MAX) {
                indexedClose = index;
                break;
            }
        }

        ::Aero::DependencyObject* propertyTarget = &target;
        bool indexedPathResolved = false;
        if (indexedOpen != UINT32_MAX) {
            if (indexedClose == UINT32_MAX ||
                indexedClose == indexedOpen + 1U) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Indexed Storyboard TargetProperty path has an invalid index");
            }
            std::uint64_t parsedIndex = 0U;
            for (std::uint32_t index = indexedOpen + 1U;
                 index < indexedClose; ++index) {
                if (path[index] < '0' || path[index] > '9') {
                    return Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        "Indexed Storyboard TargetProperty index must be numeric");
                }
                parsedIndex =
                    parsedIndex * 10U +
                    static_cast<std::uint64_t>(
                        path[index] - '0');
                if (parsedIndex > UINT32_MAX) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Indexed Storyboard TargetProperty index is too large");
                }
            }

            const Base::StringView beforeIndex =
                path.Substr(0U, indexedOpen);
            Base::StringView terminalPath =
                path.Substr(
                    indexedClose + 1U,
                    path.SizeBytes() -
                        indexedClose - 1U);
            if (!terminalPath.Empty() &&
                terminalPath[0] == '.') {
                terminalPath = terminalPath.Substr(
                    1U, terminalPath.SizeBytes() - 1U);
            }
            if (terminalPath.SizeBytes() >= 2U &&
                terminalPath[0] == '(' &&
                terminalPath[
                    terminalPath.SizeBytes() - 1U] == ')') {
                terminalPath = terminalPath.Substr(
                    1U, terminalPath.SizeBytes() - 2U);
            }

            auto endsWith =
                [](Base::StringView value,
                   Base::StringView suffix) noexcept {
                    return value.SizeBytes() >=
                               suffix.SizeBytes() &&
                        value.Substr(
                            value.SizeBytes() -
                                suffix.SizeBytes(),
                            suffix.SizeBytes()) ==
                            suffix;
                };
            const bool gradientStops =
                endsWith(
                    beforeIndex,
                    Base::StringView(
                        ").(GradientBrush.GradientStops)")) ||
                endsWith(
                    beforeIndex,
                    Base::StringView(
                        ".GradientStops"));
            const bool transformChildren =
                beforeIndex ==
                    Base::StringView(
                        "(UIElement.RenderTransform).(TransformGroup.Children)") ||
                beforeIndex ==
                    Base::StringView(
                        "RenderTransform.Children") ||
                beforeIndex ==
                    Base::StringView(
                        "(FrameworkElement.LayoutTransform).(TransformGroup.Children)") ||
                beforeIndex ==
                    Base::StringView(
                        "LayoutTransform.Children") ||
                beforeIndex ==
                    Base::StringView(
                        "(TransformGroup.Children)");
            if (gradientStops) {
                Base::StringView brushOwnerPath;
                if (!beforeIndex.Empty() &&
                    beforeIndex[0] == '(') {
                    std::uint32_t close = UINT32_MAX;
                    for (std::uint32_t index = 1U;
                         index < beforeIndex.SizeBytes();
                         ++index) {
                        if (beforeIndex[index] == ')') {
                            close = index;
                            break;
                        }
                    }
                    if (close == UINT32_MAX ||
                        close <= 1U) {
                        return Base::Status::Failure(
                            Base::ErrorCode::ValidationFailed,
                            "Storyboard GradientStops owner path is invalid");
                    }
                    brushOwnerPath =
                        beforeIndex.Substr(
                            1U, close - 1U);
                } else {
                    std::uint32_t separator = UINT32_MAX;
                    for (std::uint32_t index = 0U;
                         index < beforeIndex.SizeBytes();
                         ++index) {
                        if (beforeIndex[index] == '.') {
                            separator = index;
                            break;
                        }
                    }
                    if (separator == UINT32_MAX ||
                        separator == 0U) {
                        return Base::Status::Failure(
                            Base::ErrorCode::ValidationFailed,
                            "Storyboard GradientStops owner property is missing");
                    }
                    brushOwnerPath =
                        beforeIndex.Substr(
                            0U, separator);
                }
                std::uint32_t ownerDot = UINT32_MAX;
                for (std::uint32_t index = 0U;
                     index < brushOwnerPath.SizeBytes();
                     ++index) {
                    if (brushOwnerPath[index] == '.') {
                        ownerDot = index;
                    }
                }
                const Base::StringView brushProperty =
                    ownerDot == UINT32_MAX
                    ? brushOwnerPath
                    : brushOwnerPath.Substr(
                          ownerDot + 1U,
                          brushOwnerPath.SizeBytes() -
                              ownerDot - 1U);
                const Meta::DependencyProperty* background =
                    ::Aero::MetadataPrivate::
                        DependencyProperties(*metadata)
                            .Find(
                                target.RuntimeType(),
                                brushProperty);
                if (background == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard brush property was not found");
                }
                Base::Result<Meta::PropertyValue> value =
                    target.GetValue(background->Handle());
                if (!value ||
                    value.Value().Kind() !=
                        Meta::ValueKind::Object ||
                    !value.Value().AsObject() ||
                    !metadata->Types().IsDerivedFrom(
                        value.Value().AsObject()->RuntimeType(),
                        Media::GradientBrush::StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard target property is not a GradientBrush");
                }
                auto& brush = static_cast<
                    Media::GradientBrush&>(
                        *value.Value().AsObject());
                const auto stops = brush.GetGradientStops();
                if (parsedIndex >= stops.Size() ||
                    !stops[static_cast<std::uint32_t>(
                        parsedIndex)]) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Storyboard GradientStops index is out of range");
                }
                propertyTarget =
                    stops[static_cast<std::uint32_t>(
                        parsedIndex)].Get();
                path = terminalPath;
                indexedPathResolved = true;
            } else if (transformChildren) {
                Base::Ref<Media::Transform> transform;
                if (beforeIndex ==
                        Base::StringView(
                            "(TransformGroup.Children)") &&
                    metadata->Types().IsDerivedFrom(
                        target.RuntimeType(),
                        Media::TransformGroup::StaticTypeId())) {
                    transform =
                        Base::Ref<Media::Transform>::
                            TryFromBorrowed(
                                static_cast<
                                    Media::Transform&>(
                                        target));
                } else {
                    const bool layoutPath =
                        beforeIndex ==
                            Base::StringView(
                                "(FrameworkElement.LayoutTransform).(TransformGroup.Children)") ||
                        beforeIndex ==
                            Base::StringView(
                                "LayoutTransform.Children");
                    if (layoutPath) {
                        if (!metadata->Types().IsDerivedFrom(
                                target.RuntimeType(),
                                Aero::FrameworkElement::StaticTypeId())) {
                            return Base::Status::Failure(
                                Base::ErrorCode::InvalidArgument,
                                "Storyboard LayoutTransform target is not a FrameworkElement");
                        }
                        transform =
                            static_cast<Aero::FrameworkElement&>(
                                target).GetLayoutTransform();
                    } else {
                        if (!metadata->Types().IsDerivedFrom(
                                target.RuntimeType(),
                                Aero::UIElement::StaticTypeId())) {
                            return Base::Status::Failure(
                                Base::ErrorCode::InvalidArgument,
                                "Storyboard RenderTransform target is not a UIElement");
                        }
                        transform =
                            static_cast<Aero::UIElement&>(
                                target).GetRenderTransform();
                    }
                }
                if (!transform ||
                    !metadata->Types().IsDerivedFrom(
                        transform->RuntimeType(),
                        Media::TransformGroup::StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard transform path has no TransformGroup");
                }
                auto& group = static_cast<
                    Media::TransformGroup&>(
                        *transform);
                const auto children = group.GetChildren();
                if (parsedIndex >= children.Size() ||
                    !children[static_cast<std::uint32_t>(
                        parsedIndex)]) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Storyboard TransformGroup index is out of range");
                }
                propertyTarget =
                    children[static_cast<std::uint32_t>(
                        parsedIndex)].Get();
                path = terminalPath;
                indexedPathResolved = true;
            } else {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Indexed Storyboard TargetProperty collection is not supported");
            }
        }

        if (!indexedPathResolved &&
            indexedOpen == UINT32_MAX &&
            compoundParenthesizedPath &&
            path.SizeBytes() >= 7U &&
            path[0] == '(' &&
            path[path.SizeBytes() - 1U] == ')') {
            std::uint32_t separator = UINT32_MAX;
            for (std::uint32_t index = 1U;
                 index + 2U < path.SizeBytes();
                 ++index) {
                if (path[index] == ')' &&
                    path[index + 1U] == '.' &&
                    path[index + 2U] == '(') {
                    separator = index;
                    break;
                }
            }
            if (separator != UINT32_MAX) {
                Base::StringView ownerPath =
                    path.Substr(
                        1U,
                        separator - 1U);
                const std::uint32_t terminalStart =
                    separator + 3U;
                Base::StringView terminalPath =
                    path.Substr(
                        terminalStart,
                        path.SizeBytes() -
                            terminalStart - 1U);
                const Meta::DependencyProperty*
                    ownerDependency =
                        findDependencyProperty(
                            target, ownerPath);
                if (ownerDependency == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard compound object property was not found");
                }
                Base::Result<Meta::PropertyValue>
                    ownerValue = target.GetValue(
                        ownerDependency->Handle());
                if (!ownerValue ||
                    ownerValue.Value().Kind() !=
                        Meta::ValueKind::Object ||
                    !ownerValue.Value().AsObject() ||
                    !metadata->Types().IsDerivedFrom(
                        ownerValue.Value().
                            AsObject()->RuntimeType(),
                        ::Aero::DependencyObject::
                            StaticTypeId())) {
                    thread_local char message[384];
                    const Meta::TypeInfo* targetType =
                        metadata->Types().FindType(
                            target.RuntimeType());
                    const Base::StringView targetTypeName =
                        targetType != nullptr
                        ? targetType->Name()
                        : Base::StringView("<unknown>");
                    std::snprintf(
                        message,
                        sizeof(message),
                        "Storyboard compound object property '%.*s' on '%.*s' has no DependencyObject value",
                        static_cast<int>(ownerPath.SizeBytes()),
                        ownerPath.Data(),
                        static_cast<int>(targetTypeName.SizeBytes()),
                        targetTypeName.Data());
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        message);
                }
                propertyTarget =
                    static_cast<
                        ::Aero::DependencyObject*>(
                        ownerValue.Value().
                            AsObject().Get());
                path = terminalPath;
                indexedPathResolved = true;
            }
        }

        std::uint32_t dot = UINT32_MAX;
        if (!indexedPathResolved) {
            for (std::uint32_t index = 0U;
                 index < path.SizeBytes(); ++index) {
                if (path[index] == '.') {
                    dot = index;
                    break;
                }
            }
        }
        if (path.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Storyboard TargetProperty is empty");
        }

        if (!indexedPathResolved) {
            std::uint32_t dotCount = 0U;
            for (char character : path) {
                if (character == '.') ++dotCount;
            }
            if (dotCount >= 2U && path[0] != '(') {
                ::Aero::DependencyObject* currentTarget = propertyTarget;
                std::uint32_t segmentBegin = 0U;
                while (segmentBegin < path.SizeBytes()) {
                    std::uint32_t segmentEnd = segmentBegin;
                    while (segmentEnd < path.SizeBytes() &&
                           path[segmentEnd] != '.') {
                        ++segmentEnd;
                    }
                    const Base::StringView segment = path.Substr(
                        segmentBegin, segmentEnd - segmentBegin);
                    const bool terminal = segmentEnd == path.SizeBytes();
                    const Meta::DependencyProperty* segmentProperty =
                        ::Aero::MetadataPrivate::
                            DependencyProperties(*metadata)
                                .Find(currentTarget->RuntimeType(), segment);
                    if (segmentProperty == nullptr) {
                        return Base::Status::Failure(
                            Base::ErrorCode::NotFound,
                            "Storyboard object path property was not found");
                    }
                    if (terminal) {
                        return ResolvedAnimationProperty{
                            currentTarget, segmentProperty->Handle()};
                    }
                    Base::Result<Meta::PropertyValue> segmentValue =
                        currentTarget->GetValue(segmentProperty->Handle());
                    if (!segmentValue ||
                        segmentValue.Value().Kind() !=
                            Meta::ValueKind::Object ||
                        segmentValue.Value().IsNullObject() ||
                        !segmentValue.Value().AsObject() ||
                        !metadata->Types().IsDerivedFrom(
                            segmentValue.Value().AsObject()->RuntimeType(),
                            ::Aero::DependencyObject::StaticTypeId())) {
                        return Base::Status::Failure(
                            Base::ErrorCode::NotFound,
                            "Storyboard object path has no DependencyObject value");
                    }
                    currentTarget = static_cast<::Aero::DependencyObject*>(
                        segmentValue.Value().AsObject().Get());
                    segmentBegin = segmentEnd + 1U;
                }
            }
        }

        if (dot != UINT32_MAX) {
            Base::StringView ownerProperty =
                path.Substr(0U, dot);
            Base::StringView nestedProperty =
                path.Substr(
                    dot + 1U,
                    path.SizeBytes() - dot - 1U);
            if (ownerProperty ==
                    Base::StringView("RenderTransform") ||
                ownerProperty ==
                    Base::StringView("LayoutTransform")) {
                const bool layoutPath =
                    ownerProperty ==
                    Base::StringView(
                        "LayoutTransform");
                Base::Ref<Media::Transform> transform;
                if (layoutPath) {
                    if (!metadata->Types().IsDerivedFrom(
                            target.RuntimeType(),
                            Aero::FrameworkElement::StaticTypeId())) {
                        return Base::Status::Failure(
                            Base::ErrorCode::InvalidArgument,
                            "LayoutTransform animation target is not a FrameworkElement");
                    }
                    transform =
                        static_cast<Aero::FrameworkElement&>(
                            target).GetLayoutTransform();
                } else {
                    if (!metadata->Types().IsDerivedFrom(
                            target.RuntimeType(),
                            Aero::UIElement::StaticTypeId())) {
                        return Base::Status::Failure(
                            Base::ErrorCode::InvalidArgument,
                            "RenderTransform animation target is not a UIElement");
                    }
                    transform =
                        static_cast<Aero::UIElement&>(
                            target).GetRenderTransform();
                }
                if (!transform) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        layoutPath
                            ? "Storyboard target has no LayoutTransform object"
                            : "Storyboard target has no RenderTransform object");
                }
                propertyTarget = transform.Get();
                path = nestedProperty;
            } else if (
                nestedProperty == Base::StringView("Color")) {
                const Meta::DependencyProperty*
                    ownerDependency =
                        ::Aero::MetadataPrivate::
                                DependencyProperties(
                                    *metadata)
                                    .Find(
                                        target.
                                            RuntimeType(),
                                        ownerProperty);
                if (ownerDependency == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard object property was not found on the target");
                }
                Base::Result<Meta::PropertyValue>
                    ownerValue = target.GetValue(
                        ownerDependency->Handle());
                if (!ownerValue ||
                    ownerValue.Value().Kind() !=
                        Meta::ValueKind::Object ||
                    !ownerValue.Value().AsObject() ||
                    !metadata->Types().IsDerivedFrom(
                        ownerValue.Value().
                            AsObject()->RuntimeType(),
                        ::Aero::DependencyObject::
                            StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard object property has no DependencyObject value");
                }
                propertyTarget =
                    static_cast<
                        ::Aero::DependencyObject*>(
                            ownerValue.Value().
                                AsObject().Get());
                path = nestedProperty;
            } else {
                // Object-property chains such as Effect.Radius descend into
                // the DependencyObject value before resolving the nested
                // property. Owner-qualified direct properties such as
                // FrameworkElement.MinWidth resolve on the original target.
                const Meta::DependencyProperty* ownerDependency =
                    ::Aero::MetadataPrivate::
                            DependencyProperties(
                                *metadata)
                            .Find(
                                target.RuntimeType(),
                                ownerProperty);
                if (ownerDependency != nullptr) {
                    Base::Result<Meta::PropertyValue> ownerValue =
                        target.GetValue(
                            ownerDependency->Handle());
                    if (ownerValue &&
                        ownerValue.Value().Kind() ==
                            Meta::ValueKind::Object &&
                        ownerValue.Value().AsObject() &&
                        metadata->Types().IsDerivedFrom(
                            ownerValue.Value().
                                AsObject()->RuntimeType(),
                            ::Aero::DependencyObject::
                                StaticTypeId())) {
                        propertyTarget =
                            static_cast<
                                ::Aero::DependencyObject*>(
                                    ownerValue.Value().
                                        AsObject().Get());
                        path = nestedProperty;
                    } else {
                        path = nestedProperty;
                    }
                } else {
                    path = nestedProperty;
                }
            }
        }
        // Resolve ordinary and parenthesized object-property chains such as
        // Foreground.Color and Fill.(aero:Brush.Shader).Time.
        if (indexedOpen == UINT32_MAX) {
            ::Aero::DependencyObject* current = propertyTarget;
            std::uint32_t start = 0U;
            std::uint32_t depth = 0U;
            while (start < path.SizeBytes()) {
                std::uint32_t end = start;
                std::uint32_t parentheses = 0U;
                while (end < path.SizeBytes()) {
                    const char character = path[end];
                    if (character == '(') ++parentheses;
                    else if (character == ')' && parentheses != 0U) {
                        --parentheses;
                    } else if (character == '.' && parentheses == 0U) {
                        break;
                    }
                    ++end;
                }
                Base::StringView token = path.Substr(start, end - start);
                while (!token.Empty() &&
                       (token[0] == '(' || token[0] == ' ')) {
                    token = token.Substr(1U, token.SizeBytes() - 1U);
                }
                while (!token.Empty() &&
                       (token[token.SizeBytes() - 1U] == ')' ||
                        token[token.SizeBytes() - 1U] == ' ')) {
                    token = token.Substr(0U, token.SizeBytes() - 1U);
                }
                std::uint32_t owner = UINT32_MAX;
                for (std::uint32_t index = 0U;
                     index < token.SizeBytes(); ++index) {
                    if (token[index] == '.') owner = index;
                }
                if (owner != UINT32_MAX) {
                    token = token.Substr(
                        owner + 1U,
                        token.SizeBytes() - owner - 1U);
                }
                const Meta::DependencyProperty* dependency =
                    ::Aero::MetadataPrivate::
                        DependencyProperties(*metadata).Find(
                            current->RuntimeType(), token);
                if (dependency == nullptr) break;
                if (end >= path.SizeBytes()) {
                    return ResolvedAnimationProperty{
                        current, dependency->Handle()};
                }
                Base::Result<Meta::PropertyValue> value =
                    current->GetValue(dependency->Handle());
                if (!value ||
                    value.Value().Kind() != Meta::ValueKind::Object ||
                    !value.Value().AsObject() ||
                    !metadata->Types().IsDerivedFrom(
                        value.Value().AsObject()->RuntimeType(),
                        ::Aero::DependencyObject::StaticTypeId())) {
                    break;
                }
                current = static_cast<::Aero::DependencyObject*>(
                    value.Value().AsObject().Get());
                start = end + 1U;
                if (++depth > 16U) break;
            }
        }
        if (path.SizeBytes() >= 2U &&
            path[0] == '(' &&
            path[path.SizeBytes() - 1U] == ')') {
            path = path.Substr(1U, path.SizeBytes() - 2U);
        }
        std::uint32_t ownerDot = UINT32_MAX;
        for (std::uint32_t index = 0U;
             index < path.SizeBytes(); ++index) {
            if (path[index] == '.') ownerDot = index;
        }
        if (ownerDot != UINT32_MAX) {
            path = path.Substr(
                ownerDot + 1U,
                path.SizeBytes() - ownerDot - 1U);
        }
        const Meta::DependencyProperty* property =
            ::Aero::MetadataPrivate::
                DependencyProperties(*metadata)
                    .Find(propertyTarget->RuntimeType(), path);
        if (property == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Storyboard TargetProperty was not found on the target");
        }
        return ResolvedAnimationProperty{
            propertyTarget, property->Handle()};
    }

ViewState::StoryboardTimingState ViewState::ComposeStoryboardTiming(
        const StoryboardTimingState* inherited,
        const MediaAnimation::Timeline& storyboard,
        bool preservesChildDuration) noexcept {
        StoryboardTimingState result =
            inherited != nullptr
            ? *inherited
            : StoryboardTimingState{};
        const Aero::Media::Animation::Model::TimelineTiming authored =
            Aero::Media::AnimationPrivate::Timing(storyboard);
        if (UINT64_MAX - result.beginTimeMicroseconds <
            authored.beginTimeMicroseconds) {
            result.beginTimeMicroseconds = UINT64_MAX;
        } else {
            result.beginTimeMicroseconds +=
                authored.beginTimeMicroseconds;
        }
        if (!storyboard.GetDuration().Empty()) {
            result.durationMicroseconds =
                authored.durationMicroseconds;
            result.hasDuration = true;
            result.preservesChildDuration =
                preservesChildDuration;
        }
        if (!storyboard.GetRepeatBehavior().Empty()) {
            result.repeat = authored.repeat;
            result.hasRepeat = true;
        }
        result.speedRatio *= authored.speedRatio;
        result.autoReverse =
            result.autoReverse || authored.autoReverse;
        return result;
    }

Aero::Media::Animation::Model::TimelineTiming ViewState::EffectiveTimelineTiming(
        const MediaAnimation::Timeline& timeline,
        const StoryboardTimingState* inherited) noexcept {
        Aero::Media::Animation::Model::TimelineTiming result =
            Aero::Media::AnimationPrivate::Timing(timeline);
        if (inherited == nullptr) return result;
        if (UINT64_MAX - inherited->beginTimeMicroseconds <
            result.beginTimeMicroseconds) {
            result.beginTimeMicroseconds = UINT64_MAX;
        } else {
            result.beginTimeMicroseconds +=
                inherited->beginTimeMicroseconds;
        }
        if (inherited->hasDuration &&
            !inherited->preservesChildDuration) {
            result.durationMicroseconds =
                inherited->durationMicroseconds;
        } else if (inherited->hasDuration &&
                   inherited->preservesChildDuration) {
            const Aero::Media::Animation::AnimationTime childBegin =
                Aero::Media::AnimationPrivate::
                    Timing(timeline).beginTimeMicroseconds;
            const Aero::Media::Animation::AnimationTime available =
                childBegin >= inherited->durationMicroseconds
                ? 0U
                : inherited->durationMicroseconds - childBegin;
            if (result.durationMicroseconds == 0U) {
                result.durationMicroseconds = available;
                result.repeat =
                    Aero::Media::Animation::Model::
                        RepeatBehavior::Once();
            } else {
                const long double cycle =
                    static_cast<long double>(
                        result.durationMicroseconds) *
                    (result.autoReverse ? 2.0L : 1.0L);
                const double maximumCount =
                    cycle > 0.0L
                    ? static_cast<double>(
                        static_cast<long double>(available) /
                        cycle)
                    : 1.0;
                if (available == 0U) {
                    result.durationMicroseconds = 0U;
                    result.repeat =
                        Aero::Media::Animation::Model::
                            RepeatBehavior::Once();
                } else if (result.repeat.forever ||
                           result.repeat.count >
                               maximumCount) {
                    result.repeat =
                        Aero::Media::Animation::Model::
                            RepeatBehavior::Count(
                                std::max(
                                    maximumCount,
                                    1.0e-9));
                }
            }
        }
        if (inherited->hasRepeat) {
            result.repeat = inherited->repeat;
        }
        result.speedRatio *= inherited->speedRatio;
        result.autoReverse =
            result.autoReverse || inherited->autoReverse;
        return result;
    }

Base::Result<std::uint32_t>
 ViewState::RetainStartedAnimation(
        Base::Result<
            Aero::Media::Animation::Model::AnimationHandle>
            started,
        Base::Vector<
            Aero::Media::Animation::Model::AnimationHandle>*
            retainedHandles) noexcept {
        if (!started) {
            return started.GetStatus();
        }
        if (retainedHandles != nullptr) {
            Base::Result<void> retained =
                retainedHandles->PushBack(
                    started.Value());
            if (!retained) {
                static_cast<void>(
                    animations->Remove(
                        started.Value()));
                return retained.GetStatus();
            }
        }
        return std::uint32_t{1U};
    }

Base::Result<std::uint32_t> ViewState::BeginTimeline(
        MediaAnimation::Timeline& timeline,
        Aero::FrameworkElement& triggerOwner,
        const Aero::NameScope* names,
        const StoryboardTimingState* inherited,
        Base::Vector<
            Aero::Media::Animation::Model::AnimationHandle>*
            retainedHandles,
        Aero::Controls::DataTemplateTriggerState*
            dataTemplateContext) noexcept {
        if (animations == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Storyboard requires the animation manager");
        }
        if (metadata->Types().IsDerivedFrom(
                timeline.RuntimeType(),
                MediaAnimation::Storyboard::StaticTypeId())) {
            auto& nested =
                static_cast<MediaAnimation::Storyboard&>(timeline);
            const StoryboardTimingState timing =
                ComposeStoryboardTiming(
                    inherited,
                    nested,
                    timeline.RuntimeType() ==
                        MediaAnimation::ParallelTimeline::
                            StaticTypeId());
            std::uint32_t count = 0U;
            for (const Base::Ref<MediaAnimation::Timeline>& child :
                 nested.GetTimelines()) {
                if (!child) continue;
                Base::Result<std::uint32_t> started =
                    BeginTimeline(
                        *child, triggerOwner, names, &timing,
                        retainedHandles,
                        dataTemplateContext);
                if (!started) return started.GetStatus();
                if (count > UINT32_MAX - started.Value()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Storyboard child count overflow");
                }
                count += started.Value();
            }
            return count;
        }

        Base::Result<Base::StringView> targetName =
            AnimationAttachedString(
                timeline,
                MediaAnimation::Storyboard::TargetNameProperty);
        if (!targetName) return targetName.GetStatus();
        Base::Result<Base::StringView> targetPath =
            AnimationAttachedString(
                timeline,
                MediaAnimation::Storyboard::TargetPropertyProperty);
        if (!targetPath) return targetPath.GetStatus();

        Base::Object* targetObject =
            targetName.Value().Empty()
            ? static_cast<Base::Object*>(
                  &triggerOwner)
            : dataTemplateContext != nullptr
                ? dataTemplateContext->FindName(
                      targetName.Value())
                : names != nullptr
                    ? names->Find(targetName.Value())
                    : loadedDocument.names.Find(
                          targetName.Value());
        if (targetObject == nullptr && names != nullptr) {
            targetObject = loadedDocument.names.Find(
                targetName.Value());
        }
        if (targetObject == nullptr ||
            !metadata->Types().IsDerivedFrom(
                targetObject->RuntimeType(),
                ::Aero::DependencyObject::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Storyboard target name does not resolve to a DependencyObject");
        }
        auto& target =
            static_cast<::Aero::DependencyObject&>(*targetObject);
        Base::Result<ResolvedAnimationProperty> property =
            ResolveAnimationProperty(target, targetPath.Value());
        if (!property) return property.GetStatus();
        ::Aero::DependencyObject& propertyTarget =
            *property.Value().target;
        const Meta::DependencyPropertyHandle propertyHandle =
            property.Value().property;

        const Meta::TypeId type = timeline.RuntimeType();
        if (type == MediaAnimation::DoubleAnimation::StaticTypeId()) {
            auto& animation =
                static_cast<MediaAnimation::DoubleAnimation&>(timeline);
            Aero::Media::Animation::Model::DoubleAnimation runtime =
                Aero::Media::AnimationPrivate::Double(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<Aero::Media::Animation::Model::AnimationHandle> started =
                animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (metadata->Types().IsDerivedFrom(
                type,
                MediaAnimation::DoubleAnimationBase::StaticTypeId())) {
            auto& animation = static_cast<
                MediaAnimation::DoubleAnimationBase&>(timeline);
            Base::Result<Meta::PropertyValue> current =
                propertyTarget.GetValue(propertyHandle);
            if (!current) return current.GetStatus();
            Base::Result<double> origin =
                Meta::ValueCodec<double>::Decode(current.Value());
            if (!origin) return origin.GetStatus();

            Aero::Media::Animation::Model::CustomDoubleAnimation runtime;
            runtime.animation =
                Base::Ref<MediaAnimation::DoubleAnimationBase>::
                    TryFromBorrowed(animation);
            if (!runtime.animation) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Custom DoubleAnimation is not reference-counted");
            }
            runtime.defaultOriginValue = origin.Value();
            runtime.defaultDestinationValue =
                animation.ResolveTo(origin.Value());
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type == MediaAnimation::ColorAnimation::StaticTypeId()) {
            auto& animation =
                static_cast<MediaAnimation::ColorAnimation&>(timeline);
            Aero::Media::Animation::Model::ColorAnimation runtime =
                Aero::Media::AnimationPrivate::Color(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<Aero::Media::Animation::Model::AnimationHandle> started =
                animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            MediaAnimation::PointAnimation::
                StaticTypeId()) {
            auto& animation =
                static_cast<
                    MediaAnimation::PointAnimation&>(
                        timeline);
            Aero::Media::Animation::Model::PointAnimation runtime =
                Aero::Media::AnimationPrivate::Point(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            MediaAnimation::RectAnimation::
                StaticTypeId()) {
            auto& animation =
                static_cast<
                    MediaAnimation::RectAnimation&>(
                        timeline);
            Aero::Media::Animation::Model::RectAnimation runtime =
                Aero::Media::AnimationPrivate::Rect(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            MediaAnimation::ThicknessAnimation::
                StaticTypeId()) {
            auto& animation =
                static_cast<
                    MediaAnimation::ThicknessAnimation&>(
                        timeline);
            Aero::Media::Animation::Model::ThicknessAnimation runtime =
                Aero::Media::AnimationPrivate::Thickness(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            MediaAnimation::DoubleAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::DoubleAnimationUsingKeyFrames&>(timeline);
            Base::Vector<Aero::Media::Animation::Model::DoubleKeyFrame> frames(allocator);
            for (const Base::Ref<MediaAnimation::DoubleKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Base::Result<void> appended =
                    frames.PushBack(Aero::Media::AnimationPrivate::DoubleFrame(*frame));
                if (!appended) return appended.GetStatus();
            }
            for (std::uint32_t index = 1U;
                 index < frames.Size(); ++index) {
                Aero::Media::Animation::Model::DoubleKeyFrame current =
                    frames[index];
                std::uint32_t position = index;
                while (position > 0U &&
                       frames[position - 1U]
                               .keyTimeMicroseconds >
                           current.keyTimeMicroseconds) {
                    frames[position] =
                        frames[position - 1U];
                    --position;
                }
                frames[position] = current;
            }
            Base::Result<Meta::PropertyValue> base =
                propertyTarget.GetValue(propertyHandle);
            if (!base) return base.GetStatus();
            Base::Result<double> baseDouble =
                Meta::ValueCodec<double>::Decode(base.Value());
            Aero::Media::Animation::Model::DoubleKeyFrameAnimation runtime;
            if (baseDouble) {
                runtime.baseValue = baseDouble.Value();
            } else if (!frames.Empty() &&
                       frames.Front().keyTimeMicroseconds == 0U) {
                // A zero-time key frame defines the initial animated value;
                // no interpolation can observe the underlying base value.
                // This also lets XAML start a key-frame animation on a
                // property whose unset metadata representation is not a
                // concrete double.
                runtime.baseValue = frames.Front().value;
            } else {
                return baseDouble.GetStatus();
            }
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            if (runtime.timing.durationMicroseconds == 0U &&
                !frames.Empty()) {
                runtime.timing.durationMicroseconds =
                    frames.Back().keyTimeMicroseconds;
            }
            runtime.keyFrames = frames.AsSpan();
            Base::Result<Aero::Media::Animation::Model::AnimationHandle> started =
                animations->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            MediaAnimation::ColorAnimationUsingKeyFrames::
                StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::ColorAnimationUsingKeyFrames&>(
                    timeline);
            Base::Vector<Aero::Media::Animation::Model::ColorKeyFrame>
                frames(allocator);
            for (const Base::Ref<
                     MediaAnimation::ColorKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Base::Result<void> appended =
                    frames.PushBack(
                        Aero::Media::AnimationPrivate::ColorFrame(*frame));
                if (!appended) {
                    return appended.GetStatus();
                }
            }
            for (std::uint32_t index = 1U;
                 index < frames.Size();
                 ++index) {
                Aero::Media::Animation::Model::ColorKeyFrame current =
                    frames[index];
                std::uint32_t position = index;
                while (position > 0U &&
                       frames[position - 1U]
                               .keyTimeMicroseconds >
                           current.keyTimeMicroseconds) {
                    frames[position] =
                        frames[position - 1U];
                    --position;
                }
                frames[position] = current;
            }
            Base::Result<Meta::PropertyValue> base =
                propertyTarget.GetValue(
                    propertyHandle);
            if (!base) return base.GetStatus();
            Base::Result<Base::Color> baseColor =
                Meta::ValueCodec<Base::Color>::Decode(
                    base.Value());
            if (!baseColor) {
                return baseColor.GetStatus();
            }
            Aero::Media::Animation::Model::ColorKeyFrameAnimation
                runtime;
            runtime.baseValue = baseColor.Value();
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            if (runtime.timing.durationMicroseconds ==
                    0U &&
                !frames.Empty()) {
                runtime.timing.durationMicroseconds =
                    frames.Back()
                        .keyTimeMicroseconds;
            }
            runtime.keyFrames = frames.AsSpan();
            Base::Result<
                Aero::Media::Animation::Model::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }

        Base::Vector<Aero::Media::Animation::Model::DiscreteAnimationKeyFrame>
            frames(allocator);
        if (type ==
            MediaAnimation::PointAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::PointAnimationUsingKeyFrames&>(timeline);
            for (const Base::Ref<MediaAnimation::PointKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Aero::Media::Animation::Model::DiscreteAnimationKeyFrame runtime;
                runtime.keyTimeMicroseconds = frame->GetKeyTimeMicroseconds();
                Base::Result<Meta::PropertyValue> encoded =
                    Meta::ValueCodec<Base::Point>::Encode(frame->GetValue());
                if (!encoded) return encoded.GetStatus();
                runtime.value = std::move(encoded).Value();
                Base::Result<void> appended =
                    frames.PushBack(std::move(runtime));
                if (!appended) return appended.GetStatus();
            }
        } else         if (type ==
            MediaAnimation::ThicknessAnimationUsingKeyFrames::
                StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::ThicknessAnimationUsingKeyFrames&>(
                    timeline);
            for (const Base::Ref<
                     MediaAnimation::ThicknessKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Aero::Media::Animation::Model::DiscreteAnimationKeyFrame runtime;
                runtime.keyTimeMicroseconds =
                    frame->GetKeyTimeMicroseconds();
                Base::Result<Meta::PropertyValue> encoded =
                    Meta::ValueCodec<
                        Base::Thickness>::Encode(
                            frame->GetValue());
                if (!encoded) return encoded.GetStatus();
                runtime.value =
                    std::move(encoded).Value();
                Base::Result<void> appended =
                    frames.PushBack(
                        std::move(runtime));
                if (!appended) {
                    return appended.GetStatus();
                }
            }
        } else if (type ==
            MediaAnimation::BooleanAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::BooleanAnimationUsingKeyFrames&>(timeline);
            for (const Base::Ref<
                     MediaAnimation::DiscreteBooleanKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Aero::Media::Animation::Model::DiscreteAnimationKeyFrame runtime;
                runtime.keyTimeMicroseconds =
                    frame->GetKeyTimeMicroseconds();
                Base::Result<Meta::PropertyValue> encoded =
                    Meta::ValueCodec<bool>::Encode(frame->GetValue());
                if (!encoded) return encoded.GetStatus();
                runtime.value = std::move(encoded).Value();
                Base::Result<void> appended =
                    frames.PushBack(std::move(runtime));
                if (!appended) return appended.GetStatus();
            }
        } else if (type ==
            MediaAnimation::ObjectAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::ObjectAnimationUsingKeyFrames&>(timeline);
            for (const Base::Ref<
                     MediaAnimation::DiscreteObjectKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Aero::Media::Animation::Model::DiscreteAnimationKeyFrame runtime;
                runtime.keyTimeMicroseconds =
                    frame->GetKeyTimeMicroseconds();
                runtime.value = frame->GetValue();
                const Meta::DependencyProperty* targetProperty =
                    propertyTarget.PropertyRegistry().Find(
                        propertyHandle);
                if (targetProperty != nullptr &&
                    runtime.value.IsNullObject() &&
                    runtime.value.Type() !=
                        targetProperty->ValueType()) {
                    runtime.value =
                        Meta::PropertyValue::NullObject(
                            targetProperty->ValueType());
                }
                Base::Result<void> appended =
                    frames.PushBack(std::move(runtime));
                if (!appended) return appended.GetStatus();
            }
        } else {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Storyboard contains an unsupported Timeline type");
        }
        for (std::uint32_t index = 1U;
             index < frames.Size(); ++index) {
            Aero::Media::Animation::Model::DiscreteAnimationKeyFrame current =
                std::move(frames[index]);
            std::uint32_t position = index;
            while (position > 0U &&
                   frames[position - 1U]
                           .keyTimeMicroseconds >
                       current.keyTimeMicroseconds) {
                frames[position] =
                    std::move(frames[position - 1U]);
                --position;
            }
            frames[position] = std::move(current);
        }
        Base::Result<Meta::PropertyValue> base =
            propertyTarget.GetValue(propertyHandle);
        if (!base) return base.GetStatus();
        Aero::Media::Animation::Model::DiscreteAnimation runtime;
        runtime.baseValue = base.Value();
        runtime.timing =
            EffectiveTimelineTiming(
                timeline, inherited);
        if (runtime.timing.durationMicroseconds == 0U &&
            !frames.Empty()) {
            runtime.timing.durationMicroseconds =
                frames.Back().keyTimeMicroseconds;
        }
        runtime.keyFrames = frames.AsSpan();
        Base::Result<Aero::Media::Animation::Model::AnimationHandle> started =
            animations->Begin(
                propertyTarget, propertyHandle, runtime);
        return RetainStartedAnimation(
            std::move(started),
            retainedHandles);
    }

Base::Result<std::uint32_t> ViewState::StartContentElementAnimations(
        Aero::FrameworkContentElement& content,
        Aero::FrameworkElement& actionOwner,
        const Aero::NameScope* names) noexcept {
        std::uint32_t count = 0U;
        for (const Base::Ref<Base::Object>& authored :
             AeroGuiInternal::AuthoredTriggers(
                 content)) {
            if (!authored || authored->RuntimeType() !=
                    MediaAnimation::EventTrigger::StaticTypeId()) {
                continue;
            }
            Base::Result<bool> started = StartEventTrigger(
                static_cast<MediaAnimation::EventTrigger&>(*authored),
                content,
                actionOwner,
                names);
            if (!started) return started.GetStatus();
            if (started.Value()) ++count;
        }
        if (metadata->Types().IsDerivedFrom(
                content.RuntimeType(),
                Documents::Span::StaticTypeId())) {
            const Documents::InlineCollectionView inlines =
                static_cast<const Documents::Span&>(content).GetInlines();
            for (std::uint32_t index = 0U;
                 index < inlines.GetCount(); ++index) {
                const Documents::Inline* child = inlines.GetItem(index);
                if (child == nullptr) continue;
                Base::Result<std::uint32_t> nested =
                    StartContentElementAnimations(
                        const_cast<Documents::Inline&>(*child),
                        actionOwner,
                        names);
                if (!nested) return nested.GetStatus();
                if (count > UINT32_MAX - nested.Value()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Content trigger count overflow");
                }
                count += nested.Value();
            }
        }
        return count;
    }

Base::Result<std::uint32_t> ViewState::StartLoadedAnimations(
        Aero::Media::Visual* visual,
        const Aero::NameScope* names) noexcept {
        if (visual == nullptr) return std::uint32_t{0U};
        std::uint32_t count = 0U;
        Aero::FrameworkElement* element =
            visual->AsFrameworkElement();
        if (element != nullptr) {
            for (const Base::Ref<Base::Object>& authoredBehavior :
                 AeroGuiInternal::AuthoredBehaviors(
                     *element)) {
                if (!authoredBehavior ||
                    !metadata->Types().IsDerivedFrom(
                        authoredBehavior->RuntimeType(),
                        Interactivity::Behavior::StaticTypeId())) {
                    continue;
                }
                Base::Result<void> attached = AttachBehavior(
                    static_cast<const Interactivity::Behavior&>(
                        *authoredBehavior),
                    *element,
                    names,
                    false);
                if (!attached) return attached.GetStatus();
            }
            for (const Base::Ref<Base::Object>& behaviorPrototype :
                 AeroGuiInternal::StyleBehaviorPrototypes(
                     *element)) {
                if (!behaviorPrototype ||
                    !metadata->Types().IsDerivedFrom(
                        behaviorPrototype->RuntimeType(),
                        Interactivity::Behavior::StaticTypeId())) {
                    continue;
                }
                Base::Result<void> attached = AttachBehavior(
                    static_cast<const Interactivity::Behavior&>(
                        *behaviorPrototype),
                    *element,
                    names,
                    true);
                if (!attached) return attached.GetStatus();
            }
            if (input != nullptr &&
                metadata->Types().IsDerivedFrom(
                    element->RuntimeType(),
                    Controls::Grid::StaticTypeId())) {
                auto& grid = static_cast<Controls::Grid&>(*element);
                for (const Base::Ref<Input::KeyBinding>& binding :
                     grid.GetInputBindings()) {
                    if (!binding) continue;
                    Base::Result<Input::InputBindingHandle> added =
                        input->AddInputBinding(*element, binding);
                    if (!added) return added.GetStatus();
                }
            }
            for (const Base::Ref<Base::Object>& authored :
                 AeroGuiInternal::AuthoredTriggers(*element)) {
                if (!authored) {
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::Controls::DataTemplateTriggerState::
                            StaticTypeId()) {
                    Base::Result<std::uint32_t> started =
                        StartDataTemplateTriggers(
                            static_cast<
                                Aero::Controls::DataTemplateTriggerState&>(
                                        *authored));
                    if (!started) {
                        return started.GetStatus();
                    }
                    if (count >
                        UINT32_MAX - started.Value()) {
                        return Base::Status::Failure(
                            Base::ErrorCode::OutOfRange,
                            "DataTemplate Trigger subscription count overflow");
                    }
                    count += started.Value();
                    continue;
                }
                if (authored->RuntimeType() ==
                    MediaAnimation::StoryboardCompletedTrigger::
                        StaticTypeId()) {
                    Base::Result<void> retained =
                        storyboardCompletedSubscriptions.
                            PushBack({
                                static_cast<
                                    MediaAnimation::
                                        StoryboardCompletedTrigger*>(
                                            authored.Get()),
                                element,
                                names});
                    if (!retained) {
                        return retained.GetStatus();
                    }
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::Interactivity::PropertyChangedTrigger::
                        StaticTypeId()) {
                    Base::Result<bool> started =
                        StartPropertyChangedTrigger(
                            static_cast<
                                Aero::Interactivity::PropertyChangedTrigger&>(
                                    *authored),
                            *element,
                            names);
                    if (!started) return started.GetStatus();
                    if (started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::Interactivity::KeyTrigger::StaticTypeId()) {
                    Base::Result<bool> started = StartKeyTrigger(
                        static_cast<Aero::Interactivity::KeyTrigger&>(
                            *authored),
                        *element,
                        names);
                    if (!started) return started.GetStatus();
                    if (started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::DataTrigger::StaticTypeId()) {
                    Base::Result<bool> started =
                        StartInteractionDataTrigger(
                            static_cast<Aero::DataTrigger&>(*authored),
                            *element,
                            names);
                    if (!started) return started.GetStatus();
                    if (started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() !=
                    MediaAnimation::EventTrigger::StaticTypeId()) {
                    continue;
                }
                Base::Result<bool> started = StartEventTrigger(
                    static_cast<MediaAnimation::EventTrigger&>(*authored),
                    *element,
                    *element,
                    names);
                if (!started) return started.GetStatus();
                if (started.Value()) ++count;
            }
            for (const Base::Ref<Base::Object>& authored :
                 AeroGuiInternal::StyleTriggerPrototypes(
                     *element)) {
                if (!authored) continue;
                if (authored->RuntimeType() ==
                    MediaAnimation::StoryboardCompletedTrigger::StaticTypeId()) {
                    Base::Result<void> retained =
                        storyboardCompletedSubscriptions.PushBack({
                            static_cast<MediaAnimation::StoryboardCompletedTrigger*>(
                                authored.Get()),
                            element,
                            names});
                    if (!retained) return retained.GetStatus();
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::Interactivity::PropertyChangedTrigger::StaticTypeId()) {
                    Base::Result<bool> started = StartPropertyChangedTrigger(
                        static_cast<Aero::Interactivity::PropertyChangedTrigger&>(
                            *authored),
                        *element,
                        names);
                    if (!started) return started.GetStatus();
                    if (started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::Interactivity::KeyTrigger::StaticTypeId()) {
                    Base::Result<bool> started = StartKeyTrigger(
                        static_cast<Aero::Interactivity::KeyTrigger&>(*authored),
                        *element,
                        names);
                    if (!started) return started.GetStatus();
                    if (started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::DataTrigger::StaticTypeId()) {
                    Base::Result<bool> started =
                        StartInteractionDataTrigger(
                            static_cast<Aero::DataTrigger&>(*authored),
                            *element,
                            names);
                    if (!started) return started.GetStatus();
                    if (started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    MediaAnimation::EventTrigger::StaticTypeId()) {
                    Base::Result<bool> started = StartEventTrigger(
                        static_cast<MediaAnimation::EventTrigger&>(*authored),
                        *element,
                        *element,
                        names);
                    if (!started) return started.GetStatus();
                    if (started.Value()) ++count;
                }
            }
            if (styles != nullptr) {
                const Aero::Style* applied = styles->AppliedStyle(*element);
                if (applied != nullptr) {
                    for (const Base::Ref<Aero::TriggerBase>& authored :
                         applied->GetAuthoredTriggers()) {
                        if (!authored ||
                            !metadata->Types().IsDerivedFrom(
                                authored->RuntimeType(),
                                MediaAnimation::EventTrigger::StaticTypeId())) {
                            continue;
                        }
                        Base::Result<bool> started = StartEventTrigger(
                            static_cast<MediaAnimation::EventTrigger&>(
                                *authored),
                            *element,
                            *element,
                            names);
                        if (!started) return started.GetStatus();
                        if (started.Value()) ++count;
                    }
                }
            }
            if (metadata->Types().IsDerivedFrom(
                    element->RuntimeType(),
                    Controls::TextBlock::StaticTypeId())) {
                const Documents::InlineCollectionView inlines =
                    static_cast<const Controls::TextBlock&>(*element)
                        .GetInlines();
                for (std::uint32_t index = 0U;
                     index < inlines.GetCount(); ++index) {
                    const Documents::Inline* inlineValue =
                        inlines.GetItem(index);
                    if (inlineValue == nullptr) continue;
                    Base::Result<std::uint32_t> started =
                        StartContentElementAnimations(
                            const_cast<Documents::Inline&>(*inlineValue),
                            *element,
                            names);
                    if (!started) return started.GetStatus();
                    if (count > UINT32_MAX - started.Value()) {
                        return Base::Status::Failure(
                            Base::ErrorCode::OutOfRange,
                            "Inline trigger count overflow");
                    }
                    count += started.Value();
                }
            }
        }
        for (Aero::Media::Visual* child :
             visual->GetVisualChildren()) {
            Base::Result<std::uint32_t> started =
                StartLoadedAnimations(child, names);
            if (!started) return started.GetStatus();
            if (count > UINT32_MAX - started.Value()) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Loaded animation count overflow");
            }
            count += started.Value();
        }
        return count;
    }

Base::Result<void>
ViewState::ExecuteAnimationAction(
    Aero::Interactivity::TriggerAction& action,
    Aero::FrameworkElement& owner,
    Aero::Controls::DataTemplateTriggerState*
        dataTemplateContext,
    const Aero::NameScope* names) noexcept
{
    const Meta::TypeId type =
        action.RuntimeType();
    if (type ==
        Aero::Interactivity::ChangePropertyAction::StaticTypeId()) {
        auto& change =
            static_cast<Aero::Interactivity::ChangePropertyAction&>(
                action);
        Base::Object* targetObject =
            change.GetTargetName().Empty()
            ? static_cast<Base::Object*>(&owner)
            : dataTemplateContext != nullptr
                ? dataTemplateContext->FindName(
                      change.GetTargetName())
                : names != nullptr
                    ? names->Find(change.GetTargetName())
                    : loadedDocument.names.Find(
                          change.GetTargetName());
        if (targetObject == nullptr ||
            !metadata->Types().IsDerivedFrom(
                targetObject->RuntimeType(),
                ::Aero::DependencyObject::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "ChangePropertyAction TargetName did not resolve to a DependencyObject");
        }
        auto& target =
            static_cast<::Aero::DependencyObject&>(
                *targetObject);
        Base::Result<ResolvedAnimationProperty> resolved =
            ResolveAnimationProperty(
                target, change.GetPropertyName());
        if (!resolved) return resolved.GetStatus();

        ::Aero::DependencyObject& propertyTarget =
            *resolved.Value().target;
        const Meta::DependencyPropertyHandle propertyHandle =
            resolved.Value().property;
        const Meta::DependencyProperty* property =
            ::Aero::MetadataPrivate::
                DependencyProperties(*metadata)
                    .Find(propertyHandle);
        if (property == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "ChangePropertyAction property metadata was not found");
        }

        Meta::PropertyValue value = change.GetValue();
        Base::Ref<Data::Binding> valueBinding =
            change.GetValueBinding();
        if (valueBinding) {
            Base::Result<Meta::PropertyValue> evaluated =
                EvaluateAuthoredBinding(
                    *valueBinding,
                    owner,
                    dataTemplateContext,
                    names,
                    &action);
            if (!evaluated) return evaluated.GetStatus();
            value = std::move(evaluated).Value();
        }
        if (value.IsNullObject() &&
            propertyHandle ==
                Controls::Primitives::ToggleButton::
                    IsCheckedProperty.Handle() &&
            metadata->Types().IsDerivedFrom(
                propertyTarget.RuntimeType(),
                Controls::Primitives::ToggleButton::
                    StaticTypeId())) {
            static_cast<Controls::Primitives::ToggleButton&>(
                propertyTarget).SetIsChecked(Nullable<bool>{});
            return {};
        }
        Base::Result<Meta::PropertyValue> coerced =
            Data::CoerceBindingTargetValue(
                metadata,
                *property,
                std::move(value));
        if (!coerced) return coerced.GetStatus();
        propertyTarget.SetCurrentValue(
            propertyHandle,
            std::move(coerced).Value());
        return {};
    }

    if (type ==
        Aero::Interactivity::InvokeCommandAction::StaticTypeId()) {
        auto& invoke =
            static_cast<Aero::Interactivity::InvokeCommandAction&>(action);
        Base::Ref<Input::ICommand> command = invoke.GetCommand();
        if (!command && invoke.GetCommandBinding()) {
            Base::Result<Meta::PropertyValue> evaluated =
                EvaluateAuthoredBinding(
                    *invoke.GetCommandBinding(),
                    owner,
                    dataTemplateContext,
                    names,
                    &action);
            if (!evaluated) return evaluated.GetStatus();
            if (evaluated.Value().Kind() != Meta::ValueKind::Object ||
                evaluated.Value().IsNullObject() ||
                !evaluated.Value().AsObject() ||
                !metadata->Types().IsDerivedFrom(
                    evaluated.Value().AsObject()->RuntimeType(),
                    Input::ICommand::StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "InvokeCommandAction Binding did not return ICommand");
            }
            command = Base::Ref<Input::ICommand>::FromBorrowed(
                *static_cast<Input::ICommand*>(
                    evaluated.Value().AsObject().Get()));
        }
        if (!command) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "InvokeCommandAction Command is unavailable");
        }

        Meta::PropertyValue parameter = invoke.GetCommandParameter();
        if (invoke.GetCommandParameterBinding()) {
            Base::Result<Meta::PropertyValue> evaluated =
                EvaluateAuthoredBinding(
                    *invoke.GetCommandParameterBinding(),
                    owner,
                    dataTemplateContext,
                    names,
                    &action);
            if (!evaluated) return evaluated.GetStatus();
            parameter = std::move(evaluated).Value();
        }
        if (parameter.IsUnset()) {
            parameter = Meta::PropertyValue::NullObject(
                Meta::TypeOf<Base::Object>());
        }
        Aero::UIElement* target = owner.AsUIElement();
        if (target == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "InvokeCommandAction owner is not a UIElement");
        }
        Base::Result<bool> canExecute = input != nullptr
            ? input->CanExecute(*command, parameter, *target)
            : command->CanExecute(parameter, target);
        if (!canExecute) return canExecute.GetStatus();
        if (!canExecute.Value()) return {};
        if (input != nullptr) {
            Base::Result<bool> executed =
                input->Execute(*command, parameter, *target);
            return executed
                ? Base::Result<void>()
                : Base::Result<void>(executed.GetStatus());
        }
        command->Execute(parameter, target);
        return {};
    }

    if (type == Aero::Interactivity::SetFocusAction::StaticTypeId()) {
        auto& setFocus = static_cast<Aero::Interactivity::SetFocusAction&>(action);
        if (!setFocus.GetEngage() || input == nullptr) return {};
        Base::Object* targetObject = setFocus.GetTargetName().Empty()
            ? static_cast<Base::Object*>(&owner)
            : dataTemplateContext != nullptr
                ? dataTemplateContext->FindName(setFocus.GetTargetName())
                : names != nullptr
                    ? names->Find(setFocus.GetTargetName())
                    : loadedDocument.names.Find(setFocus.GetTargetName());
        Aero::UIElement* target =
            targetObject != nullptr && metadata->Types().IsDerivedFrom(
                targetObject->RuntimeType(), Aero::UIElement::StaticTypeId())
            ? static_cast<Aero::UIElement*>(targetObject)
            : nullptr;
        if (target == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "SetFocusAction target is unavailable");
        }
        if (!target->GetIsLoaded()) {
            return QueueFocus(*target);
        }
        if (!target->GetIsEnabled()) return {};
        Base::Result<bool> focused = input->SetFocus(target);
        return focused
            ? Base::Result<void>()
            : Base::Result<void>(focused.GetStatus());
    }

    if (type == Aero::Interactivity::SelectAction::StaticTypeId()) {
        if (metadata->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Controls::ListBoxItem::StaticTypeId())) {
            static_cast<Controls::ListBoxItem&>(owner)
                .SetIsSelected(true);
            return {};
        }
        if (metadata->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Controls::TabItem::StaticTypeId())) {
            static_cast<Controls::TabItem&>(owner)
                .SetIsSelected(true);
            return {};
        }
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "SelectAction owner is not a selectable item container");
    }

    if (type == Aero::Interactivity::SelectAllAction::StaticTypeId()) {
        if (metadata->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Controls::TextBox::StaticTypeId())) {
            return static_cast<Controls::TextBox&>(owner)
                .SelectAll();
        }
        if (metadata->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Controls::PasswordBox::StaticTypeId())) {
            return static_cast<Controls::PasswordBox&>(owner)
                .SelectAll();
        }
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "SelectAllAction owner is not a text editor");
    }

    if (type == Aero::Interactivity::PlaySoundAction::StaticTypeId()) {
        auto& playSound =
            static_cast<Aero::Interactivity::PlaySoundAction&>(action);
        if (!playSound.GetIsEnabled() ||
            playSound.GetSource().Empty()) {
            return {};
        }
        const double volume = playSound.GetVolume();
        if (!std::isfinite(volume) ||
            volume < 0.0 || volume > 1.0) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "PlaySoundAction Volume must be between zero and one");
        }
        Base::Result<void> initialized = audio.Initialize();
        if (!initialized &&
            (initialized.GetStatus().code ==
                 Base::ErrorCode::Unsupported ||
             initialized.GetStatus().code ==
                 Base::ErrorCode::InvalidState)) {
            // Audio is optional for headless and provider-free hosts.
            return {};
        }
        if (!initialized) return initialized.GetStatus();
        audio.SetEffectsVolume(
            static_cast<float>(volume));
        Base::Result<void> played =
            audio.PlayEffect(playSound.GetSource());
        if (!played &&
            (played.GetStatus().code ==
                 Base::ErrorCode::InvalidState ||
             played.GetStatus().code ==
                 Base::ErrorCode::NotFound)) {
            // A missing device or authored file must not poison the UI
            // trigger pipeline.
            return {};
        }
        return played;
    }

    if (type == Aero::Interactivity::RemoveElementAction::StaticTypeId()) {
        auto& remove = static_cast<Aero::Interactivity::RemoveElementAction&>(action);
        Base::Object* targetObject = static_cast<Base::Object*>(&owner);
        Base::Ref<Data::Binding> targetBinding =
            remove.GetTargetObject();
        if (targetBinding) {
            const Base::Ref<Data::RelativeSource> relative = targetBinding->GetRelativeSource();
            if (!relative || relative->GetMode() != Data::RelativeSourceMode::FindAncestor ||
                relative->GetAncestorType() != Base::StringView("ContextMenu") ||
                targetBinding->GetPath().GetPath() != Base::StringView("PlacementTarget")) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "RemoveElementAction TargetObject binding is not supported");
            }
            Aero::Media::Visual* current = &owner;
            Controls::ContextMenu* contextMenu = nullptr;
            while (current != nullptr) {
                if (metadata->Types().IsDerivedFrom(
                        current->RuntimeType(),
                        Controls::ContextMenu::StaticTypeId())) {
                    contextMenu = static_cast<Controls::ContextMenu*>(
                        current);
                    break;
                }
                current = current->GetLogicalParent() != nullptr
                    ? current->GetLogicalParent()
                    : current->GetVisualParent();
            }
            if (contextMenu == nullptr ||
                !contextMenu->GetPlacementTarget()) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "RemoveElementAction ContextMenu PlacementTarget was not found");
            }
            targetObject = contextMenu->GetPlacementTarget().Get();
        }
        if (targetObject == nullptr ||
            !metadata->Types().IsDerivedFrom(
                targetObject->RuntimeType(),
                Aero::UIElement::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "RemoveElementAction target is not a UIElement");
        }
        auto& target = static_cast<Aero::UIElement&>(*targetObject);
        Aero::Media::Visual* current = target.GetLogicalParent() != nullptr
            ? target.GetLogicalParent() : target.GetVisualParent();
        while (current != nullptr) {
            if (metadata->Types().IsDerivedFrom(
                    current->RuntimeType(),
                    Controls::ItemsControl::StaticTypeId())) {
                auto& items = static_cast<Controls::ItemsControl&>(*current);
                std::uint32_t index = UINT32_MAX;
                for (std::uint32_t candidate = 0U;
                     candidate < items.GetCount(); ++candidate) {
                    Base::Ref<Base::Object> item = items.GetItem(candidate);
                    if (item.Get() == &target) {
                        index = candidate;
                        break;
                    }
                }
                if (index != UINT32_MAX) {
                    Base::Result<Base::Ref<Base::Object>> removed =
                        items.GetItems().RemoveAt(index);
                    return removed
                        ? Base::Result<void>()
                        : Base::Result<void>(removed.GetStatus());
                }
            }
            current = current->GetLogicalParent() != nullptr
                ? current->GetLogicalParent() : current->GetVisualParent();
        }
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "RemoveElementAction target is not owned by an ItemsControl");
    }

    if (animations == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Storyboard action requires the animation manager");
    }
    if (type ==
        MediaAnimation::BeginStoryboard::StaticTypeId()) {
        auto& begin =
            static_cast<MediaAnimation::BeginStoryboard&>(
                action);
        if (!begin.GetStoryboard()) return {};
        if (!begin.GetName().Empty()) {
            for (std::uint32_t index = 0U;
                 index < storyboardSessions.Size();
                 ++index) {
                StoryboardSession& existing =
                    storyboardSessions[index];
                if (existing.name.View() != begin.GetName()) {
                    continue;
                }
                CancelStoryboardCompletionSessions(
                    existing.handles.AsSpan());
                for (Aero::Media::Animation::Model::AnimationHandle handle :
                     existing.handles) {
                    static_cast<void>(
                        animations->Remove(handle));
                }
                for (std::uint32_t next = index + 1U;
                     next < storyboardSessions.Size();
                     ++next) {
                    storyboardSessions[next - 1U] =
                        std::move(
                            storyboardSessions[next]);
                }
                storyboardSessions.PopBack();
                break;
            }
        }
        StoryboardCompletionSession completion(allocator);
        completion.storyboard = begin.GetStoryboard();
        completion.owner = &owner;
        Base::Result<std::uint32_t> started =
            BeginTimeline(
                *begin.GetStoryboard(),
                owner, names, nullptr,
                &completion.handles,
                dataTemplateContext);
        if (!started) {
            for (Aero::Media::Animation::Model::AnimationHandle handle :
                 completion.handles) {
                static_cast<void>(
                    animations->Remove(handle));
            }
            return started.GetStatus();
        }
        StoryboardSession namedSession(allocator);
        if (!begin.GetName().Empty()) {
            namedSession.owner = &owner;
            Base::Result<void> named =
                namedSession.name.Assign(begin.GetName());
            if (named) {
                named = namedSession.handles.Append(
                    completion.handles.AsSpan());
            }
            if (!named) {
                for (Aero::Media::Animation::Model::AnimationHandle handle :
                     completion.handles) {
                    static_cast<void>(
                        animations->Remove(handle));
                }
                return named.GetStatus();
            }
        }
        Base::Result<void> retained =
            storyboardCompletionSessions.PushBack(
                std::move(completion));
        if (!retained) {
            for (Aero::Media::Animation::Model::AnimationHandle handle :
                 completion.handles) {
                static_cast<void>(
                    animations->Remove(handle));
            }
            return retained.GetStatus();
        }
        if (!begin.GetName().Empty()) {
            retained = storyboardSessions.PushBack(
                std::move(namedSession));
            if (!retained) {
                for (Aero::Media::Animation::Model::AnimationHandle handle :
                     storyboardCompletionSessions.Back().
                         handles) {
                    static_cast<void>(
                        animations->Remove(handle));
                }
                storyboardCompletionSessions.PopBack();
                return retained.GetStatus();
            }
        }
        return {};
    }

    if (type == MediaAnimation::ControlStoryboardAction::StaticTypeId()) {
        auto& control = static_cast<MediaAnimation::ControlStoryboardAction&>(action);
        if (!control.GetStoryboard()) return {};
        if (control.GetControlOption() == MediaAnimation::ControlStoryboardAction::Option::Play) {
            MediaAnimation::BeginStoryboard begin;
            begin.SetStoryboard(control.GetStoryboard());
            return ExecuteAnimationAction(
                begin, owner, dataTemplateContext, names);
        }
        bool found = false;
        for (StoryboardCompletionSession& session : storyboardCompletionSessions) {
            if (session.owner != &owner || session.storyboard.Get() != control.GetStoryboard().Get()) continue;
            found = true;
            for (Aero::Media::Animation::Model::AnimationHandle handle : session.handles) {
                Base::Result<void> result;
                if (control.GetControlOption() == MediaAnimation::ControlStoryboardAction::Option::Stop) result = animations->Stop(handle);
                else if (control.GetControlOption() == MediaAnimation::ControlStoryboardAction::Option::Pause) result = animations->Pause(handle);
                else if (control.GetControlOption() == MediaAnimation::ControlStoryboardAction::Option::Resume) result = animations->Resume(handle);
                else return Base::Status::Failure(Base::ErrorCode::Unsupported, "ControlStoryboardAction option is not implemented");
                if (!result) return result.GetStatus();
            }
        }
        return found ? Base::Result<void>{} : Base::Status::Failure(
            Base::ErrorCode::NotFound, "ControlStoryboardAction storyboard was not started");
    }

    if (type == MediaAnimation::PlayMediaAction::StaticTypeId() ||
        type == MediaAnimation::PauseMediaAction::StaticTypeId() ||
        type == MediaAnimation::StopMediaAction::StaticTypeId()) {
        Base::StringView targetName = type ==
                MediaAnimation::PlayMediaAction::StaticTypeId()
            ? static_cast<MediaAnimation::PlayMediaAction&>(action)
                  .GetTargetName()
            : type == MediaAnimation::PauseMediaAction::StaticTypeId()
                ? static_cast<MediaAnimation::PauseMediaAction&>(action)
                      .GetTargetName()
                : static_cast<MediaAnimation::StopMediaAction&>(action)
                      .GetTargetName();
        Base::Object* targetObject = targetName.Empty()
            ? static_cast<Base::Object*>(&owner)
            : names != nullptr
                ? names->Find(targetName)
                : loadedDocument.names.Find(targetName);
        if (targetObject == nullptr ||
            !metadata->Types().IsDerivedFrom(
                targetObject->RuntimeType(),
                Aero::Media::MediaElement::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "MediaAction TargetName did not resolve to a MediaElement");
        }
        auto& media = static_cast<Aero::Media::MediaElement&>(
            *targetObject);
        if (type == MediaAnimation::PlayMediaAction::StaticTypeId()) {
            media.Play();
        } else if (type ==
            MediaAnimation::PauseMediaAction::StaticTypeId()) {
            media.Pause();
        } else {
            media.Stop();
        }
        return {};
    }

    if (!metadata->Types().IsDerivedFrom(
            type,
            MediaAnimation::
                ControllableStoryboardAction::
                    StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "EventTrigger contains an unsupported action");
    }
    auto& control =
        static_cast<
            MediaAnimation::ControllableStoryboardAction&>(
                action);
    std::uint32_t sessionIndex = UINT32_MAX;
    for (std::uint32_t index = 0U;
         index < storyboardSessions.Size();
         ++index) {
        if (storyboardSessions[index].name.View() ==
                control.GetBeginStoryboardName()) {
            sessionIndex = index;
            break;
        }
    }
    if (sessionIndex == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Controllable Storyboard was not started");
    }
    StoryboardSession& session =
        storyboardSessions[sessionIndex];
    for (Aero::Media::Animation::Model::AnimationHandle handle :
         session.handles) {
        Base::Result<void> result;
        if (type ==
            MediaAnimation::PauseStoryboard::
                StaticTypeId()) {
            result = animations->Pause(handle);
        } else if (type ==
            MediaAnimation::ResumeStoryboard::
                StaticTypeId()) {
            result = animations->Resume(handle);
        } else if (type ==
            MediaAnimation::StopStoryboard::
                StaticTypeId()) {
            result = animations->Stop(handle);
        } else if (type ==
            MediaAnimation::RemoveStoryboard::
                StaticTypeId()) {
            result = animations->Remove(handle);
        } else if (type ==
            MediaAnimation::SeekStoryboard::
                StaticTypeId()) {
            result = animations->Seek(
                handle,
                static_cast<
                    MediaAnimation::SeekStoryboard&>(
                        action).
                    GetOffsetMicroseconds());
        } else {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Controllable Storyboard action is unsupported");
        }
        if (!result) return result.GetStatus();
    }
    if (type ==
            MediaAnimation::StopStoryboard::StaticTypeId() ||
        type ==
            MediaAnimation::RemoveStoryboard::StaticTypeId()) {
        CancelStoryboardCompletionSessions(
            session.handles.AsSpan());
    }
    if (type ==
        MediaAnimation::RemoveStoryboard::StaticTypeId()) {
        for (std::uint32_t next =
                 sessionIndex + 1U;
             next < storyboardSessions.Size();
             ++next) {
            storyboardSessions[next - 1U] =
                std::move(
                    storyboardSessions[next]);
        }
        storyboardSessions.PopBack();
    }
    return {};
}

void ViewState::
CancelStoryboardCompletionSessions(
    Base::Span<const Aero::Media::Animation::Model::AnimationHandle>
        handles) noexcept
{
    for (std::uint32_t index = 0U;
         index < storyboardCompletionSessions.Size();) {
        bool matches = false;
        for (Aero::Media::Animation::Model::AnimationHandle sessionHandle :
             storyboardCompletionSessions[index].handles) {
            for (Aero::Media::Animation::Model::AnimationHandle handle :
                 handles) {
                if (sessionHandle == handle) {
                    matches = true;
                    break;
                }
            }
            if (matches) break;
        }
        if (!matches) {
            ++index;
            continue;
        }
        for (std::uint32_t next = index + 1U;
             next < storyboardCompletionSessions.Size();
             ++next) {
            storyboardCompletionSessions[next - 1U] =
                std::move(
                    storyboardCompletionSessions[next]);
        }
        storyboardCompletionSessions.PopBack();
    }
}

Base::Result<std::uint32_t>
ViewState::ProcessStoryboardCompletions() noexcept
{
    std::uint32_t actionCount = 0U;
    std::uint32_t index = 0U;
    while (index < storyboardCompletionSessions.Size()) {
        StoryboardCompletionSession& session =
            storyboardCompletionSessions[index];
        bool completed = true;
        for (Aero::Media::Animation::Model::AnimationHandle handle :
             session.handles) {
            const Aero::Media::Animation::Model::AnimationState state =
                animations->State(handle);
            if (state ==
                    Aero::Media::Animation::Model::AnimationState::Active ||
                state ==
                    Aero::Media::Animation::Model::AnimationState::Paused) {
                completed = false;
                break;
            }
        }
        if (!completed) {
            ++index;
            continue;
        }

        Base::Ref<MediaAnimation::Storyboard> storyboard =
            session.storyboard;
        for (std::uint32_t next = index + 1U;
             next < storyboardCompletionSessions.Size();
             ++next) {
            storyboardCompletionSessions[next - 1U] =
                std::move(
                    storyboardCompletionSessions[next]);
        }
        storyboardCompletionSessions.PopBack();

        for (const StoryboardCompletedSubscription&
                 subscription :
             storyboardCompletedSubscriptions) {
            if (subscription.trigger == nullptr ||
                subscription.owner == nullptr ||
                subscription.trigger->GetStoryboard().Get() !=
                    storyboard.Get()) {
                continue;
            }
            Base::Result<bool> allowed = ConditionBehaviorsAllowExecution(
                subscription.trigger->GetBehaviors(),
                *subscription.owner,
                subscription.names);
            if (!allowed) return allowed.GetStatus();
            if (!allowed.Value()) continue;
            for (const Base::Ref<
                     Aero::Interactivity::TriggerAction>& action :
                 subscription.trigger->GetActions()) {
                if (!action) continue;
                Base::Result<void> executed =
                    ExecuteAnimationAction(
                        *action, *subscription.owner,
                        nullptr, subscription.names);
                if (!executed) {
                    return executed.GetStatus();
                }
                if (actionCount == UINT32_MAX) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Storyboard completed action count overflow");
                }
                ++actionCount;
            }
        }
    }
    return actionCount;
}


} // namespace Aero
