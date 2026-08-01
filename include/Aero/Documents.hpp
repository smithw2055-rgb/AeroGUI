#pragma once

#include <Aero/ContentElement.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Input.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Text/TextTypes.hpp>

#include <cstdint>
#include <utility>

namespace Aero::Detail { class DocumentTextAccess; }

namespace Aero::Documents {

class Inline;
class Span;
class Hyperlink;

// Read-only projection over a TextBlock or Span inline collection.
class AERO_API InlineCollectionView final {
public:
    InlineCollectionView() noexcept = default;
    std::uint32_t Count() const noexcept;
    bool Empty() const noexcept { return Count() == 0U; }
    const Inline* At(std::uint32_t index) const noexcept;

private:
    friend class InlineCollection;
    friend class Span;
    friend class Aero::Controls::TextBlock;
    explicit InlineCollectionView(const Base::Object& owner) noexcept
        : owner_(&owner) {}
    const Base::Object* owner_ = nullptr;
};

// Mutable WPF-shaped collection. The collection itself is the logical child
// store; Inline objects are not Visuals and are rendered by their TextBlock host.
class AERO_API InlineCollection final {
public:
    InlineCollection() noexcept = default;
    std::uint32_t Count() const noexcept;
    bool Empty() const noexcept { return Count() == 0U; }
    Inline* At(std::uint32_t index) const noexcept;
    InlineCollectionView View() const noexcept;
    Base::Result<void> Add(Base::Ref<Inline> value) noexcept;
    Base::Result<bool> Remove(Inline& value) noexcept;
    Base::Result<void> Clear() noexcept;

private:
    friend class Span;
    friend class Aero::Controls::TextBlock;
    explicit InlineCollection(Base::Object& owner) noexcept : owner_(&owner) {}
    Base::Object* owner_ = nullptr;
};

enum class LogicalDirection : std::uint8_t {
    Backward = 0U,
    Forward,
};

// Borrowed text position in a formatted text container. Storage offsets remain
// private so the public contract is independent of the engine's UTF encoding.
class AERO_API TextPointer final {
public:
    TextPointer() noexcept = default;
    bool IsValid() const noexcept { return container_ != nullptr; }
    Controls::TextBlock* GetTextContainer() const noexcept { return container_; }
    LogicalDirection GetLogicalDirection() const noexcept { return direction_; }
    bool IsAtInsertionPosition() const noexcept { return IsValid(); }

    Base::Result<std::int32_t> CompareTo(
        const TextPointer& other) const noexcept;
    Base::Result<TextPointer> GetPositionAtOffset(
        std::int32_t delta,
        LogicalDirection direction = LogicalDirection::Forward) const noexcept;

    friend bool operator==(
        const TextPointer& left,
        const TextPointer& right) noexcept {
        return left.container_ == right.container_ &&
            left.offset_ == right.offset_ &&
            left.direction_ == right.direction_;
    }
    friend bool operator!=(
        const TextPointer& left,
        const TextPointer& right) noexcept {
        return !(left == right);
    }

private:
    friend class Aero::Detail::DocumentTextAccess;
    friend class Aero::Controls::TextBlock;
    friend class TextRange;
    TextPointer(
        Controls::TextBlock& container,
        std::uint32_t offset,
        LogicalDirection direction) noexcept
        : container_(&container), offset_(offset), direction_(direction) {}

    Controls::TextBlock* container_ = nullptr;
    std::uint32_t offset_ = 0U;
    LogicalDirection direction_ = LogicalDirection::Forward;
};

class AERO_API TextRange final {
public:
    TextRange() noexcept = default;
    static Base::Result<TextRange> TryCreate(
        TextPointer start,
        TextPointer end) noexcept;

    bool IsValid() const noexcept {
        return start_.IsValid() && end_.IsValid();
    }
    bool IsEmpty() const noexcept {
        return IsValid() && start_.offset_ == end_.offset_;
    }
    std::uint32_t Length() const noexcept {
        return IsValid() ? end_.offset_ - start_.offset_ : 0U;
    }
    const TextPointer& GetStart() const noexcept { return start_; }
    const TextPointer& GetEnd() const noexcept { return end_; }
    Base::Result<Base::String> GetText() const noexcept;
    Base::Result<void> CopyText(Base::String& output) const noexcept;

private:
    TextRange(TextPointer start, TextPointer end) noexcept
        : start_(start), end_(end) {}
    TextPointer start_;
    TextPointer end_;
};

Base::Result<void> CopyText(
    const Controls::TextBlock& container,
    Base::String& output) noexcept;
Base::Result<TextPointer> GetPositionFromPoint(
    Controls::TextBlock& container,
    Aero::Base::Point point,
    bool snapToText = true) noexcept;
Base::Result<Aero::Base::Rect> GetCharacterRect(
    const TextPointer& position) noexcept;

// Non-visual WPF document content. TextElement values are interpreted by the
// owning TextBlock during formatting and participate in logical/event routing.
class AERO_API TextElement : public FrameworkContentElement {
    AERO_DECLARE_TYPE(TextElement, FrameworkContentElement)
public:
    ~TextElement() override = default;

    Base::StringView GetFontFamily() const noexcept {
        return GetValueOr(FontFamilyProperty, Base::StringView{});
    }
    double GetFontSize() const noexcept {
        return GetValueOr(FontSizeProperty, 16.0);
    }
    FontWeight GetFontWeight() const noexcept {
        return GetValueOr(FontWeightProperty, FontWeight::Normal);
    }
    Text::FontStyle GetFontStyle() const noexcept {
        return GetValueOr(FontStyleProperty, Text::FontStyle::Normal);
    }
    Base::Ref<Media::Brush> GetForeground() const noexcept {
        return GetValueOr(ForegroundProperty, Base::Ref<Media::Brush>{});
    }
    Controls::TextDecorations GetTextDecorations() const noexcept {
        return GetValueOr(
            TextDecorationsProperty,
            Controls::TextDecorations::None);
    }

    Base::Result<void> SetFontFamily(Base::StringView value) noexcept {
        return SetValue(FontFamilyProperty, value);
    }
    Base::Result<void> SetFontSize(double value) noexcept {
        return SetValue(FontSizeProperty, value);
    }
    Base::Result<void> SetFontWeight(FontWeight value) noexcept {
        return SetValue(FontWeightProperty, value);
    }
    Base::Result<void> SetFontStyle(Text::FontStyle value) noexcept {
        return SetValue(FontStyleProperty, value);
    }
    Base::Result<void> SetForeground(Base::Ref<Media::Brush> value) noexcept {
        return SetValue(ForegroundProperty, std::move(value));
    }
    Base::Result<void> SetTextDecorations(
        Controls::TextDecorations value) noexcept {
        return SetValue(TextDecorationsProperty, value);
    }

    inline static constexpr Members::Property<Base::String> FontFamilyProperty{"FontFamily"};
    inline static constexpr Members::Property<double> FontSizeProperty{"FontSize"};
    inline static constexpr Members::Property<FontWeight> FontWeightProperty{"FontWeight"};
    inline static constexpr Members::Property<Text::FontStyle> FontStyleProperty{"FontStyle"};
    inline static constexpr Members::Property<Base::Ref<Media::Brush>> ForegroundProperty{"Foreground"};
    inline static constexpr Members::Property<Controls::TextDecorations> TextDecorationsProperty{"TextDecorations"};

protected:
    explicit TextElement(Core::TypeId runtimeType) noexcept
        : FrameworkContentElement(runtimeType) {}
};

class AERO_API Inline : public TextElement {
    AERO_DECLARE_TYPE(Inline, TextElement)
public:
    ~Inline() override = default;

protected:
    explicit Inline(Core::TypeId runtimeType) noexcept
        : TextElement(runtimeType) {}
};

class AERO_API Run final : public Inline {
    AERO_DECLARE_TYPE(Run, Inline)
public:
    Run() noexcept : Inline(StaticTypeId()) {}
    ~Run() override = default;

    Base::StringView GetText() const noexcept {
        return GetValueOr(TextProperty, Base::StringView{});
    }
    Base::StringView Content() const noexcept { return GetText(); }
    Base::Result<void> SetText(Base::StringView value) noexcept {
        return SetValue(TextProperty, value);
    }
    Base::Result<void> SetContent(Base::StringView value) noexcept {
        return SetText(value);
    }

    inline static constexpr Members::Property<Base::String> TextProperty{"Text"};
};

class AERO_API Span : public Inline {
    AERO_DECLARE_TYPE(Span, Inline)
public:
    Span() noexcept : Span(StaticTypeId()) {}
    ~Span() override = default;

    InlineCollection GetInlines() noexcept { return InlineCollection(*this); }
    InlineCollectionView GetInlines() const noexcept {
        return InlineCollectionView(*this);
    }
    Core::Value MetadataInlines() const noexcept;
    Base::Result<void> SetInlineValue(Core::Value value) noexcept;
    Base::Result<void> AddOwnedInline(Base::Ref<Inline> value) noexcept;
    Base::Result<void> ClearOwnedInlines() noexcept;

protected:
    explicit Span(Core::TypeId runtimeType) noexcept
        : Inline(runtimeType), inlines_() {}
    std::uint32_t GetLogicalChildrenCount() const noexcept override {
        return inlines_.Size();
    }
    DependencyObject* GetLogicalChild(std::uint32_t index) const noexcept override {
        return index < inlines_.Size() ? inlines_[index].Get() : nullptr;
    }

private:
    friend class Aero::Detail::DocumentTextAccess;
    Base::Vector<Base::Ref<Inline>> inlines_;
    Base::Ref<Inline> pendingInline_;
};

class AERO_API Bold final : public Span {
    AERO_DECLARE_TYPE(Bold, Span)
public:
    Bold() noexcept : Span(StaticTypeId()) {}
    ~Bold() override = default;
};

class AERO_API Italic final : public Span {
    AERO_DECLARE_TYPE(Italic, Span)
public:
    Italic() noexcept : Span(StaticTypeId()) {}
    ~Italic() override = default;
};

class AERO_API Underline final : public Span {
    AERO_DECLARE_TYPE(Underline, Span)
public:
    Underline() noexcept : Span(StaticTypeId()) {}
    ~Underline() override = default;
};

class AERO_API LineBreak final : public Inline {
    AERO_DECLARE_TYPE(LineBreak, Inline)
public:
    LineBreak() noexcept : Inline(StaticTypeId()) {}
    ~LineBreak() override = default;
};

struct RequestNavigateEventArgs final : Aero::RoutedEventArgs {
    AERO_DECLARE_TYPE(RequestNavigateEventArgs, Aero::RoutedEventArgs)
public:
    RequestNavigateEventArgs() noexcept
        : Aero::RoutedEventArgs(StaticTypeId()) {}
    RequestNavigateEventArgs(
        Base::StringView uri,
        Hyperlink* hyperlink) noexcept
        : Aero::RoutedEventArgs(StaticTypeId()),
          uri_(uri), hyperlink_(hyperlink) {}

    Base::StringView GetUri() const noexcept { return uri_; }
    Hyperlink* GetHyperlink() const noexcept { return hyperlink_; }

private:
    Base::StringView uri_;
    Hyperlink* hyperlink_ = nullptr;
};
using RequestNavigateEventHandler = Base::Delegate<void(
    Base::Object*, RequestNavigateEventArgs&)>;

class AERO_API Hyperlink final : public Span {
    AERO_DECLARE_TYPE(Hyperlink, Span)
public:
    Hyperlink() noexcept : Span(StaticTypeId()) {}
    ~Hyperlink() override = default;

    inline static constexpr Members::RoutedEvent<Aero::RoutedEventArgs> ClickEvent{"Click"};
    ContentElement::Event<Aero::RoutedEventArgs> Click() noexcept {
        return GetEvent(ClickEvent);
    }
    inline static constexpr Members::RoutedEvent<RequestNavigateEventArgs> RequestNavigateEvent{"RequestNavigate"};
    ContentElement::Event<RequestNavigateEventArgs> RequestNavigate() noexcept {
        return GetEvent(RequestNavigateEvent);
    }

    Base::StringView NavigateUri() const noexcept;
    Aero::Input::ICommand* GetCommand() const noexcept;
    Base::Ref<Base::Object> CommandParameter() const noexcept;
    Aero::UIElement* CommandTarget() const noexcept;

    Base::Result<void> SetNavigateUri(Base::StringView value) noexcept;
    Base::Result<void> SetCommand(
        Base::Ref<Aero::Input::ICommand> command) noexcept;
    Base::Result<void> SetCommandParameter(
        Base::Ref<Base::Object> parameter) noexcept;
    Base::Result<void> SetCommandTarget(
        Base::Ref<Aero::UIElement> target) noexcept;

    inline static constexpr Members::Property<Base::String> NavigateUriProperty{"NavigateUri"};
    inline static constexpr Members::Property<Base::Ref<Aero::Input::ICommand>> CommandProperty{"Command"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> CommandParameterProperty{"CommandParameter"};
    inline static constexpr Members::Property<Base::Ref<Aero::UIElement>> CommandTargetProperty{"CommandTarget"};
};

using NavigationHandler = Base::Delegate<bool(
    Base::StringView, Hyperlink&)>;

class AERO_API NavigationService final {
public:
    explicit NavigationService(
        NavigationHandler handler = {}) noexcept;
    NavigationService(const NavigationService&) = delete;
    NavigationService& operator=(const NavigationService&) = delete;
    ~NavigationService() noexcept;

    void SetHandler(NavigationHandler handler) noexcept {
        handler_ = std::move(handler);
    }
    Base::Result<void> Attach(Aero::UIElement& root) noexcept;
    bool Detach() noexcept;
    bool IsAttached() const noexcept { return root_ != nullptr; }

private:
    void OnRequestNavigate(
        Base::Object* sender,
        RequestNavigateEventArgs& args) noexcept;

    NavigationHandler handler_;
    Aero::UIElement* root_ = nullptr;
    RequestNavigateEventHandler requestHandler_;
};

} // namespace Aero::Documents
