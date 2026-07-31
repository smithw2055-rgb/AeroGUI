#pragma once

#include <Aero/Controls/Items.hpp>

namespace Aero::Controls::Detail {

using DeferredObjectFactory = Base::Result<Base::Ref<Base::Object>> (*)(const Base::Ref<Base::Object>& item, void* context) noexcept;

struct DeferredObjectProgram final {
    Base::Result<void> Configure(DeferredObjectFactory factory, void* context = nullptr) noexcept;
    Base::Result<void> Configure(DeferredObjectFactory factory, void* context, Base::Ref<Base::Object> factoryOwner) noexcept;
    Base::Result<void> SetBaseUri(const Base::ResourceUri& value) noexcept;
    Base::Result<void> Seal() noexcept;
    Base::Result<Base::Ref<Base::Object>> Instantiate(const Base::Ref<Base::Object>& payload = {}) const noexcept;

    DeferredObjectFactory factory = nullptr;
    void* context = nullptr;
    Base::Ref<Base::Object> factoryOwner;
    Base::ResourceUri baseUri;
    bool sealed = false;
};

struct DataTemplateState final {
    DeferredObjectProgram program;
    TypeId dataType = InvalidTypeId;
    Base::Ref<Base::Object> hierarchicalItemsSource;
    Base::Ref<Base::Object> hierarchicalItemTemplate;
    ResourceDictionary resources;
    Base::Ref<Base::Object> authoredVisualTree;
    Base::Vector<Base::Ref<Aero::TriggerBase>> authoredTriggers;
    Aero::NameScope authoredNames;
};

struct ItemsPanelTemplateState final {
    DeferredObjectProgram program;
    ResourceDictionary resources;
    Base::Ref<Base::Object> authoredVisualTree;
};

} // namespace Aero::Controls::Detail
