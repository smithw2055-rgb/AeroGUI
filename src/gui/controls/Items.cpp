#include <Aero/Controls.hpp>
#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include "gui/data/BindingState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/controls/State.hpp" 
#include "gui/templates/TemplateState.hpp"

#include "gui/core/facets/VisualFacet.hpp"
#include <Aero/FrameworkElement.hpp>

#include <algorithm>
#include <cstdio>
#include <new>
#include <utility>
#include "ControlBehavior.hpp"

namespace Aero::Controls {
using Aero::Controls::TemplateEngine;

Base::Result<Value> AlternationConverter::Convert(
    const Value& value,
    const Value&) noexcept {
    if (values_.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "AlternationConverter has no values");
    }

    std::uint64_t index = 0U;
    if (value.Kind() == Meta::ValueKind::UnsignedInteger) {
        index = value.AsUnsignedInteger();
    } else if (value.Kind() == Meta::ValueKind::SignedInteger &&
               value.AsSignedInteger() >= 0) {
        index = static_cast<std::uint64_t>(
            value.AsSignedInteger());
    } else {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "AlternationConverter requires a non-negative integer index");
    }

    const Base::Ref<Base::Object>& selected =
        values_[static_cast<std::uint32_t>(
            index % values_.Size())];
    if (!selected) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "AlternationConverter contains a null value");
    }
    return Value::FromObject(
        selected->RuntimeType(),
        selected);
}

Base::Result<Value> AlternationConverter::ConvertBack(
    const Value& value,
    const Value&) noexcept {
    if (value.Kind() != Meta::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "AlternationConverter ConvertBack requires an object value");
    }
    for (std::uint32_t index = 0U;
         index < values_.Size();
         ++index) {
        if (values_[index].Get() == value.AsObject().Get()) {
            return Meta::ValueCodec<std::uint32_t>::Encode(index);
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "AlternationConverter value was not found");
}

Panel* ItemsPresenter::GetItemsHost() const noexcept {
    UIElement* child = GetChild();
    return child != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            child->RuntimeType(), Panel::StaticTypeId())
        ? static_cast<Panel*>(child)
        : nullptr;
}

void ItemsPresenter::SetItemsHost(
    const Base::Ref<Base::Object>& owner,
    Panel& panel) noexcept {
    Base::Result<void> assigned =
        ::Aero::Core::VisualFacet::DecoratorSetOwnedChild(
            *this, owner, panel);
    if (assigned) {
        static_cast<void>(InvalidateMeasure());
    }
}


using namespace ::Aero::Controls;
using namespace ::Aero;

Base::Result<void> AddBoxedItem(
    Collections::ObservableCollection& source,
    Meta::Value value) noexcept {
    Base::Result<Base::Ref<::Aero::Controls::BoxedItemValue>> boxed =
        Base::MakeRef<::Aero::Controls::BoxedItemValue>(std::move(value));
    if (!boxed) return boxed.GetStatus();
    return source.Add(
        Base::Ref<Base::Object>(std::move(boxed).Value()));
}

Base::Result<void> AddBoxedStringItem(
    Collections::ObservableCollection& source,
    Base::StringView value) noexcept {
    Base::Result<Meta::Value> boxed =
        Meta::Value::TryFromString(
            Meta::TypeOf<Base::String>(), value);
    if (!boxed) return boxed.GetStatus();
    return AddBoxedItem(
        source, std::move(boxed).Value());
}

ContentControl::ContentControl(
    TypeId runtimeType) noexcept
    : Control(runtimeType),
      foregroundChangedHandler_(
          this,
          &ContentControl::OnForegroundChanged) {
    static_cast<void>(AddValueChangedHandlerChecked(
        Control::ForegroundProperty,
        foregroundChangedHandler_));
}

ContentControl::~ContentControl() {
    static_cast<void>(RemoveValueChangedHandler(
        Control::ForegroundProperty,
        foregroundChangedHandler_));
}

void ContentControl::OnForegroundChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&)
        noexcept {
}

void ContentControl::SetGeneratedTextContent(
    const Base::Ref<Base::Object>& contentObject,
    UIElement& content) noexcept {
    if (!PropertyRegistry().Types().IsDerivedFrom(
            content.RuntimeType(),
            TextBlock::StaticTypeId())) {
        return;
    }
    SetOwnedContent(contentObject, content);
    literalTextContent_ = true;
}

Base::Ref<Base::Object> ItemCollection::GetItem(
    std::uint32_t index) const noexcept {
    return index < items_.Size()
        ? items_[index]
        : Base::Ref<Base::Object>();
}

void ItemCollection::Notify(
    const ItemsChangedEvent& event) noexcept {
    if (!changed_.Empty()) changed_.Invoke(event);
}

Base::Result<void> ItemCollection::Add(
    Base::Ref<Base::Object> item) noexcept {
    return Insert(items_.Size(), std::move(item));
}

Base::Result<void> ItemCollection::Insert(
    std::uint32_t index,
    Base::Ref<Base::Object> item) noexcept {
    if (!item || index > items_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ItemCollection insert is invalid");
    }
    Base::Result<void> reserved =
        items_.Reserve(items_.Size() + 1U);
    if (!reserved) return reserved.GetStatus();
    Base::Result<void> added =
        items_.PushBack(std::move(item));
    if (!added) return added.GetStatus();
    Base::Ref<Base::Object> moving =
        std::move(items_.Back());
    for (std::uint32_t current =
            items_.Size() - 1U;
        current > index; --current) {
        items_[current] =
            std::move(items_[current - 1U]);
    }
    items_[index] = std::move(moving);
    Notify({
        ItemsChangeAction::Add,
        UINT32_MAX,
        index,
        0U,
        1U});
    return {};
}

Base::Result<Base::Ref<Base::Object>>
ItemCollection::RemoveAt(
    std::uint32_t index) noexcept {
    if (index >= items_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "ItemCollection remove index is out of range");
    }
    Base::Ref<Base::Object> removed =
        std::move(items_[index]);
    for (std::uint32_t current = index;
        current + 1U < items_.Size(); ++current) {
        items_[current] =
            std::move(items_[current + 1U]);
    }
    items_.PopBack();
    Notify({
        ItemsChangeAction::Remove,
        index,
        UINT32_MAX,
        1U,
        0U});
    return removed;
}

Base::Result<void> ItemCollection::Replace(
    std::uint32_t index,
    Base::Ref<Base::Object> item) noexcept {
    if (!item || index >= items_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ItemCollection replacement is invalid");
    }
    items_[index] = std::move(item);
    Notify({
        ItemsChangeAction::Replace,
        index,
        index,
        1U,
        1U});
    return {};
}

Base::Result<void> ItemCollection::Move(
    std::uint32_t oldIndex,
    std::uint32_t newIndex) noexcept {
    if (oldIndex >= items_.Size() ||
        newIndex >= items_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "ItemCollection move index is out of range");
    }
    if (oldIndex == newIndex) return {};
    Base::Ref<Base::Object> moving =
        std::move(items_[oldIndex]);
    if (oldIndex < newIndex) {
        for (std::uint32_t index = oldIndex;
            index < newIndex; ++index) {
            items_[index] =
                std::move(items_[index + 1U]);
        }
    } else {
        for (std::uint32_t index = oldIndex;
            index > newIndex; --index) {
            items_[index] =
                std::move(items_[index - 1U]);
        }
    }
    items_[newIndex] = std::move(moving);
    Notify({
        ItemsChangeAction::Move,
        oldIndex,
        newIndex,
        1U,
        1U});
    return {};
}

void ItemCollection::Reset() noexcept {
    const std::uint32_t oldCount = items_.Size();
    items_.Clear();
    Notify({
        ItemsChangeAction::Reset,
        0U,
        0U,
        oldCount,
        0U});
}

Base::Result<void> ItemCollection::Reset(
    Base::Span<const Base::Ref<Base::Object>>
        items) noexcept {
    Base::Vector<Base::Ref<Base::Object>> replacement;
    Base::Result<void> reserved =
        replacement.Reserve(items.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Base::Ref<Base::Object>& item : items) {
        if (!item) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "ItemCollection reset item must not be null");
        }
        Base::Result<void> added =
            replacement.PushBack(item);
        if (!added) return added.GetStatus();
    }
    const std::uint32_t oldCount = items_.Size();
    items_ = std::move(replacement);
    Notify({
        ItemsChangeAction::Reset,
        0U,
        0U,
        oldCount,
        items_.Size()});
    return {};
}

Base::Result<void> DeferredObjectProgram::Configure(
    DeferredObjectFactory valueFactory,
    void* valueContext) noexcept {
    return Configure(valueFactory, valueContext, {});
}

Base::Result<void> DeferredObjectProgram::Configure(
    DeferredObjectFactory valueFactory,
    void* valueContext,
    Base::Ref<Base::Object> valueOwner) noexcept {
    if (sealed) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Deferred object program is sealed");
    }
    if (valueFactory == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Deferred object factory is null");
    }
    factory = valueFactory;
    context = valueContext;
    factoryOwner = std::move(valueOwner);
    return {};
}

Base::Result<void> DeferredObjectProgram::SetBaseUri(
    const Base::ResourceUri& value) noexcept {
    if (sealed) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Deferred object program is sealed");
    }
    baseUri = value;
    return {};
}

Base::Result<void> DeferredObjectProgram::Seal() noexcept {
    if (factory == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Deferred object program has no factory");
    }
    sealed = true;
    return {};
}

Base::Result<Base::Ref<Base::Object>> DeferredObjectProgram::Instantiate(
    const Base::Ref<Base::Object>& payload,
    Aero::BindingEngine* bindings) const noexcept {
    if (factory == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Deferred object program is not ready");
    }
    Base::Result<Base::Ref<Base::Object>> result =
        factory(payload, context, bindings);
    if (!result || result.Value()) return result;
    return Base::Status::Failure(Base::ErrorCode::InvalidState,
        "Deferred object factory returned null");
}

} // namespace Aero::Controls

namespace Aero {

using namespace Controls;

DataTemplate::DataTemplate() noexcept
    : state_(new (std::nothrow) Controls::DataTemplateState()) {
    if (state_ == nullptr) {
        Base::ReportOutOfMemory(sizeof(Controls::DataTemplateState), alignof(Controls::DataTemplateState), Base::MemoryTag::Ui);
    }
}

DataTemplate::~DataTemplate() noexcept {
    delete static_cast<Controls::DataTemplateState*>(state_);
    state_ = nullptr;
}

TypeId DataTemplate::GetDataType() const noexcept {
    const Controls::DataTemplateState* state = static_cast<const Controls::DataTemplateState*>(state_);
    return state != nullptr ? state->dataType : InvalidTypeId;
}

void DataTemplate::SetDataType(TypeId value) noexcept {
    Controls::DataTemplateState* state = static_cast<Controls::DataTemplateState*>(state_);
    if (state == nullptr) return;
    if (state->program.sealed || value == InvalidTypeId) {
        return;
    }
    state->dataType = value;
}

Base::Ref<Base::Object> DataTemplate::GetHierarchicalItemsSource() const noexcept {
    const Controls::DataTemplateState* state = static_cast<const Controls::DataTemplateState*>(state_);
    return state != nullptr ? state->hierarchicalItemsSource : Base::Ref<Base::Object>{};
}

void DataTemplate::SetHierarchicalItemsSource(Base::Ref<Base::Object> value) noexcept {
    Controls::DataTemplateState* state = static_cast<Controls::DataTemplateState*>(state_);
    if (state != nullptr) state->hierarchicalItemsSource = std::move(value);
}

Base::Ref<Base::Object> DataTemplate::GetHierarchicalItemTemplate() const noexcept {
    const Controls::DataTemplateState* state = static_cast<const Controls::DataTemplateState*>(state_);
    return state != nullptr ? state->hierarchicalItemTemplate : Base::Ref<Base::Object>{};
}

void DataTemplate::SetHierarchicalItemTemplate(Base::Ref<Base::Object> value) noexcept {
    Controls::DataTemplateState* state = static_cast<Controls::DataTemplateState*>(state_);
    if (state != nullptr) state->hierarchicalItemTemplate = std::move(value);
}

ResourceKey DataTemplate::GetImplicitKey() const noexcept {
    return ResourceKey::FromType(GetDataType());
}

ResourceDictionary& DataTemplate::GetResources() noexcept {
    Controls::DataTemplateState* state = static_cast<Controls::DataTemplateState*>(state_);
    if (state != nullptr) return state->resources;
    static ResourceDictionary fallback;
    return fallback;
}

const ResourceDictionary& DataTemplate::GetResources() const noexcept {
    const Controls::DataTemplateState* state = static_cast<const Controls::DataTemplateState*>(state_);
    if (state != nullptr) return state->resources;
    static ResourceDictionary fallback;
    return fallback;
}

bool DataTemplate::GetIsSealed() const noexcept {
    const Controls::DataTemplateState* state = static_cast<const Controls::DataTemplateState*>(state_);
    return state != nullptr && state->program.sealed;
}

ItemsPanelTemplate::ItemsPanelTemplate() noexcept
    : state_(new (std::nothrow) Controls::ItemsPanelTemplateState()) {
    if (state_ == nullptr) {
        Base::ReportOutOfMemory(sizeof(Controls::ItemsPanelTemplateState), alignof(Controls::ItemsPanelTemplateState), Base::MemoryTag::Ui);
    }
}

ItemsPanelTemplate::~ItemsPanelTemplate() noexcept {
    delete static_cast<Controls::ItemsPanelTemplateState*>(state_);
    state_ = nullptr;
}

ResourceDictionary& ItemsPanelTemplate::GetResources() noexcept {
    Controls::ItemsPanelTemplateState* state = static_cast<Controls::ItemsPanelTemplateState*>(state_);
    if (state != nullptr) return state->resources;
    static ResourceDictionary fallback;
    return fallback;
}

const ResourceDictionary& ItemsPanelTemplate::GetResources() const noexcept {
    const Controls::ItemsPanelTemplateState* state = static_cast<const Controls::ItemsPanelTemplateState*>(state_);
    if (state != nullptr) return state->resources;
    static ResourceDictionary fallback;
    return fallback;
}

bool ItemsPanelTemplate::GetIsSealed() const noexcept {
    const Controls::ItemsPanelTemplateState* state = static_cast<const Controls::ItemsPanelTemplateState*>(state_);
    return state != nullptr && state->program.sealed;
}

} // namespace Aero

namespace Aero {

using namespace ::Aero;
using namespace ::Aero::Controls;
using namespace ::Aero::Controls;

::Aero::Controls::DataTemplateState* DataTemplateRuntime::State(DataTemplate& value) noexcept {
    return static_cast<DataTemplateState*>(value.state_);
}

const ::Aero::Controls::DataTemplateState* DataTemplateRuntime::State(const DataTemplate& value) noexcept {
    return static_cast<const DataTemplateState*>(value.state_);
}

::Aero::Controls::ItemsPanelTemplateState* ItemsPanelTemplateRuntime::State(ItemsPanelTemplate& value) noexcept {
    return static_cast<ItemsPanelTemplateState*>(value.state_);
}

const ::Aero::Controls::ItemsPanelTemplateState* ItemsPanelTemplateRuntime::State(const ItemsPanelTemplate& value) noexcept {
    return static_cast<const ItemsPanelTemplateState*>(value.state_);
}

Base::Result<void> DataTemplateRuntime::Configure(DataTemplate& value, DeferredObjectFactory factory, void* context, Base::Ref<Base::Object> owner) noexcept {
    DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    return state->program.Configure(factory, context, std::move(owner));
}

Base::Result<void> ItemsPanelTemplateRuntime::Configure(ItemsPanelTemplate& value, DeferredObjectFactory factory, void* context, Base::Ref<Base::Object> owner) noexcept {
    ItemsPanelTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "ItemsPanelTemplate state allocation failed");
    return state->program.Configure(factory, context, std::move(owner));
}

Base::Result<void> DataTemplateRuntime::SetBaseUri(DataTemplate& value, const Base::ResourceUri& uri) noexcept {
    DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    return state->program.SetBaseUri(uri);
}

Base::Result<void> ItemsPanelTemplateRuntime::SetBaseUri(ItemsPanelTemplate& value, const Base::ResourceUri& uri) noexcept {
    ItemsPanelTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "ItemsPanelTemplate state allocation failed");
    return state->program.SetBaseUri(uri);
}

const Base::ResourceUri& DataTemplateRuntime::BaseUri(const DataTemplate& value) noexcept {
    static Base::ResourceUri empty;
    const DataTemplateState* state = State(value);
    return state != nullptr ? state->program.baseUri : empty;
}

const Base::ResourceUri& ItemsPanelTemplateRuntime::BaseUri(const ItemsPanelTemplate& value) noexcept {
    static Base::ResourceUri empty;
    const ItemsPanelTemplateState* state = State(value);
    return state != nullptr ? state->program.baseUri : empty;
}

Base::Result<void> DataTemplateRuntime::SetAuthoredVisualTree(DataTemplate& value, const Base::Ref<Base::Object>& tree) noexcept {
    DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    if (state->program.sealed || !tree) return Base::Status::Failure(Base::ErrorCode::InvalidState, "DataTemplate VisualTree assignment is invalid");
    state->authoredVisualTree = tree;
    return {};
}

Base::Result<void> ItemsPanelTemplateRuntime::SetAuthoredVisualTree(ItemsPanelTemplate& value, const Base::Ref<Base::Object>& tree) noexcept {
    ItemsPanelTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "ItemsPanelTemplate state allocation failed");
    if (state->program.sealed || !tree) return Base::Status::Failure(Base::ErrorCode::InvalidState, "ItemsPanelTemplate VisualTree assignment is invalid");
    state->authoredVisualTree = tree;
    return {};
}

void DataTemplateRuntime::ClearAuthoredVisualTree(DataTemplate& value) noexcept { DataTemplateState* state = State(value); if (state != nullptr) state->authoredVisualTree.Reset(); }
void ItemsPanelTemplateRuntime::ClearAuthoredVisualTree(ItemsPanelTemplate& value) noexcept { ItemsPanelTemplateState* state = State(value); if (state != nullptr) state->authoredVisualTree.Reset(); }

Base::Result<void> DataTemplateRuntime::AddAuthoredTrigger(DataTemplate& value, Base::Ref<Aero::TriggerBase> trigger) noexcept {
    DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    if (!trigger || state->program.factory != nullptr) return Base::Status::Failure(Base::ErrorCode::InvalidState, "DataTemplate Trigger cannot be added after sealing");
    return state->authoredTriggers.PushBack(std::move(trigger));
}

void DataTemplateRuntime::ClearAuthoredTriggers(DataTemplate& value) noexcept { DataTemplateState* state = State(value); if (state != nullptr) state->authoredTriggers.Clear(); }

Base::Span<const Base::Ref<Aero::TriggerBase>> DataTemplateRuntime::AuthoredTriggers(const DataTemplate& value) noexcept {
    const DataTemplateState* state = State(value);
    return state != nullptr ? Base::Span<const Base::Ref<Aero::TriggerBase>>(state->authoredTriggers.Data(), state->authoredTriggers.Size()) : Base::Span<const Base::Ref<Aero::TriggerBase>>{};
}

Base::Result<void> DataTemplateRuntime::RegisterAuthoredName(DataTemplate& value, Base::StringView name, Base::Object& object) noexcept {
    DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    return state->authoredNames.Register(name, object);
}

void DataTemplateRuntime::ClearAuthoredNames(DataTemplate& value) noexcept { DataTemplateState* state = State(value); if (state != nullptr) state->authoredNames.Clear(); }

const Aero::NameScope& DataTemplateRuntime::AuthoredNames(const DataTemplate& value) noexcept {
    static Aero::NameScope empty;
    const DataTemplateState* state = State(value);
    return state != nullptr ? state->authoredNames : empty;
}

const Base::Ref<Base::Object>& DataTemplateRuntime::AuthoredVisualTree(const DataTemplate& value) noexcept {
    static Base::Ref<Base::Object> empty;
    const DataTemplateState* state = State(value);
    return state != nullptr ? state->authoredVisualTree : empty;
}

const Base::Ref<Base::Object>& ItemsPanelTemplateRuntime::AuthoredVisualTree(const ItemsPanelTemplate& value) noexcept {
    static Base::Ref<Base::Object> empty;
    const ItemsPanelTemplateState* state = State(value);
    return state != nullptr ? state->authoredVisualTree : empty;
}

Base::Result<void> DataTemplateRuntime::Seal(DataTemplate& value) noexcept {
    DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    Base::Result<void> program = state->program.Seal();
    if (!program) return program.GetStatus();
    return state->resources.Seal();
}

Base::Result<void> ItemsPanelTemplateRuntime::Seal(ItemsPanelTemplate& value) noexcept {
    ItemsPanelTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "ItemsPanelTemplate state allocation failed");
    Base::Result<void> program = state->program.Seal();
    if (!program) return program.GetStatus();
    return state->resources.Seal();
}

Base::Result<Base::Ref<Base::Object>> DataTemplateRuntime::Instantiate(
    const DataTemplate& value, const Base::Ref<Base::Object>& item,
    Aero::BindingEngine* bindings) noexcept {
    const DataTemplateState* state = State(value);
    if (state == nullptr || state->program.factory == nullptr || !item) return Base::Status::Failure(Base::ErrorCode::InvalidState, "DataTemplate is not ready");
    return state->program.Instantiate(item, bindings);
}

Base::Result<Base::Ref<Base::Object>> ItemsPanelTemplateRuntime::Instantiate(const ItemsPanelTemplate& value) noexcept {
    const ItemsPanelTemplateState* state = State(value);
    if (state == nullptr || state->program.factory == nullptr) return Base::Status::Failure(Base::ErrorCode::InvalidState, "ItemsPanelTemplate is not ready");
    return state->program.Instantiate();
}

} // namespace Aero

namespace Aero::Controls {

using namespace ::Aero::Controls;
using namespace ::Aero::Controls;

DataTemplateState* TemplatePrivate::State(DataTemplate& value) noexcept {
    return DataTemplateRuntime::State(value);
}

const DataTemplateState* TemplatePrivate::State(
    const DataTemplate& value) noexcept {
    return DataTemplateRuntime::State(value);
}

ItemsPanelTemplateState* TemplatePrivate::State(
    ItemsPanelTemplate& value) noexcept {
    return ItemsPanelTemplateRuntime::State(value);
}

const ItemsPanelTemplateState* TemplatePrivate::State(
    const ItemsPanelTemplate& value) noexcept {
    return ItemsPanelTemplateRuntime::State(value);
}

Base::Result<void> TemplatePrivate::Configure(
    DataTemplate& value,
    DeferredObjectFactory factory,
    void* context,
    Base::Ref<Base::Object> owner) noexcept {
    return DataTemplateRuntime::Configure(
        value, factory, context, std::move(owner));
}

Base::Result<void> TemplatePrivate::Configure(
    ItemsPanelTemplate& value,
    DeferredObjectFactory factory,
    void* context,
    Base::Ref<Base::Object> owner) noexcept {
    return ItemsPanelTemplateRuntime::Configure(
        value, factory, context, std::move(owner));
}

Base::Result<void> TemplatePrivate::SetBaseUri(
    DataTemplate& value,
    const Base::ResourceUri& uri) noexcept {
    return DataTemplateRuntime::SetBaseUri(value, uri);
}

Base::Result<void> TemplatePrivate::SetBaseUri(
    ItemsPanelTemplate& value,
    const Base::ResourceUri& uri) noexcept {
    return ItemsPanelTemplateRuntime::SetBaseUri(value, uri);
}

const Base::ResourceUri& TemplatePrivate::BaseUri(
    const DataTemplate& value) noexcept {
    return DataTemplateRuntime::BaseUri(value);
}

const Base::ResourceUri& TemplatePrivate::BaseUri(
    const ItemsPanelTemplate& value) noexcept {
    return ItemsPanelTemplateRuntime::BaseUri(value);
}

Base::Result<void> TemplatePrivate::SetAuthoredVisualTree(
    DataTemplate& value,
    const Base::Ref<Base::Object>& tree) noexcept {
    return DataTemplateRuntime::SetAuthoredVisualTree(value, tree);
}

Base::Result<void> TemplatePrivate::SetAuthoredVisualTree(
    ItemsPanelTemplate& value,
    const Base::Ref<Base::Object>& tree) noexcept {
    return ItemsPanelTemplateRuntime::SetAuthoredVisualTree(value, tree);
}

void TemplatePrivate::ClearAuthoredVisualTree(
    DataTemplate& value) noexcept {
    DataTemplateRuntime::ClearAuthoredVisualTree(value);
}

void TemplatePrivate::ClearAuthoredVisualTree(
    ItemsPanelTemplate& value) noexcept {
    ItemsPanelTemplateRuntime::ClearAuthoredVisualTree(value);
}

Base::Result<void> TemplatePrivate::AddAuthoredTrigger(
    DataTemplate& value,
    Base::Ref<Aero::TriggerBase> trigger) noexcept {
    return DataTemplateRuntime::AddAuthoredTrigger(
        value, std::move(trigger));
}

void TemplatePrivate::ClearAuthoredTriggers(
    DataTemplate& value) noexcept {
    DataTemplateRuntime::ClearAuthoredTriggers(value);
}

Base::Span<const Base::Ref<Aero::TriggerBase>>
TemplatePrivate::AuthoredTriggers(
    const DataTemplate& value) noexcept {
    return DataTemplateRuntime::AuthoredTriggers(value);
}

Base::Result<void> TemplatePrivate::RegisterAuthoredName(
    DataTemplate& value,
    Base::StringView name,
    Base::Object& object) noexcept {
    return DataTemplateRuntime::RegisterAuthoredName(
        value, name, object);
}

void TemplatePrivate::ClearAuthoredNames(
    DataTemplate& value) noexcept {
    DataTemplateRuntime::ClearAuthoredNames(value);
}

const Aero::NameScope& TemplatePrivate::AuthoredNames(
    const DataTemplate& value) noexcept {
    return DataTemplateRuntime::AuthoredNames(value);
}

const Base::Ref<Base::Object>& TemplatePrivate::AuthoredVisualTree(
    const DataTemplate& value) noexcept {
    return DataTemplateRuntime::AuthoredVisualTree(value);
}

const Base::Ref<Base::Object>& TemplatePrivate::AuthoredVisualTree(
    const ItemsPanelTemplate& value) noexcept {
    return ItemsPanelTemplateRuntime::AuthoredVisualTree(value);
}

Base::Result<void> TemplatePrivate::Seal(
    DataTemplate& value) noexcept {
    return DataTemplateRuntime::Seal(value);
}

Base::Result<void> TemplatePrivate::Seal(
    ItemsPanelTemplate& value) noexcept {
    return ItemsPanelTemplateRuntime::Seal(value);
}

Base::Result<Base::Ref<Base::Object>> TemplatePrivate::Instantiate(
    const DataTemplate& value,
    const Base::Ref<Base::Object>& item,
    Aero::BindingEngine* bindings) noexcept {
    return DataTemplateRuntime::Instantiate(value, item, bindings);
}

Base::Result<Base::Ref<Base::Object>> TemplatePrivate::Instantiate(
    const ItemsPanelTemplate& value) noexcept {
    return ItemsPanelTemplateRuntime::Instantiate(value);
}

} // namespace Aero::Controls

namespace Aero::Controls {

Base::Result<void> ContentControl::StoreContentProperty(
    Meta::Value value) noexcept {
    if (synchronizingContentProperty_) return {};
    synchronizingContentProperty_ = true;
    SetValue(ContentProperty, std::move(value));
    synchronizingContentProperty_ = false;
    return {};
}

void ContentControl::OnContentPropertyChanged(
    ::Aero::DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs&
        change) noexcept {
    auto& control = static_cast<ContentControl&>(object);
    if (control.synchronizingContentProperty_) return;
    control.synchronizingContentProperty_ = true;
    static_cast<void>(
        ::Aero::Core::InteractionStateFacet::SetContentValue(control, change.GetNewValue()));
    control.synchronizingContentProperty_ = false;
}

void ContentControl::SetContentValue(
    Base::Ref<Base::Object> value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    if (value &&
        PropertyRegistry().Types().IsDerivedFrom(
            value->RuntimeType(),
            UIElement::StaticTypeId())) {
        authoredContent_ = Meta::Value::FromObject(
            value->RuntimeType(), value);
        SetOwnedContent(
            value,
            *static_cast<UIElement*>(value.Get()));
        return;
    }
    literalTextContent_ = false;
    if (content_ != nullptr) {
        SetContent(nullptr);
    }
    contentValue_ = std::move(value);
    ownedContent_.Reset();
    authoredContent_ = contentValue_
        ? Meta::Value::FromObject(
            contentValue_->RuntimeType(),
            contentValue_)
        : Meta::Value::NullObject(
            Meta::TypeOf<Base::Object>());
    (void)InvalidateMeasure();
}

void ContentControl::SetContentValue(
    Meta::Value value) noexcept {
    if (value.IsUnset()) {
        return;
    }
    if (value.Kind() == Meta::ValueKind::Object) {
        authoredContent_ = value;
        SetContentValue(value.AsObject());
        return;
    }
    if (value.Kind() != Meta::ValueKind::String) {
        (void)StoreContentProperty(value);
        authoredContent_ = std::move(value);
        contentValue_.Reset();
        ownedContent_.Reset();
        (void)InvalidateMeasure();
        return;
    }

    if (literalTextContent_ && content_ != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            content_->RuntimeType(), TextBlock::StaticTypeId())) {
        auto* textBlock = static_cast<TextBlock*>(content_);
        textBlock->SetValue(RichText::TextProperty, value.AsString());
        textBlock->SetText(value.AsString());
        (void)StoreContentProperty(value);
        authoredContent_ = std::move(value);
        contentValue_.Reset();
        (void)InvalidateMeasure();
        (void)InvalidateVisual();
        return;
    }

    Base::Result<Base::Ref<TextBlock>> created =
        Base::MakeRef<TextBlock>();
    if (!created) return;
    created.Value()->SetValue(RichText::TextProperty, value.AsString());
    created.Value()->SetText(value.AsString());
    Base::Ref<Base::Object> retained(created.Value());
    SetOwnedContent(retained, *created.Value());
    (void)StoreContentProperty(value);
    authoredContent_ = std::move(value);
    contentValue_.Reset();
    literalTextContent_ = true;
    return;
}

Base::Result<Base::Ref<Base::Object>>
ContentControl::CreateTemplatedContent() const noexcept {
    if (content_ != nullptr) {
        return ownedContent_;
    }
    if (!contentValue_) {
        return Base::Ref<Base::Object>{};
    }
    Base::Ref<Base::Object> contentTemplate =
        GetContentTemplate();
    if (!contentTemplate) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "ContentControl business content requires a ContentTemplate");
    }
    if (contentTemplate->RuntimeType() !=
        DataTemplate::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ContentControl ContentTemplate is not a DataTemplate");
    }
    Base::Result<Base::Ref<Base::Object>> created =
        DataTemplateRuntime::Instantiate(
            *static_cast<DataTemplate*>(contentTemplate.Get()),
            contentValue_,
            Aero::Core::GetFacet<::Aero::BindingEngine>(*this));
    if (!created) return created.GetStatus();
    if (!created.Value() ||
        !PropertyRegistry().Types().IsDerivedFrom(
            created.Value()->RuntimeType(),
            UIElement::StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ContentTemplate must create a UIElement");
    }
    return created;
}

ItemsControl::ItemsControl() noexcept
    : ItemsControl(StaticTypeId()) {}

ItemsControl::ItemsControl(TypeId runtimeType) noexcept
    : Control(runtimeType),
      localHandler_(
          this, &ItemsControl::OnLocalChanged),
      sourceHandler_(
          this, &ItemsControl::OnSourceChanged) {
    static_cast<void>(
        items_.AddItemsChanged(localHandler_));
}

ItemsControl::~ItemsControl() {
    if (generator_ != nullptr) {
        static_cast<void>(generator_->Detach());
    }
    static_cast<void>(
        items_.RemoveItemsChanged(localHandler_));
    if (source_ != nullptr) {
        static_cast<void>(
            source_->RemoveItemsChanged(
                sourceHandler_));
    }
}

void ItemsControl::OnApplyTemplate() noexcept {
    DependencyObject* part =
        GetTemplateChild("ItemsHost");
    if (part == nullptr) {
        part = GetTemplateChild("ItemsPresenter");
    }
    if (part == nullptr) {
        part = GetTemplateChild(
            ItemsPresenter::StaticTypeId());
    }
    if (part == nullptr) {
        // Reference XAML is also allowed to declare an items host directly
        // (<StackPanel IsItemsHost="True"/>), without an ItemsPresenter or a
        // PART name. TemplateEngine resolves such a panel before returning a
        // generic Panel part.
        part = GetTemplateChild(Panel::StaticTypeId());
    }
    if (part == nullptr) {
        // Content inside a Popup is structurally projected by the template
        // builder and is therefore not necessarily present in the outer
        // template part table. Discover the direct IsItemsHost declaration
        // from the complete applied visual subtree.
        Base::Vector<::Aero::Media::Visual*> pending;
        UIElement* root = GetTemplateRoot();
        if (root != nullptr) {
            static_cast<void>(pending.PushBack(root));
        }
        while (!pending.Empty() && part == nullptr) {
            ::Aero::Media::Visual* current = pending.Back();
            pending.PopBack();
            if (current == nullptr) continue;
            if (PropertyRegistry().Types().IsDerivedFrom(
                    current->RuntimeType(), Panel::StaticTypeId())) {
                auto& panel = *static_cast<Panel*>(current);
                if (panel.GetValueOr(Panel::IsItemsHostProperty, false)) {
                    part = current;
                    break;
                }
            }
            if (PropertyRegistry().Types().IsDerivedFrom(
                    current->RuntimeType(), ContentControl::StaticTypeId())) {
                UIElement* content = ::Aero::Core::InteractionStateFacet::ContentElement(
                    *static_cast<ContentControl*>(current));
                if (content != nullptr) {
                    static_cast<void>(pending.PushBack(content));
                }
            }
            for (::Aero::Media::Visual* child :
                     current->GetVisualChildren()) {
                if (child != nullptr) {
                    static_cast<void>(pending.PushBack(child));
                }
            }
        }
    }
    if (part == nullptr) {
        return;
    }
    if (PropertyRegistry().Types().IsDerivedFrom(
            part->RuntimeType(),
            ItemsPresenter::StaticTypeId())) {
        itemsHost_ =
            static_cast<ItemsPresenter*>(part)->
                GetItemsHost();
    } else if (PropertyRegistry().Types().IsDerivedFrom(
                   part->RuntimeType(),
                   Panel::StaticTypeId())) {
        itemsHost_ = static_cast<Panel*>(part);
    }
    if (itemsHost_ == nullptr) {
        return;
    }
    return;
}

void ItemsControl::OnTemplateDetached() noexcept {
    if (generator_ != nullptr) {
        static_cast<void>(generator_->Detach());
    }
    itemsHost_ = nullptr;
}

std::uint32_t ItemsControl::GetCount() const noexcept {
    return source_ != nullptr
        ? source_->GetCount()
        : items_.GetCount();
}

std::uint32_t ItemsControl::GetRealizedItemCount() const noexcept {
    return generator_ != nullptr
        ? generator_->GetGeneratedCount()
        : 0U;
}

std::uint32_t ItemsControl::GetCreatedContainerCount() const noexcept {
    return generator_ != nullptr
        ? generator_->GetCreatedContainerCount()
        : 0U;
}

std::uint32_t
ItemsControl::GetRecycledContainerUseCount() const noexcept {
    return generator_ != nullptr
        ? generator_->GetRecycledContainerUseCount()
        : 0U;
}

Base::Ref<Base::Object> ItemsControl::GetItem(
    std::uint32_t index) const noexcept {
    return source_ != nullptr
        ? source_->GetItem(index)
        : items_.GetItem(index);
}

void ItemsControl::SetItemsSourceCore(
    Collections::IItemsSource* source) noexcept {
    if (source_ == source) return;
    if (source != nullptr) {
        source->AddItemsChanged(sourceHandler_);
    }
    if (source_ != nullptr) {
        static_cast<void>(
            source_->RemoveItemsChanged(
                sourceHandler_));
    }
    source_ = source;
    fprintf(stderr, "[SRC] SetItemsSourceCore source=%p count=%u\n",
        (void*)source, source ? source->GetCount() : 0U);
    PublishItemCount();
    PublishReset();
}

void ItemsControl::SetItemTemplateCore(
    const DataTemplate* value) noexcept {
    if (itemTemplate_ == value) return;
    itemTemplate_ = value;
    PublishReset();
}

void ItemsControl::SetItemsPanelCore(
    const ItemsPanelTemplate* value) noexcept {
    if (itemsPanel_ == value) return;
    itemsPanel_ = value;
    PublishReset();
}

void ItemsControl::SetItemContainerStyleCore(
    const Style* value) noexcept {
    if (itemContainerStyle_ == value) return;
    itemContainerStyle_ = value;
    PublishReset();
}

void ItemsControl::OnLocalChanged(
    const ItemsChangedEvent& event) noexcept {
    if (source_ != nullptr) return;
    PublishItemCount();
    if (!changed_.Empty()) changed_.Invoke(event);
}

void ItemsControl::OnSourceChanged(
    const ItemsChangedEvent& event) noexcept {
    PublishItemCount();
    if (!changed_.Empty()) changed_.Invoke(event);
}

void ItemsControl::PublishReset() noexcept {
    if (!changed_.Empty()) {
        changed_.Invoke({
            ItemsChangeAction::Reset,
            0U,
            0U,
            GetCount(),
            GetCount()});
    }
}

void ItemsControl::PublishItemCount() noexcept {
    const std::uint32_t count = GetCount();
    static_cast<void>(SetReadOnlyCurrentValue(
        ItemCountProperty, count));
    static_cast<void>(SetReadOnlyCurrentValue(
        HasItemsProperty, count != 0U));
}

Base::Result<Base::Ref<FrameworkElement>>
ItemsControl::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    class GeneratedContentControl : public ContentControl {
    public:
        GeneratedContentControl() noexcept
            : ContentControl(ContentControl::StaticTypeId()) {}
        ~GeneratedContentControl() override = default;
    };
    Base::Result<Base::Ref<GeneratedContentControl>> made =
        Base::MakeRef<GeneratedContentControl>();
    if (!made) return made.GetStatus();
    return Base::Ref<FrameworkElement>(
        std::move(made).Value());
}

Base::Result<void> ItemsControl::PrepareContainer(
    FrameworkElement& container,
    const Base::Ref<Base::Object>& item,
    std::uint32_t) noexcept {
    if (item && item.Get() != &container) {
        container.SetDataContext(
            Value::FromObject(
                item->RuntimeType(), item));
    }
    if (!item || itemTemplate_ == nullptr ||
        !PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(), ItemsControl::StaticTypeId())) {
        return {};
    }

    const Base::Ref<Base::Object> hierarchicalSource =
        itemTemplate_->GetHierarchicalItemsSource();
    const Base::Ref<Base::Object> hierarchicalTemplate =
        itemTemplate_->GetHierarchicalItemTemplate();
    if (!hierarchicalSource && !hierarchicalTemplate) return {};

    auto& childItems = static_cast<ItemsControl&>(container);
    Base::Ref<DataTemplate> childItemTemplate;
    if (hierarchicalTemplate &&
        PropertyRegistry().Types().IsDerivedFrom(
            hierarchicalTemplate->RuntimeType(),
            DataTemplate::StaticTypeId())) {
        childItemTemplate = Base::Ref<DataTemplate>::FromBorrowed(
            static_cast<DataTemplate&>(*hierarchicalTemplate));
    }
    if (!hierarchicalSource) {
        childItems.SetItemTemplate(std::move(childItemTemplate));
        return {};
    }
    if (!PropertyRegistry().Types().IsDerivedFrom(
            hierarchicalSource->RuntimeType(),
            Data::Binding::StaticTypeId())) {
        if (PropertyRegistry().Types().IsDerivedFrom(
                container.RuntimeType(),
                TreeViewItem::StaticTypeId())) {
            static_cast<TreeViewItem&>(container).SetHierarchicalContent(
                hierarchicalSource,
                std::move(childItemTemplate));
            return {};
        }
        childItems.SetItemTemplate(std::move(childItemTemplate));
        childItems.SetItemsSource(hierarchicalSource);
        return {};
    }

    auto* bindings = ::Aero::Core::GetFacet<::Aero::BindingEngine>(
        childItems);
    if (bindings == nullptr || bindings->Metadata() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "HierarchicalDataTemplate Binding services are unavailable");
    }
    const auto& binding =
        static_cast<const Data::Binding&>(*hierarchicalSource);
    if (PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            TreeViewItem::StaticTypeId())) {
        static_cast<TreeViewItem&>(container).SetHierarchicalBinding(
            Base::Ref<Data::Binding>::FromBorrowed(
                const_cast<Data::Binding&>(binding)),
            item,
            std::move(childItemTemplate));
        return {};
    }
    childItems.SetItemTemplate(std::move(childItemTemplate));
    Data::MetadataBindingDescriptor descriptor;
    descriptor.metadata = bindings->Metadata();
    descriptor.source = item.Get();
    descriptor.target = &childItems;
    descriptor.targetProperty = ItemsSourceProperty.Handle();
    descriptor.path = binding.GetPathText();
    descriptor.stringFormat = binding.GetStringFormat();
    descriptor.mode = binding.GetMode() == Data::BindingMode::Default
        ? Data::BindingMode::OneWay
        : binding.GetMode();
    descriptor.updateSourceTrigger = binding.GetUpdateSourceTrigger() ==
            Meta::UpdateSourceTrigger::Default
        ? Meta::UpdateSourceTrigger::PropertyChanged
        : binding.GetUpdateSourceTrigger();
    descriptor.converterResource = binding.GetConverter();
    descriptor.converterParameter = binding.GetConverterParameter();
    descriptor.fallbackValue = binding.GetFallbackValue();
    descriptor.targetNullValue = binding.GetTargetNullValue();
    Base::Result<void> queued = bindings->QueueDeferred(descriptor);
    if (!queued) return queued.GetStatus();
    return bindings->ActivateDeferredWhenReady(childItems);
}

void ItemsControl::ClearContainer(
    FrameworkElement& container) noexcept {
    if (itemTemplate_ != nullptr &&
        (itemTemplate_->GetHierarchicalItemsSource() ||
         itemTemplate_->GetHierarchicalItemTemplate()) &&
        PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(), ItemsControl::StaticTypeId())) {
        auto& childItems = static_cast<ItemsControl&>(container);
        if (PropertyRegistry().Types().IsDerivedFrom(
                container.RuntimeType(),
                TreeViewItem::StaticTypeId())) {
            static_cast<TreeViewItem&>(container)
                .ClearHierarchicalContent();
        }
        childItems.SetItemsSource(Base::Ref<Base::Object>{});
        childItems.SetItemTemplate(Base::Ref<DataTemplate>{});
    }
    container.ClearValue(
        FrameworkElement::DataContextProperty);
}

} // namespace Aero::Controls

namespace Aero::Controls {

using namespace ::Aero;
using namespace ::Aero::Controls;
using namespace ::Aero::Controls;
using namespace ::Aero;

struct ItemContainerGeneratorRuntime {
public:
    ItemContainerGeneratorRuntime(
        ItemContainerGenerator& facade,
        ElementTree& tree,
        LayoutEngine& layout,
        EffectiveValueEngine& values,
        StyleEngine* styles,
        ::Aero::Render::RenderTree* renderer,
        TemplateEngine* templates,
        ItemSubtreeCallback subtreeCallback,
        void* subtreeContext) noexcept;
    ~ItemContainerGeneratorRuntime() noexcept;

    Base::Result<void> Attach(ItemsControl& owner, Panel& itemsHost) noexcept;
    Base::Result<void> AttachVirtualized(
        ItemsControl& owner,
        VirtualizingStackPanel& itemsHost) noexcept;
    Base::Result<bool> Detach() noexcept;
    Base::Result<void> Refresh() noexcept;
    Base::Result<bool> SetRealizationRange(
        std::uint32_t firstIndex,
        std::uint32_t count) noexcept;
    FrameworkElement* ContainerFromIndex(std::uint32_t index) const noexcept;
    std::uint32_t IndexFromContainer(
        const FrameworkElement& container) const noexcept;
    Base::Ref<Base::Object> ItemFromContainer(
        const FrameworkElement& container) const noexcept;

    std::uint32_t GetGeneratedCount() const noexcept { return records_.Size(); }
    std::uint32_t GetFirstGeneratedIndex() const noexcept {
        return firstGeneratedIndex_;
    }
    std::uint32_t GetCreatedContainerCount() const noexcept {
        return createdContainerCount_;
    }
    std::uint32_t GetRecycledContainerUseCount() const noexcept {
        return recycledContainerUseCount_;
    }
    Base::Status LastError() const noexcept { return lastError_; }

private:
    struct Record {
        Base::Ref<Base::Object> item;
        Base::Ref<FrameworkElement> container;
        Base::Ref<Base::Object> content;
        ElementAttachment containerMount;
        ElementAttachment contentMount;
        Base::Vector<ElementAttachment> subtreeMounts;
        const Style* appliedStyle = nullptr;
        bool itemIsOwnContainer = false;
        bool generatedTextContent = false;
        bool generatedHeader = false;
        bool subtreeMounted = false;
    };

    ItemContainerGenerator* facade_ = nullptr;
    ElementTree* tree_ = nullptr;
    LayoutEngine* layout_ = nullptr;
    EffectiveValueEngine* values_ = nullptr;
    StyleEngine* styles_ = nullptr;
    ::Aero::Render::RenderTree* renderer_ = nullptr;
    TemplateEngine* templates_ = nullptr;
    ItemSubtreeCallback subtreeCallback_ = nullptr;
    void* subtreeContext_ = nullptr;
    ItemsControl* owner_ = nullptr;
    Panel* host_ = nullptr;
    VirtualizingStackPanel* virtualizingHost_ = nullptr;
    Base::Vector<Record> records_;
    Base::Vector<Base::Ref<FrameworkElement>> recycledContainers_;
    ItemsChangedHandler changedHandler_;
    DependencyPropertyChangedEventHandler
        generatedHeaderChangedHandler_;
    std::uint32_t firstGeneratedIndex_ = 0U;
    std::uint32_t targetRealizationCount_ = 0U;
    std::uint32_t createdContainerCount_ = 0U;
    std::uint32_t recycledContainerUseCount_ = 0U;
    Base::Status lastError_;
    bool inRealizationRange_ = false;
    bool virtualized_ = false;

    Base::Result<Record> CreateRecord(
        std::uint32_t index) noexcept;
    Base::Result<void> AttachRecord(
        Record& record,
        std::uint32_t index) noexcept;
    Base::Result<void> DetachRecord(
        Record& record,
        bool recycleContainer = false) noexcept;
    Base::Result<void> AttachOwnedSubtree(
        Record& record,
        Aero::Media::Visual& root) noexcept;
    Base::Result<void> DetachOwnedSubtree(
        Record& record) noexcept;
    Base::Result<void> UpdateGeneratedHeader(
        Record& record) noexcept;
    void OnGeneratedHeaderChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void OnItemsChanged(
        const ItemsChangedEvent& event) noexcept;
    Base::Result<void> ApplyChange(
        const ItemsChangedEvent& event) noexcept;
    Base::Result<void> InsertRecord(
        std::uint32_t index,
        Record record) noexcept;
    void RemoveRecordAt(
        std::uint32_t index) noexcept;
    Base::Result<void> ReorderVisuals() noexcept;
    Base::Result<bool> SetRealizationRangeInternal(
        std::uint32_t firstIndex,
        std::uint32_t count,
        bool pruneRecycled) noexcept;
    Base::Result<void> ReleaseRecycledContainers() noexcept;
};

ItemContainerGeneratorRuntime::ItemContainerGeneratorRuntime(
    ItemContainerGenerator& facade,
    ElementTree& tree,
    LayoutEngine& layout,
    EffectiveValueEngine& values,
    StyleEngine* styles,
    ::Aero::Render::RenderTree* renderer,
    TemplateEngine* templates,
    ItemSubtreeCallback subtreeCallback,
    void* subtreeContext) noexcept
    : facade_(&facade),
      tree_(&tree),
      layout_(&layout),
      values_(&values),
      styles_(styles),
      renderer_(renderer),
      templates_(templates),
      subtreeCallback_(subtreeCallback),
      subtreeContext_(subtreeContext),
      changedHandler_(
          this,
          &ItemContainerGeneratorRuntime::OnItemsChanged),
      generatedHeaderChangedHandler_(
          this,
          &ItemContainerGeneratorRuntime::OnGeneratedHeaderChanged) {}

ItemContainerGeneratorRuntime::~ItemContainerGeneratorRuntime() noexcept {
    static_cast<void>(Detach());
}

Base::Result<void> ItemContainerGeneratorRuntime::Attach(
    ItemsControl& owner,
    Panel& itemsHost) noexcept {
    if (owner_ != nullptr ||
        owner.generator_ != nullptr ||
        owner.GetTree() != tree_ ||
        itemsHost.GetTree() != tree_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemContainerGenerator attach state is invalid");
    }
    owner.AddItemsChanged(changedHandler_);
    owner_ = &owner;
    host_ = &itemsHost;
    virtualizingHost_ = nullptr;
    firstGeneratedIndex_ = 0U;
    owner.generator_ = facade_;
    Base::Result<void> refreshed = Refresh();
    if (!refreshed) {
        static_cast<void>(
            owner.RemoveItemsChanged(
                changedHandler_));
        for (std::uint32_t index = records_.Size();
            index > 0U; --index) {
            static_cast<void>(
                DetachRecord(records_[index - 1U]));
        }
        records_.Clear();
        owner_ = nullptr;
        host_ = nullptr;
        owner.generator_ = nullptr;
        return refreshed.GetStatus();
    }
    return {};
}

Base::Result<void>
ItemContainerGeneratorRuntime::AttachVirtualized(
    ItemsControl& owner,
    VirtualizingStackPanel& itemsHost) noexcept {
    if (owner_ != nullptr ||
        owner.generator_ != nullptr ||
        owner.GetTree() != tree_ ||
        itemsHost.GetTree() != tree_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Virtualized item generator attach state is invalid");
    }
    owner.AddItemsChanged(changedHandler_);
    owner_ = &owner;
    host_ = &itemsHost;
    virtualizingHost_ = &itemsHost;
    firstGeneratedIndex_ = 0U;
    owner.generator_ = facade_;
    Base::Result<void> attached =
        itemsHost.AttachGenerator(*facade_, owner.GetCount());
    if (!attached) {
        static_cast<void>(
            owner.RemoveItemsChanged(changedHandler_));
        owner.generator_ = nullptr;
        owner_ = nullptr;
        host_ = nullptr;
        virtualizingHost_ = nullptr;
        return attached.GetStatus();
    }
    Base::Result<bool> realized =
        SetRealizationRangeInternal(
            itemsHost.desiredFirstIndex_,
            itemsHost.desiredCount_,
            true);
    if (!realized) {
        itemsHost.DetachGenerator(*facade_);
        static_cast<void>(
            owner.RemoveItemsChanged(changedHandler_));
        owner.generator_ = nullptr;
        owner_ = nullptr;
        host_ = nullptr;
        virtualizingHost_ = nullptr;
        return realized.GetStatus();
    }
    owner.OnContainersChanged();
    return {};
}

Base::Result<bool> ItemContainerGeneratorRuntime::Detach() noexcept {
    if (owner_ == nullptr) return false;
    static_cast<void>(
        owner_->RemoveItemsChanged(
            changedHandler_));
    Base::Status firstError;
    for (std::uint32_t index = records_.Size();
        index > 0U; --index) {
        Base::Result<void> detached =
            DetachRecord(records_[index - 1U]);
        if (!detached && firstError.IsOk()) {
            firstError = detached.GetStatus();
        }
    }
    records_.Clear();
    Base::Result<void> released =
        ReleaseRecycledContainers();
    if (!released && firstError.IsOk()) {
        firstError = released.GetStatus();
    }
    if (virtualizingHost_ != nullptr) {
        virtualizingHost_->DetachGenerator(*facade_);
    }
    owner_->generator_ = nullptr;
    owner_ = nullptr;
    host_ = nullptr;
    virtualizingHost_ = nullptr;
    firstGeneratedIndex_ = 0U;
    return firstError.IsOk()
        ? Base::Result<bool>(true)
        : Base::Result<bool>(firstError);
}

Base::Result<ItemContainerGeneratorRuntime::Record>
ItemContainerGeneratorRuntime::CreateRecord(
    std::uint32_t index) noexcept {
    if (owner_ == nullptr ||
        index >= owner_->GetCount()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Item generation index is out of range");
    }
    Record record;
    record.item = owner_->GetItem(index);
    if (!record.item) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemsSource returned null");
    }
    if (owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.item->RuntimeType(),
            FrameworkElement::StaticTypeId())) {
        record.container =
            Base::Ref<FrameworkElement>::FromBorrowed(
                *static_cast<FrameworkElement*>(
                    record.item.Get()));
        record.itemIsOwnContainer = true;
        return record;
    }
    const DataTemplate* itemTemplate =
        owner_->GetItemTemplate();
    if (itemTemplate != nullptr) {
        Base::Result<Base::Ref<Base::Object>>
            content =
                DataTemplateRuntime::Instantiate(
                    *itemTemplate, record.item,
                    Aero::Core::GetFacet<::Aero::BindingEngine>(*owner_));
        if (!content) return content.GetStatus();
        record.content =
            std::move(content).Value();
    } else if (!owner_->GetDisplayMemberPath().Empty()) {
        Aero::BindingEngine* bindings =
            Aero::Core::GetFacet<::Aero::BindingEngine>(*owner_);
        Meta::Registry* metadata =
            bindings != nullptr ? bindings->Metadata() : nullptr;
        if (metadata == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "DisplayMemberPath metadata services are unavailable");
        }
        const Meta::PropertyInfo* property =
            metadata->Types().FindProperty(
                record.item->RuntimeType(),
                owner_->GetDisplayMemberPath(),
                true);
        if (property == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "DisplayMemberPath property was not found on the item type");
        }
        Base::Result<Meta::Value> value =
            metadata->GetProperty(
                *record.item, property->Id());
        if (!value) return value.GetStatus();
        Base::String display;
        Base::Result<void> formatted;
        switch (value.Value().Kind()) {
        case Meta::ValueKind::String:
            formatted = display.Assign(value.Value().AsString());
            break;
        case Meta::ValueKind::Boolean:
            formatted = display.Assign(
                value.Value().AsBoolean()
                    ? Base::StringView("True")
                    : Base::StringView("False"));
            break;
        case Meta::ValueKind::SignedInteger: {
            char buffer[48]{};
            const int length = std::snprintf(
                buffer, sizeof(buffer), "%lld",
                static_cast<long long>(value.Value().AsSignedInteger()));
            formatted = length > 0
                ? display.Assign(Base::StringView(
                      buffer, static_cast<std::uint32_t>(length)))
                : Base::Result<void>(Base::Status::Failure(
                      Base::ErrorCode::ValidationFailed,
                      "DisplayMemberPath integer formatting failed"));
            break;
        }
        case Meta::ValueKind::UnsignedInteger: {
            char buffer[48]{};
            const int length = std::snprintf(
                buffer, sizeof(buffer), "%llu",
                static_cast<unsigned long long>(
                    value.Value().AsUnsignedInteger()));
            formatted = length > 0
                ? display.Assign(Base::StringView(
                      buffer, static_cast<std::uint32_t>(length)))
                : Base::Result<void>(Base::Status::Failure(
                      Base::ErrorCode::ValidationFailed,
                      "DisplayMemberPath unsigned formatting failed"));
            break;
        }
        case Meta::ValueKind::Double: {
            char buffer[64]{};
            const int length = std::snprintf(
                buffer, sizeof(buffer), "%.15g", value.Value().AsDouble());
            formatted = length > 0
                ? display.Assign(Base::StringView(
                      buffer, static_cast<std::uint32_t>(length)))
                : Base::Result<void>(Base::Status::Failure(
                      Base::ErrorCode::ValidationFailed,
                      "DisplayMemberPath numeric formatting failed"));
            break;
        }
        default:
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "DisplayMemberPath value has no text representation");
        }
        if (!formatted) return formatted.GetStatus();
        Base::Result<Base::Ref<TextBlock>> text =
            Base::MakeRef<TextBlock>();
        if (!text) return text.GetStatus();
        text.Value()->SetText(display.View());
        record.content = Base::Ref<Base::Object>(
            std::move(text).Value());
        record.generatedTextContent = true;
    } else if (
        record.item->RuntimeType() ==
            ::Aero::Controls::BoxedItemValue::StaticTypeId()) {
        const Meta::Value& value =
            static_cast<const ::Aero::Controls::BoxedItemValue&>(
                *record.item).Value();
        Base::String display;
        Base::Result<void> formatted;
        switch (value.Kind()) {
        case Meta::ValueKind::String:
            formatted = display.Assign(value.AsString());
            break;
        case Meta::ValueKind::Boolean:
            formatted = display.Assign(
                value.AsBoolean() ? Base::StringView("True")
                                  : Base::StringView("False"));
            break;
        case Meta::ValueKind::SignedInteger: {
            char buffer[48]{};
            const int length = std::snprintf(
                buffer, sizeof(buffer), "%lld",
                static_cast<long long>(value.AsSignedInteger()));
            formatted = length > 0
                ? display.Assign(Base::StringView(
                      buffer, static_cast<std::uint32_t>(length)))
                : Base::Result<void>(Base::Status::Failure(
                      Base::ErrorCode::ValidationFailed,
                      "Boxed integer item formatting failed"));
            break;
        }
        case Meta::ValueKind::UnsignedInteger: {
            char buffer[48]{};
            const int length = std::snprintf(
                buffer, sizeof(buffer), "%llu",
                static_cast<unsigned long long>(value.AsUnsignedInteger()));
            formatted = length > 0
                ? display.Assign(Base::StringView(
                      buffer, static_cast<std::uint32_t>(length)))
                : Base::Result<void>(Base::Status::Failure(
                      Base::ErrorCode::ValidationFailed,
                      "Boxed unsigned item formatting failed"));
            break;
        }
        case Meta::ValueKind::Double: {
            char buffer[64]{};
            const int length = std::snprintf(
                buffer, sizeof(buffer), "%.15g", value.AsDouble());
            formatted = length > 0
                ? display.Assign(Base::StringView(
                      buffer, static_cast<std::uint32_t>(length)))
                : Base::Result<void>(Base::Status::Failure(
                      Base::ErrorCode::ValidationFailed,
                      "Boxed numeric item formatting failed"));
            break;
        }
        default:
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Boxed data item has no default text representation");
        }
        if (!formatted) return formatted.GetStatus();
        Base::Result<Base::Ref<TextBlock>> text =
            Base::MakeRef<TextBlock>();
        if (!text) return text.GetStatus();
        text.Value()->SetText(display.View());
        record.content =
            Base::Ref<Base::Object>(
                std::move(text).Value());
        record.generatedTextContent = true;
    } else if (owner_->PropertyRegistry().Types()
        .IsDerivedFrom(
            record.item->RuntimeType(),
            UIElement::StaticTypeId())) {
        record.content = record.item;
    } else if (owner_->PropertyRegistry().Types().IsDerivedFrom(
                   owner_->RuntimeType(),
                   ListView::StaticTypeId()) &&
               static_cast<ListView*>(owner_)->GetView()) {
        // GridView presents the data item through GridViewRowPresenter and
        // column bindings. Keep a zero-content visual for the container's
        // ordinary ContentControl contract while DataContext carries the item.
        Base::Result<Base::Ref<TextBlock>> placeholder =
            Base::MakeRef<TextBlock>();
        if (!placeholder) return placeholder.GetStatus();
        record.content = Base::Ref<Base::Object>(
            std::move(placeholder).Value());
        record.generatedTextContent = true;
    } else {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Data items require an ItemTemplate");
    }
    if (!record.content ||
        !owner_->PropertyRegistry().Types()
            .IsDerivedFrom(
                record.content->RuntimeType(),
                UIElement::StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemTemplate must create a UIElement");
    }
    if (!recycledContainers_.Empty()) {
        record.container =
            std::move(recycledContainers_.Back());
        recycledContainers_.PopBack();
        ++recycledContainerUseCount_;
    } else {
        Base::Result<Base::Ref<FrameworkElement>> made =
            owner_->CreateContainer(record.item);
        if (!made || !made.Value()) {
            if (!made) return made.GetStatus();
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "ItemsControl returned null container");
        }
        record.container =
            std::move(made).Value();
        ++createdContainerCount_;
    }

    return record;
}

Base::Result<void>
ItemContainerGeneratorRuntime::AttachOwnedSubtree(
    Record& record,
    Aero::Media::Visual& root) noexcept {
    Base::Vector<Aero::Media::Visual*> pending;
    Base::Result<void> pushed =
        pending.PushBack(&root);
    if (!pushed) return pushed.GetStatus();

    const auto attachChild =
        [this, &record, &pending](
            Aero::Media::Visual& parent,
            const Base::Ref<Base::Object>& owned)
            noexcept -> Base::Result<void> {
        if (!owned ||
            !owner_->PropertyRegistry().Types().
                IsDerivedFrom(
                    owned->RuntimeType(),
                    UIElement::StaticTypeId())) {
            return {};
        }
        auto& child =
            *static_cast<Aero::Media::Visual*>(
                owned.Get());
        if (child.GetVisualParent() == &parent &&
            child.GetTree() == tree_) {
            return pending.PushBack(&child);
        }
        if (child.GetVisualParent() != nullptr ||
            child.GetLogicalParent() != nullptr ||
            child.GetTree() != nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Owned item-template child is already mounted elsewhere");
        }
        Base::Result<ElementAttachment> mounted =
            tree_->AttachElement(parent, child);
        if (!mounted) return mounted.GetStatus();
        ElementAttachment edge =
            std::move(mounted).Value();
        Base::Result<void> tracked =
            record.subtreeMounts.PushBack(
                std::move(edge));
        if (!tracked) {
            (void)tree_->DetachElement(edge);
            return tracked.GetStatus();
        }
        Base::Result<void> queued =
            pending.PushBack(&child);
        if (!queued) {
            ElementAttachment rollback =
                std::move(record.subtreeMounts.Back());
            record.subtreeMounts.PopBack();
            (void)tree_->DetachElement(rollback);
            return queued.GetStatus();
        }
        return {};
    };

    while (!pending.Empty()) {
        Aero::Media::Visual* current =
            pending.Back();
        pending.PopBack();
        if (current == nullptr) continue;
        const Meta::TypeId type =
            current->RuntimeType();
        if (owner_->PropertyRegistry().Types().
                IsDerivedFrom(
                    type, Panel::StaticTypeId())) {
            auto& panel =
                *static_cast<Panel*>(current);
            for (std::uint32_t index = 0U;
                 index < ::Aero::Core::VisualFacet::PanelChildCount(panel);
                 ++index) {
                Base::Result<void> attached =
                    attachChild(
                        panel,
                        ::Aero::Core::VisualFacet::PanelChildAt(panel, index));
                if (!attached) {
                    (void)DetachOwnedSubtree(record);
                    return attached.GetStatus();
                }
            }
        } else if (owner_->PropertyRegistry().Types().
                       IsDerivedFrom(
                           type,
                           Decorator::StaticTypeId())) {
            auto& decorator =
                *static_cast<Decorator*>(current);
            Base::Result<void> attached =
                attachChild(
                    decorator,
                    ::Aero::Core::VisualFacet::DecoratorOwnedChild(decorator));
            if (!attached) {
                (void)DetachOwnedSubtree(record);
                return attached.GetStatus();
            }
        } else if (owner_->PropertyRegistry().Types().
                       IsDerivedFrom(
                           type,
                           ContentControl::StaticTypeId())) {
            auto& content =
                *static_cast<ContentControl*>(current);
            Base::Result<void> attached =
                attachChild(
                    content,
                    ::Aero::Core::InteractionStateFacet::OwnedContent(content));
            if (!attached) {
                (void)DetachOwnedSubtree(record);
                return attached.GetStatus();
            }
        } else if (owner_->PropertyRegistry().Types().
                       IsDerivedFrom(
                           type,
                           ContentPresenter::StaticTypeId())) {
            auto& presenter =
                *static_cast<ContentPresenter*>(current);
            Base::Result<void> attached =
                attachChild(
                    presenter,
                    presenter.GetOwnedContent());
            if (!attached) {
                (void)DetachOwnedSubtree(record);
                return attached.GetStatus();
            }
        }
    }
    return {};
}

Base::Result<void>
ItemContainerGeneratorRuntime::DetachOwnedSubtree(
    Record& record) noexcept {
    Base::Status firstError;
    for (std::uint32_t index =
             record.subtreeMounts.Size();
         index > 0U; --index) {
        Base::Result<void> detached =
            tree_->DetachElement(
                record.subtreeMounts[index - 1U]);
        if (!detached && firstError.IsOk()) {
            firstError = detached.GetStatus();
        }
    }
    record.subtreeMounts.Clear();
    return firstError.IsOk()
        ? Base::Result<void>()
        : Base::Result<void>(firstError);
}

Base::Result<void>
ItemContainerGeneratorRuntime::AttachRecord(
    Record& record,
    std::uint32_t index) noexcept {
    FrameworkElement& container = *record.container;

    Base::Result<ElementAttachment> containerMounted =
        tree_->AttachElement(*owner_, *host_, container);
    if (!containerMounted) return containerMounted.GetStatus();
    record.containerMount = std::move(containerMounted).Value();

    // ItemContainerStyle must be present before activating the generated
    // subtree. Activation resolves implicit styles and materializes the
    // control template; applying the explicit container style afterwards
    // leaves the already-instantiated implicit template in place.
    const Style* style = owner_->GetItemContainerStyle();
    if (style != nullptr && styles_ != nullptr) {
        Base::Result<Base::Ref<Style>> retained =
            owner_->GetValue(ItemsControl::ItemContainerStyleProperty);
        if (!retained || !retained.Value()) {
            (void)tree_->DetachElement(record.containerMount);
            return retained
                ? Base::Status::Failure(
                      Base::ErrorCode::InvalidState,
                      "ItemContainerStyle is not retained")
                : retained.GetStatus();
        }
        container.SetValue(
            FrameworkElement::StyleProperty,
            std::move(retained).Value());
        record.appliedStyle = style;
    }

    ContentControl* contentControl = nullptr;
    HeaderedItemsControl* headeredItemsControl = nullptr;
    if (owner_->PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            ContentControl::StaticTypeId())) {
        contentControl = static_cast<ContentControl*>(&container);
    }
    if (owner_->PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            HeaderedItemsControl::StaticTypeId())) {
        headeredItemsControl = static_cast<HeaderedItemsControl*>(&container);
    }
    if (!record.itemIsOwnContainer && headeredItemsControl != nullptr &&
        record.content.Get() != nullptr &&
        owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.content->RuntimeType(), TextBlock::StaticTypeId())) {
        Base::Result<void> subscribed =
            static_cast<TextBlock*>(record.content.Get())
                ->AddValueChangedHandlerChecked(
                    TextBlock::TextProperty,
                    generatedHeaderChangedHandler_);
        if (!subscribed) {
            (void)tree_->DetachElement(record.containerMount);
            return subscribed.GetStatus();
        }
        record.generatedHeader = true;
        Base::Result<void> assigned = UpdateGeneratedHeader(record);
        if (!assigned) {
            static_cast<void>(
                static_cast<TextBlock*>(record.content.Get())
                    ->RemoveValueChangedHandler(
                        TextBlock::TextProperty,
                        generatedHeaderChangedHandler_));
            record.generatedHeader = false;
            (void)tree_->DetachElement(record.containerMount);
            return assigned.GetStatus();
        }
    } else if (!record.itemIsOwnContainer && contentControl != nullptr) {
        auto& content =
            *static_cast<UIElement*>(
                record.content.Get());
        Base::Result<ElementAttachment> contentMounted =
            tree_->AttachElement(container, content);
        if (!contentMounted) {
            (void)tree_->DetachElement(record.containerMount);
            return contentMounted.GetStatus();
        }
        record.contentMount =
            std::move(contentMounted).Value();

        Base::Result<void> selected =
            record.generatedTextContent
            ? ::Aero::Core::InteractionStateFacet::
                      SetGeneratedTextContent(
                      *contentControl, record.content, content)
            : ::Aero::Core::InteractionStateFacet::SetOwnedContent(*contentControl,
                  record.content, content);
        if (!selected) {
            (void)tree_->DetachElement(
                record.contentMount);
            (void)tree_->DetachElement(
                record.containerMount);
            return selected.GetStatus();
        }
    }
    // UI activation starts at the generated container. This is
    // required for implicit container styles and control templates (for
    // example ComboBoxItem), while traversal still reaches DataTemplate
    // content mounted beneath it.
    Aero::Media::Visual& subtree =
        static_cast<Aero::Media::Visual&>(container);
    Base::Result<void> subtreeAttached =
        AttachOwnedSubtree(record, subtree);
    if (!subtreeAttached) {
        (void)DetachRecord(record);
        return subtreeAttached.GetStatus();
    }
    Base::Result<void> prepared = owner_->PrepareContainer(container, record.item, index);
    if (!prepared) { (void)DetachRecord(record); return prepared.GetStatus(); }
    if (subtreeCallback_ != nullptr) {
        Base::Result<void> presented =
            subtreeCallback_(
                subtree,
                ItemSubtreeChange::Mounted,
                subtreeContext_);
        if (!presented) {
            (void)DetachRecord(record);
            return presented.GetStatus();
        }
        record.subtreeMounted = true;
    }
    if (record.generatedHeader) {
        Base::Result<void> synchronized = UpdateGeneratedHeader(record);
        if (!synchronized) {
            (void)DetachRecord(record);
            return synchronized.GetStatus();
        }
    }
    return {};
}

Base::Result<void>
ItemContainerGeneratorRuntime::UpdateGeneratedHeader(
    Record& record) noexcept {
    if (!record.generatedHeader || !record.content || !record.container ||
        !owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.content->RuntimeType(), TextBlock::StaticTypeId()) ||
        !owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.container->RuntimeType(),
            HeaderedItemsControl::StaticTypeId())) {
        return {};
    }
    const Base::StringView text =
        static_cast<TextBlock*>(record.content.Get())->GetText();
    if (owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.container->RuntimeType(),
            TreeViewItem::StaticTypeId())) {
        return static_cast<TreeViewItem*>(record.container.Get())
            ->SetHeader(text);
    }
    return static_cast<HeaderedItemsControl*>(record.container.Get())
        ->SetHeader(text);
}

void ItemContainerGeneratorRuntime::OnGeneratedHeaderChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept {
    for (Record& record : records_) {
        if (record.generatedHeader && record.content.Get() == &object) {
            Base::Result<void> synchronized = UpdateGeneratedHeader(record);
            if (!synchronized) lastError_ = synchronized.GetStatus();
            return;
        }
    }
}

Base::Result<void>
ItemContainerGeneratorRuntime::DetachRecord(
    Record& record,
    bool recycleContainer) noexcept {
    if (!record.container) return {};
    FrameworkElement& container = *record.container;
    ContentControl* contentControl = nullptr;
    HeaderedItemsControl* headeredItemsControl = nullptr;
    if (owner_->PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            ContentControl::StaticTypeId())) {
        contentControl = static_cast<ContentControl*>(&container);
    }
    if (owner_->PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            HeaderedItemsControl::StaticTypeId())) {
        headeredItemsControl = static_cast<HeaderedItemsControl*>(&container);
    }
    Base::Status firstError;
    const auto capture = [&firstError](const Base::Result<void>& result) noexcept {
        if (!result && firstError.IsOk()) firstError = result.GetStatus();
    };
    if (record.subtreeMounted &&
        subtreeCallback_ != nullptr) {
        Aero::Media::Visual& subtree =
            static_cast<Aero::Media::Visual&>(
                container);
        capture(subtreeCallback_(
            subtree,
            ItemSubtreeChange::Unmounting,
            subtreeContext_));
        record.subtreeMounted = false;
    }
    // Headered item containers consume a DataTemplate TextBlock as a string
    // header instead of mounting it below the container. Its bindings are
    // nevertheless activated when the template is instantiated, so it needs
    // an explicit teardown because the container subtree walk cannot reach it.
    if (record.generatedHeader && record.content &&
        subtreeCallback_ != nullptr &&
        owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.content->RuntimeType(), UIElement::StaticTypeId())) {
        capture(subtreeCallback_(
            *static_cast<Aero::Media::Visual*>(record.content.Get()),
            ItemSubtreeChange::Unmounting,
            subtreeContext_));
    }
    capture(DetachOwnedSubtree(record));
    owner_->ClearContainer(container);
    if (record.generatedHeader && headeredItemsControl != nullptr) {
        if (record.content && owner_->PropertyRegistry().Types().IsDerivedFrom(
                record.content->RuntimeType(), TextBlock::StaticTypeId())) {
            static_cast<void>(
                static_cast<TextBlock*>(record.content.Get())
                    ->RemoveValueChangedHandler(
                        TextBlock::TextProperty,
                        generatedHeaderChangedHandler_));
        }
        const auto clearHeader = [&]() noexcept {
            if (owner_->PropertyRegistry().Types().IsDerivedFrom(
                    container.RuntimeType(), TreeViewItem::StaticTypeId())) {
                static_cast<TreeViewItem&>(container).SetHeader(
                    Value::NullObject(Meta::TypeOf<Base::Object>()));
                return;
            }
            headeredItemsControl->SetHeader(
                Value::NullObject(Meta::TypeOf<Base::Object>()));
        };
        clearHeader();
        record.generatedHeader = false;
    }
    if (record.appliedStyle != nullptr && styles_ != nullptr) {
        capture(styles_->Clear(container, *record.appliedStyle));
        container.ClearValue(FrameworkElement::StyleProperty);
        record.appliedStyle = nullptr;
    }
    if (!record.itemIsOwnContainer &&
        templates_ != nullptr &&
        owner_->PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(), Control::StaticTypeId()) &&
        ::Aero::Core::InteractionStateFacet::IsTemplateApplied(
            static_cast<Control&>(container))) {
        Base::Result<bool> cleared =
            templates_->Clear(static_cast<Control&>(container));
        if (!cleared) {
            capture(Base::Result<void>(
                cleared.GetStatus()));
        }
    }
    UIElement* content = nullptr;
    if (!record.itemIsOwnContainer && contentControl != nullptr) {
        content = ::Aero::Core::InteractionStateFacet::ContentElement(
            *contentControl);
    }
    if (content != nullptr) {
        capture(tree_->DetachElement(record.contentMount));
        contentControl->SetContent(nullptr);
        capture(values_->DetachObject(*content));
    }
    capture(tree_->DetachElement(record.containerMount));
    if (!record.itemIsOwnContainer &&
        recycleContainer && firstError.IsOk()) {
        Base::Result<void> recycled = recycledContainers_.PushBack(std::move(record.container));
        if (!recycled) {
            capture(values_->DetachObject(container));
            if (firstError.IsOk()) firstError = recycled.GetStatus();
        }
    } else {
        capture(values_->DetachObject(container));
    }
    record.item.Reset();
    record.content.Reset();
    record.containerMount = {};
    record.contentMount = {};
    record.itemIsOwnContainer = false;
    record.generatedTextContent = false;
    record.generatedHeader = false;
    record.subtreeMounted = false;
    return firstError.IsOk() ? Base::Result<void>() : Base::Result<void>(firstError);
}

Base::Result<void>
ItemContainerGeneratorRuntime::ReleaseRecycledContainers() noexcept {
    Base::Status firstError;
    for (Base::Ref<FrameworkElement>& container :
        recycledContainers_) {
        if (!container) continue;
        Base::Result<void> detached =
            values_->DetachObject(*container);
        if (!detached && firstError.IsOk()) {
            firstError = detached.GetStatus();
        }
    }
    recycledContainers_.Clear();
    return firstError.IsOk()
        ? Base::Result<void>()
        : Base::Result<void>(firstError);
}

Base::Result<void>
ItemContainerGeneratorRuntime::InsertRecord(
    std::uint32_t index,
    Record record) noexcept {
    if (index > records_.Size()) {
        static_cast<void>(DetachRecord(record));
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Generated container insert is out of range");
    }
    Base::Result<void> reserved =
        records_.Reserve(records_.Size() + 1U);
    if (!reserved) {
        const Base::Status error = reserved.GetStatus();
        static_cast<void>(DetachRecord(record));
        return error;
    }
    Base::Result<void> appended =
        records_.PushBack(std::move(record));
    if (!appended) {
        const Base::Status error = appended.GetStatus();
        static_cast<void>(DetachRecord(record));
        return error;
    }
    if (index + 1U == records_.Size()) return {};
    Record moving = std::move(records_.Back());
    for (std::uint32_t current =
            records_.Size() - 1U;
        current > index; --current) {
        records_[current] =
            std::move(records_[current - 1U]);
    }
    records_[index] = std::move(moving);
    return {};
}

void ItemContainerGeneratorRuntime::RemoveRecordAt(
    std::uint32_t index) noexcept {
    for (std::uint32_t current = index;
        current + 1U < records_.Size(); ++current) {
        records_[current] =
            std::move(records_[current + 1U]);
    }
    records_.PopBack();
}

Base::Result<void>
ItemContainerGeneratorRuntime::ReorderVisuals() noexcept {
    for (Record& record : records_) {
        Base::Result<void> detached = tree_->DetachVisual(record.containerMount);
        if (!detached) return detached.GetStatus();
    }
    for (Record& record : records_) {
        Base::Result<void> attached = tree_->AttachVisual(record.containerMount, *host_);
        if (!attached) return attached.GetStatus();
    }
    return {};
}

Base::Result<bool>
ItemContainerGeneratorRuntime::SetRealizationRangeInternal(
    std::uint32_t firstIndex,
    std::uint32_t count,
    bool force) noexcept {
    if (owner_ == nullptr ||
        host_ == nullptr ||
        virtualizingHost_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Item realization range requires a virtualizing host");
    }
    const std::uint32_t itemCount =
        owner_->GetCount();
    firstIndex = std::min(firstIndex, itemCount);
    count = std::min(count, itemCount - firstIndex);
    fprintf(stderr,
        "[REALIZE] first=%u count=%u itemCount=%u force=%d\n",
        firstIndex, count, itemCount, (int)force);
    if (!force &&
        firstGeneratedIndex_ == firstIndex &&
        records_.Size() == count) {
        return false;
    }

    Base::Status firstError;
    for (std::uint32_t index = records_.Size();
        index > 0U; --index) {
        Base::Result<void> detached =
            DetachRecord(records_[index - 1U], true);
        if (!detached && firstError.IsOk()) {
            firstError = detached.GetStatus();
        }
    }
    records_.Clear();
    firstGeneratedIndex_ = firstIndex;
    if (!firstError.IsOk()) {
        return firstError;
    }

    Base::Result<void> reserved =
        records_.Reserve(count);
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t offset = 0U;
        offset < count; ++offset) {
        const std::uint32_t index =
            firstIndex + offset;
        Base::Result<Record> made =
            CreateRecord(index);
        if (!made) {
            firstError = made.GetStatus();
            break;
        }
        Record record = std::move(made).Value();
        Base::Result<void> attached =
            AttachRecord(record, index);
        if (!attached) {
            firstError = attached.GetStatus();
            break;
        }
        Base::Result<void> added =
            records_.PushBack(std::move(record));
        if (!added) {
            firstError = added.GetStatus();
            static_cast<void>(
                DetachRecord(record, true));
            break;
        }
    }
    if (!firstError.IsOk()) {
        for (std::uint32_t index = records_.Size();
            index > 0U; --index) {
            static_cast<void>(
                DetachRecord(
                    records_[index - 1U], true));
        }
        records_.Clear();
        return firstError;
    }
    return true;
}

Base::Result<bool>
ItemContainerGeneratorRuntime::SetRealizationRange(
    std::uint32_t firstIndex,
    std::uint32_t count) noexcept {
    Base::Result<bool> changed =
        SetRealizationRangeInternal(
            firstIndex, count, false);
    lastError_ = changed
        ? Base::Status{}
        : changed.GetStatus();
    if (changed && changed.Value() &&
        owner_ != nullptr) {
        owner_->OnContainersChanged();
    }
    return changed;
}

Base::Result<void> ItemContainerGeneratorRuntime::Refresh() noexcept {
    if (owner_ == nullptr || host_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemContainerGenerator is not attached");
    }
    if (virtualizingHost_ != nullptr) {
        Base::Result<bool> realized =
            SetRealizationRangeInternal(
                virtualizingHost_->desiredFirstIndex_,
                virtualizingHost_->desiredCount_,
                true);
        return realized
            ? Base::Result<void>()
            : Base::Result<void>(
                realized.GetStatus());
    }
    firstGeneratedIndex_ = 0U;
    for (std::uint32_t index = records_.Size();
        index > 0U; --index) {
        Base::Result<void> detached =
            DetachRecord(records_[index - 1U]);
        if (!detached) return detached.GetStatus();
    }
    records_.Clear();
    Base::Result<void> reserved =
        records_.Reserve(owner_->GetCount());
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t index = 0U;
        index < owner_->GetCount(); ++index) {
        Base::Result<Record> made =
            CreateRecord(index);
        if (!made) {
            const Base::Status error = made.GetStatus();
            for (std::uint32_t cleanup = records_.Size();
                cleanup > 0U; --cleanup) {
                static_cast<void>(
                    DetachRecord(records_[cleanup - 1U]));
            }
            records_.Clear();
            return error;
        }
        Record record = std::move(made).Value();
        Base::Result<void> attached =
            AttachRecord(record, index);
        if (!attached) {
            const Base::Status error = attached.GetStatus();
            for (std::uint32_t cleanup = records_.Size();
                cleanup > 0U; --cleanup) {
                static_cast<void>(
                    DetachRecord(records_[cleanup - 1U]));
            }
            records_.Clear();
            return error;
        }
        Base::Result<void> added =
            records_.PushBack(
                std::move(record));
        if (!added) {
            const Base::Status error = added.GetStatus();
            static_cast<void>(DetachRecord(record));
            for (std::uint32_t cleanup = records_.Size();
                cleanup > 0U; --cleanup) {
                static_cast<void>(
                    DetachRecord(records_[cleanup - 1U]));
            }
            records_.Clear();
            return error;
        }
    }
    fprintf(stderr,
        "[REFRESH] non-virt records=%u hostChildren=%u host=%p\n",
        records_.Size(),
        host_ ? Core::VisualFacet::PanelChildCount(*host_) : 0U,
        (void*)host_);
    return {};
}

Base::Result<void> ItemContainerGeneratorRuntime::ApplyChange(
    const ItemsChangedEvent& event) noexcept {
    if (event.action == ItemsChangeAction::Reset) {
        return Refresh();
    }
    if (event.action == ItemsChangeAction::Add) {
        for (std::uint32_t offset = 0U;
            offset < event.newCount; ++offset) {
            const std::uint32_t index =
                event.newIndex + offset;
            Base::Result<Record> made =
                CreateRecord(index);
            if (!made) return made.GetStatus();
            Record record = std::move(made).Value();
            Base::Result<void> attached =
                AttachRecord(record, index);
            if (!attached) return attached.GetStatus();
            Base::Result<void> inserted =
                InsertRecord(
                    index, std::move(record));
            if (!inserted) {
                return inserted.GetStatus();
            }
        }
        return ReorderVisuals();
    }
    if (event.action == ItemsChangeAction::Remove) {
        if (event.oldIndex > records_.Size() ||
            event.oldCount >
                records_.Size() - event.oldIndex) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Items removal delta is invalid");
        }
        for (std::uint32_t count = 0U;
            count < event.oldCount; ++count) {
            Base::Result<void> detached =
                DetachRecord(
                    records_[event.oldIndex]);
            if (!detached) return detached.GetStatus();
            RemoveRecordAt(event.oldIndex);
        }
        return ReorderVisuals();
    }
    if (event.action == ItemsChangeAction::Replace) {
        if (event.oldIndex > records_.Size() ||
            event.oldCount >
                records_.Size() - event.oldIndex) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Items replacement delta is invalid");
        }
        for (std::uint32_t count = 0U;
            count < event.oldCount; ++count) {
            Base::Result<void> detached =
                DetachRecord(
                    records_[event.oldIndex]);
            if (!detached) return detached.GetStatus();
            RemoveRecordAt(event.oldIndex);
        }
        for (std::uint32_t offset = 0U;
            offset < event.newCount; ++offset) {
            const std::uint32_t index =
                event.newIndex + offset;
            Base::Result<Record> made =
                CreateRecord(index);
            if (!made) return made.GetStatus();
            Record record = std::move(made).Value();
            Base::Result<void> attached =
                AttachRecord(record, index);
            if (!attached) return attached.GetStatus();
            Base::Result<void> inserted =
                InsertRecord(
                    index, std::move(record));
            if (!inserted) {
                return inserted.GetStatus();
            }
        }
        return ReorderVisuals();
    }
    if (event.action == ItemsChangeAction::Move &&
        event.oldCount == 1U &&
        event.newCount == 1U &&
        event.oldIndex < records_.Size() &&
        event.newIndex < records_.Size()) {
        Record moving =
            std::move(records_[event.oldIndex]);
        if (event.oldIndex < event.newIndex) {
            for (std::uint32_t index = event.oldIndex;
                index < event.newIndex; ++index) {
                records_[index] =
                    std::move(records_[index + 1U]);
            }
        } else {
            for (std::uint32_t index = event.oldIndex;
                index > event.newIndex; --index) {
                records_[index] =
                    std::move(records_[index - 1U]);
            }
        }
        records_[event.newIndex] =
            std::move(moving);
        return ReorderVisuals();
    }
    return Refresh();
}

void ItemContainerGeneratorRuntime::OnItemsChanged(
    const ItemsChangedEvent& event) noexcept {
    Base::Result<void> applied;
    fprintf(stderr,
        "[GEN] OnItemsChanged action=%d vhost=%p ownerCount=%u\n",
        (int)event.action, (void*)virtualizingHost_,
        owner_ ? owner_->GetCount() : 0U);
    if (virtualizingHost_ != nullptr &&
        owner_ != nullptr) {
        applied =
            virtualizingHost_->HandleItemsChanged(
                event, owner_->GetCount());
        if (applied) {
            Base::Result<bool> realized =
                SetRealizationRangeInternal(
                    virtualizingHost_->
                        desiredFirstIndex_,
                    virtualizingHost_->
                        desiredCount_,
                    true);
            if (!realized) {
                applied = realized.GetStatus();
            }
        }
    } else {
        applied = ApplyChange(event);
    }
    lastError_ = applied
        ? Base::Status{}
        : applied.GetStatus();
    if (applied && owner_ != nullptr) {
        owner_->OnContainersChanged();
    }
}

FrameworkElement*
ItemContainerGeneratorRuntime::ContainerFromIndex(
    std::uint32_t index) const noexcept {
    return index >= firstGeneratedIndex_ &&
        index - firstGeneratedIndex_ <
            records_.Size()
        ? records_[
            index - firstGeneratedIndex_]
              .container.Get()
        : nullptr;
}

std::uint32_t
ItemContainerGeneratorRuntime::IndexFromContainer(
    const FrameworkElement& container) const noexcept {
    for (std::uint32_t index = 0U;
        index < records_.Size(); ++index) {
        if (records_[index].container.Get() ==
            &container) {
            return firstGeneratedIndex_ + index;
        }
    }
    return UINT32_MAX;
}

Base::Ref<Base::Object>
ItemContainerGeneratorRuntime::ItemFromContainer(
    const FrameworkElement& container) const noexcept {
    const std::uint32_t index =
        IndexFromContainer(container);
    return index != UINT32_MAX
        ? records_[
            index - firstGeneratedIndex_].item
        : Base::Ref<Base::Object>();
}

} // namespace Aero::Controls

namespace Aero::Controls {

ItemContainerGenerator::~ItemContainerGenerator() noexcept {
    delete static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    impl_ = nullptr;
}

Base::Result<void> ItemContainerGenerator::Attach(
    ItemsControl& owner,
    Panel& itemsHost) noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr
        ? runtime->Attach(owner, itemsHost)
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::NotInitialized,
              "ItemContainerGenerator is not initialized"));
}

Base::Result<void> ItemContainerGenerator::AttachVirtualized(
    ItemsControl& owner,
    VirtualizingStackPanel& itemsHost) noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr
        ? runtime->AttachVirtualized(owner, itemsHost)
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::NotInitialized,
              "ItemContainerGenerator is not initialized"));
}

Base::Result<bool> ItemContainerGenerator::Detach() noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->Detach() : Base::Result<bool>(false);
}

Base::Result<void> ItemContainerGenerator::Refresh() noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr
        ? runtime->Refresh()
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::NotInitialized,
              "ItemContainerGenerator is not initialized"));
}

void ItemContainerGenerator::SetRealizationRange(
    std::uint32_t firstIndex,
    std::uint32_t count) noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    if (runtime != nullptr) (void)runtime->SetRealizationRange(firstIndex, count);
}

std::uint32_t ItemContainerGenerator::GetGeneratedCount() const noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->GetGeneratedCount() : 0U;
}

std::uint32_t ItemContainerGenerator::GetFirstGeneratedIndex() const noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->GetFirstGeneratedIndex() : 0U;
}

std::uint32_t ItemContainerGenerator::GetCreatedContainerCount() const noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->GetCreatedContainerCount() : 0U;
}

std::uint32_t ItemContainerGenerator::GetRecycledContainerUseCount() const noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->GetRecycledContainerUseCount() : 0U;
}

FrameworkElement* ItemContainerGenerator::ContainerFromIndex(
    std::uint32_t index) const noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->ContainerFromIndex(index) : nullptr;
}

std::uint32_t ItemContainerGenerator::IndexFromContainer(
    const FrameworkElement& container) const noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->IndexFromContainer(container) : UINT32_MAX;
}

Base::Ref<Base::Object> ItemContainerGenerator::ItemFromContainer(
    const FrameworkElement& container) const noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr
        ? runtime->ItemFromContainer(container)
        : Base::Ref<Base::Object>{};
}

Base::Status ItemContainerGenerator::LastError() const noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr
        ? runtime->LastError()
        : Base::Status::Failure(
              Base::ErrorCode::NotInitialized,
              "ItemContainerGenerator is not initialized");
}

} // namespace Aero::Controls

namespace Aero::Core {

Base::Result<Controls::ItemContainerGenerator*>
InteractionStateFacet::CreateItemContainerGenerator(
    ElementTree& tree,
    Aero::LayoutEngine& layout,
    Meta::EffectiveValueEngine& values,
    Aero::StyleEngine* styles,
    ::Aero::Render::RenderTree* renderer,
    Controls::TemplateEngine* templates,
    Controls::ItemSubtreeCallback subtreeCallback,
    void* subtreeContext) noexcept {
    auto* generator = new (std::nothrow) Controls::ItemContainerGenerator();
    if (generator == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "ItemContainerGenerator allocation failed");
    }
    generator->impl_ = new (std::nothrow) Controls::ItemContainerGeneratorImpl(
        *generator,
        tree,
        layout,
        values,
        styles,
        renderer,
        templates,
        subtreeCallback,
        subtreeContext);
    if (generator->impl_ == nullptr) {
        delete generator;
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "ItemContainerGenerator runtime allocation failed");
    }
    return generator;
}

} // namespace Aero::Core



namespace Aero {

void DataTemplate::SetResources(Base::Ref<ResourceDictionary> value) noexcept {
    Controls::DataTemplateState* state = static_cast<Controls::DataTemplateState*>(state_);
    if (state == nullptr) return;
    (void)Aero::AssignResourceDictionary(state->resources, std::move(value), "DataTemplate Resources is already assigned");
}

void Controls::ItemsPanelTemplate::SetResources(Base::Ref<ResourceDictionary> value) noexcept {
    Controls::ItemsPanelTemplateState* state = static_cast<Controls::ItemsPanelTemplateState*>(state_);
    if (state == nullptr) return;
    (void)Aero::AssignResourceDictionary(state->resources, std::move(value), "ItemsPanelTemplate Resources is already assigned");
}

} // namespace Aero
