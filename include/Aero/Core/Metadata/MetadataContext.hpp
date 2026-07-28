#pragma once

#include <Aero/Base/Config.hpp>

namespace Aero::Core {

class MetadataDomain;
#if !defined(AERO_SDK_SURFACE_ONLY)
class DependencyPropertyRegistry;
class MetadataRegistrationTypes;
class MetadataValueRegistrationStore;
#endif
class MetadataRegistrationValues;

template<class T>
class TypeDescription;

namespace Detail {
class MetadataAuthoringSession;
}

// Opaque, callback-scoped registration context. Module authors consume it
// through Describe<T>; registration stores and runtime registries remain
// owned by MetadataDomain and are not part of the Module SDK surface.
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
#if !defined(AERO_SDK_SURFACE_ONLY)
    MetadataRegistrationTypes Types() noexcept;
    MetadataValueRegistrationStore& ValueRegistrations() noexcept;
    DependencyPropertyRegistry& DependencyProperties() noexcept;
#endif

    void* state_ = nullptr;
};

} // namespace Aero::Core
