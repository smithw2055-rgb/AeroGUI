#include <Aero/Controls/Items.hpp>
#include "ContentControlAccess.hpp"
#include "ControlAccess.hpp"
#include "DeferredTemplateAccess.hpp"

#include "ItemContainerGeneratorAccess.hpp"
#include "render/RenderingInternal.hpp"
#include "../ui/ResourceAssignment.hpp"
#include "../ui/MountService.hpp"

#include "../core/metadata/BuiltinTypeIds.hpp"
#include <Aero/FrameworkElement.hpp>

#include <algorithm>
#include <new>
#include <utility>
#include "../ui/RuntimeManagers.hpp"
#include "RuntimeManagers.hpp"
#include "ControlCollections.hpp"

namespace Aero::Controls {

Base::Result<void> ItemsPresenter::SetItemsHost(
    const Base::Ref<Base::Object>& owner,
    Panel& panel) noexcept {
    return Detail::DecoratorAccess::SetOwnedChild(*this, owner, panel);
}


using namespace Aero::Detail;

ContentControl::ContentControl(
    TypeId runtimeType) noexcept
    : Control(runtimeType),
      foregroundChangedHandler_(
          this,
          &ContentControl::OnForegroundChanged) {
    static_cast<void>(TryAddValueChangedHandler(
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
    if (!literalTextContent_ ||
        content_ == nullptr ||
        !PropertyRegistry().Types().IsDerivedFrom(
            content_->RuntimeType(),
            TextBlock::StaticTypeId())) {
        return;
    }
    static_cast<void>(
        static_cast<TextBlock*>(content_)->
            SetForeground(Foreground()));
}

Base::Result<void> ContentControl::SetGeneratedTextContent(
    const Base::Ref<Base::Object>& contentObject,
    UIElement& content) noexcept {
    if (!PropertyRegistry().Types().IsDerivedFrom(
            content.RuntimeType(),
            TextBlock::StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Generated text content must be a TextBlock");
    }
    Base::Result<void> assigned =
        SetOwnedContent(contentObject, content);
    if (!assigned) return assigned.GetStatus();
    literalTextContent_ = true;
    return static_cast<TextBlock&>(content).
        SetForeground(Foreground());
}

Base::Ref<Base::Object> ItemsCollection::ItemAt(
    std::uint32_t index) const noexcept {
    return index < items_.Size()
        ? items_[index]
        : Base::Ref<Base::Object>();
}

void ItemsCollection::Notify(
    const ItemsChangedEvent& event) noexcept {
    if (!changed_.Empty()) changed_.Invoke(event);
}

Base::Result<void> ItemsCollection::Add(
    Base::Ref<Base::Object> item) noexcept {
    return Insert(items_.Size(), std::move(item));
}

Base::Result<void> ItemsCollection::Insert(
    std::uint32_t index,
    Base::Ref<Base::Object> item) noexcept {
    if (!item || index > items_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ItemsCollection insert is invalid");
    }
    Base::Result<void> reserved =
        items_.TryReserve(items_.Size() + 1U);
    if (!reserved) return reserved.GetStatus();
    Base::Result<void> added =
        items_.TryPushBack(std::move(item));
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
ItemsCollection::RemoveAt(
    std::uint32_t index) noexcept {
    if (index >= items_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "ItemsCollection remove index is out of range");
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

Base::Result<void> ItemsCollection::Replace(
    std::uint32_t index,
    Base::Ref<Base::Object> item) noexcept {
    if (!item || index >= items_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ItemsCollection replacement is invalid");
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

Base::Result<void> ItemsCollection::Move(
    std::uint32_t oldIndex,
    std::uint32_t newIndex) noexcept {
    if (oldIndex >= items_.Size() ||
        newIndex >= items_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "ItemsCollection move index is out of range");
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

void ItemsCollection::Reset() noexcept {
    const std::uint32_t oldCount = items_.Size();
    items_.Clear();
    Notify({
        ItemsChangeAction::Reset,
        0U,
        0U,
        oldCount,
        0U});
}

Base::Result<void> ItemsCollection::Reset(
    Base::Span<const Base::Ref<Base::Object>>
        items) noexcept {
    Base::Vector<Base::Ref<Base::Object>> replacement;
    Base::Result<void> reserved =
        replacement.TryReserve(items.Size());
    if (!reserved) return reserved.GetStatus();
    for (const Base::Ref<Base::Object>& item : items) {
        if (!item) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "ItemsCollection cannot contain null");
        }
        Base::Result<void> added =
            replacement.TryPushBack(item);
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

Base::Result<void> Detail::DeferredObjectProgram::Configure(
    DeferredObjectFactory valueFactory,
    void* valueContext) noexcept {
    return Configure(valueFactory, valueContext, {});
}

Base::Result<void> Detail::DeferredObjectProgram::Configure(
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

Base::Result<void> Detail::DeferredObjectProgram::SetBaseUri(
    const Base::ResourceUri& value) noexcept {
    if (sealed) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Deferred object program is sealed");
    }
    baseUri = value;
    return {};
}

Base::Result<void> Detail::DeferredObjectProgram::Seal() noexcept {
    if (factory == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Deferred object program has no factory");
    }
    sealed = true;
    return {};
}

Base::Result<Base::Ref<Base::Object>> Detail::DeferredObjectProgram::Instantiate(
    const Base::Ref<Base::Object>& payload) const noexcept {
    if (factory == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Deferred object program is not ready");
    }
    Base::Result<Base::Ref<Base::Object>> result = factory(payload, context);
    if (!result || result.Value()) return result;
    return Base::Status::Failure(Base::ErrorCode::InvalidState,
        "Deferred object factory returned null");
}

DataTemplate::DataTemplate() noexcept
    : state_(new (std::nothrow) Detail::DataTemplateState()) {
    if (state_ == nullptr) {
        Base::ReportOutOfMemory(sizeof(Detail::DataTemplateState), alignof(Detail::DataTemplateState), Base::MemoryTag::Ui);
    }
}

DataTemplate::~DataTemplate() noexcept {
    delete static_cast<Detail::DataTemplateState*>(state_);
    state_ = nullptr;
}

TypeId DataTemplate::GetDataType() const noexcept {
    const Detail::DataTemplateState* state = static_cast<const Detail::DataTemplateState*>(state_);
    return state != nullptr ? state->dataType : InvalidTypeId;
}

Base::Result<void> DataTemplate::SetDataType(TypeId value) noexcept {
    Detail::DataTemplateState* state = static_cast<Detail::DataTemplateState*>(state_);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    if (state->program.sealed || value == InvalidTypeId) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "DataTemplate DataType is invalid");
    }
    state->dataType = value;
    return {};
}

Base::Ref<Base::Object> DataTemplate::GetHierarchicalItemsSource() const noexcept {
    const Detail::DataTemplateState* state = static_cast<const Detail::DataTemplateState*>(state_);
    return state != nullptr ? state->hierarchicalItemsSource : Base::Ref<Base::Object>{};
}

void DataTemplate::SetHierarchicalItemsSource(Base::Ref<Base::Object> value) noexcept {
    Detail::DataTemplateState* state = static_cast<Detail::DataTemplateState*>(state_);
    if (state != nullptr) state->hierarchicalItemsSource = std::move(value);
}

Base::Ref<Base::Object> DataTemplate::GetHierarchicalItemTemplate() const noexcept {
    const Detail::DataTemplateState* state = static_cast<const Detail::DataTemplateState*>(state_);
    return state != nullptr ? state->hierarchicalItemTemplate : Base::Ref<Base::Object>{};
}

void DataTemplate::SetHierarchicalItemTemplate(Base::Ref<Base::Object> value) noexcept {
    Detail::DataTemplateState* state = static_cast<Detail::DataTemplateState*>(state_);
    if (state != nullptr) state->hierarchicalItemTemplate = std::move(value);
}

ResourceKey DataTemplate::GetImplicitKey() const noexcept {
    return ResourceKey::FromType(GetDataType());
}

ResourceDictionary& DataTemplate::GetResources() noexcept {
    Detail::DataTemplateState* state = static_cast<Detail::DataTemplateState*>(state_);
    if (state != nullptr) return state->resources;
    static ResourceDictionary fallback;
    return fallback;
}

const ResourceDictionary& DataTemplate::GetResources() const noexcept {
    const Detail::DataTemplateState* state = static_cast<const Detail::DataTemplateState*>(state_);
    if (state != nullptr) return state->resources;
    static ResourceDictionary fallback;
    return fallback;
}

bool DataTemplate::GetIsSealed() const noexcept {
    const Detail::DataTemplateState* state = static_cast<const Detail::DataTemplateState*>(state_);
    return state != nullptr && state->program.sealed;
}

ItemsPanelTemplate::ItemsPanelTemplate() noexcept
    : state_(new (std::nothrow) Detail::ItemsPanelTemplateState()) {
    if (state_ == nullptr) {
        Base::ReportOutOfMemory(sizeof(Detail::ItemsPanelTemplateState), alignof(Detail::ItemsPanelTemplateState), Base::MemoryTag::Ui);
    }
}

ItemsPanelTemplate::~ItemsPanelTemplate() noexcept {
    delete static_cast<Detail::ItemsPanelTemplateState*>(state_);
    state_ = nullptr;
}

ResourceDictionary& ItemsPanelTemplate::GetResources() noexcept {
    Detail::ItemsPanelTemplateState* state = static_cast<Detail::ItemsPanelTemplateState*>(state_);
    if (state != nullptr) return state->resources;
    static ResourceDictionary fallback;
    return fallback;
}

const ResourceDictionary& ItemsPanelTemplate::GetResources() const noexcept {
    const Detail::ItemsPanelTemplateState* state = static_cast<const Detail::ItemsPanelTemplateState*>(state_);
    if (state != nullptr) return state->resources;
    static ResourceDictionary fallback;
    return fallback;
}

bool ItemsPanelTemplate::GetIsSealed() const noexcept {
    const Detail::ItemsPanelTemplateState* state = static_cast<const Detail::ItemsPanelTemplateState*>(state_);
    return state != nullptr && state->program.sealed;
}

Detail::DataTemplateState* Detail::DeferredTemplateAccess::State(DataTemplate& value) noexcept {
    return static_cast<DataTemplateState*>(value.state_);
}

const Detail::DataTemplateState* Detail::DeferredTemplateAccess::State(const DataTemplate& value) noexcept {
    return static_cast<const DataTemplateState*>(value.state_);
}

Detail::ItemsPanelTemplateState* Detail::DeferredTemplateAccess::State(ItemsPanelTemplate& value) noexcept {
    return static_cast<ItemsPanelTemplateState*>(value.state_);
}

const Detail::ItemsPanelTemplateState* Detail::DeferredTemplateAccess::State(const ItemsPanelTemplate& value) noexcept {
    return static_cast<const ItemsPanelTemplateState*>(value.state_);
}

Base::Result<void> Detail::DeferredTemplateAccess::Configure(DataTemplate& value, DeferredObjectFactory factory, void* context, Base::Ref<Base::Object> owner) noexcept {
    DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    return state->program.Configure(factory, context, std::move(owner));
}

Base::Result<void> Detail::DeferredTemplateAccess::Configure(ItemsPanelTemplate& value, DeferredObjectFactory factory, void* context, Base::Ref<Base::Object> owner) noexcept {
    ItemsPanelTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "ItemsPanelTemplate state allocation failed");
    return state->program.Configure(factory, context, std::move(owner));
}

Base::Result<void> Detail::DeferredTemplateAccess::SetBaseUri(DataTemplate& value, const Base::ResourceUri& uri) noexcept {
    DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    return state->program.SetBaseUri(uri);
}

Base::Result<void> Detail::DeferredTemplateAccess::SetBaseUri(ItemsPanelTemplate& value, const Base::ResourceUri& uri) noexcept {
    ItemsPanelTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "ItemsPanelTemplate state allocation failed");
    return state->program.SetBaseUri(uri);
}

const Base::ResourceUri& Detail::DeferredTemplateAccess::BaseUri(const DataTemplate& value) noexcept {
    static Base::ResourceUri empty;
    const DataTemplateState* state = State(value);
    return state != nullptr ? state->program.baseUri : empty;
}

const Base::ResourceUri& Detail::DeferredTemplateAccess::BaseUri(const ItemsPanelTemplate& value) noexcept {
    static Base::ResourceUri empty;
    const ItemsPanelTemplateState* state = State(value);
    return state != nullptr ? state->program.baseUri : empty;
}

Base::Result<void> Detail::DeferredTemplateAccess::SetAuthoredVisualTree(DataTemplate& value, const Base::Ref<Base::Object>& tree) noexcept {
    DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    if (state->program.sealed || !tree) return Base::Status::Failure(Base::ErrorCode::InvalidState, "DataTemplate VisualTree assignment is invalid");
    state->authoredVisualTree = tree;
    return {};
}

Base::Result<void> Detail::DeferredTemplateAccess::SetAuthoredVisualTree(ItemsPanelTemplate& value, const Base::Ref<Base::Object>& tree) noexcept {
    ItemsPanelTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "ItemsPanelTemplate state allocation failed");
    if (state->program.sealed || !tree) return Base::Status::Failure(Base::ErrorCode::InvalidState, "ItemsPanelTemplate VisualTree assignment is invalid");
    state->authoredVisualTree = tree;
    return {};
}

void Detail::DeferredTemplateAccess::ClearAuthoredVisualTree(DataTemplate& value) noexcept { DataTemplateState* state = State(value); if (state != nullptr) state->authoredVisualTree.Reset(); }
void Detail::DeferredTemplateAccess::ClearAuthoredVisualTree(ItemsPanelTemplate& value) noexcept { ItemsPanelTemplateState* state = State(value); if (state != nullptr) state->authoredVisualTree.Reset(); }

Base::Result<void> Detail::DeferredTemplateAccess::TryAddAuthoredTrigger(DataTemplate& value, Base::Ref<Aero::TriggerBase> trigger) noexcept {
    DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    if (!trigger || state->program.factory != nullptr) return Base::Status::Failure(Base::ErrorCode::InvalidState, "DataTemplate Trigger cannot be added after sealing");
    return state->authoredTriggers.TryPushBack(std::move(trigger));
}

void Detail::DeferredTemplateAccess::ClearAuthoredTriggers(DataTemplate& value) noexcept { DataTemplateState* state = State(value); if (state != nullptr) state->authoredTriggers.Clear(); }

Base::Span<const Base::Ref<Aero::TriggerBase>> Detail::DeferredTemplateAccess::AuthoredTriggers(const DataTemplate& value) noexcept {
    const DataTemplateState* state = State(value);
    return state != nullptr ? Base::Span<const Base::Ref<Aero::TriggerBase>>(state->authoredTriggers.Data(), state->authoredTriggers.Size()) : Base::Span<const Base::Ref<Aero::TriggerBase>>{};
}

Base::Result<void> Detail::DeferredTemplateAccess::RegisterAuthoredName(DataTemplate& value, Base::StringView name, Base::Object& object) noexcept {
    DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    return state->authoredNames.TryRegister(name, object);
}

void Detail::DeferredTemplateAccess::ClearAuthoredNames(DataTemplate& value) noexcept { DataTemplateState* state = State(value); if (state != nullptr) state->authoredNames.Clear(); }

const Aero::NameScope& Detail::DeferredTemplateAccess::AuthoredNames(const DataTemplate& value) noexcept {
    static Aero::NameScope empty;
    const DataTemplateState* state = State(value);
    return state != nullptr ? state->authoredNames : empty;
}

const Base::Ref<Base::Object>& Detail::DeferredTemplateAccess::AuthoredVisualTree(const DataTemplate& value) noexcept {
    static Base::Ref<Base::Object> empty;
    const DataTemplateState* state = State(value);
    return state != nullptr ? state->authoredVisualTree : empty;
}

const Base::Ref<Base::Object>& Detail::DeferredTemplateAccess::AuthoredVisualTree(const ItemsPanelTemplate& value) noexcept {
    static Base::Ref<Base::Object> empty;
    const ItemsPanelTemplateState* state = State(value);
    return state != nullptr ? state->authoredVisualTree : empty;
}

Base::Result<void> Detail::DeferredTemplateAccess::Seal(DataTemplate& value) noexcept {
    DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    Base::Result<void> program = state->program.Seal();
    if (!program) return program.GetStatus();
    return state->resources.Seal();
}

Base::Result<void> Detail::DeferredTemplateAccess::Seal(ItemsPanelTemplate& value) noexcept {
    ItemsPanelTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "ItemsPanelTemplate state allocation failed");
    Base::Result<void> program = state->program.Seal();
    if (!program) return program.GetStatus();
    return state->resources.Seal();
}

Base::Result<Base::Ref<Base::Object>> Detail::DeferredTemplateAccess::Instantiate(const DataTemplate& value, const Base::Ref<Base::Object>& item) noexcept {
    const DataTemplateState* state = State(value);
    if (state == nullptr || state->program.factory == nullptr || !item) return Base::Status::Failure(Base::ErrorCode::InvalidState, "DataTemplate is not ready");
    return state->program.Instantiate(item);
}

Base::Result<Base::Ref<Base::Object>> Detail::DeferredTemplateAccess::Instantiate(const ItemsPanelTemplate& value) noexcept {
    const ItemsPanelTemplateState* state = State(value);
    if (state == nullptr || state->program.factory == nullptr) return Base::Status::Failure(Base::ErrorCode::InvalidState, "ItemsPanelTemplate is not ready");
    return state->program.Instantiate();
}

Base::Result<void> ContentControl::StoreContentProperty(
    Core::Value value) noexcept {
    if (synchronizingContentProperty_) return {};
    synchronizingContentProperty_ = true;
    Base::Result<void> stored =
        SetValue(ContentProperty, std::move(value));
    synchronizingContentProperty_ = false;
    return stored;
}

void ContentControl::OnContentPropertyChanged(
    Core::DependencyObject& object,
    const Core::DependencyPropertyChangedEventArgs&
        change) noexcept {
    auto& control = static_cast<ContentControl&>(object);
    if (control.synchronizingContentProperty_) return;
    control.synchronizingContentProperty_ = true;
    static_cast<void>(
        Detail::ContentControlAccess::SetContentValue(control, change.newValue));
    control.synchronizingContentProperty_ = false;
}

Base::Result<void> ContentControl::SetContentValue(
    Base::Ref<Base::Object> value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    if (value &&
        PropertyRegistry().Types().IsDerivedFrom(
            value->RuntimeType(),
            UIElement::StaticTypeId())) {
        authoredContent_ = Core::Value::FromObject(
            value->RuntimeType(), value);
        return SetOwnedContent(
            value,
            *static_cast<UIElement*>(value.Get()));
    }
    literalTextContent_ = false;
    if (content_ != nullptr) {
        Base::Result<void> cleared = SetContent(nullptr);
        if (!cleared) return cleared.GetStatus();
    }
    contentValue_ = std::move(value);
    ownedContent_.Reset();
    authoredContent_ = contentValue_
        ? Core::Value::FromObject(
            contentValue_->RuntimeType(),
            contentValue_)
        : Core::Value::NullObject(
            Core::TypeOf<Base::Object>());
    return InvalidateMeasure();
}

Base::Result<void> ContentControl::SetContentValue(
    Core::Value value) noexcept {
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ContentControl Content cannot be unset");
    }
    if (value.Kind() == Core::ValueKind::Object) {
        authoredContent_ = value;
        return SetContentValue(value.AsObject());
    }
    if (value.Kind() != Core::ValueKind::String) {
        Base::Result<void> stored =
            StoreContentProperty(value);
        if (!stored) return stored.GetStatus();
        authoredContent_ = std::move(value);
        contentValue_.Reset();
        ownedContent_.Reset();
        return InvalidateMeasure();
    }

    Base::Result<Base::Ref<TextBlock>> created =
        Base::MakeRef<TextBlock>();
    if (!created) return created.GetStatus();
    Base::Result<void> text =
        created.Value()->SetText(value.AsString());
    if (!text) return text.GetStatus();
    text = created.Value()->SetForeground(Foreground());
    if (!text) return text.GetStatus();
    Base::Ref<Base::Object> retained(created.Value());
    Base::Result<void> assigned = SetOwnedContent(
        retained, *created.Value());
    if (!assigned) return assigned.GetStatus();
    Base::Result<void> stored =
        StoreContentProperty(value);
    if (!stored) return stored.GetStatus();
    authoredContent_ = std::move(value);
    contentValue_.Reset();
    literalTextContent_ = true;
    return {};
}

Base::Result<Base::Ref<Base::Object>>
ContentControl::TryCreateTemplatedContent() const noexcept {
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
        Detail::DeferredTemplateAccess::Instantiate(
            *static_cast<DataTemplate*>(contentTemplate.Get()),
            contentValue_);
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
        items_.TryAddItemsChanged(localHandler_));
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

Base::Result<void> ItemsControl::OnApplyTemplate() noexcept {
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
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "ItemsControl template is missing required part 'ItemsHost'");
    }
    if (PropertyRegistry().Types().IsDerivedFrom(
            part->RuntimeType(),
            ItemsPresenter::StaticTypeId())) {
        itemsHost_ =
            static_cast<ItemsPresenter*>(part)->
                ItemsHost();
    } else if (PropertyRegistry().Types().IsDerivedFrom(
                   part->RuntimeType(),
                   Panel::StaticTypeId())) {
        itemsHost_ = static_cast<Panel*>(part);
    }
    if (itemsHost_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemsControl ItemsHost part must be a Panel or populated ItemsPresenter");
    }
    return {};
}

void ItemsControl::OnTemplateDetached() noexcept {
    if (generator_ != nullptr) {
        static_cast<void>(generator_->Detach());
    }
    itemsHost_ = nullptr;
}

std::uint32_t ItemsControl::ItemCount() const noexcept {
    return source_ != nullptr
        ? source_->Count()
        : items_.Count();
}

std::uint32_t ItemsControl::RealizedItemCount() const noexcept {
    return generator_ != nullptr
        ? generator_->GeneratedCount()
        : 0U;
}

std::uint32_t ItemsControl::CreatedContainerCount() const noexcept {
    return generator_ != nullptr
        ? generator_->CreatedContainerCount()
        : 0U;
}

std::uint32_t
ItemsControl::RecycledContainerUseCount() const noexcept {
    return generator_ != nullptr
        ? generator_->RecycledContainerUseCount()
        : 0U;
}

Base::Ref<Base::Object> ItemsControl::ItemAt(
    std::uint32_t index) const noexcept {
    return source_ != nullptr
        ? source_->ItemAt(index)
        : items_.ItemAt(index);
}

Base::Result<void> ItemsControl::SetItemsSource(
    IItemsSource* source) noexcept {
    if (source_ == source) return {};
    if (source != nullptr) {
        Base::Result<void> subscribed =
            source->TryAddItemsChanged(
                sourceHandler_);
        if (!subscribed) {
            return subscribed.GetStatus();
        }
    }
    if (source_ != nullptr) {
        static_cast<void>(
            source_->RemoveItemsChanged(
                sourceHandler_));
    }
    source_ = source;
    PublishItemCount();
    PublishReset();
    return {};
}

void ItemsControl::SetItemTemplate(
    const DataTemplate* value) noexcept {
    if (itemTemplate_ == value) return;
    itemTemplate_ = value;
    PublishReset();
}

void ItemsControl::SetItemsPanel(
    const ItemsPanelTemplate* value) noexcept {
    if (itemsPanel_ == value) return;
    itemsPanel_ = value;
    PublishReset();
}

void ItemsControl::SetItemContainerStyle(
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
            ItemCount(),
            ItemCount()});
    }
}

void ItemsControl::PublishItemCount() noexcept {
    const std::uint32_t count = ItemCount();
    static_cast<void>(SetReadOnlyCurrentValue(
        ItemCountProperty, count));
    static_cast<void>(SetReadOnlyCurrentValue(
        HasItemsProperty, count != 0U));
}

Base::Result<Base::Ref<ItemContainer>>
ItemsControl::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    return Base::MakeRef<ItemContainer>();
}

Base::Result<void> ItemsControl::PrepareContainer(
    ItemContainer&,
    const Base::Ref<Base::Object>&,
    std::uint32_t) noexcept {
    return {};
}

void ItemsControl::ClearContainer(
    ItemContainer&) noexcept {}

namespace Detail {

class ItemContainerGeneratorImpl final {
public:
    ItemContainerGeneratorImpl(
        ItemContainerGenerator& facade,
        ObjectTree& tree,
        LayoutManager& layout,
        EffectiveValueEngine& values,
        StyleManager* styles,
        RenderManager* renderer,
        TemplateManager* templates,
        ItemSubtreeCallback subtreeCallback,
        void* subtreeContext) noexcept;
    ~ItemContainerGeneratorImpl() noexcept;

    Base::Result<void> Attach(ItemsControl& owner, Panel& itemsHost) noexcept;
    Base::Result<void> AttachVirtualized(
        ItemsControl& owner,
        VirtualizingStackPanel& itemsHost) noexcept;
    Base::Result<bool> Detach() noexcept;
    Base::Result<void> Refresh() noexcept;
    Base::Result<bool> SetRealizationRange(
        std::uint32_t firstIndex,
        std::uint32_t count) noexcept;
    ItemContainer* ContainerFromIndex(std::uint32_t index) const noexcept;
    std::uint32_t IndexFromContainer(
        const ItemContainer& container) const noexcept;
    Base::Ref<Base::Object> ItemFromContainer(
        const ItemContainer& container) const noexcept;

    std::uint32_t GeneratedCount() const noexcept { return records_.Size(); }
    std::uint32_t FirstGeneratedIndex() const noexcept {
        return firstGeneratedIndex_;
    }
    std::uint32_t CreatedContainerCount() const noexcept {
        return createdContainerCount_;
    }
    std::uint32_t RecycledContainerUseCount() const noexcept {
        return recycledContainerUseCount_;
    }
    Base::Status LastError() const noexcept { return lastError_; }

private:
    struct Record final {
        Base::Ref<Base::Object> item;
        Base::Ref<ItemContainer> container;
        Base::Ref<Base::Object> content;
        MountEdgeState containerMount;
        MountEdgeState contentMount;
        Base::Vector<MountEdgeState> subtreeMounts;
        const Style* appliedStyle = nullptr;
        bool itemIsOwnContainer = false;
        bool generatedTextContent = false;
        bool subtreeMounted = false;
    };

    ItemContainerGenerator* facade_ = nullptr;
    ObjectTree* tree_ = nullptr;
    LayoutManager* layout_ = nullptr;
    EffectiveValueEngine* values_ = nullptr;
    StyleManager* styles_ = nullptr;
    RenderManager* renderer_ = nullptr;
    TemplateManager* templates_ = nullptr;
    ItemSubtreeCallback subtreeCallback_ = nullptr;
    void* subtreeContext_ = nullptr;
    MountService mounts_;
    ItemsControl* owner_ = nullptr;
    Panel* host_ = nullptr;
    VirtualizingStackPanel* virtualizingHost_ = nullptr;
    Base::Vector<Record> records_;
    Base::Vector<Base::Ref<ItemContainer>> recycledContainers_;
    std::uint32_t firstGeneratedIndex_ = 0U;
    std::uint32_t createdContainerCount_ = 0U;
    std::uint32_t recycledContainerUseCount_ = 0U;
    ItemsChangedHandler changedHandler_;
    Base::Status lastError_;

    void OnItemsChanged(const ItemsChangedEvent& event) noexcept;
    Base::Result<Record> CreateRecord(std::uint32_t index) noexcept;
    Base::Result<void> AttachRecord(Record& record, std::uint32_t index) noexcept;
    Base::Result<void> AttachOwnedSubtree(Record& record, Aero::Visual& root) noexcept;
    Base::Result<void> DetachOwnedSubtree(Record& record) noexcept;
    Base::Result<void> DetachRecord(
        Record& record,
        bool recycleContainer = false) noexcept;
    Base::Result<void> InsertRecord(std::uint32_t index, Record record) noexcept;
    void RemoveRecordAt(std::uint32_t index) noexcept;
    Base::Result<void> ReorderVisuals() noexcept;
    Base::Result<void> ApplyChange(const ItemsChangedEvent& event) noexcept;
    Base::Result<bool> SetRealizationRangeInternal(
        std::uint32_t firstIndex,
        std::uint32_t count,
        bool force) noexcept;
    Base::Result<void> ReleaseRecycledContainers() noexcept;
};

} // namespace Detail

Detail::ItemContainerGeneratorImpl::ItemContainerGeneratorImpl(
    ItemContainerGenerator& facade,
    ObjectTree& tree,
    LayoutManager& layout,
    EffectiveValueEngine& values,
    StyleManager* styles,
    RenderManager* renderer,
    TemplateManager* templates,
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
      mounts_(tree, &layout, renderer),
      changedHandler_(
          this,
          &ItemContainerGeneratorImpl::OnItemsChanged) {}

Detail::ItemContainerGeneratorImpl::~ItemContainerGeneratorImpl() noexcept {
    static_cast<void>(Detach());
}

Base::Result<void> Detail::ItemContainerGeneratorImpl::Attach(
    ItemsControl& owner,
    Panel& itemsHost) noexcept {
    if (owner_ != nullptr ||
        owner.generator_ != nullptr ||
        Aero::Detail::VisualAccess::Tree(owner) != tree_ ||
        Aero::Detail::VisualAccess::Tree(itemsHost) != tree_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemContainerGenerator attach state is invalid");
    }
    Base::Result<void> subscribed =
        owner.TryAddItemsChanged(changedHandler_);
    if (!subscribed) return subscribed.GetStatus();
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
Detail::ItemContainerGeneratorImpl::AttachVirtualized(
    ItemsControl& owner,
    VirtualizingStackPanel& itemsHost) noexcept {
    if (owner_ != nullptr ||
        owner.generator_ != nullptr ||
        Aero::Detail::VisualAccess::Tree(owner) != tree_ ||
        Aero::Detail::VisualAccess::Tree(itemsHost) != tree_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Virtualized item generator attach state is invalid");
    }
    Base::Result<void> subscribed =
        owner.TryAddItemsChanged(changedHandler_);
    if (!subscribed) return subscribed.GetStatus();
    owner_ = &owner;
    host_ = &itemsHost;
    virtualizingHost_ = &itemsHost;
    firstGeneratedIndex_ = 0U;
    owner.generator_ = facade_;
    Base::Result<void> attached =
        itemsHost.AttachGenerator(*facade_, owner.ItemCount());
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

Base::Result<bool> Detail::ItemContainerGeneratorImpl::Detach() noexcept {
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

Base::Result<Detail::ItemContainerGeneratorImpl::Record>
Detail::ItemContainerGeneratorImpl::CreateRecord(
    std::uint32_t index) noexcept {
    if (owner_ == nullptr ||
        index >= owner_->ItemCount()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Item generation index is out of range");
    }
    Record record;
    record.item = owner_->ItemAt(index);
    if (!record.item) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemsSource returned null");
    }
    if (owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.item->RuntimeType(),
            ItemContainer::StaticTypeId())) {
        record.container =
            Base::Ref<ItemContainer>::FromBorrowed(
                *static_cast<ItemContainer*>(
                    record.item.Get()));
        record.itemIsOwnContainer = true;
        return record;
    }
    const DataTemplate* itemTemplate =
        owner_->GetItemTemplate();
    if (itemTemplate != nullptr) {
        Base::Result<Base::Ref<Base::Object>>
            content =
                Detail::DeferredTemplateAccess::Instantiate(*itemTemplate, record.item);
        if (!content) return content.GetStatus();
        record.content =
            std::move(content).Value();
    } else if (
        record.item->RuntimeType() ==
            BoxedItemValue::StaticTypeId()) {
        const Core::Value& value =
            static_cast<const BoxedItemValue&>(
                *record.item).Value();
        if (value.Kind() !=
                Core::ValueKind::String) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Boxed data item has no default text representation");
        }
        Base::Result<Base::Ref<TextBlock>> text =
            Base::MakeRef<TextBlock>();
        if (!text) return text.GetStatus();
        Base::Result<void> assigned =
            text.Value()->SetText(value.AsString());
        if (!assigned) return assigned.GetStatus();
        record.content =
            Base::Ref<Base::Object>(
                std::move(text).Value());
        record.generatedTextContent = true;
    } else if (owner_->PropertyRegistry().Types()
        .IsDerivedFrom(
            record.item->RuntimeType(),
            UIElement::StaticTypeId())) {
        record.content = record.item;
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
        Base::Result<Base::Ref<ItemContainer>> made =
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
Detail::ItemContainerGeneratorImpl::AttachOwnedSubtree(
    Record& record,
    Aero::Visual& root) noexcept {
    Base::Vector<Aero::Visual*> pending;
    Base::Result<void> pushed =
        pending.TryPushBack(&root);
    if (!pushed) return pushed.GetStatus();

    const auto attachChild =
        [this, &record, &pending](
            Aero::Visual& parent,
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
            *static_cast<Aero::Visual*>(
                owned.Get());
        if (child.GetVisualParent() == &parent &&
            Aero::Detail::VisualAccess::Tree(child) == tree_) {
            return pending.TryPushBack(&child);
        }
        if (child.GetVisualParent() != nullptr ||
            child.GetLogicalParent() != nullptr ||
            Aero::Detail::VisualAccess::Tree(child) != nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Owned item-template child is already mounted elsewhere");
        }
        Base::Result<MountEdgeState> mounted =
            mounts_.Attach(parent, child);
        if (!mounted) return mounted.GetStatus();
        MountEdgeState edge =
            std::move(mounted).Value();
        Base::Result<void> tracked =
            record.subtreeMounts.TryPushBack(
                std::move(edge));
        if (!tracked) {
            (void)mounts_.Detach(edge);
            return tracked.GetStatus();
        }
        Base::Result<void> queued =
            pending.TryPushBack(&child);
        if (!queued) {
            MountEdgeState rollback =
                std::move(record.subtreeMounts.Back());
            record.subtreeMounts.PopBack();
            (void)mounts_.Detach(rollback);
            return queued.GetStatus();
        }
        return {};
    };

    while (!pending.Empty()) {
        Aero::Visual* current =
            pending.Back();
        pending.PopBack();
        if (current == nullptr) continue;
        const Core::TypeId type =
            current->RuntimeType();
        if (owner_->PropertyRegistry().Types().
                IsDerivedFrom(
                    type, Panel::StaticTypeId())) {
            auto& panel =
                *static_cast<Panel*>(current);
            for (std::uint32_t index = 0U;
                 index < Detail::PanelAccess::Count(panel);
                 ++index) {
                Base::Result<void> attached =
                    attachChild(
                        panel,
                        Detail::PanelAccess::At(panel, index));
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
                    Detail::DecoratorAccess::OwnedChild(decorator));
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
                    Detail::ContentControlAccess::OwnedContent(content));
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
                    presenter.OwnedContent());
            if (!attached) {
                (void)DetachOwnedSubtree(record);
                return attached.GetStatus();
            }
        }
    }
    return {};
}

Base::Result<void>
Detail::ItemContainerGeneratorImpl::DetachOwnedSubtree(
    Record& record) noexcept {
    Base::Status firstError;
    for (std::uint32_t index =
             record.subtreeMounts.Size();
         index > 0U; --index) {
        Base::Result<void> detached =
            mounts_.Detach(
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
Detail::ItemContainerGeneratorImpl::AttachRecord(
    Record& record,
    std::uint32_t index) noexcept {
    ItemContainer& container = *record.container;

    Base::Result<MountEdgeState> containerMounted =
        mounts_.Attach(*owner_, *host_, container);
    if (!containerMounted) return containerMounted.GetStatus();
    record.containerMount = std::move(containerMounted).Value();

    if (!record.itemIsOwnContainer) {
        auto& content =
            *static_cast<UIElement*>(
                record.content.Get());
        Base::Result<MountEdgeState> contentMounted =
            mounts_.Attach(container, content);
        if (!contentMounted) {
            (void)mounts_.Detach(record.containerMount);
            return contentMounted.GetStatus();
        }
        record.contentMount =
            std::move(contentMounted).Value();

        Base::Result<void> selected =
            record.generatedTextContent
            ? Detail::ItemContainerGeneratorAccess::
                  SetGeneratedTextContent(
                      container, record.content, content)
            : Detail::ContentControlAccess::SetOwnedContent(container,
                  record.content, content);
        if (!selected) {
            (void)mounts_.Detach(
                record.contentMount);
            (void)mounts_.Detach(
                record.containerMount);
            return selected.GetStatus();
        }
    }
    // UI activation starts at the generated container. This is
    // required for implicit container styles and control templates (for
    // example ComboBoxItem), while traversal still reaches DataTemplate
    // content mounted beneath it.
    Aero::Visual& subtree =
        static_cast<Aero::Visual&>(container);
    Base::Result<void> subtreeAttached =
        AttachOwnedSubtree(record, subtree);
    if (!subtreeAttached) {
        (void)DetachRecord(record);
        return subtreeAttached.GetStatus();
    }
    const Style* style = owner_->GetItemContainerStyle();
    if (style != nullptr && styles_ != nullptr) {
        Base::Result<void> styled = styles_->Apply(container, *style);
        if (!styled) { (void)DetachRecord(record); return styled.GetStatus(); }
        record.appliedStyle = style;
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
    return {};
}

Base::Result<void>
Detail::ItemContainerGeneratorImpl::DetachRecord(
    Record& record,
    bool recycleContainer) noexcept {
    if (!record.container) return {};
    ItemContainer& container = *record.container;
    Base::Status firstError;
    const auto capture = [&firstError](const Base::Result<void>& result) noexcept {
        if (!result && firstError.IsOk()) firstError = result.GetStatus();
    };
    if (record.subtreeMounted &&
        subtreeCallback_ != nullptr) {
        Aero::Visual& subtree =
            static_cast<Aero::Visual&>(
                container);
        capture(subtreeCallback_(
            subtree,
            ItemSubtreeChange::Unmounting,
            subtreeContext_));
        record.subtreeMounted = false;
    }
    capture(DetachOwnedSubtree(record));
    owner_->ClearContainer(container);
    if (record.appliedStyle != nullptr && styles_ != nullptr) {
        capture(styles_->Clear(container, *record.appliedStyle));
        record.appliedStyle = nullptr;
    }
    if (!record.itemIsOwnContainer &&
        templates_ != nullptr &&
        Detail::ControlAccess::IsTemplateApplied(container)) {
        Base::Result<bool> cleared =
            templates_->Clear(container);
        if (!cleared) {
            capture(Base::Result<void>(
                cleared.GetStatus()));
        }
    }
    UIElement* content = record.itemIsOwnContainer
        ? nullptr
        : Detail::ContentControlAccess::ContentElement(container);
    if (content != nullptr) {
        capture(mounts_.Detach(record.contentMount));
        capture(container.SetContent(nullptr));
        capture(values_->DetachObject(*content));
    }
    capture(mounts_.Detach(record.containerMount));
    if (!record.itemIsOwnContainer &&
        recycleContainer && firstError.IsOk()) {
        Base::Result<void> recycled = recycledContainers_.TryPushBack(std::move(record.container));
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
    record.subtreeMounted = false;
    return firstError.IsOk() ? Base::Result<void>() : Base::Result<void>(firstError);
}

Base::Result<void>
Detail::ItemContainerGeneratorImpl::ReleaseRecycledContainers() noexcept {
    Base::Status firstError;
    for (Base::Ref<ItemContainer>& container :
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
Detail::ItemContainerGeneratorImpl::InsertRecord(
    std::uint32_t index,
    Record record) noexcept {
    if (index > records_.Size()) {
        static_cast<void>(DetachRecord(record));
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Generated container insert is out of range");
    }
    Base::Result<void> reserved =
        records_.TryReserve(records_.Size() + 1U);
    if (!reserved) {
        const Base::Status error = reserved.GetStatus();
        static_cast<void>(DetachRecord(record));
        return error;
    }
    Base::Result<void> appended =
        records_.TryPushBack(std::move(record));
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

void Detail::ItemContainerGeneratorImpl::RemoveRecordAt(
    std::uint32_t index) noexcept {
    for (std::uint32_t current = index;
        current + 1U < records_.Size(); ++current) {
        records_[current] =
            std::move(records_[current + 1U]);
    }
    records_.PopBack();
}

Base::Result<void>
Detail::ItemContainerGeneratorImpl::ReorderVisuals() noexcept {
    for (Record& record : records_) {
        Base::Result<void> detached = mounts_.DetachVisual(record.containerMount);
        if (!detached) return detached.GetStatus();
    }
    for (Record& record : records_) {
        Base::Result<void> attached = mounts_.AttachVisual(record.containerMount, *host_);
        if (!attached) return attached.GetStatus();
    }
    return {};
}

Base::Result<bool>
Detail::ItemContainerGeneratorImpl::SetRealizationRangeInternal(
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
        owner_->ItemCount();
    firstIndex = std::min(firstIndex, itemCount);
    count = std::min(count, itemCount - firstIndex);
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
        records_.TryReserve(count);
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
            records_.TryPushBack(std::move(record));
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
Detail::ItemContainerGeneratorImpl::SetRealizationRange(
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

Base::Result<void> Detail::ItemContainerGeneratorImpl::Refresh() noexcept {
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
        records_.TryReserve(owner_->ItemCount());
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t index = 0U;
        index < owner_->ItemCount(); ++index) {
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
            records_.TryPushBack(
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
    return {};
}

Base::Result<void> Detail::ItemContainerGeneratorImpl::ApplyChange(
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

void Detail::ItemContainerGeneratorImpl::OnItemsChanged(
    const ItemsChangedEvent& event) noexcept {
    Base::Result<void> applied;
    if (virtualizingHost_ != nullptr &&
        owner_ != nullptr) {
        applied =
            virtualizingHost_->OnItemsChanged(
                event, owner_->ItemCount());
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

ItemContainer*
Detail::ItemContainerGeneratorImpl::ContainerFromIndex(
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
Detail::ItemContainerGeneratorImpl::IndexFromContainer(
    const ItemContainer& container) const noexcept {
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
Detail::ItemContainerGeneratorImpl::ItemFromContainer(
    const ItemContainer& container) const noexcept {
    const std::uint32_t index =
        IndexFromContainer(container);
    return index != UINT32_MAX
        ? records_[
            index - firstGeneratedIndex_].item
        : Base::Ref<Base::Object>();
}

ItemContainerGenerator::~ItemContainerGenerator() noexcept {
    delete static_cast<Detail::ItemContainerGeneratorImpl*>(impl_);
    impl_ = nullptr;
}

Base::Result<void> ItemContainerGenerator::Attach(
    ItemsControl& owner,
    Panel& itemsHost) noexcept {
    auto* runtime = static_cast<Detail::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr
        ? runtime->Attach(owner, itemsHost)
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::NotInitialized,
              "ItemContainerGenerator is not initialized"));
}

Base::Result<void> ItemContainerGenerator::AttachVirtualized(
    ItemsControl& owner,
    VirtualizingStackPanel& itemsHost) noexcept {
    auto* runtime = static_cast<Detail::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr
        ? runtime->AttachVirtualized(owner, itemsHost)
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::NotInitialized,
              "ItemContainerGenerator is not initialized"));
}

Base::Result<bool> ItemContainerGenerator::Detach() noexcept {
    auto* runtime = static_cast<Detail::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->Detach() : Base::Result<bool>(false);
}

Base::Result<void> ItemContainerGenerator::Refresh() noexcept {
    auto* runtime = static_cast<Detail::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr
        ? runtime->Refresh()
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::NotInitialized,
              "ItemContainerGenerator is not initialized"));
}

Base::Result<bool> ItemContainerGenerator::SetRealizationRange(
    std::uint32_t firstIndex,
    std::uint32_t count) noexcept {
    auto* runtime = static_cast<Detail::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr
        ? runtime->SetRealizationRange(firstIndex, count)
        : Base::Result<bool>(Base::Status::Failure(
              Base::ErrorCode::NotInitialized,
              "ItemContainerGenerator is not initialized"));
}

std::uint32_t ItemContainerGenerator::GeneratedCount() const noexcept {
    auto* runtime = static_cast<Detail::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->GeneratedCount() : 0U;
}

std::uint32_t ItemContainerGenerator::FirstGeneratedIndex() const noexcept {
    auto* runtime = static_cast<Detail::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->FirstGeneratedIndex() : 0U;
}

std::uint32_t ItemContainerGenerator::CreatedContainerCount() const noexcept {
    auto* runtime = static_cast<Detail::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->CreatedContainerCount() : 0U;
}

std::uint32_t ItemContainerGenerator::RecycledContainerUseCount() const noexcept {
    auto* runtime = static_cast<Detail::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->RecycledContainerUseCount() : 0U;
}

ItemContainer* ItemContainerGenerator::ContainerFromIndex(
    std::uint32_t index) const noexcept {
    auto* runtime = static_cast<Detail::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->ContainerFromIndex(index) : nullptr;
}

std::uint32_t ItemContainerGenerator::IndexFromContainer(
    const ItemContainer& container) const noexcept {
    auto* runtime = static_cast<Detail::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->IndexFromContainer(container) : UINT32_MAX;
}

Base::Ref<Base::Object> ItemContainerGenerator::ItemFromContainer(
    const ItemContainer& container) const noexcept {
    auto* runtime = static_cast<Detail::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr
        ? runtime->ItemFromContainer(container)
        : Base::Ref<Base::Object>{};
}

Base::Status ItemContainerGenerator::LastError() const noexcept {
    auto* runtime = static_cast<Detail::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr
        ? runtime->LastError()
        : Base::Status::Failure(
              Base::ErrorCode::NotInitialized,
              "ItemContainerGenerator is not initialized");
}

Base::Result<ItemContainerGenerator*>
Detail::ItemContainerGeneratorAccess::Create(
    ObjectTree& tree,
    Aero::Detail::LayoutManager& layout,
    Core::EffectiveValueEngine& values,
    Aero::Detail::StyleManager* styles,
    Render::RenderManager* renderer,
    TemplateManager* templates,
    ItemSubtreeCallback subtreeCallback,
    void* subtreeContext) noexcept {
    auto* generator = new (std::nothrow) ItemContainerGenerator();
    if (generator == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "ItemContainerGenerator allocation failed");
    }
    generator->impl_ = new (std::nothrow) ItemContainerGeneratorImpl(
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

Base::Result<void> DataTemplate::SetResources(Base::Ref<ResourceDictionary> value) noexcept {
    Detail::DataTemplateState* state = static_cast<Detail::DataTemplateState*>(state_);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    return Aero::Detail::AssignResourceDictionary(state->resources, std::move(value), "DataTemplate Resources is already assigned");
}

Base::Result<void> ItemsPanelTemplate::SetResources(Base::Ref<ResourceDictionary> value) noexcept {
    Detail::ItemsPanelTemplateState* state = static_cast<Detail::ItemsPanelTemplateState*>(state_);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "ItemsPanelTemplate state allocation failed");
    return Aero::Detail::AssignResourceDictionary(state->resources, std::move(value), "ItemsPanelTemplate Resources is already assigned");
}

} // namespace Aero::Controls
