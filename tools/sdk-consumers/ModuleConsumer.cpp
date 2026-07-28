#include <Aero/ModuleSdk.hpp>

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
    ConsumerModule.registerModule != nullptr,
    "Module SDK must author typed custom controls");

} // namespace
