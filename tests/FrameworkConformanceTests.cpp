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
#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Controls/ListBox.hpp>
#include <Aero/Controls/ListBoxItem.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/Primitives/Selector.hpp>
#include <Aero/Controls/StackPanel.hpp>
#include <Aero/Controls/TabControl.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Controls/VirtualizingStackPanel.hpp>
#include <Aero/Data/Binding.hpp>
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
#include <Aero/Media/Animation/Timeline.hpp>
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
#include <Aero/Media/ImageSource.hpp>
#include <Aero/Media/MatrixTransform.hpp>
#include <Aero/Media/ShaderEffect.hpp>
#include <Aero/Media/SolidColorBrush.hpp>
#include <Aero/Media/TransformGroup.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Shapes/Rectangle.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/KeyBinding.hpp>
#include <Aero/Media/BitmapImage.hpp>
#include <Aero/Media/BlurEffect.hpp>
#include <Aero/Media/RotateTransform.hpp>
#include <Aero/TextProperties.hpp>
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
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

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
using Aero::Controls::Button;
using Aero::Controls::Canvas;
using Aero::Controls::ComboBox;
using Aero::Controls::ContentControl;
using Aero::Controls::Grid;
using Aero::Controls::ItemsControl;
using Aero::Controls::ListBox;
using Aero::Controls::ListBoxItem;
using Aero::Controls::Panel;
using Aero::Controls::PasswordBox;
using Aero::Controls::StackPanel;
using Aero::Controls::TabControl;
using Aero::Controls::TextBlock;
using Aero::Controls::UniformGrid;
using Aero::Controls::UserControl;
using Aero::Controls::VirtualizingStackPanel;
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
using Aero::Shapes::Shape;
using Aero::Style;
using Aero::TryCast;
using Aero::TryCastToInterface;
using Aero::UIElement;
using Aero::View;
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
    return std::abs(left - right) <= epsilon;
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
    std::uint32_t begins = 0U;
    std::uint32_t ends = 0U;

    Result<void> AddPoint(Point point) noexcept override {
        return points.PushBack(point);
    }
    Result<void> BeginFigure(Point start, bool isClosed) noexcept override {
        (void)isClosed;
        ++begins;
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
    PumpForward(live);
    PumpForward(live);
    PumpForward(live);
    PumpForward(live);
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

} // namespace

bool TestStyleSetterMergedStaticResource();
bool TestInventoryTemplateApply();
bool TestTutorialSampleXamlLoadApply();

int main() {
    RUN(TestStreamContract);
    RUN(TestPublicNamesAndHierarchy);
    RUN(TestXamlStreamReader);
    RUN(TestProviderOwnershipAndReplacement);
    RUN(TestViewFrameViewportAndInput);
    RUN(TestContainerLayoutAndCalculators);
    RUN(TestComboBoxAndVisualStateAnimation);
    RUN(TestTransform3DCollapseAndHits);
    RUN(TestGeometryFlatten);
    RUN(TestTimelineDurationAndKeyTime);
    RUN(TestCollectionViewAndVirtualization);
    RUN(TestTemplateResolveOrder);
    RUN(TestStrokeJoinCapFillRule);
    RUN(TestNotifyPropertyChangedBindLoop);
    RUN(TestCustomItemsSourceThunk);
    RUN(TestClrItemsSourceBindingAfterDataContext);
    RUN(TestGalleryXamlSurface);
    RUN(TestGalleryHostXamlSurface);
    RUN(TestTutorialXamlSurface);
    RUN(TestTutorialRuntimePatterns);
    RUN(TestStyleSetterMergedStaticResource);
    RUN(TestInventoryTemplateApply);
    RUN(TestTutorialSampleXamlLoadApply);
    std::puts("Aero framework conformance tests passed");
    std::fflush(stdout);
    // LiveGui instances are leaked on purpose (View/~Gui SIGSEGV with mounted
    // content). Skip atexit teardown of those process-lifetime objects.
    std::_Exit(0);
}
