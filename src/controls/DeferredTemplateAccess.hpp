#pragma once

#include "DeferredTemplateRuntime.hpp"

namespace Aero::Controls::Detail {

class DeferredTemplateAccess final {
public:
    static DataTemplateState* State(DataTemplate& value) noexcept;
    static const DataTemplateState* State(const DataTemplate& value) noexcept;
    static ItemsPanelTemplateState* State(ItemsPanelTemplate& value) noexcept;
    static const ItemsPanelTemplateState* State(const ItemsPanelTemplate& value) noexcept;

    static Base::Result<void> Configure(DataTemplate& value, DeferredObjectFactory factory, void* context = nullptr, Base::Ref<Base::Object> owner = {}) noexcept;
    static Base::Result<void> Configure(ItemsPanelTemplate& value, DeferredObjectFactory factory, void* context = nullptr, Base::Ref<Base::Object> owner = {}) noexcept;
    static Base::Result<void> SetBaseUri(DataTemplate& value, const Base::ResourceUri& uri) noexcept;
    static Base::Result<void> SetBaseUri(ItemsPanelTemplate& value, const Base::ResourceUri& uri) noexcept;
    static const Base::ResourceUri& BaseUri(const DataTemplate& value) noexcept;
    static const Base::ResourceUri& BaseUri(const ItemsPanelTemplate& value) noexcept;
    static Base::Result<void> SetAuthoredVisualTree(DataTemplate& value, const Base::Ref<Base::Object>& tree) noexcept;
    static Base::Result<void> SetAuthoredVisualTree(ItemsPanelTemplate& value, const Base::Ref<Base::Object>& tree) noexcept;
    static void ClearAuthoredVisualTree(DataTemplate& value) noexcept;
    static void ClearAuthoredVisualTree(ItemsPanelTemplate& value) noexcept;
    static Base::Result<void> TryAddAuthoredTrigger(DataTemplate& value, Base::Ref<Aero::TriggerBase> trigger) noexcept;
    static void ClearAuthoredTriggers(DataTemplate& value) noexcept;
    static Base::Span<const Base::Ref<Aero::TriggerBase>> AuthoredTriggers(const DataTemplate& value) noexcept;
    static Base::Result<void> RegisterAuthoredName(DataTemplate& value, Base::StringView name, Base::Object& object) noexcept;
    static void ClearAuthoredNames(DataTemplate& value) noexcept;
    static const Aero::NameScope& AuthoredNames(const DataTemplate& value) noexcept;
    static const Base::Ref<Base::Object>& AuthoredVisualTree(const DataTemplate& value) noexcept;
    static const Base::Ref<Base::Object>& AuthoredVisualTree(const ItemsPanelTemplate& value) noexcept;
    static Base::Result<void> Seal(DataTemplate& value) noexcept;
    static Base::Result<void> Seal(ItemsPanelTemplate& value) noexcept;
    static Base::Result<Base::Ref<Base::Object>> Instantiate(const DataTemplate& value, const Base::Ref<Base::Object>& item) noexcept;
    static Base::Result<Base::Ref<Base::Object>> Instantiate(const ItemsPanelTemplate& value) noexcept;
};

} // namespace Aero::Controls::Detail
