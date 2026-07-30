#pragma once

#include <Aero/Detail/RuntimeManagersFwd.hpp>

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Presentation/Binding.hpp>

namespace Aero::Presentation {
}

namespace Aero::Markup {

struct DeferredContentEdge final {
    Base::Object* owner = nullptr;
    Base::Object* parent = nullptr;
    Base::Ref<Base::Object> child;
    Core::MetadataRuntime* runtime = nullptr;
    Core::MemberId member = Core::InvalidMemberId;
    bool property = false;
};

struct DeferredBindingEdge final {
    Base::Object* owner = nullptr;
    Base::Object* source = nullptr;
    Core::DependencyObject* target = nullptr;
    Presentation::BindingManager* manager = nullptr;
    Core::MetadataRuntime* metadata = nullptr;
    Core::DependencyPropertyHandle targetProperty;
    Core::DependencyPropertyHandle dataContextProperty;
    Base::String path;
    Base::String stringFormat;
    bool bindsToSource = false;
    Presentation::BindingMode mode =
        Presentation::BindingMode::OneWay;
    Core::UpdateSourceTrigger updateSourceTrigger =
        Core::UpdateSourceTrigger::PropertyChanged;
};

class DeferredContentPlan final {
public:
    Base::Result<void> Stage(
        Base::Object& owner,
        Base::Object& parent,
        const Base::Ref<Base::Object>& child,
        Core::MetadataRuntime& runtime,
        Core::MemberId member) noexcept;
    Base::Result<void> StageProperty(
        Base::Object& owner,
        Base::Object& parent,
        const Base::Ref<Base::Object>& child,
        Core::MetadataRuntime& runtime,
        Core::MemberId member) noexcept;
    Base::Result<void> CopyForOwner(
        const Base::Object& owner,
        Base::Vector<DeferredContentEdge>& output) const noexcept;
    Base::Result<void> StageBinding(
        Base::Object& owner,
        Base::Object* source,
        Core::DependencyObject& target,
        Presentation::BindingManager& manager,
        Core::MetadataRuntime& metadata,
        Core::DependencyPropertyHandle targetProperty,
        Core::DependencyPropertyHandle dataContextProperty,
        Base::StringView path,
        Base::StringView stringFormat,
        Presentation::BindingMode mode,
        Core::UpdateSourceTrigger updateSourceTrigger,
        bool bindsToSource) noexcept;
    Base::Result<void> CopyBindingsForOwner(
        const Base::Object& owner,
        Base::Vector<DeferredBindingEdge>& output) const noexcept;
    void ReleaseOwner(Base::Object& owner) noexcept;
    void ReleaseAll() noexcept;
    bool Empty() const noexcept {
        return edges_.Empty() && bindings_.Empty();
    }

private:
    Base::Vector<DeferredContentEdge> edges_;
    Base::Vector<DeferredBindingEdge> bindings_;
};

} // namespace Aero::Markup
