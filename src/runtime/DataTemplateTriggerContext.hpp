#pragma once

#include <Aero/Base/Ref.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Style.hpp>

#include <cstdint>

namespace Aero::Detail {

struct DataTemplateTriggerSetter final {
    Base::Ref<Core::DependencyObject> target;
    Core::DependencyPropertyHandle property;
    Core::PropertyValue value;
    Core::PropertyProviderToken token;
};

struct DataTemplateTriggerCondition final {
    // Property triggers are compiled directly to a DependencyProperty
    // condition. Data triggers retain their Binding so the runtime can
    // resolve ElementName against the document name scope and subscribe to
    // the resulting source property.
    Base::Ref<Base::Object> source;
    Base::Ref<Data::Binding> binding;
    Base::Ref<Core::DependencyObject> dependencySource;
    Core::DependencyPropertyHandle property;
    Core::PropertyValue value;
};

struct DataTemplatePropertyTrigger final {
    Base::Vector<DataTemplateTriggerCondition> conditions;
    Base::Vector<DataTemplateTriggerSetter> setters;
    Base::Vector<Base::Ref<Base::Object>> enterActions;
    Base::Vector<Base::Ref<Base::Object>> exitActions;
    bool active = false;
};

class DataTemplateTriggerContext final
    : public Base::Object {
public:
    struct NamedObject final {
        Base::String name;
        Base::Ref<Base::Object> object;
    };

    static constexpr Core::TypeId StaticTypeId() noexcept {
        return Core::MakeTypeId(
            "urn:aero-internal",
            "DataTemplateTriggerContext");
    }

    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::Object* FindName(
        Base::StringView name) const noexcept {
        for (const NamedObject& named : names) {
            if (named.name.View() == name) {
                return named.object.Get();
            }
        }
        return nullptr;
    }

    Aero::FrameworkElement* root = nullptr;
    std::uint32_t providerOrigin = 0U;
    Base::Vector<NamedObject> names;
    Base::Vector<DataTemplatePropertyTrigger> triggers;
};

} // namespace Aero::Detail
