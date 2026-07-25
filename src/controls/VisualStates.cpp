#include <Aero/Controls/Templates.hpp>

#include <utility>

namespace Aero::Controls {

std::uint32_t VisualStateManager::FindActive(
    TemplateHandle handle,
    Base::StringView groupName) const noexcept {
    for (std::uint32_t index = 0U;
        index < active_.Size(); ++index) {
        if (active_[index].templateValue == handle.value &&
            active_[index].groupName.View() == groupName) {
            return index;
        }
    }
    return UINT32_MAX;
}

const VisualStateGroup* VisualStateManager::FindGroup(
    const ControlTemplate& plan,
    Base::StringView groupName) noexcept {
    for (const VisualStateGroup& group :
        plan.VisualStateGroups()) {
        if (group.name.View() == groupName) return &group;
    }
    return nullptr;
}

const VisualState* VisualStateManager::FindState(
    const VisualStateGroup& group,
    Base::StringView stateName) noexcept {
    for (const VisualState& state : group.states) {
        if (state.name.View() == stateName) return &state;
    }
    return nullptr;
}

Base::Result<void> VisualStateManager::ApplyState(
    TemplateHandle handle,
    const VisualState& state) noexcept {
    for (const VisualStateSetter& setter : state.setters) {
        DependencyObject* target =
            templates_->FindName(handle, setter.targetName.View());
        if (target == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Visual state target name was not found");
        }
    }
    std::uint32_t appliedCount = 0U;
    for (const VisualStateSetter& setter : state.setters) {
        DependencyObject* target =
            templates_->FindName(handle, setter.targetName.View());
        Base::Result<void> applied = values_->SetAnimationValue(
            *target, setter.property, setter.value);
        if (!applied) {
            for (std::uint32_t index = 0U;
                index < appliedCount; ++index) {
                const VisualStateSetter& rollback =
                    state.setters[index];
                DependencyObject* rollbackTarget =
                    templates_->FindName(
                        handle, rollback.targetName.View());
                if (rollbackTarget != nullptr) {
                    static_cast<void>(
                        values_->ClearAnimationValue(
                            *rollbackTarget, rollback.property));
                }
            }
            return applied.GetStatus();
        }
        ++appliedCount;
    }
    return {};
}

Base::Result<void> VisualStateManager::ClearStateValues(
    TemplateHandle handle,
    const VisualState& state) noexcept {
    for (const VisualStateSetter& setter : state.setters) {
        DependencyObject* target =
            templates_->FindName(handle, setter.targetName.View());
        if (target == nullptr) continue;
        Base::Result<void> cleared =
            values_->ClearAnimationValue(
                *target, setter.property);
        if (!cleared) return cleared.GetStatus();
    }
    return {};
}

void VisualStateManager::RemoveActiveAt(
    std::uint32_t index) noexcept {
    if (index + 1U != active_.Size()) {
        active_[index] =
            std::move(active_[active_.Size() - 1U]);
    }
    active_.PopBack();
}

void VisualStateManager::PruneStale() noexcept {
    for (std::uint32_t index = active_.Size();
        index > 0U; --index) {
        TemplateHandle handle;
        handle.value = active_[index - 1U].templateValue;
        if (templates_->AppliedTemplate(handle) == nullptr) {
            RemoveActiveAt(index - 1U);
        }
    }
}

Base::Result<bool> VisualStateManager::GoToState(
    Control& control,
    Base::StringView groupName,
    Base::StringView stateName) noexcept {
    if (groupName.Empty() || stateName.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Visual state group and state names are required");
    }
    PruneStale();
    const TemplateHandle handle =
        templates_->AppliedHandle(control);
    const ControlTemplate* plan =
        templates_->AppliedTemplate(handle);
    if (!handle.IsValid() || plan == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Control does not have an applied template");
    }
    const VisualStateGroup* group =
        FindGroup(*plan, groupName);
    const VisualState* next =
        group != nullptr ? FindState(*group, stateName) : nullptr;
    if (next == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Visual state was not found");
    }

    std::uint32_t activeIndex = FindActive(handle, groupName);
    if (activeIndex != UINT32_MAX &&
        active_[activeIndex].stateName.View() == stateName) {
        return false;
    }
    Base::String nextGroup;
    Base::Result<void> groupAssigned =
        nextGroup.TryAssign(groupName);
    if (!groupAssigned) return groupAssigned.GetStatus();
    Base::String nextName;
    Base::Result<void> nameAssigned =
        nextName.TryAssign(stateName);
    if (!nameAssigned) return nameAssigned.GetStatus();
    bool addedRecord = false;
    if (activeIndex == UINT32_MAX) {
        ActiveGroup active;
        active.templateValue = handle.value;
        active.groupName = std::move(nextGroup);
        Base::Result<void> appended =
            active_.TryPushBack(std::move(active));
        if (!appended) return appended.GetStatus();
        activeIndex = active_.Size() - 1U;
        addedRecord = true;
    }

    const VisualState* previous = nullptr;
    if (!active_[activeIndex].stateName.Empty()) {
        previous = FindState(
            *group, active_[activeIndex].stateName.View());
    }
    if (previous != nullptr) {
        Base::Result<void> cleared =
            ClearStateValues(handle, *previous);
        if (!cleared) {
            if (addedRecord) RemoveActiveAt(activeIndex);
            return cleared.GetStatus();
        }
    }
    Base::Result<void> applied = ApplyState(handle, *next);
    if (!applied) {
        if (previous != nullptr) {
            static_cast<void>(ApplyState(handle, *previous));
        }
        if (addedRecord) RemoveActiveAt(activeIndex);
        return applied.GetStatus();
    }
    active_[activeIndex].stateName = std::move(nextName);
    return true;
}

Base::Result<bool> VisualStateManager::ClearState(
    Control& control,
    Base::StringView groupName) noexcept {
    PruneStale();
    const TemplateHandle handle =
        templates_->AppliedHandle(control);
    if (!handle.IsValid()) return false;
    const std::uint32_t activeIndex =
        FindActive(handle, groupName);
    if (activeIndex == UINT32_MAX) return false;
    const ControlTemplate* plan =
        templates_->AppliedTemplate(handle);
    const VisualStateGroup* group = plan != nullptr
        ? FindGroup(*plan, groupName) : nullptr;
    const VisualState* state = group != nullptr
        ? FindState(*group, active_[activeIndex].stateName.View())
        : nullptr;
    if (state != nullptr) {
        Base::Result<void> cleared =
            ClearStateValues(handle, *state);
        if (!cleared) return cleared.GetStatus();
    }
    RemoveActiveAt(activeIndex);
    return true;
}

Base::Result<std::uint32_t> VisualStateManager::Clear(
    Control& control) noexcept {
    PruneStale();
    const TemplateHandle handle =
        templates_->AppliedHandle(control);
    if (!handle.IsValid()) return 0U;
    std::uint32_t clearedCount = 0U;
    for (std::uint32_t index = active_.Size();
        index > 0U; --index) {
        if (active_[index - 1U].templateValue != handle.value) {
            continue;
        }
        Base::Result<bool> cleared = ClearState(
            control, active_[index - 1U].groupName.View());
        if (!cleared) return cleared.GetStatus();
        if (cleared.Value()) ++clearedCount;
    }
    return clearedCount;
}

Base::StringView VisualStateManager::CurrentState(
    const Control& control,
    Base::StringView groupName) const noexcept {
    const TemplateHandle handle =
        templates_->AppliedHandle(control);
    if (!handle.IsValid()) return {};
    const std::uint32_t index =
        FindActive(handle, groupName);
    return index != UINT32_MAX
        ? active_[index].stateName.View()
        : Base::StringView{};
}

} // namespace Aero::Controls
