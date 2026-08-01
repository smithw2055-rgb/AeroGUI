#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Meta/TypeRegistry.hpp>

namespace Aero::Core {

namespace Detail {
class MetaTable;
}
class RegistrationValues;

// Mutable registration storage for custom value semantics and text converters.
//
// The store is owned beside TypeRegistry by MetaRegistry. It validates type
// identities through the structural registry, but does not make executable
// value behavior part of TypeRegistry's ownership or public API.
class AERO_API ValueTable final {
public:
    explicit ValueTable(TypeRegistry& types) noexcept
        : types_(&types) {}

    ValueTable(const ValueTable&) = delete;
    ValueTable& operator=(
        const ValueTable&) = delete;
    ValueTable(ValueTable&&) = delete;
    ValueTable& operator=(
        ValueTable&&) = delete;

    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    const TypeRegistry& Types() const noexcept { return *types_; }

private:
    friend class Detail::MetaTable;
    friend class RegistrationValues;

    struct ValueSemanticsEntry final {
        TypeId type = InvalidTypeId;
        Base::Ref<ValueTypeSemantics> semantics;
    };

    Base::Result<void> TryRegisterValueSemantics(
        TypeId type,
        const ValueTypeRegistration& registration) noexcept;
    Base::Result<void> TryRegisterTextConverter(
        const TextValueConverterRegistration& registration) noexcept;
    const Base::Ref<ValueTypeSemantics>* FindValueSemantics(
        TypeId type) const noexcept;
    const TextValueConverterRegistration* FindTextConverter(
        TypeId type) const noexcept;

    TypeRegistry* types_ = nullptr;
    Base::Vector<ValueSemanticsEntry> valueSemantics_;
    Base::Vector<TextValueConverterRegistration> textConverters_;
    bool frozen_ = false;
};

} // namespace Aero::Core
