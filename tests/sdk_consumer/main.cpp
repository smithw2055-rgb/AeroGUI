#include <Aero/RuntimeEnvironment.hpp>
#include <Aero/Version.hpp>

int main() {
    static_assert(Aero::VersionMajor == 0U, "unexpected Aero major version");
    static_assert(Aero::VersionMinor == 3U, "unexpected Aero minor version");
    static_assert(Aero::ModuleAbiVersion == 3U, "unexpected module ABI");
    static_assert(Aero::XamlSchemaAbiVersion == 9U, "unexpected schema ABI");

    Aero::RuntimeEnvironment environment;
    if (!environment.Initialize()) return 1;
    auto view = environment.CreateView();
    if (!view) return 2;
    view.Value()->Shutdown();
    return 0;
}
