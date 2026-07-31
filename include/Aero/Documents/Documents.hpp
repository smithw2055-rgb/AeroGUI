#pragma once

#include <Aero/Controls/Controls.hpp>
#include <Aero/Presentation/Commands.hpp>

#include <cstdint>
#include <utility>

namespace Aero::Detail { class DocumentTextAccess; }

namespace Aero::Documents {

class Inline;
class Hyperlink;

// Read-only projection over the canonical TextBlock owned-inline store.
class AERO_API InlineCollectionView final {
public:
    InlineCollectionView() noexcept = default;
    std::uint32_t Count() const noexcept;
    bool Empty() const noexcept { return Count() == 0U; }
    const Inline* At(std::uint32_t index) const noexcept;

private:
    friend class InlineCollection;
    friend class Aero::Controls::TextBlock;
    explicit InlineCollectionView(
        const Controls::TextBlock& owner) noexcept : owner_(&owner) {}
    const Controls::TextBlock* owner_ = nullptr;
};

// Mutable WPF-shaped collection. Structural mutation is accepted while the
// owner is detached; mounted edits remain the responsibility of MountService.
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
    friend class Aero::Controls::TextBlock;
    explicit InlineCollection(
        Controls::TextBlock& owner) noexcept : owner_(&owner) {}
    Controls::TextBlock* owner_ = nullptr;
};

enum class LogicalDirection : std::uint8_t {
    Backward = 0U,
    Forward,
};

// Borrowed text position bound to a root TextBlock. Offsets are UTF-8 byte
// offsets in the flattened inline document and match shaping cluster offsets.
class AERO_API TextPointer final {
public:
    TextPointer() noexcept = default;
    bool IsValid() const noexcept { return container_ != nullptr; }
    Controls::TextBlock* Container() const noexcept { return container_; }
    std::uint32_t Offset() const noexcept { return offset_; }
    LogicalDirection Direction() const noexcept { return direction_; }

    Base::Result<std::int32_t> CompareTo(
        const TextPointer& other) const noexcept;
    Base::Result<TextPointer> GetPositionAtOffset(
        std::int32_t delta,
        LogicalDirection direction = LogicalDirection::Forward) const noexcept;

    friend bool operator==(const TextPointer& left,
                           const TextPointer& right) noexcept {
        return left.container_ == right.container_ &&
            left.offset_ == right.offset_ &&
            left.direction_ == right.direction_;
    }
    friend bool operator!=(const TextPointer& left,
                           const TextPointer& right) noexcept {
        return !(left == right);
    }

private:
    friend class Aero::Detail::DocumentTextAccess;
    friend class Aero::Controls::TextBlock;
    TextPointer(Controls::TextBlock& container,
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
        TextPointer start, TextPointer end) noexcept;

    bool IsValid() const noexcept {
        return start_.IsValid() && end_.IsValid();
    }
    bool IsEmpty() const noexcept {
        return IsValid() && start_.Offset() == end_.Offset();
    }
    std::uint32_t Length() const noexcept {
        return IsValid() ? end_.Offset() - start_.Offset() : 0U;
    }
    const TextPointer& Start() const noexcept { return start_; }
    const TextPointer& End() const noexcept { return end_; }
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
    Presentation::Point point,
    bool snapToText = true) noexcept;
Base::Result<Presentation::Rect> GetCharacterRect(
    const TextPointer& position) noexcept;

// WPF-shaped document content surface. The implementation deliberately reuses
// TextBlock as the retained visual/text-layout carrier; public document semantics
// are expressed through this hierarchy rather than a second text engine.
class AERO_API TextElement : public Controls::TextBlock {
    AERO_DECLARE_TYPE(TextElement, Controls::TextBlock)
public:
    ~TextElement() override = default;

    inline static constexpr auto FontWeightProperty =
        Controls::TextBlock::FontWeightProperty;
    inline static constexpr auto ForegroundProperty =
        Controls::TextBlock::ForegroundProperty;
    inline static constexpr auto FontSizeProperty =
        Controls::TextBlock::FontSizeProperty;

protected:
    explicit TextElement(Core::TypeId runtimeType) noexcept
        : Controls::TextBlock(runtimeType) {}
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

    Base::StringView Content() const noexcept { return Text(); }
    Base::Result<void> SetContent(Base::StringView value) noexcept {
        return SetText(value);
    }
};

class AERO_API Span : public Inline {
    AERO_DECLARE_TYPE(Span, Inline)
public:
    Span() noexcept : Span(StaticTypeId()) {}
    ~Span() override = default;

    Base::Result<void> TryAddInline(Base::Ref<Inline> value) noexcept {
        return Inlines().Add(std::move(value));
    }
    Base::Result<void> ClearInlines() noexcept {
        return Inlines().Clear();
    }

protected:
    explicit Span(Core::TypeId runtimeType) noexcept
        : Inline(runtimeType) {}
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

protected:
    Base::Result<Presentation::Size> MeasureOverride(
        Presentation::Size) noexcept override {
        return Presentation::Size{};
    }
};

struct RequestNavigateEventArgs final : Presentation::RoutedEventArgs {
    AERO_DECLARE_TYPE(RequestNavigateEventArgs, Presentation::RoutedEventArgs)
    RequestNavigateEventArgs() noexcept
        : Presentation::RoutedEventArgs(StaticTypeId()) {}
    RequestNavigateEventArgs(
        Base::StringView value, Hyperlink* sourceLink) noexcept
        : Presentation::RoutedEventArgs(StaticTypeId()),
          uri(value), hyperlink(sourceLink) {}

    Base::StringView uri;
    Hyperlink* hyperlink = nullptr;
};
using RequestNavigateEventHandler = Base::Delegate<void(
    Base::Object*, const RequestNavigateEventArgs&)>;

class AERO_API Hyperlink final : public Span {
    AERO_DECLARE_TYPE(Hyperlink, Span)
public:
    Hyperlink() noexcept : Span(StaticTypeId()) {}
    ~Hyperlink() override = default;

    inline static constexpr Members::RoutedEvent<
        Presentation::RoutedEventArgs> ClickEvent{"Click"};
    Presentation::UIElement::RoutedEvent_<
        Presentation::RoutedEventHandler> Click() noexcept {
        return Event(ClickEvent);
    }
    inline static constexpr Members::RoutedEvent<
        RequestNavigateEventArgs> RequestNavigateEvent{"RequestNavigate"};
    Presentation::UIElement::RoutedEvent_<
        RequestNavigateEventHandler> RequestNavigate() noexcept {
        return Event(RequestNavigateEvent);
    }

    Base::StringView NavigateUri() const noexcept;
    Presentation::ICommand* Command() const noexcept;
    Base::Ref<Base::Object> CommandParameter() const noexcept;
    Presentation::UIElement* CommandTarget() const noexcept;

    Base::Result<void> SetNavigateUri(Base::StringView value) noexcept;
    Base::Result<void> SetCommand(
        Base::Ref<Presentation::ICommand> command) noexcept;
    Base::Result<void> SetCommandParameter(
        Base::Ref<Base::Object> parameter) noexcept;
    Base::Result<void> SetCommandTarget(
        Base::Ref<Presentation::UIElement> target) noexcept;

    inline static constexpr Members::Property<Base::String>
        NavigateUriProperty{"NavigateUri"};
    inline static constexpr Members::Property<
        Base::Ref<Presentation::ICommand>> CommandProperty{"Command"};
    inline static constexpr Members::Property<
        Base::Ref<Base::Object>> CommandParameterProperty{"CommandParameter"};
    inline static constexpr Members::Property<
        Base::Ref<Presentation::UIElement>> CommandTargetProperty{"CommandTarget"};
};

using NavigationHandler = Base::Delegate<bool(
    Base::StringView, Hyperlink&)>;

// Host-facing adapter for RequestNavigate. It owns no browser or OS policy;
// applications provide the callback and attach the service to a routed-event root.
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
    Base::Result<void> Attach(
        Presentation::UIElement& root) noexcept;
    bool Detach() noexcept;
    bool IsAttached() const noexcept { return root_ != nullptr; }

private:
    void OnRequestNavigate(
        Base::Object* sender,
        const RequestNavigateEventArgs& args) noexcept;

    NavigationHandler handler_;
    Presentation::UIElement* root_ = nullptr;
    RequestNavigateEventHandler requestHandler_;
};

} // namespace Aero::Documents
