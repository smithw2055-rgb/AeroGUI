#include <Aero/Core/MetadataDescriptors.hpp>

namespace Aero::Core {
namespace {

template<class Key>
Base::Result<void> InsertValueFacetIndex(
    Base::HashMap<Key, std::uint32_t>& index,
    Key key,
    std::uint32_t value,
    const char* message) noexcept {
    Base::Result<typename Base::HashMap<Key, std::uint32_t>::InsertResult> inserted =
        index.TryInsert(key, value);
    if (!inserted) return inserted.GetStatus();
    if (!inserted.Value().inserted) {
        return Base::Status::Failure(Base::ErrorCode::AlreadyExists, message);
    }
    return {};
}

bool IsValueType(const MetadataTypeDescriptor& type) noexcept {
    return (static_cast<std::uint32_t>(type.Flags()) &
        static_cast<std::uint32_t>(TypeFlags::ValueType)) != 0U;
}

} // namespace

Base::Result<void> MetadataFacetStore::BuildValueFacets(
    const TypeRegistry& source,
    const MetadataDescriptorStore& descriptors) noexcept {
    if (!sealed_ || valueFacetsSealed_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            valueFacetsSealed_
                ? "Metadata value facets are already sealed"
                : "Core metadata facets must be built before value facets");
    }
    if (!source.IsFrozen() || !descriptors.IsSealed() ||
        descriptors_ != &descriptors) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Metadata value facet sources are not sealed or do not match");
    }

    std::uint32_t valueTypeCount = 0U;
    for (const MetadataTypeDescriptor& type : descriptors.Types()) {
        if (IsValueType(type)) ++valueTypeCount;
    }
    Base::Result<void> result = valueSemantics_.TryReserve(valueTypeCount);
    if (!result) return result.GetStatus();
    result = textConverters_.TryReserve(valueTypeCount);
    if (!result) return result.GetStatus();
    result = valueSemanticsIndex_.TryReserve(valueTypeCount);
    if (!result) return result.GetStatus();
    result = textConverterIndex_.TryReserve(valueTypeCount);
    if (!result) return result.GetStatus();

    for (const MetadataTypeDescriptor& type : descriptors.Types()) {
        if (!IsValueType(type)) continue;

        std::uint32_t index = valueSemantics_.Size();
        result = valueSemantics_.TryPushBack({type.Id(), &source});
        if (!result) return result.GetStatus();
        result = InsertValueFacetIndex(
            valueSemanticsIndex_, type.Id(), index,
            "Value semantics facet collision");
        if (!result) return result.GetStatus();
        result = AddTypeMask(type.Id(), MetadataFacetKind::ValueSemantics);
        if (!result) return result.GetStatus();

        index = textConverters_.Size();
        result = textConverters_.TryPushBack({type.Id(), &source});
        if (!result) return result.GetStatus();
        result = InsertValueFacetIndex(
            textConverterIndex_, type.Id(), index,
            "Text converter facet collision");
        if (!result) return result.GetStatus();
        result = AddTypeMask(type.Id(), MetadataFacetKind::TextConverter);
        if (!result) return result.GetStatus();
    }

    valueFacetsSealed_ = true;
    return {};
}

const ValueSemanticsFacet* MetadataFacetStore::FindValueSemantics(
    TypeId type) const noexcept {
    const std::uint32_t* index = valueSemanticsIndex_.Find(type);
    return index != nullptr && *index < valueSemantics_.Size()
        ? &valueSemantics_[*index] : nullptr;
}

const TextConverterFacet* MetadataFacetStore::FindTextConverter(
    TypeId type) const noexcept {
    const std::uint32_t* index = textConverterIndex_.Find(type);
    return index != nullptr && *index < textConverters_.Size()
        ? &textConverters_[*index] : nullptr;
}

} // namespace Aero::Core
