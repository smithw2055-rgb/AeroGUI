#pragma once

#include <Aero/Controls/Controls.hpp>
#include <Aero/Presentation/Commands.hpp>

#include <utility>

namespace Aero::Documents {

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
        if (!value) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Span inline cannot be null");
        }
        Base::Ref<Base::Object> object(value);
        return AddOwnedInline(object, *value);
    }
    Base::Result<void> ClearInlines() noexcept {
        return ClearOwnedInlines();
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

} // namespace Aero::Documents
