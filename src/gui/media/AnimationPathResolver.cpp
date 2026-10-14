#include <cstdio>
#include <algorithm>
#include <cstdint>
#include "gui/media/AnimationEngine.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/DependencyPropertyRegistry.hpp"

#include <Aero/DependencyProperty.hpp>
#include <Aero/DependencyObject.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Value.hpp>
#include <Aero/PropertyPath.hpp>
#include <Aero/TryCast.hpp>

namespace Aero {

namespace {

const Meta::DependencyProperty* FindPropertyForToken(
    Meta::DependencyPropertyRegistry& properties,
    DependencyObject& object,
    const PropertyPathToken& token) noexcept {
    if (token.isTypeQualified && !token.typeQualifier.Empty()) {
        for (const Meta::TypeInfo& type : properties.Types().Types()) {
            if (type.Name() != token.typeQualifier) continue;
            const Meta::DependencyProperty* prop =
                properties.Find(type.Id(), token.propertyName);
            if (prop != nullptr &&
                (prop->IsAttached() ||
                 properties.Types().IsDerivedFrom(object.RuntimeType(), type.Id()))) {
                return prop;
            }
        }
    }
    // Also check if propertyName has an unparenthesized type qualification (e.g. Type.Prop)
    std::uint32_t dot = UINT32_MAX;
    for (std::uint32_t i = 0U; i < token.propertyName.SizeBytes(); ++i) {
        if (token.propertyName[i] == '.') dot = i;
    }
    if (dot != UINT32_MAX) {
        Base::StringView ownerName = token.propertyName.Substr(0U, dot);
        Base::StringView propName = token.propertyName.Substr(dot + 1U, token.propertyName.SizeBytes() - dot - 1U);
        for (std::uint32_t i = 0U; i < ownerName.SizeBytes(); ++i) {
            if (ownerName[i] == ':') {
                ownerName = ownerName.Substr(i + 1U, ownerName.SizeBytes() - i - 1U);
            }
        }
        for (const Meta::TypeInfo& type : properties.Types().Types()) {
            if (type.Name() != ownerName) continue;
            const Meta::DependencyProperty* prop =
                properties.Find(type.Id(), propName);
            if (prop != nullptr &&
                (prop->IsAttached() ||
                 properties.Types().IsDerivedFrom(object.RuntimeType(), type.Id()))) {
                return prop;
            }
        }
        return properties.Find(object.RuntimeType(), propName);
    }
    return properties.Find(object.RuntimeType(), token.propertyName);
}

Base::Result<ResolvedAnimationProperty> ResolvePathGeometryAnimationProperty(
    ::Aero::DependencyObject& target,
    Base::StringView authoredPath,
    Meta::DependencyPropertyRegistry& properties) noexcept {
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
    if (findText(authoredPath, figuresToken) == UINT32_MAX ||
        findText(authoredPath, segmentsToken) == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Path is not a PathGeometry indexed property");
    }
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
                static_cast<std::uint32_t>(authoredPath[cursor] - '0');
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
        properties.Find(target.RuntimeType(), "Data");
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

    const Base::StringView figuresToken("PathGeometry.Figures");
    const Base::StringView segmentsToken("PathFigure.Segments");
    auto containsToken = [](Base::StringView text, Base::StringView token) noexcept {
        for (std::uint32_t i = 0U; i + token.SizeBytes() <= text.SizeBytes(); ++i) {
            if (text.Substr(i, token.SizeBytes()) == token) return true;
        }
        return false;
    };
    if (containsToken(authoredPath, figuresToken) &&
        containsToken(authoredPath, segmentsToken)) {
        return ResolvePathGeometryAnimationProperty(rootTarget, authoredPath, properties);
    }

    Base::Vector<PropertyPathToken> tokens;
    Base::Result<void> tokenized = PropertyPath::ParseTokens(authoredPath, tokens);
    if (!tokenized) return tokenized.GetStatus();
    if (tokens.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Animation target property is required");
    }

    DependencyObject* target = &rootTarget;

    for (std::uint32_t i = 0U; i < tokens.Size(); ++i) {
        const PropertyPathToken& token = tokens[i];
        const bool isLeaf = (i + 1U == tokens.Size());

        // Transform3D owner special handling
        if (token.propertyName == Base::StringView("Transform3D") ||
            token.typeQualifier == Base::StringView("Transform3D")) {
            if (properties.Types().IsDerivedFrom(target->RuntimeType(), UIElement::StaticTypeId())) {
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
                if (!isLeaf) continue;
            }
        }
        // TransformGroup.Children special handling
        if ((token.propertyName == Base::StringView("Children") ||
             token.typeQualifier == Base::StringView("TransformGroup")) &&
            properties.Types().IsDerivedFrom(target->RuntimeType(), Media::TransformGroup::StaticTypeId())) {
            auto& group = static_cast<Media::TransformGroup&>(*target);
            const auto children = group.GetChildren();
            if (token.hasIndex) {
                if (token.collectionIndex >= children.Size() || !children[token.collectionIndex]) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "TransformGroup index is out of range");
                }
                target = children[token.collectionIndex].Get();
                if (!isLeaf) continue;
            }
        }

        const Meta::DependencyProperty* property =
            FindPropertyForToken(properties, *target, token);
        if (property == nullptr) {
            std::fprintf(stderr, "[AnimationPathResolver] property not found: token='%.*s' targetType=%llu\n",
                static_cast<int>(token.raw.SizeBytes()), token.raw.Data(),
                static_cast<unsigned long long>(target->RuntimeType()));
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Animation target property was not found");
        }

        if (isLeaf && !token.hasIndex) {
            return ResolvedAnimationProperty{target, property->Handle()};
        }

        // Must step through intermediate property or indexed property
        Base::Result<Meta::PropertyValue> value = target->GetValue(property->Handle());
        if (!value) return value.GetStatus();

        if (token.hasIndex) {
            const bool isGradientStops =
                token.propertyName == Base::StringView("GradientStops") ||
                (value.Value().Kind() == Meta::ValueKind::Object &&
                 value.Value().AsObject() &&
                 properties.Types().IsDerivedFrom(value.Value().AsObject()->RuntimeType(), Media::GradientBrush::StaticTypeId()));

            const bool isTransformChildren =
                token.propertyName == Base::StringView("Children") ||
                token.propertyName == Base::StringView("RenderTransform") ||
                token.propertyName == Base::StringView("LayoutTransform");

            if (isGradientStops) {
                Media::GradientBrush* brush = nullptr;
                if (properties.Types().IsDerivedFrom(target->RuntimeType(), Media::GradientBrush::StaticTypeId())) {
                    brush = static_cast<Media::GradientBrush*>(target);
                } else if (value.Value().Kind() == Meta::ValueKind::Object && value.Value().AsObject() &&
                           properties.Types().IsDerivedFrom(value.Value().AsObject()->RuntimeType(), Media::GradientBrush::StaticTypeId())) {
                    brush = static_cast<Media::GradientBrush*>(value.Value().AsObject().Get());
                }
                if (brush == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Target property is not a GradientBrush");
                }
                const auto stops = brush->GetGradientStops();
                if (token.collectionIndex >= stops.Size() || !stops[token.collectionIndex]) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "GradientStops index is out of range");
                }
                target = stops[token.collectionIndex].Get();
            } else if (isTransformChildren) {
                Base::Ref<Media::Transform> transform;
                if (properties.Types().IsDerivedFrom(target->RuntimeType(), Media::TransformGroup::StaticTypeId())) {
                    transform = Base::Ref<Media::Transform>::TryFromBorrowed(
                        static_cast<Media::Transform&>(*target));
                } else {
                    const bool layoutPath = token.propertyName == Base::StringView("LayoutTransform") ||
                        token.raw.Substr(0U, std::min<std::uint32_t>(token.raw.SizeBytes(), 15U)) == Base::StringView("LayoutTransform");
                    if (layoutPath) {
                        if (!properties.Types().IsDerivedFrom(target->RuntimeType(), FrameworkElement::StaticTypeId())) {
                            return Base::Status::Failure(
                                Base::ErrorCode::InvalidArgument,
                                "LayoutTransform target is not a FrameworkElement");
                        }
                        transform = static_cast<FrameworkElement&>(*target).GetLayoutTransform();
                    } else {
                        if (!properties.Types().IsDerivedFrom(target->RuntimeType(), UIElement::StaticTypeId())) {
                            return Base::Status::Failure(
                                Base::ErrorCode::InvalidArgument,
                                "RenderTransform target is not a UIElement");
                        }
                        transform = static_cast<UIElement&>(*target).GetRenderTransform();
                    }
                }
                if (!transform || !properties.Types().IsDerivedFrom(transform->RuntimeType(), Media::TransformGroup::StaticTypeId())) {
                    if (properties.Types().IsDerivedFrom(target->RuntimeType(), UIElement::StaticTypeId())) {
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
                            const bool layoutPath = token.propertyName == Base::StringView("LayoutTransform");
                            if (layoutPath && properties.Types().IsDerivedFrom(target->RuntimeType(), FrameworkElement::StaticTypeId())) {
                                static_cast<FrameworkElement&>(element).SetLayoutTransform(Base::Ref<Media::Transform>(group.Value()));
                            } else {
                                element.SetRenderTransform(Base::Ref<Media::Transform>(group.Value()));
                            }
                            transform = Base::Ref<Media::Transform>(group.Value());
                        }
                    }
                }
                if (!transform || !properties.Types().IsDerivedFrom(transform->RuntimeType(), Media::TransformGroup::StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Transform path has no TransformGroup");
                }
                auto& group = static_cast<Media::TransformGroup&>(*transform);
                const auto children = group.GetChildren();
                if (token.collectionIndex >= children.Size() || !children[token.collectionIndex]) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "TransformGroup index is out of range");
                }
                target = children[token.collectionIndex].Get();
            } else {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Indexed animation collection is not supported");
            }
        } else {
            // Non-indexed intermediate object step
            if (value.Value().Kind() != Meta::ValueKind::Object ||
                !value.Value().AsObject() ||
                !properties.Types().IsDerivedFrom(value.Value().AsObject()->RuntimeType(), DependencyObject::StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Animation object path has no DependencyObject value");
            }
            target = static_cast<DependencyObject*>(value.Value().AsObject().Get());
        }
    }

    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "Animation path cannot terminate on index without property");
}

} // namespace Aero
