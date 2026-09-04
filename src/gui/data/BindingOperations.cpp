#include "gui/data/BindingInternal.hpp"

#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/controls/State.hpp"
#include <Aero/Data/Binding.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/LogicalTreeHelper.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Visual.hpp>
#include <Aero/VisualTreeHelper.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/SolidColorBrush.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Data;

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

Base::Result<std::uint32_t> BindingEngine::Flush() noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!initialized_) {
        return InvalidState("BindingEngine is not initialized");
    }
    if (flushing_) {
        return InvalidState("BindingEngine cannot flush recursively");
    }

    flushing_ = true;
    lastError_ = {};
    const std::uint32_t snapshotCount = bindings_.Size();
    std::uint32_t updated = 0U;
    // Equip slots author DataContext="{Binding Player.Slots[i]}" plus
    // Content="{Binding}". Apply DataContext writes first so empty-path
    // Content bindings in the same Flush see the slot object.
    for (std::uint32_t pass = 0U; pass < 2U; ++pass) {
    for (std::uint32_t index = 0U; index < snapshotCount; ++index) {
        BindingRecord& record = bindings_[index];
        const bool writesDataContext =
            record.dataContextProperty.IsValid() &&
            record.descriptor.targetProperty == record.dataContextProperty;
        if (pass == 0U && !writesDataContext) {
            continue;
        }
        if (pass == 1U && writesDataContext) {
            continue;
        }
        if (record.descriptor.mode == BindingMode::OneTime && record.applied) {
            continue;
        }
        const bool metadataPath =
            record.sourceKind != BindingSourceKind::DependencyProperty;
        const bool hasNotify =
            record.notificationSubscription != 0U ||
            record.sourceDependencyProperty.IsValid();
        const bool unresolvedDataContext =
            record.sourceKind == BindingSourceKind::DataContext &&
            (!record.applied || record.metadataSource == nullptr ||
             (!record.bindsToSource && !record.pathPlan.IsValid()));
        const bool pollMetadata =
            (metadataPath && !hasNotify) || unresolvedDataContext;
        if (record.applied && !record.sourceDirty &&
            !record.targetDirty && !pollMetadata) {
            continue;
        }
        if (pollMetadata && !record.pollHintEmitted) {
            record.pollHintEmitted = true;
            ReportDiagnostic(
                record,
                BindingDiagnosticStage::ResolveSource,
                Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Binding source does not implement NotifyPropertyChanged; "
                    "polling metadata path every frame"));
        }

        Base::Result<PropertyValue> source = ReadSource(record);
        bool usedFallback = false;
        if (!source) {
            const BindingDiagnosticStage stage =
                record.sourceKind == BindingSourceKind::DataContext &&
                source.GetStatus().code == Base::ErrorCode::NotFound
                    ? BindingDiagnosticStage::ResolveSource
                    : BindingDiagnosticStage::ReadSource;
            ReportDiagnostic(record, stage, source.GetStatus());
            if (record.descriptor.fallbackValue.IsUnset()) {
                record.applied = false;
                record.sourceDirty = true;
                continue;
            }
            source = record.descriptor.fallbackValue;
            usedFallback = true;
        }
        Base::Result<PropertyValue> target =
            record.descriptor.target->GetValue(record.descriptor.targetProperty);
        if (!target) {
            ReportDiagnostic(
                record,
                BindingDiagnosticStage::WriteTarget,
                target.GetStatus());
            continue;
        }

        const bool sourceChanged = !record.applied ||
            record.sourceDirty ||
            usedFallback ||
            (pollMetadata &&
             source.Value() != record.lastSourceValue);
        const bool targetChanged =
            record.descriptor.updateSourceTrigger ==
                    UpdateSourceTrigger::Explicit ||
                record.descriptor.updateSourceTrigger ==
                    UpdateSourceTrigger::LostFocus
            ? record.forceSourceUpdate
            : (!record.applied || record.targetDirty);
        if (!sourceChanged && !targetChanged) {
            record.sourceDirty = false;
            if (record.descriptor.updateSourceTrigger !=
                    UpdateSourceTrigger::LostFocus &&
                record.descriptor.updateSourceTrigger !=
                    UpdateSourceTrigger::Explicit) {
                record.targetDirty = false;
            }
            continue;
        }
        Base::Result<void> applied =
            Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Binding update was not attempted");
        switch (record.descriptor.mode) {
        case BindingMode::OneTime:
            if (!record.applied) {
                applied = ApplySourceToTarget(
                    record,
                    source.Value(),
                    usedFallback,
                    target.Value());
                if (applied) ++updated;
            }
            break;
        case BindingMode::Default:
        case BindingMode::OneWay:
            if (sourceChanged) {
                applied = ApplySourceToTarget(
                    record,
                    source.Value(),
                    usedFallback,
                    target.Value());
                if (applied) ++updated;
            }
            break;
        case BindingMode::OneWayToSource:
            if (targetChanged) {
                applied = ApplyTargetToSource(
                    record,
                    target.Value(),
                    source.Value());
                if (applied) ++updated;
            }
            break;
        case BindingMode::TwoWay:
            // A target edit (ToggleButton click) must write back even when
            // metadata-path polling reports a unchanged source as "changed".
            // Source still wins when the source property itself is dirty.
            if (targetChanged && !record.sourceDirty) {
                applied = ApplyTargetToSource(
                    record,
                    target.Value(),
                    source.Value());
                if (applied) ++updated;
            } else if (sourceChanged) {
                applied = ApplySourceToTarget(
                    record,
                    source.Value(),
                    usedFallback,
                    target.Value());
                if (applied) ++updated;
            } else if (targetChanged) {
                applied = ApplyTargetToSource(
                    record,
                    target.Value(),
                    source.Value());
                if (applied) ++updated;
            }
            break;
        }
        if (!applied && (sourceChanged || targetChanged)) {
            record.sourceDirty = true;
            continue;
        }
        record.lastSourceValue = source.Value();
        record.lastTargetValue = target.Value();
        record.applied = true;
        record.lastStatus = {};
        record.sourceDirty = false;
        record.targetDirty = false;
        record.forceSourceUpdate = false;
    }
    }
    flushing_ = false;
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
        Base::Result<void> appended =
            output.PushBack(
                std::move(inspection));
        if (!appended) {
            output.Clear();
            return appended.GetStatus();
        }
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
    bool lostFocusFlush = false;
    for (BindingRecord& record : bindings_) {
        if (record.sourceKind == BindingSourceKind::DependencyProperty &&
            record.descriptor.source == &object &&
            record.descriptor.sourceProperty == args.GetProperty()) {
            record.sourceDirty = true;
        }
        if (record.sourceDependencyProperty.IsValid() &&
            record.metadataSource == &object &&
            record.sourceDependencyProperty == args.GetProperty()) {
            record.sourceDirty = true;
        }
        if (record.sourceKind == BindingSourceKind::DataContext &&
            record.dataContextProperty == args.GetProperty() &&
            BindingOwnerSeesDataContextChange(
                record.dataContextOwner, object)) {
            ReleaseMetadataSource(record);
            record.metadataSource = nullptr;
            record.pathPlan = {};
            record.sourceDirty = true;
            record.applied = false;
        }
        if (record.descriptor.target == &object &&
            record.descriptor.targetProperty == args.GetProperty()) {
            record.targetDirty = true;
        }
        if (record.lostFocusSubscribed &&
            record.descriptor.target == &object &&
            args.GetProperty() ==
                UIElement::IsKeyboardFocusedProperty.Handle() &&
            record.descriptor.updateSourceTrigger ==
                UpdateSourceTrigger::LostFocus) {
            const PropertyValue& next = args.GetNewValue();
            const bool focused =
                next.Kind() == ValueKind::Boolean && next.AsBoolean();
            if (!focused) {
                record.forceSourceUpdate = true;
                record.targetDirty = true;
                lostFocusFlush = true;
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
    for (BindingRecord& record : bindings_) {
        if (record.sourceKind ==
                BindingSourceKind::DependencyProperty ||
            record.metadataSource != &object ||
            !record.pathPlan.IsValid() ||
            record.pathPlan.Segments().Empty()) {
            continue;
        }
        const BindingPathSegment& first =
            record.pathPlan.Segments()[0];
        if (property == InvalidMemberId ||
            first.member == property ||
            first.dynamic) {
            record.sourceDirty = true;
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
    if (!descriptor.fallbackValue.IsUnset() &&
        descriptor.fallbackValue.Type() != target.Value().Type()) {
        return InvalidArgument(
            "Binding fallback value type differs from the target property");
    }
    if (!descriptor.targetNullValue.IsUnset() &&
        descriptor.targetNullValue.Type() != target.Value().Type()) {
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
    if (!descriptor.fallbackValue.IsUnset() &&
        descriptor.fallbackValue.Type() != target.Value().Type()) {
        return InvalidArgument(
            "Binding fallback value type differs from the target property");
    }
    if (!descriptor.targetNullValue.IsUnset() &&
        descriptor.targetNullValue.Type() != target.Value().Type()) {
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
            record.descriptor.target->PropertyRegistry().Find(
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
        record.descriptor.target->PropertyRegistry().Find(
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
    Base::Result<void> subscribed =
        SubscribeMetadataSource(record);
    if (!subscribed) {
        record.metadataSource = nullptr;
        record.pathPlan = {};
        return subscribed.GetStatus();
    }
    record.sourceDirty = true;
    record.applied = false;
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
        record.descriptor.target->PropertyRegistry().Find(
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
            record.descriptor.source->PropertyRegistry().Find(
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
                if (sourceObject->PropertyRegistry().Find(handle) !=
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

void BindingEngine::RemoveAt(std::uint32_t index) noexcept {
    BindingRecord& removed = bindings_[index];
    handleIndexMap_.Erase(removed.handle.value);
    if (removed.sourceKind ==
        BindingSourceKind::DependencyProperty) {
        (void)removed.descriptor.source->RemoveValueChangedHandler(
            removed.descriptor.sourceProperty,
            propertyChangedHandler_);
    } else {
        ReleaseMetadataSource(removed);
        if (removed.sourceKind ==
            BindingSourceKind::DataContext) {
            (void)removed.dataContextOwner->RemoveValueChangedHandler(
                removed.dataContextProperty,
                propertyChangedHandler_);
        }
    }
    (void)removed.descriptor.target->RemoveValueChangedHandler(
        removed.descriptor.targetProperty, propertyChangedHandler_);
    UnsubscribeLostFocus(removed);
    for (std::uint32_t current = index + 1U;
         current < bindings_.Size();
         ++current) {
        bindings_[current - 1U] = std::move(bindings_[current]);
        static_cast<void>(handleIndexMap_.Set(bindings_[current - 1U].handle.value, current - 1U));
    }
    bindings_.PopBack();
}

} // namespace Aero
