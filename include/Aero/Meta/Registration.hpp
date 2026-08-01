#pragma once

#include <Aero/Base/Config.hpp>

namespace Aero::Core {

class DependencyPropertyRegistry;
class RegistrationTypes;
class RegistrationValues;
class ValueTable;
template<class T>
class TypeDescription;

namespace Detail {
class MetadataAuthoringSession;
}

} // namespace Aero::Core

namespace Aero::Meta {

class Registry;

// Callback-scoped metadata authoring session. Module authors use Describe<T>
// against this object; mutable tables and registration storage stay private to
// Registry.
class AERO_API Registration final {
private:
    friend class Registry;
    template<class T>
    friend class Core::TypeDescription;
    friend class Core::Detail::MetadataAuthoringSession;

    explicit Registration(void* state) noexcept
        : state_(state) {}

    Core::RegistrationValues Values() noexcept;
    Core::RegistrationValues Values() const noexcept;
    Core::RegistrationTypes Types() noexcept;
    Core::ValueTable& ValueRegistrations() noexcept;
    Core::DependencyPropertyRegistry& DependencyProperties() noexcept;

    void* state_ = nullptr;
};

} // namespace Aero::Meta
