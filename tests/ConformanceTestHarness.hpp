#pragma once
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Stream.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Controls/Menus.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/BindingExpression.hpp>
#include <Aero/Data/BindingOperations.hpp>
#include <Aero/Data/CollectionView.hpp>
#include <Aero/Data/NotifyPropertyChanged.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/DependencyObject.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Gui.hpp>
#include <Aero/Input.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Value.hpp>
#include <Aero/Markup/XamlProvider.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/FontProvider.hpp>
#include <Aero/Media/TextureProvider.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Module.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/ApplicationCommands.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Events.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/Media/DrawingContext.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/TextProperties.hpp>
#include <Aero/Threading.hpp>
#include <Aero/Documents/Inlines.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/View.hpp>
#include <Aero/ViewOptions.hpp>
#include <Aero/Visual.hpp>
#include <Aero/VisualStateManager.hpp>
#include <Aero/VisualTreeHelper.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>

namespace Aero {
double MaxAbsCommittedProjectiveM13(const View& view) noexcept;
}

namespace Aero::ConformanceHarness {

using Aero::Base::ErrorCode;
using Aero::Base::MakeRef;
using Aero::Base::Point;
using Aero::Base::Ref;
using Aero::Base::Result;
using Aero::Base::Span;
using Aero::Base::Status;
using Aero::Base::Stream;
using Aero::Base::String;
using Aero::Base::StringView;
using Aero::Base::Vector;
using Aero::Collections::IItemsSource;
using Aero::Collections::ObservableCollection;
using Aero::Collections::ObservableCollectionBase;
using Aero::Collections::ObservableObjectCollection;
using Aero::Controls::Border;
using Aero::Controls::Button;
using Aero::Controls::Canvas;
using Aero::Controls::ComboBox;
using Aero::Controls::ComboBoxItem;
using Aero::Controls::ContentControl;
using Aero::Controls::Expander;
using Aero::Controls::Grid;
using Aero::Controls::ItemsControl;
using Aero::Controls::ListBox;
using Aero::Controls::ListBoxItem;
using Aero::Controls::Panel;
using Aero::Controls::PasswordBox;
using Aero::Controls::StackPanel;
using Aero::Controls::Slider;
using Aero::Controls::TabControl;
using Aero::Controls::TextBlock;
using Aero::Controls::TextBox;
using Aero::Controls::UniformGrid;
using Aero::Controls::UserControl;
using Aero::Controls::Viewbox;
using Aero::Controls::VirtualizingStackPanel;
using Aero::Controls::ScrollViewer;
using Aero::Controls::Menu;
using Aero::Controls::MenuItem;
using Aero::Controls::Primitives::Popup;
using Aero::Documents::Run;
using Aero::Documents::Inline;
using Aero::Media::ScaleTransform;
using Aero::Media::Animation::EasingFunctionBase;
using Aero::Media::Animation::EasingMode;
using Aero::Media::VisualTreeHelper;
using Aero::Controls::ClickMode;
using Aero::Controls::Primitives::RepeatButton;
using Aero::Controls::Primitives::ToggleButton;
using Aero::Input::KeyboardNavigation;
using Aero::Data::BindingExpression;
using Aero::Data::BindingOperations;
using Aero::Data::BindingStatus;
using Aero::Data::CollectionView;
using Aero::Data::CollectionViewSource;
using Aero::Data::IMultiValueConverter;
using Aero::Data::ListSortDirection;
using Aero::Data::NotifyPropertyChanged;
using Aero::DataTemplate;
using Aero::DataTemplateSelector;
using Aero::FrameworkElement;
using Aero::Gui;
using Aero::BlendMode;
using Aero::Input::ApplicationCommands;
using Aero::Input::ICommand;
using Aero::Media::ArcSegment;
using Aero::Media::BezierSegment;
using Aero::Media::CombinedGeometry;
using Aero::Media::CompositeTransform3D;
using Aero::Media::FlattenSink;
using Aero::Media::Geometry;
using Aero::Media::GeometryCombineMode;
using Aero::Media::GeometryGroup;
using Aero::Media::LineGeometry;
using Aero::Media::LineSegment;
using Aero::Media::PathFigure;
using Aero::Media::PathGeometry;
using Aero::Media::Pen;
using Aero::Media::StreamGeometry;
using Aero::Media::StreamGeometryContext;
using Aero::Media::PerspectiveTransform3D;
using Aero::Media::PolyBezierSegment;
using Aero::Media::PolyLineSegment;
using Aero::Media::PolyQuadraticBezierSegment;
using Aero::Media::QuadraticBezierSegment;
using Aero::Media::ShaderEffect;
using Aero::Media::SolidColorBrush;
using Aero::Media::TranslateTransform;
using Aero::Media::Animation::DoubleAnimation;
using Aero::Media::Animation::DoubleAnimationBase;
using Aero::Media::Animation::Duration;
using Aero::Media::Animation::KeyTime;
using Aero::Media::Animation::RepeatBehavior;
using Aero::Media::Animation::Timeline;
using Aero::Media::Animation::TimeSpan;
using Aero::Meta::Registration;
using Aero::Shapes::FillRule;
using Aero::Shapes::Path;
using Aero::Shapes::PenLineCap;
using Aero::Shapes::PenLineJoin;
using Aero::Shapes::Rectangle;
using Aero::Shapes::Shape;
using Aero::Style;
using Aero::Setter;
using Aero::TryCast;
using Aero::TryCastToInterface;
using Aero::UIElement;
using Aero::View;
using Aero::Visibility;
using Aero::ViewOptions;
using Aero::ViewViewport;
using Aero::VisualStateManager;

#define CHECK(expression)                                                     \
    do {                                                                      \
        if (!(expression)) {                                                  \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n",               \
                __FILE__, __LINE__, #expression);                             \
            return false;                                                     \
        }                                                                     \
    } while (false)

#define RUN(test)                                                             \
    do {                                                                      \
        std::fprintf(stderr, "begin %s\n", #test);                            \
        std::fflush(stderr);                                                  \
        if (!(test)()) {                                                      \
            std::fprintf(stderr, "fail %s\n", #test);                         \
            std::fflush(stderr);                                              \
            std::_Exit(1);                                                    \
        }                                                                     \
        std::fprintf(stderr, "pass %s\n", #test);                             \
        std::fflush(stderr);                                                  \
    } while (false)

constexpr bool Near(double left, double right, double epsilon = 1.0e-4) noexcept {
    const double diff = left - right;
    return (diff < 0.0 ? -diff : diff) <= epsilon;
}

inline bool Contains(StringView haystack, StringView needle) noexcept {
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

template<class T, class = void>
struct HasVisualGetTree : std::false_type {};

template<class T>
struct HasVisualGetTree<
    T,
    std::void_t<decltype(std::declval<const T&>().GetTree())>>
    : std::true_type {};

template<class T, class = void>
struct HasAsUIElement : std::false_type {};

template<class T>
struct HasAsUIElement<
    T,
    std::void_t<decltype(std::declval<T&>().AsUIElement())>>
    : std::true_type {};

class MemoryStream final : public Stream {
public:
    MemoryStream(const std::uint8_t* data, std::uint32_t size) noexcept
        : data_(data), size_(size) {}

    bool CanRead() const noexcept override { return true; }
    bool CanSeek() const noexcept override { return true; }

    Result<std::uint32_t> Read(
        Span<std::uint8_t> destination) noexcept override {
        const std::uint32_t remaining = size_ - position_;
        const std::uint32_t count =
            destination.Size() < remaining ? destination.Size() : remaining;
        if (count != 0U) {
            std::memcpy(destination.Data(), data_ + position_, count);
            position_ += count;
        }
        return count;
    }

    Result<std::uint64_t> Position() const noexcept override {
        return static_cast<std::uint64_t>(position_);
    }
    Result<std::uint64_t> Length() const noexcept override {
        return static_cast<std::uint64_t>(size_);
    }
    Result<std::uint64_t> Seek(
        std::int64_t offset,
        Aero::Base::SeekOrigin origin) noexcept override {
        std::int64_t originPosition = 0;
        if (origin == Aero::Base::SeekOrigin::Current) {
            originPosition = static_cast<std::int64_t>(position_);
        } else if (origin == Aero::Base::SeekOrigin::End) {
            originPosition = static_cast<std::int64_t>(size_);
        }
        const std::int64_t next = originPosition + offset;
        if (next < 0 || next > static_cast<std::int64_t>(size_)) {
            return Aero::Base::Status::Failure(
                ErrorCode::OutOfRange, "MemoryStream seek is out of range");
        }
        position_ = static_cast<std::uint32_t>(next);
        return static_cast<std::uint64_t>(position_);
    }

private:
    const std::uint8_t* data_ = nullptr;
    std::uint32_t size_ = 0U;
    std::uint32_t position_ = 0U;
};

struct PointSink final : FlattenSink {
    Vector<Point> points;
    Vector<Point> figureStarts;
    std::uint32_t begins = 0U;
    std::uint32_t ends = 0U;

    void AddPoint(Point point) noexcept override {
        points.PushBack(point);
    }
    void BeginFigure(Point start, bool isClosed) noexcept override {
        (void)isClosed;
        ++begins;
        figureStarts.PushBack(start);
        AddPoint(start);
    }
    void EndFigure(bool isClosed) noexcept override {
        (void)isClosed;
        ++ends;
    }
};

class SilentFontProvider final : public Aero::Media::FontProvider {
public:
    Result<Aero::Media::FontResource> MatchFont(
        const Aero::Base::ResourceUri&,
        StringView,
        Aero::FontWeight&,
        Aero::FontStretch&,
        Aero::FontStyle&) const noexcept override {
        return Aero::Base::Status::Failure(
            ErrorCode::NotFound, "test font provider has no faces");
    }
    bool FamilyExists(
        const Aero::Base::ResourceUri&,
        StringView) const noexcept override {
        return false;
    }
};

class SilentTextureProvider final : public Aero::Media::TextureProvider {
public:
    Result<Aero::Media::TextureResourceInfo> Open(
        const Aero::Base::ResourceUri&) const noexcept override {
        return Aero::Base::Status::Failure(
            ErrorCode::NotFound, "test texture provider has no images");
    }
};

class Person final :
    public Aero::Base::Object,
    public NotifyPropertyChanged<Person> {
    AERO_DECLARE_TYPE(Person, Aero::Base::Object)
public:
    Aero::Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    const String& GetName() const noexcept { return name_; }
    void SetName(String value) noexcept {
        name_ = std::move(value);
        RaisePropertyChanged("Name");
    }

private:
    String name_;
};

class NamedItems final : public Aero::Base::Object, public IItemsSource {
    AERO_DECLARE_TYPE(NamedItems, Aero::Base::Object)
public:
    Aero::Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    std::uint32_t GetCount() const noexcept override {
        return items_.Size();
    }
    Ref<Aero::Base::Object> GetItem(
        std::uint32_t index) const noexcept override {
        return index < items_.Size() ? items_[index] : Ref<Aero::Base::Object>{};
    }
    void AddItemsChanged(
        const Aero::Collections::ItemsChangedHandler& handler) noexcept override {
        changed_.Add(handler);
    }
    bool RemoveItemsChanged(
        const Aero::Collections::ItemsChangedHandler& handler) noexcept override {
        return changed_.Remove(handler);
    }
    Aero::Base::Object* AsObject() noexcept override { return this; }
    Result<void> Add(Ref<Aero::Base::Object> item) noexcept {
        items_.PushBack(std::move(item));
        if (!changed_.Empty()) {
            changed_.Invoke({
                Aero::Collections::ItemsChangeAction::Add,
                UINT32_MAX,
                items_.Size() - 1U,
                0U,
                1U});
        }
        return {};
    }

private:
    Vector<Ref<Aero::Base::Object>> items_;
    Aero::Collections::ItemsChangedHandler changed_;
};

class UnregisteredItems final :
    public Aero::Base::Object,
    public IItemsSource {
public:
    Aero::Base::Object* AsObject() noexcept override { return this; }
    std::uint32_t GetCount() const noexcept override { return 3U; }
    Ref<Aero::Base::Object> GetItem(std::uint32_t) const noexcept override {
        return {};
    }
    void AddItemsChanged(
        const Aero::Collections::ItemsChangedHandler&) noexcept override {}
    bool RemoveItemsChanged(
        const Aero::Collections::ItemsChangedHandler&) noexcept override {
        return false;
    }
};

class BindingSlotItem final : public Aero::Base::Object {
    AERO_DECLARE_TYPE(BindingSlotItem, Aero::Base::Object)
public:
    Aero::Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    const String& GetTeam() const noexcept {
        return team_;
    }
    void SetTeam(String value) noexcept {
        team_ = std::move(value);
    }

private:
    String team_{};
};

class BindingPlayer final : public Aero::Base::Object {
    AERO_DECLARE_TYPE(BindingPlayer, Aero::Base::Object)
public:
    Aero::Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Ref<ObservableCollection<BindingSlotItem>> GetSlots() const noexcept {
        return slots_;
    }
    void SetSlots(
        Ref<ObservableCollection<BindingSlotItem>> value) noexcept {
        slots_ = std::move(value);
    }

private:
    Ref<ObservableCollection<BindingSlotItem>> slots_{};
};

class BindingItemsViewModel final : public Aero::Base::Object {
    AERO_DECLARE_TYPE(BindingItemsViewModel, Aero::Base::Object)
public:
    Aero::Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Ref<ObservableCollection<BindingSlotItem>> GetItems() const noexcept {
        return items_;
    }
    void SetItems(
        Ref<ObservableCollection<BindingSlotItem>> value) noexcept {
        items_ = std::move(value);
    }
    Ref<BindingPlayer> GetPlayer() const noexcept {
        return player_;
    }
    void SetPlayer(Ref<BindingPlayer> value) noexcept {
        player_ = std::move(value);
    }

private:
    Ref<ObservableCollection<BindingSlotItem>> items_{};
    Ref<BindingPlayer> player_{};
};

class BindingDoItemsViewModel final : public Aero::DependencyObject {
    AERO_DECLARE_TYPE(BindingDoItemsViewModel, Aero::DependencyObject)
public:
    BindingDoItemsViewModel() noexcept : DependencyObject(StaticTypeId()) {}
    Ref<ObservableCollection<BindingSlotItem>> GetItems() const noexcept {
        return items_;
    }
    void SetItems(
        Ref<ObservableCollection<BindingSlotItem>> value) noexcept {
        items_ = std::move(value);
    }
    Ref<ObservableObjectCollection> GetInventory() const noexcept {
        return inventory_;
    }
    void SetInventory(Ref<ObservableObjectCollection> value) noexcept {
        inventory_ = std::move(value);
    }

private:
    Ref<ObservableCollection<BindingSlotItem>> items_{};
    Ref<ObservableObjectCollection> inventory_{};
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
        return GetValue(ValueProperty);
    }
    std::int32_t GetMinValue() const noexcept {
        return GetValue(MinValueProperty);
    }
    std::int32_t GetMaxValue() const noexcept {
        return GetValue(MaxValueProperty);
    }
    std::int32_t GetStepValue() const noexcept {
        return GetValue(StepValueProperty);
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

// DP虚函数试点： registru时不挂Validate/Coerce delegate，引擎回退到虚函数。
// ValidateValueCore 拒收负数，CoerceValueCore 报>100 钳到 100。
class CoercePilot final : public UserControl {
    AERO_DECLARE_TYPE_NAMED(
        CoercePilot,
        UserControl,
        "clr-namespace:UserControls",
        "CoercePilot")
public:
    CoercePilot() noexcept : UserControl(StaticTypeId()) {}

    std::int32_t GetLevel() const noexcept {
        return GetValue(LevelProperty);
    }
    void SetLevel(std::int32_t value) noexcept {
        SetValue(LevelProperty, value);
    }

    inline static constexpr DependencyProperty<std::int32_t> LevelProperty{"Level"};

protected:
    bool ValidateValueCore(
        Aero::Meta::DependencyPropertyHandle property,
        const Aero::Meta::PropertyValue& value) const noexcept override {
        if (property != LevelProperty.Handle()) return true;
        if (value.Kind() != Aero::Base::ValueKind::SignedInteger) return true;
        return value.AsSignedInteger() >= 0;
    }
    Aero::Meta::PropertyValue CoerceValueCore(
        Aero::Meta::DependencyPropertyHandle property,
        const Aero::Meta::PropertyValue& baseValue) noexcept override {
        if (property != LevelProperty.Handle()) return baseValue;
        if (baseValue.Kind() != Aero::Base::ValueKind::SignedInteger) {
            return baseValue;
        }
        const std::int64_t raw = baseValue.AsSignedInteger();
        if (raw <= 100) return baseValue;
        Result<Aero::Meta::PropertyValue> clamped =
            Aero::Meta::ValueCodec<std::int32_t>::Encode(
                static_cast<std::int32_t>(100));
        return clamped ? std::move(clamped).Value() : baseValue;
    }
};

class ColorConverter final : public IMultiValueConverter {
    AERO_DECLARE_TYPE_NAMED(
        ColorConverter,
        IMultiValueConverter,
        "clr-namespace:UserControls",
        "ColorConverter")
public:
    Aero::Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Result<Aero::Value> Convert(
        Span<const Aero::Value> values,
        Aero::Meta::TypeId,
        const Aero::Value&) noexcept override {
        if (values.Size() < 3U) {
            return Aero::Base::Status::Failure(
                ErrorCode::InvalidArgument,
                "ColorConverter requires three channels");
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

class Clock final : public Aero::Controls::Control {
    AERO_DECLARE_TYPE_NAMED(
        Clock,
        Aero::Controls::Control,
        "clr-namespace:CustomControl",
        "Clock")
public:
    Clock() noexcept : Control(StaticTypeId()) {}
    std::int32_t GetHour() const noexcept {
        return GetValue(HourProperty);
    }
    void SetHour(std::int32_t value) noexcept {
        SetValue(HourProperty, value);
    }
    inline static constexpr DependencyProperty<std::int32_t> HourProperty{"Hour"};
};

class CircleAnimation final : public DoubleAnimationBase {
    AERO_DECLARE_TYPE_NAMED(
        CircleAnimation,
        DoubleAnimationBase,
        "clr-namespace:CustomAnimation",
        "CircleAnimation")
public:
    CircleAnimation() noexcept : DoubleAnimationBase(StaticTypeId()) {}
    double GetRadius() const noexcept {
        return GetValue(RadiusProperty);
    }
    void SetRadius(double value) noexcept {
        SetValue(RadiusProperty, value);
    }
    inline static constexpr DependencyProperty<double> RadiusProperty{"Radius"};
public:
    double GetCurrentValue(
        double defaultOriginValue,
        double defaultDestinationValue,
        double progress) const noexcept override {
        const double from = ResolveFrom(defaultOriginValue);
        const double to = ResolveTo(defaultDestinationValue);
        const double clamped = progress < 0.0 ? 0.0 : (progress > 1.0 ? 1.0 : progress);
        const double circular = 1.0 - std::sqrt(std::max(0.0, 1.0 - clamped * clamped));
        return from + (to - from) * circular * GetRadius();
    }
};

class Game final : public FrameworkElement {
    AERO_DECLARE_TYPE_NAMED(
        Game,
        FrameworkElement,
        "clr-namespace:CustomRender",
        "Game")
public:
    inline static int renderCount = 0;

    Game() noexcept : FrameworkElement(StaticTypeId()) {}
protected:
    void OnRender(Aero::Media::DrawingContext& context) noexcept override {
        ++renderCount;
        static_cast<void>(context.DrawRectangle(
            {0.0, 0.0, GetRenderSize().width, GetRenderSize().height},
            Aero::Base::Color{0.2F, 0.3F, 0.8F, 1.0F}));
        Result<Ref<Aero::Media::Pen>> pen = MakeRef<Aero::Media::Pen>();
        Result<Ref<Aero::Media::Brush>> stroke = Aero::Media::MakeSolidColorBrush(
            {1.0F, 1.0F, 1.0F, 1.0F});
        if (pen && stroke) {
            pen->SetBrush(stroke.Value());
            pen->SetThickness(2.0);
            static_cast<void>(context.DrawLine(
                pen.Value(), {0.0, 0.0}, {10.0, 0.0}));
            Aero::Media::StreamGeometry geometry;
            geometry.SetData("M 0,0 L 8,0 L 8,8 Z");
            static_cast<void>(context.DrawGeometry(
                stroke.Value(), pen.Value(), geometry));
        }
    }
};


class RgbModel final :
    public Aero::Base::Object,
    public NotifyPropertyChanged<RgbModel> {
    AERO_DECLARE_TYPE_NAMED(
        RgbModel,
        Aero::Base::Object,
        "clr-namespace:UserControls",
        "RgbModel")
public:
    Aero::Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    std::int32_t GetR() const noexcept { return r_; }
    std::int32_t GetG() const noexcept { return g_; }
    std::int32_t GetB() const noexcept { return b_; }
    void SetR(std::int32_t value) noexcept {
        r_ = value;
        RaisePropertyChanged("R");
    }
    void SetG(std::int32_t value) noexcept {
        g_ = value;
        RaisePropertyChanged("G");
    }
    void SetB(std::int32_t value) noexcept {
        b_ = value;
        RaisePropertyChanged("B");
    }
private:
    std::int32_t r_ = 0;
    std::int32_t g_ = 0;
    std::int32_t b_ = 0;
};

class HelloCommand;

class CommandsViewModel final :
    public Aero::Base::Object,
    public NotifyPropertyChanged<CommandsViewModel> {
    AERO_DECLARE_TYPE_NAMED(
        CommandsViewModel,
        Aero::Base::Object,
        "clr-namespace:Commands",
        "ViewModel")
public:
    Aero::Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    const String& GetInput() const noexcept { return input_; }
    const String& GetOutput() const noexcept { return output_; }
    Ref<ICommand> GetSayHelloCommand() const noexcept { return command_; }
    void SetInput(String value) noexcept {
        input_ = std::move(value);
        RaisePropertyChanged("Input");
    }
    void SetOutput(String value) noexcept {
        output_ = std::move(value);
        RaisePropertyChanged("Output");
    }
    void SetSayHelloCommand(Ref<ICommand> value) noexcept {
        command_ = std::move(value);
        RaisePropertyChanged("SayHelloCommand");
    }
private:
    String input_;
    String output_;
    Ref<ICommand> command_;
};

class HelloCommand final : public ICommand {
    AERO_DECLARE_TYPE_NAMED(
        HelloCommand,
        ICommand,
        "clr-namespace:Commands",
        "HelloCommand")
public:
    explicit HelloCommand(CommandsViewModel* owner) noexcept
        : owner_(owner) {}
    Aero::Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    bool CanExecute(
        const Aero::Value&,
        Aero::UIElement* = nullptr) noexcept override {
        return true;
    }
    void Execute(
        const Aero::Value& parameter,
        Aero::UIElement* = nullptr) noexcept override {
        ++executionCount_;
        if (owner_ == nullptr) return;
        String greeting;
        static_cast<void>(greeting.Assign("Hello"));
        if (parameter.Kind() == Aero::Base::ValueKind::String &&
            !parameter.AsString().Empty()) {
            static_cast<void>(greeting.Assign("Hello "));
            static_cast<void>(greeting.Append(parameter.AsString()));
        }
        owner_->SetOutput(std::move(greeting));
    }
    std::uint32_t GetExecutionCount() const noexcept {
        return executionCount_;
    }
private:
    CommandsViewModel* owner_ = nullptr;
    std::uint32_t executionCount_ = 0U;
};

class PickingSelector final : public DataTemplateSelector {
public:
    Ref<DataTemplate> pick;
    bool enabled = true;

    Ref<DataTemplate> SelectTemplate(
        Aero::Base::Object* item,
        Aero::DependencyObject* container) noexcept override {
        (void)item;
        (void)container;
        return enabled ? pick : Ref<DataTemplate>{};
    }
};

inline bool KeepWide(const Aero::Base::Object* item) noexcept {
    const FrameworkElement* element =
        TryCast<FrameworkElement>(const_cast<Aero::Base::Object*>(item));
    return element != nullptr && element->GetWidth() >= 50.0;
}

inline Result<void> RegisterTestTypes(Registration& registration) noexcept {
    Result<void> person = Aero::Meta::Register<Person>(registration)
        .Property<&Person::GetName, &Person::SetName>("Name")
        .PropertyChangeNotifications()
        .Factory()
        .Result();
    if (!person) return person;
    Result<void> named = Aero::Meta::Register<NamedItems>(registration)
        .Implements<IItemsSource>()
        .Factory()
        .Result();
    if (!named) return named;
    Result<void> slot = Aero::Meta::Register<BindingSlotItem>(registration)
        .Property<&BindingSlotItem::GetTeam, &BindingSlotItem::SetTeam>("Team")
        .Factory()
        .Result();
    if (!slot) return slot;
    Result<void> player = Aero::Meta::Register<BindingPlayer>(registration)
        .Property<&BindingPlayer::GetSlots, &BindingPlayer::SetSlots>("Slots")
        .Factory()
        .Result();
    if (!player) return player;
    Result<void> doModel =
        Aero::Meta::Register<BindingDoItemsViewModel>(registration)
            .Property<&BindingDoItemsViewModel::GetItems, &BindingDoItemsViewModel::SetItems>(
                "Items")
            .Property<
                &BindingDoItemsViewModel::GetInventory,
                &BindingDoItemsViewModel::SetInventory>("Inventory")
            .Factory()
            .Result();
    if (!doModel) return doModel;
    return Aero::Meta::Register<BindingItemsViewModel>(registration)
        .Property<&BindingItemsViewModel::GetItems, &BindingItemsViewModel::SetItems>(
            "Items")
        .Property<&BindingItemsViewModel::GetPlayer, &BindingItemsViewModel::SetPlayer>(
            "Player")
        .Factory()
        .Result();
}

inline const Aero::ModuleRegistration kTestModule =
    Aero::DefineModule("Aero.FrameworkTests", RegisterTestTypes);

inline Result<void> RegisterTutorialTypes(Registration& registration) noexcept {
    using Aero::Meta::FrameworkPropertyMetadata;
    std::printf("        [debug] Register NumericUpDown\n"); std::fflush(stdout);
    Result<void> status = Aero::Meta::Register<NumericUpDown>(registration)
        .Property(
            NumericUpDown::ValueProperty,
            FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(
            NumericUpDown::MinValueProperty,
            FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(
            NumericUpDown::MaxValueProperty,
            FrameworkPropertyMetadata(std::int32_t{255}))
        .Property(
            NumericUpDown::StepValueProperty,
            FrameworkPropertyMetadata(std::int32_t{1}))
        .EventHandler<Aero::RoutedEventArgs, &NumericUpDown::UpButton_Click>(
            "UpButton_Click")
        .EventHandler<Aero::RoutedEventArgs, &NumericUpDown::DownButton_Click>(
            "DownButton_Click")
        .Factory()
        .Result();
    if (!status) return status;

    // CoercePilot registers a plain default WITHOUT Validate/Coerce delegates;
    // the engine must fall back to ValidateValueCore/CoerceValueCore virtuals.
    status = Aero::Meta::Register<CoercePilot>(registration)
        .Property(
            CoercePilot::LevelProperty,
            FrameworkPropertyMetadata(std::int32_t{0}))
        .Factory()
        .Result();
    if (!status) return status;

    status = Aero::Meta::Register<ColorConverter>(registration)
        .Factory()
        .Result();
    if (!status) return status;

    status = Aero::Meta::Register<RgbModel>(registration)
        .Property<&RgbModel::GetR, &RgbModel::SetR>("R")
        .Property<&RgbModel::GetG, &RgbModel::SetG>("G")
        .Property<&RgbModel::GetB, &RgbModel::SetB>("B")
        .PropertyChangeNotifications()
        .Factory()
        .Result();
    if (!status) return status;

    status = Aero::Meta::Register<Clock>(registration)
        .Property(
            Clock::HourProperty,
            FrameworkPropertyMetadata(std::int32_t{0}))
        .Factory()
        .Result();
    if (!status) return status;

    status = Aero::Meta::Register<CircleAnimation>(registration)
        .Property(
            CircleAnimation::RadiusProperty,
            FrameworkPropertyMetadata(1.0))
        .Factory()
        .Result();
    if (!status) return status;

    status = Aero::Meta::Register<Game>(registration)
        .Factory()
        .Result();
    if (!status) return status;

    status = Aero::Meta::Register<HelloCommand>(registration)
        .Result();
    if (!status) return status;

    return Aero::Meta::Register<CommandsViewModel>(registration)
        .Property<&CommandsViewModel::GetInput, &CommandsViewModel::SetInput>(
            "Input")
        .Property<&CommandsViewModel::GetOutput, &CommandsViewModel::SetOutput>(
            "Output")
        .Property<
            &CommandsViewModel::GetSayHelloCommand,
            &CommandsViewModel::SetSayHelloCommand>("SayHelloCommand")
        .PropertyChangeNotifications()
        .Factory()
        .Result();
}

inline const Aero::ModuleRegistration kTutorialModule =
    Aero::DefineModule("Aero.TutorialTests", RegisterTutorialTypes);

constexpr char kNumericUpDownXaml[] =
    "<UserControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
    " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
    " x:Class=\"UserControls.NumericUpDown\" x:Name=\"Root\">"
    "<StackPanel>"
    "<RepeatButton x:Name=\"UpButton\" Content=\"+\" Click=\"UpButton_Click\"/>"
    "<TextBlock x:Name=\"ValueText\" Text=\"{Binding Value, ElementName=Root}\"/>"
    "<RepeatButton x:Name=\"DownButton\" Content=\"-\" Click=\"DownButton_Click\"/>"
    "</StackPanel>"
    "</UserControl>";

constexpr char kStarColorGridXaml[] =
    "<UserControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
    " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
    "<Grid>"
    "<Grid.RowDefinitions>"
    "<RowDefinition Height=\"*\"/>"
    "<RowDefinition Height=\"*\"/>"
    "<RowDefinition Height=\"*\"/>"
    "<RowDefinition Height=\"*\"/>"
    "</Grid.RowDefinitions>"
    "<Grid.ColumnDefinitions>"
    "<ColumnDefinition Width=\"1*\"/>"
    "<ColumnDefinition Width=\"2*\"/>"
    "</Grid.ColumnDefinitions>"
    "<Rectangle Fill=\"Red\" Stroke=\"Black\" Grid.RowSpan=\"4\" Margin=\"0,0,5,0\"/>"
    "<Slider x:Name=\"R\" Grid.Column=\"1\" Maximum=\"255\" VerticalAlignment=\"Center\""
    " Margin=\"10,1,0,1\"/>"
    "<TextBlock Text=\"R\" Grid.Column=\"1\" VerticalAlignment=\"Center\"/>"
    "</Grid>"
    "</UserControl>";

constexpr char kLanguageEnXaml[] =
    "<ResourceDictionary xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
    " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
    " xmlns:sys=\"clr-namespace:System;assembly=mscorlib\">"
    "<sys:String x:Key=\"Greeting\">Hello</sys:String>"
    "</ResourceDictionary>";

constexpr char kLanguageFrXaml[] =
    "<ResourceDictionary xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
    " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
    " xmlns:sys=\"clr-namespace:System;assembly=mscorlib\">"
    "<sys:String x:Key=\"Greeting\">Bonjour</sys:String>"
    "</ResourceDictionary>";

inline bool UriEndsWith(
    const Aero::Base::ResourceUri& uri,
    StringView suffix) noexcept {
    const StringView canonical = uri.Canonical();
    const StringView path = uri.Path();
    auto has = [&](StringView value) noexcept {
        if (suffix.SizeBytes() > value.SizeBytes()) return false;
        const std::uint32_t offset =
            value.SizeBytes() - suffix.SizeBytes();
        return std::memcmp(
            value.Data() + offset,
            suffix.Data(),
            suffix.SizeBytes()) == 0;
    };
    return has(canonical) || has(path);
}

inline Result<Aero::Markup::StreamResourceInfo> OpenTutorialXaml(
    const Aero::Base::ResourceUri& uri,
    void*) noexcept {
    const char* text = nullptr;
    if (UriEndsWith(uri, StringView("NumericUpDown.xaml"))) {
        text = kNumericUpDownXaml;
    } else if (UriEndsWith(uri, StringView("StarColorGrid.xaml"))) {
        text = kStarColorGridXaml;
    } else if (UriEndsWith(uri, StringView("Language-en.xaml"))) {
        text = kLanguageEnXaml;
    } else if (UriEndsWith(uri, StringView("Language-fr.xaml"))) {
        text = kLanguageFrXaml;
    }
    if (text == nullptr) {
        return Aero::Base::Status::Failure(
            ErrorCode::NotFound, "tutorial XAML provider has no such uri");
    }
    Result<Ref<MemoryStream>> stream = MakeRef<MemoryStream>(
        reinterpret_cast<const std::uint8_t*>(text),
        static_cast<std::uint32_t>(std::strlen(text)));
    if (!stream) return stream.GetStatus();
    Aero::Markup::StreamResourceInfo info;
    info.uri = uri;
    info.stream = Ref<Stream>(std::move(stream).Value());
    info.revision = 1U;
    return info;
}

// View::~View currently SIGSEGVs when content is mounted (object-factory
// shutdown vs. UnmountRoot). Tests that SetContent keep the Gui+View on the
// heap for the process lifetime so CHECK-failure returns cannot unwind it.
struct LiveGui {
    Aero::Diagnostics::DiagnosticBag diagnostics;
    Gui gui;
    Ref<View> view;
    double viewTime = 0.0;
};

inline void Pump(View& view, double timeInSeconds) noexcept {
    static_cast<void>(view.Update(timeInSeconds));
}

inline void PumpForward(LiveGui& live) noexcept {
    if (!live.view) {
        return;
    }
    live.viewTime += 0.016;
    Pump(*live.view, live.viewTime);
}

inline LiveGui* NewLiveGui(ViewOptions options = {}) {
    auto* live = new LiveGui();
    options.diagnostics = &live->diagnostics;
    Result<void> module = live->gui.AddModule(kTestModule);
    if (!module) {
        std::fprintf(stderr, "AddModule failed: %s\n",
            module.GetStatus().message);
        return nullptr;
    }
    Result<void> initialized = live->gui.Initialize();
    if (!initialized) {
        std::fprintf(stderr, "Initialize failed: %s\n",
            initialized.GetStatus().message);
        return nullptr;
    }
    Ref<View> created = live->gui.CreateView(options);
    if (!created) {
        std::fprintf(stderr, "CreateView failed\n");
        return nullptr;
    }
    live->view = std::move(created);
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

inline LiveGui* NewTutorialLiveGui(ViewOptions options = {}) {
    std::printf("    [debug] new LiveGui\n"); std::fflush(stdout);
    auto* live = new LiveGui();
    options.diagnostics = &live->diagnostics;
    std::printf("    [debug] AddModule test\n"); std::fflush(stdout);
    Result<void> module = live->gui.AddModule(kTestModule);
    if (!module) {
        std::fprintf(stderr, "AddModule test failed: %s\n",
            module.GetStatus().message);
        return nullptr;
    }
    std::printf("    [debug] AddModule tutorial\n"); std::fflush(stdout);
    Result<void> tutorial = live->gui.AddModule(kTutorialModule);
    if (!tutorial) {
        std::fprintf(stderr, "AddModule tutorial failed: %s\n",
            tutorial.GetStatus().message);
        return nullptr;
    }
    std::printf("    [debug] SetXamlProvider\n"); std::fflush(stdout);
    Result<Ref<Aero::Markup::XamlProviderAdapter>> provider =
        MakeRef<Aero::Markup::XamlProviderAdapter>(
            &OpenTutorialXaml, nullptr, nullptr);
    if (!provider) {
        std::fprintf(stderr, "tutorial XAML provider allocation failed\n");
        return nullptr;
    }
    Result<void> xaml = live->gui.SetXamlProvider(provider.Value(), "memory");
    if (!xaml) {
        std::fprintf(stderr, "SetXamlProvider failed: %s\n",
            xaml.GetStatus().message);
        return nullptr;
    }
    std::printf("    [debug] live->gui.Initialize\n"); std::fflush(stdout);
    Result<void> initialized = live->gui.Initialize();
    if (!initialized) {
        std::fprintf(stderr, "Initialize failed: %s\n",
            initialized.GetStatus().message);
        return nullptr;
    }
    std::printf("    [debug] live->gui.CreateView\n"); std::fflush(stdout);
    Ref<View> created = live->gui.CreateView(options);
    if (!created) {
        std::fprintf(stderr, "CreateView failed\n");
        return nullptr;
    }
    live->view = std::move(created);
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

inline void DumpDiagnostics(const Aero::Diagnostics::DiagnosticBag& bag) noexcept {
    for (std::uint32_t index = 0U; index < bag.Size(); ++index) {
        const StringView message = bag.Items()[index].Message();
        std::fprintf(stderr, "diagnostic: %.*s\n",
            static_cast<int>(message.SizeBytes()),
            message.Data());
    }
}

constexpr StringView kGalleryHostDpTarget(
    "XAML target does not support dependency properties");
constexpr StringView kGalleryHostMarkupFailed(
    "XAML markup-extension value provider failed");
constexpr StringView kGalleryHostSourceFailed(
    "ResourceDictionary Source could not be loaded");

inline bool ReportsGalleryHostFailure(StringView message) noexcept {
    return Contains(message, kGalleryHostDpTarget) ||
        Contains(message, kGalleryHostMarkupFailed) ||
        Contains(message, kGalleryHostSourceFailed);
}

inline bool DiagnosticsReportGalleryHostFailure(
    const Aero::Diagnostics::DiagnosticBag& bag) noexcept {
    for (std::uint32_t index = 0U; index < bag.Size(); ++index) {
        const Aero::Diagnostics::Diagnostic& item = bag.Items()[index];
        if (ReportsGalleryHostFailure(item.Message())) {
            return true;
        }
        const Span<const Aero::Diagnostics::DiagnosticNote> notes = item.Notes();
        for (std::uint32_t note = 0U; note < notes.Size(); ++note) {
            if (ReportsGalleryHostFailure(notes[note].Message())) {
                return true;
            }
        }
    }
    return false;
}

inline StringView AsView(const std::string& text) noexcept {
    return StringView(
        text.data(),
        static_cast<std::uint32_t>(text.size()));
}

inline StringView CStringView(const char* text) noexcept {
    if (text == nullptr || text[0] == '\0') {
        return {};
    }
    return StringView(
        text,
        static_cast<std::uint32_t>(std::strlen(text)));
}

inline bool WriteUtf8File(
    const std::filesystem::path& path,
    const char* text) noexcept {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out << text;
    return static_cast<bool>(out);
}

inline bool ParseMarkup(
    LiveGui& live,
    StringView markup) noexcept {
    Aero::Markup::XamlReader reader(live.gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(markup);
    if (!document) {
        std::fprintf(stderr, "XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live.diagnostics);
        return false;
    }
    return document.Value().IsValid();
}

inline bool MountMarkup(
    LiveGui& live,
    StringView markup,
    Aero::Size size) noexcept {
    Aero::Markup::XamlReader reader(live.gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(markup);
    if (!document) {
        std::fprintf(stderr, "XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live.diagnostics);
        return false;
    }
    bool mounted = live.view->SetContent(
        std::move(document).Value(), size);
    if (!mounted) {
        std::fprintf(stderr, "SetContent failed\n");
        DumpDiagnostics(live.diagnostics);
        return false;
    }
    live.view->SetSize(size);
    Pump(*live.view, 0.016);
    return true;
}

} // namespace Aero::ConformanceHarness

using namespace Aero::ConformanceHarness;

bool TestStreamContract();
bool TestDependencyPropertyVirtualFallback();
bool TestPublicNamesAndHierarchy();
bool TestXamlStreamReader();
bool TestProviderOwnershipAndReplacement();
bool TestRenderCommitContentStability();
bool TestDrawGeometryFillStrokeScene();
bool TestViewFrameViewportAndInput();
bool TestDispatcherCrossThreadPost();
bool TestContainerLayoutAndCalculators();
bool TestComboBoxAndVisualStateAnimation();
bool TestTransform3DCollapseAndHits();
bool TestViewboxHoverAndPopupFlip();
bool TestViewboxFrameworkElementSpacer();
bool TestStackPanelZIndexDoesNotReorderLayout();
bool TestPanelProgrammaticAddAttachesVisual();
bool TestPanelXamlChildrenStayVisuallyParented();
bool TestExpanderTemplatedParentIsCheckedWritesBack();
bool TestExpanderUnnamedHeaderClickWritesBack();
bool TestExpanderResourceDictionaryHeaderClick();
bool TestControlTemplateHoverStoryboard();
bool TestGridStarRowsSizeInStackPanel();
bool TestThemeTagAndIndentationAndColor();
bool TestGridSpanningAndStarMeasure();
bool TestBlendTutorialSidebarInteractions();
bool TestLoadComponentUserControlInStackPanel();
bool TestLoadComponentStarGridUserControlInStackPanel();
bool TestMenuTooltipForegroundAndFocus();
bool TestViewboxTransform3DButtonHit();
bool TestKeyboardNavigationIsTabStopTemplateTrigger();
bool TestCheckedVisualStateRevertsArrowOpacity();
bool TestRepeatButtonPressClickAndScaleX();
bool TestComboBoxPopupItemClickSelects();
bool TestComboBoxItemsSourcePopupClickSelects();
bool TestDataTemplateElementNameSelectedItemFilter();
bool TestDataTemplateMultiDataTriggerTeamFilter();
bool TestDataTemplateItemHoverStoryboard();
bool TestGeometryFlatten();
bool TestClosedPathRelativeMove();
bool TestStreamGeometryFlattenCore();
bool TestStreamGeometryContextFlatten();
bool TestTimelineDurationAndKeyTime();
bool TestCollectionViewAndVirtualization();
bool TestTemplateResolveOrder();
bool TestStrokeJoinCapFillRule();
bool TestNotifyPropertyChangedBindLoop();
bool TestBindingExpressionAndLostFocus();
bool TestCustomItemsSourceThunk();
bool TestClrItemsSourceBindingAfterDataContext();
bool TestGalleryXamlSurface();
bool TestGalleryHostXamlSurface();
bool TestTutorialXamlSurface();
bool TestTutorialRuntimePatterns();
bool TestLoadedIntroStoryboard();
bool TestBoardFlipRotationYStoryboard();
bool TestValueLayoutAndSSO();
bool TestPasswordBoxDecoupled();
bool TestArchitectureRefactoringValidation();
bool TestStyleSetterMergedStaticResource();
bool TestMergedNestedStaticResource();
bool TestTypeKeyedStaticResourceBasedOn();
bool TestControlTemplateBasedOn();
bool TestInventoryTemplateApply();
bool TestTutorialSampleXamlLoadApply();
bool TestMarkupExtensionContext();
bool TestViewLoadXamlAndHotReload();
