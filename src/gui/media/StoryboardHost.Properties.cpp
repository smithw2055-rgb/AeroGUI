#include "gui/ViewState.hpp"
#include "gui/media/StoryboardHostInternal.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/core/state/EventRouter.hpp"
#include <Aero/CommandBinding.hpp>
#include <Aero/Media/Animation/EventTrigger.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/PathGeometry.hpp>
#include <Aero/Media/LineSegment.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Media/CompositeTransform3D.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/TryCast.hpp>

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

Base::Result<StoryboardHost::ResolvedAnimationProperty>
StoryboardHost::ResolveAnimationProperty(
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
                    // Blend/WPF intro storyboards target
                    // RenderTransform.Children[0]/[3] of the default
                    // Scale/Skew/Rotate/Translate group. If the Style has
                    // not applied yet (Loaded clocks), materialize that group.
                    if (!metadata->Types().IsDerivedFrom(
                            target.RuntimeType(),
                            Aero::UIElement::StaticTypeId())) {
                        return Base::Status::Failure(
                            Base::ErrorCode::NotFound,
                            "Storyboard transform path has no TransformGroup");
                    }
                    Base::Result<Base::Ref<Media::TransformGroup>> group =
                        Base::MakeRef<Media::TransformGroup>();
                    if (!group) return group.GetStatus();
                    Base::Result<Base::Ref<Media::ScaleTransform>> scale =
                        Base::MakeRef<Media::ScaleTransform>();
                    Base::Result<Base::Ref<Media::SkewTransform>> skew =
                        Base::MakeRef<Media::SkewTransform>();
                    Base::Result<Base::Ref<Media::RotateTransform>> rotate =
                        Base::MakeRef<Media::RotateTransform>();
                    Base::Result<Base::Ref<Media::TranslateTransform>>
                        translate =
                            Base::MakeRef<Media::TranslateTransform>();
                    if (!scale || !skew || !rotate || !translate) {
                        return Base::Status::Failure(
                            Base::ErrorCode::OutOfMemory,
                            "Unable to allocate Storyboard TransformGroup children");
                    }
                    Base::Result<void> added =
                        group.Value()->AddChild(
                            Base::Ref<Media::Transform>(scale.Value()));
                    if (added) {
                        added = group.Value()->AddChild(
                            Base::Ref<Media::Transform>(skew.Value()));
                    }
                    if (added) {
                        added = group.Value()->AddChild(
                            Base::Ref<Media::Transform>(rotate.Value()));
                    }
                    if (added) {
                        added = group.Value()->AddChild(
                            Base::Ref<Media::Transform>(translate.Value()));
                    }
                    if (!added) return added.GetStatus();
                    auto& element =
                        static_cast<Aero::UIElement&>(target);
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
                        static_cast<Aero::FrameworkElement&>(element)
                            .SetLayoutTransform(
                                Base::Ref<Media::Transform>(
                                    group.Value()));
                    } else {
                        element.SetRenderTransform(
                            Base::Ref<Media::Transform>(
                                group.Value()));
                    }
                    transform = Base::Ref<Media::Transform>(
                        group.Value());
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
                const bool transform3DOwner =
                    ownerPath.SizeBytes() >= 11U &&
                    ownerPath.Substr(
                        ownerPath.SizeBytes() - 11U,
                        11U) == Base::StringView("Transform3D");
                if (transform3DOwner &&
                    metadata->Types().IsDerivedFrom(
                        target.RuntimeType(),
                        Aero::UIElement::StaticTypeId())) {
                    // `(aero:Element.Transform3D).(…RotationY)` is a
                    // parenthesized compound path. Find("Transform3D") on a
                    // Grid returns UIElement::Transform3D, whose default is
                    // empty even when XAML wrote the Element attached DP.
                    // GetTransform3D() reads both; render already does.
                    auto& element =
                        static_cast<Aero::UIElement&>(target);
                    Base::Ref<Media::Transform3D> existing =
                        element.GetTransform3D();
                    if (!existing) {
                        Base::Result<Base::Ref<Media::CompositeTransform3D>>
                            created =
                                Base::MakeRef<Media::CompositeTransform3D>();
                        if (!created) return created.GetStatus();
                        element.SetTransform3D(
                            Base::Ref<Media::Transform3D>(
                                created.Value()));
                        existing = Base::Ref<Media::Transform3D>(
                            created.Value());
                    }
                    propertyTarget = existing.Get();
                    path = terminalPath;
                    indexedPathResolved = true;
                } else {
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
        }

        std::uint32_t dot = UINT32_MAX;
        if (!indexedPathResolved) {
            std::uint32_t parentheses = 0U;
            for (std::uint32_t index = 0U;
                 index < path.SizeBytes(); ++index) {
                const char character = path[index];
                if (character == '(') {
                    ++parentheses;
                } else if (character == ')' && parentheses != 0U) {
                    --parentheses;
                } else if (character == '.' && parentheses == 0U) {
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
                    if (token == Base::StringView("Transform3D") &&
                        metadata->Types().IsDerivedFrom(
                            current->RuntimeType(),
                            Aero::UIElement::StaticTypeId())) {
                        auto& element =
                            *static_cast<Aero::UIElement*>(current);
                        Base::Ref<Media::Transform3D> existing =
                            element.GetTransform3D();
                        if (existing) {
                            current = existing.Get();
                            start = end + 1U;
                            if (++depth > 16U) break;
                            continue;
                        }
                        Base::Result<Base::Ref<Media::CompositeTransform3D>>
                            created = Base::MakeRef<Media::CompositeTransform3D>();
                        if (!created) return created.GetStatus();
                        element.SetTransform3D(
                            Base::Ref<Media::Transform3D>(created.Value()));
                        current = created.Value().Get();
                        start = end + 1U;
                        if (++depth > 16U) break;
                        continue;
                    }
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

} // namespace Aero
