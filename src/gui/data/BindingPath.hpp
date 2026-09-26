#pragma once

// Immutable property-path plans used by binding expressions and compiled XAML.
#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero::Meta {

enum class BindingPathSegmentKind : std::uint8_t {
    ObjectProperty = 0U,
    ValueField,
    CollectionIndex
};

struct BindingPathSegment {
    BindingPathSegmentKind kind = BindingPathSegmentKind::ObjectProperty;
    MemberId member = InvalidMemberId;
    TypeId inputType = InvalidTypeId;
    TypeId outputType = InvalidTypeId;
    Base::String dynamicName;
    std::uint32_t collectionIndex = UINT32_MAX;
    bool readable = false;
    bool writable = false;
    bool copyOnWrite = false;
    bool dynamic = false;
};

struct BindingPathCompileError {
    std::uint32_t segmentIndex = UINT32_MAX;
    TypeId inputType = InvalidTypeId;
    Base::String segment;
    Base::Status status;
};

// Compilation resolves every textual segment to stable descriptor IDs. Get/Set
// execute those IDs directly and never repeat member-name lookup.
class BindingPathPlan {
public:
    BindingPathPlan() noexcept = default;

    static Base::Result<BindingPathPlan> Compile(
        Meta::Registry& runtime,
        TypeId rootType,
        Base::StringView path,
        BindingPathCompileError* error = nullptr) noexcept;

    bool IsValid() const noexcept {
        return rootType_ != InvalidTypeId &&
            resultType_ != InvalidTypeId &&
            schemaHash_ != 0U &&
            !segments_.Empty();
    }
    TypeId RootType() const noexcept { return rootType_; }
    TypeId ResultType() const noexcept { return resultType_; }
    Base::HashCode SchemaHash() const noexcept { return schemaHash_; }
    bool CanRead() const noexcept { return canRead_; }
    bool CanWrite() const noexcept { return canWrite_; }
    bool HasDynamicResult() const noexcept { return hasDynamicResult_; }
    Base::Span<const BindingPathSegment> Segments() const noexcept {
        return {segments_.Data(), segments_.Size()};
    }

    Base::Result<Value> Get(
        Meta::Registry& runtime,
        const Base::Object& root) const noexcept;
    Base::Result<Value> Get(
        Meta::Registry& runtime,
        const Value& root) const noexcept;
    Base::Result<void> Set(
        Meta::Registry& runtime,
        Base::Object& root,
        const Value& value) const noexcept;
    Base::Result<void> Set(
        Meta::Registry& runtime,
        Value& root,
        const Value& value) const noexcept;

private:
    const Meta::Registry* compiledDomain_ = nullptr;
    TypeId rootType_ = InvalidTypeId;
    TypeId resultType_ = InvalidTypeId;
    Base::HashCode schemaHash_ = 0U;
    Base::Vector<BindingPathSegment> segments_;
    bool canRead_ = false;
    bool canWrite_ = false;
    bool hasDynamicResult_ = false;

    Base::Result<void> VerifyRuntime(
        Meta::Registry& runtime) const noexcept;
    Base::Result<Value> GetObject(
        Meta::Registry& runtime,
        const Base::Object& object,
        std::uint32_t segmentIndex) const noexcept;
    Base::Result<Value> GetValue(
        Meta::Registry& runtime,
        const Value& value,
        std::uint32_t segmentIndex) const noexcept;
    Base::Result<void> SetObject(
        Meta::Registry& runtime,
        Base::Object& object,
        std::uint32_t segmentIndex,
        const Value& value) const noexcept;
    Base::Result<bool> SetValue(
        Meta::Registry& runtime,
        Value& owner,
        std::uint32_t segmentIndex,
        const Value& value) const noexcept;
};

} // namespace Aero::Meta
