#include <Aero/Freezable.hpp>
#include <Aero/Base/Vector.hpp>

#include "gui/meta/TypeRegistryDetail.hpp"
#include "gui/core/DependencyPropertyRegistry.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"

#include <new>
#include <utility>

namespace Aero {

struct Freezable::State {
    struct ConsumerRecord {
        Base::WeakRef<DependencyObject> object;
        DependencyObject* unmanagedObject = nullptr;
        Meta::DependencyPropertyHandle property;
    };

    struct HandlerRecord {
        FreezableChangedHandler handler;
        bool active = false;
    };

    explicit State(Base::IAllocator& allocator) noexcept
        : consumers(&allocator), handlers(&allocator) {}

    Base::Vector<ConsumerRecord> consumers;
    Base::Vector<HandlerRecord> handlers;
    std::uint64_t revision = 0U;
    std::uint32_t notificationDepth = 0U;
    bool frozen = false;
    bool freezing = false;
};

namespace {

constexpr Base::Status FrozenStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ReadOnly,
        "A frozen Freezable is read-only");
}

constexpr Base::Status FreezeGraphStatus(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

Freezable* AsFreezable(
    DependencyObject& owner,
    const Meta::PropertyValue& value) noexcept {
    if (value.Kind() != Meta::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject()) {
        return nullptr;
    }
    Base::Object* object = value.AsObject().Get();
    if (!AeroGuiInternal::PropertyRegistry(owner).Types().IsDerivedFrom(
            object->RuntimeType(), Freezable::StaticTypeId())) {
        return nullptr;
    }
    return static_cast<Freezable*>(object);
}

template<class T>
bool Contains(
    const Base::Vector<T*>& values,
    const T* value) noexcept {
    for (const T* candidate : values) {
        if (candidate == value) return true;
    }
    return false;
}

struct FreezeCheckContext {
    explicit FreezeCheckContext(Base::IAllocator& allocator) noexcept
        : visiting(&allocator), complete(&allocator) {}

    Base::Vector<Freezable*> visiting;
    Base::Vector<Freezable*> complete;
};

thread_local FreezeCheckContext* activeFreezeCheck = nullptr;

Base::Result<void> CheckFreezeNode(
    FreezeCheckContext& context,
    Freezable& value) noexcept;

Base::Result<void> CheckFreezeChild(
    void* context,
    Freezable& child) noexcept {
    return CheckFreezeNode(
        *static_cast<FreezeCheckContext*>(context), child);
}

Base::Result<void> CheckFreezeNode(
    FreezeCheckContext& context,
    Freezable& value) noexcept {
    if (value.IsFrozen() || Contains(context.complete, &value)) return {};
    if (Contains(context.visiting, &value)) {
        return FreezeGraphStatus(
            "A Freezable object graph contains a cycle");
    }
    if (AeroGuiInternal::HasUnfreezableValueState(value)) {
        return FreezeGraphStatus(
            "A Freezable with an expression or animation cannot be frozen");
    }
    context.visiting.PushBack(&value);
    Base::Result<void> children =
        AeroGuiInternal::VisitFreezableChildren(
            value, &context, &CheckFreezeChild);
    if (!children) {
        context.visiting.PopBack();
        return children.GetStatus();
    }
    if (!AeroGuiInternal::FreezableCheckCore(value)) {
        context.visiting.PopBack();
        return FreezeGraphStatus(
            "A Freezable child rejected the freeze operation");
    }
    context.visiting.PopBack();
    context.complete.PushBack(&value);
    return {};
}

void RemoveHandlerAt(
    Base::Vector<Freezable::State::HandlerRecord>& handlers,
    std::uint32_t index) noexcept {
    for (std::uint32_t next = index + 1U;
         next < handlers.Size(); ++next) {
        handlers[next - 1U] = std::move(handlers[next]);
    }
    handlers.PopBack();
}

void RemoveConsumerAt(
    Base::Vector<Freezable::State::ConsumerRecord>& consumers,
    std::uint32_t index) noexcept {
    for (std::uint32_t next = index + 1U;
         next < consumers.Size(); ++next) {
        consumers[next - 1U] = std::move(consumers[next]);
    }
    consumers.PopBack();
}

} // namespace

Freezable::Freezable(Meta::TypeId runtimeType) noexcept
    : DependencyObject(runtimeType) {}

bool Freezable::EnsureState() noexcept {
    if (state_ != nullptr) return true;
    Base::IAllocator& allocator = Base::GetDefaultAllocator();
    void* memory = allocator.Allocate({
        sizeof(Freezable::State), alignof(Freezable::State), Base::MemoryTag::Ui});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Freezable::State), alignof(Freezable::State), Base::MemoryTag::Ui);
        return false;
    }
    state_ = new (memory) Freezable::State(allocator);
    return true;
}

Freezable::~Freezable() {
    if (state_ == nullptr) return;
    state_->consumers.Clear();
    state_->handlers.Clear();
    state_->~State();
    Base::GetDefaultAllocator().Deallocate(
        state_, sizeof(Freezable::State), alignof(Freezable::State), Base::MemoryTag::Ui);
    state_ = nullptr;
}

bool Freezable::IsFrozen() const noexcept {
    return state_ != nullptr && state_->frozen;
}

bool Freezable::CanFreeze() const noexcept {
    if (IsFrozen()) return true;
    if (!VerifyAccess()) return false;
    if (state_ != nullptr && state_->freezing) return false;
    if (activeFreezeCheck != nullptr) {
        return CheckFreezeNode(
            *activeFreezeCheck,
            *const_cast<Freezable*>(this)).HasValue();
    }
    FreezeCheckContext context(Base::GetDefaultAllocator());
    activeFreezeCheck = &context;
    Base::Result<void> checked = CheckFreezeNode(
        context, *const_cast<Freezable*>(this));
    activeFreezeCheck = nullptr;
    return checked.HasValue();
}

Base::Result<void> Freezable::Freeze() noexcept {
    if (IsFrozen()) return {};
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    if (state_ != nullptr && state_->freezing) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Freezable freeze operation is already active");
    }

    if (activeFreezeCheck != nullptr) {
        return CheckFreezeNode(*activeFreezeCheck, *this);
    }

    FreezeCheckContext context(Base::GetDefaultAllocator());
    activeFreezeCheck = &context;
    Base::Result<void> checked = CheckFreezeNode(context, *this);
    activeFreezeCheck = nullptr;
    if (!checked) return checked.GetStatus();

    // CheckFreezeNode records a child-first order. The commit phase performs
    // no allocation and cannot leave an object graph partially frozen because
    // every overridable check has already succeeded.
    for (Freezable* current : context.complete) {
        if (current != nullptr && !current->IsFrozen()) {
            if (!current->EnsureState()) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfMemory,
                    "Freezable freeze state allocation failed");
            }
            current->state_->freezing = true;
        }
    }
    for (Freezable* current : context.complete) {
        if (current == nullptr || current->IsFrozen()) continue;
        static_cast<void>(current->FreezeCore(false));
        current->state_->frozen = true;
    }
    // Publish one final notification per object only after the complete graph
    // has committed. Child notifications therefore cannot re-notify a parent
    // that is already frozen.
    for (Freezable* current : context.complete) {
        if (current == nullptr || !current->state_->freezing) continue;
        current->state_->freezing = false;
        current->OnChanged();
        current->state_->consumers.Clear();
        current->state_->handlers.Clear();
    }
    return {};
}

void Freezable::AddChangedHandler(
    const FreezableChangedHandler& handler) noexcept {
    if (handler.Empty()) return;
    Base::Result<void> writable = WritePreamble();
    if (!writable) return;
    Freezable::State::HandlerRecord record;
    record.handler = handler;
    record.active = true;
    if (!EnsureState()) {
        return;
    }
    static_cast<void>(state_->handlers.PushBack(std::move(record)));
}

bool Freezable::RemoveChangedHandler(
    const FreezableChangedHandler& handler) noexcept {
    if (!VerifyAccess() || handler.Empty() || state_ == nullptr ||
        state_->frozen) {
        return false;
    }
    for (std::uint32_t index = 0U;
         index < state_->handlers.Size(); ++index) {
        Freezable::State::HandlerRecord& record = state_->handlers[index];
        if (!record.active || record.handler != handler) continue;
        if (state_->notificationDepth != 0U) {
            record.active = false;
        } else {
            RemoveHandlerAt(state_->handlers, index);
        }
        return true;
    }
    return false;
}

Base::Result<void> Freezable::WritePreamble() const noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    return IsFrozen() ? Base::Result<void>(FrozenStatus())
                      : Base::Result<void>();
}

void Freezable::WritePostscript() noexcept {
    if (!IsFrozen() && state_ != nullptr && !state_->freezing) OnChanged();
}

bool Freezable::FreezeCore(bool) noexcept {
    return true;
}

void Freezable::OnChanged() noexcept {
    if (state_ == nullptr) return;
    if (state_->revision != UINT64_MAX) ++state_->revision;

    ++state_->notificationDepth;
    const std::uint32_t handlerCount = state_->handlers.Size();
    for (std::uint32_t index = 0U; index < handlerCount; ++index) {
        if (index >= state_->handlers.Size()) break;
        const Freezable::State::HandlerRecord& record = state_->handlers[index];
        if (!record.active || record.handler.Empty()) continue;
        FreezableChangedHandler handler = record.handler;
        handler(*this);
    }
    --state_->notificationDepth;
    if (state_->notificationDepth == 0U) {
        for (std::uint32_t index = 0U;
             index < state_->handlers.Size();) {
            if (!state_->handlers[index].active) {
                RemoveHandlerAt(state_->handlers, index);
            } else {
                ++index;
            }
        }
    }

    const std::uint32_t consumerCount = state_->consumers.Size();
    for (std::uint32_t index = 0U; index < consumerCount; ++index) {
        if (index >= state_->consumers.Size()) break;
        const Freezable::State::ConsumerRecord& record = state_->consumers[index];
        Base::Ref<DependencyObject> retained = record.object.Lock();
        DependencyObject* consumer = retained
            ? retained.Get()
            : record.unmanagedObject;
        const Meta::DependencyPropertyHandle property = record.property;
        if (consumer != nullptr) {
            AeroGuiInternal::InvalidateSubProperty(
                *consumer, property);
        }
    }
    for (std::uint32_t index = 0U;
         index < state_->consumers.Size();) {
        const Freezable::State::ConsumerRecord& record = state_->consumers[index];
        if (record.unmanagedObject == nullptr && record.object.Expired()) {
            RemoveConsumerAt(state_->consumers, index);
        } else {
            ++index;
        }
    }
}

void Freezable::OnPropertyInvalidated(
    Meta::PropertyInvalidationFlags flags) noexcept {
    DependencyObject::OnPropertyInvalidated(flags);
    WritePostscript();
}

Base::Result<void> Freezable::VerifyMutationAllowed() const noexcept {
    return WritePreamble();
}

} // namespace Aero

namespace Aero {

Base::Result<void> AeroGuiInternal::AttachFreezableConsumer(
    Freezable& value,
    DependencyObject& object,
    Meta::DependencyPropertyHandle property) noexcept {
    if (value.IsFrozen() || !property.IsValid()) return {};
    if (!value.EnsureState()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Freezable consumer state allocation failed");
    }
    Freezable::State* state = value.state_;
    for (const Freezable::State::ConsumerRecord& record : state->consumers) {
        Base::Ref<DependencyObject> retained = record.object.Lock();
        DependencyObject* candidate = retained
            ? retained.Get()
            : record.unmanagedObject;
        if (candidate == &object && record.property == property) return {};
    }
    Freezable::State::ConsumerRecord record;
    Base::Ref<DependencyObject> retained =
        Base::Ref<DependencyObject>::TryFromBorrowed(object);
    if (retained) {
        record.object = Base::WeakRef<DependencyObject>(retained);
    } else {
        record.unmanagedObject = &object;
    }
    record.property = property;
    state->consumers.PushBack(std::move(record));
    return {};
}

void AeroGuiInternal::DetachFreezableConsumer(
    Freezable& value,
    DependencyObject& object,
    Meta::DependencyPropertyHandle property) noexcept {
    Freezable::State* state = value.state_;
    if (state == nullptr) return;
    for (std::uint32_t index = 0U;
         index < state->consumers.Size(); ++index) {
        Freezable::State::ConsumerRecord& record = state->consumers[index];
        Base::Ref<DependencyObject> retained = record.object.Lock();
        DependencyObject* candidate = retained
            ? retained.Get()
            : record.unmanagedObject;
        if (candidate == &object && record.property == property) {
            RemoveConsumerAt(state->consumers, index);
            return;
        }
    }
}

std::uint64_t AeroGuiInternal::FreezableRevision(
    const Freezable& value) noexcept {
    Freezable::State* state = value.state_;
    return state != nullptr ? state->revision : 0U;
}

bool AeroGuiInternal::FreezableCheckCore(
    Freezable& value) noexcept {
    return value.FreezeCore(true);
}

DependencyObject* AeroGuiInternal::FreezableParent(
    const Freezable& value) noexcept {
    Freezable::State* state = value.state_;
    if (state == nullptr || state->consumers.Empty()) return nullptr;
    for (const auto& consumer : state->consumers) {
        Base::Ref<DependencyObject> retained = consumer.object.Lock();
        if (retained) return retained.Get();
        if (consumer.unmanagedObject != nullptr) return consumer.unmanagedObject;
    }
    return nullptr;
}

bool AeroGuiInternal::HasUnfreezableValueState(
    const DependencyObject& object) noexcept {
    const PropertyStore* store = AeroGuiInternal::Store(object);
    if (store == nullptr) {
        return false;
    }
    for (const auto& record : store->entries) {
        const StoredValueEntry& entry = record.Value();
        if (entry.HasExpression() || entry.HasAnimation() ||
            entry.SourceInfo().hasExpression || entry.SourceInfo().isAnimated) {
            return true;
        }
    }
    return false;
}

Base::Result<void> AeroGuiInternal::VisitFreezableChildren(
    DependencyObject& object,
    void* context,
    FreezableVisitor visitor) noexcept {
    if (visitor == nullptr) return {};
    for (const Meta::DependencyProperty& property :
         AeroGuiInternal::PropertyRegistry(object).Properties()) {
        if (property.MetadataFor(object.RuntimeType()) == nullptr) continue;
        const Meta::PropertyValue value = object.GetValue(property.Handle());
        Freezable* child = AsFreezable(object, value);
        if (child == nullptr) continue;
        Base::Result<void> visited = visitor(context, *child);
        if (!visited) return visited.GetStatus();
    }
    return {};
}

Base::Result<void> AeroGuiInternal::PrepareConsumerChange(
    DependencyObject& consumer,
    Meta::DependencyPropertyHandle property,
    const Meta::PropertyValue& oldValue,
    const Meta::PropertyValue& newValue) noexcept {
    Freezable* oldChild = AsFreezable(consumer, oldValue);
    Freezable* newChild = AsFreezable(consumer, newValue);
    if (oldChild == newChild || newChild == nullptr) return {};
    return AttachFreezableConsumer(
        *newChild, consumer, property);
}

void AeroGuiInternal::CommitConsumerChange(
    DependencyObject& consumer,
    Meta::DependencyPropertyHandle property,
    const Meta::PropertyValue& oldValue,
    const Meta::PropertyValue& newValue) noexcept {
    Freezable* oldChild = AsFreezable(consumer, oldValue);
    Freezable* newChild = AsFreezable(consumer, newValue);
    if (oldChild != nullptr && oldChild != newChild) {
        DetachFreezableConsumer(
            *oldChild, consumer, property);
    }
}

void AeroGuiInternal::InvalidateSubProperty(
    DependencyObject& object,
    Meta::DependencyPropertyHandle propertyHandle) noexcept {
    const Meta::DependencyProperty* property =
        AeroGuiInternal::PropertyRegistry(object).Find(propertyHandle);
    const Meta::PropertyMetadata* metadata = property != nullptr
        ? property->MetadataFor(object.RuntimeType())
        : nullptr;
    if (metadata == nullptr) return;
    const Meta::PropertyInvalidationFlags flags =
        object.AccumulateInvalidations(metadata->flags);
    object.OnPropertyInvalidated(flags);
}

} // namespace Aero
