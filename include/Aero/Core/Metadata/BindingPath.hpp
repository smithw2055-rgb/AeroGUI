#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>

#include <cstdint>

namespace Aero::Core {

enum class BindingPathSegmentKind : std::uint8_t {
    ObjectProperty = 0U,
    ValueField
};

struct BindingPathSegment final {
    BindingPathSegmentKind kind = BindingPathSegmentKind::ObjectProperty;
    MemberId member = InvalidMemberId;
    TypeId inputType = InvalidTypeId;
    TypeId outputType = InvalidTypeId;
    bool readable = false;
    bool writable = false;
    bool copyOnWrite = false;
};

struct BindingPathCompileError final {
    std::uint32_t segmentIndex = UINT32_MAX;
    TypeId inputType = InvalidTypeId;
    Base::String segment;
    Base::Status status;
};

// Immutable, schema-bound access plan used by Binding and compiled XAML.
// Compilation resolves every textual segment to stable descriptor IDs. Get/Set
// execute those IDs directly and never repeat member-name lookup.
class AERO_API BindingPathPlan final {
public:
    BindingPathPlan() noexcept = default;

    static Base::Result<BindingPathPlan> Compile(
        MetadataRuntime& runtime,
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
    Base::Span<const BindingPathSegment> Segments() const noexcept {
        return {segments_.Data(), segments_.Size()};
    }

    Base::Result<Value> Get(
        MetadataRuntime& runtime,
        const Base::Object& root) const noexcept;
    Base::Result<Value> Get(
        MetadataRuntime& runtime,
        const Value& root) const noexcept;
    Base::Result<void> Set(
        MetadataRuntime& runtime,
        Base::Object& root,
        const Value& value) const noexcept;
    Base::Result<void> Set(
        MetadataRuntime& runtime,
        Value& root,
        const Value& value) const noexcept;

private:
    const MetadataDomain* compiledDomain_ = nullptr;
    TypeId rootType_ = InvalidTypeId;
    TypeId resultType_ = InvalidTypeId;
    Base::HashCode schemaHash_ = 0U;
    Base::Vector<BindingPathSegment> segments_;
    bool canRead_ = false;
    bool canWrite_ = false;

    Base::Result<void> VerifyRuntime(
        MetadataRuntime& runtime) const noexcept;
    Base::Result<Value> GetObject(
        MetadataRuntime& runtime,
        const Base::Object& object,
        std::uint32_t segmentIndex) const noexcept;
    Base::Result<Value> GetValue(
        MetadataRuntime& runtime,
        const Value& value,
        std::uint32_t segmentIndex) const noexcept;
    Base::Result<void> SetObject(
        MetadataRuntime& runtime,
        Base::Object& object,
        std::uint32_t segmentIndex,
        const Value& value) const noexcept;
    Base::Result<bool> SetValue(
        MetadataRuntime& runtime,
        Value& owner,
        std::uint32_t segmentIndex,
        const Value& value) const noexcept;
};

} // namespace Aero::Core
