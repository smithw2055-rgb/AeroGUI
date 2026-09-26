#pragma once

// Inline family: TextElement through the concrete inlines.
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/TextFormatting.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/Events/NavigationEventArgs.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>

#include <cstdint>

namespace Aero::Controls { class TextBlock; }
namespace Aero::Controls { struct TextBlockDocumentHelper; }

namespace Aero::Documents {

class AERO_GUI_API TextElement : public FrameworkContentElement {
    AERO_DECLARE_TYPE(TextElement, FrameworkContentElement)
public:
    ~TextElement() override = default;

    Ref<Media::FontFamily> GetFontFamily() const noexcept { return GetValue(FontFamilyProperty); }
    double GetFontSize() const noexcept { return GetValue(FontSizeProperty); }
    FontWeight GetFontWeight() const noexcept { return GetValue(FontWeightProperty); }
    FontStyle GetFontStyle() const noexcept { return GetValue(FontStyleProperty); }
    Ref<Media::Brush> GetForeground() const noexcept { return GetValue(ForegroundProperty); }
    Controls::TextDecorations GetTextDecorations() const noexcept { return GetValue(TextDecorationsProperty); }

    void SetFontFamily(Ref<Media::FontFamily> value) noexcept { SetValue(FontFamilyProperty, std::move(value)); }
    void SetFontFamily(StringView value) noexcept {
        Result<Ref<Media::FontFamily>> family = Base::MakeRef<Media::FontFamily>();
        if (!family) { AERO_ASSERT(false); return; }
        family.Value()->SetSource(value);
        SetFontFamily(std::move(family).Value());
    }
    void SetFontSize(double value) noexcept { SetValue(FontSizeProperty, value); }
    void SetFontWeight(FontWeight value) noexcept { SetValue(FontWeightProperty, value); }
    void SetFontStyle(FontStyle value) noexcept { SetValue(FontStyleProperty, value); }
    void SetForeground(Ref<Media::Brush> value) noexcept { SetValue(ForegroundProperty, std::move(value)); }
    void SetTextDecorations(Controls::TextDecorations value) noexcept { SetValue(TextDecorationsProperty, value); }

    AERO_ATTACHED_PROPERTY(Ref<Media::FontFamily>, FontFamily);
    AERO_ATTACHED_PROPERTY(double, FontSize);
    AERO_ATTACHED_PROPERTY(FontWeight, FontWeight);
    AERO_ATTACHED_PROPERTY(FontStyle, FontStyle);
    AERO_ATTACHED_PROPERTY(Ref<Media::Brush>, Foreground);
    AERO_ATTACHED_PROPERTY(Controls::TextDecorations, TextDecorations);

protected:
    explicit TextElement(Meta::TypeId runtimeType) noexcept : FrameworkContentElement(runtimeType) {}
};

class AERO_GUI_API Inline : public TextElement {
    AERO_DECLARE_TYPE(Inline, TextElement)
public:
    ~Inline() override = default;

protected:
    explicit Inline(Meta::TypeId runtimeType) noexcept : TextElement(runtimeType) {}
};

class Inline;
class Span;
class InlineCollection;

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
    explicit InlineCollectionView(const Base::Object& owner) noexcept : owner_(&owner) {}
    const Base::Object* owner_ = nullptr;
};

class AERO_GUI_API InlineCollection {
public:
    InlineCollection() noexcept = default;
    std::uint32_t GetCount() const noexcept;
    bool GetIsEmpty() const noexcept { return GetCount() == 0U; }
    Inline* GetItem(std::uint32_t index) const noexcept;
    InlineCollectionView GetView() const noexcept;
    void Add(Ref<Inline> value) noexcept;
    Result<bool> Remove(Inline& value) noexcept;
    void Clear() noexcept;

private:
    friend class Span;
    friend class Aero::Controls::TextBlock;
    explicit InlineCollection(Base::Object& owner) noexcept : owner_(&owner) {}
    Base::Object* owner_ = nullptr;
};

class AERO_GUI_API Span : public Inline {
    AERO_DECLARE_TYPE(Span, Inline)
public:
    Span() noexcept : Span(StaticTypeId()) {}
    ~Span() override;

    InlineCollection GetInlines() noexcept { return InlineCollection(*this); }
    InlineCollectionView GetInlines() const noexcept { return InlineCollectionView(*this); }
    Value GetMetadataInlines() const noexcept;
    void SetInlineValue(Value value) noexcept;
    void AddOwnedInline(Ref<Inline> value) noexcept;
    void ClearOwnedInlines() noexcept;

protected:
    explicit Span(Meta::TypeId runtimeType) noexcept : Inline(runtimeType), inlines_() {}
    std::uint32_t GetLogicalChildrenCount() const noexcept override { return inlines_.Size(); }
    DependencyObject* GetLogicalChild(std::uint32_t index) const noexcept override {
        return index < inlines_.Size() ? inlines_[index].Get() : nullptr;
    }

private:
    friend class Aero::Controls::TextBlock;
    friend struct Aero::Controls::TextBlockDocumentHelper;
    Base::Vector<Ref<Inline>> inlines_;
    Ref<Inline> pendingInline_;
};

class AERO_GUI_API Run : public Inline {
    AERO_DECLARE_TYPE(Run, Inline)
public:
    Run() noexcept : Inline(StaticTypeId()) {}
    ~Run() override = default;

    StringView GetText() const noexcept { return GetValue(TextProperty); }
    StringView GetContent() const noexcept { return GetText(); }
    void SetText(StringView value) noexcept { SetValue(TextProperty, value); }
    void SetContent(StringView value) noexcept { SetText(value); }

    AERO_DEPENDENCY_PROPERTY(String, Text);
};

class AERO_GUI_API LineBreak : public Inline {
    AERO_DECLARE_TYPE(LineBreak, Inline)
public:
    LineBreak() noexcept : Inline(StaticTypeId()) {}
    ~LineBreak() override = default;
};

class AERO_GUI_API InlineUIContainer : public Inline {
    AERO_DECLARE_TYPE(InlineUIContainer, Inline)
public:
    InlineUIContainer() noexcept : Inline(StaticTypeId()) {}

    UIElement* GetChild() const noexcept { return child_.Get(); }
    void SetChild(Ref<UIElement> value) noexcept { child_ = std::move(value); }

    AERO_DEPENDENCY_PROPERTY(Ref<UIElement>, Child);

private:
    Ref<UIElement> child_;
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

class AERO_GUI_API Hyperlink : public Span {
    AERO_DECLARE_TYPE(Hyperlink, Span)
public:
    Hyperlink() noexcept : Span(StaticTypeId()) {}
    ~Hyperlink() override = default;

    inline static constexpr RoutedEvent<Aero::RoutedEventArgs> ClickEvent{"Click"};
    ContentElement::Event<Aero::RoutedEventArgs> Click() noexcept { return GetEvent(ClickEvent); }
    inline static constexpr RoutedEvent<RequestNavigateEventArgs> RequestNavigateEvent{"RequestNavigate"};
    ContentElement::Event<RequestNavigateEventArgs> RequestNavigate() noexcept {
        return GetEvent(RequestNavigateEvent);
    }

    StringView GetNavigateUri() const noexcept;
    Aero::Input::ICommand* GetCommand() const noexcept;
    Value GetCommandParameter() const noexcept;
    Aero::UIElement* GetCommandTarget() const noexcept;

    void SetNavigateUri(StringView value) noexcept;
    void SetCommand(Ref<Aero::Input::ICommand> command) noexcept;
    void SetCommandParameter(Value parameter) noexcept;
    void SetCommandTarget(Ref<Aero::UIElement> target) noexcept;

    AERO_DEPENDENCY_PROPERTY(String, NavigateUri);
    AERO_DEPENDENCY_PROPERTY(Ref<Aero::Input::ICommand>, Command);
    AERO_DEPENDENCY_PROPERTY(Value, CommandParameter);
    AERO_DEPENDENCY_PROPERTY(Ref<Aero::UIElement>, CommandTarget);
};

} // namespace Aero::Documents
