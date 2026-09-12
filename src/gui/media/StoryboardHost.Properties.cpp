#include "gui/ViewFrame.hpp"
#include "gui/media/StoryboardHost.hpp"
#include "gui/media/AnimationPathResolver.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/core/EventRouter.hpp"
#include <Aero/CommandBinding.hpp>
#include <Aero/Media/Animation/EventTrigger.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>

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
