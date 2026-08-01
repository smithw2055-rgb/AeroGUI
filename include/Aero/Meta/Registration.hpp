#pragma once

#include <Aero/Base/Config.hpp>

namespace Aero::Core {

class MetaRegistry;
class DependencyPropertyRegistry;
class RegistrationTypes;
class ValueTable;
class RegistrationValues;

template<class T>
class TypeDescription;

namespace Detail {
class MetadataAuthoringSession;
}

// Opaque, callback-scoped registration context. Module authors consume it
// through Describe<T>; registration stores and runtime registries remain
// owned by MetaRegistry and are not part of the public authoring surface.
class AERO_API MetaRegistration final {
private:
    friend class MetaRegistry;
    template<class T>
    friend class TypeDescription;
    friend class Detail::MetadataAuthoringSession;

    explicit MetaRegistration(void* state) noexcept
        : state_(state) {}

    RegistrationValues Values() noexcept;
    RegistrationValues Values() const noexcept;
    RegistrationTypes Types() noexcept;
    ValueTable& ValueRegistrations() noexcept;
    DependencyPropertyRegistry& DependencyProperties() noexcept;

    void* state_ = nullptr;
};

} // namespace Aero::Core
