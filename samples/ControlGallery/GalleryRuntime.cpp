#include "GalleryRuntime.hpp"

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/App/Application.hpp>
#include <Aero/App/Window.hpp>
#include <Aero/Controls/Bars.hpp>
#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/ContentControls.hpp>
#include <Aero/Controls/ListView.hpp>
#include <Aero/Controls/Menus.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/Trees.hpp>
#include <Aero/Controls/Virtualization.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Integration.hpp>
#include <Aero/Integration/SourceProvider.hpp>
#include <Aero/Metadata.hpp>
#include <Aero/RuntimeEnvironment.hpp>
#include <Aero/Platform/Clipboard.hpp>
#include <Aero/Presentation/Brushes.hpp>
#include <Aero/Presentation/Effects.hpp>
#include <Aero/Presentation/Transforms.hpp>

// Repository dogfood uses View publicly; diagnostics stay behind ViewAccess.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace Aero::Samples::ControlGallery {
namespace {

using namespace Base;
using namespace Controls;
using namespace Core;
using namespace Presentation;

Status Failure(const char* message) noexcept {
    return Status::Failure(
        ErrorCode::InvalidState, message);
}

bool ReadFile(
    const std::string& path,
    std::vector<std::uint8_t>& output) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    stream.seekg(0, std::ios::end);
    const std::streamoff length = stream.tellg();
    if (length < 0 ||
        static_cast<std::uint64_t>(length) > UINT32_MAX) {
        return false;
    }
    stream.seekg(0, std::ios::beg);
    output.resize(static_cast<std::size_t>(length));
    return output.empty() || static_cast<bool>(
        stream.read(
            reinterpret_cast<char*>(output.data()),
            static_cast<std::streamsize>(length)));
}

class GalleryItem final : public Object {
    AERO_DECLARE_TYPE_NAMED(
        GalleryItem,
        Object,
        "urn:aero-control-gallery",
        "GalleryItem")
public:
    GalleryItem() noexcept = default;
};

class GalleryItemsSource final : public IItemsSource {
public:
    Result<void> Initialize() noexcept {
        Result<Ref<GalleryItem>> made =
            MakeRef<GalleryItem>();
        if (!made) return made.GetStatus();
        item_ = Ref<Object>(std::move(made).Value());
        return {};
    }

    std::uint32_t Count() const noexcept override {
        return count_;
    }

    Ref<Object> ItemAt(
        std::uint32_t index) const noexcept override {
        return index < Count()
            ? item_ : Ref<Object>();
    }

    Result<void> TryAddItemsChanged(
        const ItemsChangedHandler& handler) noexcept override {
        return changed_.TryAdd(handler);
    }

    bool RemoveItemsChanged(
        const ItemsChangedHandler& handler) noexcept override {
        return changed_.Remove(handler);
    }
    void ExerciseChange(
        std::uint32_t step) noexcept {
        ItemsChangedEvent event;
        switch (step % 5U) {
        case 0U:
            if (count_ == 0U) return;
            --count_;
            event.action =
                ItemsChangeAction::Remove;
            event.oldIndex = 100U;
            event.oldCount = 1U;
            break;
        case 1U:
            ++count_;
            event.action =
                ItemsChangeAction::Add;
            event.newIndex = 100U;
            event.newCount = 1U;
            break;
        case 2U:
            event.action =
                ItemsChangeAction::Move;
            event.oldIndex = 120U;
            event.newIndex = 140U;
            event.oldCount = 1U;
            event.newCount = 1U;
            break;
        case 3U:
            event.action =
                ItemsChangeAction::Replace;
            event.oldIndex = 160U;
            event.newIndex = 160U;
            event.oldCount = 1U;
            event.newCount = 1U;
            break;
        default:
            count_ = 10000U;
            event.action =
                ItemsChangeAction::Reset;
            event.oldCount = count_;
            event.newCount = count_;
            break;
        }
        if (!changed_.Empty()) {
            changed_.Invoke(event);
        }
    }

private:
    Ref<Object> item_;
    ItemsChangedHandler changed_;
    std::uint32_t count_ = 10000U;
};

Result<Ref<Object>> MakeVirtualizedItem(
    const Ref<Object>&,
    void*) noexcept {
    Result<Ref<TextBlock>> made = MakeRef<TextBlock>();
    if (!made) return made.GetStatus();
    Ref<TextBlock> text = std::move(made).Value();
    Result<void> configured =
        text->SetText("Virtualized gallery item");
    if (configured) configured = text->SetHeight(24.0);
    if (configured) configured = text->SetWidth(340.0);
    if (!configured) return configured.GetStatus();
    return Ref<Object>(std::move(text));
}

} // namespace

struct GalleryRuntime::Impl final {
    Impl() noexcept
        : selectorChangedHandler(
              this,
              &Impl::OnSelectorChanged),
          selectorMouseDownHandler(
              this,
              &Impl::OnSelectorMouseDown),
          selectorExpanderClickHandler(
              this,
              &Impl::OnSelectorExpanderClick) {}

    Platform::MemoryClipboard clipboard;
    RuntimeEnvironment environment;
    Ref<View> view;
    Ref<Object> root;
    Integration::SourceProviderAdapter
        galleryThemeSource;
    GalleryItemsSource items;
    DataTemplate itemTemplate{
        &MakeVirtualizedItem, nullptr};
    ListBox* listBox = nullptr;
    VirtualizingStackPanel* virtualizingPanel = nullptr;
    TreeView* selectorTree = nullptr;
    Grid* galleryLayoutRoot = nullptr;
    ScaleTransform* galleryRootScale = nullptr;
    UIElement* navigationPanel = nullptr;
    Button* selectorExpanderButton = nullptr;
    RoutedEventHandler selectorChangedHandler;
    MouseButtonEventHandler selectorMouseDownHandler;
    RoutedEventHandler selectorExpanderClickHandler;
    GallerySnapshot snapshot;
    std::string assetPath;
    GalleryLoadMode loadMode = GalleryLoadMode::Runtime;
    GalleryTheme theme = GalleryTheme::Light;
    GalleryScenario configuredScenario =
        GalleryScenario::Smoke;
    std::uint32_t configuredFrameCount = 0U;
    bool configuredBatchingEnabled = true;
    std::string configuredPage = "home";
    std::string pendingPage;
    bool manualAnimationClock = false;
    bool portraitLayout = false;
    bool navigationOpen = true;
    double layoutWidth = 1280.0;
    double layoutHeight = 768.0;
    double layoutDpiScale = 1.0;

    ~Impl() { Cleanup(); }

    View& Runtime() const noexcept {
        return *view;
    }

    static Result<Integration::Source>
    LoadGalleryThemeSource(
        const ResourceUri& uri,
        void* context) noexcept {
        constexpr StringView prefix(
            "Aero.GUI.Extensions;component/Theme/");
        const StringView path = uri.Path();
        if (context == nullptr ||
            path.SizeBytes() <= prefix.SizeBytes() ||
            path.Substr(0U, prefix.SizeBytes()) != prefix) {
            return Status::Failure(
                ErrorCode::NotFound,
                "Aero.GUI.Extensions theme resource was not found");
        }

        Impl& owner = *static_cast<Impl*>(context);
        const StringView relative = path.Substr(
            prefix.SizeBytes());
        std::string file = owner.assetPath + "/Theme/";
        file.append(relative.Data(), relative.SizeBytes());
        std::vector<std::uint8_t> bytes;
        if (!ReadFile(file, bytes)) {
            return Status::Failure(
                ErrorCode::NotFound,
                "Aero.GUI.Extensions theme source could not be opened");
        }

        Integration::Source source;
        source.uri = uri;
        Result<void> copied = source.bytes.TryAppend({
            bytes.data(),
            static_cast<std::uint32_t>(bytes.size())});
        if (!copied) return copied.GetStatus();
        source.revision = 1U;
        return source;
    }

    Result<void> LoadDocument(
        const std::string& assetDirectory,
        GalleryLoadMode mode) noexcept {
        const std::string applicationPath =
            assetDirectory + "/Data/App.xaml";
        Result<ResourceUri> applicationOrigin =
            ResourceUri::Parse(StringView(
                applicationPath.data(),
                static_cast<std::uint32_t>(
                    applicationPath.size())));
        if (!applicationOrigin) return applicationOrigin.GetStatus();
        std::vector<std::uint8_t> applicationSource;
        if (!ReadFile(applicationPath, applicationSource)) {
            return Failure("Gallery App XAML is unavailable");
        }
        DiagnosticBag applicationDiagnostics;
        Result<UiDocument> applicationDocument = Runtime().Parse(
            {reinterpret_cast<const char*>(applicationSource.data()),
             static_cast<std::uint32_t>(applicationSource.size())},
            applicationOrigin.Value(),
            &applicationDiagnostics);
        if (!applicationDocument) {
            for (const Diagnostic& diagnostic : applicationDiagnostics.Items()) {
                const SourceSpan sourceSpan = diagnostic.Source();
                std::fprintf(stderr,
                    "Gallery App XAML %u:%u: %.*s\n",
                    sourceSpan.begin.line, sourceSpan.begin.column,
                    static_cast<int>(diagnostic.Message().SizeBytes()),
                    diagnostic.Message().Data());
            }
            return applicationDocument.GetStatus();
        }
        const Ref<Object>& applicationRoot = applicationDocument.Value().Root();
        if (!applicationRoot ||
            applicationRoot->RuntimeType() !=
                App::Application::StaticTypeId()) {
            return Failure("Gallery App XAML root is not Application");
        }
        App::Application& application =
            static_cast<App::Application&>(*applicationRoot);
        Ref<ResourceDictionary> applicationResources =
            application.Resources();
        if (!applicationResources) {
            return Failure("Gallery App XAML has no resources");
        }
        if (!applicationResources->Contains(
                StringView("MainWindowBackground"))) {
            return Failure(
                "Gallery App XAML did not resolve its merged Resources.xaml");
        }
        Result<void> resources = Runtime().SetResourceDictionary(
            RuntimeResourceLayer::Application,
            *applicationResources);
        if (!resources) return resources.GetStatus();

        const std::string documentPath =
            assetDirectory + "/Data/MainWindow.xaml";
        Result<ResourceUri> origin =
            ResourceUri::Parse(StringView(
                documentPath.data(),
                static_cast<std::uint32_t>(
                    documentPath.size())));
        if (!origin) return origin.GetStatus();
        std::vector<std::uint8_t> source;
        if (!ReadFile(documentPath, source)) {
            return Failure("Gallery MainWindow XAML is unavailable");
        }
        DiagnosticBag diagnostics;
        Result<UiDocument> loaded = Runtime().Parse(
            {reinterpret_cast<const char*>(source.data()),
             static_cast<std::uint32_t>(source.size())},
            origin.Value(), &diagnostics);
        if (!loaded) {
            for (const Diagnostic& diagnostic : diagnostics.Items()) {
                const SourceSpan sourceSpan = diagnostic.Source();
                std::fprintf(stderr,
                    "Gallery MainWindow XAML %u:%u: %.*s\n",
                    sourceSpan.begin.line, sourceSpan.begin.column,
                    static_cast<int>(diagnostic.Message().SizeBytes()),
                    diagnostic.Message().Data());
            }
            return loaded.GetStatus();
        }
        root = loaded.Value().Root();
        if (mode == GalleryLoadMode::Compiled) {
            std::fprintf(stderr,
                "AeroControlGallery: Data XAML compiled mode currently uses the source document\n");
        }
        return Runtime().SetContent(
            std::move(loaded).Value(), {1280.0, 768.0});
    }

    Result<void> LoadTheme(
        const std::string&,
        GalleryTheme requested) noexcept {
        return Runtime().LoadBuiltInTheme(
            requested == GalleryTheme::Light
            ? BuiltInTheme::Light
            : BuiltInTheme::Dark);
    }

    Result<void> ValidateBinding() noexcept {
        TextBox* input =
            Runtime().FindNamed<TextBox>("Input");
        TextBlock* mirror =
            Runtime().FindNamed<TextBlock>(
                "BindingMirror");
        UIElement* homePage =
            Runtime().FindNamed<UIElement>(
                "HomePage");
        UIElement* textPage =
            Runtime().FindNamed<UIElement>(
                "TextBoxPage");
        if (input == nullptr || mirror == nullptr ||
            homePage == nullptr || textPage == nullptr) {
            return Failure(
                "ControlGallery binding endpoints are missing");
        }
        Result<void> visibility =
            homePage->SetVisibility(
                Visibility::Collapsed);
        if (visibility) {
            visibility = textPage->SetVisibility(
                Visibility::Visible);
        }
        if (!visibility) {
            return visibility.GetStatus();
        }
        Result<void> changed =
            input->SetText(
                "Binding validation passed");
        if (!changed) {
            return changed.GetStatus();
        }
        Result<ViewFrameResult> frame =
            Runtime().RunFrame();
        if (!frame) {
            return frame.GetStatus();
        }
        UpdateFrameSnapshot(frame.Value());
        if (mirror->Text() != input->Text()) {
            return Failure(
                "ControlGallery binding validation failed");
        }
        changed = input->SetText({});
        if (!changed) {
            return changed.GetStatus();
        }
        frame = Runtime().RunFrame();
        if (!frame) {
            return frame.GetStatus();
        }
        UpdateFrameSnapshot(frame.Value());
        if (!mirror->Text().Empty()) {
            return Failure(
                "ControlGallery binding reset failed");
        }
        visibility = textPage->SetVisibility(
            Visibility::Collapsed);
        if (visibility) {
            visibility = homePage->SetVisibility(
                Visibility::Visible);
        }
        return visibility;
    }

    Result<void> ConfigureItems() noexcept {
        listBox = Runtime().FindNamed<ListBox>("BigList");
        if (listBox == nullptr) {
            return Failure(
                "ControlGallery items endpoint is missing");
        }
        Panel* itemsHost = listBox->ItemsHost();
        if (itemsHost == nullptr ||
            !itemsHost->PropertyRegistry().
                Types().IsDerivedFrom(
                    itemsHost->RuntimeType(),
                    VirtualizingStackPanel::StaticTypeId())) {
            return Failure(
                "ListBox template did not supply a VirtualizingStackPanel ItemsHost");
        }
        virtualizingPanel =
            static_cast<VirtualizingStackPanel*>(
                itemsHost);
        Result<void> initialized = items.Initialize();
        if (!initialized) return initialized.GetStatus();
        listBox->SetItemTemplate(&itemTemplate);
        Result<void> source =
            listBox->SetItemsSource(&items);
        if (!source) return source.GetStatus();
        Result<void> extent =
            virtualizingPanel->SetEstimatedItemExtent(
                24.0);
        if (extent) {
            extent = virtualizingPanel->SetOverscanCount(
                2U);
        }
        if (!extent) return extent.GetStatus();
        Result<bool> viewport =
            virtualizingPanel->SetViewport(
                {360.0, 120.0});
        if (!viewport) return viewport.GetStatus();
        Result<bool> scrolled =
            virtualizingPanel->SetVerticalOffset(
                4800.0);
        if (!scrolled) return scrolled.GetStatus();
        return {};
    }

    void UpdateFrameSnapshot(
        const ViewFrameResult& frame) noexcept {
        snapshot.layoutPassVersion =
            frame.layout.passVersion;
        snapshot.measuredCount =
            frame.layout.measuredCount;
        snapshot.arrangedCount =
            frame.layout.arrangedCount;
        snapshot.pendingMeasureCount =
            frame.layout.pendingMeasureCount;
        snapshot.pendingArrangeCount =
            frame.layout.pendingArrangeCount;
        snapshot.planHash =
            frame.render.snapshotHash;
        snapshot.nodeCount =
            frame.render.nodeCount;
        snapshot.commandCount =
            frame.render.commandCount;
        snapshot.textCommandCount =
            frame.render.glyphCommandCount;
        snapshot.drawPacketCount =
            frame.render.drawPacketCount;
        snapshot.batchCount =
            frame.render.batchCount;
        snapshot.drawCallCount =
            frame.render.drawCallCount;
        snapshot.mergedPacketCount =
            frame.render.mergedPacketCount;
        snapshot.barrierCount =
            frame.render.barrierCount;
        snapshot.instanceCount =
            frame.render.instanceCount;
        snapshot.stateBindingCount =
            frame.render.stateBindingCount;
        snapshot.batchingEnabled =
            frame.render.batchingEnabled;
        ++snapshot.executedFrameCount;
    }

    Result<void> RunFrame() noexcept {
        if (manualAnimationClock) {
            Result<std::uint32_t> advanced =
                Runtime().AdvanceAnimationTime(16U);
            if (!advanced) return advanced.GetStatus();
        }
        Result<ViewFrameResult> frame =
            Runtime().RunFrame();
        if (!frame) return frame.GetStatus();
        UpdateFrameSnapshot(frame.Value());
        return {};
    }

    Point ElementCenter(
        UIElement& element) const noexcept {
        Point origin{};
        Visual* current = &element;
        while (current != nullptr) {
            UIElement* currentElement =
                current->AsUIElement();
            if (currentElement != nullptr) {
                const Rect slot =
                    currentElement->LayoutSlot();
                origin.x += slot.x;
                origin.y += slot.y;
            }
            current = current->VisualParent();
        }
        origin.x += element.RenderSize().width * 0.5;
        origin.y += element.RenderSize().height * 0.5;
        return origin;
    }

    Result<void> ClickElement(
        UIElement& element,
        std::uint32_t pointerId) noexcept {
        Result<void> settled = RunFrame();
        if (!settled) {
            return settled.GetStatus();
        }
        for (std::uint32_t attempt = 0U;
             attempt < 4U &&
             !element.IsArrangeValid();
             ++attempt) {
            settled = RunFrame();
            if (!settled) {
                return settled.GetStatus();
            }
        }
        if (!element.IsArrangeValid()) {
            return Failure(
                "ControlGallery click target did not settle layout");
        }
        Border* rootElement =
            root
            ? static_cast<Border*>(root.Get())
            : nullptr;
        if (rootElement == nullptr ||
            !rootElement->IsArrangeValid()) {
            return Failure(
                "ControlGallery root did not settle layout before click");
        }
        PointerInput input;
        input.pointerId = pointerId;
        input.position = ElementCenter(element);
        input.changedButton = MouseButton::Left;
        input.action = PointerAction::Down;
        Result<PointerDispatchResult> down =
            Runtime().DispatchPointer(input);
        if (!down) {
            return Failure(
                "ControlGallery pointer down dispatch failed");
        }
        settled = RunFrame();
        if (!settled) {
            return settled.GetStatus();
        }
        input.action = PointerAction::Up;
        Result<PointerDispatchResult> up =
            Runtime().DispatchPointer(input);
        if (!up) {
            return Failure(
                "ControlGallery pointer up dispatch failed");
        }
        return {};
    }

    Result<void> ConfigurePopupServices() noexcept {
        Button* target =
            Runtime().FindNamed<Button>(
                "PrimaryButton");
        ContextMenu* contextMenu =
            Runtime().FindNamed<ContextMenu>(
                "SampleContextMenu");
        ToolTip* toolTip =
            Runtime().FindNamed<ToolTip>(
                "SampleToolTip");
        if (target == nullptr ||
            contextMenu == nullptr ||
            toolTip == nullptr) {
            return Failure(
                "ControlGallery popup service targets are missing");
        }
        Ref<ContextMenu> contextReference =
            Ref<ContextMenu>::TryFromBorrowed(
                *contextMenu);
        Ref<ToolTip> toolTipReference =
            Ref<ToolTip>::TryFromBorrowed(
                *toolTip);
        if (!contextReference ||
            !toolTipReference) {
            return Failure(
                "ControlGallery popup service targets are not managed objects");
        }
        Result<void> attached =
            ContextMenuService::SetContextMenu(
                *target,
                std::move(contextReference));
        if (attached) {
            attached =
                ToolTipService::SetToolTip(
                    *target,
                    std::move(toolTipReference));
        }
        if (attached) {
            attached =
                ToolTipService::
                    SetInitialShowDelay(
                        *target, 50U);
        }
        if (attached) {
            attached =
                ToolTipService::SetShowDuration(
                    *target, 500U);
        }
        if (!attached) {
            return attached.GetStatus();
        }
        if (ContextMenuService::
                GetContextMenu(*target).Get() !=
                contextMenu ||
            ToolTipService::
                GetToolTip(*target).Get() !=
                toolTip) {
            return Failure(
                "ControlGallery popup service attachment failed");
        }

        UIElement* pageContextTarget =
            Runtime().FindNamed<UIElement>(
                "ContextMenuPageTarget");
        ContextMenu* pageContextMenu =
            Runtime().FindNamed<ContextMenu>(
                "ContextMenuPageSample");
        Button* pageToolTipTarget =
            Runtime().FindNamed<Button>(
                "ToolTipPageTarget");
        ToolTip* pageToolTip =
            Runtime().FindNamed<ToolTip>(
                "ToolTipPageSample");
        if (pageContextTarget == nullptr ||
            pageContextMenu == nullptr ||
            pageToolTipTarget == nullptr ||
            pageToolTip == nullptr) {
            return Failure(
                "ControlGallery page popup service targets are missing");
        }
        Ref<ContextMenu> pageContextReference =
            Ref<ContextMenu>::TryFromBorrowed(
                *pageContextMenu);
        Ref<ToolTip> pageToolTipReference =
            Ref<ToolTip>::TryFromBorrowed(
                *pageToolTip);
        if (!pageContextReference ||
            !pageToolTipReference) {
            return Failure(
                "ControlGallery page popup service targets are not managed objects");
        }
        attached = ContextMenuService::SetContextMenu(
            *pageContextTarget,
            std::move(pageContextReference));
        if (attached) {
            attached = ToolTipService::SetToolTip(
                *pageToolTipTarget,
                std::move(pageToolTipReference));
        }
        if (attached) {
            attached =
                ToolTipService::SetInitialShowDelay(
                    *pageToolTipTarget, 300U);
        }
        if (attached) {
            attached =
                ToolTipService::SetShowDuration(
                    *pageToolTipTarget, 5000U);
        }
        if (!attached) return attached.GetStatus();
        if (ContextMenuService::GetContextMenu(
                *pageContextTarget).Get() !=
                pageContextMenu ||
            ToolTipService::GetToolTip(
                *pageToolTipTarget).Get() !=
                pageToolTip) {
            return Failure(
                "ControlGallery page popup service attachment failed");
        }
        return {};
    }

    void QueuePageForItem(
        TreeViewItem* selected) noexcept {
        if (selected == nullptr) return;
        const Base::StringView header =
            selected->Header();
        if (header == "Button") {
            pendingPage = "button";
        } else if (header == "RepeatButton") {
            pendingPage = "repeatbutton";
        } else if (header == "ToggleButton") {
            pendingPage = "togglebutton";
        } else if (header == "CheckBox") {
            pendingPage = "checkbox";
        } else if (header == "RadioButton") {
            pendingPage = "radiobutton";
        } else if (header == "Slider") {
            pendingPage = "slider";
        } else if (header == "ProgressBar") {
            pendingPage = "progressbar";
        } else if (header == "TextBlock") {
            pendingPage = "textblock";
        } else if (header == "TextBox") {
            pendingPage = "textbox";
        } else if (header == "PasswordBox") {
            pendingPage = "passwordbox";
        } else if (header == "Hyperlink") {
            pendingPage = "hyperlink";
        } else if (header == "GroupBox") {
            pendingPage = "groupbox";
        } else if (header == "Expander") {
            pendingPage = "expander";
        } else if (header == "TabControl") {
            pendingPage = "tabcontrol";
        } else if (header == "ScrollViewer") {
            pendingPage = "scrollviewer";
        } else if (header == "Menu") {
            pendingPage = "menu";
        } else if (header == "ContextMenu") {
            pendingPage = "contextmenu";
        } else if (header == "ToolBar") {
            pendingPage = "toolbar";
        } else if (header == "StatusBar") {
            pendingPage = "statusbar";
        } else if (header == "ToolTip") {
            pendingPage = "tooltip";
        } else if (header == "Brushes") {
            pendingPage = "brushes";
        } else if (header == "Image") {
            pendingPage = "image";
        } else if (header == "Effects") {
            pendingPage = "effects";
        } else if (header == "Blending") {
            pendingPage = "blending";
        } else if (header == "Animation") {
            pendingPage = "animation";
        } else if (header == "Canvas") {
            pendingPage = "canvas";
        } else if (header == "StackPanel") {
            pendingPage = "stackpanel";
        } else if (header == "WrapPanel") {
            pendingPage = "wrappanel";
        } else if (header == "DockPanel") {
            pendingPage = "dockpanel";
        } else if (header == "Grid") {
            pendingPage = "grid";
        } else if (header == "UniformGrid") {
            pendingPage = "uniformgrid";
        } else if (header == "ItemsControl") {
            pendingPage = "itemscontrol";
        } else if (header == "ComboBox") {
            pendingPage = "combobox";
        } else if (header == "ListBox") {
            pendingPage = "listbox";
        } else if (header == "ListView") {
            pendingPage = "listview";
        } else if (header == "TreeView") {
            pendingPage = "treeview";
        }
    }

    void OnSelectorChanged(
        Base::Object* sender,
        const RoutedEventArgs&) noexcept {
        if (sender != selectorTree ||
            selectorTree == nullptr) {
            return;
        }
        Base::Ref<Base::Object> selected =
            selectorTree->SelectedItem();
        if (!selected ||
            !selectorTree->PropertyRegistry().Types().
                IsDerivedFrom(
                    selected->RuntimeType(),
                    TreeViewItem::StaticTypeId())) {
            return;
        }
        QueuePageForItem(
            static_cast<TreeViewItem*>(
                selected.Get()));
        if (portraitLayout &&
            !pendingPage.empty()) {
            navigationOpen = false;
            Result<void> collapsed =
                ApplyResponsiveLayout(
                    layoutWidth,
                    layoutHeight);
            if (!collapsed) {
                std::fprintf(
                    stderr,
                    "ControlGallery navigation collapse failed: %s\n",
                    collapsed.GetStatus().message);
            }
        }
    }

    void OnSelectorMouseDown(
        Base::Object* sender,
        const MouseButtonEventArgs& args) noexcept {
        if (sender != selectorTree ||
            selectorTree == nullptr ||
            args.changedButton != MouseButton::Left ||
            args.originalSource == nullptr ||
            !selectorTree->PropertyRegistry().Types().
                IsDerivedFrom(
                    args.originalSource->RuntimeType(),
                    UIElement::StaticTypeId())) {
            return;
        }
        Visual* current =
            static_cast<UIElement*>(
                args.originalSource);
        while (current != nullptr &&
               current != selectorTree) {
            UIElement* element =
                current->AsUIElement();
            if (element != nullptr &&
                selectorTree->PropertyRegistry().Types().
                    IsDerivedFrom(
                        element->RuntimeType(),
                        TreeViewItem::StaticTypeId())) {
                QueuePageForItem(
                    static_cast<TreeViewItem*>(
                        element));
                return;
            }
            current = current->VisualParent();
        }
    }

    Result<void> ApplyResponsiveLayout(
        double width,
        double height) noexcept {
        if (galleryLayoutRoot == nullptr ||
            navigationPanel == nullptr ||
            selectorExpanderButton == nullptr ||
            !std::isfinite(width) ||
            !std::isfinite(height) ||
            width <= 0.0 ||
            height <= 0.0) {
            return Failure(
                "ControlGallery responsive layout endpoints are invalid");
        }

        const bool nextPortrait = width <= height;
        if (nextPortrait != portraitLayout) {
            navigationOpen = !nextPortrait;
        }
        portraitLayout = nextPortrait;
        if (!portraitLayout) {
            navigationOpen = true;
        }
        layoutWidth = width;
        layoutHeight = height;

        const bool showNavigation =
            !portraitLayout || navigationOpen;
        const double rootScale =
            layoutDpiScale *
            (portraitLayout
                ? std::max(
                      1.0,
                      height / 720.0)
                : std::max(
                      1.0,
                      width / 1280.0));
        Result<void> changed =
            galleryRootScale->SetScaleX(
                rootScale);
        if (changed) {
            changed =
                galleryRootScale->SetScaleY(
                    rootScale);
        }
        GridLength columns[] = {
            GridLength::Pixel(
                showNavigation ? 215.0 : 0.0),
            GridLength::Star()};
        if (changed) {
            changed =
                galleryLayoutRoot->
                    SetColumnDefinitions(
                        columns);
        }
        if (changed) {
            changed = navigationPanel->SetVisibility(
                showNavigation
                    ? Visibility::Visible
                    : Visibility::Collapsed);
        }
        if (changed) {
            changed =
                selectorExpanderButton->SetVisibility(
                    portraitLayout
                        ? Visibility::Visible
                        : Visibility::Collapsed);
        }
        return changed;
    }

    Result<void> ApplyDpiScale(
        Visual& visual,
        double scale) noexcept {
        FrameworkElement* element =
            visual.AsFrameworkElement();
        if (element != nullptr) {
            Result<void> updated =
                element->SetLayoutRounding(
                    element->UseLayoutRounding(),
                    scale);
            if (!updated) {
                return updated.GetStatus();
            }
        }
        for (Visual* child : visual.VisualChildren()) {
            if (child == nullptr) continue;
            Result<void> updated =
                ApplyDpiScale(*child, scale);
            if (!updated) {
                return updated.GetStatus();
            }
        }
        return {};
    }

    void OnSelectorExpanderClick(
        Base::Object* sender,
        const RoutedEventArgs&) noexcept {
        if (sender != selectorExpanderButton ||
            !portraitLayout) {
            return;
        }
        navigationOpen = !navigationOpen;
        Result<void> changed =
            ApplyResponsiveLayout(
                layoutWidth,
                layoutHeight);
        if (!changed) {
            std::fprintf(
                stderr,
                "ControlGallery navigation toggle failed: %s\n",
                changed.GetStatus().message);
        }
    }

    Result<void> ConfigureNavigation() noexcept {
        selectorTree =
            Runtime().FindNamed<TreeView>(
                "SelectorTree");
        if (selectorTree == nullptr) {
            return Failure(
                "ControlGallery selector tree is missing");
        }
        Result<void> selected =
            selectorTree->TryAddHandler(
            TreeView::SelectedItemChangedEvent,
            selectorChangedHandler);
        if (!selected) return selected.GetStatus();
        Result<void> clicked =
            selectorTree->TryAddHandler(
                UIElement::MouseDownEvent,
                selectorMouseDownHandler,
                true);
        if (!clicked) {
            static_cast<void>(
                selectorTree->RemoveHandler(
                    TreeView::SelectedItemChangedEvent,
                    selectorChangedHandler));
            return clicked.GetStatus();
        }
        return {};
    }

    Result<void> ConfigureResponsiveLayout() noexcept {
        galleryLayoutRoot =
            Runtime().FindNamed<Grid>(
                "GalleryLayoutRoot");
        navigationPanel =
            Runtime().FindNamed<UIElement>(
                "NavigationPanel");
        selectorExpanderButton =
            Runtime().FindNamed<Button>(
                "SelectorExpanderButton");
        Base::Ref<Transform> rootTransform =
            galleryLayoutRoot != nullptr
            ? galleryLayoutRoot->
                  LayoutTransform()
            : Base::Ref<Transform>{};
        if (rootTransform &&
            rootTransform->RuntimeType() ==
                ScaleTransform::StaticTypeId()) {
            galleryRootScale =
                static_cast<ScaleTransform*>(
                    rootTransform.Get());
        }
        if (galleryLayoutRoot == nullptr ||
            galleryRootScale == nullptr ||
            navigationPanel == nullptr ||
            selectorExpanderButton == nullptr) {
            return Failure(
                "ControlGallery responsive layout endpoints are missing");
        }
        Result<void> clicked =
            selectorExpanderButton->TryAddHandler(
                ButtonBase::ClickEvent,
                selectorExpanderClickHandler);
        if (!clicked) return clicked.GetStatus();
        return ApplyResponsiveLayout(
            layoutWidth,
            layoutHeight);
    }

    Result<bool> NavigatePendingPage() noexcept {
        if (pendingPage.empty()) return false;
        std::string page =
            std::move(pendingPage);
        pendingPage.clear();
        Result<void> selected = SelectPage(
            Base::StringView(
                page.data(),
                static_cast<std::uint32_t>(
                    page.size())));
        return selected
            ? Result<bool>(true)
            : Result<bool>(
                  selected.GetStatus());
    }

    Result<void> ExecuteScenario(
        GalleryScenario requested,
        std::uint32_t frameCount) noexcept {
        if (!view || frameCount == 0U) {
            return Failure(
                "ControlGallery scenario requires an initialized view and frames");
        }
        snapshot.scenario = requested;
        double pausedAnimationValue = 0.0;
        for (std::uint32_t frameIndex = 0U;
             frameIndex < frameCount;
             ++frameIndex) {
            if (requested == GalleryScenario::Interaction) {
                if (configuredPage == "brushes") {
                    Slider* offset =
                        Runtime().FindNamed<Slider>(
                            "BrushOffset");
                    GradientStop* stop =
                        Runtime().FindNamed<
                            GradientStop>(
                                "BrushGradientStop");
                    if (offset == nullptr ||
                        stop == nullptr) {
                        return Failure(
                            "ControlGallery brush interaction targets are missing");
                    }
                    if (frameIndex == 0U) {
                        Result<bool> changed =
                            offset->SetValue(0.65);
                        if (!changed) {
                            return changed.GetStatus();
                        }
                    } else if (frameIndex == 1U) {
                        if (std::fabs(
                                stop->Offset() -
                                0.65) > 0.0001) {
                            return Failure(
                                "ControlGallery GradientStop binding did not update");
                        }
                    }
                }
                if (configuredPage == "effects") {
                    Slider* radius =
                        Runtime().FindNamed<Slider>(
                            "EffectBlurRadius");
                    UIElement* sample =
                        Runtime().FindNamed<UIElement>(
                            "EffectsPageSample");
                    if (radius == nullptr ||
                        sample == nullptr) {
                        return Failure(
                            "ControlGallery effect interaction targets are missing");
                    }
                    if (frameIndex == 0U) {
                        Result<bool> changed =
                            radius->SetValue(24.0);
                        if (!changed) {
                            return changed.GetStatus();
                        }
                    } else if (frameIndex == 1U) {
                        Ref<Effect> effect =
                            sample->GetEffect();
                        if (!effect ||
                            effect->RuntimeType() !=
                                BlurEffect::StaticTypeId() ||
                            std::fabs(
                                static_cast<BlurEffect*>(
                                    effect.Get())->Radius() -
                                24.0) > 0.0001) {
                            return Failure(
                                "ControlGallery BlurEffect binding did not update");
                        }
                    }
                }
                if (configuredPage == "slider") {
                    Slider* slider =
                        Runtime().FindNamed<Slider>(
                            "SliderSample");
                    if (slider == nullptr) {
                        return Failure(
                            "ControlGallery slider interaction target is missing");
                    }
                    if (frameIndex == 0U) {
                        Result<void> clicked =
                            ClickElement(
                                *slider, 93U);
                        if (!clicked) {
                            return clicked.GetStatus();
                        }
                        if (slider->Value() <= 35.0) {
                            return Failure(
                                "ControlGallery slider pointer interaction did not increase Value");
                        }
                    } else if (frameIndex == 1U) {
                        const double before =
                            slider->Value();
                        KeyboardInput input;
                        input.action =
                            KeyboardAction::Down;
                        input.key =
                            KeyboardKeyRight;
                        Result<KeyboardDispatchResult>
                            dispatched =
                                Runtime().
                                    DispatchKeyboard(
                                        input);
                        if (!dispatched) {
                            return dispatched.
                                GetStatus();
                        }
                        if (slider->Value() <= before) {
                            return Failure(
                                "ControlGallery slider keyboard interaction did not increase Value");
                        }
                    }
                }
                if (configuredPage ==
                        "togglebutton") {
                    ToggleButton* largeToggle =
                        Runtime().FindNamed<
                            ToggleButton>(
                            "LargeToggleSample");
                    if (largeToggle == nullptr) {
                        return Failure(
                            "ControlGallery LayoutTransform toggle target is missing");
                    }
                    if (frameIndex == 0U ||
                        frameIndex == 30U) {
                        const bool before =
                            largeToggle->
                                IsChecked();
                        Result<void> clicked =
                            ClickElement(
                                *largeToggle,
                                104U + frameIndex);
                        if (!clicked) {
                            return clicked.
                                GetStatus();
                        }
                        if (largeToggle->
                                IsChecked() ==
                            before) {
                            return Failure(
                                "ControlGallery LayoutTransform toggle interaction did not change state");
                        }
                    }
                }
                if (configuredPage == "animation") {
                    Button* animationButton =
                        Runtime().FindNamed<Button>(
                            "AnimationPositionButton");
                    Rectangle* animationRectangle =
                        Runtime().FindNamed<Rectangle>(
                            "AnimationRect1");
                    Button* colorFramesButton =
                        Runtime().FindNamed<Button>(
                            "AnimationColorFramesButton");
                    Rectangle* gradientRectangle =
                        Runtime().FindNamed<Rectangle>(
                            "AnimationGradientRect");
                    Button* pointButton =
                        Runtime().FindNamed<Button>(
                            "AnimationPointButton");
                    Rectangle* pointRectangle =
                        Runtime().FindNamed<Rectangle>(
                            "AnimationPointRect");
                    Button* valueButton =
                        Runtime().FindNamed<Button>(
                            "AnimationValueButton");
                    Rectangle* thicknessTarget =
                        Runtime().FindNamed<Rectangle>(
                            "AnimationThicknessTarget");
                    Button* controlStart =
                        Runtime().FindNamed<Button>(
                            "AnimationControlStart");
                    Button* controlPause =
                        Runtime().FindNamed<Button>(
                            "AnimationControlPause");
                    Button* controlResume =
                        Runtime().FindNamed<Button>(
                            "AnimationControlResume");
                    Button* controlStop =
                        Runtime().FindNamed<Button>(
                            "AnimationControlStop");
                    Button* controlSeek =
                        Runtime().FindNamed<Button>(
                            "AnimationControlSeek");
                    Button* controlRemove =
                        Runtime().FindNamed<Button>(
                            "AnimationControlRemove");
                    Rectangle* controlRectangle =
                        Runtime().FindNamed<Rectangle>(
                            "AnimationControlRect");
                    if (animationButton == nullptr ||
                        animationRectangle == nullptr ||
                        colorFramesButton == nullptr ||
                        gradientRectangle == nullptr ||
                        pointButton == nullptr ||
                        pointRectangle == nullptr ||
                        valueButton == nullptr ||
                        thicknessTarget == nullptr ||
                        controlStart == nullptr ||
                        controlPause == nullptr ||
                        controlResume == nullptr ||
                        controlStop == nullptr ||
                        controlSeek == nullptr ||
                        controlRemove == nullptr ||
                        controlRectangle == nullptr) {
                        return Failure(
                            "ControlGallery animation interaction targets are missing");
                    }
                    if (frameIndex == 0U) {
                        Result<void> clicked =
                            ClickElement(
                                *animationButton, 92U);
                        if (!clicked) {
                            return clicked.GetStatus();
                        }
                        clicked = ClickElement(
                            *colorFramesButton, 93U);
                        if (!clicked) {
                            return clicked.GetStatus();
                        }
                    } else if (frameIndex == 1U) {
                        Ref<Transform> transform =
                            animationRectangle->
                                RenderTransform();
                        if (!transform ||
                            transform->RuntimeType() !=
                                TranslateTransform::
                                    StaticTypeId() ||
                            static_cast<
                                TranslateTransform*>(
                                    transform.Get())->X() <=
                                0.0) {
                            return Failure(
                                "ControlGallery Storyboard click did not advance the rectangle transform");
                        }
                        const Color fill =
                            animationRectangle->Fill();
                        if (fill.red < 0.9F ||
                            fill.green > 0.6F) {
                            return Failure(
                                "ControlGallery ColorAnimationUsingKeyFrames did not apply");
                        }
                        Ref<Brush> gradientFill =
                            gradientRectangle->FillBrush();
                        LinearGradientBrush* gradient =
                            gradientFill &&
                            gradientFill->RuntimeType() ==
                                LinearGradientBrush::StaticTypeId()
                            ? static_cast<
                                  LinearGradientBrush*>(
                                  gradientFill.Get())
                            : nullptr;
                        if (gradient == nullptr ||
                            gradient->GradientStops().Empty() ||
                            !gradient->GradientStops()[0] ||
                            gradient->GradientStops()[0]->
                                GetColor().red <= 0.12F) {
                            return Failure(
                                "ControlGallery indexed GradientStops animation did not apply");
                        }
                    } else if (frameIndex == 2U) {
                        Result<void> clicked =
                            ClickElement(
                                *pointButton, 94U);
                        if (!clicked) {
                            return clicked.GetStatus();
                        }
                    } else if (frameIndex == 3U) {
                        const Point origin =
                            pointRectangle->
                                RenderTransformOrigin();
                        if (origin.x <= 0.0 ||
                            origin.y <= 0.0) {
                            return Failure(
                                "ControlGallery PointAnimation did not advance RenderTransformOrigin");
                        }
                    } else if (frameIndex == 4U) {
                        Result<void> clicked =
                            ClickElement(
                                *valueButton, 95U);
                        if (!clicked) {
                            return clicked.GetStatus();
                        }
                    } else if (frameIndex == 5U) {
                        const Thickness margin =
                            thicknessTarget->Margin();
                        Ref<Brush> fill =
                            thicknessTarget->FillBrush();
                        ImageBrush* viewboxBrush =
                            fill &&
                            fill->RuntimeType() ==
                                ImageBrush::StaticTypeId()
                            ? static_cast<ImageBrush*>(
                                fill.Get())
                            : nullptr;
                        if (viewboxBrush == nullptr) {
                            return Failure(
                                "ControlGallery RectAnimation brush target is missing");
                        }
                        const Rect viewbox =
                            viewboxBrush->Viewbox();
                        if (margin.left <= 0.0 ||
                            viewbox.x <= 0.0 ||
                            viewbox.width >= 1.0) {
                            return Failure(
                                "ControlGallery RectAnimation or ThicknessAnimation did not advance");
                        }
                    } else if (frameIndex == 6U) {
                        Result<void> clicked =
                            ClickElement(
                                *controlStart, 96U);
                        if (!clicked) {
                            return clicked.GetStatus();
                        }
                    } else if (frameIndex == 7U) {
                        Ref<Transform> transform =
                            controlRectangle->
                                RenderTransform();
                        if (!transform ||
                            transform->RuntimeType() !=
                                TranslateTransform::
                                    StaticTypeId()) {
                            return Failure(
                                "ControlGallery controllable Storyboard transform is missing");
                        }
                        const double runningValue =
                            static_cast<
                                TranslateTransform*>(
                                    transform.Get())->X();
                        if (runningValue <= 0.0) {
                            return Failure(
                                "ControlGallery controllable Storyboard did not start");
                        }
                        Result<void> clicked =
                            ClickElement(
                                *controlPause, 97U);
                        if (!clicked) {
                            return clicked.GetStatus();
                        }
                        pausedAnimationValue =
                            static_cast<
                                TranslateTransform*>(
                                    controlRectangle->
                                        RenderTransform().
                                        Get())->X();
                    } else if (frameIndex == 8U) {
                        const double current =
                            static_cast<
                                TranslateTransform*>(
                                    controlRectangle->
                                        RenderTransform().
                                        Get())->X();
                        if (std::fabs(
                                current -
                                pausedAnimationValue) >
                            0.0001) {
                            return Failure(
                                "ControlGallery PauseStoryboard did not hold the clock");
                        }
                        Result<void> clicked =
                            ClickElement(
                                *controlSeek, 98U);
                        if (!clicked) {
                            return clicked.GetStatus();
                        }
                        clicked = ClickElement(
                            *controlResume, 100U);
                        if (!clicked) {
                            return clicked.GetStatus();
                        }
                    } else if (frameIndex == 9U) {
                        const double current =
                            static_cast<
                                TranslateTransform*>(
                                    controlRectangle->
                                        RenderTransform().
                                        Get())->X();
                        if (current < 140.0) {
                            return Failure(
                                "ControlGallery SeekStoryboard or ResumeStoryboard did not advance the clock");
                        }
                        Result<void> clicked =
                            ClickElement(
                                *controlStop, 99U);
                        if (!clicked) {
                            return clicked.GetStatus();
                        }
                    } else if (frameIndex == 10U) {
                        const double current =
                            static_cast<
                                TranslateTransform*>(
                                    controlRectangle->
                                        RenderTransform().
                                        Get())->X();
                        if (std::fabs(current) >
                            0.0001) {
                            return Failure(
                                "ControlGallery StopStoryboard did not restore the base value");
                        }
                        Result<void> clicked =
                            ClickElement(
                                *controlStart, 101U);
                        if (!clicked) {
                            return clicked.GetStatus();
                        }
                    } else if (frameIndex == 11U) {
                        const double current =
                            static_cast<
                                TranslateTransform*>(
                                    controlRectangle->
                                        RenderTransform().
                                        Get())->X();
                        if (current <= 0.0) {
                            return Failure(
                                "ControlGallery controllable Storyboard restart failed");
                        }
                        Result<void> clicked =
                            ClickElement(
                                *controlRemove, 102U);
                        if (!clicked) {
                            return clicked.GetStatus();
                        }
                    } else if (frameIndex == 12U) {
                        const double current =
                            static_cast<
                                TranslateTransform*>(
                                    controlRectangle->
                                        RenderTransform().
                                        Get())->X();
                        if (std::fabs(current) >
                            0.0001) {
                            return Failure(
                                "ControlGallery RemoveStoryboard did not restore the base value");
                        }
                    }
                }
                TextBox* input =
                    Runtime().FindNamed<TextBox>("Input");
                CheckBox* check =
                    Runtime().FindNamed<CheckBox>("FeatureCheck");
                ComboBox* combo =
                    Runtime().FindNamed<ComboBox>(
                        "ThemeColorPicker");
                TreeViewItem* treeItem =
                    Runtime().FindNamed<
                        TreeViewItem>(
                            "TreeLevel1");
                MenuItem* menuItem =
                    Runtime().FindNamed<MenuItem>(
                        "MenuViewItem");
                MenuItem* submenuItem =
                    Runtime().FindNamed<MenuItem>(
                        "MenuNewItem");
                MenuItem* contextSubmenuItem =
                    Runtime().FindNamed<MenuItem>(
                        "ContextViewItem");
                ContextMenu* contextMenu =
                    Runtime().FindNamed<ContextMenu>(
                        "SampleContextMenu");
                ListView* products =
                    Runtime().FindNamed<ListView>(
                        "ProductsList");
                ToolTip* toolTip =
                    Runtime().FindNamed<ToolTip>(
                        "SampleToolTip");
                Button* serviceTarget =
                    Runtime().FindNamed<Button>(
                        "PrimaryButton");
                if (input == nullptr || check == nullptr ||
                    combo == nullptr ||
                    treeItem == nullptr ||
                    menuItem == nullptr ||
                    submenuItem == nullptr ||
                    contextSubmenuItem == nullptr ||
                    contextMenu == nullptr ||
                    products == nullptr ||
                    toolTip == nullptr ||
                    serviceTarget == nullptr) {
                    return Failure(
                        "ControlGallery interaction targets are missing");
                }
                char text[64] = {};
                const int length = std::snprintf(
                    text, sizeof(text),
                    "Interaction frame %u", frameIndex);
                if (length <= 0) {
                    return Failure(
                        "ControlGallery interaction text formatting failed");
                }
                Result<void> changed = input->SetText(
                    Base::StringView(
                        text,
                        static_cast<std::uint32_t>(length)));
                if (changed) {
                    changed = check->SetIsChecked(
                        (frameIndex & 1U) != 0U);
                }
                if (changed) {
                    Result<bool> selected =
                        combo->SetSelectedIndex(
                            frameIndex %
                            combo->ItemCount());
                    if (!selected) {
                        return selected.GetStatus();
                    }
                    changed =
                        combo->SetIsDropDownOpen(
                            (frameIndex & 1U) != 0U);
                }
                if (changed) {
                    changed =
                        treeItem->SetIsExpanded(
                            (frameIndex & 1U) == 0U);
                }
                if (changed) {
                    changed =
                        menuItem->SetIsChecked(
                            (frameIndex & 1U) == 0U);
                }
                if (changed) {
                    changed =
                        submenuItem->SetIsExpanded(
                            (frameIndex & 1U) == 0U);
                }
                if (changed) {
                    changed =
                        contextSubmenuItem->
                            SetIsExpanded(
                                (frameIndex & 1U) ==
                                    0U);
                }
                if (changed) {
                    changed =
                        contextMenu->SetIsOpen(
                            (frameIndex & 1U) != 0U);
                }
                if (changed) {
                    Result<bool> selected =
                        products->SetSelectedIndex(
                            frameIndex %
                            products->ItemCount());
                    if (!selected) {
                        return selected.GetStatus();
                    }
                    changed =
                        toolTip->SetIsOpen(
                            (frameIndex & 1U) != 0U);
                }
                if (!changed) return changed.GetStatus();
                if (frameIndex == 0U &&
                    (configuredPage == "home" ||
                     configuredPage == "buttons" ||
                     configuredPage == "button") &&
                    serviceTarget->RenderSize().width >
                        0.0 &&
                    serviceTarget->RenderSize().height >
                        0.0) {
                    Result<void> settled =
                        RunFrame();
                    if (!settled) {
                        return settled.GetStatus();
                    }
                    UIElement* rootElement =
                        root
                        ? static_cast<Border*>(
                              root.Get())
                        : nullptr;
                    for (std::uint32_t attempt = 0U;
                         attempt < 4U &&
                         (rootElement == nullptr ||
                          !rootElement->
                              IsArrangeValid() ||
                          !serviceTarget->
                              IsArrangeValid());
                         ++attempt) {
                        settled = RunFrame();
                        if (!settled) {
                            return settled.GetStatus();
                        }
                    }
                    if (rootElement == nullptr ||
                        !rootElement->
                            IsArrangeValid() ||
                        !serviceTarget->
                            IsArrangeValid()) {
                        return Failure(
                            "ControlGallery popup service target did not settle layout");
                    }
                    Point origin{};
                    Visual* current =
                        serviceTarget;
                    while (current != nullptr) {
                        UIElement* element =
                            current->AsUIElement();
                        if (element != nullptr) {
                            const Rect slot =
                                element->LayoutSlot();
                            origin.x += slot.x;
                            origin.y += slot.y;
                        }
                        current =
                            current->VisualParent();
                    }
                    const Point center = {
                        origin.x +
                            serviceTarget->
                                RenderSize().width *
                                0.5,
                        origin.y +
                            serviceTarget->
                                RenderSize().height *
                                0.5};
                    PointerInput pointerInput;
                    pointerInput.pointerId = 91U;
                    pointerInput.action =
                        PointerAction::Move;
                    pointerInput.position = center;
                    Result<PointerDispatchResult>
                        hovered =
                            Runtime().
                                DispatchPointer(
                                    pointerInput);
                    if (!hovered) {
                        std::fprintf(
                            stderr,
                            "ControlGallery popup service hover failed: %s\n",
                            hovered.GetStatus().message);
                        return hovered.GetStatus();
                    }
                    Result<std::uint32_t> advanced =
                        Runtime().AdvanceTime(50U);
                    if (!advanced) {
                        return advanced.GetStatus();
                    }
                    if (!toolTip->IsOpen()) {
                        return Failure(
                            "ToolTipService delay did not open the tooltip");
                    }
                    settled = RunFrame();
                    if (!settled) {
                        return settled.GetStatus();
                    }
                    pointerInput.action =
                        PointerAction::Down;
                    pointerInput.changedButton =
                        MouseButton::Right;
                    Result<PointerDispatchResult>
                        opened =
                            Runtime().
                                DispatchPointer(
                                    pointerInput);
                    if (!opened) {
                        std::fprintf(
                            stderr,
                            "ControlGallery context service open failed: %s\n",
                            opened.GetStatus().message);
                        return opened.GetStatus();
                    }
                    if (!contextMenu->IsOpen() ||
                        toolTip->IsOpen()) {
                        return Failure(
                            "Popup services did not transition on right click");
                    }
                }
            } else if (requested == GalleryScenario::Scroll) {
                items.ExerciseChange(frameIndex);
                const double extent =
                    static_cast<double>(items.Count()) * 24.0;
                const double offset = std::fmod(
                    static_cast<double>(frameIndex) * 211.0,
                    std::max(1.0, extent - 120.0));
                Result<bool> scrolled =
                    virtualizingPanel->SetVerticalOffset(offset);
                if (!scrolled) return scrolled.GetStatus();
            }
            Result<void> frame = RunFrame();
            if (!frame) return frame.GetStatus();
            if (configuredPage == "togglebutton" &&
                (frameIndex == 0U ||
                 (requested == GalleryScenario::Interaction &&
                  frameIndex == 30U))) {
                ToggleButton* largeToggle =
                    Runtime().FindNamed<ToggleButton>(
                        "LargeToggleSample");
                if (largeToggle == nullptr) {
                    return Failure(
                        "ControlGallery toggle visual-state validation endpoints are missing");
                }
                const bool expectedChecked =
                    requested != GalleryScenario::Interaction ||
                    frameIndex == 30U;
                if (largeToggle->IsChecked() !=
                    expectedChecked) {
                    return Failure(
                        "ControlGallery toggle property state diverged after rendering");
                }
            }
        }
        if (requested == GalleryScenario::Scroll &&
            items.Count() != 10000U) {
            items.ExerciseChange(4U);
            Result<void> frame = RunFrame();
            if (!frame) return frame.GetStatus();
        }
        return {};
    }

    Result<void> RunScenario(
        GalleryScenario requested,
        std::uint32_t frameCount) noexcept {
        configuredScenario = requested;
        configuredFrameCount = frameCount;
        return ExecuteScenario(requested, frameCount);
    }

    Result<void> SynchronizeNavigation(
        Base::StringView header) noexcept {
        if (selectorTree == nullptr) {
            return Failure(
                "ControlGallery selector tree is missing");
        }
        TreeViewItem* target = nullptr;
        TreeViewItem* targetCategory = nullptr;
        for (std::uint32_t rootIndex = 0U;
             rootIndex < selectorTree->ItemCount();
             ++rootIndex) {
            Base::Ref<Base::Object> rootObject =
                selectorTree->ItemAt(rootIndex);
            if (!rootObject ||
                !selectorTree->PropertyRegistry().
                    Types().IsDerivedFrom(
                        rootObject->RuntimeType(),
                        TreeViewItem::StaticTypeId())) {
                continue;
            }
            auto* category =
                static_cast<TreeViewItem*>(
                    rootObject.Get());
            if (category->Header() == header) {
                target = category;
                targetCategory = category;
            }
            for (std::uint32_t itemIndex = 0U;
                 itemIndex < category->Count();
                 ++itemIndex) {
                Base::Ref<Base::Object> itemObject =
                    category->ItemAt(itemIndex);
                if (!itemObject ||
                    !selectorTree->PropertyRegistry().
                        Types().IsDerivedFrom(
                            itemObject->RuntimeType(),
                            TreeViewItem::StaticTypeId())) {
                    continue;
                }
                auto* item =
                    static_cast<TreeViewItem*>(
                        itemObject.Get());
                if (item->Header() == header) {
                    target = item;
                    targetCategory = category;
                    break;
                }
            }
        }
        if (!header.Empty() && target == nullptr) {
            return Failure(
                "ControlGallery navigation item is missing");
        }
        for (std::uint32_t rootIndex = 0U;
             rootIndex < selectorTree->ItemCount();
             ++rootIndex) {
            Base::Ref<Base::Object> rootObject =
                selectorTree->ItemAt(rootIndex);
            if (!rootObject ||
                !selectorTree->PropertyRegistry().
                    Types().IsDerivedFrom(
                        rootObject->RuntimeType(),
                        TreeViewItem::StaticTypeId())) {
                continue;
            }
            auto* category =
                static_cast<TreeViewItem*>(
                    rootObject.Get());
            Result<void> expanded =
                category->SetIsExpanded(
                    category == targetCategory);
            if (!expanded) {
                return expanded.GetStatus();
            }
        }
        Result<bool> selected =
            selectorTree->SelectItem(target);
        return selected
            ? Result<void>{}
            : Result<void>(selected.GetStatus());
    }

    Result<void> SelectPage(
        Base::StringView page) noexcept {
        struct PageRecord final {
            Base::StringView id;
            Base::StringView pageName;
            Base::StringView requiredName;
            Base::StringView navigationHeader;
        };
        constexpr PageRecord registry[] = {
            {"home", "HomePage", "SelectorTree", ""},
            {"layout", "GridPage", "LayoutGrid", "Grid"},
            {"canvas", "CanvasPage", "LayoutCanvas", "Canvas"},
            {"stackpanel", "StackPanelPage", "LayoutStack", "StackPanel"},
            {"wrappanel", "WrapPanelPage", "LayoutWrapPage", "WrapPanel"},
            {"dockpanel", "DockPanelPage", "LayoutDock", "DockPanel"},
            {"grid", "GridPage", "LayoutGrid", "Grid"},
            {"uniformgrid", "UniformGridPage", "LayoutUniformPage", "UniformGrid"},
            {"content", "ContentPage", "ContentGroup", "GroupBox"},
            {"buttons", "ButtonsPage", "PrimaryButton", "Button"},
            {"button", "ButtonsPage", "PrimaryButton", "Button"},
            {"repeatbutton", "RepeatButtonPage", "RepeatSampleButton", "RepeatButton"},
            {"togglebutton", "ToggleButtonPage", "ToggleSampleButton", "ToggleButton"},
            {"checkbox", "CheckBoxPage", "CheckBoxSample", "CheckBox"},
            {"radiobutton", "RadioButtonPage", "RadioButtonPage", "RadioButton"},
            {"text", "TextBlockPage", "TextWrapSelector", "TextBlock"},
            {"textblock", "TextBlockPage", "TextWrapSelector", "TextBlock"},
            {"textbox", "TextBoxPage", "Input", "TextBox"},
            {"passwordbox", "PasswordBoxPage", "PasswordInput", "PasswordBox"},
            {"hyperlink", "HyperlinkPage", "AeroHomeLink", "Hyperlink"},
            {"range", "RangePage", "RangeSlider", "Slider"},
            {"slider", "SliderPage", "SliderSample", "Slider"},
            {"progressbar", "ProgressBarPage", "ProgressBarPageSample", "ProgressBar"},
            {"groupbox", "GroupBoxPage", "GroupBoxSample", "GroupBox"},
            {"expander", "ExpanderPage", "ExpanderSample", "Expander"},
            {"items", "ItemsPage", "BigList", "ListBox"},
            {"itemscontrol", "ItemsControlPage", "ItemsControlSample", "ItemsControl"},
            {"combobox", "ComboBoxPage", "ComboBoxSample", "ComboBox"},
            {"listbox", "ListBoxPage", "ListBoxSample", "ListBox"},
            {"listview", "ListViewPage", "ListViewSample", "ListView"},
            {"treeview", "TreeViewPage", "TreeViewSample", "TreeView"},
            {"tabcontrol", "TabControlPage", "TabControlSample", "TabControl"},
            {"scrolling", "ScrollingPage", "Scroller", "ScrollViewer"},
            {"scrollviewer", "ScrollViewerPage", "ScrollViewerSample", "ScrollViewer"},
            {"menu", "MenuPage", "MenuPageSample", "Menu"},
            {"contextmenu", "ContextMenuPage", "ContextMenuPageSample", "ContextMenu"},
            {"toolbar", "ToolBarPage", "ToolBarPageSample", "ToolBar"},
            {"statusbar", "StatusBarPage", "StatusBarPageSample", "StatusBar"},
            {"tooltip", "ToolTipPage", "ToolTipPageSample", "ToolTip"},
            {"image", "ImagePage", "ImagePageSample", "Image"},
            {"brushes", "BrushesPage", "BrushesPageSample", "Brushes"},
            {"effects", "EffectsPage", "EffectsPageSample", "Effects"},
            {"blending", "BlendingPage", "BlendingPageSample", "Blending"},
            {"animation", "AnimationPage", "AnimationRect1", "Animation"},
        };
        auto select = [this, &registry](
            const PageRecord& record)
            -> Result<void> {
            if (Runtime().FindNamedObject(
                    record.requiredName,
                    Core::InvalidTypeId) == nullptr) {
                return Failure(
                    "ControlGallery page capability target is missing");
            }
            for (const PageRecord& candidate : registry) {
                UIElement* pageElement =
                    Runtime().FindNamed<UIElement>(
                        candidate.pageName);
                if (pageElement == nullptr) {
                    return Failure(
                        "ControlGallery page visual is missing");
                }
                Result<void> changed =
                    pageElement->SetVisibility(
                        candidate.pageName == record.pageName
                            ? Visibility::Visible
                            : Visibility::Collapsed);
                if (!changed) return changed.GetStatus();
            }
            Result<void> navigation =
                SynchronizeNavigation(
                    record.navigationHeader);
            if (!navigation) {
                return navigation.GetStatus();
            }
            return RunFrame();
        };
        if (page == Base::StringView("all")) {
            for (const PageRecord& record : registry) {
                if (Runtime().FindNamedObject(
                        record.requiredName,
                        Core::InvalidTypeId) == nullptr ||
                    Runtime().FindNamed<UIElement>(
                        record.pageName) == nullptr) {
                    return Failure(
                        "ControlGallery page registry is incomplete");
                }
            }
            constexpr std::uint32_t pageCount =
                static_cast<std::uint32_t>(
                    sizeof(registry) /
                    sizeof(registry[0U]));
            for (std::uint32_t index = 0U;
                 index < pageCount;
                 ++index) {
                bool alreadyRendered = false;
                for (std::uint32_t prior = 0U;
                     prior < index;
                     ++prior) {
                    if (registry[prior].pageName ==
                        registry[index].pageName) {
                        alreadyRendered = true;
                        break;
                    }
                }
                if (alreadyRendered) continue;
                Result<void> rendered =
                    select(registry[index]);
                if (!rendered) return rendered.GetStatus();
            }
            Result<void> selected = select(registry[0U]);
            if (selected) configuredPage = "all";
            return selected;
        }
        for (const PageRecord& record : registry) {
            if (page != record.id) continue;
            Result<void> selected = select(record);
            if (selected) {
                configuredPage.assign(
                    record.id.Data(),
                    record.id.SizeBytes());
            }
            return selected;
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ControlGallery page id is invalid");
    }

    void CleanupView() noexcept {
        if (selectorTree != nullptr) {
            static_cast<void>(
                selectorTree->RemoveHandler(
                    TreeView::SelectedItemChangedEvent,
                    selectorChangedHandler));
            static_cast<void>(
                selectorTree->RemoveHandler(
                    UIElement::MouseDownEvent,
                    selectorMouseDownHandler));
        }
        if (selectorExpanderButton != nullptr) {
            static_cast<void>(
                selectorExpanderButton->RemoveHandler(
                    ButtonBase::ClickEvent,
                    selectorExpanderClickHandler));
        }
        selectorTree = nullptr;
        galleryLayoutRoot = nullptr;
        galleryRootScale = nullptr;
        navigationPanel = nullptr;
        selectorExpanderButton = nullptr;
        portraitLayout = false;
        navigationOpen = true;
        layoutWidth = 1280.0;
        layoutHeight = 768.0;
        layoutDpiScale = 1.0;
        pendingPage.clear();
        if (listBox != nullptr) {
            static_cast<void>(
                listBox->SetItemsSource(nullptr));
            listBox->SetItemTemplate(nullptr);
        }
        listBox = nullptr;
        virtualizingPanel = nullptr;
        if (view && Runtime().Root()) {
            static_cast<void>(Runtime().Unmount());
        }
        root.Reset();
        view.Reset();
    }

    Result<void> CreateView() noexcept {
        CleanupView();
        snapshot = {};

        Integration::ViewHostOptions options;
        manualAnimationClock = true;
        options.clipboard = &clipboard;
        options.text.fontSearchRoot =
            StringView(
                assetPath.data(),
                static_cast<std::uint32_t>(
                    assetPath.size()));
        Result<Ref<View>> created =
            Integration::ViewHost::CreateView(
                environment, options);
        if (!created) return created.GetStatus();
        view = std::move(created).Value();

        galleryThemeSource = Integration::SourceProviderAdapter(
            &Impl::LoadGalleryThemeSource,
            this,
            nullptr,
            UINT64_C(0xA3E0C011EC710001));
        Result<void> provider = Integration::ViewHost(Runtime()).
            RegisterSourceProvider(
            galleryThemeSource,
            StringView("pack"),
            StringView("Aero.GUI.Extensions"));
        if (!provider) return provider.GetStatus();

        Result<void> status =
            Runtime().
                SetRenderBatchingEnabledForTesting(
                    configuredBatchingEnabled);
        if (status) {
            status = LoadTheme(assetPath, theme);
            if (!status) {
                std::fprintf(
                    stderr,
                    "AeroControlGallery: theme load failed: %s\n",
                    status.GetStatus().message);
            }
        }
        if (status) {
            status = LoadDocument(assetPath, loadMode);
            if (!status) {
                std::fprintf(
                    stderr,
                    "AeroControlGallery: document load failed: %s\n",
                    status.GetStatus().message);
            }
        }
        if (!status) return status.GetStatus();
        if (!root ||
            root->RuntimeType() !=
                Border::StaticTypeId()) {
            return Failure(
                "ControlGallery root is not a Border");
        }
        status = ConfigureNavigation();
        if (!status) {
            std::fprintf(
                stderr,
                "AeroControlGallery: navigation configuration failed: %s\n",
                status.GetStatus().message);
        }
        if (status) {
            status = ConfigureResponsiveLayout();
            if (!status) {
                std::fprintf(
                    stderr,
                    "AeroControlGallery: responsive layout configuration failed: %s\n",
                    status.GetStatus().message);
            }
        }
        if (status) {
            status = ConfigurePopupServices();
        }
        if (!status) {
            std::fprintf(
                stderr,
                "AeroControlGallery: popup service configuration failed: %s\n",
                status.GetStatus().message);
        }
        if (status) {
            status = ValidateBinding();
            if (!status) {
                std::fprintf(
                    stderr,
                    "AeroControlGallery: binding validation failed: %s\n",
                    status.GetStatus().message);
            }
        }
        if (status) {
            status = ConfigureItems();
            if (!status) {
                std::fprintf(
                    stderr,
                    "AeroControlGallery: item configuration failed: %s\n",
                    status.GetStatus().message);
            }
        }
        if (status) {
            status = RunFrame();
            if (!status) {
                std::fprintf(
                    stderr,
                    "AeroControlGallery: initial frame failed: %s\n",
                status.GetStatus().message);
            }
        }
        if (status) {
            status = SelectPage(
                Base::StringView(
                    configuredPage.data(),
                    static_cast<std::uint32_t>(
                        configuredPage.size())));
            if (!status) {
                std::fprintf(
                    stderr,
                    "AeroControlGallery: page restore failed: %s\n",
                    status.GetStatus().message);
            }
        }
        if (status && configuredFrameCount != 0U) {
            status = ExecuteScenario(
                configuredScenario,
                configuredFrameCount);
        }
        if (!status) return status.GetStatus();

        snapshot.namedObjectCount =
            Runtime().NamedObjectCount();
        snapshot.itemCount = items.Count();
        snapshot.realizedItemCount =
            listBox->RealizedItemCount();
        snapshot.createdContainerCount =
            listBox->CreatedContainerCount();
        snapshot.recycledContainerUseCount =
            listBox->
                RecycledContainerUseCount();
        snapshot.loadMode = loadMode;
        snapshot.theme = theme;

        if (snapshot.nodeCount == 0U ||
            snapshot.commandCount == 0U ||
            snapshot.namedObjectCount < 10U ||
            snapshot.itemCount != 10000U ||
            snapshot.realizedItemCount == 0U ||
            snapshot.realizedItemCount >=
                snapshot.itemCount ||
            snapshot.createdContainerCount > 16U) {
            std::fprintf(
                stderr,
                "ControlGallery metrics: nodes=%u commands=%u names=%u "
                "items=%u realized=%u created=%u\n",
                snapshot.nodeCount,
                snapshot.commandCount,
                snapshot.namedObjectCount,
                snapshot.itemCount,
                snapshot.realizedItemCount,
                snapshot.createdContainerCount);
            return Failure(
                "ControlGallery acceptance metrics are invalid");
        }
        return {};
    }
    void Cleanup() noexcept {
        CleanupView();
    }
};

GalleryRuntime::GalleryRuntime() noexcept = default;

GalleryRuntime::~GalleryRuntime() {
    Shutdown();
}

Result<void> GalleryRuntime::Initialize(
    StringView assetDirectory,
    GalleryLoadMode loadMode,
    GalleryTheme requestedTheme) noexcept {
    if (impl_) {
        return Failure(
            "ControlGallery runtime is already initialized");
    }
    if (assetDirectory.Empty()) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "ControlGallery asset directory is empty");
    }

    std::unique_ptr<Impl> state =
        std::make_unique<Impl>();
    state->assetPath.assign(
        assetDirectory.Data(),
        assetDirectory.SizeBytes());
    state->loadMode = loadMode;
    state->theme = requestedTheme;
    Result<void> status = state->environment.Initialize();
    if (status) {
        status = state->CreateView();
    }
    if (!status) return status.GetStatus();

    impl_ = std::move(state);
    return {};
}

void GalleryRuntime::Shutdown() noexcept {
    impl_.reset();
}

const GallerySnapshot&
GalleryRuntime::Snapshot() const noexcept {
    static const GallerySnapshot empty;
    return impl_ ? impl_->snapshot : empty;
}

Base::Result<void> GalleryRuntime::RunScenario(
    GalleryScenario scenario,
    std::uint32_t frameCount) noexcept {
    if (!impl_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ControlGallery runtime is not initialized");
    }
    return impl_->RunScenario(scenario, frameCount);
}

Base::Result<void> GalleryRuntime::SelectPage(
    Base::StringView page) noexcept {
    if (!impl_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ControlGallery runtime is not initialized");
    }
    return impl_->SelectPage(page);
}

Base::Result<void>
GalleryRuntime::SetBatchingEnabledForTesting(
    bool enabled) noexcept {
    if (!impl_ || !impl_->view) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ControlGallery runtime is not initialized");
    }
    impl_->configuredBatchingEnabled = enabled;
    return impl_->Runtime().
        SetRenderBatchingEnabledForTesting(
            enabled);
}

} // namespace Aero::Samples::ControlGallery




