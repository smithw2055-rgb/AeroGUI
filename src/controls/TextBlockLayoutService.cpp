#include <Aero/Controls/TextBlockLayoutService.hpp>

#include <Aero/Base/Assert.hpp>

namespace Aero::Controls {
namespace {

thread_local ITextBlockLayoutService* CurrentService = nullptr;

} // namespace

ITextBlockLayoutService*
GetCurrentTextBlockLayoutService() noexcept {
    return CurrentService;
}

TextBlockLayoutServiceScope::TextBlockLayoutServiceScope(
    ITextBlockLayoutService& service) noexcept
    : service_(&service),
      previous_(CurrentService),
      ownerThread_(Core::CurrentDispatcherThreadToken()) {
    CurrentService = service_;
}

TextBlockLayoutServiceScope::~TextBlockLayoutServiceScope() {
    AERO_ASSERT(
        ownerThread_ == Core::CurrentDispatcherThreadToken());
    AERO_ASSERT(CurrentService == service_);
    CurrentService = previous_;
}

} // namespace Aero::Controls
