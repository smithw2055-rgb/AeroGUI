#include <Aero/Data/Binding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Layout.hpp>

namespace Aero::Data {

Base::Result<Value> BooleanToVisibilityConverter::Convert(
    const Value& value,
    const Value& parameter) noexcept {
    (void)parameter;
    Base::Result<bool> converted =
        Meta::ValueCodec<bool>::Decode(value);
    if (!converted) return converted.GetStatus();
    return Meta::ValueCodec<Aero::Visibility>::Encode(
        converted.Value()
            ? Aero::Visibility::Visible
            : Aero::Visibility::Collapsed);
}

Base::Result<Value> BooleanToVisibilityConverter::ConvertBack(
    const Value& value,
    const Value& parameter) noexcept {
    (void)parameter;
    Base::Result<Aero::Visibility> converted =
        Meta::ValueCodec<Aero::Visibility>::Decode(value);
    if (!converted) return converted.GetStatus();
    return Meta::ValueCodec<bool>::Encode(
        converted.Value() == Aero::Visibility::Visible);
}

Base::Ref<RelativeSource> RelativeSource::ForSelf() noexcept {
    Base::Result<Base::Ref<RelativeSource>> source = Base::MakeRef<RelativeSource>(RelativeSourceMode::Self);
    return source ? std::move(source).Value() : Base::Ref<RelativeSource>{};
}

Base::Ref<RelativeSource> RelativeSource::ForTemplatedParent() noexcept {
    Base::Result<Base::Ref<RelativeSource>> source = Base::MakeRef<RelativeSource>(RelativeSourceMode::TemplatedParent);
    return source ? std::move(source).Value() : Base::Ref<RelativeSource>{};
}

} // namespace Aero::Data
