// BindingEngine: lifecycle, evaluation, and source/target operations.
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/LogicalTreeHelper.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Visual.hpp>
#include <Aero/VisualTreeHelper.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Brushes.hpp>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>
#include "gui/data/BindingCommon.hpp"
#include "gui/controls/ItemsContainers.hpp"
#include <Aero/Freezable.hpp>
#include "gui/core/Describe.hpp"
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <cctype>
#include <cstdlib>
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Data;


::Aero::DependencyObject* BindingEngine::BindingParent(
    ::Aero::DependencyObject& node) noexcept {
    ::Aero::DependencyObject* parent =
        ::Aero::LogicalTreeHelper::GetParent(node);
    if (parent == nullptr) {
        if (::Aero::Media::Visual* visual =
                ::Aero::TryCast<::Aero::Media::Visual>(&node)) {
            parent = ::Aero::Media::VisualTreeHelper::GetParent(*visual);
        }
    }
    if (parent == nullptr) {
        if (::Aero::Freezable* freezable =
                ::Aero::TryCast<::Aero::Freezable>(&node)) {
            parent = (*freezable).Parent();
        }
    }
    return parent;
}

bool BindingEngine::BindingOwnerSeesDataContextChange(
    ::Aero::DependencyObject* owner,
    ::Aero::DependencyObject& changed) noexcept {
    ::Aero::DependencyObject* node = owner;
    for (std::uint32_t depth = 0U; depth < 64U && node != nullptr; ++depth) {
        if (node == &changed) {
            return true;
        }
        node = BindingParent(*node);
    }
    return false;
}

Base::Result<PropertyValue> BindingEngine::ReadDataContextValue(
    ::Aero::DependencyObject& node,
    Meta::DependencyPropertyHandle handle) noexcept {
    if (::Aero::FrameworkElement* element =
            ::Aero::TryCast<::Aero::FrameworkElement>(&node)) {
        return element->GetDataContext();
    }
    if (DependencyObjectAccess::PropertyRegistry((node)).Find(handle) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "DataContext property is not registered on this object");
    }
    return node.GetValue(handle);
}

BindingEngine::BindingEngine(
    Dispatcher& dispatcher,
    Meta::Registry* metadata) noexcept
    : dispatcher_(&dispatcher),
      metadata_(metadata),
      bindings_(),
      handleIndexMap_(),
      propertyChangedHandler_(this, &BindingEngine::OnPropertyChanged) {}

BindingEngine::~BindingEngine() noexcept {
    Shutdown();
}

Base::Result<void> BindingEngine::Initialize() noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess();
    }
    if (initialized_) {
        return {};
    }
    // P3.2: ViewFrame drives DataBindHook() directly; no DataBind hook.
    initialized_ = true;
    return {};
}

void BindingEngine::Shutdown() noexcept {
    while (!bindings_.Empty()) {
        RemoveAt(bindings_.Size() - 1U);
    }
    deferredBindings_.Clear();
    pendingDeferredActivations_.Clear();
    multiBindings_.Clear();
    sourceToBindings_.Clear();
    targetToBindings_.Clear();
    dataContextBindings_.Clear();
    flushing_ = false;
    hasDirtyBindings_ = true;
    pollingBindingCount_ = 0U;
    writesDataContextCount_ = 0U;
    initialized_ = false;
}





Base::Result<void> BindingEngine::QueueDeferred(
    const MetadataBindingDescriptor& descriptor) noexcept {
    hasDirtyBindings_ = true;
    if (descriptor.metadata == nullptr ||
        descriptor.target == nullptr ||
        !descriptor.targetProperty.IsValid() ||
        (descriptor.path.Empty() && !descriptor.bindsToSource)) {
        return InvalidArgument(
            "Deferred Binding descriptor is invalid");
    }
    if (descriptor.source == nullptr &&
        !descriptor.dataContextProperty.IsValid()) {
        return InvalidArgument(
            "Deferred DataContext Binding requires a DataContext property");
    }
    DeferredBindingRecord record;
    record.metadata = descriptor.metadata;
    record.source = descriptor.source;
    record.target = descriptor.target;
    record.targetProperty = descriptor.targetProperty;
    record.dataContextProperty =
        descriptor.dataContextProperty;
    record.dataContextOwner = descriptor.dataContextOwner != nullptr
        ? descriptor.dataContextOwner
        : descriptor.target;
    record.mode = descriptor.mode;
    record.updateSourceTrigger =
        descriptor.updateSourceTrigger;
    record.bindsToSource = descriptor.bindsToSource;
    record.convert = descriptor.convert;
    record.convertBack = descriptor.convertBack;
    record.converterResource = descriptor.converterResource;
    record.converterParameter = descriptor.converterParameter;
    record.validate = descriptor.validate;
    record.validateBack = descriptor.validateBack;
    record.conversionContext =
        descriptor.conversionContext;
    record.fallbackValue = descriptor.fallbackValue;
    record.targetNullValue =
        descriptor.targetNullValue;
    record.diagnostic = descriptor.diagnostic;
    record.diagnosticContext =
        descriptor.diagnosticContext;
    Base::Result<void> assigned =
        record.path.Assign(descriptor.path);
    if (!assigned) return assigned.GetStatus();
    assigned = record.stringFormat.Assign(
        descriptor.stringFormat);
    if (!assigned) return assigned.GetStatus();
    deferredBindings_.PushBack(
        std::move(record));
    return {};
}

Base::Result<std::uint32_t>
BindingEngine::ActivateDeferred(
    DependencyObject& target) noexcept {
    hasDirtyBindings_ = true;
    std::uint32_t activated = 0U;
    std::uint32_t writeIndex = 0U;
    const std::uint32_t initialSize = deferredBindings_.Size();
    for (std::uint32_t readIndex = 0U; readIndex < initialSize; ++readIndex) {
        DeferredBindingRecord& record = deferredBindings_[readIndex];
        if (record.target != &target) {
            if (writeIndex != readIndex) {
                deferredBindings_[writeIndex] = std::move(record);
            }
            ++writeIndex;
            continue;
        }
        MetadataBindingDescriptor descriptor;
        descriptor.metadata = record.metadata;
        descriptor.source = record.source;
        descriptor.target = record.target;
        descriptor.targetProperty = record.targetProperty;
        descriptor.dataContextProperty = record.dataContextProperty;
        descriptor.dataContextOwner = record.dataContextOwner;
        descriptor.path = record.path.View();
        descriptor.stringFormat = record.stringFormat.View();
        descriptor.bindsToSource = record.bindsToSource;
        descriptor.mode = record.mode;
        descriptor.updateSourceTrigger = record.updateSourceTrigger;
        descriptor.convert = record.convert;
        descriptor.convertBack = record.convertBack;
        descriptor.converterResource = record.converterResource;
        descriptor.converterParameter = record.converterParameter;
        descriptor.validate = record.validate;
        descriptor.validateBack = record.validateBack;
        descriptor.conversionContext = record.conversionContext;
        descriptor.fallbackValue = record.fallbackValue;
        descriptor.targetNullValue = record.targetNullValue;
        descriptor.diagnostic = record.diagnostic;
        descriptor.diagnosticContext = record.diagnosticContext;

        Base::Result<BindingHandle> attached = Attach(descriptor);
        if (!attached) {
            RecordError(attached.GetStatus());
        } else {
            ++activated;
        }
    }
    if (writeIndex < initialSize) {
        for (std::uint32_t tail = initialSize; tail < deferredBindings_.Size(); ++tail) {
            deferredBindings_[writeIndex++] = std::move(deferredBindings_[tail]);
        }
        (void)deferredBindings_.Resize(writeIndex);
    }
    return activated;
}

Base::Result<void> BindingEngine::ActivateDeferredWhenReady(
    DependencyObject& target) noexcept {
    hasDirtyBindings_ = true;
    if (!flushing_) {
        Base::Result<std::uint32_t> activated =
            ActivateDeferred(target);
        return activated
            ? Base::Result<void>{}
            : Base::Result<void>(activated.GetStatus());
    }
    for (DependencyObject* pending : pendingDeferredActivations_) {
        if (pending == &target) return {};
    }
    pendingDeferredActivations_.PushBack(&target);
    return {};
}

Base::Result<std::uint32_t>
BindingEngine::ActivatePendingDeferred() noexcept {
    if (flushing_) {
        return InvalidState(
            "Deferred Binding activation cannot run while flushing");
    }
    Base::Vector<DependencyObject*> pending =
        std::move(pendingDeferredActivations_);
    pendingDeferredActivations_.Clear();
    std::uint32_t activated = 0U;
    for (DependencyObject* target : pending) {
        if (target == nullptr) continue;
        Base::Result<std::uint32_t> current =
            ActivateDeferred(*target);
        if (!current) {
            RecordError(current.GetStatus());
            continue;
        }
        activated += current.Value();
    }
    return activated;
}







BindingHandle BindingEngine::FindBinding(
    DependencyObject& target,
    DependencyPropertyHandle property) const noexcept {
    const auto* list = targetToBindings_.Find(&target);
    if (list != nullptr) {
        for (BindingHandle handle : *list) {
            const BindingRecord* record = FindRecord(handle);
            if (record != nullptr && record->descriptor.targetProperty == property) {
                return handle;
            }
        }
    }
    return {};
}

Data::BindingStatus BindingEngine::QueryStatus(
    BindingHandle handle) const noexcept {
    const BindingRecord* record = FindRecord(handle);
    if (record == nullptr) return Data::BindingStatus::Unattached;
    if (!record->lastStatus.IsOk()) {
        switch (record->conversionFailureStage) {
        case BindingDiagnosticStage::ConvertBack:
        case BindingDiagnosticStage::ValidateBack:
        case BindingDiagnosticStage::WriteSource:
            return Data::BindingStatus::UpdateSourceError;
        default:
            return Data::BindingStatus::UpdateTargetError;
        }
    }
    return record->applied
        ? Data::BindingStatus::Active
        : Data::BindingStatus::Inactive;
}

bool BindingEngine::Contains(BindingHandle handle) const noexcept {
    return FindRecord(handle) != nullptr;
}

UpdateSourceTrigger BindingEngine::ResolveUpdateSourceTrigger(
    DependencyObject& target,
    DependencyPropertyHandle property,
    UpdateSourceTrigger requested) noexcept {
    if (requested != UpdateSourceTrigger::Default) {
        return requested;
    }
    const DependencyProperty* info =
        DependencyObjectAccess::PropertyRegistry((target)).Find(property);
    if (info == nullptr) {
        return UpdateSourceTrigger::PropertyChanged;
    }
    const PropertyMetadata* metadata =
        info->MetadataFor(target.RuntimeType());
    if (metadata == nullptr ||
        metadata->defaultUpdateSourceTrigger ==
            UpdateSourceTrigger::Default) {
        return UpdateSourceTrigger::PropertyChanged;
    }
    return metadata->defaultUpdateSourceTrigger;
}

BindingMode BindingEngine::ResolveBindingMode(
    DependencyObject& target,
    DependencyPropertyHandle property,
    BindingMode requested) noexcept {
    if (requested != BindingMode::Default) {
        return requested;
    }
    const DependencyProperty* info =
        DependencyObjectAccess::PropertyRegistry((target)).Find(property);
    if (info == nullptr) {
        return BindingMode::OneWay;
    }
    const PropertyMetadata* metadata =
        info->MetadataFor(target.RuntimeType());
    if (metadata != nullptr &&
        HasFlag(
            metadata->flags,
            FrameworkPropertyMetadataOptions::BindsTwoWayByDefault)) {
        return BindingMode::TwoWay;
    }
    return BindingMode::OneWay;
}

void BindingEngine::RegisterMultiBinding(
    DependencyObject& target,
    DependencyPropertyHandle property,
    Base::Span<const Data::BindingHandle> handles) noexcept {
    MultiBindingGroup group;
    group.target = &target;
    group.targetProperty = property;
    for (std::uint32_t index = 0U; index < handles.Size(); ++index) {
        group.handles.PushBack(handles[index]);
    }
    static_cast<void>(multiBindings_.PushBack(std::move(group)));
}

Data::MultiBindingExpression BindingEngine::FindMultiBinding(
    DependencyObject& target,
    DependencyPropertyHandle property) const noexcept {
    Data::MultiBindingExpression expression;
    for (const MultiBindingGroup& group : multiBindings_) {
        if (group.target != &target ||
            group.targetProperty != property) {
            continue;
        }
        for (std::uint32_t index = 0U; index < group.handles.Size(); ++index) {
            expression.handles_.PushBack(group.handles[index]);
        }
        return expression;
    }
    return {};
}

BindingEngine::BindingRecord* BindingEngine::FindRecord(
    BindingHandle handle) noexcept {
    if (!handle.IsValid() || handle.engine_ != this) return nullptr;
    const std::uint32_t* found = handleIndexMap_.Find(handle.value);
    if (found != nullptr && *found < bindings_.Size() && bindings_[*found].handle.value == handle.value) {
        return &bindings_[*found];
    }
    return nullptr;
}

const BindingEngine::BindingRecord* BindingEngine::FindRecord(
    BindingHandle handle) const noexcept {
    if (!handle.IsValid() || handle.engine_ != this) return nullptr;
    const std::uint32_t* found = handleIndexMap_.Find(handle.value);
    if (found != nullptr && *found < bindings_.Size() && bindings_[*found].handle.value == handle.value) {
        return &bindings_[*found];
    }
    return nullptr;
}

Base::Result<void> BindingEngine::SubscribeLostFocus(
    BindingRecord& record) noexcept {
    if (record.descriptor.updateSourceTrigger !=
            UpdateSourceTrigger::LostFocus ||
        record.descriptor.target == nullptr) {
        return {};
    }
    UIElement* element = TryCast<UIElement>(record.descriptor.target);
    if (element == nullptr) return {};
    if (record.descriptor.targetProperty ==
        UIElement::IsKeyboardFocusedProperty.Handle()) {
        record.lostFocusSubscribed = true;
        return {};
    }
    element->AddValueChangedHandler(
        UIElement::IsKeyboardFocusedProperty.Handle(),
        propertyChangedHandler_);
    record.lostFocusSubscribed = true;
    return {};
}

void BindingEngine::UnsubscribeLostFocus(BindingRecord& record) noexcept {
    if (!record.lostFocusSubscribed || record.descriptor.target == nullptr) {
        return;
    }
    UIElement* element = TryCast<UIElement>(record.descriptor.target);
    if (element == nullptr) return;
    if (record.descriptor.targetProperty ==
        UIElement::IsKeyboardFocusedProperty.Handle()) {
        record.lostFocusSubscribed = false;
        return;
    }
    (void)element->RemoveValueChangedHandler(
        UIElement::IsKeyboardFocusedProperty.Handle(),
        propertyChangedHandler_);
    record.lostFocusSubscribed = false;
}

Base::Result<std::uint32_t> BindingEngine::DetachObject(
    DependencyObject& object) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    std::uint32_t detached = 0U;
    Base::Vector<BindingHandle> handlesToDetach;
    if (const auto* targetList = targetToBindings_.Find(&object)) {
        for (BindingHandle h : *targetList) {
            handlesToDetach.PushBack(h);
        }
    }
    if (const auto* sourceList = sourceToBindings_.Find(&object)) {
        for (BindingHandle h : *sourceList) {
            bool found = false;
            for (BindingHandle existing : handlesToDetach) {
                if (existing.value == h.value) { found = true; break; }
            }
            if (!found) handlesToDetach.PushBack(h);
        }
    }

    for (BindingHandle handle : handlesToDetach) {
        const std::uint32_t* found = handleIndexMap_.Find(handle.value);
        if (found == nullptr || *found >= bindings_.Size()) continue;
        if (flushing_) {
            const std::uint32_t index = *found;
            handleIndexMap_.Erase(handle.value);
            CleanupRecord(bindings_[index]);
            bindings_[index].handle.value = 0U;
            hasPendingDetaches_ = true;
            ++detached;
        } else {
            RemoveAt(*found);
            ++detached;
        }
    }
    std::uint32_t writeIndex = 0U;
    for (std::uint32_t readIndex = 0U; readIndex < deferredBindings_.Size(); ++readIndex) {
        DeferredBindingRecord& record = deferredBindings_[readIndex];
        if (record.source != &object &&
            record.target != &object &&
            record.dataContextOwner != &object) {
            if (writeIndex != readIndex) {
                deferredBindings_[writeIndex] = std::move(record);
            }
            ++writeIndex;
        } else {
            ++detached;
        }
    }
    (void)deferredBindings_.Resize(writeIndex);

    std::uint32_t writePending = 0U;
    for (std::uint32_t readIndex = 0U; readIndex < pendingDeferredActivations_.Size(); ++readIndex) {
        if (pendingDeferredActivations_[readIndex] != &object) {
            if (writePending != readIndex) {
                pendingDeferredActivations_[writePending] = pendingDeferredActivations_[readIndex];
            }
            ++writePending;
        }
    }
    (void)pendingDeferredActivations_.Resize(writePending);
    return detached;
}



} // namespace Aero

namespace Aero::Data {

Base::Result<Value> BooleanToVisibilityConverter::Convert(
    const Value& value,
    const Value& parameter) noexcept {
    (void)parameter;
    Base::Result<bool> converted =
        Meta::ValueCodec<bool>::Decode(value);
    if (!converted) return converted.GetStatus();
    return Meta::ValueCodec<Aero::Visibility>::Encode(
        converted.Value()
            ? Aero::Visibility::Visible
            : Aero::Visibility::Collapsed);
}

Base::Result<Value> BooleanToVisibilityConverter::ConvertBack(
    const Value& value,
    const Value& parameter) noexcept {
    (void)parameter;
    Base::Result<Aero::Visibility> converted =
        Meta::ValueCodec<Aero::Visibility>::Decode(value);
    if (!converted) return converted.GetStatus();
    return Meta::ValueCodec<bool>::Encode(
        converted.Value() == Aero::Visibility::Visible);
}

Base::Ref<RelativeSource> RelativeSource::ForSelf() noexcept {
    return Base::MakeRef<RelativeSource>(RelativeSourceMode::Self);
}

Base::Ref<RelativeSource> RelativeSource::ForTemplatedParent() noexcept {
    return Base::MakeRef<RelativeSource>(RelativeSourceMode::TemplatedParent);
}

} // namespace Aero::Data

namespace Aero::Data {

using namespace Aero::Meta;
using namespace Aero::Threading;


Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status BindingTypeMismatch(
    const TypeRegistry& types,
    Base::StringView path,
    TypeId sourceType,
    const ::Aero::DependencyObject& target,
    const DependencyProperty* targetProperty) noexcept {
    thread_local char message[512];
    const TypeInfo* source =
        types.FindType(sourceType);
    const TypeInfo* targetType =
        types.FindType(target.RuntimeType());
    const TypeInfo* targetValue =
        targetProperty != nullptr
        ? types.FindType(
              targetProperty->ValueType())
        : nullptr;
    const Base::StringView sourceName =
        source != nullptr
        ? source->Name()
        : Base::StringView("<unknown>");
    const Base::StringView targetName =
        targetType != nullptr
        ? targetType->Name()
        : Base::StringView("<unknown>");
    const Base::StringView propertyName =
        targetProperty != nullptr
        ? targetProperty->Name()
        : Base::StringView("<unknown>");
    const Base::StringView targetValueName =
        targetValue != nullptr
        ? targetValue->Name()
        : Base::StringView("<unknown>");
    std::snprintf(
        message,
        sizeof(message),
        "Binding path '%.*s' result '%.*s' does not match target '%.*s.%.*s' type '%.*s'",
        static_cast<int>(path.SizeBytes()),
        path.Data(),
        static_cast<int>(
            sourceName.SizeBytes()),
        sourceName.Data(),
        static_cast<int>(
            targetName.SizeBytes()),
        targetName.Data(),
        static_cast<int>(
            propertyName.SizeBytes()),
        propertyName.Data(),
        static_cast<int>(
            targetValueName.SizeBytes()),
        targetValueName.Data());
    return InvalidArgument(message);
}

Base::Status BindingPathFailure(
    const TypeRegistry& types,
    Base::StringView path,
    const BindingPathCompileError& error,
    Base::Status status) noexcept {
    thread_local char message[512];
    const TypeInfo* input = types.FindType(error.inputType);
    const Base::StringView inputName = input != nullptr
        ? input->Name()
        : Base::StringView("<unknown>");
    std::snprintf(
        message,
        sizeof(message),
        "Binding path '%.*s' failed at segment %u '%.*s' on type '%.*s': %s",
        static_cast<int>(path.SizeBytes()),
        path.Data(),
        error.segmentIndex,
        static_cast<int>(error.segment.SizeBytes()),
        error.segment.CStr(),
        static_cast<int>(inputName.SizeBytes()),
        inputName.Data(),
        status.message != nullptr ? status.message : "operation failed");
    return Base::Status::Failure(status.code, message);
}

bool IsNumericType(TypeId type) noexcept {
    return type == TypeOf<std::int8_t>() ||
        type == TypeOf<std::int16_t>() ||
        type == TypeOf<std::int32_t>() ||
        type == TypeOf<std::int64_t>() ||
        type == TypeOf<std::uint8_t>() ||
        type == TypeOf<std::uint16_t>() ||
        type == TypeOf<std::uint32_t>() ||
        type == TypeOf<std::uint64_t>() ||
        type == TypeOf<double>();
}

Base::Result<PropertyValue> ConvertNumericValue(
    const PropertyValue& value,
    TypeId targetType) noexcept {
    long double number = 0.0L;
    switch (value.Kind()) {
    case ValueKind::SignedInteger:
        number = static_cast<long double>(
            value.AsSignedInteger());
        break;
    case ValueKind::UnsignedInteger:
        number = static_cast<long double>(
            value.AsUnsignedInteger());
        break;
    case ValueKind::Double:
        if (!std::isfinite(value.AsDouble())) {
            return InvalidArgument(
                "Binding numeric value is not finite");
        }
        number = static_cast<long double>(
            value.AsDouble());
        break;
    default:
        return InvalidArgument(
            "Binding numeric value has an invalid representation");
    }

    if (targetType == TypeOf<double>()) {
        const double result =
            static_cast<double>(number);
        if (!std::isfinite(result)) {
            return InvalidArgument(
                "Binding numeric value exceeds the Double range");
        }
        return PropertyValue::FromDouble(
            targetType, result);
    }

    const bool signedTarget =
        targetType == TypeOf<std::int8_t>() ||
        targetType == TypeOf<std::int16_t>() ||
        targetType == TypeOf<std::int32_t>() ||
        targetType == TypeOf<std::int64_t>();
    if (signedTarget) {
        long double minimum = static_cast<long double>(
            std::numeric_limits<std::int64_t>::min());
        long double maximum = static_cast<long double>(
            std::numeric_limits<std::int64_t>::max());
        if (targetType == TypeOf<std::int8_t>()) {
            minimum = std::numeric_limits<std::int8_t>::min();
            maximum = std::numeric_limits<std::int8_t>::max();
        } else if (
            targetType == TypeOf<std::int16_t>()) {
            minimum = std::numeric_limits<std::int16_t>::min();
            maximum = std::numeric_limits<std::int16_t>::max();
        } else if (
            targetType == TypeOf<std::int32_t>()) {
            minimum = std::numeric_limits<std::int32_t>::min();
            maximum = std::numeric_limits<std::int32_t>::max();
        }
        if (number < minimum || number > maximum) {
            return InvalidArgument(
                "Binding numeric value exceeds the signed target range");
        }
        return PropertyValue::FromSignedInteger(
            targetType,
            static_cast<std::int64_t>(number));
    }

    long double maximum = static_cast<long double>(
        std::numeric_limits<std::uint64_t>::max());
    if (targetType == TypeOf<std::uint8_t>()) {
        maximum = std::numeric_limits<std::uint8_t>::max();
    } else if (
        targetType == TypeOf<std::uint16_t>()) {
        maximum = std::numeric_limits<std::uint16_t>::max();
    } else if (
        targetType == TypeOf<std::uint32_t>()) {
        maximum = std::numeric_limits<std::uint32_t>::max();
    }
    if (number < 0.0L || number > maximum) {
        return InvalidArgument(
            "Binding numeric value exceeds the unsigned target range");
    }
    return PropertyValue::FromUnsignedInteger(
        targetType,
        static_cast<std::uint64_t>(number));
}

bool HasDefaultTargetConversion(
    TypeId sourceType,
    TypeId targetType) noexcept {
    return sourceType == TypeOf<Meta::Value>() ||
        (sourceType == TypeOf<bool>() &&
         targetType == TypeOf<::Aero::Nullable<bool>>()) ||
        (sourceType == TypeOf<::Aero::Nullable<bool>>() &&
         targetType == TypeOf<bool>()) ||
        (sourceType != targetType &&
         IsNumericType(sourceType) &&
         IsNumericType(targetType)) ||
        (IsNumericType(sourceType) &&
         targetType == TypeOf<Aero::Length>()) ||
        (IsNumericType(sourceType) &&
         targetType == TypeOf<Base::Thickness>()) ||
        (sourceType == TypeOf<Aero::Length>() &&
         IsNumericType(targetType)) ||
        (sourceType == TypeOf<Base::Color>() &&
         targetType == Media::Brush::StaticTypeId()) ||
        (targetType == TypeOf<Base::String>() &&
         sourceType != InvalidTypeId) ||
        (sourceType == TypeOf<Base::String>() &&
         targetType != InvalidTypeId);
}

bool TargetAcceptsPathResult(
    const TypeRegistry& types,
    const DependencyProperty* targetProperty,
    TypeId resultType,
    bool hasDynamicResult) noexcept {
    if (targetProperty == nullptr) {
        return false;
    }
    if (targetProperty->AcceptsAnyValue() ||
        hasDynamicResult ||
        resultType == TypeOf<Base::Object>() ||
        targetProperty->ValueType() == TypeOf<Base::Object>()) {
        return true;
    }
    return types.IsAssignableFrom(
               targetProperty->ValueType(), resultType) ||
        HasDefaultTargetConversion(
            resultType, targetProperty->ValueType());
}

Base::Result<PropertyValue> ConvertNullableBooleanValue(
    const PropertyValue& value,
    TypeId targetType) noexcept {
    if (targetType == TypeOf<::Aero::Nullable<bool>>() &&
        value.Type() == TypeOf<bool>() &&
        value.Kind() == ValueKind::Boolean) {
        return ValueCodec<::Aero::Nullable<bool>>::Encode(
            ::Aero::Nullable<bool>{value.AsBoolean()});
    }
    if (targetType == TypeOf<bool>() &&
        value.Type() == TypeOf<::Aero::Nullable<bool>>()) {
        Base::Result<::Aero::Nullable<bool>> decoded =
            ValueCodec<::Aero::Nullable<bool>>::Decode(value);
        if (!decoded) return decoded.GetStatus();
        if (!decoded.Value().GetHasValue()) {
            return InvalidArgument(
                "Indeterminate Nullable Boolean cannot be converted to Boolean");
        }
        return PropertyValue::FromBoolean(
            targetType, decoded.Value().GetValue());
    }
    return InvalidArgument(
        "Nullable Boolean conversion is incompatible");
}

Base::Result<PropertyValue> ConvertThicknessValue(
    const PropertyValue& value) noexcept {
    Base::Result<PropertyValue> numeric =
        ConvertNumericValue(value, TypeOf<double>());
    if (!numeric) return numeric.GetStatus();
    const double uniform = numeric.Value().AsDouble();
    return Meta::ValueCodec<Base::Thickness>::Encode(
        Base::Thickness{uniform, uniform, uniform, uniform});
}

Base::Result<PropertyValue> ConvertLengthValue(
    const PropertyValue& value,
    TypeId targetType) noexcept {
    if (targetType == TypeOf<Aero::Length>() &&
        IsNumericType(value.Type())) {
        Base::Result<PropertyValue> numeric =
            ConvertNumericValue(value, TypeOf<double>());
        if (!numeric) return numeric.GetStatus();
        return Meta::ValueCodec<Aero::Length>::Encode(
            Aero::Length::Pixels(numeric.Value().AsDouble()));
    }
    if (value.Type() == TypeOf<Aero::Length>() &&
        IsNumericType(targetType)) {
        Base::Result<Aero::Length> length =
            Meta::ValueCodec<Aero::Length>::Decode(value);
        if (!length) return length.GetStatus();
        if (length.Value().isAuto) {
            return InvalidArgument(
                "Auto Length cannot be converted to a numeric binding target");
        }
        return ConvertNumericValue(
            PropertyValue::FromDouble(
                TypeOf<double>(), length.Value().value),
            targetType);
    }
    return InvalidArgument(
        "Binding Length conversion is not supported");
}

Base::Result<PropertyValue> ConvertColorToBrush(
    const PropertyValue& value) noexcept {
    Base::Result<Base::Color> color =
        Meta::ValueCodec<Base::Color>::Decode(value);
    if (!color) return color.GetStatus();
    Base::Ref<Media::Brush> brush =
        Media::MakeSolidColorBrush(color.Value());
    if (!brush) {
        return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to create brush");
    }
    return PropertyValue::FromObject(
        Media::Brush::StaticTypeId(),
        Base::Ref<Base::Object>(std::move(brush)));
}

// A TwoWay object binding may intentionally expose a concrete source object
// through an Object-typed target property (for example a view-model Language
// through Selector.SelectedItem). The forward assignment is type-safe; the
// reverse assignment is checked against the runtime object's real type.
bool CanRoundTripObjectValue(
    const TypeRegistry& types,
    TypeId sourceType,
    TypeId targetType) noexcept {
    return sourceType != InvalidTypeId &&
        targetType != InvalidTypeId &&
        sourceType != targetType &&
        types.IsAssignableFrom(targetType, sourceType);
}

bool TryParseFixedPrecision(
    Base::StringView specifier,
    std::uint32_t& precision) noexcept {
    if (specifier.Empty() ||
        (specifier[0] != 'F' && specifier[0] != 'f')) {
        return false;
    }
    if (specifier.SizeBytes() == 1U) {
        precision = 2U;
        return true;
    }
    std::uint32_t parsed = 0U;
    for (std::uint32_t index = 1U;
         index < specifier.SizeBytes();
         ++index) {
        const char digit = specifier[index];
        if (digit < '0' || digit > '9') return false;
        parsed = parsed * 10U +
            static_cast<std::uint32_t>(digit - '0');
        if (parsed > 15U) return false;
    }
    precision = parsed;
    return true;
}

bool TryParseCurrencyPrecision(
    Base::StringView specifier,
    std::uint32_t& precision) noexcept {
    if (specifier.Empty() ||
        (specifier[0] != 'C' && specifier[0] != 'c')) {
        return false;
    }
    if (specifier.SizeBytes() == 1U) {
        precision = 2U;
        return true;
    }
    std::uint32_t parsed = 0U;
    for (std::uint32_t index = 1U;
         index < specifier.SizeBytes();
         ++index) {
        const char digit = specifier[index];
        if (digit < '0' || digit > '9') return false;
        parsed = parsed * 10U +
            static_cast<std::uint32_t>(digit - '0');
        if (parsed > 15U) return false;
    }
    precision = parsed;
    return true;
}

bool IsZeroPaddingFormat(
    Base::StringView specifier) noexcept {
    if (specifier.Empty()) return false;
    for (std::uint32_t index = 0U;
         index < specifier.SizeBytes();
         ++index) {
        if (specifier[index] != '0') return false;
    }
    return true;
}

Base::Result<Base::String> FormatBindingString(
    const PropertyValue& value,
    Base::StringView format,
    const Registry* metadata) noexcept {
    Base::Result<PropertyValue> unboxed = UnboxItemsValue(value);
    const PropertyValue& targetVal = unboxed ? unboxed.Value() : value;

    // Markup extensions write \{ and \} so the XAML parser does not treat
    // the placeholder as nested markup. WPF stores StringFormat without those
    // slashes ("Orbit: {0:F2} AU"). Unescape before looking up {0:...}.
    Base::String unescapedFormat;
    if (!format.Empty()) {
        unescapedFormat.Reserve(format.SizeBytes());
        for (std::uint32_t index = 0U;
             index < format.SizeBytes();
             ++index) {
            if (format[index] == '\\' &&
                index + 1U < format.SizeBytes()) {
                ++index;
            }
            const char character = format[index];
            Base::Result<void> appended = unescapedFormat.Append(
                Base::StringView(&character, 1U));
            if (!appended) return appended.GetStatus();
        }
    }
    Base::StringView activeFormat = unescapedFormat.View();
    if (activeFormat.SizeBytes() >= 2U &&
        activeFormat[0] == '{' &&
        activeFormat[1] == '}') {
        activeFormat = activeFormat.Substr(
            2U, activeFormat.SizeBytes() - 2U);
    }

    char raw[128]{};
    if (targetVal.Type() == TypeOf<::Aero::Nullable<bool>>()) {
        Base::Result<::Aero::Nullable<bool>> decoded =
            ValueCodec<::Aero::Nullable<bool>>::Decode(targetVal);
        if (!decoded) return decoded.GetStatus();
        if (decoded.Value().GetHasValue()) {
            std::snprintf(
                raw, sizeof(raw), "%s",
                decoded.Value().GetValue() ? "True" : "False");
        }
    } else if (targetVal.Type() == TypeOf<Base::Color>()) {
        Base::Result<Base::Color> color =
            Meta::ValueCodec<Base::Color>::Decode(targetVal);
        if (!color) return color.GetStatus();
        const auto toByte = [](float v) -> unsigned int {
            float clamped = std::clamp(v, 0.0f, 1.0f);
            return static_cast<unsigned int>(std::lround(clamped * 255.0f));
        };
        const unsigned int a = toByte(color.Value().alpha);
        const unsigned int r = toByte(color.Value().red);
        const unsigned int g = toByte(color.Value().green);
        const unsigned int b = toByte(color.Value().blue);
        std::snprintf(raw, sizeof(raw), "#%02X%02X%02X%02X", a, r, g, b);
    }
    bool numeric = false;
    double numericValue = 0.0;
    switch ((targetVal.Type() == TypeOf<::Aero::Nullable<bool>>() ||
             targetVal.Type() == TypeOf<Base::Color>())
        ? ValueKind::Unset
        : targetVal.Kind()) {
    case ValueKind::Unset:
        break;
    case ValueKind::String: {
        if (activeFormat.Empty() || activeFormat == "{0}") {
            Base::String result;
            Base::Result<void> assigned =
                result.Assign(targetVal.AsString());
            return assigned
                ? Base::Result<Base::String>(
                      std::move(result))
                : Base::Result<Base::String>(
                      assigned.GetStatus());
        }
        Base::StringView prefix;
        Base::StringView suffix;
        for (std::uint32_t index = 0U;
             index + 1U < activeFormat.SizeBytes();
             ++index) {
            if (activeFormat[index] == '{' &&
                activeFormat[index + 1U] == '0') {
                std::uint32_t close = index + 2U;
                while (close < activeFormat.SizeBytes() &&
                       activeFormat[close] != '}') {
                    ++close;
                }
                if (close < activeFormat.SizeBytes()) {
                    prefix = activeFormat.Substr(0U, index);
                    suffix = activeFormat.Substr(
                        close + 1U,
                        activeFormat.SizeBytes() - close - 1U);
                }
                break;
            }
        }
        Base::String result;
        Base::Result<void> appended = result.Append(prefix);
        if (appended) appended = result.Append(targetVal.AsString());
        if (appended) appended = result.Append(suffix);
        return appended
            ? Base::Result<Base::String>(std::move(result))
            : Base::Result<Base::String>(appended.GetStatus());
    }
    case ValueKind::Boolean:
        std::snprintf(
            raw, sizeof(raw),
            "%s",
            targetVal.AsBoolean() ? "True" : "False");
        break;
    case ValueKind::SignedInteger: {
        const std::uint64_t rawValue =
            static_cast<std::uint64_t>(targetVal.AsSignedInteger());
        const EnumValueInfo* enumVal = metadata != nullptr &&
            targetVal.Type() != TypeOf<std::int32_t>() &&
            targetVal.Type() != TypeOf<std::int64_t>() &&
            targetVal.Type() != TypeOf<std::int16_t>() &&
            targetVal.Type() != TypeOf<std::int8_t>()
            ? metadata->Types().FindEnumValue(targetVal.Type(), rawValue)
            : nullptr;
        if (enumVal != nullptr) {
            std::snprintf(
                raw, sizeof(raw),
                "%.*s",
                static_cast<int>(enumVal->Name().SizeBytes()),
                enumVal->Name().Data());
        } else {
            numeric = true;
            numericValue = static_cast<double>(
                targetVal.AsSignedInteger());
            std::snprintf(
                raw, sizeof(raw),
                "%lld",
                static_cast<long long>(
                    targetVal.AsSignedInteger()));
        }
        break;
    }
    case ValueKind::UnsignedInteger: {
        const std::uint64_t rawValue = targetVal.AsUnsignedInteger();
        const EnumValueInfo* enumVal = metadata != nullptr &&
            targetVal.Type() != TypeOf<std::uint32_t>() &&
            targetVal.Type() != TypeOf<std::uint64_t>() &&
            targetVal.Type() != TypeOf<std::uint16_t>() &&
            targetVal.Type() != TypeOf<std::uint8_t>()
            ? metadata->Types().FindEnumValue(targetVal.Type(), rawValue)
            : nullptr;
        if (enumVal != nullptr) {
            std::snprintf(
                raw, sizeof(raw),
                "%.*s",
                static_cast<int>(enumVal->Name().SizeBytes()),
                enumVal->Name().Data());
        } else {
            numeric = true;
            numericValue = static_cast<double>(
                targetVal.AsUnsignedInteger());
            std::snprintf(
                raw, sizeof(raw),
                "%llu",
                static_cast<unsigned long long>(
                    targetVal.AsUnsignedInteger()));
        }
        break;
    }
    case ValueKind::Double:
        numeric = true;
        numericValue = targetVal.AsDouble();
        std::snprintf(
            raw, sizeof(raw),
            "%.15g",
            numericValue);
        break;
    case ValueKind::Object:
        if (!targetVal.IsNullObject() &&
            targetVal.AsObject() &&
            targetVal.AsObject()->RuntimeType() ==
                Media::StreamGeometry::StaticTypeId()) {
            Base::String result;
            Base::Result<void> assigned =
                result.Assign(
                    static_cast<Media::StreamGeometry&>(
                        *targetVal.AsObject()).GetData());
            return assigned
                ? Base::Result<Base::String>(
                      std::move(result))
                : Base::Result<Base::String>(
                      assigned.GetStatus());
        }
        if (targetVal.IsNullObject()) {
            raw[0] = '\0';
        } else if (targetVal.AsObject()) {
            const TypeInfo* type = metadata != nullptr
                ? metadata->Types().FindType(targetVal.AsObject()->RuntimeType())
                : nullptr;
            if (type != nullptr) {
                std::snprintf(
                    raw, sizeof(raw), "%.*s",
                    static_cast<int>(type->Name().SizeBytes()),
                    type->Name().Data());
            } else {
                std::snprintf(raw, sizeof(raw), "%s", "Object");
            }
        } else {
            raw[0] = '\0';
        }
        break;
    default:
        return InvalidArgument(
            "Binding value has no default text conversion");
    }

    Base::StringView prefix;
    Base::StringView suffix;
    Base::StringView specifier;
    for (std::uint32_t index = 0U;
         index + 1U < activeFormat.SizeBytes();
         ++index) {
        if (activeFormat[index] != '{' ||
            activeFormat[index + 1U] != '0') {
            continue;
        }
        std::uint32_t close = index + 2U;
        while (close < activeFormat.SizeBytes() &&
               activeFormat[close] != '}') {
            ++close;
        }
        if (close >= activeFormat.SizeBytes()) {
            return InvalidArgument(
                "Binding StringFormat placeholder is incomplete");
        }
        prefix = activeFormat.Substr(0U, index);
        suffix = activeFormat.Substr(
            close + 1U,
            activeFormat.SizeBytes() - close - 1U);
        if (index + 2U < close &&
            activeFormat[index + 2U] == ':') {
            specifier = activeFormat.Substr(
                index + 3U,
                close - index - 3U);
        }
        break;
    }
    if (specifier.Empty() && !activeFormat.Empty() &&
        activeFormat[0] != '{') {
        specifier = activeFormat;
    }

    char formatted[160]{};
    std::uint32_t fixedPrecision = 0U;
    const bool fixedPoint =
        numeric &&
        TryParseFixedPrecision(
            specifier, fixedPrecision);
    std::uint32_t currencyPrecision = 0U;
    const bool currency =
        numeric &&
        TryParseCurrencyPrecision(
            specifier, currencyPrecision);
    const bool zeroPadding =
        numeric &&
        IsZeroPaddingFormat(specifier);
    const bool thousandsScale =
        numeric &&
        specifier == Base::StringView("#,.##");
    if (currency) {
        std::snprintf(
            formatted,
            sizeof(formatted),
            "$%.*f",
            static_cast<int>(currencyPrecision),
            numericValue);
    } else if (fixedPoint) {
        std::snprintf(
            formatted,
            sizeof(formatted),
            "%.*f",
            static_cast<int>(fixedPrecision),
            numericValue);
    } else if (zeroPadding) {
        std::snprintf(
            formatted,
            sizeof(formatted),
            "%0*lld",
            static_cast<int>(specifier.SizeBytes()),
            static_cast<long long>(numericValue));
    } else if (thousandsScale) {
        char decimal[96]{};
        std::snprintf(
            decimal,
            sizeof(decimal),
            "%.2f",
            numericValue / 1000.0);
        std::uint32_t decimalLength =
            static_cast<std::uint32_t>(
                std::strlen(decimal));
        while (decimalLength > 0U &&
               decimal[decimalLength - 1U] == '0') {
            decimal[--decimalLength] = '\0';
        }
        if (decimalLength > 0U &&
            decimal[decimalLength - 1U] == '.') {
            decimal[--decimalLength] = '\0';
        }
        const char* digits = decimal;
        bool negative = *digits == '-';
        if (negative) ++digits;
        const char* point = std::strchr(digits, '.');
        const std::size_t integerLengthBytes =
            point != nullptr
            ? static_cast<std::size_t>(point - digits)
            : std::strlen(digits);
        const std::uint32_t integerLength =
            static_cast<std::uint32_t>(integerLengthBytes);
        std::uint32_t output = 0U;
        if (negative) formatted[output++] = '-';
        for (std::uint32_t index = 0U;
             index < integerLength;
             ++index) {
            if (index != 0U &&
                (integerLength - index) % 3U == 0U) {
                formatted[output++] = ',';
            }
            formatted[output++] = digits[index];
        }
        if (point != nullptr) {
            while (*point != '\0' &&
                   output + 1U < sizeof(formatted)) {
                formatted[output++] = *point++;
            }
        }
        formatted[output] = '\0';
    } else {
        std::snprintf(
            formatted,
            sizeof(formatted),
            "%s",
            raw);
    }

    Base::String result;
    Base::Result<void> appended =
        result.Append(prefix);
    if (appended) {
        appended = result.Append(
            Base::StringView(
                formatted,
                static_cast<std::uint32_t>(
                    std::strlen(formatted))));
    }
    if (appended) {
        appended = result.Append(suffix);
    }
    return appended
        ? Base::Result<Base::String>(
              std::move(result))
        : Base::Result<Base::String>(
              appended.GetStatus());
}

Base::Result<PropertyValue> UnboxItemsValue(
    Base::Result<PropertyValue> value) noexcept {
    if (!value) return value.GetStatus();
    const PropertyValue& current = value.Value();
    if (current.Kind() == ValueKind::Object &&
        !current.IsNullObject() && current.AsObject() &&
        current.AsObject()->RuntimeType() ==
            ::Aero::Controls::BoxedItemValue::StaticTypeId()) {
        return static_cast<const
            ::Aero::Controls::BoxedItemValue&>(
                *current.AsObject()).Value();
    }
    return value;
}


} // namespace Aero::Data

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Data;





Base::Result<std::uint32_t> BindingEngine::Flush() noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!initialized_) {
        return InvalidState("BindingEngine is not initialized");
    }
    if (flushing_) {
        return std::uint32_t{0U};
    }
    if (!hasDirtyBindings_ && pollingBindingCount_ == 0U && pendingDeferredActivations_.Empty()) {
        return std::uint32_t{0U};
    }

    flushing_ = true;
    lastError_ = {};
    std::uint32_t updated = 0U;

    for (std::uint32_t iteration = 0U; iteration < 4U; ++iteration) {
        const std::uint32_t currentCount = bindings_.Size();
        std::uint32_t iterationUpdated = 0U;
        // Equip slots author DataContext="{Binding Player.Slots[i]}" plus
        // Content="{Binding}". Apply DataContext writes first so empty-path
        // Content bindings in the same Flush see the slot object.
        const std::uint32_t startPass = (writesDataContextCount_ > 0U) ? 0U : 1U;
        for (std::uint32_t pass = startPass; pass < 2U; ++pass) {
            for (std::uint32_t index = 0U; index < currentCount; ++index) {
                if (index >= bindings_.Size() || bindings_[index].handle.value == 0U) {
                    continue;
                }
                const bool writesDataContext =
                    bindings_[index].dataContextProperty.IsValid() &&
                    bindings_[index].descriptor.targetProperty == bindings_[index].dataContextProperty;
                if (pass == 0U && !writesDataContext) {
                    continue;
                }
                if (pass == 1U && writesDataContext) {
                    continue;
                }
                if (bindings_[index].descriptor.mode == BindingMode::OneTime && bindings_[index].applied) {
                    bindings_[index].sourceDirty = false;
                    bindings_[index].targetDirty = false;
                    continue;
                }
                const bool metadataPath =
                    bindings_[index].sourceKind != BindingSourceKind::DependencyProperty;
                const bool hasNotify =
                    bindings_[index].notificationSubscription != 0U ||
                    bindings_[index].sourceDependencyProperty.IsValid();
                const bool unresolvedDataContext =
                    bindings_[index].sourceKind == BindingSourceKind::DataContext &&
                    (!bindings_[index].applied || bindings_[index].metadataSource == nullptr ||
                     (!bindings_[index].bindsToSource && !bindings_[index].pathPlan.IsValid()));
                const bool pollMetadata =
                    (metadataPath && !hasNotify) || unresolvedDataContext;
                if (bindings_[index].applied && !bindings_[index].sourceDirty &&
                    !bindings_[index].targetDirty && !pollMetadata) {
                    continue;
                }
                if (pollMetadata && !bindings_[index].pollHintEmitted) {
                    bindings_[index].pollHintEmitted = true;
                    ReportDiagnostic(
                        bindings_[index],
                        BindingDiagnosticStage::ResolveSource,
                        Base::Status::Failure(
                            Base::ErrorCode::Unsupported,
                            "Binding source does not implement NotifyPropertyChanged; "
                            "polling metadata path every frame"));
                }

                Base::Result<PropertyValue> source = ReadSource(bindings_[index]);
                bool usedFallback = false;
                if (!source) {
                    const BindingDiagnosticStage stage =
                        bindings_[index].sourceKind == BindingSourceKind::DataContext &&
                        source.GetStatus().code == Base::ErrorCode::NotFound
                            ? BindingDiagnosticStage::ResolveSource
                            : BindingDiagnosticStage::ReadSource;
                    ReportDiagnostic(bindings_[index], stage, source.GetStatus());
                    if (bindings_[index].descriptor.fallbackValue.IsUnset()) {
                        bindings_[index].applied = false;
                        bindings_[index].sourceDirty = true;
                        continue;
                    }
                    source = bindings_[index].descriptor.fallbackValue;
                    usedFallback = true;
                }
                Base::Result<PropertyValue> target =
                    bindings_[index].descriptor.target->GetValue(bindings_[index].descriptor.targetProperty);
                if (!target) {
                    ReportDiagnostic(
                        bindings_[index],
                        BindingDiagnosticStage::WriteTarget,
                        target.GetStatus());
                    continue;
                }

                const BindingMode mode = bindings_[index].descriptor.mode;
                const bool canReadSource = (mode != BindingMode::OneWayToSource);
                const bool canWriteSource = (mode != BindingMode::OneWay && mode != BindingMode::OneTime);
                const bool sourceChanged = canReadSource && (!bindings_[index].applied ||
                    bindings_[index].sourceDirty ||
                    usedFallback ||
                    (pollMetadata &&
                     source.Value() != bindings_[index].lastSourceValue));
                const bool targetChanged = canWriteSource &&
                    ((bindings_[index].descriptor.updateSourceTrigger ==
                            UpdateSourceTrigger::Explicit ||
                        bindings_[index].descriptor.updateSourceTrigger ==
                            UpdateSourceTrigger::LostFocus)
                    ? bindings_[index].forceSourceUpdate
                    : (!bindings_[index].applied || bindings_[index].targetDirty));
                if (!canReadSource) {
                    bindings_[index].sourceDirty = false;
                }
                if (!canWriteSource &&
                    bindings_[index].descriptor.updateSourceTrigger != UpdateSourceTrigger::LostFocus &&
                    bindings_[index].descriptor.updateSourceTrigger != UpdateSourceTrigger::Explicit) {
                    bindings_[index].targetDirty = false;
                }
                if (!sourceChanged && !targetChanged) {
                    bindings_[index].sourceDirty = false;
                    if (bindings_[index].descriptor.updateSourceTrigger !=
                            UpdateSourceTrigger::LostFocus &&
                        bindings_[index].descriptor.updateSourceTrigger !=
                            UpdateSourceTrigger::Explicit) {
                        bindings_[index].targetDirty = false;
                    }
                    continue;
                }
                Base::Result<void> applied =
                    Base::Status::Failure(
                        Base::ErrorCode::InvalidState,
                        "Binding update was not attempted");
                switch (bindings_[index].descriptor.mode) {
                case BindingMode::OneTime:
                    if (!bindings_[index].applied) {
                        applied = ApplySourceToTarget(
                            bindings_[index],
                            source.Value(),
                            usedFallback,
                            target.Value());
                        if (applied) ++iterationUpdated;
                    }
                    break;
                case BindingMode::Default:
                case BindingMode::OneWay:
                    if (sourceChanged) {
                        applied = ApplySourceToTarget(
                            bindings_[index],
                            source.Value(),
                            usedFallback,
                            target.Value());
                        if (applied) ++iterationUpdated;
                    }
                    break;
                case BindingMode::OneWayToSource:
                    if (targetChanged) {
                        applied = ApplyTargetToSource(
                            bindings_[index],
                            target.Value(),
                            source.Value());
                        if (applied) ++iterationUpdated;
                    }
                    break;
                case BindingMode::TwoWay:
                    // A target edit (ToggleButton click) must write back even when
                    // metadata-path polling reports a unchanged source as "changed".
                    // Source still wins when the source property itself is dirty.
                    if (targetChanged && !bindings_[index].sourceDirty) {
                        applied = ApplyTargetToSource(
                            bindings_[index],
                            target.Value(),
                            source.Value());
                        if (applied) ++iterationUpdated;
                    } else if (sourceChanged) {
                        applied = ApplySourceToTarget(
                            bindings_[index],
                            source.Value(),
                            usedFallback,
                            target.Value());
                        if (applied) ++iterationUpdated;
                    } else if (targetChanged) {
                        applied = ApplyTargetToSource(
                            bindings_[index],
                            target.Value(),
                            source.Value());
                        if (applied) ++iterationUpdated;
                    }
                    break;
                }
                if (index >= bindings_.Size() || bindings_[index].handle.value == 0U) {
                    continue;
                }
                if (!applied && (sourceChanged || targetChanged)) {
                    if (canReadSource && sourceChanged) {
                        bindings_[index].sourceDirty = true;
                    }
                    if (canWriteSource && targetChanged) {
                        bindings_[index].targetDirty = true;
                    }
                    continue;
                }
                bindings_[index].lastSourceValue = source.Value();
                bindings_[index].lastTargetValue = target.Value();
                bindings_[index].applied = true;
                bindings_[index].lastStatus = {};
                bindings_[index].sourceDirty = false;
                bindings_[index].targetDirty = false;
                bindings_[index].forceSourceUpdate = false;
            }
        }
        updated += iterationUpdated;
        if (iterationUpdated == 0U && bindings_.Size() == currentCount) {
            break;
        }
    }
    flushing_ = false;
    std::uint32_t pollCount = 0U;
    bool anyDirty = false;
    for (std::uint32_t index = 0U; index < bindings_.Size(); ++index) {
        if (bindings_[index].handle.value == 0U) continue;
        const bool metadataPath =
            bindings_[index].sourceKind != BindingSourceKind::DependencyProperty;
        const bool hasNotify =
            bindings_[index].notificationSubscription != 0U ||
            bindings_[index].sourceDependencyProperty.IsValid();
        const bool unresolvedDataContext =
            bindings_[index].sourceKind == BindingSourceKind::DataContext &&
            (!bindings_[index].applied || bindings_[index].metadataSource == nullptr ||
             (!bindings_[index].bindsToSource && !bindings_[index].pathPlan.IsValid()));
        const bool pollMetadata =
            (metadataPath && !hasNotify) || unresolvedDataContext;
        if (pollMetadata) {
            ++pollCount;
        }
        const bool isOneTimeApplied =
            bindings_[index].descriptor.mode == BindingMode::OneTime && bindings_[index].applied;
        if (!isOneTimeApplied && (!bindings_[index].applied || bindings_[index].sourceDirty || bindings_[index].targetDirty)) {
            anyDirty = true;
        }
    }
    pollingBindingCount_ = pollCount;
    hasDirtyBindings_ = anyDirty;

    if (hasPendingDetaches_) {
        hasPendingDetaches_ = false;
        std::uint32_t writeIndex = 0U;
        for (std::uint32_t readIndex = 0U; readIndex < bindings_.Size(); ++readIndex) {
            if (bindings_[readIndex].handle.value != 0U) {
                if (writeIndex != readIndex) {
                    bindings_[writeIndex] = std::move(bindings_[readIndex]);
                    static_cast<void>(handleIndexMap_.Set(bindings_[writeIndex].handle.value, writeIndex));
                }
                ++writeIndex;
            }
        }
        bindings_.Resize(writeIndex);
    }
    return updated;
}

Base::Result<std::uint32_t>
BindingEngine::InspectBindings(
    const DependencyObject& object,
    Base::Vector<BindingInspection>&
        output) const noexcept {
    output.Clear();
    for (const BindingRecord& record :
        bindings_) {
        Base::Object* source =
            record.sourceKind ==
                BindingSourceKind::
                    DependencyProperty
            ? static_cast<Base::Object*>(
                record.descriptor.source)
            : record.metadataSource;
        if (source != &object &&
            record.descriptor.target !=
                &object) {
            continue;
        }
        BindingInspection inspection;
        inspection.handle = record.handle;
        inspection.source = source;
        inspection.target =
            record.descriptor.target;
        inspection.sourceProperty =
            record.descriptor.
                sourceProperty;
        inspection.targetProperty =
            record.descriptor.
                targetProperty;
        inspection.mode =
            record.descriptor.mode;
        inspection.updateSourceTrigger =
            record.descriptor.
                updateSourceTrigger;
        inspection.usesDataContext =
            record.sourceKind ==
                BindingSourceKind::
                    DataContext;
        inspection.applied =
            record.applied;
        Base::Result<void> assigned =
            inspection.path.Assign(
                record.path.View());
        if (!assigned) {
            output.Clear();
            return assigned.GetStatus();
        }
        output.PushBack(
                std::move(inspection));
    }
    return output.Size();
}

void BindingEngine::DataBindHook(void* context) noexcept {
    BindingEngine* manager = static_cast<BindingEngine*>(context);
    if (manager == nullptr) return;
    constexpr std::uint32_t MaximumActivationWaves = 16U;
    for (std::uint32_t wave = 0U;
         wave < MaximumActivationWaves; ++wave) {
        // Apply already-attached bindings even when a deferred template
        // Binding fails to activate. Sample trees (Inventory, QuestLog)
        // attach ItemsSource="{Binding ...}" on the live tree, then expand
        // ControlTemplates that can reject individual deferred Bindings.
        Base::Result<std::uint32_t> flushed = manager->Flush();
        if (!flushed) {
            manager->RecordError(flushed.GetStatus());
        }
        Base::Result<std::uint32_t> activated =
            manager->ActivatePendingDeferred();
        if (!activated) {
            manager->RecordError(activated.GetStatus());
        }
        flushed = manager->Flush();
        if (!flushed) {
            manager->RecordError(flushed.GetStatus());
            return;
        }
        if (manager->pendingDeferredActivations_.Empty()) return;
    }
    manager->RecordError(Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Deferred Binding activation exceeded the bounded DataBind waves"));
}

void BindingEngine::OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept {
    hasDirtyBindings_ = true;
    bool lostFocusFlush = false;

    if (const auto* targetList = targetToBindings_.Find(&object)) {
        for (BindingHandle handle : *targetList) {
            BindingRecord* record = FindRecord(handle);
            if (record == nullptr) continue;
            if (record->descriptor.targetProperty == args.GetProperty()) {
                record->targetDirty = true;
            }
            if (record->lostFocusSubscribed &&
                record->descriptor.target == &object &&
                args.GetProperty() == UIElement::IsKeyboardFocusedProperty.Handle() &&
                record->descriptor.updateSourceTrigger == UpdateSourceTrigger::LostFocus) {
                const PropertyValue& next = args.GetNewValue();
                const bool focused =
                    next.Kind() == ValueKind::Boolean && next.AsBoolean();
                if (!focused) {
                    record->forceSourceUpdate = true;
                    record->targetDirty = true;
                    lostFocusFlush = true;
                }
            }
        }
    }

    if (const auto* sourceList = sourceToBindings_.Find(&object)) {
        for (BindingHandle handle : *sourceList) {
            BindingRecord* record = FindRecord(handle);
            if (record == nullptr) continue;
            if (record->sourceKind == BindingSourceKind::DependencyProperty &&
                record->descriptor.source == &object &&
                record->descriptor.sourceProperty == args.GetProperty()) {
                record->sourceDirty = true;
            }
            if (record->sourceDependencyProperty.IsValid() &&
                record->metadataSource == &object &&
                record->sourceDependencyProperty == args.GetProperty()) {
                record->sourceDirty = true;
            }
            if (record->sourceKind == BindingSourceKind::DataContext &&
                record->dataContextProperty == args.GetProperty() &&
                record->dataContextOwner == &object) {
                ReleaseMetadataSource(*record);
                record->metadataSource = nullptr;
                record->pathPlan = {};
                record->sourceDirty = true;
                record->applied = false;
            }
        }
    }

    if (!dataContextBindings_.Empty()) {
        for (BindingHandle handle : dataContextBindings_) {
            BindingRecord* record = FindRecord(handle);
            if (record == nullptr) continue;
            if (record->sourceKind == BindingSourceKind::DataContext &&
                record->dataContextProperty == args.GetProperty() &&
                record->dataContextOwner != &object &&
                BindingOwnerSeesDataContextChange(record->dataContextOwner, object)) {
                ReleaseMetadataSource(*record);
                record->metadataSource = nullptr;
                record->pathPlan = {};
                record->sourceDirty = true;
                record->applied = false;
            }
        }
    }

    if (lostFocusFlush && !flushing_ && initialized_) {
        static_cast<void>(Flush());
    }
}

void BindingEngine::OnMetadataPropertyChanged(
    Base::Object& object,
    MemberId property) noexcept {
    hasDirtyBindings_ = true;
    if (const auto* sourceList = sourceToBindings_.Find(&object)) {
        for (BindingHandle handle : *sourceList) {
            BindingRecord* record = FindRecord(handle);
            if (record == nullptr) continue;
            if (record->sourceKind == BindingSourceKind::DependencyProperty ||
                record->metadataSource != &object ||
                !record->pathPlan.IsValid() ||
                record->pathPlan.Segments().Empty()) {
                continue;
            }
            const BindingPathSegment& first =
                record->pathPlan.Segments()[0];
            if (property == InvalidMemberId ||
                first.member == property ||
                first.dynamic) {
                record->sourceDirty = true;
            }
        }
    }
}

void BindingEngine::MetadataPropertyChanged(
    Base::Object& object,
    MemberId property,
    void* context) noexcept {
    BindingEngine* manager = static_cast<BindingEngine*>(context);
    if (manager != nullptr) {
        manager->OnMetadataPropertyChanged(object, property);
    }
}

static bool FallbackValueMatchesTarget(
    const TypeRegistry& types,
    TypeId valueType,
    TypeId targetType) noexcept {
    if (valueType == targetType ||
        targetType == TypeOf<Base::Object>() ||
        targetType == TypeOf<Meta::Value>() ||
        valueType == TypeOf<Base::Object>() ||
        valueType == TypeOf<Meta::Value>()) {
        return true;
    }
    return types.IsAssignableFrom(targetType, valueType) ||
        HasDefaultTargetConversion(valueType, targetType);
}

Base::Result<void> BindingEngine::VerifyDescriptor(
    const BindingDescriptor& descriptor) const noexcept {
    if (descriptor.source == nullptr || descriptor.target == nullptr ||
        !descriptor.sourceProperty.IsValid() ||
        !descriptor.targetProperty.IsValid()) {
        return InvalidArgument("Binding descriptor is incomplete");
    }
    if (&descriptor.source->GetDispatcher() != dispatcher_ ||
        &descriptor.target->GetDispatcher() != dispatcher_) {
        return InvalidArgument("Binding source and target must use this Dispatcher");
    }
    Base::Result<PropertyValue> source =
        descriptor.source->GetValue(descriptor.sourceProperty);
    if (!source) {
        return source.GetStatus();
    }
    Base::Result<PropertyValue> target =
        descriptor.target->GetValue(descriptor.targetProperty);
    if (!target) {
        return target.GetStatus();
    }
    if (source.Value().Type() != target.Value().Type() &&
        descriptor.convert == nullptr &&
        !descriptor.converterResource &&
        !HasDefaultTargetConversion(
            source.Value().Type(), target.Value().Type())) {
        return InvalidArgument("Binding source and target property types differ");
    }
    if ((descriptor.mode == BindingMode::TwoWay ||
         descriptor.mode == BindingMode::OneWayToSource) &&
        source.Value().Type() != target.Value().Type() &&
        descriptor.convertBack == nullptr &&
        !descriptor.converterResource &&
        !HasDefaultTargetConversion(
            target.Value().Type(),
            source.Value().Type())) {
        return InvalidArgument(
            "Binding requires ConvertBack for different source and target types");
    }
    const TypeRegistry& types = DependencyObjectAccess::PropertyRegistry(*descriptor.target).Types();
    if (!descriptor.fallbackValue.IsUnset() &&
        !FallbackValueMatchesTarget(types, descriptor.fallbackValue.Type(), target.Value().Type())) {
        return InvalidArgument(
            "Binding fallback value type differs from the target property");
    }
    if (!descriptor.targetNullValue.IsUnset() &&
        !FallbackValueMatchesTarget(types, descriptor.targetNullValue.Type(), target.Value().Type())) {
        return InvalidArgument(
            "Binding target-null value type differs from the target property");
    }
    return {};
}

Base::Result<void> BindingEngine::VerifyDescriptor(
    const MetadataBindingDescriptor& descriptor) const noexcept {
    if (descriptor.metadata == nullptr ||
        !descriptor.metadata->IsReady() ||
        descriptor.target == nullptr ||
        !descriptor.targetProperty.IsValid() ||
        (descriptor.path.Empty() && !descriptor.bindsToSource) ||
        (descriptor.source == nullptr &&
         !descriptor.dataContextProperty.IsValid())) {
        return InvalidArgument("Metadata binding descriptor is incomplete");
    }
    if (&descriptor.target->GetDispatcher() != dispatcher_) {
        return InvalidArgument(
            "Binding target must use this Dispatcher");
    }
    Base::Result<PropertyValue> target =
        descriptor.target->GetValue(descriptor.targetProperty);
    if (!target) return target.GetStatus();
    const TypeRegistry& metaTypes = descriptor.metadata->Types();
    if (!descriptor.fallbackValue.IsUnset() &&
        !FallbackValueMatchesTarget(metaTypes, descriptor.fallbackValue.Type(), target.Value().Type())) {
        return InvalidArgument(
            "Binding fallback value type differs from the target property");
    }
    if (!descriptor.targetNullValue.IsUnset() &&
        !FallbackValueMatchesTarget(metaTypes, descriptor.targetNullValue.Type(), target.Value().Type())) {
        return InvalidArgument(
            "Binding target-null value type differs from the target property");
    }
    if (descriptor.source != nullptr &&
        (descriptor.source->RuntimeType() == InvalidTypeId ||
        descriptor.metadata->Types().FindType(
            descriptor.source->RuntimeType()) == nullptr)) {
        return InvalidArgument(
            "Binding metadata source has no registered runtime type");
    }
    return {};
}

Base::Result<void> BindingEngine::ResolveMetadataSource(
    BindingRecord& record) noexcept {
    if (record.sourceKind == BindingSourceKind::MetadataObject) {
        return record.metadataSource != nullptr
            ? Base::Result<void>()
            : Base::Result<void>(InvalidState(
                "Binding source object is not resolved"));
    }
    if (record.sourceKind == BindingSourceKind::MetadataPath) {
        return record.metadataSource != nullptr &&
            record.pathPlan.IsValid()
            ? Base::Result<void>()
            : Base::Result<void>(InvalidState(
                "Binding metadata source is not resolved"));
    }
    if (record.dataContextOwner == nullptr) {
        ReleaseMetadataSource(record);
        record.metadataSource = nullptr;
        record.pathPlan = {};
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Binding target has no DataContext object");
    }
    Base::Result<PropertyValue> dataContext = ReadDataContextValue(
        *record.dataContextOwner,
        record.dataContextProperty);
    if (!dataContext ||
        dataContext.Value().Kind() != ValueKind::Object ||
        dataContext.Value().IsNullObject()) {
        DependencyObject* node = record.dataContextOwner;
        for (std::uint32_t depth = 0U; depth < 64U && node != nullptr; ++depth) {
            node = BindingParent(*node);
            if (node == nullptr) {
                continue;
            }
            Base::Result<PropertyValue> ancestor = ReadDataContextValue(
                *node,
                record.dataContextProperty);
            if (!ancestor) {
                continue;
            }
            if (ancestor.Value().Kind() == ValueKind::Object &&
                !ancestor.Value().IsNullObject()) {
                dataContext = std::move(ancestor);
                break;
            }
        }
    }
    if (!dataContext ||
        dataContext.Value().Kind() != ValueKind::Object ||
        dataContext.Value().IsNullObject()) {
        ReleaseMetadataSource(record);
        record.metadataSource = nullptr;
        record.pathPlan = {};
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Binding target has no DataContext object");
    }

    Base::Object* source = dataContext.Value().AsObject().Get();
    if (source == record.metadataSource &&
        (record.bindsToSource || record.pathPlan.IsValid())) {
        return {};
    }
    ReleaseMetadataSource(record);
    record.metadataSource = nullptr;
    record.pathPlan = {};

    if (record.bindsToSource) {
        const DependencyProperty* targetProperty =
            DependencyObjectAccess::PropertyRegistry(record.descriptor.target).Find(
                record.descriptor.targetProperty);
        if (targetProperty == nullptr ||
            (!targetProperty->AcceptsAnyValue() &&
            (record.descriptor.convert == nullptr &&
             !record.descriptor.converterResource &&
             !record.metadata->Types().IsAssignableFrom(
                 targetProperty->ValueType(),
                 source->RuntimeType())))) {
            return BindingTypeMismatch(
                record.metadata->Types(),
                Base::StringView("."),
                source->RuntimeType(),
                *record.descriptor.target,
                targetProperty);
        }
        if (record.descriptor.mode == BindingMode::TwoWay ||
            record.descriptor.mode == BindingMode::OneWayToSource) {
            return Base::Status::Failure(
                Base::ErrorCode::ReadOnly,
                "Binding to the DataContext object does not support writeback");
        }
        record.metadataSource = source;
        AddSourceBinding(record.metadataSource, record.handle);
        record.sourceDirty = true;
        record.applied = false;
        return {};
    }

    BindingPathCompileError compileError;
    Base::Result<BindingPathPlan> compiled =
        BindingPathPlan::Compile(
            *record.metadata,
            source->RuntimeType(),
            record.path.View(),
            &compileError);
        if (!compiled) {
            return BindingPathFailure(
                record.metadata->Types(),
                record.path.View(),
                compileError,
                compiled.GetStatus());
        }
    const DependencyProperty* targetProperty =
        DependencyObjectAccess::PropertyRegistry(record.descriptor.target).Find(
            record.descriptor.targetProperty);
    if (targetProperty == nullptr ||
        (record.descriptor.convert == nullptr &&
         !record.descriptor.converterResource &&
         !TargetAcceptsPathResult(
             record.metadata->Types(),
             targetProperty,
             compiled.Value().ResultType(),
             compiled.Value().HasDynamicResult()))) {
        return BindingTypeMismatch(
            record.metadata->Types(),
            record.path.View(),
            compiled.Value().ResultType(),
            *record.descriptor.target,
            targetProperty);
    }
    if ((record.descriptor.mode == BindingMode::TwoWay ||
         record.descriptor.mode == BindingMode::OneWayToSource) &&
        !compiled.Value().CanWrite()) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Binding source path is not writable");
    }
    if ((record.descriptor.mode == BindingMode::TwoWay ||
         record.descriptor.mode == BindingMode::OneWayToSource) &&
        !compiled.Value().HasDynamicResult() &&
        targetProperty->ValueType() != compiled.Value().ResultType() &&
        record.descriptor.convertBack == nullptr &&
        !record.descriptor.converterResource &&
        !HasDefaultTargetConversion(
            targetProperty->ValueType(),
            compiled.Value().ResultType()) &&
        !CanRoundTripObjectValue(
            record.metadata->Types(),
            compiled.Value().ResultType(),
            targetProperty->ValueType())) {
        return InvalidArgument(
            "Binding requires ConvertBack for different source and target types");
    }
    record.metadataSource = source;
    record.pathPlan = std::move(compiled).Value();
    AddSourceBinding(record.metadataSource, record.handle);
    Base::Result<void> subscribed =
        SubscribeMetadataSource(record);
    if (!subscribed) {
        RemoveSourceBinding(record.metadataSource, record.handle);
        record.metadataSource = nullptr;
        record.pathPlan = {};
        return subscribed.GetStatus();
    }
    record.sourceDirty = true;
    record.applied = false;
    return {};
}









void BindingEngine::ReportDiagnostic(
    BindingRecord& record,
    BindingDiagnosticStage stage,
    Base::Status status) noexcept {
    lastError_ = status;
    record.lastStatus = status;
    record.conversionFailureStage = stage;
    if (record.descriptor.diagnostic != nullptr) {
        record.descriptor.diagnostic(
            {record.handle, stage, status},
            record.descriptor.diagnosticContext);
    }
}

Base::Result<void> BindingEngine::SubscribeMetadataSource(
    BindingRecord& record) noexcept {
    if (record.metadata == nullptr ||
        record.metadataSource == nullptr) {
        return InvalidArgument(
            "Binding metadata source subscription is incomplete");
    }
    Base::Result<std::uint64_t> subscribed =
        record.metadata->SubscribePropertyChanged(
        *record.metadataSource,
        &BindingEngine::MetadataPropertyChanged,
        this);
    if (!subscribed) return subscribed.GetStatus();
    record.notificationSubscription = subscribed.Value();
    record.sourceDependencyProperty = {};
    if (!record.bindsToSource &&
        !record.pathPlan.Segments().Empty()) {
        if (auto* sourceObject =
                ::Aero::TryCast<::Aero::DependencyObject>(
                    record.metadataSource)) {
            const BindingPathSegment& first =
                record.pathPlan.Segments()[0];
            if (!first.dynamic && first.member != InvalidMemberId) {
                DependencyPropertyHandle handle{first.member};
                if (DependencyObjectAccess::PropertyRegistry(sourceObject).Find(handle) !=
                    nullptr) {
                    sourceObject->AddValueChangedHandler(
                        handle, propertyChangedHandler_);
                    record.sourceDependencyProperty = handle;
                }
            }
        }
    }
    return {};
}

void BindingEngine::ReleaseMetadataSource(
    BindingRecord& record) noexcept {
    if (record.metadataSource != nullptr) {
        RemoveSourceBinding(record.metadataSource, record.handle);
    }
    if (record.sourceDependencyProperty.IsValid() &&
        record.metadataSource != nullptr) {
        if (auto* sourceObject =
                ::Aero::TryCast<::Aero::DependencyObject>(
                    record.metadataSource)) {
            (void)sourceObject->RemoveValueChangedHandler(
                record.sourceDependencyProperty,
                propertyChangedHandler_);
        }
    }
    record.sourceDependencyProperty = {};
    if (record.notificationSubscription != 0U &&
        record.metadata != nullptr &&
        record.metadataSource != nullptr) {
        (void)record.metadata->UnsubscribePropertyChanged(
            *record.metadataSource,
            record.notificationSubscription);
    }
    record.notificationSubscription = 0U;
}

void BindingEngine::CleanupRecord(BindingRecord& record) noexcept {
    if (record.descriptor.target != nullptr) {
        RemoveTargetBinding(record.descriptor.target, record.handle);
    }
    if (record.sourceKind == BindingSourceKind::DependencyProperty) {
        if (record.descriptor.source != nullptr) {
            RemoveSourceBinding(record.descriptor.source, record.handle);
            if (record.descriptor.sourceProperty.IsValid()) {
                (void)record.descriptor.source->RemoveValueChangedHandler(
                    record.descriptor.sourceProperty,
                    propertyChangedHandler_);
            }
        }
    } else {
        ReleaseMetadataSource(record);
        if (record.sourceKind == BindingSourceKind::DataContext) {
            if (record.dataContextOwner != nullptr) {
                RemoveSourceBinding(record.dataContextOwner, record.handle);
                if (record.dataContextProperty.IsValid()) {
                    (void)record.dataContextOwner->RemoveValueChangedHandler(
                        record.dataContextProperty,
                        propertyChangedHandler_);
                }
            }
            RemoveDataContextBinding(record.handle);
        }
    }
    if (record.descriptor.target != nullptr && record.descriptor.targetProperty.IsValid()) {
        (void)record.descriptor.target->RemoveValueChangedHandler(
            record.descriptor.targetProperty, propertyChangedHandler_);
    }
    UnsubscribeLostFocus(record);
}

void BindingEngine::RemoveAt(std::uint32_t index) noexcept {
    if (index >= bindings_.Size()) return;
    if (bindings_[index].dataContextProperty.IsValid() &&
        bindings_[index].descriptor.targetProperty == bindings_[index].dataContextProperty &&
        writesDataContextCount_ > 0U) {
        --writesDataContextCount_;
    }
    hasDirtyBindings_ = true;
    handleIndexMap_.Erase(bindings_[index].handle.value);
    CleanupRecord(bindings_[index]);
    bindings_.Erase(index);
    for (std::uint32_t current = index; current < bindings_.Size(); ++current) {
        static_cast<void>(handleIndexMap_.Set(bindings_[current].handle.value, current));
    }
}

void BindingEngine::AddTargetBinding(const DependencyObject* target, BindingHandle handle) noexcept {
    if (target == nullptr || handle.value == 0U) return;
    auto* found = targetToBindings_.Find(target);
    if (found != nullptr) {
        found->PushBack(handle);
    } else {
        Base::Vector<BindingHandle> vec;
        vec.PushBack(handle);
        static_cast<void>(targetToBindings_.Insert(target, std::move(vec)));
    }
}

void BindingEngine::RemoveTargetBinding(const DependencyObject* target, BindingHandle handle) noexcept {
    if (target == nullptr || handle.value == 0U) return;
    auto* found = targetToBindings_.Find(target);
    if (found != nullptr) {
        for (std::uint32_t i = 0U; i < found->Size(); ++i) {
            if ((*found)[i].value == handle.value) {
                found->Erase(i);
                break;
            }
        }
        if (found->Empty()) {
            targetToBindings_.Erase(target);
        }
    }
}

void BindingEngine::AddSourceBinding(const Base::Object* source, BindingHandle handle) noexcept {
    if (source == nullptr || handle.value == 0U) return;
    auto* found = sourceToBindings_.Find(source);
    if (found != nullptr) {
        found->PushBack(handle);
    } else {
        Base::Vector<BindingHandle> vec;
        vec.PushBack(handle);
        static_cast<void>(sourceToBindings_.Insert(source, std::move(vec)));
    }
}

void BindingEngine::RemoveSourceBinding(const Base::Object* source, BindingHandle handle) noexcept {
    if (source == nullptr || handle.value == 0U) return;
    auto* found = sourceToBindings_.Find(source);
    if (found != nullptr) {
        for (std::uint32_t i = 0U; i < found->Size(); ++i) {
            if ((*found)[i].value == handle.value) {
                found->Erase(i);
                break;
            }
        }
        if (found->Empty()) {
            sourceToBindings_.Erase(source);
        }
    }
}

void BindingEngine::AddDataContextBinding(BindingHandle handle) noexcept {
    if (handle.value == 0U) return;
    dataContextBindings_.PushBack(handle);
}

void BindingEngine::RemoveDataContextBinding(BindingHandle handle) noexcept {
    for (std::uint32_t i = 0U; i < dataContextBindings_.Size(); ++i) {
        if (dataContextBindings_[i].value == handle.value) {
            dataContextBindings_.Erase(i);
            break;
        }
    }
}

} // namespace Aero

// Metadata registration for the types implemented in this file.
namespace Aero::Data {

AERO_DESCRIBE(IValueConverter) {
    using namespace Aero::Meta;
    Register<IValueConverter>(context, TypeFlags::Abstract);
}

AERO_DESCRIBE(IMultiValueConverter) {
    using namespace Aero::Meta;
    Register<IMultiValueConverter>(context, TypeFlags::Abstract);
}

AERO_DESCRIBE(BooleanToVisibilityConverter) {
    using namespace Aero::Meta;
    Register<BooleanToVisibilityConverter>(context)
            .Factory();
}

AERO_DESCRIBE(BindingBase) {
    using namespace Aero::Meta;
    Register<BindingBase>(context, TypeFlags::Abstract);
}

AERO_DESCRIBE(RelativeSource) {
    using namespace Aero::Meta;
    Register<RelativeSource>(context)
            .Property("Mode", &RelativeSource::GetMode, &RelativeSource::SetMode)
            .Property("AncestorType", &RelativeSource::GetAncestorType, &RelativeSource::SetAncestorType)
            .Property("AncestorLevel", &RelativeSource::GetAncestorLevel, &RelativeSource::SetAncestorLevel)
            .Factory();
}

AERO_DESCRIBE(Binding) {
    using namespace Aero::Meta;
    Register<Binding>(context, TypeFlags::MarkupExtension | TypeFlags::Sealed)
            .Property("Path", &Binding::GetPathText, static_cast<void (Binding::*)(Base::StringView) noexcept>(&Binding::SetPath))
            .Property("ElementName", &Binding::GetElementName, &Binding::SetElementName)
            .Property("Mode", &Binding::GetMode, &Binding::SetMode)
            .Property("UpdateSourceTrigger", &Binding::GetUpdateSourceTrigger, &Binding::SetUpdateSourceTrigger)
            .Property("StringFormat", &Binding::GetStringFormat, &Binding::SetStringFormat)
            .Property<Value, &Binding::GetFallbackValue, &Binding::SetFallbackValue>("FallbackValue", PropertyFlags::AnyValue)
            .Property<Value, &Binding::GetTargetNullValue, &Binding::SetTargetNullValue>("TargetNullValue", PropertyFlags::AnyValue)
            .Property("Source", &Binding::GetSource, &Binding::SetSource)
            .Property("RelativeSource", &Binding::GetRelativeSource, &Binding::SetRelativeSource)
            .Property("Converter", &Binding::GetConverter, &Binding::SetConverter)
            .Property<Value, &Binding::GetConverterParameter, &Binding::SetConverterParameter>("ConverterParameter", PropertyFlags::AnyValue)
            .Factory();
}

AERO_DESCRIBE(MultiBinding) {
    using namespace Aero::Meta;
    Register<MultiBinding>(context, TypeFlags::MarkupExtension | TypeFlags::Sealed)
            .Property("StringFormat", &MultiBinding::GetStringFormat, &MultiBinding::SetStringFormat)
            .Property<Value, &MultiBinding::GetFallbackValue, &MultiBinding::SetFallbackValue>("FallbackValue", PropertyFlags::AnyValue)
            .Property<Value, &MultiBinding::GetTargetNullValue, &MultiBinding::SetTargetNullValue>("TargetNullValue", PropertyFlags::AnyValue)
            .Property("Converter", &MultiBinding::GetConverter, &MultiBinding::SetConverter)
            .Property<Value, &MultiBinding::GetConverterParameter, &MultiBinding::SetConverterParameter>("ConverterParameter", PropertyFlags::AnyValue)
            .Content<Binding>("Bindings", ContentKind::Collection,
                [](Base::Object& owner,
                   const Base::Ref<Base::Object>& value,
                   void*) noexcept {
                    if (!value) return;
                    Base::Ref<Binding> retained =
                        Base::Ref<Binding>::TryFromBorrowed(
                            static_cast<Binding&>(*value));
                    if (retained) {
                        static_cast<MultiBinding&>(owner)
                            .AddBinding(std::move(retained));
                    }
                },
                [](Base::Object& owner, void*) noexcept {
                    static_cast<MultiBinding&>(owner)
                        .ClearBindings();
                })
            .Factory();
}

AERO_DESCRIBE(MultiBindingProxy) {
    using namespace Aero::Meta;
    Register<MultiBindingProxy>(context)
            .Property(MultiBindingProxy::ValueProperty, Value::NullObject(TypeOf<Base::Object>()))
            .Factory();
}

MetadataBindingDescriptor MetadataBindingDescriptor::FromBinding(
    const Data::Binding& binding,
    DependencyObject& target,
    DependencyPropertyHandle targetProperty,
    Meta::Registry* metadata,
    Base::Object* explicitSource,
    DependencyObject* dataContextOwner) noexcept {
    MetadataBindingDescriptor descriptor;
    descriptor.metadata = metadata;
    descriptor.source = explicitSource;
    descriptor.target = &target;
    descriptor.targetProperty = targetProperty;
    const bool isExplicit = explicitSource != nullptr ||
        binding.GetSource() ||
        !binding.GetElementName().Empty() ||
        binding.GetRelativeSource();
    if (!isExplicit) {
        descriptor.dataContextProperty =
            FrameworkElement::DataContextProperty.Handle();
        descriptor.dataContextOwner =
            dataContextOwner != nullptr ? dataContextOwner : &target;
    } else if (dataContextOwner != nullptr) {
        descriptor.dataContextOwner = dataContextOwner;
    }
    descriptor.path = binding.GetPathText();
    descriptor.stringFormat = binding.GetStringFormat();
    descriptor.bindsToSource = binding.GetPath().GetIsEmpty();
    descriptor.mode = BindingEngine::ResolveBindingMode(
        target, targetProperty, binding.GetMode());
    descriptor.updateSourceTrigger =
        BindingEngine::ResolveUpdateSourceTrigger(
            target, targetProperty, binding.GetUpdateSourceTrigger());
    descriptor.converterResource = binding.GetConverter();
    descriptor.converterParameter = binding.GetConverterParameter();
    descriptor.fallbackValue = binding.GetFallbackValue();
    descriptor.targetNullValue = binding.GetTargetNullValue();
    return descriptor;
}

} // namespace Aero::Data

