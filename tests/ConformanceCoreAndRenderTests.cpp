#include "ConformanceTestHarness.hpp"

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

bool TestDependencyPropertyVirtualFallback() {
    LiveGui* live = NewTutorialLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({200.0, 200.0});

    Result<Ref<Canvas>> host = MakeRef<Canvas>();
    CHECK(host);
    CHECK(view.SetContent(
        Ref<FrameworkElement>(host.Value()), {200.0, 200.0}));
    Result<Ref<CoercePilot>> control = MakeRef<CoercePilot>();
    CHECK(control);
    host->GetChildren().Add(
        Ref<Aero::UIElement>(control.Value()));
    Pump(view, 0.016);

    control->SetLevel(42);
    CHECK(control->GetLevel() == 42);
    // No delegate registered: >100 must clamp via CoerceValueCore.
    control->SetLevel(150);
    CHECK(control->GetLevel() == 100);
    // Negative must be rejected via ValidateValueCore; effective stays 100.
    control->SetLevel(-5);
    CHECK(control->GetLevel() == 100);

    // Moved TextBox coerce delegate: invalid UTF-8 is rejected, last good stays.
    Result<Ref<TextBox>> textBox = MakeRef<TextBox>();
    CHECK(textBox);
    host->GetChildren().Add(
        Ref<Aero::UIElement>(textBox.Value()));
    Pump(view, 0.016);
    textBox->SetText(StringView("hello"));
    CHECK(textBox->GetText() == StringView("hello"));
    const char badBytes[] = {'a', static_cast<char>(0xFF), 'b'};
    textBox->SetText(StringView(badBytes, 3U));
    CHECK(textBox->GetText() == StringView("hello"));
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
    static_assert(std::is_base_of<Aero::FrameworkElement, Aero::Controls::Primitives::Popup>::value,
        "Popup must derive FrameworkElement");
    static_assert(!std::is_base_of<Aero::Controls::ContentControl, Aero::Controls::Primitives::Popup>::value,
        "Popup must not derive ContentControl");
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

    Result<Ref<Popup>> popup = MakeRef<Popup>();
    CHECK(popup);
    CHECK(TryCast<FrameworkElement>(popup.Value().Get()) != nullptr);
    CHECK(TryCast<ContentControl>(popup.Value().Get()) == nullptr);

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

// P4.3 exit gate: the commit path (in-place refresh vs full rebuild) must
// be content-transparent. Drives a State-only change and its round trip
// through the public frame identity and requires bit-identical content
// hashes, stable node/command counts, and version progress. The idle pump
// additionally pins the clean-frame early-out (no new version published).
bool TestRenderCommitContentStability() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;

    // Programmatic attach pattern (mirrors TestPanelProgrammaticAdd):
    // mount the root first, then add the child so element-tree attach
    // wires visual/render parenting through the standard path.
    Result<Ref<Canvas>> canvas = MakeRef<Canvas>();
    CHECK(canvas);
    canvas->SetWidth(200.0);
    canvas->SetHeight(120.0);
    Result<Ref<Aero::Media::Brush>> background =
        Aero::Media::MakeSolidColorBrush({1.0F, 0.0F, 0.0F, 1.0F});
    CHECK(background);
    canvas->SetBackground(background.Value());
    CHECK(view.SetContent(canvas.Value(), {200.0, 120.0}));
    Pump(view, 0.016);
    Result<Ref<Button>> button = MakeRef<Button>();
    CHECK(button);
    button->SetWidth(80.0);
    button->SetHeight(32.0);
    canvas->GetChildren().Add(
        Ref<Aero::UIElement>(button.Value()));
    Pump(view, 0.032);
    Pump(view, 0.048);
    CHECK(button->GetVisualParent() == canvas.Value().Get());

    Aero::CommittedFrameInfo base = view.GetCommittedFrameInfo();
    CHECK(base.version != 0U);
    CHECK(base.nodeCount >= 2U);
    CHECK(base.contentHash != 0U);

    // Steady state: no work scheduled, the commit early-outs and the
    // published frame (version and content) is untouched.
    Pump(view, 0.064);
    const Aero::CommittedFrameInfo idle = view.GetCommittedFrameInfo();
    CHECK(idle.version == base.version);
    CHECK(idle.nodeCount == base.nodeCount);
    CHECK(idle.commandCount == base.commandCount);
    CHECK(idle.contentHash == base.contentHash);

    // State-only change (opacity touches no layout): a new version with
    // identical structure, and observably different content.
    button->SetValue(
        Aero::UIElement::OpacityProperty, 0.5);
    Pump(view, 0.080);
    Pump(view, 0.096);
    const Aero::CommittedFrameInfo dimmed = view.GetCommittedFrameInfo();
    CHECK(dimmed.version != base.version);
    CHECK(dimmed.nodeCount == base.nodeCount);
    CHECK(dimmed.commandCount == base.commandCount);
    CHECK(dimmed.contentHash != base.contentHash);

    // Round trip: restoring the exact value must reproduce the exact
    // committed bytes regardless of which commit path ran.
    button->SetValue(
        Aero::UIElement::OpacityProperty, 1.0);
    Pump(view, 0.112);
    Pump(view, 0.128);
    const Aero::CommittedFrameInfo restored = view.GetCommittedFrameInfo();
    CHECK(restored.version != dimmed.version);
    CHECK(restored.nodeCount == base.nodeCount);
    CHECK(restored.commandCount == base.commandCount);
    CHECK(restored.contentHash == base.contentHash);
    return true;
}

// P4.4 exit gate: pins the fill+stroke geometry scene bytes. Game::OnRender
// issues DrawRectangle + DrawLine + DrawGeometry(fill AND pen); the latter
// must survive the single-flatten unification bit-identically (one
// geometry.Flatten feeding both fill and stroke tessellation instead of
// two). Any tessellation divergence changes commandCount/contentHash.
bool TestDrawGeometryFillStrokeScene() {
    // Game lives in the tutorial test module.
    LiveGui* live = NewTutorialLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    Result<Ref<Game>> game = MakeRef<Game>();
    CHECK(game);
    game->SetWidth(80.0);
    game->SetHeight(60.0);
    CHECK(view.SetContent(game.Value(), {80.0, 60.0}));
    Pump(view, 0.016);
    Pump(view, 0.032);
    const Aero::CommittedFrameInfo info = view.GetCommittedFrameInfo();
    CHECK(info.version != 0U);
    CHECK(info.nodeCount >= 1U);
    CHECK(info.commandCount > 0U);
    CHECK(info.contentHash != 0U);
    // Idempotent re-commit pins the exact bytes.
    Pump(view, 0.048);
    const Aero::CommittedFrameInfo again = view.GetCommittedFrameInfo();
    CHECK(again.version == info.version);
    CHECK(again.commandCount == info.commandCount);
    CHECK(again.contentHash == info.contentHash);
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
    button->SetWidth(80.0);
    button->SetHeight(32.0);
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
        button->GetDispatcher();
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
    first->SetText("alpha");
    second->SetText("beta");
    combo->GetItems().Add(first.Value());
    combo->GetItems().Add(second.Value());
    combo->SetSelectedIndex(1U);
    CHECK(combo->GetSelectedItem().Get() == second.Value().Get());
    combo->SetIsDropDownOpen(true);
    CHECK(combo->GetIsDropDownOpen());

    CHECK(view.SetContent(combo.Value()));
    Pump(view, 0.016);
    static_cast<void>(combo->ApplyTemplate());
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
    {
        Setter duration;
        duration.Set(
            Timeline::DurationProperty,
            Duration::FromTimeSpan(TimeSpan::FromMicroseconds(2'000'000ULL)));
        style.AddSetter(duration);
        Setter begin;
        begin.Set(Timeline::BeginTimeProperty, TimeSpan::Zero());
        style.AddSetter(begin);
        Setter reverse;
        reverse.Set(Timeline::AutoReverseProperty, true);
        style.AddSetter(reverse);
    }
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
    shared->SetDepth(1000.0);
    leftSpin->SetRotationY(25.0);
    rightSpin->SetRotationY(-25.0);
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
    rectangle->SetWidth(40.0);
    rectangle->SetHeight(20.0);
    host->GetChildren().Add(
        Ref<Aero::UIElement>(rectangle.Value()));
    Pump(view, 0.032);
    CHECK(rectangle->GetVisualParent() == host);
    CHECK(Aero::Media::VisualTreeHelper::GetChildrenCount(*host) == 1U);
    CHECK(Aero::Media::VisualTreeHelper::GetChild(*host, 0U) ==
        rectangle.Value().Get());
    host->GetChildren().Remove(*rectangle.Value());
    Pump(view, 0.048);
    CHECK(rectangle->GetVisualParent() == nullptr);
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

