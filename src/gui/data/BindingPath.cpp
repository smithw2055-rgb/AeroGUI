#include "gui/core/TypeRegistryCore.hpp"
#include "gui/data/BindingPath.hpp"

#include <Aero/Base/HashMap.hpp>
#include <Aero/Collections.hpp>
#include <Aero/DependencyObject.hpp>
#include <Aero/PropertyPath.hpp>
#include <Aero/TryCast.hpp>

#include <cstdio>
#include <limits>
#include <utility>

namespace Aero::Data {
namespace {

using Aero::TryCast;
using Aero::TryCastToInterface;

constexpr std::uint32_t InvalidCompiledSegmentIndex = UINT32_MAX;

bool HasCompiledPropertyFlag(
    Meta::PropertyFlags value,
    Meta::PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Base::StringView TrimCompiledPathAscii(Base::StringView value) noexcept {
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

Base::Status InvalidCompiledPath(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::ValidationFailed, message);
}

Base::Status UnsupportedCompiledPath(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::Unsupported, message);
}

void ResetCompiledPathError(Meta::BindingPathCompileError* error) noexcept {
    if (error == nullptr) return;
    error->segmentIndex = InvalidCompiledSegmentIndex;
    error->inputType = Meta::InvalidTypeId;
    error->segment.Clear();
    error->status = {};
}

Base::Status RecordCompiledPathError(
    Meta::BindingPathCompileError* error,
    std::uint32_t segmentIndex,
    Meta::TypeId inputType,
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

bool IsCompiledPropertyReadable(
    const Meta::PropertyInfo& property,
    bool accessorReadable) noexcept {
    return accessorReadable &&
        !HasCompiledPropertyFlag(
            property.Flags(), Meta::PropertyFlags::WriteOnly);
}

bool IsCompiledPropertyWritable(
    const Meta::PropertyInfo& property,
    bool accessorWritable) noexcept {
    return accessorWritable &&
        !HasCompiledPropertyFlag(
            property.Flags(), Meta::PropertyFlags::ReadOnly);
}

bool IsObjectLikeKind(Meta::MetadataTypeKind kind) noexcept {
    return kind == Meta::MetadataTypeKind::Object ||
        kind == Meta::MetadataTypeKind::Interface;
}

const Meta::PropertyInfo* FindCompiledAttachedProperty(
    const Meta::TypeRegistry& descriptors,
    Base::StringView ownerName,
    Base::StringView propertyName) noexcept {
    for (std::uint32_t index = 0U; index < ownerName.SizeBytes(); ++index) {
        if (ownerName[index] == ':') {
            ownerName = ownerName.Substr(
                index + 1U,
                ownerName.SizeBytes() - index - 1U);
        }
    }
    if (ownerName.Empty() || propertyName.Empty()) return nullptr;
    for (const Meta::TypeInfo& type : descriptors.Types()) {
        if (type.Name() != ownerName) continue;
        const Meta::PropertyInfo* property = descriptors.FindProperty(
            type.Id(), propertyName, false);
        if (property != nullptr &&
            HasCompiledPropertyFlag(
                property->Flags(), Meta::PropertyFlags::Attached)) {
            return property;
        }
    }
    return nullptr;
}

Base::Status DynamicPropertyNotFoundStatus(
    const Meta::TypeRegistry& types,
    Meta::TypeId runtimeType,
    Base::StringView name) noexcept {
    const Meta::TypeInfo* rt = types.FindType(runtimeType);
    const Base::StringView rtName =
        rt != nullptr ? rt->Name() : Base::StringView("<unknown>");
    thread_local char message[384];
    std::snprintf(
        message,
        sizeof(message),
        "Binding dynamic path property '%.*s' was not found on '%.*s'",
        static_cast<int>(name.SizeBytes()),
        name.Data(),
        static_cast<int>(rtName.SizeBytes()),
        rtName.Data());
    return Base::Status::Failure(Base::ErrorCode::NotFound, message);
}

struct CompiledPathKey {
    const Meta::Registry* domain = nullptr;
    Meta::TypeId root = Meta::InvalidTypeId;
    Base::HashCode schema = 0U;
    Base::String path;
    bool operator==(const CompiledPathKey& other) const noexcept {
        return domain == other.domain && root == other.root &&
            schema == other.schema &&
            path.View() == other.path.View();
    }
};

struct CompiledPathKeyHash {
    Base::HashCode operator()(
        const CompiledPathKey& key,
        Base::HashCode seed = 0U) const noexcept {
        Base::HashCode hash = Base::DefaultHash<const Meta::Registry*>{}(
            key.domain, seed);
        hash = Base::DefaultHash<Meta::TypeId>{}(key.root, hash);
        hash = Base::DefaultHash<Base::HashCode>{}(key.schema, hash);
        return Base::DefaultHash<Base::String>{}(key.path, hash);
    }
};

struct CompiledPathCache {
    static constexpr std::uint32_t Capacity = 1024U;
    Base::HashMap<CompiledPathKey, CompiledBindingPath, CompiledPathKeyHash>
        plans;
};

CompiledPathCache& PathPlanCache() noexcept {
    static thread_local CompiledPathCache cache;
    return cache;
}

} // namespace

Base::Result<CompiledBindingPath> CompiledBindingPath::Compile(
    const Meta::Registry& runtime,
    Meta::TypeId rootType,
    Base::StringView path,
    Meta::BindingPathCompileError* error) noexcept {
    ResetCompiledPathError(error);
    path = TrimCompiledPathAscii(path);
    if (!runtime.IsReady() || rootType == Meta::InvalidTypeId || path.Empty()) {
        return RecordCompiledPathError(
            error,
            InvalidCompiledSegmentIndex,
            rootType,
            path,
            InvalidCompiledPath(
                "Binding path requires a frozen runtime, root type, and path"));
    }

    const Meta::TypeRegistry& descriptors = runtime.Types();
    if (descriptors.FindType(rootType) == nullptr) {
        return RecordCompiledPathError(
            error,
            0U,
            rootType,
            {},
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Binding path root type descriptor was not found"));
    }
    Base::Result<Base::HashCode> schemaHash = runtime.ComputeSchemaHash();
    if (!schemaHash) {
        return schemaHash.GetStatus();
    }

    CompiledPathKey cacheKey;
    cacheKey.domain = &runtime;
    cacheKey.root = rootType;
    cacheKey.schema = schemaHash.Value();
    const bool cacheable = static_cast<bool>(cacheKey.path.Assign(path));
    if (cacheable) {
        if (CompiledBindingPath* hit = PathPlanCache().plans.Find(cacheKey)) {
            return *hit;
        }
    }

    CompiledBindingPath plan;
    Base::Result<void> bound = plan.BindToRoot(runtime, rootType, path, error);
    if (!bound) {
        return bound.GetStatus();
    }
    plan.schemaHash_ = schemaHash.Value();

    if (cacheable) {
        CompiledPathCache& cache = PathPlanCache();
        if (cache.plans.Size() >= CompiledPathCache::Capacity) {
            cache.plans.Clear();
        }
        static_cast<void>(cache.plans.Insert(cacheKey, plan));
    }
    return plan;
}

void CompiledBindingPath::ClearStepCache(CompiledPathStep& step) noexcept {
    step.ownerType = Meta::InvalidTypeId;
    step.member = Meta::InvalidMemberId;
    step.propertyInfo = nullptr;
    step.dependencyProperty = nullptr;
    step.propertyGetter = nullptr;
    step.propertySetter = nullptr;
    step.accessorContext = nullptr;
    step.accessorReadable = false;
    step.accessorWritable = false;
    step.cachedReadable = false;
    step.cachedWritable = false;
    step.ownerIsDependencyObject = false;
    step.outputIsEnum = false;
}

void CompiledBindingPath::PopulateStepCache(
    const Meta::Registry& runtime,
    Meta::TypeId stepOwnerType,
    const Meta::PropertyInfo* property,
    CompiledPathStep& step) noexcept {
    step.ownerType = stepOwnerType;
    step.propertyInfo = property;
    step.member = property->Id();
    step.attached = step.attached ||
        HasCompiledPropertyFlag(
            property->Flags(), Meta::PropertyFlags::Attached);
    step.propertyGetter = nullptr;
    step.propertySetter = nullptr;
    step.accessorContext = nullptr;
    if (runtime.IsReady()) {
        const ::Aero::PropertyAccessorFacet* accessor =
            runtime.RuntimeData().FindPropertyAccessor(property->Id());
        if (accessor != nullptr &&
            accessor->access == Meta::PropertyAccessKind::Ordinary) {
            step.propertyGetter = accessor->get;
            step.propertySetter = accessor->set;
            step.accessorContext = accessor->context;
        }
    }
    step.accessorReadable =
        step.propertyGetter != nullptr || runtime.CanReadProperty(property->Id());
    step.accessorWritable =
        step.propertySetter != nullptr || runtime.CanWriteProperty(property->Id());
    step.cachedReadable = IsCompiledPropertyReadable(
        *property,
        step.accessorReadable);
    step.cachedWritable = IsCompiledPropertyWritable(
        *property,
        step.accessorWritable);
    step.dependencyProperty = runtime.DependencyProperties().Find(
        Meta::DependencyPropertyHandle{property->Id()});
    step.ownerIsDependencyObject =
        step.dependencyProperty != nullptr &&
        runtime.Types().IsAssignableFrom(
            Meta::TypeOf<DependencyObject>(),
            stepOwnerType);
    const Meta::TypeInfo* outInfo =
        runtime.Types().FindType(property->ValueType());
    step.outputIsEnum =
        outInfo != nullptr &&
        outInfo->Kind() == Meta::MetadataTypeKind::Enum;
}

void CompiledBindingPath::WarmCache(
    const Meta::Registry& runtime) noexcept {
    if (cachedRuntime_ != nullptr && cachedRuntime_ != &runtime) {
        for (std::uint32_t i = 0U; i < steps_.Size(); ++i) {
            if (steps_[i].dynamic) {
                ClearStepCache(steps_[i]);
            } else {
                steps_[i].ownerType = Meta::InvalidTypeId;
                steps_[i].propertyInfo = nullptr;
                steps_[i].dependencyProperty = nullptr;
                steps_[i].propertyGetter = nullptr;
                steps_[i].propertySetter = nullptr;
                steps_[i].accessorContext = nullptr;
            }
        }
    }
    cachedRuntime_ = &runtime;
    for (std::uint32_t i = 0U; i < steps_.Size(); ++i) {
        CompiledPathStep& step = steps_[i];
        if (step.kind != Meta::BindingPathSegmentKind::ObjectProperty) {
            continue;
        }
        if (!step.dynamic &&
            step.propertyInfo == nullptr &&
            step.member != Meta::InvalidMemberId &&
            step.inputType != Meta::InvalidTypeId) {
            const Meta::PropertyInfo* property =
                runtime.Types().FindProperty(step.member);
            if (property != nullptr) {
                if (step.propertyName.Empty()) {
                    (void)step.propertyName.Assign(property->Name());
                }
                PopulateStepCache(runtime, step.inputType, property, step);
            }
        }
    }
}

Base::Result<void> CompiledBindingPath::PrecompileTokens(
    Base::StringView path) noexcept {
    tokenizeAttempted_ = true;
    if (tokenized_) {
        return {};
    }
    steps_.Clear();
    valid_ = false;
    targetValidated_ = false;
    path = TrimCompiledPathAscii(path);
    if (path.Empty()) {
        return InvalidCompiledPath(
            "Binding path requires a frozen runtime, root type, and path");
    }
    Base::Vector<PropertyPathToken> tokens;
    Base::Result<void> parsed = PropertyPath::ParseTokens(path, tokens);
    if (!parsed) {
        return parsed.GetStatus();
    }
    if (tokens.Empty()) {
        return InvalidCompiledPath("Binding path contains an empty segment");
    }
    steps_.Reserve(tokens.Size());
    for (std::uint32_t tokenIndex = 0U;
         tokenIndex < tokens.Size();
         ++tokenIndex) {
        const PropertyPathToken& tok = tokens[tokenIndex];
        CompiledPathStep propStep;
        propStep.kind = Meta::BindingPathSegmentKind::ObjectProperty;
        propStep.attached = tok.isTypeQualified;
        propStep.hasCollectionIndexAfter = tok.hasIndex;
        if (!tok.propertyName.Empty()) {
            Base::Result<void> assigned =
                propStep.propertyName.Assign(tok.propertyName);
            if (!assigned) {
                steps_.Clear();
                return assigned.GetStatus();
            }
        }
        if (tok.isTypeQualified && !tok.typeQualifier.Empty()) {
            Base::Result<void> assigned =
                propStep.attachedOwnerName.Assign(tok.typeQualifier);
            if (!assigned) {
                steps_.Clear();
                return assigned.GetStatus();
            }
        }
        if (!tok.raw.Empty()) {
            Base::Result<void> assigned =
                propStep.rawToken.Assign(tok.raw);
            if (!assigned) {
                steps_.Clear();
                return assigned.GetStatus();
            }
        }
        steps_.PushBack(std::move(propStep));

        if (tok.hasIndex) {
            CompiledPathStep indexStep;
            indexStep.kind = Meta::BindingPathSegmentKind::CollectionIndex;
            indexStep.collectionIndex = tok.collectionIndex;
            indexStep.outputType = Meta::TypeOf<Base::Object>();
            indexStep.readable = true;
            indexStep.writable = true;
            indexStep.accessorReadable = true;
            indexStep.accessorWritable = true;
            indexStep.cachedReadable = true;
            indexStep.cachedWritable = true;
            if (!tok.raw.Empty()) {
                Base::Result<void> assigned =
                    indexStep.rawToken.Assign(tok.raw);
                if (!assigned) {
                    steps_.Clear();
                    return assigned.GetStatus();
                }
            }
            steps_.PushBack(std::move(indexStep));
        }
    }
    tokenized_ = true;
    return {};
}

Base::Result<void> CompiledBindingPath::BindToRoot(
    const Meta::Registry& runtime,
    Meta::TypeId rootType,
    Base::StringView path,
    Meta::BindingPathCompileError* error) noexcept {
    ResetCompiledPathError(error);
    valid_ = false;
    targetValidated_ = false;
    path = TrimCompiledPathAscii(path);
    if (!runtime.IsReady() || rootType == Meta::InvalidTypeId || path.Empty()) {
        return RecordCompiledPathError(
            error,
            InvalidCompiledSegmentIndex,
            rootType,
            path,
            InvalidCompiledPath(
                "Binding path requires a frozen runtime, root type, and path"));
    }

    const Meta::TypeRegistry& descriptors = runtime.Types();
    if (descriptors.FindType(rootType) == nullptr) {
        return RecordCompiledPathError(
            error,
            0U,
            rootType,
            {},
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Binding path root type descriptor was not found"));
    }

    if (!tokenized_) {
        Base::Result<void> precompiled = PrecompileTokens(path);
        if (!precompiled) {
            return RecordCompiledPathError(
                error,
                InvalidCompiledSegmentIndex,
                rootType,
                path,
                precompiled.GetStatus());
        }
    }

    if (cachedRuntime_ != nullptr && cachedRuntime_ != &runtime) {
        for (std::uint32_t i = 0U; i < steps_.Size(); ++i) {
            ClearStepCache(steps_[i]);
        }
    }

    Meta::TypeId currentType = rootType;
    bool hasDynamicResult = false;
    for (std::uint32_t stepIndex = 0U;
         stepIndex < steps_.Size();
         ++stepIndex) {
        CompiledPathStep& step = steps_[stepIndex];
        if (step.kind == Meta::BindingPathSegmentKind::CollectionIndex) {
            const Meta::TypeInfo* prevOutput =
                descriptors.FindType(currentType);
            if (prevOutput == nullptr ||
                (!IsObjectLikeKind(prevOutput->Kind()) &&
                 currentType != Meta::TypeOf<Meta::Value>())) {
                return RecordCompiledPathError(
                    error,
                    stepIndex,
                    currentType,
                    step.rawToken.View(),
                    UnsupportedCompiledPath(
                        "Binding collection index requires an object-valued collection"));
            }
            step.inputType = currentType;
            step.outputType = Meta::TypeOf<Base::Object>();
            step.readable = true;
            step.writable = true;
            step.accessorReadable = true;
            step.accessorWritable = true;
            step.cachedReadable = true;
            step.cachedWritable = true;
            currentType = Meta::TypeOf<Base::Object>();
            hasDynamicResult = true;
            continue;
        }

        const Base::StringView name = step.propertyName.View();
        if (name.Empty()) {
            return RecordCompiledPathError(
                error,
                stepIndex,
                currentType,
                name,
                InvalidCompiledPath("Binding path contains an empty segment"));
        }

        const Meta::TypeInfo* input = descriptors.FindType(currentType);
        if (input == nullptr) {
            return RecordCompiledPathError(
                error,
                stepIndex,
                currentType,
                name,
                Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Binding path input type descriptor was not found"));
        }

        const Meta::TypeId previousInputType = step.inputType;
        const bool wasDynamic = step.dynamic;
        step.inputType = currentType;
        step.copyOnWrite = false;
        if (IsObjectLikeKind(input->Kind())) {
            const Meta::PropertyInfo* property = nullptr;
            if (cachedRuntime_ == &runtime &&
                step.propertyInfo != nullptr &&
                (step.ownerType == currentType ||
                 (!wasDynamic &&
                  (previousInputType == currentType || step.attached)))) {
                property = step.propertyInfo;
            } else {
                property = step.attached
                    ? FindCompiledAttachedProperty(
                          descriptors,
                          step.attachedOwnerName.View(),
                          name)
                    : descriptors.FindProperty(currentType, name, true);
            }
            if (property == nullptr) {
                const bool runtimePolymorphic =
                    !step.attached && IsObjectLikeKind(input->Kind());
                if (runtimePolymorphic) {
                    if (!wasDynamic) {
                        ClearStepCache(step);
                    }
                    step.outputType = Meta::TypeOf<Base::Object>();
                    step.readable = true;
                    step.writable = true;
                    step.dynamic = true;
                    hasDynamicResult = true;
                } else {
                    thread_local char message[320];
                    std::snprintf(
                        message,
                        sizeof(message),
                        "Binding path %s property '%.*s' was not found on type '%.*s'",
                        step.attached ? "attached" : "object",
                        static_cast<int>(name.SizeBytes()),
                        name.Data(),
                        static_cast<int>(input->Name().SizeBytes()),
                        input->Name().Data());
                    return RecordCompiledPathError(
                        error,
                        stepIndex,
                        currentType,
                        name,
                        Base::Status::Failure(
                            Base::ErrorCode::NotFound,
                            message));
                }
            } else {
                const bool canKeepRuntimeOwner =
                    cachedRuntime_ == &runtime &&
                    step.propertyInfo == property &&
                    step.ownerType != Meta::InvalidTypeId &&
                    (step.ownerType == currentType ||
                     step.attached ||
                     descriptors.IsAssignableFrom(currentType, step.ownerType));
                if (!canKeepRuntimeOwner) {
                    PopulateStepCache(runtime, currentType, property, step);
                }
                step.dynamic = false;
                step.outputType = property->ValueType();
                step.readable = step.cachedReadable;
                step.writable = step.cachedWritable;
            }
        } else if (currentType == Meta::TypeOf<Meta::Value>()) {
            if (step.attached) {
                return RecordCompiledPathError(
                    error,
                    stepIndex,
                    currentType,
                    name,
                    Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Binding path attached property cannot be resolved from Any"));
            }
            if (!wasDynamic) {
                ClearStepCache(step);
            }
            step.outputType = Meta::TypeOf<Base::Object>();
            step.readable = true;
            step.writable = true;
            step.dynamic = true;
            hasDynamicResult = true;
        } else if (input->Kind() == Meta::MetadataTypeKind::Struct) {
            return RecordCompiledPathError(
                error,
                stepIndex,
                currentType,
                name,
                Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Binding path value fields are not supported"));
        } else {
            return RecordCompiledPathError(
                error,
                stepIndex,
                currentType,
                name,
                UnsupportedCompiledPath(
                    "Binding path cannot traverse this metadata type kind"));
        }

        const Meta::TypeInfo* output = descriptors.FindType(step.outputType);
        if (output == nullptr) {
            return RecordCompiledPathError(
                error,
                stepIndex,
                currentType,
                name,
                Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Binding path output type descriptor was not found"));
        }
        const bool hasMoreSteps = (stepIndex + 1U < steps_.Size());
        if (hasMoreSteps &&
            output->Kind() == Meta::MetadataTypeKind::Struct) {
            step.copyOnWrite = true;
        }
        currentType = step.outputType;
    }

    cachedRuntime_ = &runtime;
    rootType_ = rootType;
    resultType_ = currentType;
    dynamicResult_ = hasDynamicResult;
    readable_ = true;
    writable_ = true;
    for (std::uint32_t index = 0U; index < steps_.Size(); ++index) {
        const CompiledPathStep& step = steps_[index];
        readable_ = readable_ && step.readable;
        const bool leaf = (index + 1U == steps_.Size());
        if (leaf) {
            writable_ = writable_ && step.writable;
        } else {
            writable_ = writable_ && step.readable &&
                (!step.copyOnWrite || step.writable);
        }
    }
    valid_ = true;
    return {};
}

Base::Result<void> CompiledBindingPath::EnsureStepResolvedForRead(
    const Meta::Registry& runtime,
    Meta::TypeId currentType,
    CompiledPathStep& step) noexcept {
    if (step.propertyInfo != nullptr && step.ownerType == currentType) {
        return {};
    }
    const Meta::PropertyInfo* property = nullptr;
    if (!step.dynamic) {
        property = step.propertyInfo;
        if (property == nullptr && step.member != Meta::InvalidMemberId) {
            property = runtime.Types().FindProperty(step.member);
        }
        if (property == nullptr && !step.propertyName.Empty()) {
            property = step.attached
                ? FindCompiledAttachedProperty(
                      runtime.Types(),
                      step.attachedOwnerName.View(),
                      step.propertyName.View())
                : runtime.Types().FindProperty(
                      currentType, step.propertyName.View(), true);
        }
        if (property == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Metadata property descriptor was not found");
        }
        if (HasCompiledPropertyFlag(
                property->Flags(), Meta::PropertyFlags::Attached)) {
            if (currentType == Meta::InvalidTypeId ||
                runtime.Types().FindType(currentType) == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Attached metadata property target has no type information");
            }
        } else if (
            currentType != property->OwnerType() &&
            !runtime.Types().IsAssignableFrom(
                property->OwnerType(), currentType)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Object type is incompatible with the metadata property");
        }
        PopulateStepCache(runtime, currentType, property, step);
        return {};
    }

    property = step.attached
        ? FindCompiledAttachedProperty(
              runtime.Types(),
              step.attachedOwnerName.View(),
              step.propertyName.View())
        : runtime.Types().FindProperty(
              currentType, step.propertyName.View(), true);
    if (property == nullptr ||
        !runtime.CanReadProperty(property->Id())) {
        return DynamicPropertyNotFoundStatus(
            runtime.Types(),
            currentType,
            step.propertyName.View());
    }
    PopulateStepCache(runtime, currentType, property, step);
    return {};
}

Base::Result<void> CompiledBindingPath::EnsureStepResolvedForWrite(
    const Meta::Registry& runtime,
    Meta::TypeId currentType,
    CompiledPathStep& step) noexcept {
    if (step.propertyInfo != nullptr && step.ownerType == currentType) {
        return {};
    }
    const Meta::PropertyInfo* property = nullptr;
    if (!step.dynamic) {
        property = step.propertyInfo;
        if (property == nullptr && step.member != Meta::InvalidMemberId) {
            property = runtime.Types().FindProperty(step.member);
        }
        if (property == nullptr && !step.propertyName.Empty()) {
            property = step.attached
                ? FindCompiledAttachedProperty(
                      runtime.Types(),
                      step.attachedOwnerName.View(),
                      step.propertyName.View())
                : runtime.Types().FindProperty(
                      currentType, step.propertyName.View(), true);
        }
        if (property == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Metadata property descriptor was not found");
        }
        if (HasCompiledPropertyFlag(
                property->Flags(), Meta::PropertyFlags::Attached)) {
            if (currentType == Meta::InvalidTypeId ||
                runtime.Types().FindType(currentType) == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Attached metadata property target has no type information");
            }
        } else if (
            currentType != property->OwnerType() &&
            !runtime.Types().IsAssignableFrom(
                property->OwnerType(), currentType)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Object type is incompatible with the metadata property");
        }
        PopulateStepCache(runtime, currentType, property, step);
        return {};
    }

    property = step.attached
        ? FindCompiledAttachedProperty(
              runtime.Types(),
              step.attachedOwnerName.View(),
              step.propertyName.View())
        : runtime.Types().FindProperty(
              currentType, step.propertyName.View(), true);
    if (property == nullptr ||
        !runtime.CanWriteProperty(property->Id())) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Binding dynamic path property is not writable");
    }
    PopulateStepCache(runtime, currentType, property, step);
    return {};
}

Base::Result<Meta::PropertyValue> CompiledBindingPath::ReadSinglePropertyStep(
    const Meta::Registry& runtime,
    const Base::Object& object,
    CompiledPathStep& step) noexcept {
    if (step.propertyInfo == nullptr ||
        object.RuntimeType() != step.ownerType) {
        Base::Result<void> resolved =
            EnsureStepResolvedForRead(runtime, object.RuntimeType(), step);
        if (!resolved) {
            return resolved.GetStatus();
        }
    }
    if (!step.cachedReadable) {
        if (step.dynamic && !step.accessorReadable) {
            return DynamicPropertyNotFoundStatus(
                runtime.Types(),
                object.RuntimeType(),
                step.propertyName.View());
        }
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            HasCompiledPropertyFlag(
                step.propertyInfo->Flags(),
                Meta::PropertyFlags::WriteOnly)
                ? "Write-only metadata property cannot be read"
                : "Metadata property has no usable accessor");
    }
    if (step.dependencyProperty != nullptr &&
        step.ownerIsDependencyObject) {
        const auto& dependencyObject =
            static_cast<const DependencyObject&>(object);
        const Meta::PropertyValue value =
            dependencyObject.GetValue(step.dependencyProperty->Handle());
        const Meta::TypeId expectedType = step.propertyInfo->ValueType();
        if (value.IsUnset()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Metadata getter returned an incompatible value");
        }
        if (value.Type() == expectedType ||
            HasCompiledPropertyFlag(
                step.propertyInfo->Flags(),
                Meta::PropertyFlags::AnyValue)) {
            return value;
        }
        if (value.Kind() == Meta::ValueKind::Object &&
            runtime.Types().IsAssignableFrom(
                expectedType, value.Type())) {
            return Meta::PropertyValue::FromObject(
                expectedType,
                value.AsObject());
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata getter returned an incompatible value");
    }
    if (step.propertyGetter != nullptr) {
        Base::Result<Meta::PropertyValue> value =
            step.propertyGetter(object, step.accessorContext);
        if (!value) {
            return value.GetStatus();
        }
        const Meta::TypeId expectedType = step.propertyInfo->ValueType();
        if (value.Value().IsUnset()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Metadata getter returned an incompatible value");
        }
        if (value.Value().Type() == expectedType ||
            HasCompiledPropertyFlag(
                step.propertyInfo->Flags(),
                Meta::PropertyFlags::AnyValue)) {
            return value;
        }
        if (value.Value().Kind() == Meta::ValueKind::Object &&
            runtime.Types().IsAssignableFrom(
                expectedType, value.Value().Type())) {
            return Meta::PropertyValue::FromObject(
                expectedType,
                value.Value().AsObject());
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata getter returned an incompatible value");
    }
    return runtime.GetProperty(object, step.member);
}

Base::Result<void> CompiledBindingPath::WriteSinglePropertyStep(
    Meta::Registry& runtime,
    Base::Object& object,
    CompiledPathStep& step,
    const Meta::PropertyValue& value) noexcept {
    if (step.propertyInfo == nullptr ||
        object.RuntimeType() != step.ownerType) {
        Base::Result<void> resolved =
            EnsureStepResolvedForWrite(runtime, object.RuntimeType(), step);
        if (!resolved) {
            return resolved.GetStatus();
        }
    }
    if (step.dynamic && !step.accessorWritable) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Binding dynamic path property is not writable");
    }
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata property value is unset");
    }
    if (!step.cachedWritable) {
        if (HasCompiledPropertyFlag(
                step.propertyInfo->Flags(),
                Meta::PropertyFlags::ReadOnly)) {
            return Base::Status::Failure(
                Base::ErrorCode::ReadOnly,
                "Read-only metadata property cannot be written");
        }
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata property has no usable accessor");
    }
    if (!step.outputIsEnum &&
        ((step.dependencyProperty != nullptr && step.ownerIsDependencyObject) ||
         step.propertySetter != nullptr)) {
        const Meta::TypeId expectedType = step.propertyInfo->ValueType();
        const bool acceptsAnyValue = HasCompiledPropertyFlag(
            step.propertyInfo->Flags(), Meta::PropertyFlags::AnyValue);
        if (!acceptsAnyValue && value.Type() != expectedType) {
            const bool objectAssignment =
                value.Kind() == Meta::ValueKind::Object &&
                runtime.Types().IsAssignableFrom(expectedType, value.Type());
            if (!objectAssignment) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Metadata property value type does not match the descriptor");
            }
        }
        if (step.dependencyProperty != nullptr &&
            step.ownerIsDependencyObject) {
            auto& dependencyObject = static_cast<DependencyObject&>(object);
            dependencyObject.SetValue(step.dependencyProperty->Handle(), value);
            return {};
        }
        step.propertySetter(object, value, step.accessorContext);
        return {};
    }
    return runtime.SetProperty(object, step.member, value);
}

Base::Result<void> CompiledBindingPath::VerifyRuntime(
    const Meta::Registry& runtime) const noexcept {
    if (!valid_ || !runtime.IsReady() || steps_.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Binding path plan or metadata program is not ready");
    }
    if (cachedRuntime_ != &runtime) {
        if (schemaHash_ != 0U) {
            Base::Result<Base::HashCode> current = runtime.ComputeSchemaHash();
            if (!current) return current.GetStatus();
            if (current.Value() != schemaHash_) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding path schema hash does not match the runtime");
            }
        }
        const_cast<CompiledBindingPath*>(this)->WarmCache(runtime);
    }
    return {};
}

Base::Result<Meta::PropertyValue> CompiledBindingPath::Get(
    const Meta::Registry& runtime,
    const Base::Object& root) const noexcept {
    Base::Result<void> ready = VerifyRuntime(runtime);
    if (!ready) return ready.GetStatus();
    if (!readable_) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Binding path is not readable");
    }
    if (root.RuntimeType() != rootType_ &&
        !runtime.Types().IsAssignableFrom(rootType_, root.RuntimeType())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Binding path root object type is incompatible");
    }
    return GetObject(runtime, root, 0U);
}

Base::Result<Meta::PropertyValue> CompiledBindingPath::Get(
    const Meta::Registry& runtime,
    const Meta::PropertyValue& root) const noexcept {
    Base::Result<void> ready = VerifyRuntime(runtime);
    if (!ready) return ready.GetStatus();
    if (!readable_) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Binding path is not readable");
    }
    if (root.Kind() == Meta::ValueKind::Object) {
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

Base::Result<Meta::PropertyValue> CompiledBindingPath::GetObject(
    const Meta::Registry& runtime,
    const Base::Object& object,
    std::uint32_t stepIndex) const noexcept {
    CompiledPathStep& step = steps_[stepIndex];
    if (step.kind == Meta::BindingPathSegmentKind::CollectionIndex) {
        const Collections::IItemsSource* items =
            TryCastToInterface<Collections::IItemsSource>(
                const_cast<Base::Object*>(&object));
        if (items == nullptr) {
            items = Collections::CollectionAsItemsSource(&object);
        }
        if (items == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Binding collection index source does not implement IItemsSource");
        }
        Base::Ref<Base::Object> item =
            items->GetItem(step.collectionIndex);
        if (!item) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Binding collection index is outside the collection");
        }
        Meta::PropertyValue current = Meta::PropertyValue::FromObject(
            item->RuntimeType(), item);
        if (stepIndex + 1U == steps_.Size()) {
            return current;
        }
        return GetValue(runtime, current, stepIndex + 1U);
    }
    if (step.kind != Meta::BindingPathSegmentKind::ObjectProperty) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Binding path plan expected a value field on an object");
    }
    Base::Result<Meta::PropertyValue> current =
        ReadSinglePropertyStep(runtime, object, step);
    if (!current || stepIndex + 1U == steps_.Size()) {
        return current;
    }
    return GetValue(runtime, current.Value(), stepIndex + 1U);
}

Base::Result<Meta::PropertyValue> CompiledBindingPath::GetValue(
    const Meta::Registry& runtime,
    const Meta::PropertyValue& current,
    std::uint32_t stepIndex) const noexcept {
    if (current.Kind() == Meta::ValueKind::Object) {
        if (current.IsNullObject()) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Binding path reached a null object");
        }
        return GetObject(runtime, *current.AsObject(), stepIndex);
    }
    const CompiledPathStep& step = steps_[stepIndex];
    if (current.Kind() != Meta::ValueKind::Custom ||
        step.kind != Meta::BindingPathSegmentKind::ValueField ||
        current.Type() != step.inputType) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Binding path value segment is incompatible");
    }
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "Binding path value fields are not supported");
}

Base::Result<void> CompiledBindingPath::Set(
    Meta::Registry& runtime,
    Base::Object& root,
    const Meta::PropertyValue& value) const noexcept {
    Base::Result<void> ready = VerifyRuntime(runtime);
    if (!ready) return ready;
    if (!writable_) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Binding path is not writable");
    }
    if (value.IsUnset() ||
        (root.RuntimeType() != rootType_ &&
         !runtime.Types().IsAssignableFrom(rootType_, root.RuntimeType()))) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Binding path root or assigned value is incompatible");
    }
    return SetObject(runtime, root, 0U, value);
}

Base::Result<void> CompiledBindingPath::Set(
    Meta::Registry& runtime,
    Meta::PropertyValue& root,
    const Meta::PropertyValue& value) const noexcept {
    Base::Result<void> ready = VerifyRuntime(runtime);
    if (!ready) return ready;
    if (!writable_) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Binding path is not writable");
    }
    if (value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Binding path assigned value is unset");
    }
    if (root.Kind() == Meta::ValueKind::Object) {
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
    Base::Result<bool> changed = SetValue(runtime, root, 0U, value);
    return changed
        ? Base::Result<void>()
        : Base::Result<void>(changed.GetStatus());
}

Base::Result<void> CompiledBindingPath::SetObject(
    Meta::Registry& runtime,
    Base::Object& object,
    std::uint32_t stepIndex,
    const Meta::PropertyValue& value) const noexcept {
    CompiledPathStep& step = steps_[stepIndex];
    if (step.kind == Meta::BindingPathSegmentKind::CollectionIndex) {
        Collections::IItemsSource* items =
            TryCastToInterface<Collections::IItemsSource>(&object);
        if (items == nullptr) {
            items = Collections::CollectionAsItemsSource(&object);
        }
        auto* writable =
            TryCast<Collections::ObservableCollectionBase>(&object);
        if (items == nullptr && writable == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Binding collection index assignment requires IItemsSource");
        }
        if (stepIndex + 1U == steps_.Size()) {
            if (writable == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Binding collection index assignment requires ObservableCollection");
            }
            if (value.Kind() != Meta::ValueKind::Object ||
                value.IsNullObject() || !value.AsObject()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Binding collection index assignment requires an object");
            }
            if (!writable->ReplaceItem(
                    step.collectionIndex,
                    value.AsObject())) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Binding collection index assignment failed");
            }
            return {};
        }
        Base::Ref<Base::Object> item =
            items != nullptr
                ? items->GetItem(step.collectionIndex)
                : writable->GetItem(step.collectionIndex);
        if (!item) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Binding collection index is outside the collection");
        }
        Meta::PropertyValue child = Meta::PropertyValue::FromObject(
            item->RuntimeType(), item);
        Base::Result<bool> changed =
            SetValue(runtime, child, stepIndex + 1U, value);
        return changed
            ? Base::Result<void>()
            : Base::Result<void>(changed.GetStatus());
    }
    if (step.kind != Meta::BindingPathSegmentKind::ObjectProperty) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Binding path plan expected a value field on an object");
    }
    if (stepIndex + 1U == steps_.Size()) {
        return WriteSinglePropertyStep(runtime, object, step, value);
    }
    Base::Result<Meta::PropertyValue> child =
        ReadSinglePropertyStep(runtime, object, step);
    if (!child) return child.GetStatus();
    Base::Result<bool> changed =
        SetValue(runtime, child.Value(), stepIndex + 1U, value);
    if (!changed) return changed.GetStatus();
    return changed.Value()
        ? WriteSinglePropertyStep(runtime, object, step, child.Value())
        : Base::Result<void>();
}

Base::Result<bool> CompiledBindingPath::SetValue(
    Meta::Registry& runtime,
    Meta::PropertyValue& current,
    std::uint32_t stepIndex,
    const Meta::PropertyValue& value) const noexcept {
    if (current.Kind() == Meta::ValueKind::Object) {
        if (current.IsNullObject()) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Binding path reached a null object");
        }
        Base::Result<void> result =
            SetObject(runtime, *current.AsObject(), stepIndex, value);
        return result
            ? Base::Result<bool>(false)
            : Base::Result<bool>(result.GetStatus());
    }
    const CompiledPathStep& step = steps_[stepIndex];
    if (current.Kind() != Meta::ValueKind::Custom ||
        step.kind != Meta::BindingPathSegmentKind::ValueField ||
        current.Type() != step.inputType) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Binding path value segment is incompatible");
    }
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "Binding path value fields are not supported");
}

} // namespace Aero::Data
