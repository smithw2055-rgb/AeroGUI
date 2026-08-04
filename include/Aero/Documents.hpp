#pragma once

#include <Aero/ContentElement.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Input.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Events/NavigationEventArgs.hpp>

#include <cstdint>
#include <utility>

namespace Aero::Documents {

class Inline;
class Span;
class Hyperlink;
class TextRange;

// Read-only projection over a TextBlock or Span inline collection.
class AERO_API InlineCollectionView {
public:
    InlineCollectionView() noexcept = default;
    std::uint32_t GetCount() const noexcept;
    bool GetIsEmpty() const noexcept { return GetCount() == 0U; }
    const Inline* GetItem(std::uint32_t index) const noexcept;

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
class AERO_API InlineCollection {
public:
    InlineCollection() noexcept = default;
    std::uint32_t GetCount() const noexcept;
    bool GetIsEmpty() const noexcept { return GetCount() == 0U; }
    Inline* GetItem(std::uint32_t index) const noexcept;
    InlineCollectionView GetView() const noexcept;
    Base::Result<void> Add(Base::Ref<Inline> value) noexcept;
    Base::Result<bool> Remove(Inline& value) noexcept;
    void Clear() noexcept;

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
class AERO_API TextPointer {
public:
    TextPointer() noexcept = default;
    bool GetIsValid() const noexcept { return container_ != nullptr; }
    Controls::TextBlock* GetTextContainer() const noexcept { return container_; }
    LogicalDirection GetLogicalDirection() const noexcept { return direction_; }
    bool GetIsAtInsertionPosition() const noexcept { return GetIsValid(); }

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
    friend struct ::Aero::Controls::TextBlock::Impl;
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

class AERO_API TextRange {
public:
    TextRange() noexcept = default;
    static Base::Result<TextRange> Create(
        TextPointer start,
        TextPointer end) noexcept;

    bool GetIsValid() const noexcept {
        return start_.GetIsValid() && end_.GetIsValid();
    }
    bool GetIsEmpty() const noexcept {
        return GetIsValid() && start_.offset_ == end_.offset_;
    }
    std::uint32_t GetLength() const noexcept {
        return GetIsValid() ? end_.offset_ - start_.offset_ : 0U;
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

    Base::Ref<Media::FontFamily> GetFontFamily() const noexcept {
        return GetValueOr(
            FontFamilyProperty, Base::Ref<Media::FontFamily>{});
    }
    double GetFontSize() const noexcept {
        return GetValueOr(FontSizeProperty, 16.0);
    }
    FontWeight GetFontWeight() const noexcept {
        return GetValueOr(FontWeightProperty, FontWeight::Normal);
    }
    FontStyle GetFontStyle() const noexcept {
        return GetValueOr(FontStyleProperty, FontStyle::Normal);
    }
    Base::Ref<Media::Brush> GetForeground() const noexcept {
        return GetValueOr(ForegroundProperty, Base::Ref<Media::Brush>{});
    }
    Controls::TextDecorations GetTextDecorations() const noexcept {
        return GetValueOr(
            TextDecorationsProperty,
            Controls::TextDecorations::None);
    }

    void SetFontFamily(Base::Ref<Media::FontFamily> value) noexcept {
        SetValue(FontFamilyProperty, std::move(value));
    }
    Base::Result<void> SetFontFamily(Base::StringView value) noexcept {
        Base::Result<Base::Ref<Media::FontFamily>> family =
            Base::MakeRef<Media::FontFamily>();
        if (!family) return family.GetStatus();
        family.Value()->SetSource(value);
        SetFontFamily(std::move(family).Value());
        return {};
    }
    void SetFontSize(double value) noexcept {
        SetValue(FontSizeProperty, value);
    }
    void SetFontWeight(FontWeight value) noexcept {
        SetValue(FontWeightProperty, value);
    }
    void SetFontStyle(FontStyle value) noexcept {
        SetValue(FontStyleProperty, value);
    }
    void SetForeground(Base::Ref<Media::Brush> value) noexcept {
        SetValue(ForegroundProperty, std::move(value));
    }
    void SetTextDecorations(
        Controls::TextDecorations value) noexcept {
        SetValue(TextDecorationsProperty, value);
    }

    inline static constexpr Members::Property<Base::Ref<Media::FontFamily>> FontFamilyProperty{"FontFamily"};
    inline static constexpr Members::Property<double> FontSizeProperty{"FontSize"};
    inline static constexpr Members::Property<FontWeight> FontWeightProperty{"FontWeight"};
    inline static constexpr Members::Property<FontStyle> FontStyleProperty{"FontStyle"};
    inline static constexpr Members::Property<Base::Ref<Media::Brush>> ForegroundProperty{"Foreground"};
    inline static constexpr Members::Property<Controls::TextDecorations> TextDecorationsProperty{"TextDecorations"};

protected:
    explicit TextElement(Meta::TypeId runtimeType) noexcept
        : FrameworkContentElement(runtimeType) {}
};

class AERO_API Inline : public TextElement {
    AERO_DECLARE_TYPE(Inline, TextElement)
public:
    ~Inline() override = default;

protected:
    explicit Inline(Meta::TypeId runtimeType) noexcept
        : TextElement(runtimeType) {}
};

class AERO_API Run : public Inline {
    AERO_DECLARE_TYPE(Run, Inline)
public:
    Run() noexcept : Inline(StaticTypeId()) {}
    ~Run() override = default;

    Base::StringView GetText() const noexcept {
        return GetValueOr(TextProperty, Base::StringView{});
    }
    Base::StringView GetContent() const noexcept { return GetText(); }
    void SetText(Base::StringView value) noexcept {
        SetValue(TextProperty, value);
    }
    void SetContent(Base::StringView value) noexcept {
        SetText(value);
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
    Value GetMetadataInlines() const noexcept;
    void SetInlineValue(Value value) noexcept;
    Base::Result<void> AddOwnedInline(Base::Ref<Inline> value) noexcept;
    void ClearOwnedInlines() noexcept;

protected:
    explicit Span(Meta::TypeId runtimeType) noexcept
        : Inline(runtimeType), inlines_() {}
    std::uint32_t GetLogicalChildrenCount() const noexcept override {
        return inlines_.Size();
    }
    DependencyObject* GetLogicalChild(std::uint32_t index) const noexcept override {
        return index < inlines_.Size() ? inlines_[index].Get() : nullptr;
    }

private:
    friend struct ::Aero::Controls::TextBlock::Impl;
    Base::Vector<Base::Ref<Inline>> inlines_;
    Base::Ref<Inline> pendingInline_;
};

class AERO_API Bold : public Span {
    AERO_DECLARE_TYPE(Bold, Span)
public:
    Bold() noexcept : Span(StaticTypeId()) {}
    ~Bold() override = default;
};

class AERO_API Italic : public Span {
    AERO_DECLARE_TYPE(Italic, Span)
public:
    Italic() noexcept : Span(StaticTypeId()) {}
    ~Italic() override = default;
};

class AERO_API Underline : public Span {
    AERO_DECLARE_TYPE(Underline, Span)
public:
    Underline() noexcept : Span(StaticTypeId()) {}
    ~Underline() override = default;
};

class AERO_API LineBreak : public Inline {
    AERO_DECLARE_TYPE(LineBreak, Inline)
public:
    LineBreak() noexcept : Inline(StaticTypeId()) {}
    ~LineBreak() override = default;
};

class AERO_API Hyperlink : public Span {
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

    Base::StringView GetNavigateUri() const noexcept;
    Aero::Input::ICommand* GetCommand() const noexcept;
    Value GetCommandParameter() const noexcept;
    Aero::UIElement* GetCommandTarget() const noexcept;

    void SetNavigateUri(Base::StringView value) noexcept;
    void SetCommand(
        Base::Ref<Aero::Input::ICommand> command) noexcept;
    void SetCommandParameter(Value parameter) noexcept;
    void SetCommandTarget(
        Base::Ref<Aero::UIElement> target) noexcept;

    inline static constexpr Members::Property<Base::String> NavigateUriProperty{"NavigateUri"};
    inline static constexpr Members::Property<Base::Ref<Aero::Input::ICommand>> CommandProperty{"Command"};
    inline static constexpr Members::Property<Value> CommandParameterProperty{"CommandParameter"};
    inline static constexpr Members::Property<Base::Ref<Aero::UIElement>> CommandTargetProperty{"CommandTarget"};
};

using NavigationHandler = Base::Delegate<bool(
    Base::StringView, Hyperlink&)>;

class AERO_API NavigationService {
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
    bool GetIsAttached() const noexcept { return root_ != nullptr; }

private:
    void OnRequestNavigate(
        Base::Object* sender,
        RequestNavigateEventArgs& args) noexcept;

    NavigationHandler handler_;
    Aero::UIElement* root_ = nullptr;
    RequestNavigateEventHandler requestHandler_;
};

} // namespace Aero::Documents
