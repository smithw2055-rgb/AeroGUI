#include <Aero/Data.hpp>

namespace Aero::Data {

Base::Ref<RelativeSource> RelativeSource::ForSelf() noexcept {
    Base::Result<Base::Ref<RelativeSource>> source = Base::MakeRef<RelativeSource>(RelativeSourceMode::Self);
    return source ? std::move(source).Value() : Base::Ref<RelativeSource>{};
}

Base::Ref<RelativeSource> RelativeSource::ForTemplatedParent() noexcept {
    Base::Result<Base::Ref<RelativeSource>> source = Base::MakeRef<RelativeSource>(RelativeSourceMode::TemplatedParent);
    return source ? std::move(source).Value() : Base::Ref<RelativeSource>{};
}

} // namespace Aero::Data
