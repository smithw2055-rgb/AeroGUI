#include <Aero/Controls/Metadata.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>

#include <cstdio>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

bool TestCoreOnlyDomain() {
    MetadataDomain domain;
    CHECK(domain.IsValid());
    CHECK(TryRegisterCoreMetadata(domain));
    CHECK(domain.ModuleCount() == 1U);
    CHECK(domain.Types().FindType(BuiltinTypes::Object) != nullptr);
    CHECK(domain.Types().FindType(BuiltinTypes::DependencyObject) != nullptr);
    CHECK(domain.Types().FindType(BuiltinTypes::Visual) == nullptr);
    CHECK(domain.Types().FindType(BuiltinTypes::StackPanel) == nullptr);
    CHECK(domain.Seal());
    return true;
}

bool TestModuleOrderAndDuplicateRegistration() {
    MetadataDomain outOfOrder;
    Result<void> presentation =
        Aero::Presentation::TryRegisterPresentationMetadata(outOfOrder);
    CHECK(!presentation);
    CHECK(outOfOrder.ModuleCount() == 0U);
    CHECK(outOfOrder.Types().TypeCount() == 0U);

    MetadataDomain domain;
    CHECK(Aero::Controls::TryRegisterBuiltInUiMetadata(domain));
    CHECK(domain.ModuleCount() == 3U);
    CHECK(domain.Types().FindType(BuiltinTypes::Visual) != nullptr);
    CHECK(domain.Types().FindType(BuiltinTypes::StackPanel) != nullptr);

    Result<void> duplicate = TryRegisterCoreMetadata(domain);
    CHECK(!duplicate);
    CHECK(duplicate.GetStatus().code == ErrorCode::AlreadyExists);
    CHECK(domain.ModuleCount() == 3U);
    CHECK(domain.Seal());
    return true;
}

} // namespace

int main() {
    return TestCoreOnlyDomain() &&
        TestModuleOrderAndDuplicateRegistration() ? 0 : 1;
}
