// Included from AeroGuiInternal.hpp inside class AeroGuiInternal.
// Tree hub accessors, layout hot state, input/events, content attach.

    // --- View / ElementTree services ---
    static ElementTree* Tree(const ::Aero::Media::Visual& visual) noexcept {
        return AERO_GET_FIELD(visual, Visual_tree);
    }
    static ElementTree* Tree(const UIElement& element) noexcept {
        return Tree(static_cast<const ::Aero::Media::Visual&>(element));
    }
    static LayoutEngine* LayoutEngineOf(const ::Aero::Media::Visual& visual) noexcept {
        ElementTree* tree = Tree(visual);
        return tree != nullptr ? tree->Layout() : nullptr;
    }
    static LayoutEngine* LayoutEngineOf(const UIElement& element) noexcept {
        return LayoutEngineOf(static_cast<const ::Aero::Media::Visual&>(element));
    }
    static EventRouter* EventRouterOf(const UIElement& element) noexcept {
        ElementTree* tree = Tree(element);
        return tree != nullptr ? tree->Events() : nullptr;
    }
    static InputRouter* InputRouterOf(const UIElement& element) noexcept {
        ElementTree* tree = Tree(element);
        return tree != nullptr ? tree->Input() : nullptr;
    }
    static BindingEngine* BindingEngineOf(const UIElement& element) noexcept {
        ElementTree* tree = Tree(element);
        return tree != nullptr ? tree->Bindings() : nullptr;
    }
    static BindingEngine* BindingEngineOf(const DependencyObject& object) noexcept {
        const UIElement* element = ::Aero::TryCast<UIElement>(&object);
        return element != nullptr ? BindingEngineOf(*element) : nullptr;
    }
    static StyleEngine* StyleEngineOf(const UIElement& element) noexcept {
        ElementTree* tree = Tree(element);
        return tree != nullptr ? tree->Styles() : nullptr;
    }
    static AnimationEngine* AnimationEngineOf(const UIElement& element) noexcept {
        ElementTree* tree = Tree(element);
        return tree != nullptr ? tree->Animations() : nullptr;
    }
    static Controls::TemplateEngine* TemplatesOf(
        const ::Aero::Media::Visual& visual) noexcept {
        ElementTree* tree = Tree(visual);
        return tree != nullptr ? tree->Templates() : nullptr;
    }
    static VisualStateManager* VisualStatesOf(
        const ::Aero::Media::Visual& visual) noexcept {
        ElementTree* tree = Tree(visual);
        return tree != nullptr ? tree->VisualStates() : nullptr;
    }
    static Controls::TextBlockLayout* TextLayoutOf(
        const ::Aero::Media::Visual& visual) noexcept {
        ElementTree* tree = Tree(visual);
        return tree != nullptr ? tree->TextLayout() : nullptr;
    }
    static Aero::Input::IClipboard* ClipboardOf(
        const ::Aero::Media::Visual& visual) noexcept {
        ElementTree* tree = Tree(visual);
        return tree != nullptr ? tree->Clipboard() : nullptr;
    }
    static void SetActiveRepeatButton(
        const ::Aero::Media::Visual& visual,
        Controls::Primitives::RepeatButton* button) noexcept {
        ElementTree* tree = Tree(visual);
        if (tree != nullptr) {
            tree->SetActiveRepeatButton(button);
        }
    }
    static Render::MeshResources* MeshResourcesOf(
        const ::Aero::Media::Visual& visual) noexcept {
        ElementTree* tree = Tree(visual);
        return tree != nullptr ? tree->MeshResources() : nullptr;
    }
    static Base::Object* FindName(
        const UIElement& element,
        Base::StringView name,
        Meta::TypeId expectedType = Meta::InvalidTypeId) noexcept {
        ElementTree* tree = Tree(element);
        return tree != nullptr ? tree->FindName(name, expectedType) : nullptr;
    }
    static ResourceEnvironment ResourceEnvironmentOf(
        const FrameworkElement& element) noexcept {
        ElementTree* tree = Tree(element);
        return tree != nullptr ? tree->GetResourceEnvironment() : ResourceEnvironment{};
    }
    static void* TemplateRuntime(const ::Aero::Media::Visual& visual) noexcept {
        return static_cast<void*>(TemplatesOf(visual));
    }
    static void* MeshResourcesRuntime(const ::Aero::Media::Visual& visual) noexcept {
        return static_cast<void*>(MeshResourcesOf(visual));
    }
    static void* VisualStateRuntime(const ::Aero::Media::Visual& visual) noexcept {
        return static_cast<void*>(VisualStatesOf(visual));
    }

    static void* TextLayoutRuntime(const ::Aero::Media::Visual& visual) noexcept {
        return static_cast<void*>(TextLayoutOf(visual));
    }
    template<class TRuntime = void>
    static TRuntime* TypedTextLayoutRuntime(
        const ::Aero::Media::Visual& visual) noexcept {
        return static_cast<TRuntime*>(TextLayoutRuntime(visual));
    }

    // --- Layout hot state ---
    static UIElement::LayoutHot& Layout(UIElement& element) noexcept {
        return AERO_GET_FIELD(element, UIElement_layout);
    }
    static const UIElement::LayoutHot& Layout(const UIElement& element) noexcept {
        return AERO_GET_FIELD(element, UIElement_layout);
    }
    static Size MeasureOverride(UIElement& element, Size availableSize) noexcept;
    static Size ArrangeOverride(UIElement& element, Size finalSize) noexcept;
    static void SetActualSize(
        FrameworkElement& element,
        double width,
        double height) noexcept {
        Meta::PropertyValue widthVal(width);
        Meta::PropertyValue heightVal(height);
        AERO_CALL_METHOD(element, DO_SetReadOnlyCurrentValue,
            FrameworkElement::ActualWidthProperty.Handle(), widthVal);
        AERO_CALL_METHOD(element, DO_SetReadOnlyCurrentValue,
            FrameworkElement::ActualHeightProperty.Handle(), heightVal);
    }

    // --- Input / routed events ---
    static Base::Result<void> SetMouseOver(UIElement& element, bool value) noexcept {
        AERO_CALL_METHOD(element, UIElement_SetMouseOverState, value);
        return {};
    }
    static Base::Result<void> SetPressed(UIElement& element, bool value) noexcept {
        AERO_CALL_METHOD(element, UIElement_SetPressedState, value);
        return {};
    }
    static Base::Result<void> SetKeyboardFocused(
        UIElement& element, bool value) noexcept {
        AERO_CALL_METHOD(element, UIElement_SetKeyboardFocusedState, value);
        return {};
    }
    static Base::Result<void> SetKeyboardFocusWithin(
        UIElement& element, bool value) noexcept {
        AERO_CALL_METHOD(element, UIElement_SetKeyboardFocusWithinState, value);
        return {};
    }
    static void InvokeHandlers(
        UIElement& element,
        RoutedEventHandle event,
        RoutedEventArgs& args) noexcept {
        AERO_CALL_METHOD(element, UIElement_InvokeHandlers, event, args);
    }
    static void InvokeContentHandlers(
        ContentElement& element,
        RoutedEventHandle event,
        RoutedEventArgs& args) noexcept;

    // --- Content element attach ---
    static void Attach(
        ContentElement& element,
        DependencyObject* logicalParent,
        UIElement* contentHost,
        EventRouter* eventRouter) noexcept;
    static void Detach(ContentElement& element) noexcept;
    static DependencyObject* Parent(const ContentElement& element) noexcept;
    static UIElement* ContentHost(const ContentElement& element) noexcept;
    static std::uint32_t LogicalChildrenCount(
        const FrameworkContentElement& element) noexcept;
    static DependencyObject* LogicalChild(
        const FrameworkContentElement& element,
        std::uint32_t index) noexcept;
