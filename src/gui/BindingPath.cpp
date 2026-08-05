#include "gui/GuiPrivate.hpp"

#include <Aero/Collections.hpp>

#include <cstdio>
#include <limits>


namespace Aero::Meta {
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
        (void)error->segment.Assign(segment);
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

const PropertyInfo* FindAttachedProperty(
    const TypeRegistry& descriptors,
    Base::StringView authored) noexcept {
    authored = TrimAscii(authored);
    if (authored.SizeBytes() < 5U || authored[0] != '(' ||
        authored[authored.SizeBytes() - 1U] != ')') {
        return nullptr;
    }
    const Base::StringView expression = TrimAscii(authored.Substr(
        1U, authored.SizeBytes() - 2U));
    std::uint32_t separator = expression.SizeBytes();
    for (std::uint32_t index = 0U;
         index < expression.SizeBytes(); ++index) {
        if (expression[index] == '.') separator = index;
    }
    if (separator == 0U || separator + 1U >= expression.SizeBytes()) {
        return nullptr;
    }
    Base::StringView ownerName = TrimAscii(
        expression.Substr(0U, separator));
    const Base::StringView propertyName = TrimAscii(expression.Substr(
        separator + 1U,
        expression.SizeBytes() - separator - 1U));
    for (std::uint32_t index = 0U;
         index < ownerName.SizeBytes(); ++index) {
        if (ownerName[index] == ':') {
            ownerName = ownerName.Substr(
                index + 1U,
                ownerName.SizeBytes() - index - 1U);
        }
    }
    if (ownerName.Empty() || propertyName.Empty()) return nullptr;
    for (const TypeInfo& type : descriptors.Types()) {
        if (type.Name() != ownerName) continue;
        const PropertyInfo* property = descriptors.FindProperty(
            type.Id(), propertyName, false);
        if (property != nullptr &&
            HasPropertyFlag(property->Flags(), PropertyFlags::Attached)) {
            return property;
        }
    }
    return nullptr;
}

} // namespace

Base::Result<BindingPathPlan> BindingPathPlan::Compile(
    Meta::Registry& runtime,
    TypeId rootType,
    Base::StringView path,
    BindingPathCompileError* error) noexcept {
    ResetCompileError(error);
    path = TrimAscii(path);
    if (!runtime.IsReady() || rootType == InvalidTypeId || path.Empty()) {
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
        runtime.ComputeSchemaHash();
    if (!schemaHash) return schemaHash.GetStatus();

    BindingPathPlan plan;
    plan.compiledDomain_ = &runtime;
    plan.rootType_ = rootType;
    plan.schemaHash_ = schemaHash.Value();

    TypeId currentType = rootType;
    std::uint32_t segmentIndex = 0U;
    std::uint32_t begin = 0U;
    while (begin < path.SizeBytes()) {
        std::uint32_t end = begin;
        const bool attachedSyntax = path[begin] == '(';
        if (attachedSyntax) {
            while (end < path.SizeBytes() && path[end] != ')') ++end;
            if (end >= path.SizeBytes()) {
                return RecordCompileError(
                    error,
                    segmentIndex,
                    currentType,
                    path.Substr(begin, path.SizeBytes() - begin),
                    InvalidPath(
                        "Binding attached-property segment is missing ')'"));
            }
            ++end;
        } else {
            while (end < path.SizeBytes() && path[end] != '.') ++end;
        }
        const Base::StringView authoredName =
            TrimAscii(path.Substr(begin, end - begin));
        Base::StringView name = authoredName;
        bool hasCollectionIndex = false;
        std::uint32_t collectionIndex = UINT32_MAX;
        if (!attachedSyntax) {
            std::uint32_t open = authoredName.SizeBytes();
            for (std::uint32_t index = 0U;
                 index < authoredName.SizeBytes(); ++index) {
                if (authoredName[index] == '[') {
                    open = index;
                    break;
                }
            }
            if (open != authoredName.SizeBytes()) {
                if (open == 0U ||
                    authoredName[authoredName.SizeBytes() - 1U] != ']' ||
                    open + 2U > authoredName.SizeBytes()) {
                    return RecordCompileError(
                        error, segmentIndex, currentType, authoredName,
                        InvalidPath("Binding collection index syntax is invalid"));
                }
                std::uint64_t parsedIndex = 0U;
                for (std::uint32_t index = open + 1U;
                     index + 1U < authoredName.SizeBytes(); ++index) {
                    const char digit = authoredName[index];
                    if (digit < '0' || digit > '9') {
                        return RecordCompileError(
                            error, segmentIndex, currentType, authoredName,
                            InvalidPath("Binding collection index must be an unsigned integer"));
                    }
                    parsedIndex = parsedIndex * 10U +
                        static_cast<std::uint64_t>(digit - '0');
                    if (parsedIndex >
                        static_cast<std::uint64_t>(UINT32_MAX)) {
                        return RecordCompileError(
                            error, segmentIndex, currentType, authoredName,
                            Base::Status::Failure(
                                Base::ErrorCode::OutOfRange,
                                "Binding collection index is out of range"));
                    }
                }
                name = TrimAscii(authoredName.Substr(0U, open));
                collectionIndex = static_cast<std::uint32_t>(parsedIndex);
                hasCollectionIndex = true;
            }
        }
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
            const PropertyInfo* property = attachedSyntax
                ? FindAttachedProperty(descriptors, name)
                : descriptors.FindProperty(currentType, name, true);
            if (property == nullptr) {
                const bool runtimePolymorphic =
                    currentType == Meta::TypeOf<Base::Object>() ||
                    input->Kind() == MetadataTypeKind::Interface ||
                    (static_cast<std::uint32_t>(input->Flags()) &
                     static_cast<std::uint32_t>(TypeFlags::Abstract)) != 0U;
                if (!attachedSyntax && runtimePolymorphic) {
                    Base::Result<void> storedName =
                        segment.dynamicName.Assign(name);
                    if (!storedName) return storedName.GetStatus();
                    segment.kind =
                        BindingPathSegmentKind::ObjectProperty;
                    segment.outputType =
                        Meta::TypeOf<Base::Object>();
                    segment.readable = true;
                    segment.writable = false;
                    segment.dynamic = true;
                    plan.hasDynamicResult_ = true;
                } else {
                thread_local char message[320];
                std::snprintf(
                    message,
                    sizeof(message),
                    "Binding path %s property '%.*s' was not found on type '%.*s'",
                    attachedSyntax ? "attached" : "object",
                    static_cast<int>(name.SizeBytes()),
                    name.Data(),
                    static_cast<int>(input->Name().SizeBytes()),
                    input->Name().Data());
                return RecordCompileError(
                    error,
                    segmentIndex,
                    currentType,
                    name,
                    Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        message));
                }
            } else {
                segment.kind = BindingPathSegmentKind::ObjectProperty;
                segment.member = property->Id();
                segment.outputType = property->ValueType();
                segment.readable = PropertyReadable(
                    *property,
                    runtime.CanReadProperty(property->Id()));
                segment.writable = PropertyWritable(
                    *property,
                    runtime.CanWriteProperty(property->Id()));
            }
        } else if (currentType == Meta::TypeOf<Meta::Value>()) {
            // Any-valued dependency properties (Content, Header, selected
            // item values, and similar WPF surfaces) carry their concrete
            // runtime value inside Meta::Value. Compile the remaining member
            // as a dynamic object segment and resolve it from that concrete
            // object when the plan executes.
            if (attachedSyntax) {
                return RecordCompileError(
                    error,
                    segmentIndex,
                    currentType,
                    name,
                    Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Binding path attached property cannot be resolved from Any"));
            }
            Base::Result<void> storedName =
                segment.dynamicName.Assign(name);
            if (!storedName) return storedName.GetStatus();
            segment.kind = BindingPathSegmentKind::ObjectProperty;
            segment.outputType = Meta::TypeOf<Base::Object>();
            segment.readable = true;
            segment.writable = false;
            segment.dynamic = true;
            plan.hasDynamicResult_ = true;
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
        if (hasMore && path[end] != '.') {
            return RecordCompileError(
                error,
                segmentIndex,
                currentType,
                name,
                InvalidPath(
                    "Binding path segment separator is invalid"));
        }
        if (segment.kind == BindingPathSegmentKind::ObjectProperty &&
            (hasMore || hasCollectionIndex) &&
            output->Kind() == MetadataTypeKind::Struct) {
            segment.copyOnWrite = true;
        }

        const TypeId propertyOutputType = segment.outputType;
        Base::Result<void> appended =
            plan.segments_.PushBack(std::move(segment));
        if (!appended) return appended.GetStatus();
        currentType = propertyOutputType;
        ++segmentIndex;

        if (hasCollectionIndex) {
            if (!IsObjectLike(output->Kind()) &&
                currentType != Meta::TypeOf<Meta::Value>()) {
                return RecordCompileError(
                    error,
                    segmentIndex,
                    currentType,
                    authoredName,
                    UnsupportedPath(
                        "Binding collection index requires an object-valued collection"));
            }
            BindingPathSegment indexSegment;
            indexSegment.kind = BindingPathSegmentKind::CollectionIndex;
            indexSegment.inputType = currentType;
            indexSegment.outputType = Meta::TypeOf<Base::Object>();
            indexSegment.collectionIndex = collectionIndex;
            indexSegment.readable = true;
            indexSegment.writable = true;
            appended = plan.segments_.PushBack(std::move(indexSegment));
            if (!appended) return appended.GetStatus();
            currentType = Meta::TypeOf<Base::Object>();
            plan.hasDynamicResult_ = true;
            ++segmentIndex;
        }
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
    Meta::Registry& runtime) const noexcept {
    if (!IsValid() || !runtime.IsReady()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Binding path plan or metadata program is not ready");
    }
    if (&runtime == compiledDomain_) return {};
    Base::Result<Base::HashCode> current =
        runtime.ComputeSchemaHash();
    if (!current) return current.GetStatus();
    if (current.Value() != schemaHash_) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Binding path schema hash does not match the runtime");
    }
    return {};
}

Base::Result<Value> BindingPathPlan::Get(
    Meta::Registry& runtime,
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
    Meta::Registry& runtime,
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
    Meta::Registry& runtime,
    const Base::Object& object,
    std::uint32_t segmentIndex) const noexcept {
    const BindingPathSegment& segment = segments_[segmentIndex];
    if (segment.kind == BindingPathSegmentKind::CollectionIndex) {
        if (!runtime.Types().IsDerivedFrom(
                object.RuntimeType(),
                Collections::ObservableCollection::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Binding collection index source is not an ObservableCollection");
        }
        const auto& collection =
            static_cast<const Collections::ObservableCollection&>(object);
        Base::Ref<Base::Object> item =
            collection.GetItem(segment.collectionIndex);
        if (!item) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Binding collection index is outside the collection");
        }
        Value current = Value::FromObject(
            item->RuntimeType(), item);
        if (segmentIndex + 1U == segments_.Size()) {
            return current;
        }
        return GetValue(runtime, current, segmentIndex + 1U);
    }
    if (segment.kind != BindingPathSegmentKind::ObjectProperty) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Binding path plan expected a value field on an object");
    }
    MemberId member = segment.member;
    if (segment.dynamic) {
        const PropertyInfo* property =
            runtime.Types().FindProperty(
                object.RuntimeType(),
                segment.dynamicName.View(),
                true);
        if (property == nullptr ||
            !runtime.CanReadProperty(property->Id())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Binding dynamic path property was not found");
        }
        member = property->Id();
    }
    Base::Result<Value> current =
        runtime.GetProperty(object, member);
    if (!current || segmentIndex + 1U == segments_.Size()) return current;
    return GetValue(runtime, current.Value(), segmentIndex + 1U);
}

Base::Result<Value> BindingPathPlan::GetValue(
    Meta::Registry& runtime,
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
    Meta::Registry& runtime,
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
    Meta::Registry& runtime,
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
    Meta::Registry& runtime,
    Base::Object& object,
    std::uint32_t segmentIndex,
    const Value& value) const noexcept {
    const BindingPathSegment& segment = segments_[segmentIndex];
    if (segment.kind == BindingPathSegmentKind::CollectionIndex) {
        if (!runtime.Types().IsDerivedFrom(
                object.RuntimeType(),
                Collections::ObservableCollection::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Binding collection index source is not an ObservableCollection");
        }
        auto& collection =
            static_cast<Collections::ObservableCollection&>(object);
        if (segmentIndex + 1U == segments_.Size()) {
            if (value.Kind() != ValueKind::Object ||
                value.IsNullObject() || !value.AsObject()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Binding collection index assignment requires an object");
            }
            return collection.Replace(
                segment.collectionIndex,
                value.AsObject());
        }
        Base::Ref<Base::Object> item =
            collection.GetItem(segment.collectionIndex);
        if (!item) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Binding collection index is outside the collection");
        }
        Value child = Value::FromObject(
            item->RuntimeType(), item);
        Base::Result<bool> changed =
            SetValue(runtime, child, segmentIndex + 1U, value);
        return changed
            ? Base::Result<void>()
            : Base::Result<void>(changed.GetStatus());
    }
    if (segment.kind != BindingPathSegmentKind::ObjectProperty) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Binding path plan expected a value field on an object");
    }
    MemberId member = segment.member;
    if (segment.dynamic) {
        const PropertyInfo* property =
            runtime.Types().FindProperty(
                object.RuntimeType(),
                segment.dynamicName.View(),
                true);
        if (property == nullptr ||
            !runtime.CanWriteProperty(property->Id())) {
            return Base::Status::Failure(
                Base::ErrorCode::ReadOnly,
                "Binding dynamic path property is not writable");
        }
        member = property->Id();
    }
    if (segmentIndex + 1U == segments_.Size()) {
        return runtime.SetProperty(object, member, value);
    }
    Base::Result<Value> child =
        runtime.GetProperty(object, member);
    if (!child) return child.GetStatus();
    Base::Result<bool> changed =
        SetValue(runtime, child.Value(), segmentIndex + 1U, value);
    if (!changed) return changed.GetStatus();
    return changed.Value()
        ? runtime.SetProperty(object, member, child.Value())
        : Base::Result<void>();
}

Base::Result<bool> BindingPathPlan::SetValue(
    Meta::Registry& runtime,
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

} // namespace Aero::Meta
