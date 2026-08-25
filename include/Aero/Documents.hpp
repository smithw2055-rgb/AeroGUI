#pragma once

#include <Aero/FrameworkContentElement.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Input.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Events/NavigationEventArgs.hpp>

#include <cstdint>
#include <utility>

namespace Aero::Controls { struct TextBlockDocumentHelper; }

namespace Aero::Documents {

class Inline;
class Span;
class Hyperlink;
class TextRange;

// Read-only projection over a TextBlock or Span inline collection.
class AERO_GUI_API InlineCollectionView {
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
class AERO_GUI_API InlineCollection {
public:
    InlineCollection() noexcept = default;
    std::uint32_t GetCount() const noexcept;
    bool GetIsEmpty() const noexcept { return GetCount() == 0U; }
    Inline* GetItem(std::uint32_t index) const noexcept;
    InlineCollectionView GetView() const noexcept;
    Result<void> Add(Ref<Inline> value) noexcept;
    Result<bool> Remove(Inline& value) noexcept;
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
class AERO_GUI_API TextPointer {
public:
    TextPointer() noexcept = default;
    bool GetIsValid() const noexcept { return container_ != nullptr; }
    Controls::TextBlock* GetTextContainer() const noexcept { return container_; }
    LogicalDirection GetLogicalDirection() const noexcept { return direction_; }
    bool GetIsAtInsertionPosition() const noexcept { return GetIsValid(); }

    Result<std::int32_t> CompareTo(
        const TextPointer& other) const noexcept;
    Result<TextPointer> GetPositionAtOffset(
        std::int32_t delta,
        LogicalDirection direction) const noexcept;
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
    friend class Aero::Controls::TextBlock;
    friend class TextRange;
#if defined(AERO_GUI_IMPLEMENTATION)
    friend struct Aero::Controls::TextBlockDocumentHelper;
#endif
    TextPointer(
        Controls::TextBlock& container,
        std::uint32_t offset,
        LogicalDirection direction) noexcept
        : container_(&container), offset_(offset), direction_(direction) {}

    Controls::TextBlock* container_ = nullptr;
    std::uint32_t offset_ = 0U;
    LogicalDirection direction_ = LogicalDirection::Forward;
};

class AERO_GUI_API TextRange {
public:
    TextRange() noexcept = default;
    static Result<TextRange> Create(
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
    Result<String> GetText() const noexcept;
    Result<void> CopyText(String& output) const noexcept;

private:
    TextRange(TextPointer start, TextPointer end) noexcept
        : start_(start), end_(end) {}
    TextPointer start_;
    TextPointer end_;
};

Result<void> CopyText(
    const Controls::TextBlock& container,
    String& output) noexcept;
Result<TextPointer> GetPositionFromPoint(
    Controls::TextBlock& container,
    Aero::Base::Point point,
    bool snapToText = true) noexcept;
Result<Aero::Base::Rect> GetCharacterRect(
    const TextPointer& position) noexcept;

// Non-visual WPF document content. TextElement values are interpreted by the
// owning TextBlock during formatting and participate in logical/event routing.
class AERO_GUI_API TextElement : public FrameworkContentElement {
    AERO_DECLARE_TYPE(TextElement, FrameworkContentElement)
public:
    ~TextElement() override = default;

    Ref<Media::FontFamily> GetFontFamily() const noexcept {
        return GetValueOr(
            FontFamilyProperty, Ref<Media::FontFamily>{});
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
    Ref<Media::Brush> GetForeground() const noexcept {
        return GetValueOr(ForegroundProperty, Ref<Media::Brush>{});
    }
    Controls::TextDecorations GetTextDecorations() const noexcept {
        return GetValueOr(
            TextDecorationsProperty,
            Controls::TextDecorations::None);
    }

    void SetFontFamily(Ref<Media::FontFamily> value) noexcept {
        SetValue(FontFamilyProperty, std::move(value));
    }
    Result<void> SetFontFamily(StringView value) noexcept {
        Result<Ref<Media::FontFamily>> family =
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
    void SetForeground(Ref<Media::Brush> value) noexcept {
        SetValue(ForegroundProperty, std::move(value));
    }
    void SetTextDecorations(
        Controls::TextDecorations value) noexcept {
        SetValue(TextDecorationsProperty, value);
    }

    inline static constexpr AttachedProperty<Ref<Media::FontFamily>> FontFamilyProperty{"FontFamily"};
    inline static constexpr AttachedProperty<double> FontSizeProperty{"FontSize"};
    inline static constexpr AttachedProperty<FontWeight> FontWeightProperty{"FontWeight"};
    inline static constexpr AttachedProperty<FontStyle> FontStyleProperty{"FontStyle"};
    inline static constexpr AttachedProperty<Ref<Media::Brush>> ForegroundProperty{"Foreground"};
    inline static constexpr AttachedProperty<Controls::TextDecorations> TextDecorationsProperty{"TextDecorations"};

protected:
    explicit TextElement(Meta::TypeId runtimeType) noexcept
        : FrameworkContentElement(runtimeType) {}
};

class AERO_GUI_API Inline : public TextElement {
    AERO_DECLARE_TYPE(Inline, TextElement)
public:
    ~Inline() override = default;

protected:
    explicit Inline(Meta::TypeId runtimeType) noexcept
        : TextElement(runtimeType) {}
};

class AERO_GUI_API Run : public Inline {
    AERO_DECLARE_TYPE(Run, Inline)
public:
    Run() noexcept : Inline(StaticTypeId()) {}
    ~Run() override = default;

    StringView GetText() const noexcept {
        return GetValueOr(TextProperty, StringView{});
    }
    StringView GetContent() const noexcept { return GetText(); }
    void SetText(StringView value) noexcept {
        SetValue(TextProperty, value);
    }
    void SetContent(StringView value) noexcept {
        SetText(value);
    }

    inline static constexpr DependencyProperty<String> TextProperty{"Text"};
};

class AERO_GUI_API Span : public Inline {
    AERO_DECLARE_TYPE(Span, Inline)
public:
    Span() noexcept : Span(StaticTypeId()) {}
    ~Span() override;

    InlineCollection GetInlines() noexcept { return InlineCollection(*this); }
    InlineCollectionView GetInlines() const noexcept {
        return InlineCollectionView(*this);
    }
    Value GetMetadataInlines() const noexcept;
    void SetInlineValue(Value value) noexcept;
    Result<void> AddOwnedInline(Ref<Inline> value) noexcept;
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
    friend class Aero::Controls::TextBlock;
    friend class ::Aero::Core::TextLayoutFacet;
#if defined(AERO_GUI_IMPLEMENTATION)
    friend struct Aero::Controls::TextBlockDocumentHelper;
#endif
    Base::Vector<Ref<Inline>> inlines_;
    Ref<Inline> pendingInline_;
};

class AERO_GUI_API Bold : public Span {
    AERO_DECLARE_TYPE(Bold, Span)
public:
    Bold() noexcept : Span(StaticTypeId()) {}
    ~Bold() override = default;
};

class AERO_GUI_API Italic : public Span {
    AERO_DECLARE_TYPE(Italic, Span)
public:
    Italic() noexcept : Span(StaticTypeId()) {}
    ~Italic() override = default;
};

class AERO_GUI_API Underline : public Span {
    AERO_DECLARE_TYPE(Underline, Span)
public:
    Underline() noexcept : Span(StaticTypeId()) {}
    ~Underline() override = default;
};

class AERO_GUI_API LineBreak : public Inline {
    AERO_DECLARE_TYPE(LineBreak, Inline)
public:
    LineBreak() noexcept : Inline(StaticTypeId()) {}
    ~LineBreak() override = default;
};

class AERO_GUI_API Hyperlink : public Span {
    AERO_DECLARE_TYPE(Hyperlink, Span)
public:
    Hyperlink() noexcept : Span(StaticTypeId()) {}
    ~Hyperlink() override = default;

    inline static constexpr RoutedEvent<Aero::RoutedEventArgs> ClickEvent{"Click"};
    ContentElement::Event<Aero::RoutedEventArgs> Click() noexcept {
        return GetEvent(ClickEvent);
    }
    inline static constexpr RoutedEvent<RequestNavigateEventArgs> RequestNavigateEvent{"RequestNavigate"};
    ContentElement::Event<RequestNavigateEventArgs> RequestNavigate() noexcept {
        return GetEvent(RequestNavigateEvent);
    }

    StringView GetNavigateUri() const noexcept;
    Aero::Input::ICommand* GetCommand() const noexcept;
    Value GetCommandParameter() const noexcept;
    Aero::UIElement* GetCommandTarget() const noexcept;

    void SetNavigateUri(StringView value) noexcept;
    void SetCommand(
        Ref<Aero::Input::ICommand> command) noexcept;
    void SetCommandParameter(Value parameter) noexcept;
    void SetCommandTarget(
        Ref<Aero::UIElement> target) noexcept;

    inline static constexpr DependencyProperty<String> NavigateUriProperty{"NavigateUri"};
    inline static constexpr DependencyProperty<Ref<Aero::Input::ICommand>> CommandProperty{"Command"};
    inline static constexpr DependencyProperty<Value> CommandParameterProperty{"CommandParameter"};
    inline static constexpr DependencyProperty<Ref<Aero::UIElement>> CommandTargetProperty{"CommandTarget"};
};

using NavigationHandler = Base::Delegate<bool(
    StringView, Hyperlink&)>;

class AERO_GUI_API NavigationService {
public:
    explicit NavigationService(
        NavigationHandler handler = {}) noexcept;
    NavigationService(const NavigationService&) = delete;
    NavigationService& operator=(const NavigationService&) = delete;
    ~NavigationService() noexcept;

    void SetHandler(NavigationHandler handler) noexcept {
        handler_ = std::move(handler);
    }
    Result<void> Attach(Aero::UIElement& root) noexcept;
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
