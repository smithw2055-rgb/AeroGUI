#include <Aero/Controls/Metadata.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Presentation/Binding.hpp>
#include <Aero/Presentation/Metadata.hpp>

#include <cmath>
#include <cstdio>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Controls;
using namespace Aero::Platform;
using namespace Aero::Presentation;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf( \
                stderr, \
                "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

struct Fixture final {
    TypeRegistry types;
    MetadataBehaviorRegistrationStore
        typeBehaviors{types};
    MetadataRegistrationTypes
        typeRegistration{types, typeBehaviors};
    MetadataValueRegistrationStore
        valueRegistrations{types};
    DependencyPropertyRegistry
        properties{types, typeBehaviors};
    Dispatcher dispatcher;
    ObjectServicesScope services{
        dispatcher, properties,
        valueRegistrations};
    RoutedEventCatalog eventCatalog{
        types, typeBehaviors};
    RoutedEventManager events{eventCatalog};
    EffectiveValueEngine values{
        dispatcher, properties};
    ObjectTree tree{dispatcher, values};
    LayoutManager layout{dispatcher};

    bool Build() {
        MetaRegistrationContext registration(
            types, typeBehaviors,
            valueRegistrations, properties,
            &eventCatalog);
        CHECK(Aero::Core::Detail::
            PopulateCoreMetadata(registration));
        CHECK(Aero::Presentation::Detail::
            PopulatePresentationMetadata(
                registration));
        CHECK(Aero::Controls::Detail::
            PopulateControlsMetadata(
                registration));
        CHECK(types.Freeze());
        CHECK(typeBehaviors.Freeze());
        CHECK(valueRegistrations.Freeze());
        CHECK(properties.Freeze());
        CHECK(eventCatalog.Freeze());
        CHECK(values.Initialize());
        CHECK(tree.Initialize());
        CHECK(layout.Initialize());
        return true;
    }
};

class TestInputMethodHost final
    : public ITextInputMethodHost {
public:
    Result<void> SetClient(
        ITextCompositionClient* value) noexcept override {
        client = value;
        ++clientChangeCount;
        return {};
    }

    Result<void> SetCandidateWindow(
        const ImeCandidateWindow& value) noexcept override {
        candidate = value;
        ++candidateCount;
        return {};
    }

    Result<void>
    CancelNativeComposition() noexcept override {
        ++cancelCount;
        return {};
    }

    ITextCompositionClient* client = nullptr;
    ImeCandidateWindow candidate;
    std::uint32_t clientChangeCount = 0U;
    std::uint32_t candidateCount = 0U;
    std::uint32_t cancelCount = 0U;
};

bool DispatchKey(
    KeyboardInputManager& keyboard,
    std::uint32_t key,
    std::uint32_t modifiers = 0U) {
    Result<KeyboardDispatchResult> result =
        keyboard.Dispatch({
            KeyboardAction::Down,
            key,
            modifiers,
            false});
    return result && result.Value().routed;
}

bool TestKeyboardTextClipboardAndHistory() {
    Fixture fixture;
    CHECK(fixture.Build());
    TextBox textBox;
    TextBox source;
    CHECK(textBox.SetWidth(80.0));
    CHECK(textBox.SetHeight(24.0));
    CHECK(source.SetText(StringView("hello")));
    BindingManager bindings(fixture.dispatcher);
    CHECK(bindings.Initialize());
    BindingDescriptor binding;
    binding.source = &source;
    binding.sourceProperty =
        TextBox::TextProperty;
    binding.target = &textBox;
    binding.targetProperty =
        TextBox::TextProperty;
    binding.mode = BindingMode::TwoWay;
    Result<BindingHandle> bindingHandle =
        bindings.Attach(binding);
    CHECK(bindingHandle);
    CHECK(bindings.Flush().Value() == 1U);
    CHECK(textBox.Text() ==
        StringView("hello"));
    CHECK(fixture.tree.SetRoot(&textBox));
    CHECK(fixture.layout.SetRoot(
        &textBox, {80.0, 24.0}));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Lifecycle));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));
    CHECK(textBox.IsTabStop());

    HitTestManager hitTests;
    PointerInputManager pointer(
        hitTests, fixture.events, textBox);
    FocusManager focus(
        fixture.tree, fixture.events);
    MemoryClipboard clipboard;
    TextBoxInteractionManager interactions(
        fixture.tree, fixture.events,
        pointer, focus, clipboard);
    CHECK(interactions.Attach(textBox));
    CHECK(focus.SetFocus(&textBox).Value());

    KeyboardInputManager keyboard(
        focus, fixture.events, fixture.tree);
    TextInputManager textInput(
        focus, fixture.events, fixture.tree);
    CHECK(textInput.Dispatch({
        StringView(u8"中")}));
    CHECK(bindings.Flush().Value() == 1U);
    CHECK(textBox.Text() ==
        StringView(u8"hello中"));
    CHECK(source.Text() ==
        StringView(u8"hello中"));
    CHECK(textBox.Caret() == 6U);

    CHECK(DispatchKey(
        keyboard, KeyboardKeyLeft));
    CHECK(textBox.Caret() == 5U);
    CHECK(DispatchKey(
        keyboard,
        KeyboardKeyLeft,
        static_cast<std::uint32_t>(
            KeyboardModifiers::Shift)));
    CHECK(textBox.Selection().Start() == 4U);
    CHECK(textBox.Selection().End() == 5U);
    CHECK(DispatchKey(
        keyboard,
        KeyboardKeyC,
        static_cast<std::uint32_t>(
            KeyboardModifiers::Control)));
    String copied;
    CHECK(clipboard.ReadText(copied));
    CHECK(copied == StringView("o"));

    CHECK(DispatchKey(
        keyboard,
        KeyboardKeyX,
        static_cast<std::uint32_t>(
            KeyboardModifiers::Control)));
    CHECK(textBox.Text() ==
        StringView(u8"hell中"));
    CHECK(DispatchKey(
        keyboard,
        KeyboardKeyV,
        static_cast<std::uint32_t>(
            KeyboardModifiers::Control)));
    CHECK(textBox.Text() ==
        StringView(u8"hello中"));

    CHECK(DispatchKey(
        keyboard,
        KeyboardKeyA,
        static_cast<std::uint32_t>(
            KeyboardModifiers::Control)));
    CHECK(textInput.Dispatch({
        StringView("A")}));
    CHECK(textBox.Text() == StringView("A"));
    CHECK(DispatchKey(
        keyboard,
        KeyboardKeyZ,
        static_cast<std::uint32_t>(
            KeyboardModifiers::Control)));
    CHECK(textBox.Text() ==
        StringView(u8"hello中"));
    CHECK(DispatchKey(
        keyboard,
        KeyboardKeyY,
        static_cast<std::uint32_t>(
            KeyboardModifiers::Control)));
    CHECK(textBox.Text() == StringView("A"));

    CHECK(textBox.SetAcceptsReturn(true));
    CHECK(DispatchKey(
        keyboard, KeyboardKeyEnter));
    CHECK(textBox.Text() ==
        StringView("A\n"));
    CHECK(textBox.SetReadOnly(true));
    Result<TextInputDispatchResult> blocked =
        textInput.Dispatch({StringView("B")});
    CHECK(blocked && blocked.Value().routed);
    CHECK(textBox.Text() ==
        StringView("A\n"));

    CHECK(interactions.Detach(textBox).Value());
    CHECK(bindings.Detach(
        bindingHandle.Value()).Value());
    CHECK(fixture.layout.SetRoot(nullptr, {}));
    CHECK(fixture.tree.SetRoot(nullptr));
    CHECK(fixture.values.DetachObject(textBox));
    CHECK(fixture.values.DetachObject(source));
    return true;
}

bool TestPointerPasswordAndScrolling() {
    Fixture fixture;
    CHECK(fixture.Build());
    TextBox textBox;
    CHECK(textBox.SetText(
        StringView("01234567890123456789")));
    CHECK(fixture.tree.SetRoot(&textBox));
    CHECK(fixture.layout.SetRoot(
        &textBox, {48.0, 20.0}));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Lifecycle));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));
    CHECK(textBox.Data().extentWidth > 48.0);

    HitTestManager hitTests;
    PointerInputManager pointer(
        hitTests, fixture.events, textBox);
    FocusManager focus(
        fixture.tree, fixture.events);
    MemoryClipboard clipboard;
    TextBoxInteractionManager interactions(
        fixture.tree, fixture.events,
        pointer, focus, clipboard);
    CHECK(interactions.Attach(textBox));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));

    Result<PointerDispatchResult> down =
        pointer.Dispatch({
            1U, PointerAction::Down,
            {9.0, 8.0}});
    if (!down) {
        std::fprintf(
            stderr,
            "pointer down failed: %u %s\n",
            static_cast<unsigned>(
                down.GetStatus().code),
            down.GetStatus().message);
    }
    CHECK(down);
    CHECK(textBox.IsKeyboardFocused());
    CHECK(pointer.CapturedNode(1U) ==
        &textBox);
    const std::uint32_t capturedCaret =
        textBox.Caret();
    CHECK(pointer.ReleasePointer(1U).Value());
    CHECK(pointer.Dispatch({
        1U, PointerAction::Move,
        {34.0, 8.0}}));
    CHECK(textBox.Selection().Empty());
    CHECK(textBox.Caret() == capturedCaret);
    CHECK(pointer.Dispatch({
        1U, PointerAction::Down,
        {9.0, 8.0}}));
    CHECK(pointer.Dispatch({
        1U, PointerAction::Move,
        {34.0, 8.0}}));
    CHECK(!textBox.Selection().Empty());
    CHECK(pointer.Dispatch({
        1U, PointerAction::Up,
        {34.0, 8.0}}));
    CHECK(pointer.CapturedNode(1U) ==
        nullptr);

    CHECK(textBox.SetSelection(
        0U, textBox.Text().SizeBytes()));
    PasswordTextDisplayPolicy password;
    CHECK(password.SetMask(
        StringView(u8"●")));
    CHECK(textBox.SetDisplayPolicy(
        &password));
    KeyboardInputManager keyboard(
        focus, fixture.events, fixture.tree);
    CHECK(clipboard.WriteText(
        StringView("unchanged")));
    CHECK(DispatchKey(
        keyboard,
        KeyboardKeyC,
        static_cast<std::uint32_t>(
            KeyboardModifiers::Control)));
    String copied;
    CHECK(clipboard.ReadText(copied));
    CHECK(copied ==
        StringView("unchanged"));

    CHECK(textBox.SetDisplayPolicy(nullptr));
    CHECK(textBox.SetSelection(
        textBox.Text().SizeBytes(),
        textBox.Text().SizeBytes()));
    CHECK(textBox.Data().horizontalOffset > 0.0);
    const Rect caret =
        textBox.CaretRectangle();
    CHECK(std::isfinite(caret.x));
    CHECK(caret.height > 0.0);

    ScrollViewer viewer;
    CHECK(textBox.AttachScrollViewer(&viewer));
    CHECK(viewer.ContentScrollInfo() ==
        &textBox);
    CHECK(viewer.CanContentScroll());
    CHECK(textBox.AttachScrollViewer(nullptr));
    CHECK(viewer.ContentScrollInfo() ==
        nullptr);

    CHECK(interactions.Detach(textBox).Value());
    CHECK(fixture.layout.SetRoot(nullptr, {}));
    CHECK(fixture.tree.SetRoot(nullptr));
    CHECK(fixture.values.DetachObject(textBox));
    return true;
}

bool TestMaximumLengthAndMetadata() {
    Fixture fixture;
    CHECK(fixture.Build());
    TextBox textBox;
    CHECK(textBox.SetMaximumLength(2U));
    CHECK(textBox.SetText(
        StringView(u8"a中")));
    CHECK(!textBox.SetText(
        StringView(u8"ab中")));
    CHECK(textBox.Text() ==
        StringView(u8"a中"));
    Result<void> tooSmall = textBox.SetValue(
        TextBox::MaximumLengthProperty,
        Value::FromUnsignedInteger(
            BuiltinTypes::UnsignedInteger,
            1U));
    CHECK(!tooSmall);
    CHECK(textBox.MaximumLength() == 2U);
    Result<Value> tooLongValue =
        Value::TryFromString(
            BuiltinTypes::String,
            StringView("abc"));
    CHECK(tooLongValue);
    CHECK(!textBox.SetValue(
        TextBox::TextProperty,
        tooLongValue.Value()));
    CHECK(textBox.Text() ==
        StringView(u8"a中"));

    Aero::Text::EditableTextModel multiline;
    CHECK(multiline.SetText(
        StringView("a\nb")));
    PasswordTextDisplayPolicy password;
    String masked;
    CHECK(password.BuildDisplayText(
        multiline, masked));
    CHECK(masked ==
        StringView(u8"•\n•"));

    const DependencyProperty* textProperty =
        fixture.properties.Find(
            TextBox::TextProperty);
    CHECK(textProperty != nullptr);
    const PropertyMetadata* metadata =
        textProperty->MetadataFor(
            TextBox::StaticTypeId());
    CHECK(metadata != nullptr);
    CHECK(HasFlag(
        metadata->flags,
        PropertyMetadataFlags::
            BindsTwoWayByDefault));
    CHECK(fixture.values.DetachObject(textBox));
    return true;
}

bool TestImePreviewCommitCancelAndLifetime() {
    Fixture fixture;
    CHECK(fixture.Build());
    TextBox textBox;
    TextBox source;
    CHECK(textBox.SetWidth(120.0));
    CHECK(textBox.SetHeight(28.0));
    CHECK(textBox.SetLayoutRounding(
        true, 2.0));
    CHECK(source.SetText(StringView("base")));

    BindingManager bindings(fixture.dispatcher);
    CHECK(bindings.Initialize());
    BindingDescriptor binding;
    binding.source = &source;
    binding.sourceProperty = TextBox::TextProperty;
    binding.target = &textBox;
    binding.targetProperty = TextBox::TextProperty;
    binding.mode = BindingMode::TwoWay;
    Result<BindingHandle> bindingHandle =
        bindings.Attach(binding);
    CHECK(bindingHandle);
    CHECK(bindings.Flush().Value() == 1U);

    CHECK(fixture.tree.SetRoot(&textBox));
    CHECK(fixture.layout.SetRoot(
        &textBox, {120.0, 28.0}));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Lifecycle));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));

    HitTestManager hitTests;
    PointerInputManager pointer(
        hitTests, fixture.events, textBox);
    FocusManager focus(
        fixture.tree, fixture.events);
    MemoryClipboard clipboard;
    TextBoxInteractionManager interactions(
        fixture.tree, fixture.events,
        pointer, focus, clipboard);
    CHECK(interactions.Attach(textBox));
    CHECK(focus.SetFocus(&textBox).Value());

    TestInputMethodHost inputMethod;
    CHECK(textBox.SetInputMethodHost(&inputMethod));
    CHECK(inputMethod.client == &textBox);
    CHECK(textBox.SetSelection(4U, 4U));
    CHECK(inputMethod.client->BeginComposition());
    CHECK(inputMethod.client->UpdateComposition(
        StringView(u8"中")));
    CHECK(textBox.IsComposing());
    CHECK(textBox.CompositionText() ==
        StringView(u8"中"));
    CHECK(textBox.Text() == StringView("base"));
    CHECK(source.Text() == StringView("base"));
    CHECK(textBox.Caret() == 5U);
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));
    CHECK(inputMethod.candidateCount > 0U);
    CHECK(inputMethod.candidate.dpiScale == 2.0);
    CHECK(std::isfinite(
        inputMethod.candidate.caret.x));
    CHECK(inputMethod.candidate.caret.height > 0.0);

    CHECK(inputMethod.client->CommitComposition(
        StringView(u8"中文")));
    CHECK(!textBox.IsComposing());
    CHECK(textBox.Text() ==
        StringView(u8"base中文"));
    CHECK(source.Text() == StringView("base"));
    CHECK(bindings.Flush().Value() == 1U);
    CHECK(source.Text() ==
        StringView(u8"base中文"));
    CHECK(textBox.Undo());
    CHECK(bindings.Flush().Value() == 1U);
    CHECK(textBox.Text() == StringView("base"));
    CHECK(source.Text() == StringView("base"));
    CHECK(textBox.Redo());
    CHECK(bindings.Flush().Value() == 1U);
    CHECK(textBox.Text() ==
        StringView(u8"base中文"));
    CHECK(source.Text() ==
        StringView(u8"base中文"));

    CHECK(inputMethod.client->BeginComposition());
    CHECK(inputMethod.client->UpdateComposition(
        StringView(u8"取消")));
    CHECK(inputMethod.client->CancelComposition());
    CHECK(!textBox.IsComposing());
    CHECK(textBox.Text() ==
        StringView(u8"base中文"));

    CHECK(inputMethod.client->BeginComposition());
    CHECK(inputMethod.client->UpdateComposition(
        StringView(u8"焦点")));
    CHECK(focus.ClearFocus().Value());
    CHECK(!textBox.IsComposing());
    CHECK(inputMethod.cancelCount == 1U);
    CHECK(textBox.Text() ==
        StringView(u8"base中文"));

    CHECK(focus.SetFocus(&textBox).Value());
    CHECK(inputMethod.client->BeginComposition());
    CHECK(inputMethod.client->UpdateComposition(
        StringView(u8"只读")));
    CHECK(textBox.SetReadOnly(true));
    CHECK(!textBox.IsComposing());
    CHECK(inputMethod.cancelCount == 2U);
    CHECK(!inputMethod.client->BeginComposition());
    CHECK(textBox.SetReadOnly(false));

    CHECK(textBox.SetInputMethodHost(nullptr));
    CHECK(inputMethod.client == nullptr);
    CHECK(interactions.Detach(textBox).Value());
    CHECK(bindings.Detach(
        bindingHandle.Value()).Value());
    CHECK(fixture.layout.SetRoot(nullptr, {}));
    CHECK(fixture.tree.SetRoot(nullptr));
    CHECK(fixture.values.DetachObject(textBox));
    CHECK(fixture.values.DetachObject(source));

    TestInputMethodHost lifetimeHost;
    {
        TextBox lifetime;
        CHECK(lifetime.SetInputMethodHost(
            &lifetimeHost));
        CHECK(lifetime.SetText(StringView("x")));
        CHECK(lifetime.SetSelection(1U, 1U));
        CHECK(lifetime.BeginComposition());
        CHECK(lifetime.UpdateComposition(
            StringView(u8"析构")));
    }
    CHECK(lifetimeHost.client == nullptr);
    CHECK(lifetimeHost.clientChangeCount == 2U);
    return true;
}

} // namespace

int main() {
    if (!TestKeyboardTextClipboardAndHistory()) {
        return 1;
    }
    if (!TestPointerPasswordAndScrolling()) {
        return 1;
    }
    if (!TestMaximumLengthAndMetadata()) {
        return 1;
    }
    if (!TestImePreviewCommitCancelAndLifetime()) {
        return 1;
    }
    std::puts("TextBox tests passed");
    return 0;
}
