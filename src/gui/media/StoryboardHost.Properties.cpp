#include "gui/ViewFrame.hpp"
#include "gui/media/StoryboardHost.hpp"
#include "gui/media/AnimationPathResolver.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/core/EventRouter.hpp"
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
                (*Metadata()).DependencyProperties().Find(
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

        Base::Result<Aero::ResolvedAnimationProperty> resolved =
            ResolveAnimationPropertyPath(
                target, authoredPath, (*Metadata()).DependencyProperties());
        if (!resolved) {
            return resolved.GetStatus();
        }
        return ResolvedAnimationProperty{
            resolved.Value().target, resolved.Value().property};
    }

} // namespace Aero
