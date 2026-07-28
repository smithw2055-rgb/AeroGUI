#include <Aero/Runtime.hpp>

namespace {

[[maybe_unused]] void ConsumeProductSdk(
    Aero::RuntimeEnvironment& environment) {
    Aero::Base::Result<Aero::Base::Ref<Aero::View>> view =
        environment.CreateView();
    if (view) {
        static_cast<void>(view.Value()->RunFrame());
    }
}

} // namespace
