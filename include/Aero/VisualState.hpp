#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Media/Animation.hpp>

namespace Aero {

class AERO_GUI_API VisualState : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        VisualState, Base::Object, "urn:aero", "VisualState")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    StringView GetName() const noexcept { return name_.View(); }
    Result<void> SetName(StringView value) noexcept {
        return name_.Assign(value);
    }
    Span<const Ref<Base::Object>> GetSetters() const noexcept {
        return {setters_.Data(), setters_.Size()};
    }
    Result<void> AddSetter(
        Ref<Base::Object> value) noexcept {
        return setters_.PushBack(std::move(value));
    }
    void ClearSetters() noexcept { setters_.Clear(); }
    const Ref<Media::Animation::Storyboard>&
    GetStoryboard() const noexcept {
        return storyboard_;
    }
    Result<void> SetStoryboard(
        Ref<Media::Animation::Storyboard> value) noexcept {
        if (storyboard_ && value) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "VisualState accepts only one Storyboard");
        }
        storyboard_ = std::move(value);
        return {};
    }

private:
    String name_;
    Base::Vector<Ref<Base::Object>> setters_;
    Ref<Media::Animation::Storyboard> storyboard_;
};

} // namespace Aero
