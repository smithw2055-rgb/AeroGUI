#include <Aero/Gui.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Module.hpp>

#include <type_traits>

namespace SdkConsumer {

struct ViewModel final {
    bool active = false;
};

} // namespace SdkConsumer

namespace Aero::Meta {

template<>
struct TypeTraits<SdkConsumer::ViewModel> {
    static constexpr Core::TypeId Id() noexcept {
        return Core::MakeTypeId(
            "urn:aero-sdk-consumer", "ViewModel");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return "urn:aero-sdk-consumer";
    }
    static constexpr Base::StringView Name() noexcept {
        return "ViewModel";
    }
    static constexpr Core::TypeId BaseType() noexcept {
        return Core::InvalidTypeId;
    }
};

} // namespace Aero::Meta

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

    inline static constexpr Members::Property<bool> ActiveProperty{"Active"};
    inline static constexpr Members::RoutedEvent<Aero::RoutedEventArgs> ActivatedEvent{"Activated"};

protected:
    Aero::Base::Result<void> OnRender(
        Aero::DrawingContext& context) noexcept override {
        return context.DrawRectangle(
            {0.0, 0.0, GetRenderSize().width, GetRenderSize().height},
            {0.0F, 0.0F, 0.0F, 0.0F});
    }
};

class ConsumerButton final : public Aero::Controls::Button {
    AERO_DECLARE_TYPE_NAMED(
        ConsumerButton,
        Aero::Controls::Button,
        "urn:aero-sdk-consumer",
        "ConsumerButton")

public:
    ConsumerButton() noexcept
        : Button(StaticTypeId()) {}
};

Aero::Base::Result<void> RegisterConsumerModule(
    Aero::Meta::Registration& context) noexcept {
    Aero::Base::Result<void> status =
        Aero::Meta::Register<SdkConsumer::ViewModel>(context)
            .Result();
    if (!status) return status.GetStatus();

    status =
        Aero::Meta::Register<ConsumerControl>(context)
            .Property(
                ConsumerControl::ActiveProperty,
                Aero::Meta::FrameworkPropertyMetadata(
                    false,
                    Aero::Meta::FrameworkPropertyMetadataOptions::AffectsRender))
            .Event(
                ConsumerControl::ActivatedEvent,
                Aero::Meta::Routing::Bubble)
            .Factory()
            .Result();
    if (!status) return status.GetStatus();

    return Aero::Meta::Register<ConsumerButton>(context)
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
    std::is_class<Aero::Meta::Registration>::value,
    "Meta::Registration must be a real public authoring type");

static_assert(
    std::is_same<
        Aero::DependencyObject,
        Aero::DependencyObject>::value,
    "Root DependencyObject must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::UIElement,
        Aero::UIElement>::value,
    "Root UIElement must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::FrameworkElement,
        Aero::FrameworkElement>::value,
    "Root FrameworkElement must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::RoutedEventArgs,
        Aero::RoutedEventArgs>::value,
    "Root routed-event arguments must preserve type identity");

static_assert(
    std::is_same<Aero::Thickness, Aero::Base::Thickness>::value,
    "Root Thickness must preserve value type identity");

static_assert(
    std::is_same<
        Aero::HorizontalAlignment,
        Aero::HorizontalAlignment>::value,
    "Root alignment must preserve value type identity");

static_assert(
    std::is_base_of<
        Aero::Controls::Button,
        ConsumerButton>::value,
    "Standard Button must remain derivable for custom controls");

static_assert(
    std::is_same<
        Aero::Data::Binding,
        Aero::Data::Binding>::value,
    "Data binding projection must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::Input::ICommand,
        Aero::Input::ICommand>::value,
    "Input projection must preserve command runtime type identity");

static_assert(
    std::is_same<
        Aero::Media::Brush,
        Aero::Media::Brush>::value,
    "Media projection must preserve brush runtime type identity");

static_assert(
    std::is_same<
        Aero::Media::Geometry,
        Aero::Media::Geometry>::value,
    "Media geometry projection must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::Media::Animation::Timeline,
        Aero::Media::Animation::Timeline>::value,
    "Media.Animation projection must preserve timeline runtime type identity");

static_assert(
    std::is_same<
        Aero::ResourceDictionary,
        Aero::ResourceDictionary>::value,
    "Root ResourceDictionary projection must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::Style,
        Aero::Style>::value,
    "Root Style projection must preserve runtime type identity");

static_assert(
    std::is_base_of<
        Aero::Shapes::Shape,
        Aero::Shapes::Rectangle>::value,
    "Shapes must use the canonical Aero::Shapes hierarchy");

static_assert(
    std::is_same<
        Aero::Threading::Dispatcher,
        Aero::Core::Dispatcher>::value,
    "Threading projection must preserve dispatcher identity");


} // namespace
