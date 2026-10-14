#pragma once

// Internal builtin-metadata helpers. Public controls do not declare
// RegisterMetadata; a type with extra metadata defines
// AERO_DESCRIBE(Type) in its cpp, and Populate walks BaseType so a derived
// type is registered after its base.

#include <Aero/ClassHandler.hpp>
#include <Aero/Meta.hpp>

namespace Aero::Meta {

template<class T>
struct DescribeHook {
    static void Run(Registration& context) noexcept {
        T::DescribeMetadata(context);
    }
};

} // namespace Aero::Meta

// Defines the private DescribeMetadata declared by AERO_DECLARE_TYPE.
// The function is a class member, so it can name protected handlers.
#define AERO_DESCRIBE(Type) \
    void Type::DescribeMetadata( \
        ::Aero::Meta::Registration& context) noexcept
