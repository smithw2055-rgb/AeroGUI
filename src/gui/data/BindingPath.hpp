#pragma once

// Immutable and cached property-path plans used by binding expressions and compiled XAML.
#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero::Meta {

class DependencyProperty;
class PropertyInfo;
class Registry;

using PropertyValue = ::Aero::Base::Value;

using PropertyGetCallback = Base::Result<Value> (*)(
    const Base::Object& object,
    void* context) noexcept;
using PropertySetCallback = void (*)(
    Base::Object& object,
    const Value& value,
    void* context) noexcept;

enum class BindingPathSegmentKind : std::uint8_t {
    ObjectProperty = 0U,
    ValueField,
    CollectionIndex
};

struct BindingPathCompileError {
    std::uint32_t segmentIndex = UINT32_MAX;
    TypeId inputType = InvalidTypeId;
    Base::String segment;
    Base::Status status;
};

} // namespace Aero::Meta

namespace Aero::Data {

struct CompiledPathStep {
    Meta::BindingPathSegmentKind kind =
        Meta::BindingPathSegmentKind::ObjectProperty;
    std::uint32_t collectionIndex = UINT32_MAX;
    Base::String propertyName;
    Base::String attachedOwnerName;
    Base::String rawToken;
    Meta::TypeId inputType = Meta::InvalidTypeId;
    Meta::TypeId outputType = Meta::InvalidTypeId;
    bool attached = false;
    bool hasCollectionIndexAfter = false;
    bool dynamic = false;
    bool readable = false;
    bool writable = false;
    bool copyOnWrite = false;

    // Per-step runtime cache keyed by ownerType.
    Meta::TypeId ownerType = Meta::InvalidTypeId;
    Meta::MemberId member = Meta::InvalidMemberId;
    const Meta::PropertyInfo* propertyInfo = nullptr;
    const Meta::DependencyProperty* dependencyProperty = nullptr;
    Meta::PropertyGetCallback propertyGetter = nullptr;
    Meta::PropertySetCallback propertySetter = nullptr;
    void* accessorContext = nullptr;
    bool accessorReadable = false;
    bool accessorWritable = false;
    bool cachedReadable = false;
    bool cachedWritable = false;
    bool ownerIsDependencyObject = false;
    bool outputIsEnum = false;
};

class CompiledBindingPath final {
public:
    CompiledBindingPath() noexcept = default;

    static Base::Result<CompiledBindingPath> Compile(
        const Meta::Registry& runtime,
        Meta::TypeId rootType,
        Base::StringView path,
        Meta::BindingPathCompileError* error = nullptr) noexcept;

    static Base::Result<const CompiledBindingPath*> FindOrCompile(
        const Meta::Registry& runtime,
        Meta::TypeId rootType,
        Base::StringView path,
        Meta::BindingPathCompileError* error = nullptr) noexcept;

    Base::Result<void> PrecompileTokens(Base::StringView path) noexcept;
    Base::Result<void> BindToRoot(
        const Meta::Registry& runtime,
        Meta::TypeId rootType,
        Base::StringView path,
        Meta::BindingPathCompileError* error = nullptr) noexcept;
    void WarmCache(const Meta::Registry& runtime) noexcept;

    void InvalidateResolution() noexcept {
        valid_ = false;
    }

    bool HasTokenizeAttempt() const noexcept {
        return tokenizeAttempted_;
    }

    bool IsTargetValidatedFor(
        const Meta::Registry& runtime,
        Meta::TypeId rootType) const noexcept {
        return targetValidated_ &&
            cachedRuntime_ == &runtime &&
            rootType != Meta::InvalidTypeId &&
            rootType_ == rootType &&
            !steps_.Empty();
    }

    void ActivateCachedRoot() noexcept {
        valid_ = true;
    }

    void MarkTargetValidated() noexcept {
        targetValidated_ = valid_;
    }

    bool IsValid() const noexcept {
        return valid_;
    }

    Meta::TypeId RootType() const noexcept {
        return rootType_;
    }

    Meta::TypeId ResultType() const noexcept {
        return resultType_;
    }

    Base::HashCode SchemaHash() const noexcept {
        return schemaHash_;
    }

    bool HasDynamicResult() const noexcept {
        return dynamicResult_;
    }

    bool CanRead() const noexcept {
        return readable_;
    }

    bool CanWrite() const noexcept {
        return writable_;
    }

    Base::Span<const CompiledPathStep> Steps() const noexcept {
        return {steps_.Data(), steps_.Size()};
    }

    Base::Span<const CompiledPathStep> Segments() const noexcept {
        return Steps();
    }

    Base::Result<Meta::PropertyValue> Get(
        const Meta::Registry& runtime,
        const Base::Object& root) const noexcept;
    Base::Result<Meta::PropertyValue> Get(
        const Meta::Registry& runtime,
        const Meta::PropertyValue& root) const noexcept;
    Base::Result<void> Set(
        Meta::Registry& runtime,
        Base::Object& root,
        const Meta::PropertyValue& value) const noexcept;
    Base::Result<void> Set(
        Meta::Registry& runtime,
        Meta::PropertyValue& root,
        const Meta::PropertyValue& value) const noexcept;

private:
    static void ClearStepCache(CompiledPathStep& step) noexcept;
    static void PopulateStepCache(
        const Meta::Registry& runtime,
        Meta::TypeId stepOwnerType,
        const Meta::PropertyInfo* property,
        CompiledPathStep& step) noexcept;
    static Base::Result<void> EnsureStepResolved(
        const Meta::Registry& runtime,
        Meta::TypeId currentType,
        CompiledPathStep& step,
        bool forWrite) noexcept;
    static Base::Result<void> EnsureStepResolvedForRead(
        const Meta::Registry& runtime,
        Meta::TypeId currentType,
        CompiledPathStep& step) noexcept;
    static Base::Result<void> EnsureStepResolvedForWrite(
        const Meta::Registry& runtime,
        Meta::TypeId currentType,
        CompiledPathStep& step) noexcept;
    static Base::Result<Meta::PropertyValue> ValidateAndCoerceStepReadValue(
        const Meta::Registry& runtime,
        const CompiledPathStep& step,
        const Meta::PropertyValue& value) noexcept;
    static Base::Result<Meta::PropertyValue> ReadSinglePropertyStep(
        const Meta::Registry& runtime,
        const Base::Object& object,
        CompiledPathStep& step) noexcept;
    static Base::Result<void> WriteSinglePropertyStep(
        Meta::Registry& runtime,
        Base::Object& object,
        CompiledPathStep& step,
        const Meta::PropertyValue& value) noexcept;

    Base::Result<void> VerifyRuntime(
        const Meta::Registry& runtime) const noexcept;
    Base::Result<Meta::PropertyValue> GetObject(
        const Meta::Registry& runtime,
        const Base::Object& object,
        std::uint32_t stepIndex) const noexcept;
    Base::Result<Meta::PropertyValue> GetValue(
        const Meta::Registry& runtime,
        const Meta::PropertyValue& current,
        std::uint32_t stepIndex) const noexcept;
    Base::Result<void> SetObject(
        Meta::Registry& runtime,
        Base::Object& object,
        std::uint32_t stepIndex,
        const Meta::PropertyValue& value) const noexcept;
    Base::Result<bool> SetValue(
        Meta::Registry& runtime,
        Meta::PropertyValue& current,
        std::uint32_t stepIndex,
        const Meta::PropertyValue& value) const noexcept;

    mutable Base::Vector<CompiledPathStep> steps_;
    mutable const Meta::Registry* cachedRuntime_ = nullptr;
    Meta::TypeId rootType_ = Meta::InvalidTypeId;
    Meta::TypeId resultType_ = Meta::InvalidTypeId;
    Base::HashCode schemaHash_ = 0U;
    bool tokenizeAttempted_ = false;
    bool tokenized_ = false;
    bool valid_ = false;
    bool targetValidated_ = false;
    bool dynamicResult_ = false;
    bool readable_ = false;
    bool writable_ = false;
};

} // namespace Aero::Data

namespace Aero::Meta {

// class BindingPathPlan is unified with Aero::Data::CompiledBindingPath.
using BindingPathSegment = ::Aero::Data::CompiledPathStep;
using BindingPathPlan = ::Aero::Data::CompiledBindingPath;

} // namespace Aero::Meta
