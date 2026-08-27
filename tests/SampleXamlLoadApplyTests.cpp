#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Controls/Image.hpp>
#include <Aero/Controls/Label.hpp>
#include <Aero/Controls/ProgressBar.hpp>
#include <Aero/Controls/ScrollViewer.hpp>
#include <Aero/Controls/Viewbox.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Gui.hpp>
#include <Aero/Input.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/LogicalTreeHelper.hpp>
#include <Aero/Markup/XamlDocument.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Media/Animation/DoubleAnimationBase.hpp>
#include <Aero/Media/Animation/Duration.hpp>
#include <Aero/Media/BrushShader.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Module.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Style.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Value.hpp>
#include <Aero/View.hpp>
#include <Aero/ViewOptions.hpp>
#include <Aero/VisualTreeHelper.hpp>
#include <AeroApp/App.hpp>
#include <AeroApp/Application.hpp>
#include <AeroApp/Window.hpp>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include "TutorialSampleXaml.inc"

using Aero::Base::ErrorCode;
using Aero::Base::Ref;
using Aero::Base::Result;
using Aero::Base::String;
using Aero::Base::StringView;
namespace Base = Aero::Base;
using Aero::Controls::ContentControl;
using Aero::Controls::Control;
using Aero::Controls::Label;
using Aero::Controls::ProgressBar;
using Aero::Controls::ScrollViewer;
using Aero::Controls::UserControl;
using Aero::Data::IMultiValueConverter;
using Aero::Data::IValueConverter;
using Aero::FrameworkElement;
using Aero::Gui;
using Aero::Media::VisualTreeHelper;
using Aero::TryCast;
using Aero::View;
using Aero::ViewOptions;
using Aero::ViewViewport;
using Aero::Window;

#define SAMPLE_CHECK(expression)                                              \
    do {                                                                      \
        if (!(expression)) {                                                  \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n",               \
                __FILE__, __LINE__, #expression);                             \
            return false;                                                     \
        }                                                                     \
    } while (false)

namespace {

bool Contains(StringView haystack, StringView needle) noexcept {
    if (needle.Empty() || needle.SizeBytes() > haystack.SizeBytes()) {
        return false;
    }
    const std::uint32_t last =
        haystack.SizeBytes() - needle.SizeBytes();
    for (std::uint32_t index = 0U; index <= last; ++index) {
        if (std::memcmp(
                haystack.Data() + index,
                needle.Data(),
                needle.SizeBytes()) == 0) {
            return true;
        }
    }
    return false;
}

StringView CStringView(const char* text) noexcept {
    if (text == nullptr || text[0] == '\0') {
        return {};
    }
    return StringView(text, static_cast<std::uint32_t>(std::strlen(text)));
}

StringView AsView(const std::string& text) noexcept {
    return StringView(
        text.data(),
        static_cast<std::uint32_t>(text.size()));
}

bool ReportsSampleLoadFailure(StringView message) noexcept {
    constexpr StringView needles[] = {
        StringView("TemplatedParent Binding target name was not found"),
        StringView("XAML target does not support dependency properties"),
        StringView("XAML markup-extension value provider failed"),
        StringView("ResourceDictionary Source could not be loaded"),
        StringView("Binding TemplatedParent is unavailable"),
        StringView("TemplateBinding target"),
        StringView("Template trigger source name was not found")};
    for (const StringView needle : needles) {
        if (Contains(message, needle)) {
            return true;
        }
    }
    return false;
}

void DumpDiagnostics(const Aero::Diagnostics::DiagnosticBag& bag) noexcept {
    for (std::uint32_t index = 0U; index < bag.Size(); ++index) {
        const Aero::Diagnostics::Diagnostic& item = bag.Items()[index];
        const StringView message = item.Message();
        std::fprintf(stderr, "diagnostic: %.*s",
            static_cast<int>(message.SizeBytes()),
            message.Data());
        const Aero::Diagnostics::SourceSpan source = item.Source();
        if (source.begin.IsKnown()) {
            std::fprintf(stderr, " @%u:%u",
                source.begin.line, source.begin.column);
        }
        std::fputc('\n', stderr);
        const Aero::Base::Span<const Aero::Diagnostics::DiagnosticNote> notes =
            item.Notes();
        for (std::uint32_t note = 0U; note < notes.Size(); ++note) {
            const StringView noteMessage = notes[note].Message();
            std::fprintf(stderr, "  note: %.*s\n",
                static_cast<int>(noteMessage.SizeBytes()),
                noteMessage.Data());
        }
    }
}

bool DiagnosticsReportSampleFailure(
    const Aero::Diagnostics::DiagnosticBag& bag) noexcept {
    for (std::uint32_t index = 0U; index < bag.Size(); ++index) {
        const Aero::Diagnostics::Diagnostic& item = bag.Items()[index];
        if (ReportsSampleLoadFailure(item.Message())) {
            return true;
        }
        const Aero::Base::Span<const Aero::Diagnostics::DiagnosticNote> notes =
            item.Notes();
        for (std::uint32_t note = 0U; note < notes.Size(); ++note) {
            if (ReportsSampleLoadFailure(notes[note].Message())) {
                return true;
            }
        }
    }
    return false;
}

bool WriteUtf8File(
    const std::filesystem::path& path,
    StringView text) noexcept {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return false;
    }
    stream.write(text.Data(), static_cast<std::streamsize>(text.SizeBytes()));
    return static_cast<bool>(stream);
}

class IdentityConverter : public IValueConverter {
public:
    Aero::Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Result<Aero::Value> Convert(
        const Aero::Value& value,
        const Aero::Value&) noexcept override {
        return value;
    }
    Result<Aero::Value> ConvertBack(
        const Aero::Value& value,
        const Aero::Value&) noexcept override {
        return value;
    }
protected:
    using IValueConverter::IValueConverter;
};

#define SAMPLE_CONVERTER(TypeName, Ns)                                        \
class TypeName final : public IdentityConverter {                             \
    AERO_DECLARE_TYPE_NAMED(                                                  \
        TypeName, IValueConverter, "clr-namespace:" Ns, #TypeName)            \
public:                                                                       \
    TypeName() noexcept : IdentityConverter() {}                              \
    Aero::Meta::TypeId RuntimeType() const noexcept override {                \
        return StaticTypeId();                                                \
    }                                                                         \
}

SAMPLE_CONVERTER(HoursConverter, "CustomControl");
SAMPLE_CONVERTER(MinutesConverter, "CustomControl");
SAMPLE_CONVERTER(SecondsConverter, "CustomControl");
SAMPLE_CONVERTER(OrbitConverter, "DataBinding");
SAMPLE_CONVERTER(LevelToColorConverter, "Localization");
SAMPLE_CONVERTER(MultiplierConverter, "Menu3D");

class DateTimeControl final : public Control {
    AERO_DECLARE_TYPE_NAMED(
        DateTimeControl,
        Control,
        "clr-namespace:CustomControl",
        "DateTime")
public:
    DateTimeControl() noexcept : Control(StaticTypeId()) {}
    inline static constexpr DependencyProperty<std::int32_t> DayProperty{"Day"};
    inline static constexpr DependencyProperty<std::int32_t> MonthProperty{"Month"};
    inline static constexpr DependencyProperty<std::int32_t> YearProperty{"Year"};
    inline static constexpr DependencyProperty<std::int32_t> HourProperty{"Hour"};
    inline static constexpr DependencyProperty<std::int32_t> MinuteProperty{"Minute"};
    inline static constexpr DependencyProperty<std::int32_t> SecondProperty{"Second"};
};

class SampleAnimation : public Aero::Media::Animation::DoubleAnimationBase {
protected:
    explicit SampleAnimation(Aero::Meta::TypeId type) noexcept
        : DoubleAnimationBase(type) {}
    double GetCurrentValueCore(
        double defaultOriginValue,
        double defaultDestinationValue,
        double progress) const noexcept override {
        const double from = ResolveFrom(defaultOriginValue);
        const double to = ResolveTo(defaultDestinationValue);
        const double clamped =
            progress < 0.0 ? 0.0 : (progress > 1.0 ? 1.0 : progress);
        return from + (to - from) * clamped;
    }
};

#define SAMPLE_ANIM(TypeName)                                                 \
class TypeName final : public SampleAnimation {                               \
    AERO_DECLARE_TYPE_NAMED(                                                  \
        TypeName,                                                             \
        Aero::Media::Animation::DoubleAnimationBase,                          \
        "clr-namespace:CustomAnimation",                                      \
        #TypeName)                                                            \
public:                                                                       \
    TypeName() noexcept : SampleAnimation(StaticTypeId()) {}                  \
    inline static constexpr DependencyProperty<Base::String>                  \
        EdgeBehaviorProperty{"EdgeBehavior"};                                 \
    inline static constexpr DependencyProperty<double> AmplitudeProperty{"Amplitude"}; \
    inline static constexpr DependencyProperty<double> SuppressionProperty{"Suppression"}; \
    inline static constexpr DependencyProperty<double> PowerProperty{"Power"}; \
    inline static constexpr DependencyProperty<double> RadiusProperty{"Radius"}; \
    inline static constexpr DependencyProperty<Base::String>                  \
        DirectionProperty{"Direction"};                                       \
}

SAMPLE_ANIM(BackAnimation);
SAMPLE_ANIM(BounceAnimation);
SAMPLE_ANIM(CircleAnimation);
SAMPLE_ANIM(ElasticAnimation);
SAMPLE_ANIM(ExponentialAnimation);

class Game final : public FrameworkElement {
    AERO_DECLARE_TYPE_NAMED(
        Game,
        FrameworkElement,
        "clr-namespace:CustomRender",
        "Game")
public:
    Game() noexcept : FrameworkElement(StaticTypeId()) {}
};

class SolarSystem final : public Aero::Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        SolarSystem,
        Aero::Base::Object,
        "clr-namespace:DataBinding",
        "SolarSystem")
public:
    SolarSystem() noexcept = default;
    Aero::Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Ref<Aero::Collections::ObservableObjectCollection>
    GetSolarSystemObjects() const noexcept {
        return objects_;
    }
    void SetSolarSystemObjects(
        Ref<Aero::Collections::ObservableObjectCollection> value) noexcept {
        objects_ = std::move(value);
    }

private:
    Ref<Aero::Collections::ObservableObjectCollection> objects_{};
};

class ColorSelector final : public UserControl {
    AERO_DECLARE_TYPE_NAMED(
        ColorSelector,
        UserControl,
        "clr-namespace:BlendTutorial",
        "ColorSelector")
public:
    ColorSelector() noexcept : UserControl(StaticTypeId()) {}
    inline static constexpr DependencyProperty<Aero::Base::Color> ColorProperty{"Color"};
};

class NoiseBrush final : public Aero::Media::BrushShader {
    AERO_DECLARE_TYPE_NAMED(
        NoiseBrush,
        Aero::Media::BrushShader,
        "clr-namespace:BrushShaders",
        "NoiseBrush")
public:
    NoiseBrush() noexcept : BrushShader(StaticTypeId()) {}
    inline static constexpr AttachedProperty<double> SeedProperty{"Seed"};
    inline static constexpr DependencyProperty<double> FrequencyProperty{"Frequency"};
    inline static constexpr DependencyProperty<double> OctavesProperty{"Octaves"};
    inline static constexpr DependencyProperty<double> ScaleXProperty{"ScaleX"};
    inline static constexpr DependencyProperty<double> ScaleYProperty{"ScaleY"};
    inline static constexpr DependencyProperty<double> TimeProperty{"Time"};
    inline static constexpr DependencyProperty<Aero::Base::Color> ColorProperty{"Color"};
};

class MenuUserControl : public UserControl {
protected:
    explicit MenuUserControl(Aero::Meta::TypeId type) noexcept
        : UserControl(type) {}
};

#define SAMPLE_UC(TypeName, Ns)                                               \
class TypeName final : public MenuUserControl {                               \
    AERO_DECLARE_TYPE_NAMED(                                                  \
        TypeName, UserControl, "clr-namespace:" Ns, #TypeName)                \
public:                                                                       \
    TypeName() noexcept : MenuUserControl(StaticTypeId()) {}                  \
}

SAMPLE_UC(MainMenu, "Menu3D");
SAMPLE_UC(StartMenu, "Menu3D");
SAMPLE_UC(SettingsMenu, "Menu3D");

class MenuDescription final : public UserControl {
    AERO_DECLARE_TYPE_NAMED(
        MenuDescription,
        UserControl,
        "clr-namespace:Menu3D",
        "MenuDescription")
public:
    MenuDescription() noexcept : UserControl(StaticTypeId()) {}
    inline static constexpr DependencyProperty<Base::String> DescriptionProperty{"Description"};
    inline static constexpr DependencyProperty<Base::String> AcceptTextProperty{"AcceptText"};
    inline static constexpr DependencyProperty<Base::String> CancelTextProperty{"CancelText"};
};

enum class Menu3DState : std::uint8_t {
    Main = 0U,
    Start,
    Settings
};

class OptionSelector final : public UserControl {
    AERO_DECLARE_TYPE_NAMED(
        OptionSelector,
        UserControl,
        "clr-namespace:Menu3D",
        "OptionSelector")
public:
    OptionSelector() noexcept : UserControl(StaticTypeId()) {
        options_ = Aero::Base::MakeRef<Aero::Collections::ObservableObjectCollection>().Value();
    }
    Ref<Aero::Collections::ObservableObjectCollection> GetOptions() const noexcept {
        return options_;
    }
    void SetOptions(
        Ref<Aero::Collections::ObservableObjectCollection> value) noexcept {
        options_ = std::move(value);
    }
    inline static constexpr DependencyProperty<std::int32_t> SelectedIndexProperty{"SelectedIndex"};
    inline static constexpr DependencyProperty<Ref<Aero::Base::Object>>
        SelectedOptionProperty{"SelectedOption"};
private:
    Ref<Aero::Collections::ObservableObjectCollection> options_{};
};

class ScoreboardPlayer final : public Aero::DependencyObject {
    AERO_DECLARE_TYPE_NAMED(
        ScoreboardPlayer,
        Aero::DependencyObject,
        "clr-namespace:Scoreboard",
        "Player")
public:
    ScoreboardPlayer() noexcept : DependencyObject(StaticTypeId()) {}
    inline static constexpr DependencyProperty<Base::String> ClassProperty{"Class"};
    inline static constexpr DependencyProperty<std::int32_t> DeathsProperty{"Deaths"};
    inline static constexpr DependencyProperty<std::int32_t> DamageProperty{"Damage"};
    inline static constexpr DependencyProperty<std::int32_t> HealProperty{"Heal"};
    inline static constexpr DependencyProperty<std::int32_t> KillsProperty{"Kills"};
    inline static constexpr DependencyProperty<Base::String> NameProperty{"Name"};
    inline static constexpr DependencyProperty<std::int32_t> ScoreProperty{"Score"};
    inline static constexpr DependencyProperty<Base::String> TeamProperty{"Team"};
};

class ScoreboardGame final : public Aero::DependencyObject {
    AERO_DECLARE_TYPE_NAMED(
        ScoreboardGame,
        Aero::DependencyObject,
        "clr-namespace:Scoreboard",
        "Game")
public:
    ScoreboardGame() noexcept : DependencyObject(StaticTypeId()) {
        players_ = Aero::Base::MakeRef<Aero::Collections::ObservableObjectCollection>().Value();
    }
    inline static constexpr DependencyProperty<std::int32_t> ElapsedTimeProperty{"ElapsedTime"};
    inline static constexpr DependencyProperty<Base::String> NameProperty{"Name"};
    Ref<Aero::Collections::ObservableObjectCollection> GetPlayers() const noexcept {
        return players_;
    }
    void SetPlayers(
        Ref<Aero::Collections::ObservableObjectCollection> value) noexcept {
        players_ = std::move(value);
    }
private:
    Ref<Aero::Collections::ObservableObjectCollection> players_{};
};

enum class ItemCategory : std::uint8_t {
    All = 0U,
    Hand,
    Ring,
    Head,
    Chest,
    Arms,
    Legs,
    Feet
};

class InventoryItem final : public Aero::DependencyObject {
    AERO_DECLARE_TYPE_NAMED(
        InventoryItem,
        Aero::DependencyObject,
        "clr-namespace:Inventory",
        "Item")
public:
    InventoryItem() noexcept : DependencyObject(StaticTypeId()) {}
    inline static constexpr DependencyProperty<Base::String> NameProperty{"Name"};
    inline static constexpr DependencyProperty<Base::String> DescriptionProperty{"Description"};
    inline static constexpr DependencyProperty<ItemCategory> CategoryProperty{"Category"};
    inline static constexpr DependencyProperty<std::int32_t> LifeProperty{"Life"};
    inline static constexpr DependencyProperty<std::int32_t> ManaProperty{"Mana"};
    inline static constexpr DependencyProperty<std::int32_t> DpsProperty{"Dps"};
    inline static constexpr DependencyProperty<std::int32_t> ArmorProperty{"Armor"};
    inline static constexpr DependencyProperty<Ref<Aero::Media::ImageSource>> IconProperty{"Icon"};
};

class InventorySlot final : public Aero::DependencyObject {
    AERO_DECLARE_TYPE_NAMED(
        InventorySlot,
        Aero::DependencyObject,
        "clr-namespace:Inventory",
        "Slot")
public:
    InventorySlot() noexcept : DependencyObject(StaticTypeId()) {}
    inline static constexpr DependencyProperty<Base::String> NameProperty{"Name"};
    inline static constexpr DependencyProperty<ItemCategory> AllowedCategoryProperty{"AllowedCategory"};
    inline static constexpr DependencyProperty<Ref<InventoryItem>> ItemProperty{"Item"};
    inline static constexpr DependencyProperty<bool> IsDragOverProperty{"IsDragOver"};
    inline static constexpr DependencyProperty<bool> IsDropAllowedProperty{"IsDropAllowed"};
    inline static constexpr DependencyProperty<bool> IsSelectedProperty{"IsSelected"};
    inline static constexpr DependencyProperty<bool> MoveFocusProperty{"MoveFocus"};
};

class InventoryPlayer final : public Aero::DependencyObject {
    AERO_DECLARE_TYPE_NAMED(
        InventoryPlayer,
        Aero::DependencyObject,
        "clr-namespace:Inventory",
        "Player")
public:
    InventoryPlayer() noexcept : DependencyObject(StaticTypeId()) {}
    inline static constexpr DependencyProperty<Base::String> NameProperty{"Name"};
    inline static constexpr DependencyProperty<std::int32_t> LifeProperty{"Life"};
    inline static constexpr DependencyProperty<std::int32_t> ManaProperty{"Mana"};
    inline static constexpr DependencyProperty<std::int32_t> DpsProperty{"Dps"};
    inline static constexpr DependencyProperty<std::int32_t> ArmorProperty{"Armor"};
    Ref<Aero::Collections::ObservableObjectCollection> GetSlots() const noexcept {
        return slots_;
    }
    void SetSlots(
        Ref<Aero::Collections::ObservableObjectCollection> value) noexcept {
        slots_ = std::move(value);
    }

private:
    Ref<Aero::Collections::ObservableObjectCollection> slots_{};
};

class InventoryViewModel final : public Aero::DependencyObject {
    AERO_DECLARE_TYPE_NAMED(
        InventoryViewModel,
        Aero::DependencyObject,
        "clr-namespace:Inventory",
        "ViewModel")
public:
    InventoryViewModel() noexcept : DependencyObject(StaticTypeId()) {}
    inline static constexpr DependencyProperty<Base::String> PlatformProperty{"Platform"};
    inline static constexpr DependencyProperty<Ref<InventoryPlayer>> PlayerProperty{"Player"};
    Ref<Aero::Collections::ObservableObjectCollection> GetInventory() const noexcept {
        return inventory_;
    }
    void SetInventory(
        Ref<Aero::Collections::ObservableObjectCollection> value) noexcept {
        inventory_ = std::move(value);
    }
    inline static constexpr DependencyProperty<Ref<Aero::Base::Object>> ItemsProperty{"Items"};
    inline static constexpr DependencyProperty<Ref<Aero::Input::ICommand>> StartDragItemProperty{"StartDragItem"};
    inline static constexpr DependencyProperty<Ref<Aero::Input::ICommand>> EndDragItemProperty{"EndDragItem"};
    inline static constexpr DependencyProperty<Ref<Aero::Input::ICommand>> DropItemProperty{"DropItem"};
    inline static constexpr DependencyProperty<Ref<Aero::Input::ICommand>> SelectSlotProperty{"SelectSlot"};
    inline static constexpr DependencyProperty<Ref<InventorySlot>> DragSourceProperty{"DragSource"};
    inline static constexpr DependencyProperty<Ref<InventoryItem>> DraggedItemProperty{"DraggedItem"};
    inline static constexpr DependencyProperty<Ref<InventorySlot>> SelectedSlotProperty{"SelectedSlot"};

private:
    Ref<Aero::Collections::ObservableObjectCollection> inventory_{};
};

class AnimatedNumber final : public UserControl {
    AERO_DECLARE_TYPE_NAMED(
        AnimatedNumber,
        UserControl,
        "clr-namespace:Inventory",
        "AnimatedNumber")
public:
    AnimatedNumber() noexcept : UserControl(StaticTypeId()) {}
    inline static constexpr DependencyProperty<std::int32_t> NumberProperty{"Number"};
    inline static constexpr DependencyProperty<std::int32_t> AnimatedNumberProperty{"AnimatedNumber"};
    inline static constexpr DependencyProperty<Aero::Media::Animation::Duration>
        AnimationDurationProperty{"AnimationDuration"};
};

class LocalizationViewModel final : public Aero::Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        LocalizationViewModel,
        Aero::Base::Object,
        "clr-namespace:Localization",
        "ViewModel")
public:
    LocalizationViewModel() noexcept = default;
    Aero::Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Ref<Aero::Collections::ObservableObjectCollection> GetLanguages() const noexcept {
        return languages_;
    }
    void SetLanguages(
        Ref<Aero::Collections::ObservableObjectCollection> value) noexcept {
        languages_ = std::move(value);
    }

private:
    Ref<Aero::Collections::ObservableObjectCollection> languages_{};
};

class QuestLogViewModel final : public Aero::Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        QuestLogViewModel,
        Aero::Base::Object,
        "clr-namespace:QuestLog",
        "ViewModel")
public:
    QuestLogViewModel() noexcept = default;
    Aero::Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Ref<Aero::Collections::ObservableObjectCollection> GetQuests() const noexcept {
        return quests_;
    }
    void SetQuests(
        Ref<Aero::Collections::ObservableObjectCollection> value) noexcept {
        quests_ = std::move(value);
    }

private:
    Ref<Aero::Collections::ObservableObjectCollection> quests_{};
};

class TicTacToeViewModel final : public Aero::DependencyObject {
    AERO_DECLARE_TYPE_NAMED(
        TicTacToeViewModel,
        Aero::DependencyObject,
        "clr-namespace:TicTacToe",
        "ViewModel")
public:
    TicTacToeViewModel() noexcept : DependencyObject(StaticTypeId()) {}
};

enum class TicTacToeState : std::uint8_t {
    Player1 = 0U,
    Player2
};

class ColorConverter final : public IMultiValueConverter {
    AERO_DECLARE_TYPE_NAMED(
        ColorConverter,
        IMultiValueConverter,
        "clr-namespace:UserControls",
        "ColorConverter")
public:
    ColorConverter() noexcept = default;
    Aero::Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Result<Aero::Value> Convert(
        Aero::Base::Span<const Aero::Value> values,
        Aero::Meta::TypeId,
        const Aero::Value&) noexcept override {
        if (values.Size() < 3U) {
            return Aero::Value{};
        }
        auto channel = [](const Aero::Value& value) noexcept -> float {
            if (value.Kind() == Aero::Base::ValueKind::SignedInteger) {
                return static_cast<float>(value.AsSignedInteger()) / 255.0F;
            }
            if (value.Kind() == Aero::Base::ValueKind::Double) {
                return static_cast<float>(value.AsDouble()) / 255.0F;
            }
            if (value.Kind() == Aero::Base::ValueKind::UnsignedInteger) {
                return static_cast<float>(value.AsUnsignedInteger()) / 255.0F;
            }
            return 0.0F;
        };
        return Aero::Meta::ValueCodec<Aero::Base::Color>::Encode({
            channel(values[0]),
            channel(values[1]),
            channel(values[2]),
            1.0F});
    }
};

class NumericUpDown final : public UserControl {
    AERO_DECLARE_TYPE_NAMED(
        NumericUpDown,
        UserControl,
        "clr-namespace:UserControls",
        "NumericUpDown")
public:
    NumericUpDown() noexcept : UserControl(StaticTypeId()) {}
    std::int32_t GetNumericValue() const noexcept {
        return GetValueOr(ValueProperty, 0);
    }
    std::int32_t GetMinValue() const noexcept {
        return GetValueOr(MinValueProperty, 0);
    }
    std::int32_t GetMaxValue() const noexcept {
        return GetValueOr(MaxValueProperty, 255);
    }
    std::int32_t GetStepValue() const noexcept {
        return GetValueOr(StepValueProperty, 1);
    }
    void SetNumericValue(std::int32_t value) noexcept {
        const std::int32_t min = GetMinValue();
        const std::int32_t max = GetMaxValue();
        if (value < min) value = min;
        if (value > max) value = max;
        SetValue(ValueProperty, value);
    }
    void UpButton_Click(Aero::Base::Object*, Aero::RoutedEventArgs&) noexcept {
        SetNumericValue(GetNumericValue() + GetStepValue());
    }
    void DownButton_Click(Aero::Base::Object*, Aero::RoutedEventArgs&) noexcept {
        SetNumericValue(GetNumericValue() - GetStepValue());
    }
    inline static constexpr DependencyProperty<std::int32_t> ValueProperty{"Value"};
    inline static constexpr DependencyProperty<std::int32_t> MinValueProperty{"MinValue"};
    inline static constexpr DependencyProperty<std::int32_t> MaxValueProperty{"MaxValue"};
    inline static constexpr DependencyProperty<std::int32_t> StepValueProperty{"StepValue"};
};

class SampleBehavior : public Aero::Interactivity::Behavior {
protected:
    explicit SampleBehavior(Aero::Meta::TypeId type) noexcept
        : Behavior(type) {}
};

class DragAdornerBehavior final : public SampleBehavior {
    AERO_DECLARE_TYPE_NAMED(
        DragAdornerBehavior,
        Aero::Interactivity::Behavior,
        "clr-namespace:Inventory",
        "DragAdornerBehavior")
public:
    DragAdornerBehavior() noexcept : SampleBehavior(StaticTypeId()) {}
    inline static constexpr DependencyProperty<Aero::Base::Point>
        DragStartOffsetProperty{"DragStartOffset"};
    inline static constexpr DependencyProperty<double> DraggedItemXProperty{"DraggedItemX"};
    inline static constexpr DependencyProperty<double> DraggedItemYProperty{"DraggedItemY"};
};

class DragItemBehavior final : public SampleBehavior {
    AERO_DECLARE_TYPE_NAMED(
        DragItemBehavior,
        Aero::Interactivity::Behavior,
        "clr-namespace:Inventory",
        "DragItemBehavior")
public:
    DragItemBehavior() noexcept : SampleBehavior(StaticTypeId()) {}
    inline static constexpr DependencyProperty<Aero::Base::Point>
        DragStartOffsetProperty{"DragStartOffset"};
    inline static constexpr DependencyProperty<Ref<Aero::Input::ICommand>>
        StartDragCommandProperty{"StartDragCommand"};
    inline static constexpr DependencyProperty<Ref<Aero::Input::ICommand>>
        EndDragCommandProperty{"EndDragCommand"};
};

class DropItemBehavior final : public SampleBehavior {
    AERO_DECLARE_TYPE_NAMED(
        DropItemBehavior,
        Aero::Interactivity::Behavior,
        "clr-namespace:Inventory",
        "DropItemBehavior")
public:
    DropItemBehavior() noexcept : SampleBehavior(StaticTypeId()) {}
    inline static constexpr DependencyProperty<bool> IsDragOverProperty{"IsDragOver"};
    inline static constexpr DependencyProperty<Ref<Aero::Input::ICommand>>
        DropCommandProperty{"DropCommand"};
};

void AddOptionSelectorOption(
    Aero::Base::Object& owner,
    const Ref<Aero::Base::Object>& value,
    void*) noexcept {
    auto& selector = static_cast<OptionSelector&>(owner);
    Ref<Aero::Collections::ObservableObjectCollection> options =
        selector.GetOptions();
    if (!options) {
        Result<Ref<Aero::Collections::ObservableObjectCollection>> created =
            Aero::Base::MakeRef<Aero::Collections::ObservableObjectCollection>();
        if (!created) {
            return;
        }
        options = std::move(created).Value();
        selector.SetOptions(options);
    }
    if (value) {
        static_cast<void>(options->Add(value));
    }
}

void ClearOptionSelectorOptions(
    Aero::Base::Object& owner,
    void*) noexcept {
    auto& selector = static_cast<OptionSelector&>(owner);
    if (Ref<Aero::Collections::ObservableObjectCollection> options =
            selector.GetOptions()) {
        options->Reset();
    }
}

} // namespace

AERO_DECLARE_TYPE_ENUM(ItemCategory)
AERO_DECLARE_TYPE_ENUM(Menu3DState)
AERO_DECLARE_TYPE_ENUM(TicTacToeState)

namespace {

Result<void> RegisterSampleHostTypes(
    Aero::Meta::Registration& registration) noexcept {
    using Aero::Meta::FrameworkPropertyMetadata;
    using Aero::Meta::Register;
    Result<void> status;

    auto menuState = Register<Menu3DState>(
        registration, StringView("clr-namespace:Menu3D"), "State");
    menuState
        .Value("Main", Menu3DState::Main)
        .Value("Start", Menu3DState::Start)
        .Value("Settings", Menu3DState::Settings);
    status = menuState.Result();
    if (!status) return status;

    auto ticState = Register<TicTacToeState>(
        registration, StringView("clr-namespace:TicTacToe"), "State");
    ticState
        .Value("Player1", TicTacToeState::Player1)
        .Value("Player2", TicTacToeState::Player2);
    status = ticState.Result();
    if (!status) return status;

    auto category = Register<ItemCategory>(
        registration, StringView("clr-namespace:Inventory"), "ItemCategory");
    category
        .Value("All", ItemCategory::All)
        .Value("Hand", ItemCategory::Hand)
        .Value("Ring", ItemCategory::Ring)
        .Value("Head", ItemCategory::Head)
        .Value("Chest", ItemCategory::Chest)
        .Value("Arms", ItemCategory::Arms)
        .Value("Legs", ItemCategory::Legs)
        .Value("Feet", ItemCategory::Feet);
    status = category.Result();
    if (!status) return status;

    auto item = Register<InventoryItem>(registration);
    item
        .Property(InventoryItem::NameProperty, FrameworkPropertyMetadata(String{}))
        .Property(InventoryItem::DescriptionProperty, FrameworkPropertyMetadata(String{}))
        .Property(InventoryItem::CategoryProperty, FrameworkPropertyMetadata(ItemCategory::All))
        .Property(InventoryItem::LifeProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(InventoryItem::ManaProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(InventoryItem::DpsProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(InventoryItem::ArmorProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(InventoryItem::IconProperty, FrameworkPropertyMetadata(Ref<Aero::Media::ImageSource>{}))
        .Factory();
    status = item.Result();
    if (!status) return status;

    auto slot = Register<InventorySlot>(registration);
    slot
        .Property(InventorySlot::NameProperty, FrameworkPropertyMetadata(String{}))
        .Property(InventorySlot::AllowedCategoryProperty, FrameworkPropertyMetadata(ItemCategory::All))
        .Property(InventorySlot::ItemProperty, FrameworkPropertyMetadata(Ref<InventoryItem>{}))
        .Property(InventorySlot::IsDragOverProperty, FrameworkPropertyMetadata(false))
        .Property(InventorySlot::IsDropAllowedProperty, FrameworkPropertyMetadata(false))
        .Property(InventorySlot::IsSelectedProperty, FrameworkPropertyMetadata(false))
        .Property(InventorySlot::MoveFocusProperty, FrameworkPropertyMetadata(false))
        .Factory();
    status = slot.Result();
    if (!status) return status;

    auto player = Register<InventoryPlayer>(registration);
    player
        .Property(InventoryPlayer::NameProperty, FrameworkPropertyMetadata(String{}))
        .Property(InventoryPlayer::LifeProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(InventoryPlayer::ManaProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(InventoryPlayer::DpsProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(InventoryPlayer::ArmorProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property<&InventoryPlayer::GetSlots, &InventoryPlayer::SetSlots>("Slots")
        .Factory();
    status = player.Result();
    if (!status) return status;

    auto viewModel = Register<InventoryViewModel>(registration);
    viewModel
        .Property(InventoryViewModel::PlatformProperty, FrameworkPropertyMetadata(String{}))
        .Property(InventoryViewModel::PlayerProperty, FrameworkPropertyMetadata(Ref<InventoryPlayer>{}))
        .Property<&InventoryViewModel::GetInventory, &InventoryViewModel::SetInventory>(
            "Inventory")
        .Property(InventoryViewModel::ItemsProperty, FrameworkPropertyMetadata(Ref<Aero::Base::Object>{}))
        .Property(InventoryViewModel::StartDragItemProperty, FrameworkPropertyMetadata(Ref<Aero::Input::ICommand>{}))
        .Property(InventoryViewModel::EndDragItemProperty, FrameworkPropertyMetadata(Ref<Aero::Input::ICommand>{}))
        .Property(InventoryViewModel::DropItemProperty, FrameworkPropertyMetadata(Ref<Aero::Input::ICommand>{}))
        .Property(InventoryViewModel::SelectSlotProperty, FrameworkPropertyMetadata(Ref<Aero::Input::ICommand>{}))
        .Property(InventoryViewModel::DragSourceProperty, FrameworkPropertyMetadata(Ref<InventorySlot>{}))
        .Property(InventoryViewModel::DraggedItemProperty, FrameworkPropertyMetadata(Ref<InventoryItem>{}))
        .Property(InventoryViewModel::SelectedSlotProperty, FrameworkPropertyMetadata(Ref<InventorySlot>{}))
        .Factory();
    status = viewModel.Result();
    if (!status) return status;

    auto animated = Register<AnimatedNumber>(registration);
    animated
        .Property(AnimatedNumber::NumberProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(AnimatedNumber::AnimatedNumberProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(
            AnimatedNumber::AnimationDurationProperty,
            FrameworkPropertyMetadata(Aero::Media::Animation::Duration{}))
        .Factory();
    status = animated.Result();
    if (!status) return status;

    auto adorner = Register<DragAdornerBehavior>(registration);
    adorner
        .Property(DragAdornerBehavior::DragStartOffsetProperty, FrameworkPropertyMetadata(Aero::Base::Point{}))
        .Property(DragAdornerBehavior::DraggedItemXProperty, FrameworkPropertyMetadata(0.0))
        .Property(DragAdornerBehavior::DraggedItemYProperty, FrameworkPropertyMetadata(0.0))
        .Factory();
    status = adorner.Result();
    if (!status) return status;

    auto drag = Register<DragItemBehavior>(registration);
    drag
        .Property(DragItemBehavior::DragStartOffsetProperty, FrameworkPropertyMetadata(Aero::Base::Point{}))
        .Property(DragItemBehavior::StartDragCommandProperty, FrameworkPropertyMetadata(Ref<Aero::Input::ICommand>{}))
        .Property(DragItemBehavior::EndDragCommandProperty, FrameworkPropertyMetadata(Ref<Aero::Input::ICommand>{}))
        .Factory();
    status = drag.Result();
    if (!status) return status;

    auto drop = Register<DropItemBehavior>(registration);
    drop
        .Property(DropItemBehavior::IsDragOverProperty, FrameworkPropertyMetadata(false))
        .Property(DropItemBehavior::DropCommandProperty, FrameworkPropertyMetadata(Ref<Aero::Input::ICommand>{}))
        .Factory();
    status = drop.Result();
    if (!status) return status;

    auto dateTime = Register<DateTimeControl>(registration);
    dateTime
        .Property(DateTimeControl::DayProperty, FrameworkPropertyMetadata(std::int32_t{1}))
        .Property(DateTimeControl::MonthProperty, FrameworkPropertyMetadata(std::int32_t{1}))
        .Property(DateTimeControl::YearProperty, FrameworkPropertyMetadata(std::int32_t{2000}))
        .Property(DateTimeControl::HourProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(DateTimeControl::MinuteProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(DateTimeControl::SecondProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Factory();
    status = dateTime.Result();
    if (!status) return status;

#define REGISTER_CONVERTER(Type)                                              \
    status = Register<Type>(registration).Factory().Result();                 \
    if (!status) return status

    REGISTER_CONVERTER(HoursConverter);
    REGISTER_CONVERTER(MinutesConverter);
    REGISTER_CONVERTER(SecondsConverter);
    REGISTER_CONVERTER(OrbitConverter);
    REGISTER_CONVERTER(LevelToColorConverter);
    REGISTER_CONVERTER(MultiplierConverter);

#define REGISTER_ANIM(Type)                                                   \
    do {                                                                      \
        auto anim = Register<Type>(registration);                             \
        anim.Property(Type::EdgeBehaviorProperty, FrameworkPropertyMetadata(String{})) \
            .Property(Type::AmplitudeProperty, FrameworkPropertyMetadata(1.0)) \
            .Property(Type::SuppressionProperty, FrameworkPropertyMetadata(1.0)) \
            .Property(Type::PowerProperty, FrameworkPropertyMetadata(2.0))    \
            .Property(Type::RadiusProperty, FrameworkPropertyMetadata(1.0))   \
            .Property(Type::DirectionProperty, FrameworkPropertyMetadata(String{})) \
            .Factory();                                                       \
        status = anim.Result();                                               \
        if (!status) return status;                                           \
    } while (false)

    REGISTER_ANIM(BackAnimation);
    REGISTER_ANIM(BounceAnimation);
    REGISTER_ANIM(CircleAnimation);
    REGISTER_ANIM(ElasticAnimation);
    REGISTER_ANIM(ExponentialAnimation);

    status = Register<Game>(registration).Factory().Result();
    if (!status) return status;

    status = Register<SolarSystem>(registration)
        .Property<&SolarSystem::GetSolarSystemObjects, &SolarSystem::SetSolarSystemObjects>(
            "SolarSystemObjects")
        .Factory()
        .Result();
    if (!status) return status;

    auto colorSelector = Register<ColorSelector>(registration);
    colorSelector
        .Property(
            ColorSelector::ColorProperty,
            FrameworkPropertyMetadata(Aero::Base::Color{}))
        .Factory();
    status = colorSelector.Result();
    if (!status) return status;

    auto noise = Register<NoiseBrush>(registration);
    noise
        .Property(NoiseBrush::SeedProperty, FrameworkPropertyMetadata(0.0))
        .Property(NoiseBrush::FrequencyProperty, FrameworkPropertyMetadata(1.0))
        .Property(NoiseBrush::OctavesProperty, FrameworkPropertyMetadata(1.0))
        .Property(NoiseBrush::ScaleXProperty, FrameworkPropertyMetadata(1.0))
        .Property(NoiseBrush::ScaleYProperty, FrameworkPropertyMetadata(1.0))
        .Property(NoiseBrush::TimeProperty, FrameworkPropertyMetadata(0.0))
        .Property(
            NoiseBrush::ColorProperty,
            FrameworkPropertyMetadata(Aero::Base::Color{}))
        .Factory();
    status = noise.Result();
    if (!status) return status;

    REGISTER_CONVERTER(MainMenu);
    REGISTER_CONVERTER(StartMenu);
    REGISTER_CONVERTER(SettingsMenu);

    auto menuDescription = Register<MenuDescription>(registration);
    menuDescription
        .Property(MenuDescription::DescriptionProperty, FrameworkPropertyMetadata(String{}))
        .Property(MenuDescription::AcceptTextProperty, FrameworkPropertyMetadata(String{}))
        .Property(MenuDescription::CancelTextProperty, FrameworkPropertyMetadata(String{}))
        .Factory();
    status = menuDescription.Result();
    if (!status) return status;

    auto option = Register<OptionSelector>(registration);
    option
        .Property(OptionSelector::SelectedIndexProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(
            OptionSelector::SelectedOptionProperty,
            FrameworkPropertyMetadata(Ref<Aero::Base::Object>{}))
        .Content<Aero::Base::Object>(
            "Options",
            Aero::Meta::ContentKind::Collection,
            &AddOptionSelectorOption,
            &ClearOptionSelectorOptions)
        .Factory();
    status = option.Result();
    if (!status) return status;

    status = Register<ColorConverter>(registration).Factory().Result();
    if (!status) return status;

    auto numeric = Register<NumericUpDown>(registration);
    numeric
        .Property(NumericUpDown::ValueProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(NumericUpDown::MinValueProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(NumericUpDown::MaxValueProperty, FrameworkPropertyMetadata(std::int32_t{255}))
        .Property(NumericUpDown::StepValueProperty, FrameworkPropertyMetadata(std::int32_t{1}))
        .EventHandler<Aero::RoutedEventArgs, &NumericUpDown::UpButton_Click>(
            "UpButton_Click")
        .EventHandler<Aero::RoutedEventArgs, &NumericUpDown::DownButton_Click>(
            "DownButton_Click")
        .Factory();
    status = numeric.Result();
    if (!status) return status;

    auto scorePlayer = Register<ScoreboardPlayer>(registration);
    scorePlayer
        .Property(ScoreboardPlayer::ClassProperty, FrameworkPropertyMetadata(String{}))
        .Property(ScoreboardPlayer::DeathsProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(ScoreboardPlayer::DamageProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(ScoreboardPlayer::HealProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(ScoreboardPlayer::KillsProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(ScoreboardPlayer::NameProperty, FrameworkPropertyMetadata(String{}))
        .Property(ScoreboardPlayer::ScoreProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(ScoreboardPlayer::TeamProperty, FrameworkPropertyMetadata(String{}))
        .Factory();
    status = scorePlayer.Result();
    if (!status) return status;

    auto scoreGame = Register<ScoreboardGame>(registration);
    scoreGame
        .Property(ScoreboardGame::ElapsedTimeProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(ScoreboardGame::NameProperty, FrameworkPropertyMetadata(String{}))
        .Property<&ScoreboardGame::GetPlayers, &ScoreboardGame::SetPlayers>("Players")
        .Factory();
    status = scoreGame.Result();
    if (!status) return status;

    status = Register<LocalizationViewModel>(registration)
        .Property<&LocalizationViewModel::GetLanguages, &LocalizationViewModel::SetLanguages>(
            "Languages")
        .Factory()
        .Result();
    if (!status) return status;
    status = Register<QuestLogViewModel>(registration)
        .Property<&QuestLogViewModel::GetQuests, &QuestLogViewModel::SetQuests>(
            "Quests")
        .Factory()
        .Result();
    if (!status) return status;
    return Register<TicTacToeViewModel>(registration).Factory().Result();
}

#undef REGISTER_CONVERTER
#undef REGISTER_ANIM

const Aero::ModuleRegistration kSampleHostModule =
    Aero::DefineModule("Aero.SampleHostTests", RegisterSampleHostTypes);

struct LiveGui {
    Aero::Diagnostics::DiagnosticBag diagnostics;
    Gui gui;
    Ref<View> view;
    Aero::Markup::XamlDocument applicationDocument;
    Aero::Markup::XamlDocument sampleDocument;
    Aero::Application* application = nullptr;
    double viewTime = 0.0;
};

bool PumpSample(LiveGui& live) noexcept {
    if (!live.view) {
        return false;
    }
    live.viewTime += 0.016;
    // View::Update returns false when the frame version does not
    // change. Binding/inheritance still flush inside ExecuteViewFrame.
    static_cast<void>(live.view->Update(live.viewTime));
    return true;
}

LiveGui* NewSampleLiveGui() {
    auto* live = new LiveGui();
    ViewOptions options;
    options.diagnostics = &live->diagnostics;
    Result<void> sample = live->gui.AddModule(kSampleHostModule);
    if (!sample) {
        std::fprintf(stderr, "AddModule sample host failed: %s\n",
            sample.GetStatus().message);
        return nullptr;
    }
    Result<void> app = live->gui.AddModule(Aero::App::AppMetadataModule());
    if (!app) {
        std::fprintf(stderr, "AddModule AppMetadataModule failed: %s\n",
            app.GetStatus().message);
        return nullptr;
    }
    Result<void> initialized = live->gui.Initialize();
    if (!initialized) {
        std::fprintf(stderr, "Initialize failed: %s\n",
            initialized.GetStatus().message);
        return nullptr;
    }
    Result<Ref<View>> created = live->gui.CreateView(options);
    if (!created) {
        std::fprintf(stderr, "CreateView failed: %s\n",
            created.GetStatus().message);
        return nullptr;
    }
    live->view = std::move(created).Value();
    live->view->Activate();
    live->view->SetSize({640.0, 480.0});
    ViewViewport viewport;
    viewport.logicalSize = {640.0, 480.0};
    viewport.pixelWidth = 640U;
    viewport.pixelHeight = 480U;
    viewport.dpiScale = 1.0;
    static_cast<void>(live->view->SetViewport(viewport));
    return live;
}

bool FailIfSampleError(
    StringView where,
    const Aero::Base::Status& status,
    const Aero::Diagnostics::DiagnosticBag& diagnostics) {
    if (!status.IsOk() && ReportsSampleLoadFailure(CStringView(status.message))) {
        std::fprintf(stderr, "%.*s reported sample load failure: %s\n",
            static_cast<int>(where.SizeBytes()),
            where.Data(),
            status.message);
        DumpDiagnostics(diagnostics);
        return true;
    }
    if (DiagnosticsReportSampleFailure(diagnostics)) {
        std::fprintf(stderr, "%.*s diagnostics contain sample load failure (status=%s)\n",
            static_cast<int>(where.SizeBytes()),
            where.Data(),
            status.message != nullptr ? status.message : "");
        DumpDiagnostics(diagnostics);
        return true;
    }
    if (!status.IsOk()) {
        std::fprintf(stderr, "%.*s failed: %s\n",
            static_cast<int>(where.SizeBytes()),
            where.Data(),
            status.message);
        DumpDiagnostics(diagnostics);
        return true;
    }
    return false;
}

bool ApplyTemplatesRecursive(
    LiveGui& live,
    ::Aero::Media::Visual& visual,
    StringView where) {
    if (Control* control = TryCast<Control>(&visual)) {
        static_cast<void>(control->ApplyTemplate());
        if (DiagnosticsReportSampleFailure(live.diagnostics)) {
            std::fprintf(stderr, "%.*s ApplyTemplate diagnostics contain sample load failure\n",
                static_cast<int>(where.SizeBytes()),
                where.Data());
            DumpDiagnostics(live.diagnostics);
            return false;
        }
    }
    const std::uint32_t count =
        VisualTreeHelper::GetChildrenCount(visual);
    for (std::uint32_t index = 0U; index < count; ++index) {
        ::Aero::Media::Visual* child =
            VisualTreeHelper::GetChild(visual, index);
        if (child != nullptr &&
            !ApplyTemplatesRecursive(live, *child, where)) {
            return false;
        }
    }
    return true;
}

bool MountAndLayout(
    LiveGui& live,
    Aero::Markup::XamlDocument document,
    StringView where,
    bool skipNativeHost) {
    Aero::Base::Object* root = document.Root().Get();
    if (root == nullptr) {
        std::fprintf(stderr, "%.*s produced a null root\n",
            static_cast<int>(where.SizeBytes()),
            where.Data());
        return false;
    }
    if (TryCast<Aero::Application>(root) != nullptr ||
        TryCast<Aero::ResourceDictionary>(root) != nullptr) {
        return true;
    }
    FrameworkElement* element = TryCast<FrameworkElement>(root);
    if (element == nullptr) {
        return true;
    }
    if (skipNativeHost) {
        return true;
    }
    Result<void> mounted;
    if (TryCast<Window>(root) != nullptr) {
        live.sampleDocument = {};
        mounted = live.view->SetContent(
            std::move(document), {640.0, 480.0});
    } else {
        Aero::Markup::XamlReader reader(live.gui);
        Result<Aero::Markup::XamlDocument> host = reader.Parse(
            StringView(
                "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\"/>"),
            {},
            {},
            &live.diagnostics);
        if (!host) {
            return !FailIfSampleError(where, host.GetStatus(), live.diagnostics);
        }
        live.sampleDocument = {};
        mounted = live.view->SetContent(
            std::move(host).Value(), {640.0, 480.0});
        if (mounted) {
            Aero::Controls::Panel* panel =
                TryCast<Aero::Controls::Panel>(live.view->GetContent());
            if (panel == nullptr || element == nullptr) {
                std::fprintf(stderr,
                    "%.*s host Grid is unavailable for a non-Window root\n",
                    static_cast<int>(where.SizeBytes()),
                    where.Data());
                return false;
            }
            Result<void> added = panel->GetChildren().Add(
                Ref<Aero::UIElement>::FromBorrowed(
                    *static_cast<Aero::UIElement*>(element)));
            if (!added) {
                mounted = added;
            } else {
                live.sampleDocument = std::move(document);
            }
        }
    }
    if (FailIfSampleError(where, mounted.GetStatus(), live.diagnostics)) {
        return false;
    }
    if (!PumpSample(live)) {
        return false;
    }
    if (DiagnosticsReportSampleFailure(live.diagnostics)) {
        std::fprintf(stderr, "%.*s Update diagnostics contain sample load failure\n",
            static_cast<int>(where.SizeBytes()),
            where.Data());
        DumpDiagnostics(live.diagnostics);
        return false;
    }
    FrameworkElement* mountedRoot = live.view->GetContent();
    if (mountedRoot != nullptr) {
        if (!ApplyTemplatesRecursive(live, *mountedRoot, where)) {
            return false;
        }
        if (!PumpSample(live)) {
            return false;
        }
        if (DiagnosticsReportSampleFailure(live.diagnostics)) {
            std::fprintf(stderr, "%.*s apply diagnostics contain sample load failure\n",
                static_cast<int>(where.SizeBytes()),
                where.Data());
            DumpDiagnostics(live.diagnostics);
            return false;
        }
    }
    return true;
}

bool FillObjectCollection(
    Aero::Collections::ObservableObjectCollection& items,
    std::uint32_t count) noexcept {
    for (std::uint32_t index = 0U; index < count; ++index) {
        Result<Ref<Aero::Controls::TextBlock>> item =
            Aero::Base::MakeRef<Aero::Controls::TextBlock>();
        if (!item) {
            return false;
        }
        if (!items.Add(Ref<Aero::Base::Object>(item.Value()))) {
            return false;
        }
    }
    return true;
}

bool AnyItemsControlHasCount(
    ::Aero::Media::Visual& node,
    std::uint32_t expected) noexcept {
    if (Aero::Controls::ItemsControl* items =
            TryCast<Aero::Controls::ItemsControl>(&node)) {
        static_cast<void>(items->ApplyTemplate());
        if (items->GetCount() == expected) {
            return true;
        }
    }
    const std::uint32_t count = VisualTreeHelper::GetChildrenCount(node);
    for (std::uint32_t index = 0U; index < count; ++index) {
        ::Aero::Media::Visual* child =
            VisualTreeHelper::GetChild(node, index);
        if (child != nullptr &&
            AnyItemsControlHasCount(*child, expected)) {
            return true;
        }
    }
    return false;
}

bool AssertNamedItemsCount(
    FrameworkElement& root,
    const char* name,
    std::uint32_t expected) noexcept {
    Aero::Controls::ItemsControl* list =
        root.FindName<Aero::Controls::ItemsControl>(CStringView(name));
    if (list == nullptr) {
        std::fprintf(stderr, "ItemsControl '%s' was not found\n", name);
        return false;
    }
    static_cast<void>(list->ApplyTemplate());
    if (list->GetCount() != expected) {
        const Aero::Meta::PropertyValue dc = list->GetDataContext();
        std::fprintf(
            stderr,
            "ItemsControl '%s' GetCount=%u expected=%u realized=%u "
            "dcNull=%d dcType=%u itemsSource=%d list=%p\n",
            name,
            list->GetCount(),
            expected,
            list->GetRealizedItemCount(),
            dc.IsNullObject(),
            static_cast<unsigned>(
                dc.AsObject() ? dc.AsObject()->RuntimeType() : 0U),
            list->GetItemsSource().Get() != nullptr,
            static_cast<void*>(list));
        const Aero::Meta::PropertyValue localItems =
            list->ReadLocalValue(
                Aero::Controls::ItemsControl::ItemsSourceProperty);
        const Aero::Meta::PropertyValueSourceInfo sourceInfo =
            list->GetValueSourceInfo(
                Aero::Controls::ItemsControl::ItemsSourceProperty);
        std::fprintf(
            stderr,
            "  itemsLocalUnset=%d itemsSourceRank=%u itemsCount=%u\n",
            localItems.IsUnset(),
            static_cast<unsigned>(sourceInfo.rank),
            list->GetItems().GetCount());
        const Aero::DependencyObject* node = list;
        for (std::uint32_t depth = 0U; node != nullptr && depth < 16U; ++depth) {
            const FrameworkElement* fe = TryCast<const FrameworkElement>(node);
            const Aero::Meta::PropertyValue nodeDc =
                fe != nullptr ? fe->GetDataContext() : Aero::Meta::PropertyValue{};
            std::fprintf(
                stderr,
                "  ancestor[%u] type=%u dcNull=%d logical=%p visual=%p root=%d\n",
                depth,
                static_cast<unsigned>(node->RuntimeType()),
                fe != nullptr && nodeDc.IsNullObject(),
                static_cast<const void*>(
                    Aero::LogicalTreeHelper::GetParent(*node)),
                fe != nullptr
                    ? static_cast<const void*>(
                          VisualTreeHelper::GetParent(*fe))
                    : nullptr,
                fe == &root);
            node = Aero::LogicalTreeHelper::GetParent(*node);
        }
        return false;
    }
    return true;
}

bool ApplySampleItemsDataContext(
    LiveGui& live,
    const char* sample) noexcept {
    FrameworkElement* root =
        live.view ? live.view->GetContent() : nullptr;
    if (root == nullptr) {
        return true;
    }
    constexpr std::uint32_t kCount = 4U;
    if (std::strcmp(sample, "Inventory") == 0) {
        Result<Ref<InventoryViewModel>> model =
            Aero::Base::MakeRef<InventoryViewModel>();
        SAMPLE_CHECK(model);
        Result<Ref<Aero::Collections::ObservableObjectCollection>> inventory =
            Aero::Base::MakeRef<Aero::Collections::ObservableObjectCollection>();
        SAMPLE_CHECK(inventory);
        SAMPLE_CHECK(FillObjectCollection(*inventory.Value(), kCount));
        model.Value()->SetInventory(inventory.Value());
        Result<Ref<InventoryPlayer>> player =
            Aero::Base::MakeRef<InventoryPlayer>();
        SAMPLE_CHECK(player);
        Result<Ref<Aero::Collections::ObservableObjectCollection>> slots =
            Aero::Base::MakeRef<Aero::Collections::ObservableObjectCollection>();
        SAMPLE_CHECK(slots);
        SAMPLE_CHECK(FillObjectCollection(*slots.Value(), 8U));
        player.Value()->SetSlots(slots.Value());
        model.Value()->SetValue(
            InventoryViewModel::PlayerProperty, player.Value());
        root->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        SAMPLE_CHECK(PumpSample(live));
        SAMPLE_CHECK(PumpSample(live));
        if (!AssertNamedItemsCount(*root, "InventoryList", kCount)) {
            DumpDiagnostics(live.diagnostics);
            return false;
        }
        return true;
    }
    if (std::strcmp(sample, "DataBinding") == 0) {
        Result<Ref<SolarSystem>> model = Aero::Base::MakeRef<SolarSystem>();
        SAMPLE_CHECK(model);
        Result<Ref<Aero::Collections::ObservableObjectCollection>> objects =
            Aero::Base::MakeRef<Aero::Collections::ObservableObjectCollection>();
        SAMPLE_CHECK(objects);
        SAMPLE_CHECK(FillObjectCollection(*objects.Value(), kCount));
        model.Value()->SetSolarSystemObjects(objects.Value());
        root->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        SAMPLE_CHECK(PumpSample(live));
        SAMPLE_CHECK(PumpSample(live));
        if (!AnyItemsControlHasCount(*root, kCount)) {
            std::fprintf(
                stderr,
                "DataBinding DC kind=%d null=%d type=%u\n",
                static_cast<int>(root->GetDataContext().Kind()),
                root->GetDataContext().IsNullObject(),
                static_cast<unsigned>(
                    root->GetDataContext().AsObject()
                    ? root->GetDataContext().AsObject()->RuntimeType()
                    : 0U));
            DumpDiagnostics(live.diagnostics);
            return false;
        }
        return true;
    }
    if (std::strcmp(sample, "QuestLog") == 0) {
        Result<Ref<QuestLogViewModel>> model =
            Aero::Base::MakeRef<QuestLogViewModel>();
        SAMPLE_CHECK(model);
        Result<Ref<Aero::Collections::ObservableObjectCollection>> quests =
            Aero::Base::MakeRef<Aero::Collections::ObservableObjectCollection>();
        SAMPLE_CHECK(quests);
        SAMPLE_CHECK(FillObjectCollection(*quests.Value(), kCount));
        model.Value()->SetQuests(quests.Value());
        root->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        SAMPLE_CHECK(PumpSample(live));
        SAMPLE_CHECK(PumpSample(live));
        SAMPLE_CHECK(AssertNamedItemsCount(*root, "Quests", kCount));
        return true;
    }
    if (std::strcmp(sample, "Scoreboard") == 0) {
        Result<Ref<ScoreboardGame>> model =
            Aero::Base::MakeRef<ScoreboardGame>();
        SAMPLE_CHECK(model);
        SAMPLE_CHECK(model.Value()->GetPlayers());
        SAMPLE_CHECK(FillObjectCollection(*model.Value()->GetPlayers(), kCount));
        root->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        SAMPLE_CHECK(PumpSample(live));
        SAMPLE_CHECK(PumpSample(live));
        SAMPLE_CHECK(AssertNamedItemsCount(*root, "Players", kCount));
        return true;
    }
    if (std::strcmp(sample, "Localization") == 0) {
        Result<Ref<LocalizationViewModel>> model =
            Aero::Base::MakeRef<LocalizationViewModel>();
        SAMPLE_CHECK(model);
        Result<Ref<Aero::Collections::ObservableObjectCollection>> languages =
            Aero::Base::MakeRef<Aero::Collections::ObservableObjectCollection>();
        SAMPLE_CHECK(languages);
        SAMPLE_CHECK(FillObjectCollection(*languages.Value(), kCount));
        model.Value()->SetLanguages(languages.Value());
        root->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        SAMPLE_CHECK(PumpSample(live));
        SAMPLE_CHECK(PumpSample(live));
        SAMPLE_CHECK(AssertNamedItemsCount(*root, "LanguageSelector", kCount));
        return true;
    }
    return true;
}

bool ApplyOneControlTemplate(
    LiveGui& live,
    Aero::Controls::ControlTemplate& tmpl,
    StringView where) {
    const Aero::Meta::TypeId target = tmpl.GetTargetType();
    const char* hostXaml = nullptr;
    if (target == Aero::Meta::InvalidTypeId ||
        target == ContentControl::StaticTypeId() ||
        target == Control::StaticTypeId()) {
        hostXaml =
            "<ContentControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " Width=\"48\" Height=\"48\"/>";
    } else if (target == Aero::Controls::Primitives::RepeatButton::StaticTypeId()) {
        hostXaml =
            "<RepeatButton xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " Width=\"16\" Height=\"16\"/>";
    } else if (target == Aero::Controls::Primitives::Thumb::StaticTypeId()) {
        hostXaml =
            "<Thumb xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " Width=\"16\" Height=\"32\"/>";
    } else if (target == Aero::Controls::Primitives::ScrollBar::StaticTypeId()) {
        hostXaml =
            "<ScrollBar xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " Width=\"16\" Height=\"80\"/>";
    } else if (target == Aero::Controls::TextBox::StaticTypeId() ||
               target == Aero::Controls::Primitives::TextBoxBase::StaticTypeId()) {
        hostXaml =
            "<TextBox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " Width=\"80\" Height=\"24\"/>";
    } else if (target == Aero::Controls::PasswordBox::StaticTypeId()) {
        hostXaml =
            "<PasswordBox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " Width=\"80\" Height=\"24\"/>";
    } else if (target == Aero::Controls::Button::StaticTypeId() ||
               target == Aero::Controls::Primitives::ButtonBase::StaticTypeId()) {
        hostXaml =
            "<Button xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " Width=\"48\" Height=\"24\" Content=\"A\"/>";
    } else if (target == Aero::Controls::Primitives::ToggleButton::StaticTypeId() ||
               target == Aero::Controls::CheckBox::StaticTypeId()) {
        hostXaml =
            "<CheckBox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " Width=\"48\" Height=\"24\"/>";
    } else if (target == Aero::Controls::RadioButton::StaticTypeId()) {
        hostXaml =
            "<RadioButton xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " Width=\"48\" Height=\"24\"/>";
    } else if (target == Label::StaticTypeId()) {
        hostXaml =
            "<Label xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " Content=\"12\"/>";
    } else if (target == ScrollViewer::StaticTypeId()) {
        hostXaml =
            "<ScrollViewer xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " Width=\"80\" Height=\"80\"><Border Width=\"40\" Height=\"120\"/></ScrollViewer>";
    } else if (target == ProgressBar::StaticTypeId()) {
        hostXaml =
            "<ProgressBar xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " Width=\"120\" Height=\"16\" Value=\"40\" Maximum=\"100\"/>";
    } else if (target == Aero::Controls::Slider::StaticTypeId()) {
        hostXaml =
            "<Slider xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " Width=\"120\" Height=\"24\" Value=\"20\" Maximum=\"100\"/>";
    }
    if (hostXaml == nullptr) {
        return true;
    }
    Aero::Markup::XamlReader reader(live.gui);
    live.diagnostics.Clear();
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        CStringView(hostXaml), {}, {}, &live.diagnostics);
    if (FailIfSampleError(where, document.GetStatus(), live.diagnostics)) {
        return false;
    }
    if (!live.view) {
        return true;
    }
    Result<void> mounted = live.view->SetContent(
        std::move(document).Value(), {640.0, 480.0});
    if (FailIfSampleError(where, mounted.GetStatus(), live.diagnostics)) {
        return false;
    }
    Control* control = TryCast<Control>(live.view->GetContent());
    if (control == nullptr) {
        return true;
    }
    control->SetValue(
        Control::TemplateProperty,
        Ref<Aero::Controls::ControlTemplate>::FromBorrowed(tmpl));
    static_cast<void>(control->ApplyTemplate());
    if (FailIfSampleError(where, Aero::Base::Status::Ok(), live.diagnostics)) {
        return false;
    }
    if (!PumpSample(live)) {
        return false;
    }
    if (DiagnosticsReportSampleFailure(live.diagnostics)) {
        std::fprintf(stderr, "%.*s host Update diagnostics contain sample load failure\n",
            static_cast<int>(where.SizeBytes()),
            where.Data());
        DumpDiagnostics(live.diagnostics);
        return false;
    }
    return true;
}

bool ApplyControlTemplatesInDictionary(
    LiveGui& live,
    Aero::ResourceDictionary& dictionary,
    StringView where,
    std::uint32_t depth) {
    if (depth > 8U) {
        return true;
    }
    for (std::uint32_t index = 0U; index < dictionary.Size(); ++index) {
        Result<Aero::ResourceEntrySnapshot> entry = dictionary.EntryAt(index);
        if (!entry) {
            continue;
        }
        const Aero::ResourceValue& value = entry.Value().value;
        if (value.Kind() != Aero::Meta::ValueKind::Object ||
            value.IsNullObject() ||
            !value.AsObject()) {
            continue;
        }
        Aero::Base::Object* object = value.AsObject().Get();
        if (Aero::Controls::ControlTemplate* tmpl =
                TryCast<Aero::Controls::ControlTemplate>(object)) {
            if (!ApplyOneControlTemplate(live, *tmpl, where)) {
                return false;
            }
        }
        if (Aero::ResourceDictionary* nested =
                TryCast<Aero::ResourceDictionary>(object)) {
            if (!ApplyControlTemplatesInDictionary(
                    live, *nested, where, depth + 1U)) {
                return false;
            }
        }
    }
    return true;
}

bool ApplyDictionaryTemplates(
    LiveGui& live,
    Aero::ResourceDictionary& dictionary,
    StringView where) {
    if (!ApplyControlTemplatesInDictionary(live, dictionary, where, 0U)) {
        return false;
    }
    struct NamedTemplate {
        const char* key;
        const char* hostXaml;
    };
    const NamedTemplate hosts[] = {
        {"Template.DraggedItem",
         "<ContentControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
         " Width=\"48\" Height=\"48\"/>"},
        {"Template.StatsValue",
         "<Label xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
         " Content=\"12\"/>"},
        {"Template.ScrollViewer",
         "<ScrollViewer xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
         " Width=\"80\" Height=\"80\"><Border Width=\"40\" Height=\"120\"/></ScrollViewer>"},
        {"Template.ProgressBar",
         "<ProgressBar xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
         " Width=\"120\" Height=\"16\" Value=\"40\" Maximum=\"100\"/>"}};
    Aero::Markup::XamlReader reader(live.gui);
    for (const NamedTemplate& host : hosts) {
        if (!dictionary.Contains(CStringView(host.key))) {
            continue;
        }
        Result<Aero::ResourceValue> resource =
            dictionary.Lookup(CStringView(host.key));
        if (!resource) {
            continue;
        }
        live.diagnostics.Clear();
        Result<Aero::Markup::XamlDocument> document = reader.Parse(
            CStringView(host.hostXaml), {}, {}, &live.diagnostics);
        if (FailIfSampleError(
                CStringView(host.key),
                document.GetStatus(),
                live.diagnostics)) {
            return false;
        }
        if (!live.view) {
            continue;
        }
        Result<void> mounted = live.view->SetContent(
            std::move(document).Value(), {640.0, 480.0});
        if (FailIfSampleError(
                CStringView(host.key),
                mounted.GetStatus(),
                live.diagnostics)) {
            return false;
        }
        Control* control = TryCast<Control>(live.view->GetContent());
        if (control == nullptr) {
            continue;
        }
        if (resource.Value().Kind() == Aero::Meta::ValueKind::Object &&
            resource.Value().AsObject()) {
            control->SetValue(
                Control::TemplateProperty,
                resource.Value());
        }
        static_cast<void>(control->ApplyTemplate());
        if (FailIfSampleError(
                CStringView(host.key),
                Aero::Base::Status::Ok(),
                live.diagnostics)) {
            return false;
        }
        if (!PumpSample(live)) {
            return false;
        }
        if (DiagnosticsReportSampleFailure(live.diagnostics)) {
            std::fprintf(stderr, "%s host Update diagnostics contain sample load failure\n",
                host.key);
            DumpDiagnostics(live.diagnostics);
            return false;
        }
        static_cast<void>(where);
    }
    return true;
}

bool LoadDocumentAtPath(
    LiveGui& live,
    const std::filesystem::path& path,
    StringView where,
    bool skipNativeHost,
    bool applyResourceTemplates) {
    live.diagnostics.Clear();
    Aero::Markup::XamlReader reader(live.gui);
    const std::string pathText = path.string();
    Result<Aero::Markup::XamlDocument> document =
        live.application != nullptr
        ? reader.Load(
              AsView(pathText),
              live.application->GetResources(),
              {},
              &live.diagnostics)
        : reader.Load(
              AsView(pathText), {}, &live.diagnostics);
    if (FailIfSampleError(where, document.GetStatus(), live.diagnostics)) {
        return false;
    }
    SAMPLE_CHECK(document.Value().IsValid());
    if (Aero::Application* app =
            document.Value().Root<Aero::Application>()) {
        live.applicationDocument = std::move(document).Value();
        live.application = live.applicationDocument.Root<Aero::Application>();
        if (applyResourceTemplates) {
            Aero::ResourceDictionary* dictionary =
                live.application != nullptr
                ? &live.application->GetResources()
                : nullptr;
            if (dictionary != nullptr &&
                !ApplyDictionaryTemplates(live, *dictionary, where)) {
                return false;
            }
        }
        return true;
    }
    if (applyResourceTemplates) {
        Aero::ResourceDictionary* dictionary =
            document.Value().Root<Aero::ResourceDictionary>();
        if (dictionary != nullptr) {
            if (!ApplyDictionaryTemplates(live, *dictionary, where)) {
                return false;
            }
        }
    }
    return MountAndLayout(
        live, std::move(document).Value(), where, skipNativeHost);
}

} // namespace

bool TestStyleSetterMergedStaticResource() {
    LiveGui* live = NewSampleLiveGui();
    SAMPLE_CHECK(live != nullptr);

    std::error_code error;
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path(error) /
        "aero-style-static-f940";
    SAMPLE_CHECK(!error);
    SAMPLE_CHECK(WriteUtf8File(
        dir / "Palette.xaml",
        CStringView(
            "<ResourceDictionary xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " xmlns:sys=\"clr-namespace:System;assembly=mscorlib\">"
            "<sys:Double x:Key=\"Size.ScrollBar\">17</sys:Double>"
            "</ResourceDictionary>")));
    SAMPLE_CHECK(WriteUtf8File(
        dir / "Styles.xaml",
        CStringView(
            "<ResourceDictionary xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
            "<Style x:Key=\"Style.LineButton\" TargetType=\"RepeatButton\">"
            "<Setter Property=\"Height\" Value=\"{StaticResource Size.ScrollBar}\"/>"
            "<Setter Property=\"Template\">"
            "<Setter.Value>"
            "<ControlTemplate TargetType=\"RepeatButton\">"
            "<Border Background=\"Transparent\"/>"
            "</ControlTemplate>"
            "</Setter.Value>"
            "</Setter>"
            "</Style>"
            "</ResourceDictionary>")));
    SAMPLE_CHECK(WriteUtf8File(
        dir / "AppResources.xaml",
        CStringView(
            "<ResourceDictionary xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
            "<ResourceDictionary.MergedDictionaries>"
            "<ResourceDictionary Source=\"Palette.xaml\"/>"
            "<ResourceDictionary Source=\"Styles.xaml\"/>"
            "</ResourceDictionary.MergedDictionaries>"
            "</ResourceDictionary>")));

    Aero::Markup::XamlReader reader(live->gui);
    live->diagnostics.Clear();
    const std::string mergedPath = (dir / "AppResources.xaml").string();
    Result<Aero::Markup::XamlDocument> merged = reader.Load(
        AsView(mergedPath), {}, &live->diagnostics);
    SAMPLE_CHECK(!FailIfSampleError(
        StringView("Style Setter merged StaticResource"),
        merged.GetStatus(),
        live->diagnostics));
    SAMPLE_CHECK(merged);
    Aero::ResourceDictionary* resources =
        merged.Value().Root<Aero::ResourceDictionary>();
    SAMPLE_CHECK(resources != nullptr);
    SAMPLE_CHECK(resources->Contains(StringView("Style.LineButton")));

    live->diagnostics.Clear();
    const std::string stylesPath = (dir / "Styles.xaml").string();
    Result<Aero::Markup::XamlDocument> styles = reader.Load(
        AsView(stylesPath),
        *resources,
        {},
        &live->diagnostics);
    SAMPLE_CHECK(!FailIfSampleError(
        StringView("Style Setter fallback StaticResource"),
        styles.GetStatus(),
        live->diagnostics));
    SAMPLE_CHECK(styles);
    Aero::ResourceDictionary* styleDictionary =
        styles.Value().Root<Aero::ResourceDictionary>();
    SAMPLE_CHECK(styleDictionary != nullptr);
    SAMPLE_CHECK(ApplyDictionaryTemplates(
        *live, *styleDictionary, StringView("Style Setter templates")));
    return true;
}

bool TestInventoryTemplateApply() {
    LiveGui* live = NewSampleLiveGui();
    SAMPLE_CHECK(live != nullptr);
    Aero::Markup::XamlReader reader(live->gui);
    Aero::Diagnostics::DiagnosticBag& diagnostics = live->diagnostics;

    const char* unnamedImage =
        "<ContentControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
        " Width=\"64\" Height=\"64\">"
        "<ContentControl.Template>"
        "<ControlTemplate TargetType=\"ContentControl\">"
        "<Viewbox>"
        "<Border>"
        "<Image Source=\"{Binding Content.Icon, RelativeSource={RelativeSource TemplatedParent}}\" Margin=\"15\"/>"
        "</Border>"
        "</Viewbox>"
        "</ControlTemplate>"
        "</ContentControl.Template>"
        "</ContentControl>";
    diagnostics.Clear();
    Result<Aero::Markup::XamlDocument> dragged = reader.Parse(
        CStringView(unnamedImage), {}, {}, &diagnostics);
    SAMPLE_CHECK(!FailIfSampleError(
        StringView("Inventory unnamed Image"),
        dragged.GetStatus(),
        diagnostics));
    SAMPLE_CHECK(dragged);
    SAMPLE_CHECK(MountAndLayout(
        *live, std::move(dragged).Value(),
        StringView("Inventory unnamed Image"), false));

    const char* statsValue =
        "<Label xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
        " xmlns:local=\"clr-namespace:Inventory\" Content=\"7\">"
        "<Label.Template>"
        "<ControlTemplate TargetType=\"Label\">"
        "<Viewbox>"
        "<local:AnimatedNumber x:Name=\"Num\""
        " Number=\"{Binding Content, RelativeSource={RelativeSource TemplatedParent}}\""
        " AnimationDuration=\"0:0:0.3\"/>"
        "</Viewbox>"
        "</ControlTemplate>"
        "</Label.Template>"
        "</Label>";
    diagnostics.Clear();
    Result<Aero::Markup::XamlDocument> stats = reader.Parse(
        CStringView(statsValue), {}, {}, &diagnostics);
    SAMPLE_CHECK(!FailIfSampleError(
        StringView("Inventory StatsValue"),
        stats.GetStatus(),
        diagnostics));
    SAMPLE_CHECK(stats);
    SAMPLE_CHECK(MountAndLayout(
        *live, std::move(stats).Value(),
        StringView("Inventory StatsValue"), false));

    const char* scrollViewer =
        "<ScrollViewer xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
        " Width=\"80\" Height=\"80\">"
        "<ScrollViewer.Template>"
        "<ControlTemplate TargetType=\"ScrollViewer\">"
        "<Grid>"
        "<ScrollBar x:Name=\"PART_VerticalScrollBar\""
        " Value=\"{Binding VerticalOffset, Mode=OneWay, RelativeSource={RelativeSource TemplatedParent}}\""
        " ViewportSize=\"{TemplateBinding ViewportHeight}\""
        " Maximum=\"{TemplateBinding ScrollableHeight}\"/>"
        "<ScrollContentPresenter Content=\"{TemplateBinding Content}\"/>"
        "</Grid>"
        "</ControlTemplate>"
        "</ScrollViewer.Template>"
        "<Border Width=\"40\" Height=\"120\"/>"
        "</ScrollViewer>";
    diagnostics.Clear();
    Result<Aero::Markup::XamlDocument> scroller = reader.Parse(
        CStringView(scrollViewer), {}, {}, &diagnostics);
    SAMPLE_CHECK(!FailIfSampleError(
        StringView("Inventory ScrollViewer"),
        scroller.GetStatus(),
        diagnostics));
    SAMPLE_CHECK(scroller);
    SAMPLE_CHECK(MountAndLayout(
        *live, std::move(scroller).Value(),
        StringView("Inventory ScrollViewer"), false));

    const char* slotTrigger =
        "<ContentControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" Width=\"40\" Height=\"40\">"
        "<ContentControl.Template>"
        "<ControlTemplate TargetType=\"ContentControl\">"
        "<Grid x:Name=\"Root\" Background=\"Transparent\">"
        "<Border Width=\"{Binding ActualWidth, Mode=OneWay, RelativeSource={RelativeSource Self}}\"/>"
        "</Grid>"
        "<ControlTemplate.Triggers>"
        "<MultiDataTrigger>"
        "<MultiDataTrigger.Conditions>"
        "<Condition Binding=\"{Binding TemplatedParent.IsKeyboardFocused, ElementName=Root}\" Value=\"True\"/>"
        "</MultiDataTrigger.Conditions>"
        "<Setter Property=\"Opacity\" Value=\"0.5\"/>"
        "</MultiDataTrigger>"
        "</ControlTemplate.Triggers>"
        "</ControlTemplate>"
        "</ContentControl.Template>"
        "</ContentControl>";
    diagnostics.Clear();
    Result<Aero::Markup::XamlDocument> slot = reader.Parse(
        CStringView(slotTrigger), {}, {}, &diagnostics);
    SAMPLE_CHECK(!FailIfSampleError(
        StringView("Inventory MultiDataTrigger"),
        slot.GetStatus(),
        diagnostics));
    SAMPLE_CHECK(slot);
    SAMPLE_CHECK(MountAndLayout(
        *live, std::move(slot).Value(),
        StringView("Inventory MultiDataTrigger"), false));

    const char* progress =
        "<ProgressBar xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
        " Width=\"120\" Height=\"16\" Value=\"40\" Maximum=\"100\""
        " Background=\"#FF333333\" Foreground=\"#FF86C232\">"
        "<ProgressBar.Template>"
        "<ControlTemplate TargetType=\"ProgressBar\">"
        "<Border>"
        "<Border.Background>"
        "<LinearGradientBrush StartPoint=\"0,0\" EndPoint=\"1,0\">"
        "<GradientStop Color=\"{Binding Background.Color, RelativeSource={RelativeSource TemplatedParent}}\" Offset=\"0\"/>"
        "<GradientStop Color=\"{Binding Foreground.Color, RelativeSource={RelativeSource TemplatedParent}}\" Offset=\"0.5\"/>"
        "<GradientStop Color=\"{Binding Background.Color, RelativeSource={RelativeSource TemplatedParent}}\" Offset=\"1\"/>"
        "</LinearGradientBrush>"
        "</Border.Background>"
        "</Border>"
        "</ControlTemplate>"
        "</ProgressBar.Template>"
        "</ProgressBar>";
    diagnostics.Clear();
    Result<Aero::Markup::XamlDocument> bar = reader.Parse(
        CStringView(progress), {}, {}, &diagnostics);
    SAMPLE_CHECK(!FailIfSampleError(
        StringView("ProgressBar GradientStop"),
        bar.GetStatus(),
        diagnostics));
    SAMPLE_CHECK(bar);
    SAMPLE_CHECK(MountAndLayout(
        *live, std::move(bar).Value(),
        StringView("ProgressBar GradientStop"), false));

    diagnostics.Clear();
    Result<Aero::Markup::XamlDocument> theme = reader.Load(
        StringView(
            "pack://application:,,,/Aero.GUI.Extensions;component/Theme/AeroTheme.DarkBlue.xaml"),
        {},
        &diagnostics);
    SAMPLE_CHECK(!FailIfSampleError(
        StringView("AeroTheme.DarkBlue"),
        theme.GetStatus(),
        diagnostics));
    SAMPLE_CHECK(theme);
    Result<Ref<ProgressBar>> themedBar = Aero::Base::MakeRef<ProgressBar>();
    SAMPLE_CHECK(themedBar);
    Result<void> mountedTheme = live->view->SetContent(
        Ref<FrameworkElement>(themedBar.Value()), {120.0, 16.0});
    SAMPLE_CHECK(!FailIfSampleError(
        StringView("themed ProgressBar"),
        mountedTheme.GetStatus(),
        diagnostics));
    static_cast<void>(themedBar.Value()->ApplyTemplate());
    SAMPLE_CHECK(PumpSample(*live));
    SAMPLE_CHECK(!DiagnosticsReportSampleFailure(diagnostics));
    return true;
}

bool TestWindowGridItemsBinding(LiveGui& live) noexcept {
    live.diagnostics.Clear();
    Aero::Markup::XamlReader reader(live.gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(
            "<Window xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " Width=\"240\" Height=\"160\">"
            "<Grid>"
            "<ItemsControl x:Name=\"List\" ItemsSource=\"{Binding Inventory}\"/>"
            "</Grid>"
            "</Window>"),
        {},
        {},
        &live.diagnostics);
    SAMPLE_CHECK(document);
    SAMPLE_CHECK(MountAndLayout(
        live,
        std::move(document).Value(),
        StringView("Window-Grid ItemsSource"),
        false));
    Result<Ref<InventoryViewModel>> model =
        Aero::Base::MakeRef<InventoryViewModel>();
    SAMPLE_CHECK(model);
    Result<Ref<Aero::Collections::ObservableObjectCollection>> inventory =
        Aero::Base::MakeRef<Aero::Collections::ObservableObjectCollection>();
    SAMPLE_CHECK(inventory);
    SAMPLE_CHECK(FillObjectCollection(*inventory.Value(), 4U));
    model.Value()->SetInventory(inventory.Value());
    FrameworkElement* root = live.view->GetContent();
    SAMPLE_CHECK(root != nullptr);
    root->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
    SAMPLE_CHECK(PumpSample(live));
    SAMPLE_CHECK(PumpSample(live));
    SAMPLE_CHECK(AssertNamedItemsCount(*root, "List", 4U));

    {
        live.diagnostics.Clear();
        Aero::Markup::XamlReader nestedReader(live.gui);
        Result<Aero::Markup::XamlDocument> nested = nestedReader.Parse(
            StringView(
                "<Window xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
                " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
                " Width=\"240\" Height=\"160\">"
                "<Grid>"
                "<Grid.Resources>"
                "<ControlTemplate x:Key=\"SV\" TargetType=\"ScrollViewer\">"
                "<ScrollContentPresenter Content=\"{TemplateBinding Content}\"/>"
                "</ControlTemplate>"
                "</Grid.Resources>"
                "<ScrollViewer Template=\"{StaticResource SV}\">"
                "<ItemsControl x:Name=\"NestedList\" ItemsSource=\"{Binding Inventory}\">"
                "<ItemsControl.ItemsPanel>"
                "<ItemsPanelTemplate><UniformGrid Columns=\"5\"/></ItemsPanelTemplate>"
                "</ItemsControl.ItemsPanel>"
                "</ItemsControl>"
                "</ScrollViewer>"
                "</Grid>"
                "</Window>"),
            {},
            {},
            &live.diagnostics);
        SAMPLE_CHECK(nested);
        SAMPLE_CHECK(MountAndLayout(
            live,
            std::move(nested).Value(),
            StringView("Window-ScrollViewer ItemsSource"),
            false));
        Result<Ref<InventoryViewModel>> nestedModel =
            Aero::Base::MakeRef<InventoryViewModel>();
        SAMPLE_CHECK(nestedModel);
        Result<Ref<Aero::Collections::ObservableObjectCollection>> nestedItems =
            Aero::Base::MakeRef<Aero::Collections::ObservableObjectCollection>();
        SAMPLE_CHECK(nestedItems);
        SAMPLE_CHECK(FillObjectCollection(*nestedItems.Value(), 4U));
        nestedModel.Value()->SetInventory(nestedItems.Value());
        FrameworkElement* nestedRoot = live.view->GetContent();
        SAMPLE_CHECK(nestedRoot != nullptr);
        nestedRoot->SetDataContext(Ref<Aero::Base::Object>(nestedModel.Value()));
        SAMPLE_CHECK(PumpSample(live));
        SAMPLE_CHECK(PumpSample(live));
        SAMPLE_CHECK(AssertNamedItemsCount(*nestedRoot, "NestedList", 4U));
    }
    return true;
}

bool TestTutorialSampleXamlLoadApply() {
    LiveGui* live = NewSampleLiveGui();
    SAMPLE_CHECK(live != nullptr);
    SAMPLE_CHECK(TestWindowGridItemsBinding(*live));

    std::error_code error;
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path(error) /
        "aero-tutorial-samples-f940";
    SAMPLE_CHECK(!error);
    std::filesystem::create_directories(dir, error);
    SAMPLE_CHECK(!error);

    for (std::uint32_t index = 0U;
         index < kTutorialSampleXamlFileCount;
         ++index) {
        const TutorialSampleXamlFile& file = kTutorialSampleXamlFiles[index];
        const std::filesystem::path path =
            dir / file.sample / file.relative;
        SAMPLE_CHECK(WriteUtf8File(
            path,
            StringView(
                file.text,
                static_cast<std::uint32_t>(std::strlen(file.text)))));
    }

    const char* required[] = {
        "ApplicationTutorial", "BackgroundBlur", "BlendTutorial", "BrushShaders",
        "Buttons", "Commands", "CustomAnimation", "CustomControl", "CustomRender",
        "DataBinding", "HelloWorld", "Integration", "IntegrationGLUT", "Inventory",
        "Localization", "Login", "Menu3D", "QuestLog", "Scoreboard", "TicTacToe",
        "UserControl", "ControlGallery"};
    bool sawIntegrationGlut = false;
    for (const char* sample : required) {
        {
            Aero::Markup::XamlReader reader(live->gui);
            live->diagnostics.Clear();
            Result<Aero::Markup::XamlDocument> empty = reader.Parse(
                StringView(
                    "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\"/>"),
                {},
                {},
                &live->diagnostics);
            if (empty) {
                live->sampleDocument = {};
                static_cast<void>(live->view->SetContent(
                    std::move(empty).Value(), {640.0, 480.0}));
            }
        }
        live->applicationDocument = Aero::Markup::XamlDocument{};
        live->application = nullptr;
        const std::filesystem::path sampleDir = dir / sample;
        if (!std::filesystem::exists(sampleDir)) {
            if (std::strcmp(sample, "IntegrationGLUT") == 0) {
                sawIntegrationGlut = true;
                live->diagnostics.Clear();
                Aero::Markup::XamlReader reader(live->gui);
                Result<Aero::Markup::XamlDocument> empty = reader.Parse(
                    StringView(
                        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\"/>"),
                    {},
                    {},
                    &live->diagnostics);
                SAMPLE_CHECK(!FailIfSampleError(
                    StringView("IntegrationGLUT"),
                    empty.GetStatus(),
                    live->diagnostics));
                continue;
            }
            std::fprintf(stderr, "missing sample tree %s\n", sample);
            return false;
        }
        const bool skipNativeHost =
            std::strcmp(sample, "Integration") == 0 ||
            std::strcmp(sample, "IntegrationGLUT") == 0;
        const char* primary[] = {
            "App.xaml", "Resources.xaml", "MainWindow.xaml", "Settings.xaml"};
        for (const char* name : primary) {
            const std::filesystem::path path = sampleDir / name;
            if (!std::filesystem::exists(path)) {
                continue;
            }
            std::string whereText = std::string(sample) + "/" + name;
            if (!LoadDocumentAtPath(
                    *live,
                    path,
                    AsView(whereText),
                    skipNativeHost,
                    std::strcmp(name, "Resources.xaml") == 0 ||
                        std::strcmp(name, "App.xaml") == 0)) {
                return false;
            }
            if (std::strcmp(name, "MainWindow.xaml") == 0 &&
                !ApplySampleItemsDataContext(*live, sample)) {
                return false;
            }
        }
        if (std::strcmp(sample, "ControlGallery") == 0) {
            const std::filesystem::path pages = sampleDir / "Samples";
            if (std::filesystem::exists(pages)) {
                for (const auto& entry :
                     std::filesystem::directory_iterator(pages)) {
                    if (!entry.is_regular_file() ||
                        entry.path().extension() != ".xaml") {
                        continue;
                    }
                    const std::string whereText =
                        std::string("ControlGallery/") +
                        entry.path().filename().string();
                    if (!LoadDocumentAtPath(
                            *live,
                            entry.path(),
                            AsView(whereText),
                            false,
                            false)) {
                        return false;
                    }
                }
            }
        } else {
            for (const auto& entry :
                 std::filesystem::recursive_directory_iterator(sampleDir)) {
                if (!entry.is_regular_file() ||
                    entry.path().extension() != ".xaml") {
                    continue;
                }
                const std::string fileName = entry.path().filename().string();
                if (fileName == "App.xaml" ||
                    fileName == "Resources.xaml" ||
                    fileName == "MainWindow.xaml" ||
                    fileName == "Settings.xaml") {
                    continue;
                }
                const std::string generic = entry.path().generic_string();
                if (generic.find("/SampleData/") != std::string::npos ||
                    fileName.find("SampleData") != std::string::npos) {
                    continue;
                }
                const std::string whereText =
                    std::string(sample) + "/" + fileName;
                if (!LoadDocumentAtPath(
                        *live,
                        entry.path(),
                        AsView(whereText),
                        skipNativeHost,
                        false)) {
                    return false;
                }
            }
        }
        static_cast<void>(sawIntegrationGlut);
    }
    return true;
}
