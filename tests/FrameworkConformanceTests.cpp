#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Stream.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Controls/Button.hpp>
#include <Aero/Controls/Canvas.hpp>
#include <Aero/Controls/ComboBox.hpp>
#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Controls/Grid.hpp>
#include <Aero/Controls/ItemCollection.hpp>
#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Controls/ListBox.hpp>
#include <Aero/Controls/ListBoxItem.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/Primitives/Selector.hpp>
#include <Aero/Controls/StackPanel.hpp>
#include <Aero/Controls/TabControl.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Controls/VirtualizingStackPanel.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/BindingExpression.hpp>
#include <Aero/Data/BindingOperations.hpp>
#include <Aero/Data/CollectionView.hpp>
#include <Aero/Data/CollectionViewSource.hpp>
#include <Aero/Data/NotifyPropertyChanged.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/DataTemplateSelector.hpp>
#include <Aero/DependencyObject.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Gui.hpp>
#include <Aero/Input.hpp>
#include <Aero/Controls/ItemContainerGenerator.hpp>
#include <Aero/Markup/XamlDocument.hpp>
#include <Aero/Value.hpp>
#include <Aero/Markup/XamlProvider.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Media/Animation/DoubleAnimation.hpp>
#include <Aero/Media/Animation/Duration.hpp>
#include <Aero/Media/Animation/KeyTime.hpp>
#include <Aero/Media/Animation/RepeatBehavior.hpp>
#include <Aero/Media/ArcSegment.hpp>
#include <Aero/Media/BezierSegment.hpp>
#include <Aero/Media/CombinedGeometry.hpp>
#include <Aero/Media/CompositeTransform3D.hpp>
#include <Aero/Media/FontProvider.hpp>
#include <Aero/Media/GeometryGroup.hpp>
#include <Aero/Media/LineGeometry.hpp>
#include <Aero/Media/LineSegment.hpp>
#include <Aero/Media/PathFigure.hpp>
#include <Aero/Media/PathGeometry.hpp>
#include <Aero/Media/PerspectiveTransform3D.hpp>
#include <Aero/Media/PolyBezierSegment.hpp>
#include <Aero/Media/PolyLineSegment.hpp>
#include <Aero/Media/PolyQuadraticBezierSegment.hpp>
#include <Aero/Media/QuadraticBezierSegment.hpp>
#include <Aero/Media/TextureProvider.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/TranslateTransform.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Module.hpp>
#include <Aero/Shapes/Path.hpp>
#include <Aero/Style.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/ApplicationCommands.hpp>
#include <Aero/Controls/BulletDecorator.hpp>
#include <Aero/Controls/Border.hpp>
#include <Aero/Controls/ColumnDefinition.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Image.hpp>
#include <Aero/Controls/ListView.hpp>
#include <Aero/Controls/Page.hpp>
#include <Aero/Controls/PasswordBox.hpp>
#include <Aero/Controls/Popup.hpp>
#include <Aero/Controls/DockPanel.hpp>
#include <Aero/Controls/Expander.hpp>
#include <Aero/Controls/Slider.hpp>
#include <Aero/Controls/Viewbox.hpp>
#include <Aero/Controls/TreeViewItem.hpp>
#include <Aero/Controls/UniformGrid.hpp>
#include <Aero/Controls/UserControl.hpp>
#include <Aero/Controls/Primitives/RepeatButton.hpp>
#include <Aero/Controls/Primitives/Thumb.hpp>
#include <Aero/Controls/Primitives/Track.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/Markup/XamlProvider.hpp>
#include <Aero/Media/Animation/DoubleAnimationBase.hpp>
#include <Aero/Media/Animation/DiscretePointKeyFrame.hpp>
#include <Aero/Media/Animation/EasingPointKeyFrame.hpp>
#include <Aero/Media/Animation/EasingThicknessKeyFrame.hpp>
#include <Aero/Media/Animation/ParallelTimeline.hpp>
#include <Aero/Media/Animation/PointAnimationUsingKeyFrames.hpp>
#include <Aero/Media/Animation/ThicknessAnimationUsingKeyFrames.hpp>
#include <Aero/Media/DrawingContext.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/StreamGeometry.hpp>
#include <Aero/Media/StreamGeometryContext.hpp>
#include <Aero/Media/ImageSource.hpp>
#include <Aero/Media/MatrixTransform.hpp>
#include <Aero/Media/ShaderEffect.hpp>
#include <Aero/Media/SolidColorBrush.hpp>
#include <Aero/Media/ScaleTransform.hpp>
#include <Aero/Media/SkewTransform.hpp>
#include <Aero/Media/TransformGroup.hpp>
#include <Aero/Media/TranslateTransform.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Shapes/Rectangle.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/KeyBinding.hpp>
#include <Aero/Media/BitmapImage.hpp>
#include <Aero/Media/BlurEffect.hpp>
#include <Aero/Media/RotateTransform.hpp>
#include <Aero/TextProperties.hpp>
#include <Aero/Threading.hpp>
#include <Aero/Controls/Primitives/ToggleButton.hpp>
#include <Aero/Documents/TextElement.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/View.hpp>
#include <Aero/Visibility.hpp>
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

namespace {

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

    Result<void> AddPoint(Point point) noexcept override {
        return points.PushBack(point);
    }
    Result<void> BeginFigure(Point start, bool isClosed) noexcept override {
        (void)isClosed;
        ++begins;
        Result<void> recorded = figureStarts.PushBack(start);
        if (!recorded) return recorded;
        return AddPoint(start);
    }
    Result<void> EndFigure(bool isClosed) noexcept override {
        (void)isClosed;
        ++ends;
        return {};
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
        Result<void> stored = items_.PushBack(std::move(item));
        if (!stored) return stored;
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
        return GetValueOr(HourProperty, 0);
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
        return GetValueOr(RadiusProperty, 1.0);
    }
    void SetRadius(double value) noexcept {
        SetValue(RadiusProperty, value);
    }
    inline static constexpr DependencyProperty<double> RadiusProperty{"Radius"};
protected:
    double GetCurrentValueCore(
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
    static int renderCount;

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
            pen.Value()->SetBrush(stroke.Value());
            pen.Value()->SetThickness(2.0);
            static_cast<void>(context.DrawLine(
                pen.Value(), {0.0, 0.0}, {10.0, 0.0}));
            Aero::Media::StreamGeometry geometry;
            geometry.SetData("M 0,0 L 8,0 L 8,8 Z");
            static_cast<void>(context.DrawGeometry(
                stroke.Value(), pen.Value(), geometry));
        }
    }
};

int Game::renderCount = 0;

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
    Result<bool> CanExecute(
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

bool KeepWide(const Aero::Base::Object* item) noexcept {
    const FrameworkElement* element =
        TryCast<FrameworkElement>(const_cast<Aero::Base::Object*>(item));
    return element != nullptr && element->GetWidth() >= 50.0;
}

Result<void> RegisterTestTypes(Registration& registration) noexcept {
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

const Aero::ModuleRegistration kTestModule =
    Aero::DefineModule("Aero.FrameworkTests", RegisterTestTypes);

Result<void> RegisterTutorialTypes(Registration& registration) noexcept {
    using Aero::Meta::FrameworkPropertyMetadata;
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

const Aero::ModuleRegistration kTutorialModule =
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

bool UriEndsWith(
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

Result<Aero::Markup::StreamResourceInfo> OpenTutorialXaml(
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

void Pump(View& view, double timeInSeconds) noexcept {
    static_cast<void>(view.Update(timeInSeconds));
}

void PumpForward(LiveGui& live) noexcept {
    if (!live.view) {
        return;
    }
    live.viewTime += 0.016;
    Pump(*live.view, live.viewTime);
}

LiveGui* NewLiveGui(ViewOptions options = {}) {
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

LiveGui* NewTutorialLiveGui(ViewOptions options = {}) {
    auto* live = new LiveGui();
    options.diagnostics = &live->diagnostics;
    Result<void> module = live->gui.AddModule(kTestModule);
    if (!module) {
        std::fprintf(stderr, "AddModule test failed: %s\n",
            module.GetStatus().message);
        return nullptr;
    }
    Result<void> tutorial = live->gui.AddModule(kTutorialModule);
    if (!tutorial) {
        std::fprintf(stderr, "AddModule tutorial failed: %s\n",
            tutorial.GetStatus().message);
        return nullptr;
    }
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

void DumpDiagnostics(const Aero::Diagnostics::DiagnosticBag& bag) noexcept {
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

bool ReportsGalleryHostFailure(StringView message) noexcept {
    return Contains(message, kGalleryHostDpTarget) ||
        Contains(message, kGalleryHostMarkupFailed) ||
        Contains(message, kGalleryHostSourceFailed);
}

bool DiagnosticsReportGalleryHostFailure(
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

StringView AsView(const std::string& text) noexcept {
    return StringView(
        text.data(),
        static_cast<std::uint32_t>(text.size()));
}

StringView CStringView(const char* text) noexcept {
    if (text == nullptr || text[0] == '\0') {
        return {};
    }
    return StringView(
        text,
        static_cast<std::uint32_t>(std::strlen(text)));
}

bool WriteUtf8File(
    const std::filesystem::path& path,
    const char* text) noexcept {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out << text;
    return static_cast<bool>(out);
}

bool ParseMarkup(
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

bool MountMarkup(
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
    Result<void> mounted = live.view->SetContent(
        std::move(document).Value(), size);
    if (!mounted) {
        std::fprintf(stderr, "SetContent failed: %s\n",
            mounted.GetStatus().message);
        DumpDiagnostics(live.diagnostics);
        return false;
    }
    live.view->SetSize(size);
    Pump(*live.view, 0.016);
    return true;
}

bool TestStreamContract() {
    constexpr char kBytes[] = "abcdef";
    const auto* data = reinterpret_cast<const std::uint8_t*>(kBytes);
    const std::uint32_t size = 6U;
    MemoryStream stream(data, size);
    CHECK(stream.CanRead());
    CHECK(stream.CanSeek());

    std::uint8_t buffer[4] = {};
    Result<std::uint32_t> first = stream.Read({buffer, 3U});
    CHECK(first && first.Value() == 3U);
    CHECK(buffer[0] == 'a' && buffer[1] == 'b' && buffer[2] == 'c');

    Result<std::uint64_t> position = stream.Position();
    CHECK(position && position.Value() == 3U);
    Result<std::uint64_t> length = stream.Length();
    CHECK(length && length.Value() == 6U);

    Result<std::uint64_t> current = stream.Seek(
        1, Aero::Base::SeekOrigin::Current);
    CHECK(current && current.Value() == 4U);
    Result<std::uint32_t> next = stream.Read({buffer, 2U});
    CHECK(next && next.Value() == 2U);
    CHECK(buffer[0] == 'e' && buffer[1] == 'f');

    Result<std::uint32_t> eof = stream.Read({buffer, 1U});
    CHECK(eof && eof.Value() == 0U);

    Result<std::uint64_t> begin = stream.Seek(
        0, Aero::Base::SeekOrigin::Begin);
    CHECK(begin && begin.Value() == 0U);
    Result<std::uint64_t> end = stream.Seek(
        0, Aero::Base::SeekOrigin::End);
    CHECK(end && end.Value() == 6U);
    Result<std::uint64_t> invalid = stream.Seek(
        1, Aero::Base::SeekOrigin::End);
    CHECK(!invalid);
    CHECK(invalid.GetStatus().code == ErrorCode::OutOfRange);
    return true;
}

bool TestPublicNamesAndHierarchy() {
    static_assert(std::is_base_of<Aero::Controls::Primitives::Selector, TabControl>::value,
        "TabControl must derive Selector");
    static_assert(std::is_base_of<Shape, Path>::value,
        "Path must derive Shape");
    static_assert(std::is_base_of<Aero::Controls::Primitives::Selector, ComboBox>::value,
        "ComboBox must derive Selector");
    static_assert(std::is_base_of<Aero::Controls::ItemsControl, Aero::Controls::Primitives::Selector>::value,
        "Selector must derive ItemsControl");
    static_assert(std::is_base_of<Aero::FrameworkElement, Aero::Controls::Control>::value,
        "Control must derive FrameworkElement");
    static_assert(std::is_base_of<Aero::Media::Visual, UIElement>::value,
        "UIElement must derive Visual");
    static_assert(!HasVisualGetTree<Aero::Media::Visual>::value,
        "Visual.GetTree must stay out of the installed SDK");
    static_assert(!HasAsUIElement<Aero::Media::Visual>::value,
        "AsUIElement must not exist on Visual");

    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);

    Result<Ref<Button>> button = MakeRef<Button>();
    CHECK(button);
    Aero::Base::Object* object = button.Value().Get();
    CHECK(TryCast<Button>(object) == button.Value().Get());
    CHECK(TryCast<FrameworkElement>(object) == button.Value().Get());
    CHECK(TryCast<UIElement>(object) == button.Value().Get());
    CHECK(TryCast<TabControl>(object) == nullptr);
    CHECK(Aero::Media::VisualTreeHelper::GetChildrenCount(*button.Value()) == 0U);

    CHECK(UIElement::Transform3DProperty.Name() == StringView("Transform3D"));
    return true;
}

bool TestXamlStreamReader() {
    Gui gui;
    Result<void> initialized = gui.Initialize();
    CHECK(initialized);

    constexpr char kMarkup[] =
        "<Grid xmlns=\"urn:aero\" Width=\"120\" Height=\"80\"/>";
    Aero::Markup::XamlReader reader(gui);
    Result<Aero::Markup::XamlDocument> parsed = reader.Parse(
        StringView(kMarkup));
    CHECK(parsed);
    CHECK(parsed.Value().IsValid());
    Grid* grid = parsed.Value().Root<Grid>();
    CHECK(grid != nullptr);
    CHECK(Near(grid->GetWidth(), 120.0));
    CHECK(Near(grid->GetHeight(), 80.0));

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(kMarkup);
    MemoryStream stream(bytes, static_cast<std::uint32_t>(sizeof(kMarkup) - 1U));
    Result<Aero::Markup::XamlDocument> loaded = reader.Load(stream);
    CHECK(loaded);
    CHECK(loaded.Value().IsValid());
    Grid* fromStream = loaded.Value().Root<Grid>();
    CHECK(fromStream != nullptr);
    CHECK(Near(fromStream->GetWidth(), 120.0));
    return true;
}

bool TestProviderOwnershipAndReplacement() {
    Result<Ref<Aero::Markup::XamlProviderAdapter>> first =
        MakeRef<Aero::Markup::XamlProviderAdapter>();
    Result<Ref<Aero::Markup::XamlProviderAdapter>> second =
        MakeRef<Aero::Markup::XamlProviderAdapter>();
    Result<Ref<SilentFontProvider>> fonts = MakeRef<SilentFontProvider>();
    Result<Ref<SilentTextureProvider>> textures =
        MakeRef<SilentTextureProvider>();
    CHECK(first && second && fonts && textures);

    Gui gui;
    Result<void> xamlA = gui.SetXamlProvider(first.Value());
    CHECK(xamlA);
    Result<void> xamlB = gui.SetXamlProvider(second.Value());
    CHECK(xamlB);
    Result<void> fontOk = gui.SetFontProvider(fonts.Value());
    CHECK(fontOk);
    Result<void> textureOk = gui.SetTextureProvider(textures.Value());
    CHECK(textureOk);
    Result<void> initialized = gui.Initialize();
    CHECK(initialized);
    CHECK(gui.IsInitialized());

    Result<void> frozenXaml = gui.SetXamlProvider(first.Value());
    CHECK(!frozenXaml);
    CHECK(frozenXaml.GetStatus().code == ErrorCode::InvalidState);
    Result<void> frozenFont = gui.SetFontProvider(fonts.Value());
    CHECK(!frozenFont);
    CHECK(frozenFont.GetStatus().code == ErrorCode::InvalidState);
    Result<void> frozenTexture = gui.SetTextureProvider(textures.Value());
    CHECK(!frozenTexture);
    CHECK(frozenTexture.GetStatus().code == ErrorCode::InvalidState);
    Result<void> frozenModule = gui.AddModule(kTestModule);
    CHECK(!frozenModule);
    CHECK(frozenModule.GetStatus().code == ErrorCode::InvalidState);
    return true;
}

bool TestViewFrameViewportAndInput() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({320.0, 240.0});
    ViewViewport viewport;
    viewport.logicalSize = {320.0, 240.0};
    viewport.pixelWidth = 640U;
    viewport.pixelHeight = 480U;
    viewport.dpiScale = 2.0;
    CHECK(view.SetViewport(viewport));

    Result<Ref<Button>> button = MakeRef<Button>();
    CHECK(button);
    button.Value()->SetWidth(80.0);
    button.Value()->SetHeight(32.0);
    CHECK(view.SetContent(button.Value()));
    Pump(view, 0.016);
    CHECK(view.GetContent() == button.Value().Get());
    static_cast<void>(view.MouseMove(12, 8));
    static_cast<void>(view.MouseButtonDown(
        12, 8, Aero::Input::MouseButton::Left));
    static_cast<void>(view.MouseButtonUp(
        12, 8, Aero::Input::MouseButton::Left));
    static_cast<void>(view.KeyDown(Aero::Input::Key::Tab));
    return true;
}

// P3.1 exit gate: external worker threads must be able to marshal callbacks
// onto the UI thread through Dispatcher::Post, pumped by the frame loop.
namespace {

struct CrossThreadPostRecord {
    std::atomic<std::uint32_t>* executed = nullptr;
    Aero::Threading::DispatcherThreadToken ownerThread = 0U;
    std::atomic<bool>* threadOk = nullptr;
    std::uint32_t* orderSlot = nullptr;
    std::atomic<std::uint32_t>* orderNext = nullptr;
};

void CrossThreadPostCallback(void* context) noexcept {
    auto* record = static_cast<CrossThreadPostRecord*>(context);
    if (record == nullptr) {
        return;
    }
    if (record->threadOk != nullptr &&
        Aero::Threading::CurrentDispatcherThreadToken() !=
            record->ownerThread) {
        record->threadOk->store(false, std::memory_order_relaxed);
    }
    if (record->orderSlot != nullptr && record->orderNext != nullptr) {
        *record->orderSlot = record->orderNext->fetch_add(
            1U, std::memory_order_relaxed);
    }
    if (record->executed != nullptr) {
        record->executed->fetch_add(1U, std::memory_order_relaxed);
    }
}

} // namespace

bool TestDispatcherCrossThreadPost() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;

    Result<Ref<Button>> button = MakeRef<Button>();
    CHECK(button);
    CHECK(view.SetContent(button.Value()));
    Aero::Threading::Dispatcher& dispatcher =
        button.Value()->GetDispatcher();
    const Aero::Threading::DispatcherThreadToken owner =
        dispatcher.OwnerThreadToken();
    CHECK(owner == Aero::Threading::CurrentDispatcherThreadToken());

    // FIFO order on the owner thread through the direct pump.
    static constexpr std::uint32_t kFifoPosts = 8U;
    std::atomic<std::uint32_t> fifoExecuted{0U};
    std::atomic<std::uint32_t> fifoNext{0U};
    std::uint32_t fifoOrder[kFifoPosts] = {};
    CrossThreadPostRecord fifoRecords[kFifoPosts] = {};
    for (std::uint32_t index = 0U; index < kFifoPosts; ++index) {
        fifoRecords[index].executed = &fifoExecuted;
        fifoRecords[index].ownerThread = owner;
        fifoRecords[index].orderSlot = &fifoOrder[index];
        fifoRecords[index].orderNext = &fifoNext;
        Result<Aero::Threading::DispatcherTaskHandle> posted =
            dispatcher.Post(
                Aero::Threading::DispatcherPriority::Normal,
                &CrossThreadPostCallback,
                &fifoRecords[index]);
        CHECK(posted);
    }
    Result<std::uint32_t> fifoPumped = dispatcher.ProcessPending();
    CHECK(fifoPumped);
    CHECK(fifoPumped.Value() == kFifoPosts);
    CHECK(fifoExecuted.load(std::memory_order_relaxed) == kFifoPosts);
    for (std::uint32_t index = 0U; index < kFifoPosts; ++index) {
        CHECK(fifoOrder[index] == index);
    }

    // Worker threads marshal onto the UI thread; the frame loop pumps them.
    static constexpr std::uint32_t kWorkerPosts = 16U;
    static constexpr std::uint32_t kWorkers = 2U;
    std::atomic<std::uint32_t> workerExecuted{0U};
    std::atomic<bool> workerThreadOk{true};
    CrossThreadPostRecord workerRecords[kWorkers][kWorkerPosts] = {};
    for (std::uint32_t worker = 0U; worker < kWorkers; ++worker) {
        for (std::uint32_t index = 0U; index < kWorkerPosts; ++index) {
            workerRecords[worker][index].executed = &workerExecuted;
            workerRecords[worker][index].ownerThread = owner;
            workerRecords[worker][index].threadOk = &workerThreadOk;
        }
    }
    std::thread workers[kWorkers];
    for (std::uint32_t worker = 0U; worker < kWorkers; ++worker) {
        workers[worker] = std::thread(
            [&dispatcher, &workerRecords, worker]() {
                for (std::uint32_t index = 0U;
                     index < kWorkerPosts;
                     ++index) {
                    static_cast<void>(dispatcher.Post(
                        Aero::Threading::DispatcherPriority::Background,
                        &CrossThreadPostCallback,
                        &workerRecords[worker][index]));
                }
            });
    }
    for (std::uint32_t worker = 0U; worker < kWorkers; ++worker) {
        workers[worker].join();
    }
    PumpForward(*live);
    CHECK(workerExecuted.load(std::memory_order_relaxed) ==
        kWorkers * kWorkerPosts);
    CHECK(workerThreadOk.load(std::memory_order_relaxed));

    // Delayed post fires after its due time through the same pump.
    std::atomic<std::uint32_t> delayedExecuted{0U};
    CrossThreadPostRecord delayedRecord;
    delayedRecord.executed = &delayedExecuted;
    delayedRecord.ownerThread = owner;
    Result<Aero::Threading::DispatcherTaskHandle> delayedPosted =
        dispatcher.PostDelayed(
            5000U,
            Aero::Threading::DispatcherPriority::Normal,
            &CrossThreadPostCallback,
            &delayedRecord);
    CHECK(delayedPosted);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    PumpForward(*live);
    CHECK(delayedExecuted.load(std::memory_order_relaxed) == 1U);

    // Cancelled work never runs.
    std::atomic<std::uint32_t> cancelledExecuted{0U};
    CrossThreadPostRecord cancelledRecord;
    cancelledRecord.executed = &cancelledExecuted;
    Result<Aero::Threading::DispatcherTaskHandle> cancellablePosted =
        dispatcher.Post(
            Aero::Threading::DispatcherPriority::Normal,
            &CrossThreadPostCallback,
            &cancelledRecord);
    CHECK(cancellablePosted);
    CHECK(dispatcher.Cancel(cancellablePosted.Value()));
    PumpForward(*live);
    CHECK(cancelledExecuted.load(std::memory_order_relaxed) == 0U);
    return true;
}

bool TestContainerLayoutAndCalculators() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;

    constexpr char kGrid[] =
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "Width=\"200\" Height=\"120\" "
        "ColumnDefinitionsText=\"80,*\" RowDefinitionsText=\"*\">"
        "<Button x:Name=\"Left\" Grid.Column=\"0\"/>"
        "<Button x:Name=\"Right\" Grid.Column=\"1\"/>"
        "</Grid>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> gridDocument = reader.Parse(
        StringView(kGrid));
    if (!gridDocument) {
        std::fprintf(stderr, "grid XAML parse failed: %s\n",
            gridDocument.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(gridDocument);
    Button* left = gridDocument.Value().FindName<Button>("Left");
    Button* right = gridDocument.Value().FindName<Button>("Right");
    CHECK(left != nullptr && right != nullptr);
    CHECK(view.SetContent(std::move(gridDocument).Value(), {200.0, 120.0}));
    Pump(view, 0.016);
    Grid* grid = TryCast<Grid>(view.GetContent());
    CHECK(grid != nullptr);
    CHECK(grid->GetIsMeasureValid());
    CHECK(grid->GetIsArrangeValid());
    CHECK(Near(left->GetRenderSize().width, 80.0, 1.0));
    CHECK(right->GetRenderSize().width > 80.0);

    constexpr char kStack[] =
        "<StackPanel xmlns=\"urn:aero\" Width=\"80\" Height=\"60\">"
        "<Button Width=\"40\" Height=\"20\"/>"
        "</StackPanel>";
    CHECK(MountMarkup(*live, kStack, {80.0, 60.0}));
    StackPanel* stack = TryCast<StackPanel>(view.GetContent());
    CHECK(stack != nullptr);
    CHECK(stack->GetDesiredSize().height >= 20.0);

    constexpr char kCanvas[] =
        "<Canvas xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Button x:Name=\"Placed\" Width=\"10\" Height=\"10\" "
        "Canvas.Left=\"12\" Canvas.Top=\"8\"/>"
        "</Canvas>";
    Aero::Markup::XamlReader canvasReader(live->gui);
    Result<Aero::Markup::XamlDocument> canvasDocument = canvasReader.Parse(
        StringView(kCanvas));
    CHECK(canvasDocument);
    Button* placed = canvasDocument.Value().FindName<Button>("Placed");
    CHECK(placed != nullptr);
    CHECK(view.SetContent(std::move(canvasDocument).Value(), {80.0, 60.0}));
    Pump(view, 0.048);
    Canvas* canvas = TryCast<Canvas>(view.GetContent());
    CHECK(canvas != nullptr);
    const Point canvasPos = canvas->GetChildPosition(*placed);
    CHECK(Near(canvasPos.x, 12.0) && Near(canvasPos.y, 8.0));
    return true;
}

bool TestComboBoxAndVisualStateAnimation() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({240.0, 160.0});

    Result<Ref<ComboBox>> combo = MakeRef<ComboBox>();
    CHECK(combo);
    Result<Ref<TextBlock>> first = MakeRef<TextBlock>();
    Result<Ref<TextBlock>> second = MakeRef<TextBlock>();
    CHECK(first && second);
    first.Value()->SetText("alpha");
    second.Value()->SetText("beta");
    CHECK(combo.Value()->GetItems().Add(first.Value()));
    CHECK(combo.Value()->GetItems().Add(second.Value()));
    combo.Value()->SetSelectedIndex(1U);
    CHECK(combo.Value()->GetSelectedItem().Get() == second.Value().Get());
    combo.Value()->SetIsDropDownOpen(true);
    CHECK(combo.Value()->GetIsDropDownOpen());

    CHECK(view.SetContent(combo.Value()));
    Pump(view, 0.016);
    static_cast<void>(combo.Value()->ApplyTemplate());
    static_cast<void>(VisualStateManager::GoToState(
        *combo.Value(), "Normal", false));
    static_cast<void>(VisualStateManager::GoToState(
        *combo.Value(), "Focused", true));

    DoubleAnimation animation;
    animation.SetFrom(0.0);
    animation.SetTo(10.0);
    animation.SetDuration(Duration::Automatic());
    CHECK(animation.GetDuration().IsAutomatic());
    animation.SetDuration(Duration::Forever());
    CHECK(animation.GetDuration().IsForever());
    animation.SetRepeatBehavior(RepeatBehavior::Count(2.0));
    CHECK(animation.GetRepeatBehavior().HasCount());

    Style style(DoubleAnimation::StaticTypeId());
    CHECK(style.Set(
        Timeline::DurationProperty,
        Duration::FromTimeSpan(TimeSpan::FromMicroseconds(2'000'000ULL))));
    CHECK(style.Set(Timeline::BeginTimeProperty, TimeSpan::Zero()));
    CHECK(style.Set(Timeline::AutoReverseProperty, true));
    CHECK(Timeline::DurationProperty.Name() == StringView("Duration"));
    CHECK(Timeline::RepeatBehaviorProperty.Name() == StringView("RepeatBehavior"));
    return true;
}

bool TestTransform3DCollapseAndHits() {
    CompositeTransform3D rotate;
    rotate.SetRotationY(45.0);
    const Aero::Base::Transform3 model = rotate.GetTransform3D();
    const Aero::Base::ProjectiveTransform2D collapsed =
        Aero::Base::CollapsePerspective(model, 1000.0, {50.0, 40.0});
    CHECK(std::isfinite(collapsed.m11));
    CHECK(std::isfinite(collapsed.m33));
    CHECK(std::abs(collapsed.m13) > 1.0e-6 || std::abs(collapsed.m31) > 1.0e-6);

    PerspectiveTransform3D camera;
    camera.SetDepth(800.0);
    camera.SetOffsetX(4.0);
    CHECK(Near(camera.GetDepth(), 800.0));
    CHECK(Aero::Base::LeavesZ0PlaneUnchanged(camera.GetTransform3D()));

    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;

    constexpr char kTree[] =
        "<Canvas xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "Width=\"200\" Height=\"120\">"
        "<Button x:Name=\"Left\" Width=\"40\" Height=\"40\" "
        "Canvas.Left=\"20\" Canvas.Top=\"40\"/>"
        "<Button x:Name=\"Right\" Width=\"40\" Height=\"40\" "
        "Canvas.Left=\"120\" Canvas.Top=\"40\"/>"
        "</Canvas>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    CHECK(document);
    Canvas* root = document.Value().Root<Canvas>();
    Button* left = document.Value().FindName<Button>("Left");
    Button* right = document.Value().FindName<Button>("Right");
    CHECK(root != nullptr && left != nullptr && right != nullptr);

    Result<Ref<PerspectiveTransform3D>> shared = MakeRef<PerspectiveTransform3D>();
    Result<Ref<CompositeTransform3D>> leftSpin = MakeRef<CompositeTransform3D>();
    Result<Ref<CompositeTransform3D>> rightSpin = MakeRef<CompositeTransform3D>();
    CHECK(shared && leftSpin && rightSpin);
    shared.Value()->SetDepth(1000.0);
    leftSpin.Value()->SetRotationY(25.0);
    rightSpin.Value()->SetRotationY(-25.0);
    root->SetTransform3D(shared.Value());
    left->SetTransform3D(leftSpin.Value());
    right->SetTransform3D(rightSpin.Value());

    CHECK(view.SetContent(std::move(document).Value(), {200.0, 120.0}));
    Pump(view, 0.016);

    Point leftScreen{};
    Point leftRoundtrip{};
    CHECK(left->TryPointToScreen({20.0, 20.0}, leftScreen));
    CHECK(left->TryPointFromScreen(leftScreen, leftRoundtrip));
    CHECK(Near(leftRoundtrip.x, 20.0, 0.75));
    CHECK(Near(leftRoundtrip.y, 20.0, 0.75));

    Point rightScreen{};
    CHECK(right->TryPointToScreen({20.0, 20.0}, rightScreen));
    CHECK(std::abs(leftScreen.x - rightScreen.x) > 1.0);

    Aero::Base::ProjectiveTransform2D toRoot{};
    CHECK(left->TryTransformToVisual(*root, toRoot));
    CHECK(std::isfinite(toRoot.m11) && std::isfinite(toRoot.m33));

    {
        CompositeTransform3D toward;
        toward.SetTranslateZ(10.0);
        CompositeTransform3D plane;
        const Aero::Base::Point origin{0.0, 20.0};
        const Aero::Base::Point extent{40.0, 20.0};
        const Aero::Base::Point center{20.0, 20.0};
        const Aero::Base::Point nearLeft = Aero::Base::TransformPointClamped(
            toward.GetTransform3D(), origin, 1000.0, center);
        const Aero::Base::Point nearRight = Aero::Base::TransformPointClamped(
            toward.GetTransform3D(), extent, 1000.0, center);
        const Aero::Base::Point planeLeft = Aero::Base::TransformPointClamped(
            plane.GetTransform3D(), origin, 1000.0, center);
        const Aero::Base::Point planeRight = Aero::Base::TransformPointClamped(
            plane.GetTransform3D(), extent, 1000.0, center);
        CHECK(std::abs(nearRight.x - nearLeft.x) >
            std::abs(planeRight.x - planeLeft.x));
    }
    {
        CompositeTransform3D plus;
        plus.SetRotationY(8.0);
        CompositeTransform3D minus;
        minus.SetRotationY(-10.0);
        const Aero::Base::Point3 plusEdge =
            Aero::Base::TransformPoint(
                plus.GetTransform3D(), Aero::Base::Point{80.0, 20.0});
        const Aero::Base::Point3 minusEdge =
            Aero::Base::TransformPoint(
                minus.GetTransform3D(), Aero::Base::Point{80.0, 20.0});
        CHECK(plusEdge.z > 0.0);
        CHECK(minusEdge.z < 0.0);
    }
    return true;
}

bool TestViewboxHoverAndPopupFlip() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({800.0, 600.0});

    constexpr char kTree[] =
        "<Viewbox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid Width=\"400\" Height=\"300\">"
        "<Grid.Resources>"
        "<Style TargetType=\"{x:Type ComboBox}\">"
        "<Setter Property=\"Template\">"
        "<Setter.Value>"
        "<ControlTemplate TargetType=\"{x:Type ComboBox}\">"
        "<Grid>"
        "<ToggleButton IsChecked=\"{Binding IsDropDownOpen, Mode=TwoWay, "
        "RelativeSource={RelativeSource TemplatedParent}}\"/>"
        "<ContentPresenter ContentSource=\"SelectionBoxItem\" IsHitTestVisible=\"False\"/>"
        "<Popup x:Name=\"PART_Popup\" IsOpen=\"{Binding IsDropDownOpen, "
        "RelativeSource={RelativeSource TemplatedParent}}\" Placement=\"Bottom\" "
        "VerticalOffset=\"1\">"
        "<Border Background=\"#FF393B40\" MinWidth=\"{TemplateBinding ActualWidth}\">"
        "<ScrollViewer>"
        "<ItemsPresenter/>"
        "</ScrollViewer>"
        "</Border>"
        "</Popup>"
        "</Grid>"
        "</ControlTemplate>"
        "</Setter.Value>"
        "</Setter>"
        "</Style>"
        "</Grid.Resources>"
        "<Path Stretch=\"Fill\" Fill=\"#FF111111\" "
        "Data=\"M0,0 L400,0 L400,300 L0,300 z\"/>"
        "<Button x:Name=\"HitButton\" Width=\"80\" Height=\"32\" "
        "HorizontalAlignment=\"Right\" VerticalAlignment=\"Bottom\" "
        "Margin=\"0,0,8,8\" Background=\"#FF3E4146\"/>"
        "<ComboBox x:Name=\"HitCombo\" Width=\"140\" Height=\"28\" "
        "HorizontalAlignment=\"Left\" VerticalAlignment=\"Bottom\" "
        "Margin=\"8,0,0,8\">"
        "<ComboBoxItem Content=\"Alpha\"/>"
        "<ComboBoxItem Content=\"Beta\"/>"
        "<ComboBoxItem Content=\"Gamma\"/>"
        "</ComboBox>"
        "</Grid>"
        "</Viewbox>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "viewbox hover XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {800.0, 600.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    Button* button = root->FindName<Button>(StringView("HitButton"));
    ComboBox* combo = root->FindName<ComboBox>(StringView("HitCombo"));
    CHECK(button != nullptr);
    CHECK(combo != nullptr);

    const Aero::Size buttonSize = button->GetRenderSize();
    CHECK(buttonSize.width > 0.0 && buttonSize.height > 0.0);
    Point buttonScreen{};
    CHECK(button->TryPointToScreen(
        {buttonSize.width * 0.5, buttonSize.height * 0.5}, buttonScreen));
    static_cast<void>(view.MouseMove(
        static_cast<int>(buttonScreen.x),
        static_cast<int>(buttonScreen.y)));
    Pump(view, 0.048);
    if (!button->GetIsMouseOver()) {
        std::fprintf(stderr,
            "button hover miss screen=(%.1f,%.1f) size=(%.1f,%.1f) slot=(%.1f,%.1f,%.1f,%.1f)\n",
            buttonScreen.x, buttonScreen.y,
            buttonSize.width, buttonSize.height,
            button->GetLayoutSlot().x, button->GetLayoutSlot().y,
            button->GetLayoutSlot().width, button->GetLayoutSlot().height);
    }
    CHECK(button->GetIsMouseOver());

    static_cast<void>(combo->ApplyTemplate());
    combo->SetIsDropDownOpen(true);
    Pump(view, 0.064);
    Pump(view, 0.080);

    const auto findPopup = [](auto& self, Aero::Media::Visual& visual)
        -> Aero::Controls::Primitives::Popup* {
        if (auto* popup = TryCast<Aero::Controls::Primitives::Popup>(&visual)) {
            return popup;
        }
        const std::uint32_t count =
            Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
        for (std::uint32_t index = 0U; index < count; ++index) {
            Aero::Media::Visual* child =
                Aero::Media::VisualTreeHelper::GetChild(visual, index);
            if (child == nullptr) continue;
            if (Aero::Controls::Primitives::Popup* found = self(self, *child)) {
                return found;
            }
        }
        return nullptr;
    };
    Aero::Controls::Primitives::Popup* popup = findPopup(findPopup, *root);
    if (popup == nullptr) {
        popup = findPopup(findPopup, *combo);
    }
    if (popup == nullptr) {
        std::fprintf(stderr,
            "popup missing comboChildren=%u rootChildren=%u dropDown=%d\n",
            Aero::Media::VisualTreeHelper::GetChildrenCount(*combo),
            Aero::Media::VisualTreeHelper::GetChildrenCount(*root),
            combo->GetIsDropDownOpen() ? 1 : 0);
    }
    CHECK(popup != nullptr);
    CHECK(popup->GetIsOpen());
    CHECK(Aero::Media::VisualTreeHelper::GetChildrenCount(*popup) > 0U);
    UIElement* popupChild = TryCast<UIElement>(
        Aero::Media::VisualTreeHelper::GetChild(*popup, 0U));
    CHECK(popupChild != nullptr);
    const Aero::Rect popupSlot = popupChild->GetLayoutSlot();
    if (!(popupSlot.y < 0.0)) {
        std::fprintf(stderr,
            "popup did not flip up slot.y=%.1f size=(%.1f,%.1f) comboSize=(%.1f,%.1f)\n",
            popupSlot.y, popupSlot.width, popupSlot.height,
            combo->GetRenderSize().width, combo->GetRenderSize().height);
    }
    CHECK(popupSlot.y < 0.0);

    Panel* itemsHost = combo->GetItemsHost();
    CHECK(itemsHost != nullptr);
    CHECK(Aero::Media::VisualTreeHelper::GetChildrenCount(*itemsHost) >= 2U);
    Aero::Media::Visual* itemVisual =
        Aero::Media::VisualTreeHelper::GetChild(*itemsHost, 1U);
    auto* item = TryCast<ComboBoxItem>(itemVisual);
    CHECK(item != nullptr);
    const Aero::Size itemSize = item->GetRenderSize();
    CHECK(itemSize.width > 0.0 && itemSize.height > 0.0);
    Point itemScreen{};
    CHECK(item->TryPointToScreen(
        {itemSize.width * 0.5, itemSize.height * 0.5}, itemScreen));
    static_cast<void>(view.MouseMove(
        static_cast<int>(itemScreen.x),
        static_cast<int>(itemScreen.y)));
    Pump(view, 0.096);
    if (!item->GetIsMouseOver()) {
        std::fprintf(stderr,
            "combo item hover miss screen=(%.1f,%.1f) size=(%.1f,%.1f) slot=(%.1f,%.1f,%.1f,%.1f)\n",
            itemScreen.x, itemScreen.y,
            itemSize.width, itemSize.height,
            item->GetLayoutSlot().x, item->GetLayoutSlot().y,
            item->GetLayoutSlot().width, item->GetLayoutSlot().height);
    }
    CHECK(item->GetIsMouseOver());
    return true;
}

bool TestViewboxFrameworkElementSpacer() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({800.0, 600.0});

    constexpr char kTree[] =
        "<Viewbox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel>"
        "<Rectangle x:Name=\"Track\" Width=\"620\" Height=\"28\" Fill=\"WhiteSmoke\"/>"
        "<FrameworkElement Margin=\"0,28,0,0\"/>"
        "<Button x:Name=\"Go\" Width=\"120\" Height=\"28\" Content=\"Go!\"/>"
        "</StackPanel>"
        "</Viewbox>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "viewbox spacer XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {800.0, 600.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);

    auto* viewbox = TryCast<Viewbox>(view.GetContent());
    CHECK(viewbox != nullptr);
    Aero::Base::Transform2D stretch{};
    CHECK(viewbox->TryGetViewboxTransform(stretch));
    if (stretch.m11 < 0.1 || stretch.m22 < 0.1) {
        std::fprintf(stderr,
            "Viewbox spacer collapsed stretch m11=%.6g m22=%.6g\n",
            stretch.m11, stretch.m22);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(stretch.m11 >= 0.1 && stretch.m22 >= 0.1);

    Button* go = viewbox->FindName<Button>(StringView("Go"));
    CHECK(go != nullptr);
    const Aero::Size goSize = go->GetRenderSize();
    CHECK(goSize.width > 0.0 && goSize.height > 0.0);
    Point topLeft{};
    Point bottomRight{};
    CHECK(go->TryPointToScreen({0.0, 0.0}, topLeft));
    CHECK(go->TryPointToScreen(
        {goSize.width, goSize.height}, bottomRight));
    const double screenWidth = std::abs(bottomRight.x - topLeft.x);
    const double screenHeight = std::abs(bottomRight.y - topLeft.y);
    if (screenWidth < 8.0 || screenHeight < 4.0) {
        std::fprintf(stderr,
            "Viewbox spacer hid Go screen=(%.2f,%.2f)-(%.2f,%.2f)\n",
            topLeft.x, topLeft.y, bottomRight.x, bottomRight.y);
    }
    CHECK(screenWidth >= 8.0 && screenHeight >= 4.0);
    return true;
}

bool TestStackPanelZIndexDoesNotReorderLayout() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({400.0, 300.0});

    constexpr char kTree[] =
        "<StackPanel xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" x:Name=\"Host\">"
        "<Button x:Name=\"First\" Width=\"80\" Height=\"24\" Content=\"First\"/>"
        "<Button x:Name=\"Second\" Width=\"80\" Height=\"24\" Content=\"Second\"/>"
        "<Button x:Name=\"Go\" Width=\"80\" Height=\"24\" Panel.ZIndex=\"-1\" Content=\"Go\"/>"
        "</StackPanel>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "stackpanel zindex XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {400.0, 300.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);

    auto* host = TryCast<StackPanel>(view.GetContent());
    CHECK(host != nullptr);
    Button* first = host->FindName<Button>(StringView("First"));
    Button* second = host->FindName<Button>(StringView("Second"));
    Button* go = host->FindName<Button>(StringView("Go"));
    CHECK(first != nullptr && second != nullptr && go != nullptr);

    CHECK(Aero::Media::VisualTreeHelper::GetChild(*host, 0U) == go);
    CHECK(go->GetLayoutSlot().y > second->GetLayoutSlot().y);
    CHECK(second->GetLayoutSlot().y > first->GetLayoutSlot().y);
    return true;
}

bool TestPanelProgrammaticAddAttachesVisual() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({200.0, 200.0});

    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
        "<Canvas xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"Host\" Width=\"200\" Height=\"200\"/>"));
    if (!document) {
        std::fprintf(stderr, "panel add XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {200.0, 200.0}));
    Pump(view, 0.016);

    auto* host = TryCast<Canvas>(view.GetContent());
    CHECK(host != nullptr);
    Result<Ref<Rectangle>> rectangle = MakeRef<Rectangle>();
    CHECK(rectangle);
    rectangle.Value()->SetWidth(40.0);
    rectangle.Value()->SetHeight(20.0);
    CHECK(host->GetChildren().Add(
        Ref<Aero::UIElement>(rectangle.Value())));
    Pump(view, 0.032);
    CHECK(rectangle.Value()->GetVisualParent() == host);
    CHECK(Aero::Media::VisualTreeHelper::GetChildrenCount(*host) == 1U);
    CHECK(Aero::Media::VisualTreeHelper::GetChild(*host, 0U) ==
        rectangle.Value().Get());
    CHECK(host->GetChildren().Remove(*rectangle.Value()));
    Pump(view, 0.048);
    CHECK(rectangle.Value()->GetVisualParent() == nullptr);
    CHECK(Aero::Media::VisualTreeHelper::GetChildrenCount(*host) == 0U);
    return true;
}

bool TestPanelXamlChildrenStayVisuallyParented() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({200.0, 200.0});

    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
        "<Canvas xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "Width=\"200\" Height=\"200\">"
        "<Canvas.Resources>"
        "<ControlTemplate x:Key=\"HoverBtn\" TargetType=\"{x:Type Button}\">"
        "<Border x:Name=\"Bg\" Background=\"Gray\" Width=\"80\" Height=\"24\">"
        "<Border x:Name=\"BgOver\" Background=\"White\" Opacity=\"0\"/>"
        "</Border>"
        "<ControlTemplate.Triggers>"
        "<Trigger Property=\"IsMouseOver\" Value=\"True\">"
        "<Setter TargetName=\"BgOver\" Property=\"Opacity\" Value=\"1\"/>"
        "</Trigger>"
        "</ControlTemplate.Triggers>"
        "</ControlTemplate>"
        "</Canvas.Resources>"
        "<Button x:Name=\"Add\" Width=\"80\" Height=\"24\" "
        "Template=\"{StaticResource HoverBtn}\"/>"
        "<Rectangle x:Name=\"Mark\" Width=\"20\" Height=\"20\" "
        "Canvas.Left=\"40\" Canvas.Top=\"40\"/>"
        "</Canvas>"));
    if (!document) {
        std::fprintf(stderr, "panel xaml children parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {200.0, 200.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);

    auto* host = TryCast<Canvas>(view.GetContent());
    CHECK(host != nullptr);
    CHECK(Aero::Media::VisualTreeHelper::GetChildrenCount(*host) == 2U);
    Button* add = host->FindName<Button>(StringView("Add"));
    CHECK(add != nullptr);
    CHECK(add->GetVisualParent() == host);
    CHECK(Aero::Media::VisualTreeHelper::GetChildrenCount(*add) == 1U);
    auto* bg = TryCast<Aero::Controls::Border>(
        Aero::Media::VisualTreeHelper::GetChild(*add, 0U));
    CHECK(bg != nullptr);
    CHECK(Aero::Media::VisualTreeHelper::GetChildrenCount(*bg) >= 1U);
    auto* over = TryCast<Aero::Controls::Border>(
        Aero::Media::VisualTreeHelper::GetChild(*bg, 0U));
    CHECK(over != nullptr);
    CHECK(Near(over->GetOpacity(), 0.0, 0.01));
    const Aero::Size addSize = add->GetRenderSize();
    CHECK(addSize.width > 0.0 && addSize.height > 0.0);
    Point addScreen{};
    CHECK(add->TryPointToScreen(
        {addSize.width * 0.5, addSize.height * 0.5}, addScreen));
    static_cast<void>(view.MouseMove(
        static_cast<int>(addScreen.x),
        static_cast<int>(addScreen.y)));
    Pump(view, 0.048);
    if (!add->GetIsMouseOver() || !Near(over->GetOpacity(), 1.0, 0.01)) {
        std::fprintf(stderr,
            "panel xaml button hover miss screen=(%.1f,%.1f) size=(%.1f,%.1f) "
            "over=%.3f isOver=%d visualChildren=%u\n",
            addScreen.x, addScreen.y, addSize.width, addSize.height,
            over->GetOpacity(), add->GetIsMouseOver() ? 1 : 0,
            Aero::Media::VisualTreeHelper::GetChildrenCount(*add));
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(add->GetIsMouseOver());
    CHECK(Near(over->GetOpacity(), 1.0, 0.01));
    return true;
}

bool TestExpanderTemplatedParentIsCheckedWritesBack() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({240.0, 160.0});

    constexpr char kTree[] =
        "<Expander xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"Host\" Header=\"Position\" IsExpanded=\"True\">"
        "<Expander.Template>"
        "<ControlTemplate TargetType=\"{x:Type Expander}\">"
        "<StackPanel>"
        "<ToggleButton x:Name=\"HeaderButton\" "
        "IsChecked=\"{Binding IsExpanded, RelativeSource={RelativeSource TemplatedParent}}\"/>"
        "<ContentPresenter/>"
        "</StackPanel>"
        "</ControlTemplate>"
        "</Expander.Template>"
        "<TextBlock Text=\"Body\"/>"
        "</Expander>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "expander binding XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);

    auto* host = TryCast<Expander>(view.GetContent());
    CHECK(host != nullptr);
    CHECK(host->GetIsExpanded());
    ToggleButton* header = nullptr;
    const std::uint32_t visualCount =
        Aero::Media::VisualTreeHelper::GetChildrenCount(*host);
    for (std::uint32_t index = 0U; index < visualCount && header == nullptr;
         ++index) {
        Aero::Media::Visual* child =
            Aero::Media::VisualTreeHelper::GetChild(*host, index);
        if (child == nullptr) continue;
        header = TryCast<ToggleButton>(child);
        if (header != nullptr) break;
        const std::uint32_t nestedCount =
            Aero::Media::VisualTreeHelper::GetChildrenCount(*child);
        for (std::uint32_t nested = 0U;
             nested < nestedCount && header == nullptr;
             ++nested) {
            Aero::Media::Visual* nestedChild =
                Aero::Media::VisualTreeHelper::GetChild(*child, nested);
            header = nestedChild != nullptr
                ? TryCast<ToggleButton>(nestedChild)
                : nullptr;
        }
    }
    CHECK(header != nullptr);
    const Aero::Nullable<bool> checkedState = header->GetIsChecked();
    CHECK(checkedState.GetHasValue() && checkedState.GetValue());

    header->SetIsChecked(Aero::Nullable<bool>{false});
    Pump(view, 0.048);
    if (host->GetIsExpanded()) {
        std::fprintf(stderr,
            "Expander IsExpanded stayed true after ToggleButton uncheck "
            "(Binding Mode did not write back)\n");
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(!host->GetIsExpanded());
    return true;
}

bool TestExpanderUnnamedHeaderClickWritesBack() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({240.0, 200.0});

    constexpr char kTree[] =
        "<Expander xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"Host\" Header=\"Position\" IsExpanded=\"True\">"
        "<Expander.Template>"
        "<ControlTemplate TargetType=\"{x:Type Expander}\">"
        "<Grid>"
        "<Grid.RowDefinitions>"
        "<RowDefinition Height=\"Auto\"/>"
        "<RowDefinition Height=\"Auto\"/>"
        "</Grid.RowDefinitions>"
        "<Border x:Name=\"ContentBorder\" Grid.Row=\"1\" Height=\"40\" "
        "Background=\"#FFB8B8B8\" Visibility=\"Collapsed\">"
        "<ContentPresenter/>"
        "</Border>"
        "<Grid>"
        "<ToggleButton Height=\"24\" "
        "IsChecked=\"{Binding IsExpanded, RelativeSource={RelativeSource TemplatedParent}}\">"
        "<ToggleButton.Template>"
        "<ControlTemplate TargetType=\"{x:Type ToggleButton}\">"
        "<Grid Background=\"#FFA0A0A0\">"
        "<Path Data=\"M0,0L5,4 0,8\" Fill=\"White\" Margin=\"8,8,0,0\" "
        "HorizontalAlignment=\"Left\" VerticalAlignment=\"Center\"/>"
        "</Grid>"
        "</ControlTemplate>"
        "</ToggleButton.Template>"
        "</ToggleButton>"
        "<ContentPresenter ContentSource=\"Header\" Margin=\"18,0,8,0\" "
        "IsHitTestVisible=\"False\" VerticalAlignment=\"Center\"/>"
        "</Grid>"
        "</Grid>"
        "<ControlTemplate.Triggers>"
        "<Trigger Property=\"IsExpanded\" Value=\"True\">"
        "<Setter TargetName=\"ContentBorder\" Property=\"Visibility\" Value=\"Visible\"/>"
        "</Trigger>"
        "</ControlTemplate.Triggers>"
        "</ControlTemplate>"
        "</Expander.Template>"
        "<TextBlock Text=\"Body\"/>"
        "</Expander>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "unnamed expander XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {240.0, 200.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);

    auto* host = TryCast<Expander>(view.GetContent());
    CHECK(host != nullptr);
    CHECK(host->GetIsExpanded());

    ToggleButton* header = nullptr;
    const auto findToggle = [&](auto& self, Aero::Media::Visual& visual)
        -> ToggleButton* {
        if (auto* toggle = TryCast<ToggleButton>(&visual)) return toggle;
        const std::uint32_t count =
            Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
        for (std::uint32_t index = 0U; index < count; ++index) {
            Aero::Media::Visual* child =
                Aero::Media::VisualTreeHelper::GetChild(visual, index);
            if (child == nullptr) continue;
            if (ToggleButton* found = self(self, *child)) return found;
        }
        return nullptr;
    };
    header = findToggle(findToggle, *host);
    CHECK(header != nullptr);
    CHECK(header->GetIsChecked().GetHasValue() &&
        header->GetIsChecked().GetValue());

    const Aero::Size headerSize = header->GetRenderSize();
    CHECK(headerSize.width > 0.0 && headerSize.height > 0.0);
    Point headerScreen{};
    CHECK(header->TryPointToScreen({8.0, headerSize.height * 0.5}, headerScreen));
    static_cast<void>(view.MouseMove(
        static_cast<int>(headerScreen.x), static_cast<int>(headerScreen.y)));
    Pump(view, 0.032);
    static_cast<void>(view.MouseButtonDown(
        static_cast<int>(headerScreen.x), static_cast<int>(headerScreen.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.016);
    static_cast<void>(view.MouseButtonUp(
        static_cast<int>(headerScreen.x), static_cast<int>(headerScreen.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.048);
    if (host->GetIsExpanded()) {
        std::fprintf(stderr,
            "unnamed expander header click did not collapse over=%d checked=%d "
            "header=(%.1fx%.1f)\n",
            header->GetIsMouseOver() ? 1 : 0,
            header->GetIsChecked().GetValueOr(true) ? 1 : 0,
            headerSize.width, headerSize.height);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(!host->GetIsExpanded());
    return true;
}

bool TestExpanderResourceDictionaryHeaderClick() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({240.0, 200.0});

    constexpr char kTree[] =
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid.Resources>"
        "<ControlTemplate x:Key=\"ExpanderT\" TargetType=\"{x:Type Expander}\">"
        "<Grid>"
        "<Grid.RowDefinitions>"
        "<RowDefinition Height=\"Auto\"/>"
        "<RowDefinition Height=\"Auto\"/>"
        "</Grid.RowDefinitions>"
        "<Border x:Name=\"ContentBorder\" Grid.Row=\"1\" Height=\"40\" "
        "Background=\"#FFB8B8B8\" Visibility=\"Collapsed\">"
        "<ContentPresenter/>"
        "</Border>"
        "<Grid>"
        "<ToggleButton Height=\"24\" "
        "IsChecked=\"{Binding IsExpanded, RelativeSource={RelativeSource TemplatedParent}}\">"
        "<ToggleButton.Template>"
        "<ControlTemplate TargetType=\"{x:Type ToggleButton}\">"
        "<Border Background=\"#FFA0A0A0\"/>"
        "</ControlTemplate>"
        "</ToggleButton.Template>"
        "</ToggleButton>"
        "<ContentPresenter ContentSource=\"Header\" Margin=\"18,0,8,0\" "
        "IsHitTestVisible=\"False\" VerticalAlignment=\"Center\"/>"
        "</Grid>"
        "</Grid>"
        "<ControlTemplate.Triggers>"
        "<Trigger Property=\"IsExpanded\" Value=\"True\">"
        "<Setter TargetName=\"ContentBorder\" Property=\"Visibility\" Value=\"Visible\"/>"
        "</Trigger>"
        "</ControlTemplate.Triggers>"
        "</ControlTemplate>"
        "<Style TargetType=\"{x:Type Expander}\">"
        "<Setter Property=\"Template\" Value=\"{StaticResource ExpanderT}\"/>"
        "</Style>"
        "</Grid.Resources>"
        "<Expander x:Name=\"Host\" Header=\"Position\" IsExpanded=\"True\">"
        "<TextBlock Text=\"Body\"/>"
        "</Expander>"
        "</Grid>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "resource expander XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {240.0, 200.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    auto* host = root->FindName<Expander>(StringView("Host"));
    CHECK(host != nullptr);
    CHECK(host->GetIsExpanded());

    ToggleButton* header = nullptr;
    const auto findToggle = [&](auto& self, Aero::Media::Visual& visual)
        -> ToggleButton* {
        if (auto* toggle = TryCast<ToggleButton>(&visual)) return toggle;
        const std::uint32_t count =
            Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
        for (std::uint32_t index = 0U; index < count; ++index) {
            Aero::Media::Visual* child =
                Aero::Media::VisualTreeHelper::GetChild(visual, index);
            if (child == nullptr) continue;
            if (ToggleButton* found = self(self, *child)) return found;
        }
        return nullptr;
    };
    header = findToggle(findToggle, *host);
    CHECK(header != nullptr);
    header->SetIsChecked(Aero::Nullable<bool>{false});
    Pump(view, 0.048);
    if (host->GetIsExpanded()) {
        std::fprintf(stderr,
            "resource-dictionary Expander IsExpanded stayed true after uncheck\n");
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(!host->GetIsExpanded());
    return true;
}

bool TestControlTemplateHoverStoryboard() {
    ViewOptions options;
    options.automaticAnimationClock = false;
    LiveGui* live = NewLiveGui(options);
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({200.0, 80.0});

    constexpr char kTree[] =
        "<Button xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"Add\" Content=\"Add\" Width=\"120\" Height=\"32\">"
        "<Button.Template>"
        "<ControlTemplate TargetType=\"{x:Type Button}\">"
        "<ControlTemplate.Resources>"
        "<Storyboard x:Key=\"OverOn\">"
        "<DoubleAnimationUsingKeyFrames Storyboard.TargetProperty=\"(UIElement.Opacity)\" "
        "Storyboard.TargetName=\"BgOver\">"
        "<EasingDoubleKeyFrame KeyTime=\"0:0:0\" Value=\"1\"/>"
        "</DoubleAnimationUsingKeyFrames>"
        "</Storyboard>"
        "</ControlTemplate.Resources>"
        "<Grid>"
        "<Border x:Name=\"Bg\" Background=\"#FFA0A0A0\"/>"
        "<Border x:Name=\"BgOver\" Background=\"#FFFF0000\" Opacity=\"0\"/>"
        "<ContentPresenter HorizontalAlignment=\"Center\" VerticalAlignment=\"Center\"/>"
        "</Grid>"
        "<ControlTemplate.Triggers>"
        "<Trigger Property=\"IsMouseOver\" Value=\"True\">"
        "<Trigger.EnterActions>"
        "<BeginStoryboard Storyboard=\"{StaticResource OverOn}\"/>"
        "</Trigger.EnterActions>"
        "</Trigger>"
        "</ControlTemplate.Triggers>"
        "</ControlTemplate>"
        "</Button.Template>"
        "</Button>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "control template hover XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {200.0, 80.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);

    auto* add = TryCast<Button>(view.GetContent());
    CHECK(add != nullptr);
    const Aero::Size addSize = add->GetRenderSize();
    CHECK(addSize.width > 0.0 && addSize.height > 0.0);
    Point addScreen{};
    CHECK(add->TryPointToScreen(
        {addSize.width * 0.5, addSize.height * 0.5}, addScreen));
    static_cast<void>(view.MouseMove(
        static_cast<int>(addScreen.x), static_cast<int>(addScreen.y)));
    Pump(view, 0.048);
    CHECK(add->GetIsMouseOver());

    Border* over = nullptr;
    if (Aero::Media::VisualTreeHelper::GetChildrenCount(*add) > 0U) {
        Aero::Media::Visual* templateRoot =
            Aero::Media::VisualTreeHelper::GetChild(*add, 0U);
        if (templateRoot != nullptr) {
            const std::uint32_t nested =
                Aero::Media::VisualTreeHelper::GetChildrenCount(*templateRoot);
            for (std::uint32_t index = 0U; index < nested; ++index) {
                auto* border = TryCast<Border>(
                    Aero::Media::VisualTreeHelper::GetChild(*templateRoot, index));
                if (border == nullptr) continue;
                if (over == nullptr) {
                    over = border;
                    continue;
                }
                over = border;
                break;
            }
        }
    }
    CHECK(over != nullptr);
    if (over->GetOpacity() <= 0.5) {
        std::fprintf(stderr,
            "control template hover storyboard did not raise BgOver opacity=%.2f\n",
            over->GetOpacity());
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(over->GetOpacity() > 0.5);
    return true;
}

struct BlendAddClickProbe {
    std::uint32_t count = 0;
    void OnClick(Aero::Base::Object*, Aero::RoutedEventArgs&) noexcept {
        ++count;
    }
};

bool TestGridStarRowsSizeInStackPanel() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({240.0, 400.0});

    constexpr char kTree[] =
        "<StackPanel xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "Width=\"150\">"
        "<Grid x:Name=\"ColorGrid\">"
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
        "<Rectangle x:Name=\"Swatch\" Grid.RowSpan=\"4\" Fill=\"Red\" "
        "Stroke=\"Black\" Margin=\"0,0,5,0\"/>"
        "<Slider x:Name=\"R\" Grid.Column=\"1\" Height=\"18\" Margin=\"10,1,0,1\" "
        "Maximum=\"255\"/>"
        "</Grid>"
        "</StackPanel>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "star-grid XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {240.0, 400.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);
    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    Grid* grid = root->FindName<Grid>(StringView("ColorGrid"));
    Rectangle* swatch = root->FindName<Rectangle>(StringView("Swatch"));
    CHECK(grid != nullptr && swatch != nullptr);
    if (grid->GetRenderSize().height < 8.0 ||
        swatch->GetRenderSize().height < 8.0) {
        std::fprintf(stderr,
            "star Grid in StackPanel collapsed grid=(%.1fx%.1f) swatch=(%.1fx%.1f)\n",
            grid->GetRenderSize().width, grid->GetRenderSize().height,
            swatch->GetRenderSize().width, swatch->GetRenderSize().height);
    }
    CHECK(grid->GetRenderSize().height >= 8.0);
    CHECK(swatch->GetRenderSize().height >= 8.0);
    return true;
}

bool TestBlendTutorialSidebarInteractions() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({800.0, 600.0});
    ViewViewport viewport;
    viewport.logicalSize = {800.0, 600.0};
    viewport.pixelWidth = 800U;
    viewport.pixelHeight = 600U;
    viewport.dpiScale = 1.0;
    static_cast<void>(view.SetViewport(viewport));

    constexpr char kTree[] =
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid.Resources>"
        "<SolidColorBrush x:Key=\"Bg\" Color=\"#FFA0A0A0\"/>"
        "<SolidColorBrush x:Key=\"Over\" Color=\"#FFFF0000\"/>"
        "<ControlTemplate x:Key=\"HoverBtn\" TargetType=\"{x:Type Button}\">"
        "<Grid>"
        "<Border x:Name=\"Bg\" Background=\"{StaticResource Bg}\" Height=\"24\"/>"
        "<Border x:Name=\"BgOver\" Background=\"{StaticResource Over}\" Opacity=\"0\"/>"
        "<ContentPresenter HorizontalAlignment=\"Center\" VerticalAlignment=\"Center\"/>"
        "</Grid>"
        "<ControlTemplate.Triggers>"
        "<Trigger Property=\"IsMouseOver\" Value=\"True\">"
        "<Setter TargetName=\"BgOver\" Property=\"Opacity\" Value=\"1\"/>"
        "</Trigger>"
        "</ControlTemplate.Triggers>"
        "</ControlTemplate>"
        "<ControlTemplate x:Key=\"ExpanderT\" TargetType=\"{x:Type Expander}\">"
        "<StackPanel>"
        "<ToggleButton x:Name=\"HeaderButton\" Height=\"24\" Background=\"#FFA0A0A0\" "
        "IsChecked=\"{Binding IsExpanded, RelativeSource={RelativeSource TemplatedParent}}\"/>"
        "<Border x:Name=\"ContentBorder\" Background=\"#FFB8B8B8\">"
        "<ContentPresenter/>"
        "</Border>"
        "</StackPanel>"
        "<ControlTemplate.Triggers>"
        "<Trigger Property=\"IsExpanded\" Value=\"False\">"
        "<Setter TargetName=\"ContentBorder\" Property=\"Visibility\" Value=\"Collapsed\"/>"
        "</Trigger>"
        "</ControlTemplate.Triggers>"
        "</ControlTemplate>"
        "<ControlTemplate x:Key=\"SliderT\" TargetType=\"{x:Type Slider}\">"
        "<Border Background=\"#FF696969\" Height=\"20\">"
        "<Track x:Name=\"PART_Track\">"
        "<Track.Thumb>"
        "<Thumb x:Name=\"thumb\" Width=\"10\" Height=\"16\" Background=\"White\"/>"
        "</Track.Thumb>"
        "</Track>"
        "</Border>"
        "</ControlTemplate>"
        "</Grid.Resources>"
        "<Grid Margin=\"10\">"
        "<Grid.RowDefinitions>"
        "<RowDefinition Height=\"369*\"/>"
        "<RowDefinition Height=\"631*\"/>"
        "</Grid.RowDefinitions>"
        "<Decorator x:Name=\"PropsBarMaxWidth\" Grid.Column=\"1\"/>"
        "</Grid>"
        "<DockPanel LastChildFill=\"True\">"
        "<Viewbox x:Name=\"PropsBar\" DockPanel.Dock=\"Right\" VerticalAlignment=\"Top\" "
        "Margin=\"0,10,10,10\" MaxWidth=\"{Binding ActualHeight, ElementName=PropsBarMaxWidth}\">"
        "<StackPanel Width=\"150\">"
        "<Button x:Name=\"AddButton\" Content=\"Add\" Height=\"24\" Margin=\"0,0,0,3\" "
        "Template=\"{StaticResource HoverBtn}\"/>"
        "<Expander x:Name=\"PositionExpander\" Header=\"Position\" IsExpanded=\"True\" "
        "Template=\"{StaticResource ExpanderT}\" Margin=\"0,0,0,10\">"
        "<Slider x:Name=\"PositionLeft\" Minimum=\"0\" Maximum=\"100\" Value=\"10\" "
        "Template=\"{StaticResource SliderT}\"/>"
        "</Expander>"
        "</StackPanel>"
        "</Viewbox>"
        "<Border x:Name=\"ContainerBorder\" Background=\"White\" Margin=\"10\" "
        "BorderBrush=\"Gray\" BorderThickness=\"1\">"
        "<Canvas x:Name=\"ContainerCanvas\"/>"
        "</Border>"
        "</DockPanel>"
        "</Grid>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "blend sidebar XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {800.0, 600.0}));
    Pump(view, 0.016);
    Pump(view, 0.048);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    Button* add = root->FindName<Button>(StringView("AddButton"));
    Expander* expander = root->FindName<Expander>(StringView("PositionExpander"));
    Slider* slider = root->FindName<Slider>(StringView("PositionLeft"));
    CHECK(add != nullptr && expander != nullptr && slider != nullptr);

    BlendAddClickProbe probe;
    add->Click().Add({&probe, &BlendAddClickProbe::OnClick});

    const Aero::Size addSize = add->GetRenderSize();
    CHECK(addSize.width > 0.0 && addSize.height > 0.0);
    Point addScreen{};
    CHECK(add->TryPointToScreen(
        {addSize.width * 0.5, addSize.height * 0.5}, addScreen));
    static_cast<void>(view.MouseMove(
        static_cast<int>(addScreen.x), static_cast<int>(addScreen.y)));
    Pump(view, 0.048);
    Border* over = nullptr;
    if (Aero::Media::VisualTreeHelper::GetChildrenCount(*add) > 0U) {
        Aero::Media::Visual* templateRoot =
            Aero::Media::VisualTreeHelper::GetChild(*add, 0U);
        if (templateRoot != nullptr) {
            const std::uint32_t nested =
                Aero::Media::VisualTreeHelper::GetChildrenCount(*templateRoot);
            for (std::uint32_t index = 0U; index < nested; ++index) {
                auto* border = TryCast<Border>(
                    Aero::Media::VisualTreeHelper::GetChild(*templateRoot, index));
                if (border == nullptr) continue;
                if (over == nullptr) {
                    over = border;
                    continue;
                }
                over = border;
                break;
            }
        }
    }
    if (!add->GetIsMouseOver()) {
        std::fprintf(stderr,
            "blend Add hover miss screen=(%.1f,%.1f) size=(%.1fx%.1f) "
            "slot=(%.1f,%.1f,%.1f,%.1f)\n",
            addScreen.x, addScreen.y, addSize.width, addSize.height,
            add->GetLayoutSlot().x, add->GetLayoutSlot().y,
            add->GetLayoutSlot().width, add->GetLayoutSlot().height);
    }
    CHECK(add->GetIsMouseOver());
    if (over != nullptr) {
        CHECK(over->GetOpacity() > 0.5);
    }

    static_cast<void>(view.MouseButtonDown(
        static_cast<int>(addScreen.x), static_cast<int>(addScreen.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.016);
    static_cast<void>(view.MouseButtonUp(
        static_cast<int>(addScreen.x), static_cast<int>(addScreen.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.032);
    if (probe.count == 0U) {
        std::fprintf(stderr,
            "blend Add click did not fire pressed=%d over=%d\n",
            add->GetIsPressed() ? 1 : 0,
            add->GetIsMouseOver() ? 1 : 0);
    }
    CHECK(probe.count >= 1U);

    CHECK(expander->GetIsExpanded());
    ToggleButton* header = nullptr;
    const auto findToggle = [&](auto& self, Aero::Media::Visual& visual)
        -> ToggleButton* {
        if (auto* toggle = TryCast<ToggleButton>(&visual)) return toggle;
        const std::uint32_t count =
            Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
        for (std::uint32_t index = 0U; index < count; ++index) {
            Aero::Media::Visual* child =
                Aero::Media::VisualTreeHelper::GetChild(visual, index);
            if (child == nullptr) continue;
            if (ToggleButton* found = self(self, *child)) return found;
        }
        return nullptr;
    };
    header = findToggle(findToggle, *expander);
    CHECK(header != nullptr);

    static_cast<void>(view.MouseMove(0, 0));
    Pump(view, 0.016);

    const Aero::Size headerSize = header->GetRenderSize();
    CHECK(headerSize.width > 0.0 && headerSize.height > 0.0);
    Point headerScreen{};
    Point headerOrigin{};
    CHECK(header->TryPointToScreen({0.0, 0.0}, headerOrigin));
    CHECK(header->TryPointToScreen(
        {headerSize.width * 0.5, headerSize.height * 0.5}, headerScreen));
    static_cast<void>(view.MouseMove(
        static_cast<int>(headerScreen.x), static_cast<int>(headerScreen.y)));
    Pump(view, 0.032);
    std::fprintf(stderr,
        "blend expander before-slider headerOver=%d expanderOver=%d "
        "sliderOver=%d addOver=%d headerScreen=(%.1f,%.1f) "
        "hdrOrigin=(%.1f,%.1f) captured=%d arrange=%d\n",
        header->GetIsMouseOver() ? 1 : 0,
        expander->GetIsMouseOver() ? 1 : 0,
        slider->GetIsMouseOver() ? 1 : 0,
        add->GetIsMouseOver() ? 1 : 0,
        headerScreen.x, headerScreen.y,
        headerOrigin.x, headerOrigin.y,
        Aero::Input::Mouse::Captured() != nullptr ? 1 : 0,
        header->GetIsArrangeValid() ? 1 : 0);
    static_cast<void>(view.MouseButtonDown(
        static_cast<int>(headerScreen.x), static_cast<int>(headerScreen.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.016);
    static_cast<void>(view.MouseButtonUp(
        static_cast<int>(headerScreen.x), static_cast<int>(headerScreen.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.048);
    if (expander->GetIsExpanded()) {
        std::fprintf(stderr,
            "blend Expander did not collapse after header click "
            "headerOver=%d expanderOver=%d addOver=%d checked=%d "
            "headerScreen=(%.1f,%.1f) addScreen=(%.1f,%.1f) "
            "headerSize=(%.1fx%.1f) expanderSize=(%.1fx%.1f)\n",
            header->GetIsMouseOver() ? 1 : 0,
            expander->GetIsMouseOver() ? 1 : 0,
            add->GetIsMouseOver() ? 1 : 0,
            header->GetIsChecked().GetValueOr(true) ? 1 : 0,
            headerScreen.x, headerScreen.y,
            addScreen.x, addScreen.y,
            headerSize.width, headerSize.height,
            expander->GetRenderSize().width, expander->GetRenderSize().height);
    }
    CHECK(!expander->GetIsExpanded());
    expander->SetIsExpanded(true);
    Pump(view, 0.048);

    const double start = slider->GetValue();
    const Aero::Size sliderSize = slider->GetRenderSize();
    CHECK(sliderSize.width > 8.0);
    Point sliderScreen{};
    CHECK(slider->TryPointToScreen(
        {sliderSize.width * 0.85, sliderSize.height * 0.5}, sliderScreen));
    static_cast<void>(view.MouseMove(
        static_cast<int>(sliderScreen.x), static_cast<int>(sliderScreen.y)));
    Pump(view, 0.032);
    static_cast<void>(view.MouseButtonDown(
        static_cast<int>(sliderScreen.x), static_cast<int>(sliderScreen.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.016);
    static_cast<void>(view.MouseButtonUp(
        static_cast<int>(sliderScreen.x), static_cast<int>(sliderScreen.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.032);
    if (!(slider->GetValue() > start + 1.0)) {
        std::fprintf(stderr,
            "blend Slider did not follow click start=%.1f now=%.1f "
            "size=(%.1fx%.1f) over=%d screen=(%.1f,%.1f)\n",
            start, slider->GetValue(), sliderSize.width, sliderSize.height,
            slider->GetIsMouseOver() ? 1 : 0,
            sliderScreen.x, sliderScreen.y);
    }
    CHECK(slider->GetValue() > start + 1.0);
    return true;
}

bool TestLoadComponentUserControlInStackPanel() {
    LiveGui* live = NewTutorialLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({240.0, 160.0});

    Result<Ref<NumericUpDown>> control = MakeRef<NumericUpDown>();
    CHECK(control);
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> host = reader.Parse(StringView(
        "<StackPanel xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" Width=\"200\"/>"));
    CHECK(host);
    CHECK(view.SetContent(std::move(host).Value(), {240.0, 160.0}));
    Pump(view, 0.016);
    auto* stack = TryCast<StackPanel>(view.GetContent());
    CHECK(stack != nullptr);
    CHECK(stack->GetChildren().Add(
        Ref<Aero::UIElement>(control.Value())));
    Pump(view, 0.032);
    CHECK(live->gui.LoadComponent(
        *control.Value(), "memory:///NumericUpDown.xaml"));
    Pump(view, 0.016);
    Pump(view, 0.032);
    if (control.Value()->GetRenderSize().height < 1.0) {
        Aero::Media::Visual* child =
            Aero::Media::VisualTreeHelper::GetChildrenCount(*control.Value()) > 0U
            ? Aero::Media::VisualTreeHelper::GetChild(*control.Value(), 0U)
            : nullptr;
        auto* childElement = TryCast<UIElement>(child);
        std::fprintf(stderr,
            "LoadComponent UserControl in StackPanel stayed 0 height "
            "size=(%.1fx%.1f) visualChildren=%u "
            "childLayout=%d childParent=%d childSize=(%.1fx%.1f)\n",
            control.Value()->GetRenderSize().width,
            control.Value()->GetRenderSize().height,
            Aero::Media::VisualTreeHelper::GetChildrenCount(*control.Value()),
            childElement != nullptr && childElement->GetIsLayoutAttached() ? 1 : 0,
            childElement != nullptr && childElement->LayoutParent() ==
                control.Value().Get() ? 1 : 0,
            childElement != nullptr ? childElement->GetRenderSize().width : 0.0,
            childElement != nullptr ? childElement->GetRenderSize().height : 0.0);
    }
    CHECK(control.Value()->GetRenderSize().height >= 1.0);
    CHECK(control.Value()->FindName("UpButton") != nullptr);
    return true;
}

bool TestLoadComponentStarGridUserControlInStackPanel() {
    LiveGui* live = NewTutorialLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({240.0, 200.0});

    Result<Ref<UserControl>> control = MakeRef<UserControl>();
    CHECK(control);
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> host = reader.Parse(StringView(
        "<StackPanel xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" Width=\"200\"/>"));
    CHECK(host);
    CHECK(view.SetContent(std::move(host).Value(), {240.0, 200.0}));
    Pump(view, 0.016);
    auto* stack = TryCast<StackPanel>(view.GetContent());
    CHECK(stack != nullptr);
    CHECK(stack->GetChildren().Add(
        Ref<Aero::UIElement>(control.Value())));
    Pump(view, 0.032);
    CHECK(live->gui.LoadComponent(
        *control.Value(), "memory:///StarColorGrid.xaml"));
    Pump(view, 0.016);
    Pump(view, 0.032);
    if (control.Value()->GetRenderSize().height < 8.0) {
        std::fprintf(stderr,
            "LoadComponent star-grid UserControl stayed 0 height size=(%.1fx%.1f)\n",
            control.Value()->GetRenderSize().width,
            control.Value()->GetRenderSize().height);
    }
    CHECK(control.Value()->GetRenderSize().height >= 8.0);
    CHECK(control.Value()->FindName("R") != nullptr);
    return true;
}

TextBlock* FindTextBlock(Aero::Media::Visual& visual) noexcept {
    if (auto* text = TryCast<TextBlock>(&visual)) {
        return text;
    }
    const std::uint32_t count =
        Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
    for (std::uint32_t index = 0U; index < count; ++index) {
        Aero::Media::Visual* child =
            Aero::Media::VisualTreeHelper::GetChild(visual, index);
        if (child == nullptr) continue;
        if (TextBlock* found = FindTextBlock(*child)) {
            return found;
        }
    }
    return nullptr;
}

Path* FindFirstPath(Aero::Media::Visual& visual) {
    if (auto* path = TryCast<Path>(&visual)) {
        return path;
    }
    const std::uint32_t count =
        Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
    for (std::uint32_t index = 0U; index < count; ++index) {
        Aero::Media::Visual* child =
            Aero::Media::VisualTreeHelper::GetChild(visual, index);
        if (child == nullptr) continue;
        if (Path* found = FindFirstPath(*child)) {
            return found;
        }
    }
    return nullptr;
}

struct RepeatClickProbe {
    std::uint32_t count = 0;
    void OnClick(Aero::Base::Object*, Aero::RoutedEventArgs&) noexcept {
        ++count;
    }
};

bool TestMenuTooltipForegroundAndFocus() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({800.0, 600.0});

    constexpr char kTree[] =
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"LayoutRoot\" FocusManager.IsFocusScope=\"True\" Foreground=\"White\">"
        "<StackPanel>"
        "<ToggleButton x:Name=\"Start\" Content=\"START GAME\">"
        "<ToggleButton.Style>"
        "<Style TargetType=\"{x:Type ToggleButton}\">"
        "<Style.Triggers>"
        "<Trigger Property=\"IsKeyboardFocused\" Value=\"True\">"
        "<Setter Property=\"IsChecked\" Value=\"True\"/>"
        "</Trigger>"
        "</Style.Triggers>"
        "</Style>"
        "</ToggleButton.Style>"
        "</ToggleButton>"
        "<ContentControl x:Name=\"Tip\" Content=\"Dive straight into the adventure.\">"
        "<ContentControl.Style>"
        "<Style TargetType=\"{x:Type ContentControl}\">"
        "<Setter Property=\"OverridesDefaultStyle\" Value=\"True\"/>"
        "<Setter Property=\"Template\">"
        "<Setter.Value>"
        "<ControlTemplate TargetType=\"{x:Type ContentControl}\">"
        "<ContentPresenter x:Name=\"ContentHost\">"
        "<TextElement.Foreground>"
        "<SolidColorBrush Color=\"White\" Opacity=\"0\"/>"
        "</TextElement.Foreground>"
        "</ContentPresenter>"
        "</ControlTemplate>"
        "</Setter.Value>"
        "</Setter>"
        "</Style>"
        "</ContentControl.Style>"
        "</ContentControl>"
        "</StackPanel>"
        "</Grid>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "menu tooltip XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {800.0, 600.0}));
    Pump(view, 0.032);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    auto* layoutRoot = root->FindName<Grid>(StringView("LayoutRoot"));
    auto* start = root->FindName<ToggleButton>(StringView("Start"));
    auto* tip = root->FindName<ContentControl>(StringView("Tip"));
    CHECK(layoutRoot != nullptr && start != nullptr && tip != nullptr);
    static_cast<void>(tip->ApplyTemplate());
    Pump(view, 0.032);

    TextBlock* text = FindTextBlock(*tip);
    CHECK(text != nullptr);
    Ref<Aero::Media::Brush> foreground = text->GetForeground();
    CHECK(foreground);
    if (!(foreground->GetOpacity() < 0.05)) {
        std::fprintf(stderr,
            "tooltip TextElement.Foreground Opacity leaked through inherited "
            "Foreground opacity=%.3f\n",
            foreground->GetOpacity());
    }
    CHECK(foreground->GetOpacity() < 0.05);

    Result<bool> scoped = layoutRoot->Focus();
    CHECK(scoped);
    CHECK(scoped.Value());
    Pump(view, 0.016);
    CHECK(start->GetIsKeyboardFocused());
    CHECK(!layoutRoot->GetIsKeyboardFocused());
    CHECK(start->GetIsChecked().GetHasValue());
    CHECK(start->GetIsChecked().GetValue());

    Result<bool> layoutFocused = layoutRoot->Focus();
    CHECK(layoutFocused);
    CHECK(!layoutFocused.Value());
    Pump(view, 0.016);
    CHECK(start->GetIsKeyboardFocused());
    CHECK(!layoutRoot->GetIsKeyboardFocused());
    return true;
}

bool TestViewboxTransform3DButtonHit() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({800.0, 600.0});

    constexpr char kTree[] =
        "<Viewbox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid x:Name=\"Host\" Width=\"400\" Height=\"300\" Background=\"#FF102030\">"
        "<Button x:Name=\"HitButton\" Width=\"200\" Height=\"80\" "
        "HorizontalAlignment=\"Center\" VerticalAlignment=\"Center\" "
        "Background=\"#FF3E4146\" Content=\"START GAME\"/>"
        "</Grid>"
        "</Viewbox>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    CHECK(document);
    Grid* host = document.Value().FindName<Grid>("Host");
    CHECK(host != nullptr);
    Result<Ref<CompositeTransform3D>> spin = MakeRef<CompositeTransform3D>();
    CHECK(spin);
    spin.Value()->SetRotationY(-8.0);
    host->SetTransform3D(spin.Value());
    CHECK(view.SetContent(std::move(document).Value(), {800.0, 600.0}));
    Pump(view, 0.032);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    Button* button = root->FindName<Button>(StringView("HitButton"));
    CHECK(button != nullptr);
    const Aero::Size buttonSize = button->GetRenderSize();
    CHECK(buttonSize.width > 0.0 && buttonSize.height > 0.0);

    bool hovered = false;
    for (int y = 40; y < 560 && !hovered; y += 16) {
        for (int x = 40; x < 760 && !hovered; x += 16) {
            static_cast<void>(view.MouseMove(x, y));
            Pump(view, 0.008);
            hovered = button->GetIsMouseOver();
        }
    }
    if (!hovered) {
        Point screen{};
        static_cast<void>(button->TryPointToScreen(
            {buttonSize.width * 0.5, buttonSize.height * 0.5}, screen));
        std::fprintf(stderr,
            "3D viewbox button was not hittable screen=(%.1f,%.1f) size=(%.1f,%.1f)\n",
            screen.x, screen.y, buttonSize.width, buttonSize.height);
    }
    CHECK(hovered);
    return true;
}

bool TestKeyboardNavigationIsTabStopTemplateTrigger() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({400.0, 200.0});

    constexpr char kTree[] =
        "<StackPanel xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel.Resources>"
        "<ControlTemplate x:Key=\"MenuTemplate\" TargetType=\"{x:Type ToggleButton}\">"
        "<Grid>"
        "<Path x:Name=\"CircledArrow\" Data=\"M0,0 L8,4 L0,8 z\" Width=\"16\" Height=\"16\" "
        "Fill=\"White\" Visibility=\"Visible\"/>"
        "</Grid>"
        "<ControlTemplate.Triggers>"
        "<Trigger Property=\"KeyboardNavigation.IsTabStop\" Value=\"False\">"
        "<Setter Property=\"Visibility\" TargetName=\"CircledArrow\" Value=\"Collapsed\"/>"
        "</Trigger>"
        "</ControlTemplate.Triggers>"
        "</ControlTemplate>"
        "</StackPanel.Resources>"
        "<ToggleButton x:Name=\"Menu\" Width=\"80\" Height=\"40\" "
        "Template=\"{StaticResource MenuTemplate}\"/>"
        "<ToggleButton x:Name=\"Setting\" Width=\"80\" Height=\"40\" IsTabStop=\"False\" "
        "Template=\"{StaticResource MenuTemplate}\"/>"
        "</StackPanel>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "IsTabStop trigger XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {400.0, 200.0}));
    Pump(view, 0.032);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    auto* menu = root->FindName<ToggleButton>(StringView("Menu"));
    auto* setting = root->FindName<ToggleButton>(StringView("Setting"));
    CHECK(menu != nullptr && setting != nullptr);
    static_cast<void>(menu->ApplyTemplate());
    static_cast<void>(setting->ApplyTemplate());
    Pump(view, 0.016);

    CHECK(menu->GetIsTabStop());
    CHECK(menu->GetValueOr(KeyboardNavigation::IsTabStopProperty, false));
    CHECK(!setting->GetIsTabStop());
    CHECK(!setting->GetValueOr(KeyboardNavigation::IsTabStopProperty, true));

    Path* menuArrow = FindFirstPath(*menu);
    Path* settingArrow = FindFirstPath(*setting);
    if (menuArrow == nullptr || settingArrow == nullptr) {
        std::fprintf(stderr,
            "CircledArrow path missing menuChildren=%u settingChildren=%u\n",
            Aero::Media::VisualTreeHelper::GetChildrenCount(*menu),
            Aero::Media::VisualTreeHelper::GetChildrenCount(*setting));
    }
    CHECK(menuArrow != nullptr && settingArrow != nullptr);
    if (menuArrow->GetVisibility() != Visibility::Visible) {
        std::fprintf(stderr,
            "menu CircledArrow collapsed despite IsTabStop=true\n");
    }
    CHECK(menuArrow->GetVisibility() == Visibility::Visible);
    CHECK(settingArrow->GetVisibility() == Visibility::Collapsed);
    return true;
}

double PathFillOpacity(Path& path) {
    Ref<Aero::Media::Brush> fill = path.GetFill();
    return fill ? fill->GetOpacity() : -1.0;
}

bool TestCheckedVisualStateRevertsArrowOpacity() {
    ViewOptions options;
    options.automaticAnimationClock = false;
    LiveGui* live = NewLiveGui(options);
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({400.0, 200.0});

    constexpr char kTree[] =
        "<StackPanel xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<StackPanel.Resources>"
        "<ControlTemplate x:Key=\"MenuTemplate\" TargetType=\"{x:Type ToggleButton}\">"
        "<Grid>"
        "<VisualStateManager.VisualStateGroups>"
        "<VisualStateGroup x:Name=\"CheckStates\">"
        "<VisualStateGroup.Transitions>"
        "<VisualTransition From=\"Checked\" GeneratedDuration=\"0:0:0.2\"/>"
        "</VisualStateGroup.Transitions>"
        "<VisualState x:Name=\"Checked\">"
        "<Storyboard>"
        "<DoubleAnimationUsingKeyFrames Storyboard.TargetProperty=\"(Shape.Fill).(Brush.Opacity)\" "
        "Storyboard.TargetName=\"CircledArrow\">"
        "<EasingDoubleKeyFrame KeyTime=\"0\" Value=\"1\"/>"
        "</DoubleAnimationUsingKeyFrames>"
        "</Storyboard>"
        "</VisualState>"
        "<VisualState x:Name=\"Unchecked\"/>"
        "</VisualStateGroup>"
        "</VisualStateManager.VisualStateGroups>"
        "<Path x:Name=\"CircledArrow\" Data=\"M0,0 L8,4 L0,8 z\" Width=\"16\" Height=\"16\">"
        "<Path.Fill>"
        "<SolidColorBrush Color=\"White\" Opacity=\"0\"/>"
        "</Path.Fill>"
        "</Path>"
        "</Grid>"
        "</ControlTemplate>"
        "</StackPanel.Resources>"
        "<ToggleButton x:Name=\"Start\" Width=\"80\" Height=\"40\" "
        "Template=\"{StaticResource MenuTemplate}\">"
        "<ToggleButton.Style>"
        "<Style TargetType=\"{x:Type ToggleButton}\">"
        "<Style.Triggers>"
        "<Trigger Property=\"IsKeyboardFocused\" Value=\"True\">"
        "<Setter Property=\"IsChecked\" Value=\"True\"/>"
        "</Trigger>"
        "</Style.Triggers>"
        "</Style>"
        "</ToggleButton.Style>"
        "</ToggleButton>"
        "<ToggleButton x:Name=\"Settings\" Width=\"80\" Height=\"40\" "
        "Template=\"{StaticResource MenuTemplate}\">"
        "<ToggleButton.Style>"
        "<Style TargetType=\"{x:Type ToggleButton}\">"
        "<Style.Triggers>"
        "<Trigger Property=\"IsKeyboardFocused\" Value=\"True\">"
        "<Setter Property=\"IsChecked\" Value=\"True\"/>"
        "</Trigger>"
        "</Style.Triggers>"
        "</Style>"
        "</ToggleButton.Style>"
        "</ToggleButton>"
        "</StackPanel>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "checked VSM XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {400.0, 200.0}));
    Pump(view, 0.032);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    auto* start = root->FindName<ToggleButton>(StringView("Start"));
    auto* settings = root->FindName<ToggleButton>(StringView("Settings"));
    CHECK(start != nullptr && settings != nullptr);
    static_cast<void>(start->ApplyTemplate());
    static_cast<void>(settings->ApplyTemplate());
    Pump(view, 0.016);

    Path* startArrow = FindFirstPath(*start);
    Path* settingsArrow = FindFirstPath(*settings);
    CHECK(startArrow != nullptr && settingsArrow != nullptr);
    CHECK(PathFillOpacity(*startArrow) < 0.05);

    Result<bool> focused = start->Focus();
    CHECK(focused && focused.Value());
    static_cast<void>(VisualStateManager::GoToState(*start, "Checked", false));
    Pump(view, 0.05);
    CHECK(start->GetIsKeyboardFocused());
    CHECK(start->GetIsChecked().GetHasValue() && start->GetIsChecked().GetValue());
    if (!(PathFillOpacity(*startArrow) > 0.8)) {
        std::fprintf(stderr, "checked arrow opacity stayed %.3f\n",
            PathFillOpacity(*startArrow));
    }
    CHECK(PathFillOpacity(*startArrow) > 0.8);

    Result<bool> moved = settings->Focus();
    CHECK(moved && moved.Value());
    Pump(view, 0.35);
    CHECK(settings->GetIsKeyboardFocused());
    CHECK(!start->GetIsKeyboardFocused());
    if (start->GetIsChecked().GetHasValue() && start->GetIsChecked().GetValue()) {
        std::fprintf(stderr, "start IsChecked stayed true after focus moved\n");
    }
    CHECK(!(start->GetIsChecked().GetHasValue() && start->GetIsChecked().GetValue()));
    if (!(PathFillOpacity(*startArrow) < 0.15)) {
        std::fprintf(stderr,
            "unchecked arrow opacity stayed %.3f after leave\n",
            PathFillOpacity(*startArrow));
    }
    CHECK(PathFillOpacity(*startArrow) < 0.15);
    CHECK(PathFillOpacity(*settingsArrow) > 0.8);
    return true;
}

bool TestRepeatButtonPressClickAndScaleX() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({800.0, 600.0});

    constexpr char kTree[] =
        "<Viewbox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid x:Name=\"Host\" Width=\"400\" Height=\"80\" Background=\"#FF102030\">"
        "<RepeatButton x:Name=\"NextButton\" Width=\"80\" Height=\"48\" "
        "HorizontalAlignment=\"Right\" VerticalAlignment=\"Center\" "
        "Background=\"#FF3E4146\" RenderTransformOrigin=\"0.5,0.5\">"
        "<RepeatButton.RenderTransform>"
        "<ScaleTransform ScaleX=\"-1\"/>"
        "</RepeatButton.RenderTransform>"
        "</RepeatButton>"
        "</Grid>"
        "</Viewbox>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    CHECK(document);
    Grid* host = document.Value().FindName<Grid>("Host");
    CHECK(host != nullptr);
    Result<Ref<CompositeTransform3D>> spin = MakeRef<CompositeTransform3D>();
    CHECK(spin);
    spin.Value()->SetRotationY(-8.0);
    host->SetTransform3D(spin.Value());
    CHECK(view.SetContent(std::move(document).Value(), {800.0, 600.0}));
    Pump(view, 0.032);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    RepeatButton* button = root->FindName<RepeatButton>(StringView("NextButton"));
    CHECK(button != nullptr);
    CHECK(button->GetClickMode() == ClickMode::Press);

    RepeatClickProbe probe;
    button->Click().Add({&probe, &RepeatClickProbe::OnClick});

    bool hovered = false;
    int clickX = 0;
    int clickY = 0;
    for (int y = 40; y < 560 && !hovered; y += 8) {
        for (int x = 40; x < 760 && !hovered; x += 8) {
            static_cast<void>(view.MouseMove(x, y));
            Pump(view, 0.008);
            if (button->GetIsMouseOver()) {
                hovered = true;
                clickX = x;
                clickY = y;
            }
        }
    }
    if (!hovered) {
        const Aero::Size buttonSize = button->GetRenderSize();
        Point screen{};
        static_cast<void>(button->TryPointToScreen(
            {buttonSize.width * 0.5, buttonSize.height * 0.5}, screen));
        std::fprintf(stderr,
            "scaled RepeatButton was not hittable screen=(%.1f,%.1f) size=(%.1f,%.1f)\n",
            screen.x, screen.y, buttonSize.width, buttonSize.height);
    }
    CHECK(hovered);
    static_cast<void>(view.MouseButtonDown(
        clickX, clickY, Aero::Input::MouseButton::Left));
    Pump(view, 0.016);
    if (probe.count == 0U) {
        std::fprintf(stderr,
            "RepeatButton ClickMode=Press did not fire Click on MouseDown "
            "pressed=%d over=%d\n",
            button->GetIsPressed() ? 1 : 0,
            button->GetIsMouseOver() ? 1 : 0);
    }
    CHECK(probe.count >= 1U);
    static_cast<void>(view.MouseButtonUp(
        clickX, clickY, Aero::Input::MouseButton::Left));
    Pump(view, 0.016);
    return true;
}

bool TestComboBoxPopupItemClickSelects() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({800.0, 600.0});

    constexpr char kTree[] =
        "<Viewbox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid Width=\"400\" Height=\"300\">"
        "<Grid.Resources>"
        "<Style TargetType=\"{x:Type ComboBox}\">"
        "<Setter Property=\"Template\">"
        "<Setter.Value>"
        "<ControlTemplate TargetType=\"{x:Type ComboBox}\">"
        "<Grid>"
        "<ToggleButton IsChecked=\"{Binding IsDropDownOpen, Mode=TwoWay, "
        "RelativeSource={RelativeSource TemplatedParent}}\"/>"
        "<ContentPresenter x:Name=\"ContentSite\" ContentSource=\"SelectionBoxItem\" "
        "IsHitTestVisible=\"False\"/>"
        "<Popup x:Name=\"PART_Popup\" IsOpen=\"{Binding IsDropDownOpen, "
        "RelativeSource={RelativeSource TemplatedParent}}\" Placement=\"Bottom\" "
        "VerticalOffset=\"1\">"
        "<Border Background=\"#FF393B40\" MinWidth=\"{TemplateBinding ActualWidth}\">"
        "<ScrollViewer>"
        "<ItemsPresenter/>"
        "</ScrollViewer>"
        "</Border>"
        "</Popup>"
        "</Grid>"
        "</ControlTemplate>"
        "</Setter.Value>"
        "</Setter>"
        "</Style>"
        "</Grid.Resources>"
        "<ComboBox x:Name=\"HitCombo\" Width=\"140\" Height=\"28\" "
        "HorizontalAlignment=\"Left\" VerticalAlignment=\"Bottom\" "
        "Margin=\"8,0,0,8\">"
        "<ComboBoxItem Content=\"Alpha\"/>"
        "<ComboBoxItem Content=\"Beta\"/>"
        "<ComboBoxItem Content=\"Gamma\"/>"
        "</ComboBox>"
        "</Grid>"
        "</Viewbox>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "combo click XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {800.0, 600.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    ComboBox* combo = root->FindName<ComboBox>(StringView("HitCombo"));
    CHECK(combo != nullptr);
    static_cast<void>(combo->ApplyTemplate());
    combo->SetSelectedIndex(0U);
    Pump(view, 0.016);
    CHECK(combo->GetSelectedIndex() == 0U);

    combo->SetIsDropDownOpen(true);
    Pump(view, 0.064);
    Pump(view, 0.080);
    CHECK(combo->GetIsDropDownOpen());

    Panel* itemsHost = combo->GetItemsHost();
    CHECK(itemsHost != nullptr);
    CHECK(Aero::Media::VisualTreeHelper::GetChildrenCount(*itemsHost) >= 2U);
    Aero::Media::Visual* itemVisual =
        Aero::Media::VisualTreeHelper::GetChild(*itemsHost, 1U);
    auto* item = TryCast<ComboBoxItem>(itemVisual);
    CHECK(item != nullptr);
    const Aero::Size itemSize = item->GetRenderSize();
    CHECK(itemSize.width > 0.0 && itemSize.height > 0.0);
    Point itemScreen{};
    CHECK(item->TryPointToScreen(
        {itemSize.width * 0.5, itemSize.height * 0.5}, itemScreen));
    const int clickX = static_cast<int>(itemScreen.x);
    const int clickY = static_cast<int>(itemScreen.y);
    static_cast<void>(view.MouseMove(clickX, clickY));
    Pump(view, 0.032);
    if (!item->GetIsMouseOver()) {
        std::fprintf(stderr,
            "combo click hover miss screen=(%.1f,%.1f) click=(%d,%d)\n",
            itemScreen.x, itemScreen.y, clickX, clickY);
    }
    CHECK(item->GetIsMouseOver());
    static_cast<void>(view.MouseButtonDown(
        clickX, clickY, Aero::Input::MouseButton::Left));
    static_cast<void>(view.MouseButtonUp(
        clickX, clickY, Aero::Input::MouseButton::Left));
    Pump(view, 0.048);
    if (combo->GetSelectedIndex() != 1U) {
        std::fprintf(stderr,
            "combo click did not select index=%u dropDown=%d text=%.*s\n",
            combo->GetSelectedIndex(),
            combo->GetIsDropDownOpen() ? 1 : 0,
            static_cast<int>(combo->GetSelectionBoxText().View().SizeBytes()),
            combo->GetSelectionBoxText().View().Data());
    }
    CHECK(combo->GetSelectedIndex() == 1U);
    CHECK(!combo->GetIsDropDownOpen());
    return true;
}

bool TestComboBoxItemsSourcePopupClickSelects() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({800.0, 600.0});

    constexpr char kTree[] =
        "<Viewbox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid Width=\"400\" Height=\"300\">"
        "<Grid.Resources>"
        "<Style TargetType=\"{x:Type ComboBox}\">"
        "<Setter Property=\"Template\">"
        "<Setter.Value>"
        "<ControlTemplate TargetType=\"{x:Type ComboBox}\">"
        "<Grid>"
        "<ToggleButton IsChecked=\"{Binding IsDropDownOpen, Mode=TwoWay, "
        "RelativeSource={RelativeSource TemplatedParent}}\"/>"
        "<ContentPresenter x:Name=\"ContentSite\" ContentSource=\"SelectionBoxItem\" "
        "IsHitTestVisible=\"False\"/>"
        "<Popup x:Name=\"PART_Popup\" IsOpen=\"{Binding IsDropDownOpen, "
        "RelativeSource={RelativeSource TemplatedParent}}\" Placement=\"Bottom\" "
        "VerticalOffset=\"1\">"
        "<Border Background=\"#FF393B40\" MinWidth=\"{TemplateBinding ActualWidth}\">"
        "<ScrollViewer>"
        "<ItemsPresenter/>"
        "</ScrollViewer>"
        "</Border>"
        "</Popup>"
        "</Grid>"
        "</ControlTemplate>"
        "</Setter.Value>"
        "</Setter>"
        "</Style>"
        "</Grid.Resources>"
        "<ComboBox x:Name=\"HitCombo\" Width=\"140\" Height=\"28\" "
        "HorizontalAlignment=\"Left\" VerticalAlignment=\"Bottom\" "
        "Margin=\"8,0,0,8\"/>"
        "</Grid>"
        "</Viewbox>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "combo items-source click XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {800.0, 600.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    ComboBox* combo = root->FindName<ComboBox>(StringView("HitCombo"));
    CHECK(combo != nullptr);
    Result<Ref<ObservableObjectCollection>> teams =
        MakeRef<ObservableObjectCollection>();
    CHECK(teams);
    CHECK(Aero::Controls::AddBoxedStringItem(*teams.Value(), "Overall"));
    CHECK(Aero::Controls::AddBoxedStringItem(*teams.Value(), "Alliance"));
    CHECK(Aero::Controls::AddBoxedStringItem(*teams.Value(), "Horde"));
    combo->SetItemsSource(Ref<Aero::Base::Object>(teams.Value()));
    static_cast<void>(combo->ApplyTemplate());
    combo->SetSelectedIndex(0U);
    Pump(view, 0.048);
    CHECK(combo->GetSelectedIndex() == 0U);
    CHECK(combo->GetSelectionBoxText().View() == StringView("Overall"));

    combo->SetIsDropDownOpen(true);
    Pump(view, 0.064);
    Pump(view, 0.080);
    CHECK(combo->GetIsDropDownOpen());

    Panel* itemsHost = combo->GetItemsHost();
    CHECK(itemsHost != nullptr);
    CHECK(Aero::Media::VisualTreeHelper::GetChildrenCount(*itemsHost) >= 3U);
    Aero::Media::Visual* itemVisual =
        Aero::Media::VisualTreeHelper::GetChild(*itemsHost, 2U);
    auto* item = TryCast<ComboBoxItem>(itemVisual);
    CHECK(item != nullptr);
    const Aero::Size itemSize = item->GetRenderSize();
    CHECK(itemSize.width > 0.0 && itemSize.height > 0.0);
    Point itemScreen{};
    CHECK(item->TryPointToScreen(
        {itemSize.width * 0.5, itemSize.height * 0.5}, itemScreen));
    const int clickX = static_cast<int>(itemScreen.x);
    const int clickY = static_cast<int>(itemScreen.y);
    static_cast<void>(view.MouseMove(clickX, clickY));
    Pump(view, 0.096);
    CHECK(item->GetIsMouseOver());
    static_cast<void>(view.MouseButtonDown(
        clickX, clickY, Aero::Input::MouseButton::Left));
    static_cast<void>(view.MouseButtonUp(
        clickX, clickY, Aero::Input::MouseButton::Left));
    Pump(view, 0.112);
    if (combo->GetSelectedIndex() != 2U ||
        combo->GetSelectionBoxText().View() != StringView("Horde")) {
        std::fprintf(stderr,
            "combo items-source click index=%u dropDown=%d text=%.*s\n",
            combo->GetSelectedIndex(),
            combo->GetIsDropDownOpen() ? 1 : 0,
            static_cast<int>(combo->GetSelectionBoxText().View().SizeBytes()),
            combo->GetSelectionBoxText().View().Data());
    }
    CHECK(combo->GetSelectedIndex() == 2U);
    CHECK(combo->GetSelectionBoxText().View() == StringView("Horde"));
    CHECK(!combo->GetIsDropDownOpen());
    return true;
}

bool TestDataTemplateElementNameSelectedItemFilter() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({400.0, 300.0});

    constexpr char kTree[] =
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "Width=\"400\" Height=\"300\">"
        "<Grid.Resources>"
        "<DataTemplate x:Key=\"PlayerTemplate\">"
        "<Border x:Name=\"PlayerRow\" Height=\"24\" Background=\"#FF334455\"/>"
        "<DataTemplate.Triggers>"
        "<DataTrigger Binding=\"{Binding SelectedItem, ElementName=VisibleTeam}\" "
        "Value=\"Horde\">"
        "<Setter TargetName=\"PlayerRow\" Property=\"Visibility\" Value=\"Collapsed\"/>"
        "</DataTrigger>"
        "</DataTemplate.Triggers>"
        "</DataTemplate>"
        "</Grid.Resources>"
        "<StackPanel>"
        "<ComboBox x:Name=\"VisibleTeam\" Width=\"150\" Height=\"28\"/>"
        "<ItemsControl x:Name=\"Players\" ItemTemplate=\"{StaticResource PlayerTemplate}\"/>"
        "</StackPanel>"
        "</Grid>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "element-name filter XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {400.0, 300.0}));
    Pump(view, 0.016);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    ComboBox* combo = root->FindName<ComboBox>(StringView("VisibleTeam"));
    ItemsControl* players =
        root->FindName<ItemsControl>(StringView("Players"));
    CHECK(combo != nullptr);
    CHECK(players != nullptr);

    Result<Ref<ObservableObjectCollection>> teams =
        MakeRef<ObservableObjectCollection>();
    CHECK(teams);
    CHECK(Aero::Controls::AddBoxedStringItem(*teams.Value(), "Overall"));
    CHECK(Aero::Controls::AddBoxedStringItem(*teams.Value(), "Alliance"));
    CHECK(Aero::Controls::AddBoxedStringItem(*teams.Value(), "Horde"));
    combo->SetItemsSource(Ref<Aero::Base::Object>(teams.Value()));
    combo->SetSelectedIndex(0U);

    Result<Ref<ObservableCollection<BindingSlotItem>>> items =
        MakeRef<ObservableCollection<BindingSlotItem>>();
    Result<Ref<BindingSlotItem>> first = MakeRef<BindingSlotItem>();
    Result<Ref<BindingSlotItem>> second = MakeRef<BindingSlotItem>();
    CHECK(items && first && second);
    CHECK(items.Value()->Add(first.Value()));
    CHECK(items.Value()->Add(second.Value()));
    players->SetItemsSource(Ref<Aero::Base::Object>(items.Value()));
    static_cast<void>(combo->ApplyTemplate());
    static_cast<void>(players->ApplyTemplate());
    Pump(view, 0.032);
    Pump(view, 0.048);
    CHECK(players->GetRealizedItemCount() == 2U);

    const auto findRow = [](auto& self, Aero::Media::Visual& visual)
        -> Border* {
        if (auto* border = TryCast<Border>(&visual)) {
            if (Near(border->GetHeight(), 24.0, 0.5)) {
                return border;
            }
        }
        const std::uint32_t count =
            Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
        for (std::uint32_t index = 0U; index < count; ++index) {
            Aero::Media::Visual* child =
                Aero::Media::VisualTreeHelper::GetChild(visual, index);
            if (child == nullptr) continue;
            if (Border* found = self(self, *child)) return found;
        }
        return nullptr;
    };
    Border* row = findRow(findRow, *players);
    CHECK(row != nullptr);
    CHECK(row->GetVisibility() == Visibility::Visible);

    combo->SetSelectedIndex(2U);
    Pump(view, 0.080);
    Pump(view, 0.096);
    if (row->GetVisibility() != Visibility::Collapsed) {
        std::fprintf(stderr,
            "element-name filter did not collapse visibility=%u selected=%u\n",
            static_cast<unsigned>(row->GetVisibility()),
            combo->GetSelectedIndex());
    }
    CHECK(row->GetVisibility() == Visibility::Collapsed);
    return true;
}

bool TestDataTemplateMultiDataTriggerTeamFilter() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({400.0, 300.0});

    constexpr char kTree[] =
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "Width=\"400\" Height=\"300\">"
        "<Grid.Resources>"
        "<DataTemplate x:Key=\"PlayerTemplate\">"
        "<Border x:Name=\"PlayerRow\" Height=\"24\" Background=\"#FF334455\"/>"
        "<DataTemplate.Triggers>"
        "<MultiDataTrigger>"
        "<MultiDataTrigger.Conditions>"
        "<Condition Binding=\"{Binding SelectedItem, ElementName=VisibleTeam}\" "
        "Value=\"Horde\"/>"
        "<Condition Binding=\"{Binding Team}\" Value=\"Alliance\"/>"
        "</MultiDataTrigger.Conditions>"
        "<Setter TargetName=\"PlayerRow\" Property=\"Visibility\" Value=\"Collapsed\"/>"
        "</MultiDataTrigger>"
        "<MultiDataTrigger>"
        "<MultiDataTrigger.Conditions>"
        "<Condition Binding=\"{Binding SelectedItem, ElementName=VisibleTeam}\" "
        "Value=\"Alliance\"/>"
        "<Condition Binding=\"{Binding Team}\" Value=\"Horde\"/>"
        "</MultiDataTrigger.Conditions>"
        "<Setter TargetName=\"PlayerRow\" Property=\"Visibility\" Value=\"Collapsed\"/>"
        "</MultiDataTrigger>"
        "</DataTemplate.Triggers>"
        "</DataTemplate>"
        "</Grid.Resources>"
        "<StackPanel>"
        "<ItemsControl x:Name=\"Players\" ItemTemplate=\"{StaticResource PlayerTemplate}\"/>"
        "<ComboBox x:Name=\"VisibleTeam\" Width=\"150\" Height=\"28\"/>"
        "</StackPanel>"
        "</Grid>";
    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "multi-data filter XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {400.0, 300.0}));
    Pump(view, 0.016);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    ComboBox* combo = root->FindName<ComboBox>(StringView("VisibleTeam"));
    ItemsControl* players =
        root->FindName<ItemsControl>(StringView("Players"));
    CHECK(combo != nullptr);
    CHECK(players != nullptr);

    Result<Ref<ObservableObjectCollection>> teams =
        MakeRef<ObservableObjectCollection>();
    CHECK(teams);
    CHECK(Aero::Controls::AddBoxedStringItem(*teams.Value(), "Overall"));
    CHECK(Aero::Controls::AddBoxedStringItem(*teams.Value(), "Alliance"));
    CHECK(Aero::Controls::AddBoxedStringItem(*teams.Value(), "Horde"));
    combo->SetItemsSource(Ref<Aero::Base::Object>(teams.Value()));
    combo->SetSelectedIndex(0U);

    Result<Ref<ObservableCollection<BindingSlotItem>>> items =
        MakeRef<ObservableCollection<BindingSlotItem>>();
    Result<Ref<BindingSlotItem>> alliance = MakeRef<BindingSlotItem>();
    Result<Ref<BindingSlotItem>> horde = MakeRef<BindingSlotItem>();
    CHECK(items && alliance && horde);
    String allianceTeam;
    String hordeTeam;
    CHECK(allianceTeam.Assign("Alliance"));
    CHECK(hordeTeam.Assign("Horde"));
    alliance.Value()->SetTeam(std::move(allianceTeam));
    horde.Value()->SetTeam(std::move(hordeTeam));
    CHECK(items.Value()->Add(alliance.Value()));
    CHECK(items.Value()->Add(horde.Value()));
    players->SetItemsSource(Ref<Aero::Base::Object>(items.Value()));
    static_cast<void>(combo->ApplyTemplate());
    static_cast<void>(players->ApplyTemplate());
    Pump(view, 0.032);
    Pump(view, 0.048);
    CHECK(players->GetRealizedItemCount() == 2U);

    Border* rows[2] = {nullptr, nullptr};
    std::uint32_t rowCount = 0U;
    const auto collectRows = [&](auto& self, Aero::Media::Visual& visual)
        -> void {
        if (rowCount >= 2U) return;
        if (auto* border = TryCast<Border>(&visual)) {
            if (Near(border->GetHeight(), 24.0, 0.5)) {
                rows[rowCount++] = border;
                return;
            }
        }
        const std::uint32_t count =
            Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
        for (std::uint32_t index = 0U; index < count; ++index) {
            Aero::Media::Visual* child =
                Aero::Media::VisualTreeHelper::GetChild(visual, index);
            if (child == nullptr) continue;
            self(self, *child);
        }
    };
    collectRows(collectRows, *players);
    CHECK(rowCount == 2U);
    CHECK(rows[0] != nullptr && rows[1] != nullptr);
    CHECK(rows[0]->GetVisibility() == Visibility::Visible);
    CHECK(rows[1]->GetVisibility() == Visibility::Visible);

    combo->SetSelectedIndex(2U);
    Pump(view, 0.064);
    Pump(view, 0.080);
    if (rows[0]->GetVisibility() != Visibility::Collapsed ||
        rows[1]->GetVisibility() != Visibility::Visible) {
        std::fprintf(stderr,
            "horde filter vis alliance=%u horde=%u selected=%u\n",
            static_cast<unsigned>(rows[0]->GetVisibility()),
            static_cast<unsigned>(rows[1]->GetVisibility()),
            combo->GetSelectedIndex());
    }
    CHECK(rows[0]->GetVisibility() == Visibility::Collapsed);
    CHECK(rows[1]->GetVisibility() == Visibility::Visible);

    combo->SetSelectedIndex(1U);
    Pump(view, 0.096);
    Pump(view, 0.112);
    CHECK(rows[0]->GetVisibility() == Visibility::Visible);
    CHECK(rows[1]->GetVisibility() == Visibility::Collapsed);
    return true;
}

bool TestDataTemplateItemHoverStoryboard() {
    ViewOptions options;
    options.automaticAnimationClock = false;
    LiveGui* live = NewLiveGui(options);
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({400.0, 300.0});

    constexpr char kTree[] =
        "<Viewbox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid Width=\"400\" Height=\"300\">"
        "<Grid.Resources>"
        "<DataTemplate x:Key=\"PlayerTemplate\">"
        "<DataTemplate.Resources>"
        "<Storyboard x:Key=\"OverOn\">"
        "<DoubleAnimationUsingKeyFrames "
        "Storyboard.TargetProperty=\"(UIElement.RenderTransform)."
        "(TransformGroup.Children)[3].(TranslateTransform.X)\" "
        "Storyboard.TargetName=\"PlayerRow\">"
        "<EasingDoubleKeyFrame KeyTime=\"0:0:0.15\" Value=\"8\"/>"
        "</DoubleAnimationUsingKeyFrames>"
        "</Storyboard>"
        "</DataTemplate.Resources>"
        "<Grid x:Name=\"PlayerRow\" Background=\"#FF334455\" Height=\"40\">"
        "<Grid.RenderTransform>"
        "<TransformGroup>"
        "<ScaleTransform/>"
        "<SkewTransform/>"
        "<RotateTransform/>"
        "<TranslateTransform/>"
        "</TransformGroup>"
        "</Grid.RenderTransform>"
        "<TextBlock Text=\"Alpha\" VerticalAlignment=\"Center\" Margin=\"8,0\"/>"
        "</Grid>"
        "<DataTemplate.Triggers>"
        "<Trigger Property=\"IsMouseOver\" Value=\"True\">"
        "<Trigger.EnterActions>"
        "<BeginStoryboard Storyboard=\"{StaticResource OverOn}\"/>"
        "</Trigger.EnterActions>"
        "</Trigger>"
        "</DataTemplate.Triggers>"
        "</DataTemplate>"
        "</Grid.Resources>"
        "<ItemsControl x:Name=\"Players\" ItemTemplate=\"{StaticResource PlayerTemplate}\">"
        "<ItemsControl.ItemsPanel>"
        "<ItemsPanelTemplate><StackPanel/></ItemsPanelTemplate>"
        "</ItemsControl.ItemsPanel>"
        "</ItemsControl>"
        "</Grid>"
        "</Viewbox>";

    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView(kTree));
    if (!document) {
        std::fprintf(stderr, "data template hover XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {400.0, 300.0}));
    Pump(view, 0.016);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    ItemsControl* players =
        root->FindName<ItemsControl>(StringView("Players"));
    CHECK(players != nullptr);

    const DataTemplate* itemTemplate = players->GetItemTemplate();
    CHECK(itemTemplate != nullptr);
    Result<Aero::ResourceValue> overOn =
        itemTemplate->GetResources().Lookup(StringView("OverOn"));
    if (!overOn) {
        std::fprintf(stderr,
            "DataTemplate.Resources missing OverOn: %s\n",
            overOn.GetStatus().message);
    }
    CHECK(overOn);

    Result<Ref<ObservableCollection<BindingSlotItem>>> items =
        MakeRef<ObservableCollection<BindingSlotItem>>();
    Result<Ref<BindingSlotItem>> item = MakeRef<BindingSlotItem>();
    CHECK(items && item);
    CHECK(items.Value()->Add(item.Value()));
    players->SetItemsSource(Ref<Aero::Base::Object>(items.Value()));
    static_cast<void>(players->ApplyTemplate());
    Pump(view, 0.032);
    Pump(view, 0.048);
    static_cast<void>(players->ApplyTemplate());
    Pump(view, 0.064);
    if (players->GetRealizedItemCount() != 1U) {
        std::fprintf(stderr,
            "data template hover realized=%u host=%d\n",
            players->GetRealizedItemCount(),
            players->GetItemsHost() != nullptr);
    }
    CHECK(players->GetRealizedItemCount() == 1U);

    const auto findRow = [](auto& self, Aero::Media::Visual& visual)
        -> Grid* {
        if (auto* grid = TryCast<Grid>(&visual)) {
            auto* group = TryCast<Aero::Media::TransformGroup>(
                grid->GetRenderTransform().Get());
            if (group != nullptr && group->GetChildren().Size() > 3U) {
                return grid;
            }
        }
        const std::uint32_t count =
            Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
        for (std::uint32_t index = 0U; index < count; ++index) {
            Aero::Media::Visual* child =
                Aero::Media::VisualTreeHelper::GetChild(visual, index);
            if (child == nullptr) continue;
            if (Grid* found = self(self, *child)) return found;
        }
        return nullptr;
    };
    Grid* row = findRow(findRow, *players);
    if (row == nullptr && root != nullptr) {
        row = findRow(findRow, *root);
    }
    if (row == nullptr) {
        std::fprintf(stderr, "data template hover PlayerRow was not realized\n");
    }
    CHECK(row != nullptr);

    const Aero::Size rowSize = row->GetRenderSize();
    CHECK(rowSize.width > 0.0 && rowSize.height > 0.0);
    Point rowScreen{};
    CHECK(row->TryPointToScreen(
        {rowSize.width * 0.5, rowSize.height * 0.5}, rowScreen));
    static_cast<void>(view.MouseMove(
        static_cast<int>(rowScreen.x),
        static_cast<int>(rowScreen.y)));
    Pump(view, 0.080);
    if (!row->GetIsMouseOver()) {
        std::fprintf(stderr,
            "data template hover miss screen=(%.1f,%.1f) size=(%.1f,%.1f) "
            "slot=(%.1f,%.1f,%.1f,%.1f)\n",
            rowScreen.x, rowScreen.y,
            rowSize.width, rowSize.height,
            row->GetLayoutSlot().x, row->GetLayoutSlot().y,
            row->GetLayoutSlot().width, row->GetLayoutSlot().height);
    }
    CHECK(row->GetIsMouseOver());

    Pump(view, 0.250);
    auto* group = TryCast<Aero::Media::TransformGroup>(
        row->GetRenderTransform().Get());
    CHECK(group != nullptr);
    CHECK(group->GetChildren().Size() > 3U);
    auto* translate = TryCast<Aero::Media::TranslateTransform>(
        group->GetChildren()[3].Get());
    CHECK(translate != nullptr);
    if (!(translate->GetX() > 1.0)) {
        std::fprintf(stderr,
            "data template hover storyboard did not run translate.x=%.3f "
            "over=%d\n",
            translate->GetX(),
            row->GetIsMouseOver() ? 1 : 0);
    }
    CHECK(translate->GetX() > 1.0);
    return true;
}

bool TestGeometryFlatten() {
    Result<Ref<PathGeometry>> path = MakeRef<PathGeometry>();
    Result<Ref<PathFigure>> figure = MakeRef<PathFigure>();
    Result<Ref<LineSegment>> line = MakeRef<LineSegment>();
    Result<Ref<BezierSegment>> cubic = MakeRef<BezierSegment>();
    Result<Ref<QuadraticBezierSegment>> quad = MakeRef<QuadraticBezierSegment>();
    Result<Ref<ArcSegment>> arc = MakeRef<ArcSegment>();
    Result<Ref<PolyLineSegment>> polyLine = MakeRef<PolyLineSegment>();
    Result<Ref<PolyBezierSegment>> polyBezier = MakeRef<PolyBezierSegment>();
    Result<Ref<PolyQuadraticBezierSegment>> polyQuad =
        MakeRef<PolyQuadraticBezierSegment>();
    CHECK(path && figure && line && cubic && quad && arc && polyLine &&
        polyBezier && polyQuad);

    figure.Value()->SetStartPoint({0.0, 0.0});
    line.Value()->SetPoint({10.0, 0.0});
    CHECK(figure.Value()->AddSegment(line.Value()));
    cubic.Value()->SetPoint1({10.0, 10.0});
    cubic.Value()->SetPoint2({20.0, 10.0});
    cubic.Value()->SetPoint3({20.0, 0.0});
    CHECK(figure.Value()->AddSegment(cubic.Value()));
    quad.Value()->SetPoint1({30.0, 10.0});
    quad.Value()->SetPoint2({30.0, 0.0});
    CHECK(figure.Value()->AddSegment(quad.Value()));
    arc.Value()->SetPoint({40.0, 0.0});
    arc.Value()->SetSize({8.0, 8.0});
    arc.Value()->SetSweepDirection(Aero::Media::SweepDirection::Clockwise);
    CHECK(figure.Value()->AddSegment(arc.Value()));
    const Point polyPoints[] = {{50.0, 0.0}, {60.0, 0.0}};
    CHECK(polyLine.Value()->SetPoints({polyPoints, 2U}));
    CHECK(figure.Value()->AddSegment(polyLine.Value()));
    const Point bezierPts[] = {
        {70.0, 10.0}, {80.0, 10.0}, {80.0, 0.0}};
    CHECK(polyBezier.Value()->SetPoints({bezierPts, 3U}));
    CHECK(figure.Value()->AddSegment(polyBezier.Value()));
    const Point quadPts[] = {{90.0, 10.0}, {90.0, 0.0}};
    CHECK(polyQuad.Value()->SetPoints({quadPts, 2U}));
    CHECK(figure.Value()->AddSegment(polyQuad.Value()));
    CHECK(path.Value()->AddFigure(figure.Value()));

    PointSink sink;
    CHECK(path.Value()->Flatten(sink));
    CHECK(sink.begins == 1U);
    CHECK(sink.ends == 1U);
    CHECK(sink.points.Size() > 8U);
    CHECK(Near(sink.points[0].x, 0.0, 0.01));
    CHECK(Near(sink.points[sink.points.Size() - 1U].x, 90.0, 1.5));

    LineGeometry transformed;
    transformed.SetStartPoint({0.0, 0.0});
    transformed.SetEndPoint({1.0, 0.0});
    Result<Ref<TranslateTransform>> offset = MakeRef<TranslateTransform>();
    CHECK(offset);
    offset.Value()->SetX(5.0);
    transformed.SetTransform(offset.Value());
    PointSink shifted;
    CHECK(transformed.Flatten(shifted));
    CHECK(shifted.points.Size() >= 2U);
    CHECK(Near(shifted.points[0].x, 5.0, 0.01));
    CHECK(Near(shifted.points[shifted.points.Size() - 1U].x, 6.0, 0.01));

    Result<Ref<LineGeometry>> a = MakeRef<LineGeometry>();
    Result<Ref<LineGeometry>> b = MakeRef<LineGeometry>();
    CHECK(a && b);
    a.Value()->SetStartPoint({0.0, 0.0});
    a.Value()->SetEndPoint({4.0, 0.0});
    b.Value()->SetStartPoint({0.0, 2.0});
    b.Value()->SetEndPoint({4.0, 2.0});
    Result<Ref<GeometryGroup>> group = MakeRef<GeometryGroup>();
    CHECK(group);
    CHECK(group.Value()->Add(a.Value()));
    CHECK(group.Value()->Add(b.Value()));
    PointSink grouped;
    CHECK(group.Value()->Flatten(grouped));
    CHECK(grouped.points.Size() >= 4U);

    Result<Ref<CombinedGeometry>> combined = MakeRef<CombinedGeometry>();
    CHECK(combined);
    combined.Value()->SetGeometry1(a.Value());
    combined.Value()->SetGeometry2(b.Value());
    combined.Value()->SetGeometryCombineMode(GeometryCombineMode::Union);
    PointSink unioned;
    CHECK(combined.Value()->Flatten(unioned));
    CHECK(unioned.points.Size() >= grouped.points.Size());
    combined.Value()->SetGeometryCombineMode(GeometryCombineMode::Exclude);
    PointSink excluded;
    CHECK(combined.Value()->Flatten(excluded));
    CHECK(excluded.points.Size() >= 2U);
    CHECK(excluded.points.Size() < unioned.points.Size());
    return true;
}

bool SameFlattened(const PointSink& left, const PointSink& right) noexcept {
    if (left.begins != right.begins || left.ends != right.ends ||
        left.points.Size() != right.points.Size()) {
        return false;
    }
    for (std::uint32_t index = 0U; index < left.points.Size(); ++index) {
        if (!Near(left.points[index].x, right.points[index].x, 0.05) ||
            !Near(left.points[index].y, right.points[index].y, 0.05)) {
            return false;
        }
    }
    return true;
}

bool TestClosedPathRelativeMove() {
    StreamGeometry ring;
    ring.SetData("M10,10 L30,10 L30,30 L10,30 Z m2,2 h16 v16 h-16 Z");
    PointSink sink;
    CHECK(ring.Flatten(sink));
    CHECK(sink.begins == 2U);
    CHECK(sink.figureStarts.Size() == 2U);
    CHECK(Near(sink.figureStarts[0].x, 10.0));
    CHECK(Near(sink.figureStarts[0].y, 10.0));
    CHECK(Near(sink.figureStarts[1].x, 12.0));
    CHECK(Near(sink.figureStarts[1].y, 12.0));
    const Aero::Rect ringBounds = ring.GetBounds();
    CHECK(ringBounds.x >= 9.9);
    CHECK(ringBounds.y >= 9.9);
    CHECK(ringBounds.width <= 20.1);
    CHECK(ringBounds.height <= 20.1);

    StreamGeometry innerFrame;
    innerFrame.SetData(
        "M489 658.92H38.65l-.17-.78a35.52 35.52 0 0 0-26.7-26.7l-.78-.18V38.66"
        "l.78-.18a35.53 35.53 0 0 0 26.7-26.7l.17-.78H489l.17.78a35.53 35.53 "
        "0 0 0 26.7 26.7l.78.18v592.6l-.78.18a35.52 35.52 0 0 0-26.7 26.7Zm-448.79-2"
        "h447.2a37.51 37.51 0 0 1 27.24-27.25V40.25A37.51 37.51 0 0 1 487.45 13"
        "H40.25A37.54 37.54 0 0 1 13 40.25V629.67A37.53 37.53 0 0 1 40.25 656.92Z");
    PointSink frameSink;
    CHECK(innerFrame.Flatten(frameSink));
    CHECK(frameSink.begins == 2U);
    const Aero::Rect frameBounds = innerFrame.GetBounds();
    CHECK(frameBounds.x > -1.0);
    CHECK(frameBounds.y > -1.0);
    CHECK(frameBounds.width < 540.0);
    CHECK(frameBounds.height < 680.0);

    StreamGeometry listItem;
    listItem.SetData(
        "M335.67 47.74H29.81l-20-20 20-20H335.67l20 20Zm-305-2h304.2l18-18-18-18"
        "H30.64l-18 18Z");
    PointSink itemSink;
    CHECK(listItem.Flatten(itemSink));
    CHECK(itemSink.begins == 2U);
    const Aero::Rect itemBounds = listItem.GetBounds();
    CHECK(itemBounds.x > -1.0);
    CHECK(itemBounds.width < 380.0);
    return true;
}

bool TestStreamGeometryFlattenCore() {
    StreamGeometry stream;
    stream.SetData("M 0,0 L 10,0 C 10,10 20,10 20,0 Q 30,10 30,0 A 8,8 0 0 1 40,0 Z");
    PointSink fromData;
    CHECK(stream.Flatten(fromData));
    CHECK(fromData.begins >= 1U);
    CHECK(fromData.points.Size() > 8U);
    CHECK(stream.GetBounds().width > 0.0);

    Result<Ref<PathGeometry>> pathGeometry = MakeRef<PathGeometry>();
    Result<Ref<PathFigure>> figure = MakeRef<PathFigure>();
    Result<Ref<LineSegment>> first = MakeRef<LineSegment>();
    Result<Ref<LineSegment>> second = MakeRef<LineSegment>();
    CHECK(pathGeometry && figure && first && second);
    figure.Value()->SetStartPoint({0.0, 0.0});
    figure.Value()->SetIsClosed(true);
    first.Value()->SetPoint({10.0, 0.0});
    second.Value()->SetPoint({10.0, 10.0});
    CHECK(figure.Value()->AddSegment(first.Value()));
    CHECK(figure.Value()->AddSegment(second.Value()));
    CHECK(pathGeometry.Value()->AddFigure(figure.Value()));
    StreamGeometry lineStream;
    lineStream.SetData("M 0,0 L 10,0 L 10,10 Z");
    PointSink fromStream;
    PointSink fromPathGeometry;
    CHECK(lineStream.Flatten(fromStream));
    CHECK(pathGeometry.Value()->Flatten(fromPathGeometry));
    CHECK(SameFlattened(fromStream, fromPathGeometry));

    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    Result<Ref<Path>> path = MakeRef<Path>();
    Result<Ref<StreamGeometry>> mounted = MakeRef<StreamGeometry>();
    CHECK(path && mounted);
    mounted.Value()->SetData(stream.GetData());
    path.Value()->SetData(mounted.Value());
    CHECK(live->view->SetContent(path.Value(), {200.0, 200.0}));
    Pump(*live->view, 0.016);
    CHECK(path.Value()->GetGeometryBounds().width > 0.0);
    return true;
}

bool TestStreamGeometryContextFlatten() {
    const StringView data(
        "M 0,0 L 10,0 C 10,10 20,10 20,0 Q 30,10 30,0");
    StreamGeometry parsed;
    parsed.SetData(data);
    PointSink expected;
    CHECK(parsed.Flatten(expected));

    StreamGeometry built;
    StreamGeometryContext context = built.Open();
    CHECK(context.BeginFigure({0.0, 0.0}, true, false));
    CHECK(context.LineTo({10.0, 0.0}, true, false));
    CHECK(context.BezierTo({10.0, 10.0}, {20.0, 10.0}, {20.0, 0.0}, true, false));
    CHECK(context.QuadraticBezierTo({30.0, 10.0}, {30.0, 0.0}, true, false));
    CHECK(context.Close());
    PointSink actual;
    CHECK(built.Flatten(actual));
    CHECK(SameFlattened(expected, actual));
    return true;
}

bool TestTimelineDurationAndKeyTime() {
    Duration automatic = Duration::Automatic();
    CHECK(automatic.IsAutomatic());
    Duration forever = Duration::Forever();
    CHECK(forever.IsForever());
    Result<Duration> parsedAuto = Duration::TryParse("Automatic");
    Result<Duration> parsedForever = Duration::TryParse("Forever");
    CHECK(parsedAuto && parsedAuto.Value().IsAutomatic());
    CHECK(parsedForever && parsedForever.Value().IsForever());

    CHECK(KeyTime::Uniform().IsUniform());
    CHECK(KeyTime::Paced().IsPaced());
    Result<KeyTime> percent = KeyTime::TryParse("50%");
    CHECK(percent && percent.Value().IsPercent());
    CHECK(Near(percent.Value().GetPercent(), 0.5));
    const std::uint64_t uniform =
        KeyTime::Uniform().ResolveMicroseconds(1000U, 0U, 4U);
    const std::uint64_t paced =
        KeyTime::Paced().ResolveMicroseconds(1000U, 0U, 4U);
    CHECK(uniform == 250U);
    CHECK(paced == uniform);
    CHECK(percent.Value().ResolveMicroseconds(1000U, 0U, 4U) == 500U);

    DoubleAnimation animation;
    animation.SetDuration(Duration::Automatic());
    CHECK(animation.GetDuration().IsAutomatic());
    animation.SetDuration(Duration::Forever());
    CHECK(animation.GetDuration().IsForever());
    Style style(DoubleAnimation::StaticTypeId());
    CHECK(style.Set(Timeline::DurationProperty, Duration::Forever()));
    CHECK(style.Set(
        Timeline::RepeatBehaviorProperty, RepeatBehavior::Forever()));
    return true;
}

bool TestCollectionViewAndVirtualization() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& host = *live->view;
    host.SetSize({160.0, 72.0});

    Result<Ref<Button>> narrow = MakeRef<Button>();
    Result<Ref<Button>> wide = MakeRef<Button>();
    Result<Ref<Button>> wider = MakeRef<Button>();
    CHECK(narrow && wide && wider);
    narrow.Value()->SetWidth(10.0);
    wide.Value()->SetWidth(80.0);
    wider.Value()->SetWidth(90.0);

    ObservableObjectCollection source;
    CHECK(source.Add(narrow.Value()));
    CHECK(source.Add(wide.Value()));
    CHECK(source.Add(wider.Value()));

    CollectionView view(&source);
    CHECK(view.GetCount() == 3U);
    view.SetFilter(&KeepWide);
    CHECK(view.GetCount() == 2U);
    CHECK(view.GetItem(0U).Get() == wide.Value().Get());
    view.SortBy("Width", ListSortDirection::Descending);
    CHECK(view.GetItem(0U).Get() == wider.Value().Get());
    CHECK(view.GetItem(1U).Get() == wide.Value().Get());
    CHECK(view.MoveCurrentTo(wide.Value().Get()));
    CHECK(view.GetCurrentItem().Get() == wide.Value().Get());

    Result<Ref<ObservableObjectCollection>> items =
        MakeRef<ObservableObjectCollection>();
    CHECK(items);
    Vector<Ref<TextBlock>> rows;
    for (std::uint32_t index = 0U; index < 40U; ++index) {
        Result<Ref<TextBlock>> row = MakeRef<TextBlock>();
        CHECK(row);
        row.Value()->SetHeight(24.0);
        CHECK(items.Value()->Add(row.Value()));
        CHECK(rows.PushBack(row.Value()));
    }

    Result<Ref<ListBox>> list = MakeRef<ListBox>();
    CHECK(list);
    list.Value()->SetItemsSource(items.Value());
    CollectionView* defaultView =
        CollectionViewSource::GetDefaultView(items.Value().Get());
    CHECK(defaultView != nullptr);
    CHECK(defaultView->GetCount() == 40U);
    list.Value()->SetIsSynchronizedWithCurrentItem(false);
    list.Value()->SetSelectedItem(rows[3]);
    CHECK(list.Value()->GetSelectedItem().Get() == rows[3].Get());
    defaultView->MoveCurrentTo(rows[7].Get());
    CHECK(list.Value()->GetSelectedItem().Get() == rows[3].Get());
    CHECK(defaultView->GetCurrentItem().Get() == rows[7].Get());
    CHECK(list.Value()->GetIndexOfItem(rows[3].Get()) == 3U);

    defaultView->SetFilter(
        [](const Aero::Base::Object* item) noexcept {
            return item != nullptr;
        });
    CHECK(list.Value()->GetIndexOfItem(rows[3].Get()) != UINT32_MAX);
    defaultView->SetFilter(
        [](const Aero::Base::Object*) noexcept { return false; });
    CHECK(list.Value()->GetIndexOfItem(rows[3].Get()) == UINT32_MAX);
    defaultView->SetFilter({});
    CHECK(defaultView->GetCount() == 40U);

    CHECK(host.SetContent(list.Value()));
    Pump(host, 0.016);
    static_cast<void>(list.Value()->ApplyTemplate());
    Pump(host, 0.032);

    Panel* itemsHost = list.Value()->GetItemsHost();
    if (VirtualizingStackPanel* virtualizing =
            TryCast<VirtualizingStackPanel>(
                static_cast<Aero::Base::Object*>(itemsHost))) {
        virtualizing->SetVerticalOffset(240.0);
        Pump(host, 0.048);
        const std::uint32_t first =
            virtualizing->GetRealizedFirstIndex();
        Aero::Controls::ItemContainerGenerator* generator =
            list.Value()->GetItemContainerGenerator();
        CHECK(generator != nullptr);
        CHECK(generator->GetFirstGeneratedIndex() == first);
        const std::uint32_t target = first + 1U;
        CHECK(list.Value()->Select(target));
        Pump(host, 0.064);
        FrameworkElement* container = generator->ContainerFromIndex(target);
        CHECK(container != nullptr);
        ListBoxItem* item = TryCast<ListBoxItem>(container);
        CHECK(item != nullptr);
        CHECK(item->GetIsSelected());
        CHECK(list.Value()->GetIsSelected(target));
    } else {
        CHECK(list.Value()->Select(12U));
        Pump(host, 0.048);
        Aero::Controls::ItemContainerGenerator* generator =
            list.Value()->GetItemContainerGenerator();
        if (generator != nullptr && generator->GetGeneratedCount() > 12U) {
            FrameworkElement* container = generator->ContainerFromIndex(12U);
            if (ListBoxItem* item = TryCast<ListBoxItem>(container)) {
                CHECK(item->GetIsSelected());
            }
        }
    }
    return true;
}

bool TestTemplateResolveOrder() {
    Result<Ref<ItemsControl>> host = MakeRef<ItemsControl>();
    Result<Ref<DataTemplate>> selectorTemplate = MakeRef<DataTemplate>();
    Result<Ref<DataTemplate>> itemTemplate = MakeRef<DataTemplate>();
    Result<Ref<DataTemplate>> implicitTemplate = MakeRef<DataTemplate>();
    Result<Ref<Button>> item = MakeRef<Button>();
    CHECK(host && selectorTemplate && itemTemplate && implicitTemplate && item);

    implicitTemplate.Value()->SetDataType(Button::StaticTypeId());
    CHECK(host.Value()->GetResources().Add(
        implicitTemplate.Value()->GetImplicitKey(),
        Aero::Value::FromObject(
            DataTemplate::StaticTypeId(), implicitTemplate.Value())));

    Ref<DataTemplate> resolved = host.Value()->ResolveItemTemplate(item.Value(), 0U);
    CHECK(resolved.Get() == implicitTemplate.Value().Get());

    host.Value()->SetItemTemplate(itemTemplate.Value());
    resolved = host.Value()->ResolveItemTemplate(item.Value(), 0U);
    CHECK(resolved.Get() == itemTemplate.Value().Get());

    Result<Ref<PickingSelector>> selector = MakeRef<PickingSelector>();
    CHECK(selector);
    selector.Value()->pick = selectorTemplate.Value();
    host.Value()->SetItemTemplateSelector(selector.Value());
    resolved = host.Value()->ResolveItemTemplate(item.Value(), 0U);
    CHECK(resolved.Get() == selectorTemplate.Value().Get());

    selector.Value()->enabled = false;
    resolved = host.Value()->ResolveItemTemplate(item.Value(), 0U);
    CHECK(resolved.Get() == itemTemplate.Value().Get());
    return true;
}

bool TestStrokeJoinCapFillRule() {
    Path path;
    path.SetStrokeLineJoin(PenLineJoin::Round);
    path.SetStrokeStartLineCap(PenLineCap::Triangle);
    path.SetStrokeEndLineCap(PenLineCap::Square);
    path.SetFillRule(FillRule::EvenOdd);
    CHECK(path.GetStrokeLineJoin() == PenLineJoin::Round);
    CHECK(path.GetStrokeStartLineCap() == PenLineCap::Triangle);
    CHECK(path.GetStrokeEndLineCap() == PenLineCap::Square);
    CHECK(path.GetFillRule() == FillRule::EvenOdd);
    path.SetFillRule(FillRule::Nonzero);
    CHECK(path.GetFillRule() == FillRule::Nonzero);

    Result<Ref<Pen>> pen = MakeRef<Pen>();
    Result<Ref<Aero::Media::Brush>> stroke = Aero::Media::MakeSolidColorBrush(
        {0.0F, 0.0F, 0.0F, 1.0F});
    CHECK(pen && stroke);
    pen.Value()->SetBrush(stroke.Value());
    pen.Value()->SetThickness(4.0);
    pen.Value()->SetLineJoin(PenLineJoin::Miter);
    pen.Value()->SetMiterLimit(1.0);
    pen.Value()->SetStartLineCap(PenLineCap::Square);
    pen.Value()->SetEndLineCap(PenLineCap::Triangle);
    path.SetPen(pen.Value());
    CHECK(path.GetPen().Get() == pen.Value().Get());
    CHECK(Near(path.GetPen()->GetMiterLimit(), 1.0, 0.001));
    Result<Ref<StreamGeometry>> geometry = MakeRef<StreamGeometry>();
    CHECK(geometry);
    geometry.Value()->SetData("M 0,0 L 20,0 L 10,20 Z");
    path.SetData(geometry.Value());
    path.SetStroke(stroke.Value());

    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    Result<Ref<Path>> mounted = MakeRef<Path>();
    CHECK(mounted);
    mounted.Value()->SetPen(pen.Value());
    mounted.Value()->SetData(geometry.Value());
    mounted.Value()->SetStroke(stroke.Value());
    mounted.Value()->SetStrokeLineJoin(PenLineJoin::Miter);
    CHECK(live->view->SetContent(mounted.Value(), {100.0, 100.0}));
    Pump(*live->view, 0.016);
    CHECK(mounted.Value()->GetGeometryBounds().width > 0.0);
    CHECK(mounted.Value()->GetGeometryBounds().height > 0.0);
    CHECK(Near(mounted.Value()->GetPen()->GetMiterLimit(), 1.0, 0.001));
    return true;
}

bool TestNotifyPropertyChangedBindLoop() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({200.0, 40.0});

    Result<Ref<Person>> person = MakeRef<Person>();
    CHECK(person);
    String name;
    CHECK(name.Assign("Ada"));
    person.Value()->SetName(std::move(name));

    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView("<TextBlock xmlns=\"urn:aero\" Text=\"{Binding Name}\"/>"));
    CHECK(document);
    TextBlock* label = document.Value().Root<TextBlock>();
    CHECK(label != nullptr);
    label->SetValue(
        FrameworkElement::DataContextProperty,
        Aero::Value::FromObject(Person::StaticTypeId(), person.Value()));
    CHECK(view.SetContent(
        std::move(document).Value(), {200.0, 40.0}));
    label = TryCast<TextBlock>(view.GetContent());
    CHECK(label != nullptr);
    Pump(view, 0.016);
    Pump(view, 0.032);
    Pump(view, 0.048);

    bool polled = false;
    for (std::uint32_t index = 0U; index < live->diagnostics.Size(); ++index) {
        if (Contains(
                live->diagnostics.Items()[index].Message(),
                "polling metadata")) {
            polled = true;
        }
    }
    CHECK(!polled);
    CHECK(label->GetText() == StringView("Ada"));

    String renamed;
    CHECK(renamed.Assign("Bob"));
    person.Value()->SetName(std::move(renamed));
    Pump(view, 0.064);
    CHECK(label->GetText() == StringView("Bob"));
    return true;
}

bool TestBindingExpressionAndLostFocus() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({240.0, 80.0});

    Result<Ref<Person>> person = MakeRef<Person>();
    CHECK(person);
    String name;
    CHECK(name.Assign("Ada"));
    person.Value()->SetName(std::move(name));

    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
        "<StackPanel xmlns=\"urn:aero\">"
        "<TextBox Text=\"{Binding Name, Mode=TwoWay}\"/>"
        "<TextBox Text=\"other\"/>"
        "</StackPanel>"));
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {240.0, 80.0}));
    Pump(view, 0.016);
    auto* root = TryCast<StackPanel>(view.GetContent());
    CHECK(root != nullptr);
    CHECK(root->GetChildren().GetCount() >= 2U);
    auto* input = TryCast<TextBox>(root->GetChildren().GetItem(0U));
    auto* other = TryCast<TextBox>(root->GetChildren().GetItem(1U));
    CHECK(input != nullptr && other != nullptr);
    input->SetValue(
        FrameworkElement::DataContextProperty,
        Aero::Value::FromObject(Person::StaticTypeId(), person.Value()));
    Pump(view, 0.032);
    Pump(view, 0.048);
    CHECK(input->GetText() == StringView("Ada"));

    BindingExpression expression = BindingOperations::GetBindingExpression(
        input, TextBox::TextProperty);
    CHECK(expression.IsValid());
    CHECK(expression.Status() == BindingStatus::Active);

    input->SetText("Bob");
    Pump(view, 0.064);
    CHECK(person.Value()->GetName() == StringView("Ada"));

    CHECK(input->Focus());
    Pump(view, 0.080);
    CHECK(other->Focus());
    Pump(view, 0.096);
    CHECK(person.Value()->GetName() == StringView("Bob"));

    BindingExpression invalid;
    CHECK(!invalid.IsValid());
    CHECK(invalid.Status() == BindingStatus::Unattached);
    CHECK(!invalid.UpdateSource().IsOk());
    CHECK(!invalid.UpdateTarget().IsOk());

    Result<Aero::Markup::XamlDocument> explicitDocument = reader.Parse(StringView(
        "<TextBox xmlns=\"urn:aero\""
        " Text=\"{Binding Name, Mode=TwoWay, UpdateSourceTrigger=Explicit}\"/>"));
    CHECK(explicitDocument);
    CHECK(view.SetContent(std::move(explicitDocument).Value(), {240.0, 40.0}));
    auto* explicitBox = TryCast<TextBox>(view.GetContent());
    CHECK(explicitBox != nullptr);
    explicitBox->SetValue(
        FrameworkElement::DataContextProperty,
        Aero::Value::FromObject(Person::StaticTypeId(), person.Value()));
    Pump(view, 0.112);
    CHECK(explicitBox->GetText() == StringView("Bob"));
    BindingExpression explicitExpression =
        BindingOperations::GetBindingExpression(
            explicitBox, TextBox::TextProperty);
    CHECK(explicitExpression.IsValid());
    explicitBox->SetText("Cara");
    Pump(view, 0.128);
    CHECK(person.Value()->GetName() == StringView("Bob"));
    CHECK(explicitExpression.UpdateSource().IsOk());
    CHECK(person.Value()->GetName() == StringView("Cara"));
    String restored;
    CHECK(restored.Assign("Ada"));
    person.Value()->SetName(std::move(restored));
    CHECK(explicitExpression.UpdateTarget().IsOk());
    CHECK(explicitBox->GetText() == StringView("Ada"));
    return true;
}

bool TestCustomItemsSourceThunk() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);

    Result<Ref<UnregisteredItems>> raw = MakeRef<UnregisteredItems>();
    CHECK(raw);
    Result<Ref<ListBox>> emptyBox = MakeRef<ListBox>();
    CHECK(emptyBox);
    emptyBox.Value()->SetItemsSource(raw.Value());
    CHECK(emptyBox.Value()->GetCount() == 0U);
    CHECK(TryCastToInterface<IItemsSource>(raw.Value().Get()) == nullptr);

    Result<Ref<NamedItems>> named = MakeRef<NamedItems>();
    CHECK(named);
    Result<Ref<TextBlock>> one = MakeRef<TextBlock>();
    Result<Ref<TextBlock>> two = MakeRef<TextBlock>();
    CHECK(one && two);
    CHECK(named.Value()->Add(one.Value()));
    CHECK(named.Value()->Add(two.Value()));
    CHECK(TryCastToInterface<IItemsSource>(named.Value().Get()) != nullptr);

    Result<Ref<ListBox>> box = MakeRef<ListBox>();
    CHECK(box);
    box.Value()->SetItemsSource(named.Value());
    CHECK(box.Value()->GetCount() == 2U);
    CHECK(box.Value()->GetItem(1U).Get() == two.Value().Get());

    Result<Ref<ObservableCollection<BindingSlotItem>>> typed =
        MakeRef<ObservableCollection<BindingSlotItem>>();
    CHECK(typed);
    Result<Ref<BindingSlotItem>> first = MakeRef<BindingSlotItem>();
    Result<Ref<BindingSlotItem>> second = MakeRef<BindingSlotItem>();
    CHECK(first && second);
    CHECK(typed.Value()->Add(first.Value()));
    CHECK(typed.Value()->Add(second.Value()));
    CHECK(typed.Value()->RuntimeType() ==
        ObservableCollectionBase::StaticTypeId());
    CHECK(TryCastToInterface<IItemsSource>(typed.Value().Get()) != nullptr);
    CHECK(Aero::Collections::CollectionAsItemsSource(typed.Value().Get()) !=
        nullptr);

    Result<Ref<ListBox>> typedBox = MakeRef<ListBox>();
    CHECK(typedBox);
    typedBox.Value()->SetItemsSource(typed.Value());
    CHECK(typedBox.Value()->GetCount() == 2U);
    CHECK(typedBox.Value()->GetItem(0U).Get() == first.Value().Get());
    return true;
}

Result<Ref<BindingItemsViewModel>> MakeBindingItemsViewModel(
    std::uint32_t count) noexcept {
    Result<Ref<BindingItemsViewModel>> model =
        MakeRef<BindingItemsViewModel>();
    if (!model) return model;
    Result<Ref<ObservableCollection<BindingSlotItem>>> items =
        MakeRef<ObservableCollection<BindingSlotItem>>();
    if (!items) return items.GetStatus();
    Result<Ref<BindingPlayer>> player = MakeRef<BindingPlayer>();
    if (!player) return player.GetStatus();
    Result<Ref<ObservableCollection<BindingSlotItem>>> slots =
        MakeRef<ObservableCollection<BindingSlotItem>>();
    if (!slots) return slots.GetStatus();
    for (std::uint32_t index = 0U; index < count; ++index) {
        Result<Ref<BindingSlotItem>> item = MakeRef<BindingSlotItem>();
        if (!item) return item.GetStatus();
        Result<void> added = items.Value()->Add(item.Value());
        if (!added) return added.GetStatus();
        Result<Ref<BindingSlotItem>> slot = MakeRef<BindingSlotItem>();
        if (!slot) return slot.GetStatus();
        added = slots.Value()->Add(slot.Value());
        if (!added) return added.GetStatus();
    }
    model.Value()->SetItems(items.Value());
    player.Value()->SetSlots(slots.Value());
    model.Value()->SetPlayer(player.Value());
    return model;
}

void PumpBindings(LiveGui& live) noexcept {
    for (int i = 0; i < 16; ++i) {
        PumpForward(live);
    }
}

bool CountMatchesAfterLayout(
    LiveGui& live,
    ItemsControl& list,
    std::uint32_t expected) noexcept {
    static_cast<void>(list.ApplyTemplate());
    PumpBindings(live);
    static_cast<void>(list.ApplyTemplate());
    PumpBindings(live);
    const std::uint32_t count = list.GetCount();
    const std::uint32_t realized = list.GetRealizedItemCount();
    if (count != expected) {
        std::fprintf(
            stderr,
            "ItemsControl GetCount=%u expected=%u realized=%u host=%d\n",
            count,
            expected,
            realized,
            list.GetItemsHost() != nullptr);
        return false;
    }
    if (expected > 0U &&
        list.GetItemsHost() != nullptr &&
        realized == 0U) {
        std::fprintf(
            stderr,
            "ItemsControl host present but realized 0 of %u\n",
            expected);
        return false;
    }
    return true;
}

bool TestClrItemsSourceBindingAfterDataContext() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({240.0, 160.0});
    constexpr std::uint32_t kCount = 4U;
    Aero::Markup::XamlReader reader(live->gui);

    {
        Result<Ref<BindingItemsViewModel>> model =
            MakeBindingItemsViewModel(kCount);
        CHECK(model);
        CHECK(model.Value()->GetItems()->GetCount() == kCount);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " Width=\"240\" Height=\"160\">"
            "<ListBox x:Name=\"List\" ItemsSource=\"{Binding Items}\">"
            "<ListBox.ItemsPanel>"
            "<ItemsPanelTemplate><StackPanel/></ItemsPanelTemplate>"
            "</ListBox.ItemsPanel>"
            "</ListBox>"
            "</Grid>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
        Grid* grid = TryCast<Grid>(view.GetContent());
        CHECK(grid != nullptr);
        ListBox* list = grid->FindName<ListBox>(StringView("List"));
        CHECK(list != nullptr);
        CHECK(list->GetCount() == 0U);
        grid->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        CHECK(CountMatchesAfterLayout(*live, *list, kCount));
        CHECK(list->GetDataContext().AsObject().Get() == model.Value().Get());
    }

    {
        Result<Ref<BindingItemsViewModel>> model =
            MakeBindingItemsViewModel(kCount);
        CHECK(model);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " Width=\"240\" Height=\"160\">"
            "<ItemsControl x:Name=\"List\" ItemsSource=\"{Binding Items}\">"
            "<ItemsControl.ItemsPanel>"
            "<ItemsPanelTemplate><StackPanel/></ItemsPanelTemplate>"
            "</ItemsControl.ItemsPanel>"
            "</ItemsControl>"
            "</Grid>"));
        CHECK(document);
        Grid* grid = document.Value().Root<Grid>();
        CHECK(grid != nullptr);
        grid->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
        grid = TryCast<Grid>(view.GetContent());
        CHECK(grid != nullptr);
        ItemsControl* list = grid->FindName<ItemsControl>(StringView("List"));
        CHECK(list != nullptr);
        CHECK(CountMatchesAfterLayout(*live, *list, kCount));
    }

    {
        Result<Ref<BindingItemsViewModel>> model =
            MakeBindingItemsViewModel(kCount);
        CHECK(model);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<ListBox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " x:Name=\"List\" Width=\"240\" Height=\"160\""
            " ItemsSource=\"{Binding Items}\">"
            "<ListBox.ItemsPanel>"
            "<ItemsPanelTemplate><StackPanel/></ItemsPanelTemplate>"
            "</ListBox.ItemsPanel>"
            "</ListBox>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
        ListBox* list = TryCast<ListBox>(view.GetContent());
        CHECK(list != nullptr);
        list->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        CHECK(CountMatchesAfterLayout(*live, *list, kCount));
    }

    {
        Result<Ref<ObservableCollection<BindingSlotItem>>> items =
            MakeRef<ObservableCollection<BindingSlotItem>>();
        CHECK(items);
        for (std::uint32_t index = 0U; index < kCount; ++index) {
            Result<Ref<BindingSlotItem>> item = MakeRef<BindingSlotItem>();
            CHECK(item);
            CHECK(items.Value()->Add(item.Value()));
        }
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<ListBox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " x:Name=\"List\" Width=\"240\" Height=\"160\""
            " ItemsSource=\"{Binding}\">"
            "<ListBox.ItemsPanel>"
            "<ItemsPanelTemplate><StackPanel/></ItemsPanelTemplate>"
            "</ListBox.ItemsPanel>"
            "</ListBox>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
        ListBox* list = TryCast<ListBox>(view.GetContent());
        CHECK(list != nullptr);
        list->SetDataContext(Ref<Aero::Base::Object>(items.Value()));
        CHECK(CountMatchesAfterLayout(*live, *list, kCount));
    }

    {
        Result<Ref<BindingItemsViewModel>> model =
            MakeBindingItemsViewModel(kCount);
        CHECK(model);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<ContentControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " Width=\"240\" Height=\"160\">"
            "<Viewbox>"
            "<ItemsControl x:Name=\"List\" ItemsSource=\"{Binding Items}\">"
            "<ItemsControl.ItemsPanel>"
            "<ItemsPanelTemplate><StackPanel/></ItemsPanelTemplate>"
            "</ItemsControl.ItemsPanel>"
            "</ItemsControl>"
            "</Viewbox>"
            "</ContentControl>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
        ContentControl* host = TryCast<ContentControl>(view.GetContent());
        CHECK(host != nullptr);
        ItemsControl* list = host->FindName<ItemsControl>(StringView("List"));
        CHECK(list != nullptr);
        CHECK(list->GetCount() == 0U);
        host->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        CHECK(CountMatchesAfterLayout(*live, *list, kCount));
        CHECK(list->GetDataContext().AsObject().Get() == model.Value().Get());
    }

    {
        Result<Ref<BindingItemsViewModel>> model =
            MakeBindingItemsViewModel(kCount);
        CHECK(model);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " Width=\"240\" Height=\"160\">"
            "<ScrollViewer>"
            "<ItemsControl x:Name=\"List\" ItemsSource=\"{Binding Items}\">"
            "<ItemsControl.ItemsPanel>"
            "<ItemsPanelTemplate><StackPanel/></ItemsPanelTemplate>"
            "</ItemsControl.ItemsPanel>"
            "</ItemsControl>"
            "</ScrollViewer>"
            "</Grid>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
        Grid* grid = TryCast<Grid>(view.GetContent());
        CHECK(grid != nullptr);
        ItemsControl* list = grid->FindName<ItemsControl>(StringView("List"));
        CHECK(list != nullptr);
        grid->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        CHECK(CountMatchesAfterLayout(*live, *list, kCount));
    }

    {
        Result<Ref<BindingItemsViewModel>> model =
            MakeBindingItemsViewModel(kCount);
        CHECK(model);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<ContentControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " x:Name=\"Host\" Content=\"{Binding Player.Slots[0]}\"/>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 40.0}));
        ContentControl* host = TryCast<ContentControl>(view.GetContent());
        CHECK(host != nullptr);
        host->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        PumpBindings(*live);
        CHECK(host->GetContent().Kind() == Aero::Base::ValueKind::Object);
        CHECK(!host->GetContent().IsNullObject());
        CHECK(host->GetContent().AsObject().Get() ==
            model.Value()->GetPlayer()->GetSlots()->GetItem(0U).Get());
    }

    {
        Result<Ref<BindingItemsViewModel>> model =
            MakeBindingItemsViewModel(8U);
        CHECK(model);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<StackPanel xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
            "<ContentControl x:Name=\"Slot0\" Content=\"{Binding}\" DataContext=\"{Binding Player.Slots[0]}\"/>"
            "<ContentControl x:Name=\"Slot1\" Content=\"{Binding}\" DataContext=\"{Binding Player.Slots[1], Mode=TwoWay}\"/>"
            "<ContentControl x:Name=\"Slot2\" Content=\"{Binding}\" DataContext=\"{Binding Player.Slots[2], Mode=TwoWay}\"/>"
            "<ContentControl x:Name=\"Slot3\" Content=\"{Binding}\" DataContext=\"{Binding Player.Slots[3], Mode=TwoWay}\"/>"
            "<ContentControl x:Name=\"Slot4\" Content=\"{Binding}\" DataContext=\"{Binding Player.Slots[4], Mode=TwoWay}\"/>"
            "<ContentControl x:Name=\"Slot5\" Content=\"{Binding}\" DataContext=\"{Binding Player.Slots[5], Mode=TwoWay}\"/>"
            "<ContentControl x:Name=\"Slot6\" Content=\"{Binding}\" DataContext=\"{Binding Player.Slots[6], Mode=TwoWay}\"/>"
            "<ContentControl x:Name=\"Slot7\" Content=\"{Binding}\" DataContext=\"{Binding Player.Slots[7], Mode=TwoWay}\"/>"
            "</StackPanel>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
        auto* panel = TryCast<StackPanel>(view.GetContent());
        CHECK(panel != nullptr);
        panel->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        PumpBindings(*live);
        CHECK(panel->GetChildren().GetCount() == 8U);
        for (std::uint32_t index = 0U; index < 8U; ++index) {
            ContentControl* slot = TryCast<ContentControl>(
                panel->GetChildren().GetItem(index));
            CHECK(slot != nullptr);
            Aero::Base::Object* expected =
                model.Value()->GetPlayer()->GetSlots()->GetItem(index).Get();
            CHECK(slot->GetDataContext().Kind() == Aero::Base::ValueKind::Object);
            CHECK(slot->GetDataContext().AsObject().Get() == expected);
            CHECK(slot->GetContent().Kind() == Aero::Base::ValueKind::Object);
            CHECK(slot->GetContent().AsObject().Get() == expected);
        }
    }

    {
        Result<Ref<BindingDoItemsViewModel>> model =
            MakeRef<BindingDoItemsViewModel>();
        CHECK(model);
        Result<Ref<ObservableCollection<BindingSlotItem>>> items =
            MakeRef<ObservableCollection<BindingSlotItem>>();
        CHECK(items);
        Result<Ref<ObservableObjectCollection>> inventory =
            MakeRef<ObservableObjectCollection>();
        CHECK(inventory);
        for (std::uint32_t index = 0U; index < kCount; ++index) {
            Result<Ref<BindingSlotItem>> item = MakeRef<BindingSlotItem>();
            CHECK(item);
            CHECK(items.Value()->Add(item.Value()));
            CHECK(inventory.Value()->Add(item.Value()));
        }
        model.Value()->SetItems(items.Value());
        model.Value()->SetInventory(inventory.Value());
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " Width=\"240\" Height=\"160\">"
            "<Grid.Resources>"
            "<ControlTemplate x:Key=\"SV\" TargetType=\"ScrollViewer\">"
            "<ScrollContentPresenter Content=\"{TemplateBinding Content}\"/>"
            "</ControlTemplate>"
            "</Grid.Resources>"
            "<ScrollViewer Template=\"{StaticResource SV}\">"
            "<ItemsControl x:Name=\"List\" ItemsSource=\"{Binding Inventory}\">"
            "<ItemsControl.ItemsPanel>"
            "<ItemsPanelTemplate><UniformGrid Columns=\"5\"/></ItemsPanelTemplate>"
            "</ItemsControl.ItemsPanel>"
            "<ItemsControl.ItemTemplate>"
            "<DataTemplate><ContentControl Content=\"{Binding}\"/></DataTemplate>"
            "</ItemsControl.ItemTemplate>"
            "</ItemsControl>"
            "</ScrollViewer>"
            "</Grid>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
        Grid* grid = TryCast<Grid>(view.GetContent());
        CHECK(grid != nullptr);
        ItemsControl* list = grid->FindName<ItemsControl>(StringView("List"));
        CHECK(list != nullptr);
        CHECK(list->GetCount() == 0U);
        grid->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        CHECK(CountMatchesAfterLayout(*live, *list, kCount));
        CHECK(list->GetDataContext().AsObject().Get() == model.Value().Get());
    }

    {
        Result<Ref<ObservableObjectCollection>> named =
            MakeRef<ObservableObjectCollection>();
        CHECK(named);
        for (std::uint32_t index = 0U; index < kCount; ++index) {
            Result<Ref<BindingSlotItem>> item = MakeRef<BindingSlotItem>();
            CHECK(item);
            CHECK(named.Value()->Add(item.Value()));
        }
        Result<Ref<ListBox>> box = MakeRef<ListBox>();
        CHECK(box);
        box.Value()->SetItemsSource(named.Value());
        CHECK(box.Value()->GetCount() == kCount);
        CollectionView* defaultView =
            CollectionViewSource::GetDefaultView(named.Value().Get());
        CHECK(defaultView != nullptr);
        CHECK(defaultView->GetCount() == kCount);
    }
    return true;
}

bool TestGalleryXamlSurface() {
    static_assert(std::is_base_of<Aero::Base::Object, Aero::Input::CommandBinding>::value,
        "CommandBinding must be an Object so XAML can construct it");
    static_assert(std::is_base_of<Aero::Controls::HeaderedItemsControl, Aero::Controls::TreeViewItem>::value,
        "TreeViewItem must derive HeaderedItemsControl");
    static_assert(std::is_base_of<Aero::Controls::UserControl, Aero::Controls::Page>::value,
        "Page must derive UserControl");
    static_assert(std::is_base_of<Aero::Media::ImageSource, Aero::Media::BitmapImage>::value,
        "BitmapImage must derive ImageSource");

    Gui gui;
    Result<void> initialized = gui.Initialize();
    CHECK(initialized);
    Aero::Markup::XamlReader reader(gui);

    auto parse = [&](StringView markup) -> bool {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(markup);
        if (!document) {
            std::fprintf(stderr, "XAML parse failed: %s\n",
                document.GetStatus().message);
            return false;
        }
        return document.Value().IsValid();
    };

    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
            "<Grid.ColumnDefinitions>"
            "<ColumnDefinition Width=\"Auto\"/>"
            "<ColumnDefinition Width=\"*\"/>"
            "</Grid.ColumnDefinitions>"
            "<Grid.CommandBindings>"
            "<CommandBinding Command=\"Copy\"/>"
            "</Grid.CommandBindings>"
            "<Grid.InputBindings>"
            "<KeyBinding Command=\"Copy\" Key=\"C\" Modifiers=\"Control\"/>"
            "</Grid.InputBindings>"
            "<Button Content=\"Hi\"/>"
            "</Grid>"));
        CHECK(document);
        Grid* grid = document.Value().Root<Grid>();
        CHECK(grid != nullptr);
        CHECK(grid->GetCommandBindings().Size() == 1U);
        CHECK(grid->GetInputBindings().Size() == 1U);
        CHECK(grid->GetCommandBindings()[0]->GetCommand() != nullptr);
        CHECK(grid->GetInputBindings()[0]->GetCommand().Get() ==
            grid->GetCommandBindings()[0]->GetCommand());
        Result<Ref<Aero::Input::RoutedCommand>> copy =
            ApplicationCommands::Copy();
        CHECK(copy);
        CHECK(grid->GetCommandBindings()[0]->GetCommand() ==
            copy.Value().Get());
    }

    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<PasswordBox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:aero=\"clr-namespace:Aero.GUI.Extensions\""
            " aero:Text.Placeholder=\"secret\"/>"));
        CHECK(document);
        PasswordBox* password = document.Value().Root<PasswordBox>();
        CHECK(password != nullptr);
        CHECK(password->GetPlaceholder() == StringView("secret"));
    }

    CHECK(parse(StringView(
        "<Button xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:i=\"http://schemas.microsoft.com/expression/2010/interactivity\">"
        "<i:Interaction.Behaviors>"
        "</i:Interaction.Behaviors>"
        "</Button>")));

    CHECK(parse(StringView(
        "<ListView xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<ListView.View>"
        "<GridView>"
        "<GridViewColumn Header=\"Name\"/>"
        "</GridView>"
        "</ListView.View>"
        "</ListView>")));

    CHECK(parse(StringView(
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<VisualStateManager.VisualStateGroups>"
        "<VisualStateGroup>"
        "<VisualState Name=\"Normal\"/>"
        "<VisualStateGroup.Transitions>"
        "<VisualStateTransition From=\"Normal\" To=\"Normal\""
        " GeneratedDuration=\"0:0:0.1\"/>"
        "</VisualStateGroup.Transitions>"
        "</VisualStateGroup>"
        "</VisualStateManager.VisualStateGroups>"
        "</Grid>")));

    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Button xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " BlendMode=\"Multiply\">"
            "<Button.LayoutTransform>"
            "<RotateTransform Angle=\"15\"/>"
            "</Button.LayoutTransform>"
            "<Button.Effect>"
            "<BlurEffect Radius=\"4\"/>"
            "</Button.Effect>"
            "</Button>"));
        CHECK(document);
        Button* button = document.Value().Root<Button>();
        CHECK(button != nullptr);
        CHECK(button->GetBlendMode() == BlendMode::Multiply);
        CHECK(button->GetLayoutTransform().Get() != nullptr);
        CHECK(button->GetEffect().Get() != nullptr);
    }
    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Button xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:aero=\"clr-namespace:Aero.GUI.Extensions\""
            " aero:Element.BlendingMode=\"Screen\"/>"));
        CHECK(document);
        Button* button = document.Value().Root<Button>();
        CHECK(button != nullptr);
        CHECK(button->GetBlendMode() == BlendMode::Screen);
    }

    CHECK(parse(StringView(
        "<TabControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<TabItem Header=\"One\"><TextBlock Text=\"A\"/></TabItem>"
        "<TabItem Header=\"Two\"><TextBlock Text=\"B\"/></TabItem>"
        "</TabControl>")));
    CHECK(parse(StringView(
        "<ScrollViewer xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<StackPanel><TextBlock Text=\"Hi\"/></StackPanel>"
        "</ScrollViewer>")));
    return true;
}

bool TestGalleryHostXamlSurface() {
    Gui gui;
    Result<void> initialized = gui.Initialize();
    CHECK(initialized);
    Aero::Markup::XamlReader reader(gui);
    Aero::Diagnostics::DiagnosticBag diagnostics;

    auto failIfGalleryHostError = [&](StringView where,
                                      const Status& status) -> bool {
        if (ReportsGalleryHostFailure(CStringView(status.message))) {
            std::fprintf(stderr, "%.*s reported gallery host failure: %s\n",
                static_cast<int>(where.SizeBytes()),
                where.Data(),
                status.message);
            DumpDiagnostics(diagnostics);
            return true;
        }
        if (DiagnosticsReportGalleryHostFailure(diagnostics)) {
            std::fprintf(stderr, "%.*s diagnostics contain gallery host failure\n",
                static_cast<int>(where.SizeBytes()),
                where.Data());
            DumpDiagnostics(diagnostics);
            return true;
        }
        return false;
    };

    {
        diagnostics.Clear();
        Result<Aero::Markup::XamlDocument> document = reader.Parse(
            StringView(
                "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
                " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
                " xmlns:b=\"http://schemas.microsoft.com/xaml/behaviors\">"
                "<Grid.Resources>"
                "<Storyboard x:Key=\"ShowContainer1\"/>"
                "<Storyboard x:Key=\"ShowContainer2\"/>"
                "</Grid.Resources>"
                "<ContentControl x:Name=\"SampleContainer1\" Visibility=\"Collapsed\">"
                "<b:Interaction.Triggers>"
                "<b:DataTrigger Binding=\"{Binding Content, ElementName=SampleContainer1}\""
                " Comparison=\"NotEqual\" Value=\"{x:Null}\">"
                "<b:ControlStoryboardAction Storyboard=\"{StaticResource ShowContainer1}\"/>"
                "</b:DataTrigger>"
                "<b:StoryboardCompletedTrigger Storyboard=\"{StaticResource ShowContainer1}\">"
                "<b:ChangePropertyAction PropertyName=\"Content\" Value=\"{x:Null}\""
                " TargetName=\"SampleContainer2\"/>"
                "</b:StoryboardCompletedTrigger>"
                "</b:Interaction.Triggers>"
                "<ContentControl.RenderTransform>"
                "<TranslateTransform/>"
                "</ContentControl.RenderTransform>"
                "</ContentControl>"
                "<ContentControl x:Name=\"SampleContainer2\" Visibility=\"Collapsed\">"
                "<b:Interaction.Triggers>"
                "<b:DataTrigger Binding=\"{Binding Content, ElementName=SampleContainer2}\""
                " Comparison=\"NotEqual\" Value=\"{x:Null}\">"
                "<b:ControlStoryboardAction Storyboard=\"{StaticResource ShowContainer2}\"/>"
                "</b:DataTrigger>"
                "<b:StoryboardCompletedTrigger Storyboard=\"{StaticResource ShowContainer2}\">"
                "<b:ChangePropertyAction PropertyName=\"Content\" Value=\"{x:Null}\""
                " TargetName=\"SampleContainer1\"/>"
                "</b:StoryboardCompletedTrigger>"
                "</b:Interaction.Triggers>"
                "<ContentControl.RenderTransform>"
                "<TranslateTransform/>"
                "</ContentControl.RenderTransform>"
                "</ContentControl>"
                "</Grid>"),
            {},
            {},
            &diagnostics);
        if (!document) {
            std::fprintf(stderr, "Interaction.Triggers parse failed: %s\n",
                document.GetStatus().message);
            DumpDiagnostics(diagnostics);
        }
        CHECK(!failIfGalleryHostError(
            StringView("Interaction.Triggers"),
            document.GetStatus()));
        CHECK(document);
        Grid* grid = document.Value().Root<Grid>();
        CHECK(grid != nullptr);
        CHECK(grid->FindName<ContentControl>("SampleContainer1") != nullptr);
        CHECK(grid->FindName<ContentControl>("SampleContainer2") != nullptr);
    }

    {
        diagnostics.Clear();
        std::error_code error;
        const std::filesystem::path dir =
            std::filesystem::temp_directory_path(error) /
            "aero-gallery-host-f940";
        CHECK(!error);
        std::filesystem::create_directories(dir, error);
        CHECK(!error);
        const std::filesystem::path resourcesPath = dir / "Resources.xaml";
        const std::filesystem::path appPath = dir / "App.xaml";
        CHECK(WriteUtf8File(resourcesPath,
            "<ResourceDictionary xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
            "<SolidColorBrush x:Key=\"MainWindowBackground\" Color=\"#FF1A1A2E\"/>"
            "<Geometry x:Key=\"AeroLogoGeometry\">M0,0 L10,0 L10,10 Z</Geometry>"
            "<sys:Double xmlns:sys=\"clr-namespace:System;assembly=mscorlib\""
            " x:Key=\"SelectorBarWidth\">48</sys:Double>"
            "</ResourceDictionary>"));
        CHECK(WriteUtf8File(appPath,
            "<ResourceDictionary xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
            "<ResourceDictionary.MergedDictionaries>"
            "<ResourceDictionary Source=\""
            "pack://application:,,,/Aero.GUI.Extensions;component/Theme/AeroTheme.DarkBlue.xaml\"/>"
            "<ResourceDictionary Source=\"Resources.xaml\"/>"
            "</ResourceDictionary.MergedDictionaries>"
            "</ResourceDictionary>"));
        const std::string appPathText = appPath.string();
        Result<Aero::Markup::XamlDocument> document = reader.Load(
            AsView(appPathText),
            {},
            &diagnostics);
        if (!document) {
            std::fprintf(stderr,
                "Gallery ResourceDictionary Source load failed: %s\n",
                document.GetStatus().message);
            DumpDiagnostics(diagnostics);
        }
        CHECK(!failIfGalleryHostError(
            StringView("ResourceDictionary Source"),
            document.GetStatus()));
        CHECK(document);
        Aero::ResourceDictionary* dictionary =
            document.Value().Root<Aero::ResourceDictionary>();
        CHECK(dictionary != nullptr);
        CHECK(dictionary->MergedDictionaryCount() >= 2U);
        CHECK(dictionary->Contains(StringView("AeroLogoGeometry")));
        CHECK(dictionary->Contains(StringView("MainWindowBackground")));
        CHECK(dictionary->Contains(StringView("SelectorBarWidth")));
    }
    return true;
}

bool TestTutorialXamlSurface() {
    static_assert(std::is_base_of<Aero::Controls::ContentControl, Aero::Controls::UserControl>::value,
        "UserControl must derive ContentControl");
    static_assert(std::is_base_of<Aero::Data::BindingBase, Aero::Data::MultiBinding>::value,
        "MultiBinding must derive BindingBase");
    static_assert(std::is_base_of<Aero::Media::Animation::TimelineGroup, Aero::Media::Animation::ParallelTimeline>::value,
        "ParallelTimeline must derive TimelineGroup");

    Gui gui;
    Result<void> initialized = gui.Initialize();
    CHECK(initialized);
    Aero::Markup::XamlReader reader(gui);

    auto parse = [&](StringView markup) -> bool {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(markup);
        if (!document) {
            std::fprintf(stderr, "XAML parse failed: %s\n",
                document.GetStatus().message);
            return false;
        }
        return document.Value().IsValid();
    };

    CHECK(parse(StringView(
        "<UserControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<UserControl.Resources>"
        "<BooleanToVisibilityConverter x:Key=\"VisibleWhenTrue\"/>"
        "</UserControl.Resources>"
        "<Grid><TextBlock Text=\"ok\"/></Grid>"
        "</UserControl>")));
    CHECK(parse(StringView(
        "<ItemsControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<ItemsControl.ItemsPanel>"
        "<ItemsPanelTemplate><UniformGrid Columns=\"2\"/></ItemsPanelTemplate>"
        "</ItemsControl.ItemsPanel>"
        "</ItemsControl>")));
    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<TextBlock xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " FontFamily=\"./#Demo\"/>"));
        CHECK(document);
        TextBlock* text = document.Value().Root<TextBlock>();
        CHECK(text != nullptr);
        const Ref<Aero::Media::FontFamily> family = text->GetFontFamily();
        CHECK(family.Get() != nullptr);
        CHECK(family->GetSource() == StringView("./#Demo"));
    }
    CHECK(parse(StringView(
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<Grid.RenderTransform>"
        "<TransformGroup>"
        "<MatrixTransform Matrix=\"1,0,0,1,8,4\"/>"
        "</TransformGroup>"
        "</Grid.RenderTransform>"
        "<Thumb Width=\"12\" Height=\"12\"/>"
        "<Popup IsOpen=\"False\"><Border/></Popup>"
        "</Grid>")));
    CHECK(parse(StringView(
        "<ParallelTimeline xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<DoubleAnimation From=\"0\" To=\"1\" Duration=\"0:0:0.2\"/>"
        "</ParallelTimeline>")));
    CHECK(parse(StringView(
        "<Page xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<PasswordBox/>"
        "</Page>")));
    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<TextBlock xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " FontFamily=\"./#Rajdhani SemiBold\"/>"));
        CHECK(document);
        TextBlock* text = document.Value().Root<TextBlock>();
        CHECK(text != nullptr);
        const Ref<Aero::Media::FontFamily> family = text->GetFontFamily();
        CHECK(family.Get() != nullptr);
        CHECK(family->GetSource() == StringView("./#Rajdhani SemiBold"));
    }
    CHECK(parse(StringView(
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid.Resources>"
        "<ImageSource x:Key=\"FillBlue\">fill-blue.png</ImageSource>"
        "</Grid.Resources>"
        "<BulletDecorator><BulletDecorator.Bullet><Ellipse Width=\"8\" Height=\"8\"/></BulletDecorator.Bullet>"
        "<TextBlock Text=\"item\"/></BulletDecorator>"
        "</Grid>")));
    CHECK(parse(StringView(
        "<Path xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<Path.Data>"
        "<PathGeometry>"
        "<PathFigure StartPoint=\"0,0\"><LineSegment Point=\"10,0\"/></PathFigure>"
        "</PathGeometry>"
        "</Path.Data>"
        "</Path>")));
    CHECK(parse(StringView(
        "<PointAnimationUsingKeyFrames xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<DiscretePointKeyFrame KeyTime=\"0\" Value=\"0,0\"/>"
        "<EasingPointKeyFrame KeyTime=\"0:0:0.2\" Value=\"8,4\"/>"
        "</PointAnimationUsingKeyFrames>")));
    CHECK(parse(StringView(
        "<ThicknessAnimationUsingKeyFrames xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
        "<EasingThicknessKeyFrame KeyTime=\"0:0:0.1\" Value=\"1,2,3,4\"/>"
        "</ThicknessAnimationUsingKeyFrames>")));
    CHECK(parse(StringView(
        "<Style xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" TargetType=\"Button\">"
        "<Style.Triggers>"
        "<MultiDataTrigger>"
        "<MultiDataTrigger.Conditions>"
        "<Condition Binding=\"{Binding Active}\" Value=\"True\"/>"
        "</MultiDataTrigger.Conditions>"
        "<Setter Property=\"Opacity\" Value=\"0.5\"/>"
        "</MultiDataTrigger>"
        "</Style.Triggers>"
        "</Style>")));
    CHECK(parse(StringView(
        "<Style xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" TargetType=\"Button\">"
        "<Style.Triggers>"
        "<MultiDataTrigger>"
        "<MultiDataTrigger.Conditions>"
        "<Condition Binding=\"{Binding Active}\" Value=\"True\"/>"
        "<Condition Binding=\"{Binding Enabled}\" Value=\"True\"/>"
        "</MultiDataTrigger.Conditions>"
        "<Setter Property=\"Opacity\" Value=\"0.25\"/>"
        "</MultiDataTrigger>"
        "</Style.Triggers>"
        "</Style>")));
    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:noesis=\"clr-namespace:NoesisGUIExtensions;assembly=Noesis.GUI.Extensions\">"
            "<noesis:Element.Transform3D>"
            "<noesis:CompositeTransform3D RotationY=\"-8\" TranslateZ=\"40\" ScaleX=\"1.2\"/>"
            "</noesis:Element.Transform3D>"
            "<Grid.LayoutTransform>"
            "<RotateTransform Angle=\"10\"/>"
            "</Grid.LayoutTransform>"
            "</Grid>"));
        CHECK(document);
        Grid* grid = document.Value().Root<Grid>();
        CHECK(grid != nullptr);
        CHECK(grid->GetLayoutTransform().Get() != nullptr);
        CompositeTransform3D* transform =
            TryCast<CompositeTransform3D>(grid->GetTransform3D().Get());
        CHECK(transform != nullptr);
        CHECK(Near(transform->GetRotationY(), -8.0));
        CHECK(Near(transform->GetTranslateZ(), 40.0));
        CHECK(Near(transform->GetScaleX(), 1.2));
    }
    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Border xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">"
            "<Border.Effect><ShaderEffect PixelShader=\"noise.frag\"/></Border.Effect>"
            "</Border>"));
        CHECK(document);
        Aero::Controls::Border* border =
            document.Value().Root<Aero::Controls::Border>();
        CHECK(border != nullptr);
        ShaderEffect* shader = TryCast<ShaderEffect>(border->GetEffect().Get());
        CHECK(shader != nullptr);
        CHECK(shader->GetPixelShader() == StringView("noise.frag"));
    }
    CHECK(parse(StringView(
        "<Path xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:b=\"http://schemas.microsoft.com/xaml/behaviors\""
        " xmlns:noesis=\"clr-namespace:NoesisGUIExtensions;assembly=Noesis.GUI.Extensions\""
        " Data=\"M0,0 L10,0 L10,10 Z\">"
        "<b:Interaction.Behaviors>"
        "<noesis:BackgroundEffectBehavior>"
        "<BlurEffect Radius=\"6\"/>"
        "</noesis:BackgroundEffectBehavior>"
        "</b:Interaction.Behaviors>"
        "</Path>")));
    return true;
}

bool TestTutorialRuntimePatterns() {
    LiveGui* live = NewTutorialLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    Aero::Markup::XamlReader reader(live->gui);
    double clock = 0.0;
    auto pump = [&](double deltaSeconds) noexcept {
        clock += deltaSeconds;
        static_cast<void>(view.Update(clock));
    };

    {
        Result<Ref<NumericUpDown>> control = MakeRef<NumericUpDown>();
        CHECK(control);
        Result<void> loaded = live->gui.LoadComponent(
            *control.Value(), "memory:///NumericUpDown.xaml");
        CHECK(loaded);
        CHECK(control.Value()->FindName("UpButton") != nullptr);
        control.Value()->SetNumericValue(10);
        Result<void> mounted = view.SetContent(
            control.Value(), {200.0, 80.0});
        if (!mounted) {
            std::fprintf(stderr, "SetContent(NumericUpDown) failed: %s\n",
                mounted.GetStatus().message);
            DumpDiagnostics(live->diagnostics);
        }
        CHECK(mounted);
        pump( 0.016);
        Aero::Controls::Primitives::RepeatButton* up =
            control.Value()->FindName<Aero::Controls::Primitives::RepeatButton>(
                "UpButton");
        CHECK(up != nullptr);
        Aero::RoutedEventArgs click;
        control.Value()->UpButton_Click(up, click);
        pump( 0.032);
        CHECK(control.Value()->GetNumericValue() == 11);
    }

    {
        Result<Ref<RgbModel>> rgb = MakeRef<RgbModel>();
        CHECK(rgb);
        rgb.Value()->SetR(255);
        rgb.Value()->SetG(0);
        rgb.Value()->SetB(0);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Rectangle xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " xmlns:local=\"clr-namespace:UserControls\""
            " Width=\"40\" Height=\"40\">"
            "<Rectangle.Resources>"
            "<local:ColorConverter x:Key=\"ColorConverter\"/>"
            "</Rectangle.Resources>"
            "<Rectangle.Fill>"
            "<SolidColorBrush>"
            "<SolidColorBrush.Color>"
            "<MultiBinding Converter=\"{StaticResource ColorConverter}\">"
            "<Binding Path=\"R\"/>"
            "<Binding Path=\"G\"/>"
            "<Binding Path=\"B\"/>"
            "</MultiBinding>"
            "</SolidColorBrush.Color>"
            "</SolidColorBrush>"
            "</Rectangle.Fill>"
            "</Rectangle>"));
        CHECK(document);
        Aero::Shapes::Rectangle* rectangle =
            document.Value().Root<Aero::Shapes::Rectangle>();
        CHECK(rectangle != nullptr);
        rectangle->SetValue(
            FrameworkElement::DataContextProperty,
            Aero::Value::FromObject(RgbModel::StaticTypeId(), rgb.Value()));
        CHECK(view.SetContent(
            std::move(document).Value(), {80.0, 80.0}));
        rectangle = TryCast<Aero::Shapes::Rectangle>(view.GetContent());
        CHECK(rectangle != nullptr);
        pump( 0.016);
        pump( 0.032);
        SolidColorBrush* brush =
            TryCast<SolidColorBrush>(rectangle->GetFill().Get());
        CHECK(brush != nullptr);
        CHECK(Near(static_cast<double>(brush->GetColor().red), 1.0, 0.05));
        CHECK(Near(static_cast<double>(brush->GetColor().green), 0.0, 0.05));
        rgb.Value()->SetG(255);
        pump( 0.048);
        CHECK(Near(static_cast<double>(brush->GetColor().green), 1.0, 0.05));
    }

    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<local:CircleAnimation xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:local=\"clr-namespace:CustomAnimation\""
            " From=\"0\" To=\"10\" Radius=\"1\"/>"));
        CHECK(document);
        CircleAnimation* animation = document.Value().Root<CircleAnimation>();
        CHECK(animation != nullptr);
        CHECK(Near(animation->GetRadius(), 1.0));
        const double mid = animation->GetCurrentValue(0.0, 10.0, 0.5);
        CHECK(mid > 0.0 && mid < 10.0);
        CHECK(!Near(mid, 5.0, 0.01));
    }

    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " xmlns:local=\"clr-namespace:CustomControl\" Width=\"120\" Height=\"40\">"
            "<Grid.Resources>"
            "<Style TargetType=\"{x:Type local:Clock}\">"
            "<Setter Property=\"Background\" Value=\"Red\"/>"
            "<Setter Property=\"Template\">"
            "<Setter.Value>"
            "<ControlTemplate TargetType=\"{x:Type local:Clock}\">"
            "<Border Background=\"{TemplateBinding Background}\">"
            "<TextBlock Text=\"{Binding Hour, RelativeSource={RelativeSource TemplatedParent}}\"/>"
            "</Border>"
            "</ControlTemplate>"
            "</Setter.Value>"
            "</Setter>"
            "</Style>"
            "</Grid.Resources>"
            "<local:Clock x:Name=\"Face\" Hour=\"7\"/>"
            "</Grid>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {120.0, 40.0}));
        pump( 0.016);
        Grid* grid = TryCast<Grid>(view.GetContent());
        CHECK(grid != nullptr);
        Clock* clock = grid->FindName<Clock>("Face");
        CHECK(clock != nullptr);
        CHECK(clock->GetHour() == 7);
        static_cast<void>(clock->ApplyTemplate());
        CHECK(clock->GetBackground().Get() != nullptr);
        CHECK(Aero::Media::VisualTreeHelper::GetChildrenCount(*clock) >= 1U);
    }

    {
        Game::renderCount = 0;
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<local:Game xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:local=\"clr-namespace:CustomRender\" Width=\"80\" Height=\"60\"/>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {80.0, 60.0}));
        pump( 0.016);
        pump( 0.032);
        if (Game::renderCount <= 0) {
            std::fprintf(stderr, "Game OnRender was not called\n");
            DumpDiagnostics(live->diagnostics);
            Game* game = TryCast<Game>(view.GetContent());
            if (game != nullptr) {
                std::fprintf(stderr,
                    "Game arrangeValid=%d renderSize=%g x %g visibility=%d\n",
                    game->GetIsArrangeValid() ? 1 : 0,
                    game->GetRenderSize().width,
                    game->GetRenderSize().height,
                    static_cast<int>(game->GetVisibility()));
            }
        }
        CHECK(Game::renderCount > 0);
    }

    {
        Result<Ref<CommandsViewModel>> model = MakeRef<CommandsViewModel>();
        CHECK(model);
        Result<Ref<HelloCommand>> hello = MakeRef<HelloCommand>(model.Value().Get());
        CHECK(hello);
        model.Value()->SetSayHelloCommand(hello.Value());
        String input;
        CHECK(input.Assign("Ada"));
        model.Value()->SetInput(std::move(input));
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Button xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " Width=\"80\" Height=\"32\" Command=\"{Binding SayHelloCommand}\""
            " CommandParameter=\"Ada\" Content=\"Say\"/>"));
        CHECK(document);
        Button* button = document.Value().Root<Button>();
        CHECK(button != nullptr);
        button->SetValue(
            FrameworkElement::DataContextProperty,
            Aero::Value::FromObject(
                CommandsViewModel::StaticTypeId(), model.Value()));
        CHECK(view.SetContent(std::move(document).Value(), {80.0, 32.0}));
        button = TryCast<Button>(view.GetContent());
        CHECK(button != nullptr);
        pump( 0.016);
        CHECK(button->GetCommand() != nullptr);
        Result<Aero::Value> encoded = Aero::Value::TryFromString(
            Aero::Meta::TypeOf<String>(), StringView("Ada"));
        CHECK(encoded);
        button->GetCommand()->Execute(encoded.Value(), button);
        pump( 0.032);
        CHECK(hello.Value()->GetExecutionCount() >= 1U);
        CHECK(model.Value()->GetOutput() == StringView("Hello Ada") ||
            model.Value()->GetOutput() == StringView("Hello"));
    }

    {
        Result<Aero::Markup::XamlDocument> host = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" Width=\"200\" Height=\"40\">"
            "<TextBlock x:Name=\"Label\" Text=\"{DynamicResource Greeting}\"/>"
            "</Grid>"));
        CHECK(host);
        {
            Result<void> mounted = view.SetContent(
                std::move(host).Value(), {200.0, 40.0});
            if (!mounted) {
                std::fprintf(stderr, "SetContent(Localization) failed: %s\n",
                    mounted.GetStatus().message);
                DumpDiagnostics(live->diagnostics);
            }
            CHECK(mounted);
        }
        pump( 0.016);
        Grid* grid = TryCast<Grid>(view.GetContent());
        CHECK(grid != nullptr);
        Result<Aero::Markup::XamlDocument> english = reader.Parse(
            StringView(kLanguageEnXaml));
        Result<Aero::Markup::XamlDocument> french = reader.Parse(
            StringView(kLanguageFrXaml));
        CHECK(english && french);
        Aero::ResourceDictionary* en =
            english.Value().Root<Aero::ResourceDictionary>();
        Aero::ResourceDictionary* fr =
            french.Value().Root<Aero::ResourceDictionary>();
        CHECK(en != nullptr && fr != nullptr);
        CHECK(grid->GetResources().AddMerged(*en));
        pump( 0.032);
        TextBlock* label = grid->FindName<TextBlock>("Label");
        CHECK(label != nullptr);
        CHECK(label->GetText() == StringView("Hello"));
        grid->GetResources().ClearMergedDictionaries();
        CHECK(grid->GetResources().AddMerged(*fr));
        pump( 0.048);
        CHECK(label->GetText() == StringView("Bonjour"));
    }

    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" Width=\"80\" Height=\"80\">"
            "<Image x:Name=\"Backdrop\" Width=\"80\" Height=\"80\"/>"
            "<Border x:Name=\"Overlay\" Width=\"40\" Height=\"40\">"
            "<Border.Effect><BlurEffect Radius=\"12\"/></Border.Effect>"
            "</Border>"
            "</Grid>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {80.0, 80.0}));
        pump( 0.016);
        Grid* grid = TryCast<Grid>(view.GetContent());
        CHECK(grid != nullptr);
        CHECK(grid->FindName("Backdrop") != nullptr);
        Aero::Controls::Border* overlay =
            grid->FindName<Aero::Controls::Border>("Overlay");
        CHECK(overlay != nullptr);
        Aero::Media::BlurEffect* blur =
            TryCast<Aero::Media::BlurEffect>(overlay->GetEffect().Get());
        CHECK(blur != nullptr);
        CHECK(Near(blur->GetRadius(), 12.0));
    }

    {
        Result<Ref<ShaderEffect>> effect = MakeRef<ShaderEffect>();
        CHECK(effect);
        effect.Value()->SetPixelShader("custom.frag");
        CHECK(effect.Value()->GetPixelShader() == StringView("custom.frag"));
        const std::uint8_t bytes[] = {0x43, 0x47, 0x58, 0x00};
        CHECK(effect.Value()->SetBytecode({bytes, 4U}));
        CHECK(effect.Value()->GetBytecode().Size() == 4U);
        // .noesisbrush is a Noesis offline compiler artifact. Aero loads
        // PixelShader source or raw bytecode; it does not compile brushes.
    }

    {
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<ItemsControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" Width=\"80\" Height=\"80\">"
            "<ItemsControl.ItemsPanel>"
            "<ItemsPanelTemplate><UniformGrid Columns=\"2\"/></ItemsPanelTemplate>"
            "</ItemsControl.ItemsPanel>"
            "<TextBlock Text=\"A\"/><TextBlock Text=\"B\"/>"
            "</ItemsControl>"));
        CHECK(document);
        ItemsControl* items = document.Value().Root<ItemsControl>();
        CHECK(items != nullptr);
        CHECK(items->GetItemsPanel() != nullptr);
        CHECK(view.SetContent(std::move(document).Value(), {80.0, 80.0}));
        pump( 0.016);
    }
    return true;
}

bool TestLoadedIntroStoryboard() {
    ViewOptions options;
    options.automaticAnimationClock = false;
    LiveGui* live = NewLiveGui(options);
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({200.0, 80.0});

    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
        "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
        " Width=\"200\" Height=\"80\">"
        "<Grid.Resources>"
        "<Storyboard x:Key=\"Anim.Intro\">"
        "<DoubleAnimationUsingKeyFrames Storyboard.TargetProperty=\"(UIElement.Opacity)\""
        " Storyboard.TargetName=\"Child\">"
        "<EasingDoubleKeyFrame KeyTime=\"0\" Value=\"0\"/>"
        "<EasingDoubleKeyFrame KeyTime=\"0:0:0.2\" Value=\"1\"/>"
        "</DoubleAnimationUsingKeyFrames>"
        "<DoubleAnimationUsingKeyFrames"
        " Storyboard.TargetProperty=\"(UIElement.RenderTransform).(TransformGroup.Children)[3].(TranslateTransform.X)\""
        " Storyboard.TargetName=\"Child\">"
        "<EasingDoubleKeyFrame KeyTime=\"0\" Value=\"40\"/>"
        "<EasingDoubleKeyFrame KeyTime=\"0:0:0.2\" Value=\"0\"/>"
        "</DoubleAnimationUsingKeyFrames>"
        "<DoubleAnimationUsingKeyFrames Storyboard.TargetProperty=\"(UIElement.Opacity)\""
        " Storyboard.TargetName=\"Missing\">"
        "<EasingDoubleKeyFrame KeyTime=\"0\" Value=\"0\"/>"
        "<EasingDoubleKeyFrame KeyTime=\"0:0:0.2\" Value=\"1\"/>"
        "</DoubleAnimationUsingKeyFrames>"
        "</Storyboard>"
        "</Grid.Resources>"
        "<Grid.Triggers>"
        "<EventTrigger RoutedEvent=\"FrameworkElement.Loaded\">"
        "<BeginStoryboard Storyboard=\"{StaticResource Anim.Intro}\"/>"
        "</EventTrigger>"
        "</Grid.Triggers>"
        "<Border x:Name=\"Child\" Width=\"40\" Height=\"40\">"
        "<Border.RenderTransform>"
        "<TransformGroup>"
        "<ScaleTransform/>"
        "<SkewTransform/>"
        "<RotateTransform/>"
        "<TranslateTransform X=\"40\"/>"
        "</TransformGroup>"
        "</Border.RenderTransform>"
        "</Border>"
        "</Grid>"));
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {200.0, 80.0}));
    Grid* root = TryCast<Grid>(view.GetContent());
    CHECK(root != nullptr);
    Aero::Controls::Border* child =
        root->FindName<Aero::Controls::Border>(StringView("Child"));
    CHECK(child != nullptr);
    Pump(view, 0.0);
    CHECK(Near(child->GetOpacity(), 0.0, 0.05));
    Pump(view, 0.25);
    CHECK(Near(child->GetOpacity(), 1.0, 0.05));
    auto* group = TryCast<Aero::Media::TransformGroup>(
        child->GetRenderTransform().Get());
    CHECK(group != nullptr);
    CHECK(group->GetChildren().Size() > 3U);
    auto* translate = TryCast<Aero::Media::TranslateTransform>(
        group->GetChildren()[3].Get());
    CHECK(translate != nullptr);
    CHECK(Near(translate->GetX(), 0.0, 0.75));
    return true;
}

bool TestBoardFlipRotationYStoryboard() {
    ViewOptions options;
    options.automaticAnimationClock = false;
    LiveGui* live = NewLiveGui(options);
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({400.0, 400.0});

    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
        "<Viewbox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
        " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
        " xmlns:aero=\"clr-namespace:AeroGUIExtensions;assembly=Aero.GUI.Extensions\""
        " xmlns:b=\"http://schemas.microsoft.com/xaml/behaviors\""
        " Width=\"400\" Height=\"400\">"
        "<Viewbox.Resources>"
        "<Storyboard x:Key=\"BoardFlip1Anim\">"
        "<DoubleAnimationUsingKeyFrames"
        " Storyboard.TargetProperty=\"(aero:Element.Transform3D).(aero:CompositeTransform3D.RotationY)\""
        " Storyboard.TargetName=\"Board\">"
        "<EasingDoubleKeyFrame KeyTime=\"0:0:0.1\" Value=\"-90\">"
        "<EasingDoubleKeyFrame.EasingFunction>"
        "<ExponentialEase EasingMode=\"EaseInOut\"/>"
        "</EasingDoubleKeyFrame.EasingFunction>"
        "</EasingDoubleKeyFrame>"
        "</DoubleAnimationUsingKeyFrames>"
        "</Storyboard>"
        "</Viewbox.Resources>"
        "<Grid x:Name=\"Board\" Width=\"200\" Height=\"200\" Margin=\"15\">"
        "<aero:Element.Transform3D>"
        "<aero:CompositeTransform3D CenterX=\"100\" CenterY=\"100\"/>"
        "</aero:Element.Transform3D>"
        "<Border x:Name=\"Probe\" Width=\"20\" Height=\"20\""
        " HorizontalAlignment=\"Left\" VerticalAlignment=\"Center\""
        " Background=\"White\"/>"
        "<Button x:Name=\"Start\" Background=\"#00FF0000\">"
        "<b:Interaction.Triggers>"
        "<b:EventTrigger EventName=\"Click\">"
        "<b:ControlStoryboardAction Storyboard=\"{StaticResource BoardFlip1Anim}\"/>"
        "</b:EventTrigger>"
        "</b:Interaction.Triggers>"
        "</Button>"
        "</Grid>"
        "</Viewbox>"));
    if (!document) {
        std::fprintf(stderr, "board flip XAML parse failed: %s\n",
            document.GetStatus().message);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {400.0, 400.0}));
    Pump(view, 0.0);

    FrameworkElement* root = view.GetContent();
    CHECK(root != nullptr);
    Grid* board = root->FindName<Grid>(StringView("Board"));
    CHECK(board != nullptr);
    CompositeTransform3D* transform =
        TryCast<CompositeTransform3D>(board->GetTransform3D().Get());
    if (transform == nullptr) {
        std::fprintf(stderr, "Board GetTransform3D was null after load\n");
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(transform != nullptr);
    CHECK(Near(transform->GetRotationY(), 0.0, 0.5));

    Button* start = root->FindName<Button>(StringView("Start"));
    CHECK(start != nullptr);
    Point click{};
    CHECK(start->TryPointToScreen(
        {start->GetRenderSize().width * 0.5,
         start->GetRenderSize().height * 0.5},
        click));
    static_cast<void>(view.MouseButtonDown(
        static_cast<int>(click.x),
        static_cast<int>(click.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.016);
    static_cast<void>(view.MouseButtonUp(
        static_cast<int>(click.x),
        static_cast<int>(click.y),
        Aero::Input::MouseButton::Left));
    Pump(view, 0.032);

    Border* probe = root->FindName<Aero::Controls::Border>(StringView("Probe"));
    CHECK(probe != nullptr);
    Point rest{};
    CHECK(probe->TryPointToScreen({10.0, 10.0}, rest));

    Pump(view, 0.09);
    const double midY = transform->GetRotationY();
    if (!(midY < -8.0)) {
        std::fprintf(stderr,
            "BoardFlip1 RotationY stayed at %.3f after Click (expected mid-flip)\n",
            midY);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(midY < -8.0);

    const double projective = Aero::MaxAbsCommittedProjectiveM13(view);
    if (!(projective > 1.0e-4)) {
        std::fprintf(stderr,
            "BoardFlip1 RotationY=%.3f but committed renderTransform stayed affine "
            "(max |m13/m23/(m33-1)|=%.6g)\n",
            midY, projective);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(projective > 1.0e-4);

    Point flipped{};
    CHECK(probe->TryPointToScreen({10.0, 10.0}, flipped));
    const double shift = flipped.x - rest.x;
    if (!(shift > 2.0 || shift < -2.0)) {
        std::fprintf(stderr,
            "BoardFlip1 RotationY=%.3f but Probe screen x did not move "
            "(rest=%.1f flipped=%.1f)\n",
            midY, rest.x, flipped.x);
        DumpDiagnostics(live->diagnostics);
    }
    CHECK(shift > 2.0 || shift < -2.0);

    Pump(view, 0.20);
    CHECK(Near(transform->GetRotationY(), -90.0, 4.0));
    return true;
}

} // namespace

bool TestStyleSetterMergedStaticResource();
bool TestMergedNestedStaticResource();
bool TestTypeKeyedStaticResourceBasedOn();
bool TestInventoryTemplateApply();
bool TestTutorialSampleXamlLoadApply();

int main() {
    RUN(TestStreamContract);
    RUN(TestPublicNamesAndHierarchy);
    RUN(TestXamlStreamReader);
    RUN(TestProviderOwnershipAndReplacement);
    RUN(TestViewFrameViewportAndInput);
    RUN(TestDispatcherCrossThreadPost);
    RUN(TestContainerLayoutAndCalculators);
    RUN(TestComboBoxAndVisualStateAnimation);
    RUN(TestTransform3DCollapseAndHits);
    RUN(TestViewboxHoverAndPopupFlip);
    RUN(TestViewboxFrameworkElementSpacer);
    RUN(TestStackPanelZIndexDoesNotReorderLayout);
    RUN(TestPanelProgrammaticAddAttachesVisual);
    RUN(TestPanelXamlChildrenStayVisuallyParented);
    RUN(TestExpanderTemplatedParentIsCheckedWritesBack);
    RUN(TestExpanderUnnamedHeaderClickWritesBack);
    RUN(TestExpanderResourceDictionaryHeaderClick);
    RUN(TestControlTemplateHoverStoryboard);
    RUN(TestGridStarRowsSizeInStackPanel);
    RUN(TestBlendTutorialSidebarInteractions);
    RUN(TestLoadComponentUserControlInStackPanel);
    RUN(TestLoadComponentStarGridUserControlInStackPanel);
    RUN(TestMenuTooltipForegroundAndFocus);
    RUN(TestViewboxTransform3DButtonHit);
    RUN(TestKeyboardNavigationIsTabStopTemplateTrigger);
    RUN(TestCheckedVisualStateRevertsArrowOpacity);
    RUN(TestRepeatButtonPressClickAndScaleX);
    RUN(TestComboBoxPopupItemClickSelects);
    RUN(TestComboBoxItemsSourcePopupClickSelects);
    RUN(TestDataTemplateElementNameSelectedItemFilter);
    RUN(TestDataTemplateMultiDataTriggerTeamFilter);
    RUN(TestDataTemplateItemHoverStoryboard);
    RUN(TestGeometryFlatten);
    RUN(TestClosedPathRelativeMove);
    RUN(TestStreamGeometryFlattenCore);
    RUN(TestStreamGeometryContextFlatten);
    RUN(TestTimelineDurationAndKeyTime);
    RUN(TestLoadedIntroStoryboard);
    RUN(TestBoardFlipRotationYStoryboard);
    RUN(TestCollectionViewAndVirtualization);
    RUN(TestTemplateResolveOrder);
    RUN(TestStrokeJoinCapFillRule);
    RUN(TestNotifyPropertyChangedBindLoop);
    RUN(TestBindingExpressionAndLostFocus);
    RUN(TestCustomItemsSourceThunk);
    RUN(TestClrItemsSourceBindingAfterDataContext);
    RUN(TestGalleryXamlSurface);
    RUN(TestGalleryHostXamlSurface);
    RUN(TestTutorialXamlSurface);
    RUN(TestTutorialRuntimePatterns);
    RUN(TestStyleSetterMergedStaticResource);
    RUN(TestMergedNestedStaticResource);
    RUN(TestTypeKeyedStaticResourceBasedOn);
    RUN(TestInventoryTemplateApply);
    RUN(TestTutorialSampleXamlLoadApply);
    std::puts("Aero framework conformance tests passed");
    std::fflush(stdout);
    // LiveGui instances are leaked on purpose (View/~Gui SIGSEGV with mounted
    // content). Skip atexit teardown of those process-lifetime objects.
    std::_Exit(0);
}
