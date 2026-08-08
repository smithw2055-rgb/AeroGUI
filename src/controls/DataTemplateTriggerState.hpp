#pragma once

#include <Aero/Base/Ref.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Gui/DependencyProperty.hpp>
#include <Aero/Gui/ControlTemplate.hpp>

#include <cstdint>

namespace Aero::Controls {

struct DataTemplateTriggerSetter {
    Base::WeakRef<::Aero::DependencyObject> target;
    Meta::DependencyPropertyHandle property;
    Meta::PropertyValue value;
    // A default Binding in a ControlTemplate observes the templated
    // parent's current DataContext. Preserve that semantic until the
    // instance is mounted instead of capturing the authoring-time object.
    bool usesDataContext = false;
    Meta::PropertyProviderToken token;
};

struct DataTemplateTriggerCondition {
    // Property triggers are compiled directly to a DependencyProperty
    // condition. Data triggers retain their Binding so the runtime can
    // resolve ElementName against the document name scope and subscribe to
    // the resulting source property.
    Base::WeakRef<Base::Object> source;
    Base::Ref<Data::Binding> binding;
    Base::WeakRef<::Aero::DependencyObject> dependencySource;
    Meta::DependencyPropertyHandle property;
    Meta::PropertyValue value;
    // A default Binding in a ControlTemplate observes the templated
    // parent's current DataContext. Preserve that semantic until the
    // instance is mounted instead of capturing the authoring-time object.
    bool usesDataContext = false;
};

struct DataTemplatePropertyTrigger {
    Base::Vector<DataTemplateTriggerCondition> conditions;
    Base::Vector<DataTemplateTriggerSetter> setters;
    Base::Vector<Base::Ref<Base::Object>> enterActions;
    Base::Vector<Base::Ref<Base::Object>> exitActions;
    bool active = false;
};

class DataTemplateTriggerState
    : public Base::Object {
public:
    struct NamedObject {
        Base::String name;
        Base::WeakRef<Base::Object> object;
    };

    static constexpr Meta::TypeId StaticTypeId() noexcept {
        return Meta::MakeTypeId(
            "urn:aero-internal",
            "DataTemplateTriggerState");
    }

    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::Object* FindName(
        Base::StringView name) const noexcept {
        for (const NamedObject& named : names) {
            if (named.name.View() == name) {
                Base::Ref<Base::Object> object = named.object.Lock();
                return object.Get();
            }
        }
        return nullptr;
    }

    Aero::FrameworkElement* root = nullptr;
    std::uint32_t providerOrigin = 0U;
    Base::Vector<NamedObject> names;
    Base::Vector<DataTemplatePropertyTrigger> triggers;
};

} // namespace Aero::Controls
