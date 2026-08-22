#pragma once

#include <Aero/Data/Binding.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>

namespace Aero::Interactivity {

class AERO_GUI_API LaunchUriOrFileAction : public TriggerAction {
    AERO_DECLARE_TYPE(LaunchUriOrFileAction, TriggerAction)
public:
    LaunchUriOrFileAction() noexcept : TriggerAction(StaticTypeId()) {}
    StringView GetPath() const noexcept { return path_.View(); }
    void SetPath(StringView value) noexcept;
    Ref<Aero::Data::Binding> GetPathBinding() const noexcept {
        return pathBinding_;
    }
    void SetPathBinding(Ref<Aero::Data::Binding> value) noexcept {
        pathBinding_ = std::move(value);
    }

private:
    String path_;
    Ref<Aero::Data::Binding> pathBinding_;
};

} // namespace Aero::Interactivity
