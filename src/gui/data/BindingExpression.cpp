#include <Aero/Data/BindingExpression.hpp>
#include <Aero/Data/BindingOperations.hpp>
#include <Aero/UIElement.hpp>

#include "gui/data/BindingCommon.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/controls/ControlBehavior.hpp"

namespace Aero::Data {

BindingEngine* BindingExpression::EngineOf(const BindingHandle& handle) noexcept {
    return handle.IsValid()
        ? static_cast<BindingEngine*>(handle.engine_)
        : nullptr;
}

namespace {

Base::Status InvalidExpression() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "BindingExpression handle is invalid");
}

} // namespace

bool BindingExpression::IsValid() const noexcept {
    BindingEngine* engine = BindingExpression::EngineOf(handle_);
    return engine != nullptr && engine->Contains(handle_);
}

BindingStatus BindingExpression::Status() const noexcept {
    BindingEngine* engine = BindingExpression::EngineOf(handle_);
    if (engine == nullptr) return BindingStatus::Unattached;
    return engine->QueryStatus(handle_);
}

Base::Status BindingExpression::UpdateSource() noexcept {
    BindingEngine* engine = BindingExpression::EngineOf(handle_);
    if (engine == nullptr || !engine->Contains(handle_)) {
        return InvalidExpression();
    }
    Base::Result<bool> updated = engine->UpdateSource(handle_);
    if (!updated) return updated.GetStatus();
    if (!updated.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "BindingExpression cannot update the source");
    }
    Base::Result<std::uint32_t> flushed = engine->Flush();
    return flushed ? Base::Status{} : flushed.GetStatus();
}

Base::Status BindingExpression::UpdateTarget() noexcept {
    BindingEngine* engine = BindingExpression::EngineOf(handle_);
    if (engine == nullptr || !engine->Contains(handle_)) {
        return InvalidExpression();
    }
    Base::Result<bool> updated = engine->UpdateTarget(handle_);
    if (!updated) return updated.GetStatus();
    if (!updated.Value()) return InvalidExpression();
    return {};
}

bool MultiBindingExpression::IsValid() const noexcept {
    if (handles_.Empty()) return false;
    for (std::uint32_t index = 0U; index < handles_.Size(); ++index) {
        BindingEngine* engine = BindingExpression::EngineOf(handles_[index]);
        if (engine == nullptr || !engine->Contains(handles_[index])) {
            return false;
        }
    }
    return true;
}

BindingStatus MultiBindingExpression::Status() const noexcept {
    if (!IsValid()) return BindingStatus::Unattached;
    BindingStatus status = BindingStatus::Active;
    for (std::uint32_t index = 0U; index < handles_.Size(); ++index) {
        BindingEngine* engine = BindingExpression::EngineOf(handles_[index]);
        const BindingStatus child = engine->QueryStatus(handles_[index]);
        if (child == BindingStatus::UpdateSourceError ||
            child == BindingStatus::UpdateTargetError) {
            return child;
        }
        if (child == BindingStatus::Inactive) {
            status = BindingStatus::Inactive;
        }
    }
    return status;
}

Base::Status MultiBindingExpression::UpdateSource() noexcept {
    if (!IsValid()) return InvalidExpression();
    for (std::uint32_t index = 0U; index < handles_.Size(); ++index) {
        BindingExpression child(handles_[index]);
        Base::Status status = child.UpdateSource();
        if (!status.IsOk() &&
            status.code != Base::ErrorCode::InvalidState) {
            return status;
        }
    }
    return {};
}

Base::Status MultiBindingExpression::UpdateTarget() noexcept {
    if (!IsValid()) return InvalidExpression();
    for (std::uint32_t index = 0U; index < handles_.Size(); ++index) {
        BindingExpression child(handles_[index]);
        Base::Status status = child.UpdateTarget();
        if (!status.IsOk()) return status;
    }
    return {};
}

Base::Status TemplateBindingExpression::UpdateTarget() noexcept {
    if (!IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TemplateBindingExpression is invalid");
    }
    UIElement* element = TryCast<UIElement>(target_);
    if (element == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TemplateBindingExpression target is not a UIElement");
    }
    Controls::TemplateEngine* templates =
        AeroGuiInternal::TemplatesOf(*element);
    if (templates == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "TemplateEngine is unavailable");
    }
    Base::Result<void> refreshed =
        templates->RefreshTemplateBinding(*target_, targetProperty_);
    return refreshed ? Base::Status{} : refreshed.GetStatus();
}

BindingExpression BindingOperations::GetBindingExpression(
    DependencyObject* target,
    DependencyPropertyHandle property) noexcept {
    if (target == nullptr || !property.IsValid()) return {};
    BindingEngine* engine = AeroGuiInternal::BindingEngineOf(*target);
    if (engine == nullptr) return {};
    BindingHandle handle = engine->FindBinding(*target, property);
    if (!handle.IsValid()) return {};
    return BindingExpression(handle);
}

MultiBindingExpression BindingOperations::GetMultiBindingExpression(
    DependencyObject* target,
    DependencyPropertyHandle property) noexcept {
    if (target == nullptr || !property.IsValid()) return {};
    BindingEngine* engine = AeroGuiInternal::BindingEngineOf(*target);
    if (engine == nullptr) return {};
    return engine->FindMultiBinding(*target, property);
}

TemplateBindingExpression BindingOperations::GetTemplateBindingExpression(
    DependencyObject* target,
    DependencyPropertyHandle property) noexcept {
    TemplateBindingExpression expression;
    if (target == nullptr || !property.IsValid()) return expression;
    UIElement* element = TryCast<UIElement>(target);
    if (element == nullptr) return expression;
    Controls::TemplateEngine* templates =
        AeroGuiInternal::TemplatesOf(*element);
    if (templates == nullptr ||
        !templates->HasTemplateBinding(*target, property)) {
        return expression;
    }
    expression.target_ = target;
    expression.targetProperty_ = property;
    return expression;
}

} // namespace Aero::Data


namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Data;

// Evaluation / attach-update-detach main path lives with BindingExpression's
// TU. BindingEngine::Flush (dirty-list pump) remains in BindingOperations.cpp.

Base::Result<BindingHandle> BindingEngine::Attach(
    const BindingDescriptor& descriptor) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!initialized_) {
        return InvalidState("BindingEngine must be initialized before Attach");
    }
    if (flushing_) {
        return InvalidState("BindingEngine cannot attach while flushing");
    }
    Base::Result<void> valid = VerifyDescriptor(descriptor);
    if (!valid) {
        return valid.GetStatus();
    }
    if (nextHandle_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Binding handle sequence is exhausted");
    }

    BindingRecord record;
    record.handle.value = nextHandle_++;
    record.handle.engine_ = this;
    record.descriptor = descriptor;
    record.descriptor.mode = ResolveBindingMode(
        *descriptor.target,
        descriptor.targetProperty,
        descriptor.mode);
    record.descriptor.updateSourceTrigger = ResolveUpdateSourceTrigger(
        *descriptor.target,
        descriptor.targetProperty,
        descriptor.updateSourceTrigger);
    const std::uint32_t newIndex = bindings_.Size();
    Base::Result<void> appended = bindings_.PushBack(std::move(record));
    if (!appended) {
        --nextHandle_;
        return appended.GetStatus();
    }
    static_cast<void>(handleIndexMap_.Insert(bindings_.Back().handle.value, newIndex));
    descriptor.source->AddValueChangedHandler(
        descriptor.sourceProperty, propertyChangedHandler_);
    descriptor.target->AddValueChangedHandler(
        descriptor.targetProperty, propertyChangedHandler_);
    Base::Result<void> lostFocus =
        SubscribeLostFocus(bindings_.Back());
    if (!lostFocus) {
        RemoveAt(bindings_.Size() - 1U);
        return lostFocus.GetStatus();
    }
    return bindings_.Back().handle;
}

Base::Result<BindingHandle> BindingEngine::Attach(
    const MetadataBindingDescriptor& descriptor) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!initialized_) {
        return InvalidState("BindingEngine must be initialized before Attach");
    }
    if (flushing_) {
        return InvalidState("BindingEngine cannot attach while flushing");
    }
    Base::Result<void> valid = VerifyDescriptor(descriptor);
    if (!valid) return valid.GetStatus();
    if (nextHandle_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Binding handle sequence is exhausted");
    }

    BindingRecord record;
    record.handle.value = nextHandle_++;
    record.handle.engine_ = this;
    record.sourceKind = descriptor.source != nullptr
        ? (descriptor.bindsToSource
            ? BindingSourceKind::MetadataObject
            : BindingSourceKind::MetadataPath)
        : BindingSourceKind::DataContext;
    record.metadata = descriptor.metadata;
    record.metadataSource = descriptor.source;
    record.dataContextProperty = descriptor.dataContextProperty;
    record.dataContextOwner = descriptor.dataContextOwner != nullptr
        ? descriptor.dataContextOwner
        : descriptor.target;
    // A Binding authored on FrameworkElement.DataContext reads from the
    // inherited parent DataContext. Reading the target property itself would
    // create a self-reference and hide the inherited value while the
    // expression is unresolved.
    if (record.sourceKind == BindingSourceKind::DataContext &&
        descriptor.targetProperty == descriptor.dataContextProperty &&
        record.metadata->Types().IsDerivedFrom(
            descriptor.target->RuntimeType(),
            FrameworkElement::StaticTypeId())) {
        auto& targetElement =
            *static_cast<FrameworkElement*>(descriptor.target);
        ::Aero::Media::Visual* parent = ::Aero::TryCast<::Aero::Media::Visual>(targetElement.GetLogicalParent());
        if (parent == nullptr) parent = targetElement.GetVisualParent();
        if (parent != nullptr && ::Aero::TryCast<::Aero::FrameworkElement>(parent) != nullptr) {
            record.dataContextOwner = ::Aero::TryCast<::Aero::FrameworkElement>(parent);
        }
    }
    record.descriptor.target = descriptor.target;
    record.descriptor.targetProperty = descriptor.targetProperty;
    record.descriptor.mode = ResolveBindingMode(
        *descriptor.target,
        descriptor.targetProperty,
        descriptor.mode);
    record.descriptor.updateSourceTrigger = ResolveUpdateSourceTrigger(
        *descriptor.target,
        descriptor.targetProperty,
        descriptor.updateSourceTrigger);
    record.descriptor.convert = descriptor.convert;
    record.descriptor.convertBack = descriptor.convertBack;
    record.descriptor.converterResource = descriptor.converterResource;
    record.descriptor.converterParameter = descriptor.converterParameter;
    record.descriptor.validate = descriptor.validate;
    record.descriptor.validateBack = descriptor.validateBack;
    record.descriptor.conversionContext =
        descriptor.conversionContext;
    record.descriptor.fallbackValue = descriptor.fallbackValue;
    record.descriptor.targetNullValue = descriptor.targetNullValue;
    record.descriptor.diagnostic = descriptor.diagnostic;
    record.descriptor.diagnosticContext =
        descriptor.diagnosticContext;
    record.bindsToSource = descriptor.bindsToSource;
    Base::Result<void> assigned = record.path.Assign(descriptor.path);
    if (!assigned) {
        --nextHandle_;
        return assigned.GetStatus();
    }
    assigned = record.stringFormat.Assign(
        descriptor.stringFormat);
    if (!assigned) {
        --nextHandle_;
        return assigned.GetStatus();
    }

    if (record.sourceKind == BindingSourceKind::MetadataPath) {
        BindingPathCompileError compileError;
        Base::Result<BindingPathPlan> compiled = BindingPathPlan::Compile(
            *record.metadata,
            record.metadataSource->RuntimeType(),
            record.path.View(),
            &compileError);
        if (!compiled) {
            --nextHandle_;
            return BindingPathFailure(
                record.metadata->Types(),
                record.path.View(),
                compileError,
                compiled.GetStatus());
        }
        record.pathPlan = std::move(compiled).Value();
        const DependencyProperty* targetProperty =
            AeroGuiInternal::PropertyRegistry(descriptor.target).Find(
                descriptor.targetProperty);
        if (targetProperty == nullptr ||
            (descriptor.convert == nullptr &&
             !descriptor.converterResource &&
             !TargetAcceptsPathResult(
                 record.metadata->Types(),
                 targetProperty,
                 record.pathPlan.ResultType(),
                 record.pathPlan.HasDynamicResult()))) {
            --nextHandle_;
            return BindingTypeMismatch(
                record.metadata->Types(),
                record.path.View(),
                record.pathPlan.ResultType(),
                *descriptor.target,
                targetProperty);
        }
        const bool wantsWriteback =
            record.descriptor.mode == BindingMode::TwoWay ||
            record.descriptor.mode == BindingMode::OneWayToSource;
        if (wantsWriteback && !record.pathPlan.CanWrite()) {
            if (descriptor.mode != BindingMode::Default ||
                record.descriptor.mode == BindingMode::OneWayToSource) {
                --nextHandle_;
                return Base::Status::Failure(
                    Base::ErrorCode::ReadOnly,
                    "Binding source path is not writable");
            }
            record.descriptor.mode = BindingMode::OneWay;
        }
        if (wantsWriteback &&
            record.descriptor.mode != BindingMode::OneWay &&
            !record.pathPlan.HasDynamicResult() &&
            targetProperty->ValueType() != record.pathPlan.ResultType() &&
            descriptor.convertBack == nullptr &&
            !descriptor.converterResource &&
            !HasDefaultTargetConversion(
                targetProperty->ValueType(),
                record.pathPlan.ResultType()) &&
            !CanRoundTripObjectValue(
                record.metadata->Types(),
                record.pathPlan.ResultType(),
                targetProperty->ValueType())) {
            if (descriptor.mode != BindingMode::Default ||
                record.descriptor.mode == BindingMode::OneWayToSource) {
                --nextHandle_;
                return InvalidArgument(
                    "Binding requires ConvertBack for different source and target types");
            }
            record.descriptor.mode = BindingMode::OneWay;
        }
    } else if (record.sourceKind == BindingSourceKind::MetadataObject) {
        const DependencyProperty* targetProperty =
            AeroGuiInternal::PropertyRegistry(descriptor.target).Find(
                descriptor.targetProperty);
        if (targetProperty == nullptr ||
            (!targetProperty->AcceptsAnyValue() &&
            !record.metadata->Types().IsAssignableFrom(
                targetProperty->ValueType(),
                record.metadataSource->RuntimeType()))) {
            --nextHandle_;
            return BindingTypeMismatch(
                record.metadata->Types(),
                Base::StringView("."),
                record.metadataSource->RuntimeType(),
                *descriptor.target,
                targetProperty);
        }
        if (record.descriptor.mode == BindingMode::TwoWay ||
            record.descriptor.mode == BindingMode::OneWayToSource) {
            if (descriptor.mode != BindingMode::Default ||
                record.descriptor.mode == BindingMode::OneWayToSource) {
                --nextHandle_;
                return Base::Status::Failure(
                    Base::ErrorCode::ReadOnly,
                    "Binding to a source object does not support writeback");
            }
            record.descriptor.mode = BindingMode::OneWay;
        }
    }

    const std::uint32_t newIndex = bindings_.Size();
    Base::Result<void> appended =
        bindings_.PushBack(std::move(record));
    if (!appended) {
        --nextHandle_;
        return appended.GetStatus();
    }
    BindingRecord& stored = bindings_.Back();
    static_cast<void>(handleIndexMap_.Insert(stored.handle.value, newIndex));
    descriptor.target->AddValueChangedHandler(
        descriptor.targetProperty, propertyChangedHandler_);
    if (stored.sourceKind == BindingSourceKind::DataContext) {
        stored.dataContextOwner->AddValueChangedHandler(
            descriptor.dataContextProperty,
            propertyChangedHandler_);
    } else if (stored.sourceKind == BindingSourceKind::MetadataPath) {
        Base::Result<void> sourceSubscription =
            SubscribeMetadataSource(stored);
        if (!sourceSubscription) {
            RemoveAt(bindings_.Size() - 1U);
            return sourceSubscription.GetStatus();
        }
    }
    Base::Result<void> lostFocus = SubscribeLostFocus(stored);
    if (!lostFocus) {
        RemoveAt(bindings_.Size() - 1U);
        return lostFocus.GetStatus();
    }
    return stored.handle;
}

Base::Result<bool> BindingEngine::Detach(BindingHandle handle) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (flushing_) {
        return InvalidState("BindingEngine cannot detach while flushing");
    }
    if (handle.value == 0U) {
        return false;
    }
    const std::uint32_t* found = handleIndexMap_.Find(handle.value);
    if (found != nullptr && *found < bindings_.Size() && bindings_[*found].handle.value == handle.value) {
        RemoveAt(*found);
        return true;
    }
    return false;
}

Base::Result<bool> BindingEngine::UpdateSource(BindingHandle handle) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!initialized_ || flushing_) {
        return InvalidState("BindingEngine is not ready to update a source");
    }
    BindingRecord* record = FindRecord(handle);
    if (record == nullptr) {
        return false;
    }
    if (record->descriptor.mode != BindingMode::TwoWay &&
        record->descriptor.mode != BindingMode::OneWayToSource) {
        return false;
    }
    record->targetDirty = true;
    record->forceSourceUpdate = true;
    return true;
}

Base::Result<bool> BindingEngine::UpdateTarget(BindingHandle handle) noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!initialized_ || flushing_) {
        return InvalidState("BindingEngine is not ready to update a target");
    }
    BindingRecord* record = FindRecord(handle);
    if (record == nullptr) return false;
    record->sourceDirty = true;
    Base::Result<std::uint32_t> flushed = Flush();
    if (!flushed) return flushed.GetStatus();
    return true;
}

Base::Result<void> BindingEngine::ApplySourceToTarget(
    BindingRecord& record,
    const PropertyValue& source,
    bool& usedFallback,
    PropertyValue& target) noexcept {
    Base::Result<PropertyValue> converted = usedFallback
        ? Base::Result<PropertyValue>(source)
        : ConvertForTarget(record, source);
    if (!converted) {
        ReportDiagnostic(
            record,
            record.conversionFailureStage,
            converted.GetStatus());
        if (record.descriptor.fallbackValue.IsUnset() || usedFallback) {
            return converted.GetStatus();
        }
        converted = record.descriptor.fallbackValue;
        usedFallback = true;
    }
    record.descriptor.target->SetValue(
        record.descriptor.targetProperty,
        converted.Value());
    target = converted.Value();
    return {};
}

Base::Result<void> BindingEngine::ApplyTargetToSource(
    BindingRecord& record,
    const PropertyValue& target,
    PropertyValue& source) noexcept {
    Base::Result<PropertyValue> converted =
        ConvertForSource(record, target);
    if (!converted) {
        ReportDiagnostic(
            record,
            record.conversionFailureStage,
            converted.GetStatus());
        return converted.GetStatus();
    }
    Base::Result<void> written =
        WriteSource(record, converted.Value());
    if (!written) {
        ReportDiagnostic(
            record,
            BindingDiagnosticStage::WriteSource,
            written.GetStatus());
        return written.GetStatus();
    }
    source = converted.Value();
    return {};
}

Base::Result<PropertyValue> BindingEngine::ReadSource(
    BindingRecord& record) noexcept {
    if (record.sourceKind ==
        BindingSourceKind::DependencyProperty) {
        return Aero::Data::UnboxItemsValue(
            record.descriptor.source->GetValue(
                record.descriptor.sourceProperty));
    }
    if (record.sourceKind == BindingSourceKind::MetadataObject) {
        return Aero::Data::UnboxItemsValue(
            PropertyValue::FromObject(
                record.metadataSource->RuntimeType(),
                Base::Ref<Base::Object>::FromBorrowed(
                    *record.metadataSource)));
    }
    Base::Result<void> resolved = ResolveMetadataSource(record);
    if (!resolved) return resolved.GetStatus();
    if (record.bindsToSource) {
        return Aero::Data::UnboxItemsValue(
            PropertyValue::FromObject(
                record.metadataSource->RuntimeType(),
                Base::Ref<Base::Object>::FromBorrowed(
                    *record.metadataSource)));
    }
    return Aero::Data::UnboxItemsValue(
        record.pathPlan.Get(
            *record.metadata, *record.metadataSource));
}

Base::Result<void> BindingEngine::WriteSource(
    BindingRecord& record,
    const PropertyValue& value) noexcept {
    if (record.sourceKind ==
        BindingSourceKind::DependencyProperty) {
        record.descriptor.source->SetValue(
            record.descriptor.sourceProperty, value);
        return {};
    }
    if (record.sourceKind == BindingSourceKind::MetadataObject ||
        record.bindsToSource) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Binding source object cannot be replaced through writeback");
    }
    Base::Result<void> resolved = ResolveMetadataSource(record);
    if (!resolved) return resolved;
    return record.pathPlan.Set(
        *record.metadata, *record.metadataSource, value);
}

Base::Result<PropertyValue> BindingEngine::ConvertForTarget(
    BindingRecord& record,
    const PropertyValue& value) noexcept {
    record.conversionFailureStage =
        BindingDiagnosticStage::Convert;
    const DependencyProperty* targetProperty =
        AeroGuiInternal::PropertyRegistry(record.descriptor.target).Find(
            record.descriptor.targetProperty);
    if (targetProperty == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Binding target property was not found");
    }
    PropertyValue converted = value;
    if (converted.IsNullObject() &&
        !record.descriptor.targetNullValue.IsUnset()) {
        converted = record.descriptor.targetNullValue;
    } else if (record.descriptor.converterResource) {
        Base::Result<PropertyValue> result =
            record.descriptor.converterResource->Convert(
                converted,
                record.descriptor.converterParameter);
        if (!result) return result.GetStatus();
        converted = std::move(result).Value();
    } else if (record.descriptor.convert != nullptr) {
        Base::Result<PropertyValue> result =
            record.descriptor.convert(
                converted,
                targetProperty->ValueType(),
                record.descriptor.conversionContext);
        if (!result) return result.GetStatus();
        converted = std::move(result).Value();
    }
    if (converted.Type() != targetProperty->ValueType() &&
        HasDefaultTargetConversion(
            converted.Type(),
            targetProperty->ValueType())) {
        Base::Result<PropertyValue> result =
            ((converted.Type() == TypeOf<bool>() &&
              targetProperty->ValueType() ==
                  TypeOf<::Aero::Nullable<bool>>()) ||
             (converted.Type() == TypeOf<::Aero::Nullable<bool>>() &&
              targetProperty->ValueType() == TypeOf<bool>()))
            ? ConvertNullableBooleanValue(
                  converted, targetProperty->ValueType())
            : (IsNumericType(converted.Type()) &&
             targetProperty->ValueType() == TypeOf<Base::Thickness>())
            ? ConvertThicknessValue(converted)
            : ((IsNumericType(converted.Type()) &&
              targetProperty->ValueType() == TypeOf<Aero::Length>()) ||
             (converted.Type() == TypeOf<Aero::Length>() &&
              IsNumericType(targetProperty->ValueType())))
            ? ConvertLengthValue(
                  converted, targetProperty->ValueType())
            : (converted.Type() == TypeOf<Base::Color>() &&
               targetProperty->ValueType() ==
                   Media::Brush::StaticTypeId())
            ? ConvertColorToBrush(converted)
            : IsNumericType(converted.Type()) &&
                  IsNumericType(targetProperty->ValueType())
                ? ConvertNumericValue(
                      converted, targetProperty->ValueType())
                : [&]() noexcept
                  -> Base::Result<PropertyValue> {
                Base::Result<Base::String> text =
                    FormatBindingString(
                        converted,
                        record.stringFormat.View(),
                        record.metadata);
                if (!text) return text.GetStatus();
                return PropertyValue::TryFromString(
                    TypeOf<Base::String>(),
                    text.Value().View());
            }();
        if (!result) return result.GetStatus();
        converted = std::move(result).Value();
    }
    Base::Result<PropertyValue> coerced =
        NormalizeValueForProperty(
            record.metadata,
            *targetProperty,
            std::move(converted));
    if (!coerced) return coerced.GetStatus();
    converted = std::move(coerced).Value();
    if (record.descriptor.validate != nullptr) {
        record.conversionFailureStage =
            BindingDiagnosticStage::Validate;
        Base::Result<void> valid = record.descriptor.validate(
            converted, record.descriptor.conversionContext);
        if (!valid) return valid.GetStatus();
    }
    return converted;
}

Base::Result<PropertyValue> BindingEngine::ConvertForSource(
    BindingRecord& record,
    const PropertyValue& value) noexcept {
    record.conversionFailureStage =
        BindingDiagnosticStage::ConvertBack;
    TypeId sourceType = InvalidTypeId;
    if (record.sourceKind == BindingSourceKind::DependencyProperty) {
        const DependencyProperty* sourceProperty =
            AeroGuiInternal::PropertyRegistry(record.descriptor.source).Find(
                record.descriptor.sourceProperty);
        if (sourceProperty != nullptr) {
            sourceType = sourceProperty->ValueType();
        }
    } else if (record.sourceKind == BindingSourceKind::MetadataPath ||
               record.sourceKind == BindingSourceKind::DataContext) {
        Base::Result<void> resolved = ResolveMetadataSource(record);
        if (!resolved) return resolved.GetStatus();
        if (record.pathPlan.IsValid()) {
            sourceType = record.pathPlan.ResultType();
        }
    }
    if (sourceType == InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Binding source type was not found");
    }
    PropertyValue converted = value;
    if (record.descriptor.converterResource) {
        Base::Result<PropertyValue> result =
            record.descriptor.converterResource->ConvertBack(
                converted,
                record.descriptor.converterParameter);
        if (!result) return result.GetStatus();
        converted = std::move(result).Value();
    } else if (record.descriptor.convertBack != nullptr) {
        Base::Result<PropertyValue> result =
            record.descriptor.convertBack(
                converted,
                sourceType,
                record.descriptor.conversionContext);
        if (!result) return result.GetStatus();
        converted = std::move(result).Value();
    } else if (HasDefaultTargetConversion(
                   converted.Type(), sourceType)) {
        Base::Result<PropertyValue> result =
            ((converted.Type() == TypeOf<bool>() &&
              sourceType == TypeOf<::Aero::Nullable<bool>>()) ||
             (converted.Type() == TypeOf<::Aero::Nullable<bool>>() &&
              sourceType == TypeOf<bool>()))
            ? ConvertNullableBooleanValue(converted, sourceType)
            : ((IsNumericType(converted.Type()) &&
              sourceType == TypeOf<Aero::Length>()) ||
             (converted.Type() == TypeOf<Aero::Length>() &&
              IsNumericType(sourceType)))
            ? ConvertLengthValue(converted, sourceType)
            : IsNumericType(converted.Type()) &&
                  IsNumericType(sourceType)
                ? ConvertNumericValue(converted, sourceType)
                : [&]() noexcept
                  -> Base::Result<PropertyValue> {
                Base::Result<Base::String> text =
                    FormatBindingString(
                        converted, {});
                if (!text) return text.GetStatus();
                return PropertyValue::TryFromString(
                    sourceType,
                    text.Value().View());
            }();
        if (!result) return result.GetStatus();
        converted = std::move(result).Value();
    }
    if (converted.Type() != sourceType) {
        if (converted.Kind() == ValueKind::Object &&
            !converted.IsNullObject() && converted.AsObject() &&
            record.metadata->Types().IsAssignableFrom(
                sourceType,
                converted.AsObject()->RuntimeType())) {
            converted = PropertyValue::FromObject(
                sourceType,
                Base::Ref<Base::Object>::FromBorrowed(
                    *converted.AsObject()));
        }
    }
    if (converted.Type() != sourceType) {
        return InvalidArgument(
            "Binding ConvertBack returned a value with the wrong source type");
    }
    if (record.descriptor.validateBack != nullptr) {
        record.conversionFailureStage =
            BindingDiagnosticStage::ValidateBack;
        Base::Result<void> valid = record.descriptor.validateBack(
            converted, record.descriptor.conversionContext);
        if (!valid) return valid.GetStatus();
    }
    return converted;
}

} // namespace Aero
