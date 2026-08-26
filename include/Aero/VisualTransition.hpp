#pragma once

#include <Aero/Base/Config.hpp>
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
    Result<void> SetFrom(StringView value) noexcept {
        return from_.Assign(value);
    }
    Result<void> SetTo(StringView value) noexcept {
        return to_.Assign(value);
    }
    Result<void> SetGeneratedDuration(
        StringView value) noexcept {
        Media::Animation::Storyboard validator;
        Result<void> valid =
            validator.SetDurationChecked(value);
        if (!valid) return valid.GetStatus();
        return generatedDuration_.Assign(value);
    }
    Ref<Media::Animation::EasingFunctionBase>
    GetGeneratedEasingFunction() const noexcept {
        return generatedEasingFunction_;
    }
    Result<void> SetGeneratedEasingFunction(
        Ref<Media::Animation::EasingFunctionBase> value) noexcept {
        generatedEasingFunction_ = std::move(value);
        return {};
    }
    const Ref<Media::Animation::Storyboard>&
    GetStoryboard() const noexcept {
        return storyboard_;
    }
    Result<void> SetStoryboard(
        Ref<Media::Animation::Storyboard> value) noexcept {
        if (storyboard_ && value) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "VisualTransition accepts only one Storyboard");
        }
        storyboard_ = std::move(value);
        return {};
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
