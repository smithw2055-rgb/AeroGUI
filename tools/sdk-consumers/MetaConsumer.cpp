#include <Aero/Gui.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Module.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Button.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Media/Animation.hpp>

#include <cstdint>
#include <type_traits>
#include <utility>

namespace SdkConsumer {

struct ViewModel {
    bool active = false;
};

enum class Theme : std::uint8_t {
    Light = 0U,
    Dark
};

} // namespace SdkConsumer

AERO_DECLARE_TYPE_ENUM(SdkConsumer::Theme)

namespace Aero::Meta {

template<>
struct TypeTraits<SdkConsumer::ViewModel> {
    static constexpr Meta::TypeId Id() noexcept {
        return Meta::MakeTypeId(
            "urn:aero-sdk-consumer", "ViewModel");
    }
    static constexpr StringView Namespace() noexcept {
        return "urn:aero-sdk-consumer";
    }
    static constexpr StringView Name() noexcept {
        return "ViewModel";
    }
    static constexpr Meta::TypeId BaseType() noexcept {
        return Meta::InvalidTypeId;
    }
};

} // namespace Aero::Meta

namespace {

template<class T, class = void>
struct IsComplete : std::false_type {};

template<class T>
struct IsComplete<T, std::void_t<decltype(sizeof(T))>>
    : std::true_type {};

template<class T, class = void>
struct HasPropertyRegistry : std::false_type {};

template<class T>
struct HasPropertyRegistry<T, std::void_t<decltype(
    std::declval<T&>().PropertyRegistry())>>
    : std::true_type {};

using RawFactory = Aero::Result<
    Aero::Ref<Aero::Object>> (*)() noexcept;

template<class T, class = void>
struct HasRawFactoryOverload : std::false_type {};

template<class T>
struct HasRawFactoryOverload<T, std::void_t<decltype(
    std::declval<Aero::Meta::TypeBuilder<T>&>().Factory(
        static_cast<RawFactory>(nullptr)))>>
    : std::true_type {};

class ConsumerControl : public Aero::Controls::Control {
    AERO_DECLARE_TYPE_NAMED(
        ConsumerControl,
        Aero::Controls::Control,
        "urn:aero-sdk-consumer",
        "ConsumerControl")

public:
    ConsumerControl() noexcept
        : Control(StaticTypeId()) {}

    inline static constexpr DependencyProperty<bool> ActiveProperty{"Active"};
    inline static constexpr RoutedEvent<Aero::RoutedEventArgs> ActivatedEvent{"Activated"};

protected:
    void OnRender(
        Aero::Media::DrawingContext& context) noexcept override {
        static_cast<void>(context.DrawRectangle(
            {0.0, 0.0, GetRenderSize().width, GetRenderSize().height},
            {0.0F, 0.0F, 0.0F, 0.0F}));
    }
};

class ConsumerButton : public Aero::Controls::Button {
    AERO_DECLARE_TYPE_NAMED(
        ConsumerButton,
        Aero::Controls::Button,
        "urn:aero-sdk-consumer",
        "ConsumerButton")

public:
    ConsumerButton() noexcept
        : Button(StaticTypeId()) {}
};

Aero::Result<void> RegisterConsumerModule(
    Aero::Meta::Registration& context) noexcept {
    Aero::Meta::TypeBuilder<SdkConsumer::Theme> theme =
        Aero::Meta::Register<SdkConsumer::Theme>(
            context, "urn:aero-sdk-consumer", "Theme");
    theme
        .Value("Light", SdkConsumer::Theme::Light)
        .Value("Dark", SdkConsumer::Theme::Dark);
    Aero::Result<void> status =
        theme.Result();
    if (!status) return status.GetStatus();
    if (Aero::Meta::TypeOf<SdkConsumer::Theme>() !=
            Aero::Meta::MakeTypeId(
                "urn:aero-sdk-consumer", "Theme") ||
        Aero::Meta::TypeTraits<SdkConsumer::Theme>::Namespace() !=
            Aero::StringView("urn:aero-sdk-consumer") ||
        Aero::Meta::TypeTraits<SdkConsumer::Theme>::Name() !=
            Aero::StringView("Theme") ||
        Aero::Meta::ValueCodec<SdkConsumer::Theme>::Type() !=
            Aero::Meta::TypeOf<SdkConsumer::Theme>()) {
        return Aero::Base::Status::Failure(
            Aero::Base::ErrorCode::InvalidState,
            "Runtime enum metadata binding is inconsistent");
    }
    status = Aero::Meta::Register<SdkConsumer::ViewModel>(context).Result();
    if (!status) return status.GetStatus();

    status =
        Aero::Meta::Register<ConsumerControl>(context)
            .Property(
                ConsumerControl::ActiveProperty,
                Aero::Meta::FrameworkPropertyMetadata(
                    false,
                    Aero::Meta::AffectsRender))
            .Event(
                ConsumerControl::ActivatedEvent,
                Aero::RoutingStrategy::Bubble)
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
    !IsComplete<Aero::Meta::DependencyPropertyRegistry>::value,
    "Mutable dependency-property registry storage must remain SDK-opaque");

static_assert(
    !HasPropertyRegistry<Aero::DependencyObject>::value,
    "DependencyObject must not expose its runtime registry to SDK consumers");

static_assert(
    !std::is_constructible<
        Aero::Meta::MetadataAuthoringSession,
        Aero::Meta::Registration&,
        const Aero::Meta::TypeRegistration&,
        Aero::Meta::TypeId>::value,
    "The builder core must not be directly constructible by SDK consumers");

static_assert(
    !HasRawFactoryOverload<ConsumerControl>::value,
    "Custom controls must use the typed Factory() authoring operation");

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
        Aero::Threading::Dispatcher>::value,
    "Threading projection must preserve dispatcher identity");


} // namespace
