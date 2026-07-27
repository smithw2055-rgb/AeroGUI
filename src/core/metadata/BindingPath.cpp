#include <Aero/Core/Metadata/BindingPath.hpp>

namespace Aero::Core {
namespace {

constexpr std::uint32_t InvalidSegmentIndex = UINT32_MAX;

bool HasPropertyFlag(
    PropertyFlags value,
    PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::StringView TrimAscii(Base::StringView value) noexcept {
    std::uint32_t begin = 0U;
    std::uint32_t end = value.SizeBytes();
    while (begin < end &&
        (value[begin] == ' ' || value[begin] == '\t' ||
         value[begin] == '\r' || value[begin] == '\n')) {
        ++begin;
    }
    while (end > begin &&
        (value[end - 1U] == ' ' || value[end - 1U] == '\t' ||
         value[end - 1U] == '\r' || value[end - 1U] == '\n')) {
        --end;
    }
    return value.Substr(begin, end - begin);
}

Base::Status InvalidPath(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::ValidationFailed, message);
}

Base::Status UnsupportedPath(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::Unsupported, message);
}

void ResetCompileError(BindingPathCompileError* error) noexcept {
    if (error == nullptr) return;
    error->segmentIndex = InvalidSegmentIndex;
    error->inputType = InvalidTypeId;
    error->segment.Clear();
    error->status = {};
}

Base::Status RecordCompileError(
    BindingPathCompileError* error,
    std::uint32_t segmentIndex,
    TypeId inputType,
    Base::StringView segment,
    Base::Status status) noexcept {
    if (error != nullptr) {
        error->segmentIndex = segmentIndex;
        error->inputType = inputType;
        error->segment.Clear();
        (void)error->segment.TryAssign(segment);
        error->status = status;
    }
    return status;
}

bool PropertyReadable(
    const PropertyInfo& property,
    bool accessorReadable) noexcept {
    return accessorReadable &&
        !HasPropertyFlag(
            property.Flags(), PropertyFlags::WriteOnly);
}

bool PropertyWritable(
    const PropertyInfo& property,
    bool accessorWritable) noexcept {
    return accessorWritable &&
        !HasPropertyFlag(
            property.Flags(), PropertyFlags::ReadOnly);
}

bool IsObjectLike(MetadataTypeKind kind) noexcept {
    return kind == MetadataTypeKind::Object ||
        kind == MetadataTypeKind::Interface;
}

} // namespace

Base::Result<BindingPathPlan> BindingPathPlan::Compile(
    MetadataRuntime& runtime,
    TypeId rootType,
    Base::StringView path,
    BindingPathCompileError* error) noexcept {
    ResetCompileError(error);
    path = TrimAscii(path);
    if (!runtime.IsFrozen() || rootType == InvalidTypeId || path.Empty()) {
        return RecordCompileError(
            error,
            InvalidSegmentIndex,
            rootType,
            path,
            InvalidPath(
                "Binding path requires a frozen runtime, root type, and path"));
    }

    const TypeRegistry& descriptors = runtime.Types();
    if (descriptors.FindType(rootType) == nullptr) {
        return RecordCompileError(
            error,
            0U,
            rootType,
            {},
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Binding path root type descriptor was not found"));
    }
    Base::Result<Base::HashCode> schemaHash =
        runtime.Domain().ComputeSchemaHash();
    if (!schemaHash) return schemaHash.GetStatus();

    BindingPathPlan plan;
    plan.compiledDomain_ = &runtime.Domain();
    plan.rootType_ = rootType;
    plan.schemaHash_ = schemaHash.Value();

    TypeId currentType = rootType;
    std::uint32_t segmentIndex = 0U;
    std::uint32_t begin = 0U;
    while (begin <= path.SizeBytes()) {
        std::uint32_t end = begin;
        while (end < path.SizeBytes() && path[end] != '.') ++end;
        const Base::StringView name =
            TrimAscii(path.Substr(begin, end - begin));
        if (name.Empty()) {
            return RecordCompileError(
                error,
                segmentIndex,
                currentType,
                name,
                InvalidPath("Binding path contains an empty segment"));
        }

        const TypeInfo* input =
            descriptors.FindType(currentType);
        if (input == nullptr) {
            return RecordCompileError(
                error,
                segmentIndex,
                currentType,
                name,
                Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Binding path input type descriptor was not found"));
        }

        BindingPathSegment segment;
        segment.inputType = currentType;
        if (IsObjectLike(input->Kind())) {
            const PropertyInfo* property =
                descriptors.FindProperty(currentType, name, true);
            if (property == nullptr) {
                return RecordCompileError(
                    error,
                    segmentIndex,
                    currentType,
                    name,
                    Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Binding path object property was not found"));
            }
            segment.kind = BindingPathSegmentKind::ObjectProperty;
            segment.member = property->Id();
            segment.outputType = property->ValueType();
            segment.readable = PropertyReadable(
                *property,
                runtime.CanReadProperty(property->Id()));
            segment.writable = PropertyWritable(
                *property,
                runtime.CanWriteProperty(property->Id()));
        } else if (input->Kind() == MetadataTypeKind::Struct) {
            const FieldInfo* field =
                descriptors.FindField(currentType, name);
            if (field == nullptr) {
                return RecordCompileError(
                    error,
                    segmentIndex,
                    currentType,
                    name,
                    Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Binding path value field was not found"));
            }
            segment.kind = BindingPathSegmentKind::ValueField;
            segment.member = field->Id();
            segment.outputType = field->ValueType();
            segment.readable =
                runtime.CanReadValueMember(field->Id());
            segment.writable =
                runtime.CanWriteValueMember(field->Id()) &&
                !HasFieldFlag(field->Flags(), FieldFlags::ReadOnly);
            segment.copyOnWrite = true;
        } else {
            return RecordCompileError(
                error,
                segmentIndex,
                currentType,
                name,
                UnsupportedPath(
                    "Binding path cannot traverse this metadata type kind"));
        }

        const TypeInfo* output =
            descriptors.FindType(segment.outputType);
        if (output == nullptr) {
            return RecordCompileError(
                error,
                segmentIndex,
                currentType,
                name,
                Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Binding path output type descriptor was not found"));
        }
        const bool hasMore = end < path.SizeBytes();
        if (segment.kind == BindingPathSegmentKind::ObjectProperty &&
            hasMore && output->Kind() == MetadataTypeKind::Struct) {
            segment.copyOnWrite = true;
        }

        Base::Result<void> appended =
            plan.segments_.TryPushBack(segment);
        if (!appended) return appended.GetStatus();
        currentType = segment.outputType;
        ++segmentIndex;
        if (!hasMore) break;
        begin = end + 1U;
    }

    plan.resultType_ = currentType;
    plan.canRead_ = true;
    plan.canWrite_ = true;
    for (std::uint32_t index = 0U;
         index < plan.segments_.Size();
         ++index) {
        const BindingPathSegment& segment = plan.segments_[index];
        plan.canRead_ = plan.canRead_ && segment.readable;
        const bool leaf = index + 1U == plan.segments_.Size();
        if (leaf) {
            plan.canWrite_ = plan.canWrite_ && segment.writable;
        } else {
            plan.canWrite_ = plan.canWrite_ && segment.readable &&
                (!segment.copyOnWrite || segment.writable);
        }
    }
    return plan;
}

Base::Result<void> BindingPathPlan::VerifyRuntime(
    MetadataRuntime& runtime) const noexcept {
    if (!IsValid() || !runtime.IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Binding path plan or metadata runtime is not ready");
    }
    if (&runtime.Domain() == compiledDomain_) return {};
    Base::Result<Base::HashCode> current =
        runtime.Domain().ComputeSchemaHash();
    if (!current) return current.GetStatus();
    if (current.Value() != schemaHash_) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Binding path schema hash does not match the runtime");
    }
    return {};
}

Base::Result<Value> BindingPathPlan::Get(
    MetadataRuntime& runtime,
    const Base::Object& root) const noexcept {
    Base::Result<void> ready = VerifyRuntime(runtime);
    if (!ready) return ready.GetStatus();
    if (!canRead_) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Binding path is not readable");
    }
    if (!runtime.Types().IsAssignableFrom(
            rootType_, root.RuntimeType())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Binding path root object type is incompatible");
    }
    return GetObject(runtime, root, 0U);
}

Base::Result<Value> BindingPathPlan::Get(
    MetadataRuntime& runtime,
    const Value& root) const noexcept {
    Base::Result<void> ready = VerifyRuntime(runtime);
    if (!ready) return ready.GetStatus();
    if (!canRead_) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Binding path is not readable");
    }
    if (root.Kind() == ValueKind::Object) {
        if (root.IsNullObject()) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Binding path root object is null");
        }
        return Get(runtime, *root.AsObject());
    }
    if (root.Type() != rootType_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Binding path root value type is incompatible");
    }
    return GetValue(runtime, root, 0U);
}

Base::Result<Value> BindingPathPlan::GetObject(
    MetadataRuntime& runtime,
    const Base::Object& object,
    std::uint32_t segmentIndex) const noexcept {
    const BindingPathSegment& segment = segments_[segmentIndex];
    if (segment.kind != BindingPathSegmentKind::ObjectProperty) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Binding path plan expected a value field on an object");
    }
    Base::Result<Value> current =
        runtime.GetProperty(object, segment.member);
    if (!current || segmentIndex + 1U == segments_.Size()) return current;
    return GetValue(runtime, current.Value(), segmentIndex + 1U);
}

Base::Result<Value> BindingPathPlan::GetValue(
    MetadataRuntime& runtime,
    const Value& value,
    std::uint32_t segmentIndex) const noexcept {
    if (value.Kind() == ValueKind::Object) {
        if (value.IsNullObject()) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Binding path reached a null object");
        }
        return GetObject(runtime, *value.AsObject(), segmentIndex);
    }
    const BindingPathSegment& segment = segments_[segmentIndex];
    if (value.Kind() != ValueKind::Custom ||
        segment.kind != BindingPathSegmentKind::ValueField ||
        value.Type() != segment.inputType) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Binding path value segment is incompatible");
    }
    Base::Result<Value> current =
        runtime.GetValueMember(value, segment.member);
    if (!current || segmentIndex + 1U == segments_.Size()) return current;
    return GetValue(runtime, current.Value(), segmentIndex + 1U);
}

Base::Result<void> BindingPathPlan::Set(
    MetadataRuntime& runtime,
    Base::Object& root,
    const Value& value) const noexcept {
    Base::Result<void> ready = VerifyRuntime(runtime);
    if (!ready) return ready;
    if (!canWrite_) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Binding path is not writable");
    }
    if (value.IsUnset() ||
        !runtime.Types().IsAssignableFrom(
            rootType_, root.RuntimeType())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Binding path root or assigned value is incompatible");
    }
    return SetObject(runtime, root, 0U, value);
}

Base::Result<void> BindingPathPlan::Set(
    MetadataRuntime& runtime,
    Value& root,
    const Value& value) const noexcept {
    Base::Result<void> ready = VerifyRuntime(runtime);
    if (!ready) return ready;
    if (!canWrite_) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Binding path is not writable");
    }
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Binding path assigned value is unset");
    }
    if (root.Kind() == ValueKind::Object) {
        if (root.IsNullObject()) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Binding path root object is null");
        }
        return Set(runtime, *root.AsObject(), value);
    }
    if (root.Type() != rootType_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Binding path root value type is incompatible");
    }
    Base::Result<bool> changed =
        SetValue(runtime, root, 0U, value);
    return changed
        ? Base::Result<void>()
        : Base::Result<void>(changed.GetStatus());
}

Base::Result<void> BindingPathPlan::SetObject(
    MetadataRuntime& runtime,
    Base::Object& object,
    std::uint32_t segmentIndex,
    const Value& value) const noexcept {
    const BindingPathSegment& segment = segments_[segmentIndex];
    if (segment.kind != BindingPathSegmentKind::ObjectProperty) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Binding path plan expected a value field on an object");
    }
    if (segmentIndex + 1U == segments_.Size()) {
        return runtime.SetProperty(object, segment.member, value);
    }
    Base::Result<Value> child =
        runtime.GetProperty(object, segment.member);
    if (!child) return child.GetStatus();
    Base::Result<bool> changed =
        SetValue(runtime, child.Value(), segmentIndex + 1U, value);
    if (!changed) return changed.GetStatus();
    return changed.Value()
        ? runtime.SetProperty(object, segment.member, child.Value())
        : Base::Result<void>();
}

Base::Result<bool> BindingPathPlan::SetValue(
    MetadataRuntime& runtime,
    Value& owner,
    std::uint32_t segmentIndex,
    const Value& value) const noexcept {
    if (owner.Kind() == ValueKind::Object) {
        if (owner.IsNullObject()) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Binding path reached a null object");
        }
        Base::Result<void> result =
            SetObject(runtime, *owner.AsObject(), segmentIndex, value);
        return result
            ? Base::Result<bool>(false)
            : Base::Result<bool>(result.GetStatus());
    }
    const BindingPathSegment& segment = segments_[segmentIndex];
    if (owner.Kind() != ValueKind::Custom ||
        segment.kind != BindingPathSegmentKind::ValueField ||
        owner.Type() != segment.inputType) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Binding path value segment is incompatible");
    }
    if (segmentIndex + 1U == segments_.Size()) {
        Base::Result<void> stored =
            runtime.SetValueMember(owner, segment.member, value);
        return stored
            ? Base::Result<bool>(true)
            : Base::Result<bool>(stored.GetStatus());
    }
    Base::Result<Value> child =
        runtime.GetValueMember(owner, segment.member);
    if (!child) return child.GetStatus();
    Base::Result<bool> changed =
        SetValue(runtime, child.Value(), segmentIndex + 1U, value);
    if (!changed || !changed.Value()) return changed;
    Base::Result<void> stored =
        runtime.SetValueMember(owner, segment.member, child.Value());
    return stored
        ? Base::Result<bool>(true)
        : Base::Result<bool>(stored.GetStatus());
}

} // namespace Aero::Core
