#include "MetaInternals.hpp"
#include "ValueTable.hpp"

#include <utility>

namespace Aero::Core::Detail {
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

bool IsValueType(const TypeInfo& type) noexcept {
    return (static_cast<std::uint32_t>(type.Flags()) &
        static_cast<std::uint32_t>(TypeFlags::ValueType)) != 0U;
}

Base::Status ValueFacetStateError(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

} // namespace

Base::Result<void> MetaTable::BuildValueFacets(
    const ValueTable& source,
    const TypeRegistry& types) noexcept {
    if (!sealed_ || valueFacetsSealed_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            valueFacetsSealed_
                ? "Metadata value facets are already sealed"
                : "Core metadata facets must be built before value facets");
    }
    if (!source.IsFrozen() || !types.IsFrozen() ||
        types_ != &types) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Metadata value facet sources are not sealed or do not match");
    }

    std::uint32_t valueTypeCount = 0U;
    for (const TypeInfo& type : types.Types()) {
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

    for (const TypeInfo& type : types.Types()) {
        if (IsValueType(type)) {
            const Base::Ref<ValueTypeSemantics>* semantics =
                source.FindValueSemantics(type.Id());
            if (semantics != nullptr &&
                semantics->Get() != nullptr) {
                ValueSemanticsFacet facet;
                facet.type = type.Id();
                facet.semantics = *semantics;
                const std::uint32_t index =
                    valueSemantics_.Size();
                result = valueSemantics_.TryPushBack(
                    std::move(facet));
                if (!result) return result.GetStatus();
                result = InsertValueFacetIndex(
                    valueSemanticsIndex_, type.Id(), index,
                    "Value semantics facet collision");
                if (!result) return result.GetStatus();
                result = AddTypeMask(
                    type.Id(),
                    MetadataFacetKind::ValueSemantics);
                if (!result) return result.GetStatus();
            }
        }

        const TextValueConverterRegistration* converter =
            source.FindTextConverter(type.Id());
        if (converter != nullptr && converter->convert != nullptr) {
            TextConverterFacet facet;
            facet.type = type.Id();
            facet.convert = converter->convert;
            facet.context = converter->context;
            const std::uint32_t index = textConverters_.Size();
            result = textConverters_.TryPushBack(facet);
            if (!result) return result.GetStatus();
            result = InsertValueFacetIndex(
                textConverterIndex_, type.Id(), index,
                "Text converter facet collision");
            if (!result) return result.GetStatus();
            result = AddTypeMask(type.Id(), MetadataFacetKind::TextConverter);
            if (!result) return result.GetStatus();
        }
    }

    valueFacetsSealed_ = true;
    return {};
}

const ValueSemanticsFacet* MetaTable::FindValueSemantics(
    TypeId type) const noexcept {
    const std::uint32_t* index = valueSemanticsIndex_.Find(type);
    return index != nullptr && *index < valueSemantics_.Size()
        ? &valueSemantics_[*index] : nullptr;
}

const TextConverterFacet* MetaTable::FindTextConverter(
    TypeId type) const noexcept {
    const std::uint32_t* index = textConverterIndex_.Find(type);
    return index != nullptr && *index < textConverters_.Size()
        ? &textConverters_[*index] : nullptr;
}

Base::Result<Base::HashCode> ComputeMetadataValueFacetHash(
    const MetaTable& facets,
    const TypeRegistry& descriptors) noexcept {
    if (!facets.IsSealed() || !facets.ValueFacetsSealed() ||
        !descriptors.IsFrozen()) {
        return ValueFacetStateError(
            "Value facet hash requires sealed descriptors and facets");
    }

    Base::Detail::StableMetadataIdBuilder builder;
    constexpr char domain[] = "AERO.VALUE.FACETS.V1";
    builder.AddText(domain, static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddU32(MetadataFacetFormatVersion);

    std::uint32_t semanticsCount = 0U;
    std::uint32_t converterCount = 0U;
    for (const TypeInfo& type : descriptors.Types()) {
        if (facets.FindValueSemantics(type.Id()) != nullptr) ++semanticsCount;
        if (facets.FindTextConverter(type.Id()) != nullptr) ++converterCount;
    }
    builder.AddU32(semanticsCount);
    for (const TypeInfo& type : descriptors.Types()) {
        const ValueSemanticsFacet* facet =
            facets.FindValueSemantics(type.Id());
        if (facet == nullptr || !facet->semantics) continue;
        const ValueTypeRegistration& registration =
            facet->semantics->Registration();
        builder.AddU64(type.Id());
        builder.AddU32(registration.size);
        builder.AddU32(registration.alignment);
        builder.AddByte(registration.copy != nullptr ? 1U : 0U);
        builder.AddByte(registration.destroy != nullptr ? 1U : 0U);
        builder.AddByte(registration.equals != nullptr ? 1U : 0U);
        builder.AddByte(registration.inlineSafe ? 1U : 0U);
    }

    builder.AddU32(converterCount);
    for (const TypeInfo& type : descriptors.Types()) {
        const TextConverterFacet* facet =
            facets.FindTextConverter(type.Id());
        if (facet == nullptr) continue;
        builder.AddU64(type.Id());
        builder.AddByte(facet->convert != nullptr ? 1U : 0U);
    }
    return builder.Finish();
}

} // namespace Aero::Core::Detail
