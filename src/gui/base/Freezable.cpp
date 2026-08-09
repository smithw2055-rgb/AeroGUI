#include <Aero/Freezable.hpp>

#include "gui/metadata/MetadataRuntime.hpp"
#include "gui/property/PropertyRuntime.hpp"
#include "gui/base/FreezableRuntime.hpp"
#include "gui/base/ElementRuntime.hpp"
#include "gui/base/RoutedEventRuntime.hpp"
#include "gui/input/InputRuntime.hpp"
#include "gui/layout/LayoutRuntime.hpp"
#include "gui/binding/BindingRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/resources/StyleRuntime.hpp"

#include <new>
#include <utility>

namespace Aero {
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
    if (!owner.PropertyRegistry().Types().IsDerivedFrom(
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
    if (DependencyObject::Access::HasUnfreezableValueState(value)) {
        return FreezeGraphStatus(
            "A Freezable with an expression or animation cannot be frozen");
    }
    Base::Result<void> pushed = context.visiting.PushBack(&value);
    if (!pushed) return pushed.GetStatus();
    Base::Result<void> children =
        DependencyObject::Access::VisitFreezableChildren(
            value, &context, &CheckFreezeChild);
    if (!children) {
        context.visiting.PopBack();
        return children.GetStatus();
    }
    if (!Freezable::Access::CheckCore(value)) {
        context.visiting.PopBack();
        return FreezeGraphStatus(
            "A Freezable child rejected the freeze operation");
    }
    context.visiting.PopBack();
    return context.complete.PushBack(&value);
}

void RemoveHandlerAt(
    Base::Vector<Freezable::Access::HandlerRecord>& handlers,
    std::uint32_t index) noexcept {
    for (std::uint32_t next = index + 1U;
         next < handlers.Size(); ++next) {
        handlers[next - 1U] = std::move(handlers[next]);
    }
    handlers.PopBack();
}

void RemoveConsumerAt(
    Base::Vector<Freezable::Access::ConsumerRecord>& consumers,
    std::uint32_t index) noexcept {
    for (std::uint32_t next = index + 1U;
         next < consumers.Size(); ++next) {
        consumers[next - 1U] = std::move(consumers[next]);
    }
    consumers.PopBack();
}

} // namespace

Freezable::Freezable(Meta::TypeId runtimeType) noexcept
    : DependencyObject(runtimeType),
      implAllocator_(&Base::GetDefaultAllocator()) {
    void* memory = implAllocator_->Allocate({
        sizeof(Access), alignof(Access), Base::MemoryTag::Ui});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Access), alignof(Access), Base::MemoryTag::Ui);
    }
    impl_ = new (memory) Access(*implAllocator_);
}

Freezable::~Freezable() {
    if (impl_ == nullptr) return;
    impl_->consumers.Clear();
    impl_->handlers.Clear();
    impl_->~Access();
    implAllocator_->Deallocate(
        impl_, sizeof(Access), alignof(Access), Base::MemoryTag::Ui);
    impl_ = nullptr;
}

bool Freezable::IsFrozen() const noexcept {
    return impl_ != nullptr && impl_->frozen;
}

bool Freezable::CanFreeze() const noexcept {
    if (IsFrozen()) return true;
    if (!VerifyAccess()) return false;
    if (impl_ == nullptr || impl_->freezing) return false;
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
    if (impl_ == nullptr || impl_->freezing) {
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
            current->impl_->freezing = true;
        }
    }
    for (Freezable* current : context.complete) {
        if (current == nullptr || current->IsFrozen()) continue;
        static_cast<void>(current->FreezeCore(false));
        current->impl_->frozen = true;
    }
    // Publish one final notification per object only after the complete graph
    // has committed. Child notifications therefore cannot re-notify a parent
    // that is already frozen.
    for (Freezable* current : context.complete) {
        if (current == nullptr || !current->impl_->freezing) continue;
        current->impl_->freezing = false;
        current->OnChanged();
        current->impl_->consumers.Clear();
        current->impl_->handlers.Clear();
    }
    return {};
}

Base::Result<void> Freezable::AddChangedHandlerChecked(
    const FreezableChangedHandler& handler) noexcept {
    Base::Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    if (handler.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Freezable changed handler is empty");
    }
    Access::HandlerRecord record;
    record.handler = handler;
    record.active = true;
    return impl_->handlers.PushBack(std::move(record));
}

void Freezable::AddChangedHandler(
    const FreezableChangedHandler& handler) noexcept {
    Base::Result<void> added = AddChangedHandlerChecked(handler);
    if (!added && added.GetStatus().code == Base::ErrorCode::OutOfMemory) {
        Base::ReportOutOfMemory(
            sizeof(Access::HandlerRecord),
            alignof(Access::HandlerRecord),
            Base::MemoryTag::Ui);
    }
}

bool Freezable::RemoveChangedHandler(
    const FreezableChangedHandler& handler) noexcept {
    if (!VerifyAccess() || handler.Empty() || impl_ == nullptr ||
        impl_->frozen) {
        return false;
    }
    for (std::uint32_t index = 0U;
         index < impl_->handlers.Size(); ++index) {
        Access::HandlerRecord& record = impl_->handlers[index];
        if (!record.active || record.handler != handler) continue;
        if (impl_->notificationDepth != 0U) {
            record.active = false;
        } else {
            RemoveHandlerAt(impl_->handlers, index);
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
    if (!IsFrozen() && impl_ != nullptr && !impl_->freezing) OnChanged();
}

bool Freezable::FreezeCore(bool) noexcept {
    return true;
}

void Freezable::OnChanged() noexcept {
    if (impl_ == nullptr) return;
    if (impl_->revision != UINT64_MAX) ++impl_->revision;

    ++impl_->notificationDepth;
    const std::uint32_t handlerCount = impl_->handlers.Size();
    for (std::uint32_t index = 0U; index < handlerCount; ++index) {
        if (index >= impl_->handlers.Size()) break;
        const Access::HandlerRecord& record = impl_->handlers[index];
        if (!record.active || record.handler.Empty()) continue;
        FreezableChangedHandler handler = record.handler;
        handler(*this);
    }
    --impl_->notificationDepth;
    if (impl_->notificationDepth == 0U) {
        for (std::uint32_t index = 0U;
             index < impl_->handlers.Size();) {
            if (!impl_->handlers[index].active) {
                RemoveHandlerAt(impl_->handlers, index);
            } else {
                ++index;
            }
        }
    }

    const std::uint32_t consumerCount = impl_->consumers.Size();
    for (std::uint32_t index = 0U; index < consumerCount; ++index) {
        if (index >= impl_->consumers.Size()) break;
        const Access::ConsumerRecord& record = impl_->consumers[index];
        Base::Ref<DependencyObject> retained = record.object.Lock();
        DependencyObject* consumer = retained
            ? retained.Get()
            : record.unmanagedObject;
        const Meta::DependencyPropertyHandle property = record.property;
        if (consumer != nullptr) {
            DependencyObject::Access::InvalidateSubProperty(
                *consumer, property);
        }
    }
    for (std::uint32_t index = 0U;
         index < impl_->consumers.Size();) {
        const Access::ConsumerRecord& record = impl_->consumers[index];
        if (record.unmanagedObject == nullptr && record.object.Expired()) {
            RemoveConsumerAt(impl_->consumers, index);
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

Base::Result<void> Freezable::Access::AttachConsumer(
    Freezable& value,
    DependencyObject& object,
    Meta::DependencyPropertyHandle property) noexcept {
    if (value.IsFrozen() || !property.IsValid()) return {};
    for (const ConsumerRecord& record : value.impl_->consumers) {
        Base::Ref<DependencyObject> retained = record.object.Lock();
        DependencyObject* candidate = retained
            ? retained.Get()
            : record.unmanagedObject;
        if (candidate == &object && record.property == property) return {};
    }
    ConsumerRecord record;
    Base::Ref<DependencyObject> retained =
        Base::Ref<DependencyObject>::TryFromBorrowed(object);
    if (retained) {
        record.object = Base::WeakRef<DependencyObject>(retained);
    } else {
        record.unmanagedObject = &object;
    }
    record.property = property;
    return value.impl_->consumers.PushBack(std::move(record));
}

void Freezable::Access::DetachConsumer(
    Freezable& value,
    DependencyObject& object,
    Meta::DependencyPropertyHandle property) noexcept {
    if (value.impl_ == nullptr) return;
    for (std::uint32_t index = 0U;
         index < value.impl_->consumers.Size(); ++index) {
        ConsumerRecord& record = value.impl_->consumers[index];
        Base::Ref<DependencyObject> retained = record.object.Lock();
        DependencyObject* candidate = retained
            ? retained.Get()
            : record.unmanagedObject;
        if (candidate == &object && record.property == property) {
            RemoveConsumerAt(value.impl_->consumers, index);
            return;
        }
    }
}

std::uint64_t Freezable::Access::Revision(
    const Freezable& value) noexcept {
    return value.impl_ != nullptr ? value.impl_->revision : 0U;
}

bool DependencyObject::Access::HasUnfreezableValueState(
    const DependencyObject& object) noexcept {
    for (const EffectiveValueEntry& entry : object.values_) {
        if (entry.hasExpression || entry.hasAnimation ||
            entry.sourceInfo.hasExpression || entry.sourceInfo.isAnimated) {
            return true;
        }
    }
    return false;
}

Base::Result<void> DependencyObject::Access::VisitFreezableChildren(
    DependencyObject& object,
    void* context,
    FreezableVisitor visitor) noexcept {
    if (visitor == nullptr) return {};
    for (const Meta::DependencyProperty& property :
         object.registry_->Properties()) {
        if (property.MetadataFor(object.runtimeType_) == nullptr) continue;
        const Meta::PropertyValue value = object.GetValue(property.Handle());
        Freezable* child = AsFreezable(object, value);
        if (child == nullptr) continue;
        Base::Result<void> visited = visitor(context, *child);
        if (!visited) return visited.GetStatus();
    }
    return {};
}

Base::Result<void> DependencyObject::Access::PrepareConsumerChange(
    DependencyObject& consumer,
    Meta::DependencyPropertyHandle property,
    const Meta::PropertyValue& oldValue,
    const Meta::PropertyValue& newValue) noexcept {
    Freezable* oldChild = AsFreezable(consumer, oldValue);
    Freezable* newChild = AsFreezable(consumer, newValue);
    if (oldChild == newChild || newChild == nullptr) return {};
    return Freezable::Access::AttachConsumer(
        *newChild, consumer, property);
}

void DependencyObject::Access::CommitConsumerChange(
    DependencyObject& consumer,
    Meta::DependencyPropertyHandle property,
    const Meta::PropertyValue& oldValue,
    const Meta::PropertyValue& newValue) noexcept {
    Freezable* oldChild = AsFreezable(consumer, oldValue);
    Freezable* newChild = AsFreezable(consumer, newValue);
    if (oldChild != nullptr && oldChild != newChild) {
        Freezable::Access::DetachConsumer(
            *oldChild, consumer, property);
    }
}

void DependencyObject::Access::InvalidateSubProperty(
    DependencyObject& object,
    Meta::DependencyPropertyHandle propertyHandle) noexcept {
    const Meta::DependencyProperty* property =
        object.registry_->Find(propertyHandle);
    const Meta::PropertyMetadata* metadata = property != nullptr
        ? property->MetadataFor(object.runtimeType_)
        : nullptr;
    if (metadata == nullptr) return;
    const Meta::PropertyInvalidationFlags flags =
        object.AccumulateInvalidations(metadata->flags);
    object.OnPropertyInvalidated(flags);
}

} // namespace Aero
