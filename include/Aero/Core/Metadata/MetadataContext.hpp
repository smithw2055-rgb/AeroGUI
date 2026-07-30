#pragma once

#include <Aero/Base/Config.hpp>

namespace Aero::Core {

class MetadataDomain;
class DependencyPropertyRegistry;
class MetadataRegistrationTypes;
class MetadataValueRegistrationStore;
class MetadataRegistrationValues;

template<class T>
class TypeDescription;

namespace Detail {
class MetadataAuthoringSession;
}

// Opaque, callback-scoped registration context. Module authors consume it
// through Describe<T>; registration stores and runtime registries remain
// owned by MetadataDomain and are not part of the public authoring surface.
class AERO_API MetadataContext final {
private:
    friend class MetadataDomain;
    template<class T>
    friend class TypeDescription;
    friend class Detail::MetadataAuthoringSession;

    explicit MetadataContext(void* state) noexcept
        : state_(state) {}

    MetadataRegistrationValues Values() noexcept;
    MetadataRegistrationValues Values() const noexcept;
    MetadataRegistrationTypes Types() noexcept;
    MetadataValueRegistrationStore& ValueRegistrations() noexcept;
    DependencyPropertyRegistry& DependencyProperties() noexcept;

    void* state_ = nullptr;
};

} // namespace Aero::Core
