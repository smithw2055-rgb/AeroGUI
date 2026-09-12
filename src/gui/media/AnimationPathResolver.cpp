#include "gui/media/AnimationPathResolver.hpp"

#include <Aero/DependencyProperty.hpp>
#include <Aero/DependencyObject.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/GradientBrush.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Media/TransformGroup.hpp>
#include <Aero/Media/CompositeTransform3D.hpp>
#include <Aero/Value.hpp>
#include "gui/core/DependencyPropertyRegistry.hpp"
#include "gui/meta/TypeRegistryDetail.hpp"

#include <cstdint>

namespace Aero {

namespace {

Base::StringView NormalizePropertyPath(Base::StringView path) noexcept {
    while (!path.Empty() && (path[0] == '(' || path[0] == ' ' || path[0] == '\t')) {
        path = path.Substr(1U, path.SizeBytes() - 1U);
    }
    while (!path.Empty() &&
           (path[path.SizeBytes() - 1U] == ')' ||
            path[path.SizeBytes() - 1U] == ' ' ||
            path[path.SizeBytes() - 1U] == '\t')) {
        path = path.Substr(0U, path.SizeBytes() - 1U);
    }
    return path;
}

const Meta::DependencyProperty* FindDependencyProperty(
    Meta::DependencyPropertyRegistry& properties,
    DependencyObject& object,
    Base::StringView authored) noexcept {
    authored = NormalizePropertyPath(authored);
    std::uint32_t separator = UINT32_MAX;
    for (std::uint32_t index = 0U; index < authored.SizeBytes(); ++index) {
        if (authored[index] == '.') separator = index;
    }
    if (separator == UINT32_MAX) {
        return properties.Find(object.RuntimeType(), authored);
    }
    Base::StringView ownerName = authored.Substr(0U, separator);
    const Base::StringView propertyName = authored.Substr(
        separator + 1U, authored.SizeBytes() - separator - 1U);
    if (const Meta::DependencyProperty* targetProperty =
            properties.Find(object.RuntimeType(), propertyName)) {
        return targetProperty;
    }
    for (std::uint32_t index = 0U; index < ownerName.SizeBytes(); ++index) {
        if (ownerName[index] == ':') {
            ownerName = ownerName.Substr(index + 1U, ownerName.SizeBytes() - index - 1U);
        }
    }
    for (const Meta::TypeInfo& type : properties.Types().Types()) {
        if (type.Name() != ownerName) continue;
        const Meta::DependencyProperty* property =
            properties.Find(type.Id(), propertyName);
        if (property != nullptr &&
            (property->IsAttached() ||
             properties.Types().IsDerivedFrom(object.RuntimeType(), type.Id()))) {
            return property;
        }
    }
    return properties.Find(object.RuntimeType(), propertyName);
}

} // namespace

Base::Result<ResolvedAnimationProperty> ResolveAnimationPropertyPath(
    ::Aero::DependencyObject& rootTarget,
    Base::StringView authoredPath,
    Meta::DependencyPropertyRegistry& properties) noexcept {
    if (authoredPath.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Animation target property is required");
    }

    DependencyObject* target = &rootTarget;
    Base::StringView path = authoredPath;
    while (!path.Empty() && (path[0] == ' ' || path[0] == '\t')) {
        path = path.Substr(1U, path.SizeBytes() - 1U);
    }
    while (!path.Empty() &&
           (path[path.SizeBytes() - 1U] == ' ' ||
            path[path.SizeBytes() - 1U] == '\t')) {
        path = path.Substr(0U, path.SizeBytes() - 1U);
    }

    bool compoundParenthesizedPath = false;
    for (std::uint32_t index = 0U; index + 1U < path.SizeBytes(); ++index) {
        if (path[index] == ')' &&
            (path[index + 1U] == '.' || path[index + 1U] == '[')) {
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
    for (std::uint32_t index = 0U; index < path.SizeBytes(); ++index) {
        if (path[index] == '[' && indexedOpen == UINT32_MAX) {
            indexedOpen = index;
        } else if (path[index] == ']' && indexedOpen != UINT32_MAX) {
            indexedClose = index;
            break;
        }
    }

    bool nestedTargetResolved = false;
    if (indexedOpen != UINT32_MAX) {
        if (indexedClose == UINT32_MAX || indexedClose == indexedOpen + 1U) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Indexed animation path has an invalid index");
        }
        std::uint64_t parsedIndex = 0U;
        for (std::uint32_t index = indexedOpen + 1U; index < indexedClose; ++index) {
            if (path[index] < '0' || path[index] > '9') {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Indexed animation path index must be numeric");
            }
            parsedIndex = parsedIndex * 10U + static_cast<std::uint64_t>(path[index] - '0');
            if (parsedIndex > UINT32_MAX) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Indexed animation path index is too large");
            }
        }

        const Base::StringView beforeIndex = path.Substr(0U, indexedOpen);
        Base::StringView terminalPath = path.Substr(
            indexedClose + 1U, path.SizeBytes() - indexedClose - 1U);
        if (!terminalPath.Empty() && terminalPath[0] == '.') {
            terminalPath = terminalPath.Substr(1U, terminalPath.SizeBytes() - 1U);
        }
        if (terminalPath.SizeBytes() >= 2U &&
            terminalPath[0] == '(' &&
            terminalPath[terminalPath.SizeBytes() - 1U] == ')') {
            terminalPath = terminalPath.Substr(1U, terminalPath.SizeBytes() - 2U);
        }

        auto endsWith = [](Base::StringView value, Base::StringView suffix) noexcept {
            return value.SizeBytes() >= suffix.SizeBytes() &&
                   value.Substr(value.SizeBytes() - suffix.SizeBytes(), suffix.SizeBytes()) == suffix;
        };

        const bool gradientStops =
            endsWith(beforeIndex, Base::StringView(").(GradientBrush.GradientStops)")) ||
            endsWith(beforeIndex, Base::StringView(".GradientStops"));
        const bool transformChildren =
            beforeIndex == Base::StringView("(UIElement.RenderTransform).(TransformGroup.Children)") ||
            beforeIndex == Base::StringView("RenderTransform.Children") ||
            beforeIndex == Base::StringView("(FrameworkElement.LayoutTransform).(TransformGroup.Children)") ||
            beforeIndex == Base::StringView("LayoutTransform.Children") ||
            beforeIndex == Base::StringView("(TransformGroup.Children)");

        if (gradientStops) {
            Base::StringView brushOwnerPath;
            if (!beforeIndex.Empty() && beforeIndex[0] == '(') {
                std::uint32_t close = UINT32_MAX;
                for (std::uint32_t index = 1U; index < beforeIndex.SizeBytes(); ++index) {
                    if (beforeIndex[index] == ')') {
                        close = index;
                        break;
                    }
                }
                if (close == UINT32_MAX || close <= 1U) {
                    return Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        "GradientStops owner path is invalid");
                }
                brushOwnerPath = beforeIndex.Substr(1U, close - 1U);
            } else {
                std::uint32_t separator = UINT32_MAX;
                for (std::uint32_t index = 0U; index < beforeIndex.SizeBytes(); ++index) {
                    if (beforeIndex[index] == '.') {
                        separator = index;
                        break;
                    }
                }
                if (separator == UINT32_MAX || separator == 0U) {
                    return Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        "GradientStops owner property is missing");
                }
                brushOwnerPath = beforeIndex.Substr(0U, separator);
            }
            std::uint32_t ownerDot = UINT32_MAX;
            for (std::uint32_t index = 0U; index < brushOwnerPath.SizeBytes(); ++index) {
                if (brushOwnerPath[index] == '.') {
                    ownerDot = index;
                }
            }
            const Base::StringView brushProperty =
                ownerDot == UINT32_MAX
                ? brushOwnerPath
                : brushOwnerPath.Substr(ownerDot + 1U, brushOwnerPath.SizeBytes() - ownerDot - 1U);
            const Meta::DependencyProperty* background =
                properties.Find(target->RuntimeType(), brushProperty);
            if (background == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "GradientBrush property was not found");
            }
            Base::Result<Meta::PropertyValue> value =
                target->GetValue(background->Handle());
            if (!value ||
                value.Value().Kind() != Meta::ValueKind::Object ||
                !value.Value().AsObject() ||
                !properties.Types().IsDerivedFrom(
                    value.Value().AsObject()->RuntimeType(),
                    Media::GradientBrush::StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Target property is not a GradientBrush");
            }
            auto& brush = static_cast<Media::GradientBrush&>(*value.Value().AsObject());
            const auto stops = brush.GetGradientStops();
            if (parsedIndex >= stops.Size() ||
                !stops[static_cast<std::uint32_t>(parsedIndex)]) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "GradientStops index is out of range");
            }
            target = stops[static_cast<std::uint32_t>(parsedIndex)].Get();
            path = terminalPath;
            nestedTargetResolved = true;
        } else if (transformChildren) {
            Base::Ref<Media::Transform> transform;
            if (beforeIndex == Base::StringView("(TransformGroup.Children)") &&
                properties.Types().IsDerivedFrom(
                    target->RuntimeType(),
                    Media::TransformGroup::StaticTypeId())) {
                transform = Base::Ref<Media::Transform>::TryFromBorrowed(
                    static_cast<Media::Transform&>(*target));
            } else {
                const bool layoutPath =
                    beforeIndex == Base::StringView("(FrameworkElement.LayoutTransform).(TransformGroup.Children)") ||
                    beforeIndex == Base::StringView("LayoutTransform.Children");
                if (layoutPath) {
                    if (!properties.Types().IsDerivedFrom(
                            target->RuntimeType(),
                            FrameworkElement::StaticTypeId())) {
                        return Base::Status::Failure(
                            Base::ErrorCode::InvalidArgument,
                            "LayoutTransform target is not a FrameworkElement");
                    }
                    transform = static_cast<FrameworkElement&>(*target).GetLayoutTransform();
                } else {
                    if (!properties.Types().IsDerivedFrom(
                            target->RuntimeType(),
                            UIElement::StaticTypeId())) {
                        return Base::Status::Failure(
                            Base::ErrorCode::InvalidArgument,
                            "RenderTransform target is not a UIElement");
                    }
                    transform = static_cast<UIElement&>(*target).GetRenderTransform();
                }
            }
            if (!transform ||
                !properties.Types().IsDerivedFrom(
                    transform->RuntimeType(),
                    Media::TransformGroup::StaticTypeId())) {
                if (properties.Types().IsDerivedFrom(
                        target->RuntimeType(),
                        UIElement::StaticTypeId())) {
                    auto group = Base::MakeRef<Media::TransformGroup>();
                    auto scale = Base::MakeRef<Media::ScaleTransform>();
                    auto skew = Base::MakeRef<Media::SkewTransform>();
                    auto rotate = Base::MakeRef<Media::RotateTransform>();
                    auto translate = Base::MakeRef<Media::TranslateTransform>();
                    if (group && scale && skew && rotate && translate) {
                        group.Value()->AddChild(Base::Ref<Media::Transform>(scale.Value()));
                        group.Value()->AddChild(Base::Ref<Media::Transform>(skew.Value()));
                        group.Value()->AddChild(Base::Ref<Media::Transform>(rotate.Value()));
                        group.Value()->AddChild(Base::Ref<Media::Transform>(translate.Value()));
                        auto& element = static_cast<UIElement&>(*target);
                        const bool layoutPath =
                            beforeIndex == Base::StringView("(FrameworkElement.LayoutTransform).(TransformGroup.Children)") ||
                            beforeIndex == Base::StringView("LayoutTransform.Children");
                        if (layoutPath) {
                            if (properties.Types().IsDerivedFrom(
                                    target->RuntimeType(),
                                    FrameworkElement::StaticTypeId())) {
                                static_cast<FrameworkElement&>(element).SetLayoutTransform(
                                    Base::Ref<Media::Transform>(group.Value()));
                            }
                        } else {
                            element.SetRenderTransform(
                                Base::Ref<Media::Transform>(group.Value()));
                        }
                        transform = Base::Ref<Media::Transform>(group.Value());
                    }
                }
            }
            if (!transform ||
                !properties.Types().IsDerivedFrom(
                    transform->RuntimeType(),
                    Media::TransformGroup::StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Transform path has no TransformGroup");
            }
            auto& group = static_cast<Media::TransformGroup&>(*transform);
            const auto children = group.GetChildren();
            if (parsedIndex >= children.Size() ||
                !children[static_cast<std::uint32_t>(parsedIndex)]) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "TransformGroup index is out of range");
            }
            target = children[static_cast<std::uint32_t>(parsedIndex)].Get();
            path = terminalPath;
            nestedTargetResolved = true;
        } else {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Indexed animation collection is not supported");
        }
    }

    if (!nestedTargetResolved &&
        indexedOpen == UINT32_MAX &&
        compoundParenthesizedPath &&
        path.SizeBytes() >= 7U &&
        path[0] == '(' &&
        path[path.SizeBytes() - 1U] == ')') {
        std::uint32_t separator = UINT32_MAX;
        for (std::uint32_t index = 1U; index + 2U < path.SizeBytes(); ++index) {
            if (path[index] == ')' &&
                path[index + 1U] == '.' &&
                path[index + 2U] == '(') {
                separator = index;
                break;
            }
        }
        if (separator != UINT32_MAX) {
            Base::StringView ownerPath = path.Substr(1U, separator - 1U);
            const std::uint32_t terminalStart = separator + 3U;
            Base::StringView terminalPath = path.Substr(
                terminalStart, path.SizeBytes() - terminalStart - 1U);
            const bool transform3DOwner =
                ownerPath.SizeBytes() >= 11U &&
                ownerPath.Substr(ownerPath.SizeBytes() - 11U, 11U) == Base::StringView("Transform3D");
            if (transform3DOwner &&
                properties.Types().IsDerivedFrom(
                    target->RuntimeType(), UIElement::StaticTypeId())) {
                auto& element = static_cast<UIElement&>(*target);
                Base::Ref<Media::Transform3D> existing = element.GetTransform3D();
                if (!existing) {
                    Base::Result<Base::Ref<Media::CompositeTransform3D>> created =
                        Base::MakeRef<Media::CompositeTransform3D>();
                    if (!created) return created.GetStatus();
                    element.SetTransform3D(Base::Ref<Media::Transform3D>(created.Value()));
                    existing = Base::Ref<Media::Transform3D>(created.Value());
                }
                target = existing.Get();
                path = terminalPath;
                nestedTargetResolved = true;
            } else {
                const Meta::DependencyProperty* ownerDependency =
                    FindDependencyProperty(properties, *target, ownerPath);
                if (ownerDependency == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Compound object property was not found");
                }
                Base::Result<Meta::PropertyValue> ownerValue =
                    target->GetValue(ownerDependency->Handle());
                if (!ownerValue ||
                    ownerValue.Value().Kind() != Meta::ValueKind::Object ||
                    !ownerValue.Value().AsObject() ||
                    !properties.Types().IsDerivedFrom(
                        ownerValue.Value().AsObject()->RuntimeType(),
                        DependencyObject::StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Compound object property has no DependencyObject value");
                }
                target = static_cast<DependencyObject*>(ownerValue.Value().AsObject().Get());
                path = terminalPath;
                nestedTargetResolved = true;
            }
        }
    }

    std::uint32_t dot = UINT32_MAX;
    if (!nestedTargetResolved) {
        std::uint32_t parentheses = 0U;
        for (std::uint32_t index = 0U; index < path.SizeBytes(); ++index) {
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
            "Animation target property is empty");
    }

    if (!nestedTargetResolved) {
        std::uint32_t dotCount = 0U;
        for (char character : path) {
            if (character == '.') ++dotCount;
        }
        if (dotCount >= 2U && path[0] != '(') {
            DependencyObject* currentTarget = target;
            std::uint32_t segmentBegin = 0U;
            while (segmentBegin < path.SizeBytes()) {
                std::uint32_t segmentEnd = segmentBegin;
                while (segmentEnd < path.SizeBytes() && path[segmentEnd] != '.') {
                    ++segmentEnd;
                }
                const Base::StringView segment =
                    path.Substr(segmentBegin, segmentEnd - segmentBegin);
                const bool terminal = segmentEnd == path.SizeBytes();
                const Meta::DependencyProperty* segmentProperty =
                    properties.Find(currentTarget->RuntimeType(), segment);
                if (segmentProperty == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Animation object path property was not found");
                }
                if (terminal) {
                    return ResolvedAnimationProperty{
                        currentTarget, segmentProperty->Handle()};
                }
                Base::Result<Meta::PropertyValue> segmentValue =
                    currentTarget->GetValue(segmentProperty->Handle());
                if (!segmentValue ||
                    segmentValue.Value().Kind() != Meta::ValueKind::Object ||
                    segmentValue.Value().IsNullObject() ||
                    !segmentValue.Value().AsObject() ||
                    !properties.Types().IsDerivedFrom(
                        segmentValue.Value().AsObject()->RuntimeType(),
                        DependencyObject::StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Animation object path has no DependencyObject value");
                }
                currentTarget = static_cast<DependencyObject*>(
                    segmentValue.Value().AsObject().Get());
                segmentBegin = segmentEnd + 1U;
            }
        }
    }

    if (dot != UINT32_MAX) {
        Base::StringView ownerProperty = path.Substr(0U, dot);
        Base::StringView nestedProperty = path.Substr(dot + 1U, path.SizeBytes() - dot - 1U);
        ownerProperty = NormalizePropertyPath(ownerProperty);
        nestedProperty = NormalizePropertyPath(nestedProperty);
        const Meta::DependencyProperty* owner =
            FindDependencyProperty(properties, *target, ownerProperty);
        if (owner != nullptr) {
            Base::Result<Meta::PropertyValue> ownerValue =
                target->GetValue(owner->Handle());
            if (ownerValue &&
                ownerValue.Value().Kind() == Meta::ValueKind::Object &&
                ownerValue.Value().AsObject() &&
                properties.Types().IsDerivedFrom(
                    ownerValue.Value().AsObject()->RuntimeType(),
                    DependencyObject::StaticTypeId())) {
                DependencyObject* nestedTarget = static_cast<DependencyObject*>(
                    ownerValue.Value().AsObject().Get());
                const Meta::DependencyProperty* leaf =
                    FindDependencyProperty(properties, *nestedTarget, nestedProperty);
                if (leaf != nullptr) {
                    return ResolvedAnimationProperty{nestedTarget, leaf->Handle()};
                }
            }
        }
    }

    path = NormalizePropertyPath(path);
    const Meta::DependencyProperty* property =
        FindDependencyProperty(properties, *target, path);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Animation target property was not found");
    }
    return ResolvedAnimationProperty{target, property->Handle()};
}

} // namespace Aero
