#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Media/Animation.hpp>

namespace Aero {

class AERO_GUI_API VisualTransition : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        VisualTransition, Base::Object, "urn:aero", "VisualTransition")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    StringView GetFrom() const noexcept { return from_.View(); }
    StringView GetTo() const noexcept { return to_.View(); }
    StringView GetGeneratedDuration() const noexcept {
        return generatedDuration_.View();
    }
    void SetFrom(StringView value) noexcept {
        Base::Result<void> assigned = from_.Assign(value);
        if (!assigned) { AERO_ASSERT(false); return; }
    }
    void SetTo(StringView value) noexcept {
        Base::Result<void> assigned = to_.Assign(value);
        if (!assigned) { AERO_ASSERT(false); return; }
    }
    void SetGeneratedDuration(StringView value) noexcept {
        Result<Media::Animation::Duration> valid =
            Media::Animation::Duration::TryParse(value);
        if (!valid) { AERO_ASSERT(false); return; }
        Base::Result<void> assigned = generatedDuration_.Assign(value);
        if (!assigned) { AERO_ASSERT(false); return; }
    }
    Ref<Media::Animation::EasingFunctionBase>
    GetGeneratedEasingFunction() const noexcept {
        return generatedEasingFunction_;
    }
    void SetGeneratedEasingFunction(Ref<Media::Animation::EasingFunctionBase> value) noexcept {
        generatedEasingFunction_ = std::move(value);
    }
    const Ref<Media::Animation::Storyboard>&
    GetStoryboard() const noexcept {
        return storyboard_;
    }
    void SetStoryboard(Ref<Media::Animation::Storyboard> value) noexcept {
        if (storyboard_ && value) { AERO_ASSERT(false); return; }
        storyboard_ = std::move(value);
    }

private:
    String from_;
    String to_;
    String generatedDuration_;
    Ref<Media::Animation::EasingFunctionBase>
        generatedEasingFunction_;
    Ref<Media::Animation::Storyboard> storyboard_;
};

} // namespace Aero
