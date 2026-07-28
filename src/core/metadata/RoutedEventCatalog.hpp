#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Core/RoutedEvent.hpp>

namespace Aero::Core {

struct RoutedEventRegistration final {
    Base::StringView name;
    TypeId ownerType = InvalidTypeId;
    TypeId eventArgsType = InvalidTypeId;
    RoutingStrategy strategy = RoutingStrategy::Bubble;
};

class AERO_API RoutedEventCatalog final {
public:
    struct Definition final {
        RoutedEventHandle handle;
        TypeId ownerType = InvalidTypeId;
        TypeId eventArgsType = InvalidTypeId;
        RoutingStrategy strategy = RoutingStrategy::Bubble;
        Base::String name;

        Definition() noexcept : name() {}
    };

    RoutedEventCatalog(
        TypeRegistry& types,
        MetadataBehaviorRegistrationStore& behaviors) noexcept;

    RoutedEventCatalog(const RoutedEventCatalog&) = delete;
    RoutedEventCatalog& operator=(const RoutedEventCatalog&) = delete;

    Base::Result<RoutedEventHandle> TryRegister(
        const RoutedEventRegistration& registration) noexcept;
    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    const TypeRegistry& Types() const noexcept { return *types_; }
    const Definition* Find(RoutedEventHandle event) const noexcept;

private:
    TypeRegistry* types_ = nullptr;
    MetadataBehaviorRegistrationStore* behaviorRegistrations_ = nullptr;
    Base::Vector<Definition> definitions_;
    bool frozen_ = false;
};

} // namespace Aero::Core
