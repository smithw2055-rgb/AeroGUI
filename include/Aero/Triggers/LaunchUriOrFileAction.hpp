#pragma once

#include <Aero/Triggers/TriggerAction.hpp>

namespace Aero::Media::Animation {

class AERO_API LaunchUriOrFileAction : public TriggerAction {
    AERO_DECLARE_TYPE(LaunchUriOrFileAction, TriggerAction)
public:
    LaunchUriOrFileAction() noexcept : TriggerAction(StaticTypeId()) {}
    Base::StringView GetPath() const noexcept { return path_.View(); }
    void SetPath(Base::StringView value) noexcept;
    Base::Ref<Aero::Data::Binding> GetPathBinding() const noexcept {
        return pathBinding_;
    }
    void SetPathBinding(Base::Ref<Aero::Data::Binding> value) noexcept {
        pathBinding_ = std::move(value);
    }

private:
    Base::String path_;
    Base::Ref<Aero::Data::Binding> pathBinding_;
};

} // namespace Aero::Media::Animation
