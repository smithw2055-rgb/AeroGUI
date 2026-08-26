#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

using namespace ::Aero;
namespace MediaAnimation = ::Aero::Media::Animation;

void InteractivityEngine::NotifyLayoutUpdated() noexcept {
    for (auto& behavior : attachedBehaviorInstances) {
        if (behavior.instance) {
            behavior.instance->NotifyLayoutUpdated();
        }
    }
}

Base::Result<Base::Ref<Interactivity::Behavior>>
 InteractivityEngine::CloneBehaviorPrototype(
        const Interactivity::Behavior& prototype) noexcept {
        if (metadata == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Behavior metadata is unavailable");
        }
        Base::Result<Base::Ref<Base::Object>> created =
            metadata->CreateObject(prototype.RuntimeType());
        if (!created) return created.GetStatus();
        if (!created.Value() ||
            !metadata->Types().IsDerivedFrom(
                created.Value()->RuntimeType(),
                Interactivity::Behavior::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Behavior factory returned an incompatible object");
        }
        Base::Ref<Interactivity::Behavior> clone =
            Base::Ref<Interactivity::Behavior>::FromBorrowed(
                *static_cast<Interactivity::Behavior*>(
                    created.Value().Get()));
        for (const Meta::DependencyProperty& property :
             prototype.PropertyRegistry().Properties()) {
            if (property.MetadataFor(prototype.RuntimeType()) == nullptr ||
                property.MetadataFor(clone->RuntimeType()) == nullptr) {
                continue;
            }
            Meta::PropertyValue local =
                prototype.ReadLocalValue(property.Handle());
            if (local.IsUnset()) continue;
            Base::Result<void> copied = clone->SetValueChecked(
                property.Handle(), local);
            if (!copied) return copied.GetStatus();
        }
        Base::Result<void> bindingsCopied =
            prototype.CopyAuthoredBindingsTo(*clone);
        if (!bindingsCopied) return bindingsCopied.GetStatus();
        return clone;
    }

Base::Object* InteractivityEngine::ResolveBehaviorBindingSource(
        const Data::Binding& binding,
        Interactivity::Behavior& behavior,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        if (binding.GetSource()) return binding.GetSource().Get();
        if (!binding.GetElementName().Empty()) {
            Base::Object* source = owner.FindName(
                binding.GetElementName());
            if (source == nullptr && names != nullptr) {
                source = names->Find(binding.GetElementName());
            }
            if (source == nullptr) {
                source = view->loadedDocument.names.Find(
                    binding.GetElementName());
            }
            return source;
        }
        const Base::Ref<Data::RelativeSource> relative =
            binding.GetRelativeSource();
        if (!relative) return nullptr;
        if (relative->GetMode() == Data::RelativeSourceMode::Self) {
            return &behavior;
        }
        if (relative->GetMode() ==
            Data::RelativeSourceMode::TemplatedParent) {
            return owner.GetTemplatedParent();
        }
        if (relative->GetMode() !=
            Data::RelativeSourceMode::FindAncestor) {
            return nullptr;
        }
        Base::StringView ancestorName = relative->GetAncestorType();
        for (std::uint32_t index = 0U;
             index < ancestorName.SizeBytes(); ++index) {
            if (ancestorName[index] == ':') {
                ancestorName = ancestorName.Substr(
                    index + 1U,
                    ancestorName.SizeBytes() - index - 1U);
                break;
            }
        }
        std::uint32_t matched = 0U;
        Aero::Media::Visual* current = Aero::Media::Visual::Of(owner.GetLogicalParent());
        if (current == nullptr) current = owner.GetVisualParent();
        while (current != nullptr) {
            const Meta::TypeInfo* type =
                metadata->Types().FindType(current->RuntimeType());
            const bool matches = ancestorName.Empty() ||
                (type != nullptr && type->Name() == ancestorName);
            if (matches && ++matched == relative->GetAncestorLevel()) {
                return current;
            }
            Aero::Media::Visual* next = Aero::Media::Visual::Of(current->GetLogicalParent());
            if (next == nullptr) next = current->GetVisualParent();
            current = next;
        }
        return nullptr;
    }

Base::Result<void> InteractivityEngine::AttachBehavior(
        const Interactivity::Behavior& prototype,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names,
        bool clonePrototype) noexcept {
        for (const AttachedBehaviorInstance& existing :
             attachedBehaviorInstances) {
            if (existing.target == &owner &&
                existing.prototype == &prototype) {
                return {};
            }
        }
        Base::Ref<Interactivity::Behavior> instance;
        if (clonePrototype) {
            Base::Result<Base::Ref<Interactivity::Behavior>> cloned =
                CloneBehaviorPrototype(prototype);
            if (!cloned) return cloned.GetStatus();
            instance = std::move(cloned).Value();
        } else {
            instance = Base::Ref<Interactivity::Behavior>::TryFromBorrowed(
                const_cast<Interactivity::Behavior&>(prototype));
            if (!instance) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Direct Behavior instance cannot be retained");
            }
        }
        AttachedBehaviorInstance record;
        record.target = &owner;
        record.prototype = &prototype;
        record.instance = std::move(instance);

        for (const Interactivity::Behavior::AuthoredBinding& authored :
             record.instance->GetAuthoredBindings()) {
            if (!authored.binding) continue;
            Base::Object* source = ResolveBehaviorBindingSource(
                *authored.binding, *record.instance, owner, names);
            if ((!authored.binding->GetElementName().Empty() ||
                 authored.binding->GetSource() ||
                 authored.binding->GetRelativeSource()) &&
                source == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Behavior Binding source was not found");
            }
            Data::MetadataBindingDescriptor descriptor;
            descriptor.metadata = metadata;
            descriptor.source = source;
            descriptor.target = record.instance.Get();
            descriptor.targetProperty = authored.property;
            descriptor.dataContextProperty =
                FrameworkElement::DataContextProperty.Handle();
            descriptor.dataContextOwner = &owner;
            descriptor.path = authored.binding->GetPath().GetPath();
            descriptor.stringFormat =
                authored.binding->GetStringFormat();
            descriptor.bindsToSource = descriptor.path.Empty();
            descriptor.mode = authored.binding->GetMode() ==
                    Data::BindingMode::Default
                ? Data::BindingMode::OneWay
                : authored.binding->GetMode();
            descriptor.updateSourceTrigger =
                authored.binding->GetUpdateSourceTrigger() ==
                    Meta::UpdateSourceTrigger::Default
                ? Meta::UpdateSourceTrigger::PropertyChanged
                : authored.binding->GetUpdateSourceTrigger();
            descriptor.fallbackValue =
                authored.binding->GetFallbackValue();
            descriptor.targetNullValue =
                authored.binding->GetTargetNullValue();
            Base::Result<Data::BindingHandle> attached =
                bindings->Attach(descriptor);
            if (!attached) {
                for (const Data::BindingHandle handle : record.bindings) {
                    static_cast<void>(bindings->Detach(handle));
                }
                return attached.GetStatus();
            }
            Base::Result<void> retained = record.bindings.PushBack(
                attached.Value());
            if (!retained) {
                static_cast<void>(bindings->Detach(attached.Value()));
                for (const Data::BindingHandle handle : record.bindings) {
                    static_cast<void>(bindings->Detach(handle));
                }
                return retained.GetStatus();
            }
        }
        Base::Result<void> attached = record.instance->Attach(owner);
        if (!attached) {
            for (const Data::BindingHandle handle : record.bindings) {
                static_cast<void>(bindings->Detach(handle));
            }
            return attached.GetStatus();
        }
        Base::Result<void> retained = attachedBehaviorInstances.PushBack(
            std::move(record));
        if (!retained) {
            record.instance->Detach();
            for (const Data::BindingHandle handle : record.bindings) {
                static_cast<void>(bindings->Detach(handle));
            }
            return retained.GetStatus();
        }
        return {};
    }

void InteractivityEngine::DetachBehaviorsInSubtree(Aero::Media::Visual& visual) noexcept {
        for (std::uint32_t index = 0U;
             index < attachedBehaviorInstances.Size();) {
            AttachedBehaviorInstance& record =
                attachedBehaviorInstances[index];
            if (record.target == nullptr ||
                !IsInVisualSubtree(record.target, visual)) {
                ++index;
                continue;
            }
            for (const Data::BindingHandle handle : record.bindings) {
                if (bindings != nullptr) {
                    static_cast<void>(bindings->Detach(handle));
                }
            }
            if (record.instance) record.instance->Detach();
            if (index + 1U != attachedBehaviorInstances.Size()) {
                attachedBehaviorInstances[index] =
                    std::move(attachedBehaviorInstances.Back());
            }
            attachedBehaviorInstances.PopBack();
        }
    }

} // namespace Aero
