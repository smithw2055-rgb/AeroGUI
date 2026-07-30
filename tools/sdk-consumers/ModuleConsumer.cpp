#include <Aero/ModuleSdk.hpp>
#include <Aero/Controls/Primitives.hpp>
#include <Aero/Data.hpp>
#include <Aero/Media.hpp>

#include <type_traits>

namespace {

class ConsumerControl final : public Aero::Controls::Control {
    AERO_DECLARE_TYPE_NAMED(
        ConsumerControl,
        Aero::Controls::Control,
        "urn:aero-sdk-consumer",
        "ConsumerControl")

public:
    ConsumerControl() noexcept
        : Control(StaticTypeId()) {}

    inline static constexpr Members::Property<bool>
        ActiveProperty{"Active"};
    inline static constexpr Members::RoutedEvent<
        Aero::Presentation::RoutedEventArgs>
        ActivatedEvent{"Activated"};
};

Aero::Base::Result<void> RegisterConsumerModule(
    Aero::MetadataContext& context) noexcept {
    return Aero::Describe<ConsumerControl>(context)
        .Property(
            ConsumerControl::ActiveProperty,
            Aero::PropertyOptions(false)
                .AffectsRender())
        .Event(
            ConsumerControl::ActivatedEvent,
            Aero::Core::RoutingStrategy::Bubble)
        .Factory()
        .Result();
}

constexpr Aero::ModuleRegistration ConsumerModule =
    Aero::DefineModule(
        "Aero.SdkConsumer",
        &RegisterConsumerModule);

static_assert(
    std::is_same<
        decltype(ConsumerModule.registerModule),
        Aero::ModuleRegisterCallback>::value,
    "Module SDK must expose the typed registration callback");

static_assert(
    std::is_same<
        Aero::Controls::Primitives::ButtonBase,
        Aero::Controls::ButtonBase>::value,
    "Controls.Primitives projection must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::Data::Binding,
        Aero::Presentation::BindingSpec>::value,
    "Data binding projection must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::Media::Brush,
        Aero::Presentation::Brush>::value,
    "Media projection must preserve brush runtime type identity");

static_assert(
    std::is_same<
        Aero::Media::Animation::Timeline,
        Aero::Animation::Timeline>::value,
    "Media.Animation projection must preserve timeline runtime type identity");

} // namespace
