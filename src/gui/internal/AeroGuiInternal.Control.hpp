// Included from AeroGuiInternal.hpp inside class AeroGuiInternal.
// Control / template / items, panel, authored lists, text.

    // --- FrameworkElement interaction lists ---
    static Base::Result<void> SetTemplatedParent(
        FrameworkElement& element,
        DependencyObject* value) noexcept {
        AERO_CALL_METHOD(element, FE_SetTemplatedParent, value);
        return {};
    }
    static void AddAuthoredTrigger(
        FrameworkElement& element,
        Base::Ref<Base::Object> trigger) noexcept {
        AERO_CALL_METHOD(element, FE_AddAuthoredTrigger, std::move(trigger));
    }
    static Base::Result<void> ClearAuthoredTriggers(
        FrameworkElement& element) noexcept {
        AERO_CALL_METHOD0(element, FE_ClearAuthoredTriggers);
        return {};
    }
    static Base::Span<const Base::Ref<Base::Object>> AuthoredTriggers(
        const FrameworkElement& element) noexcept {
        return AERO_CALL_METHOD0(element, FE_AuthoredTriggers);
    }
    static void AddAuthoredBehavior(
        FrameworkElement& element,
        Base::Ref<Base::Object> behavior) noexcept {
        AERO_CALL_METHOD(element, FE_AddAuthoredBehavior, std::move(behavior));
    }
    static Base::Result<void> ClearAuthoredBehaviors(
        FrameworkElement& element) noexcept {
        AERO_CALL_METHOD0(element, FE_ClearAuthoredBehaviors);
        return {};
    }
    static Base::Span<const Base::Ref<Base::Object>> AuthoredBehaviors(
        const FrameworkElement& element) noexcept {
        return AERO_CALL_METHOD0(element, FE_AuthoredBehaviors);
    }
    static void AddStyleTriggerPrototype(
        FrameworkElement& element,
        Base::Ref<Base::Object> trigger) noexcept {
        AERO_CALL_METHOD(element, FE_AddStyleTriggerPrototype, std::move(trigger));
    }
    static Base::Result<void> ClearStyleTriggerPrototypes(
        FrameworkElement& element) noexcept {
        AERO_CALL_METHOD0(element, FE_ClearStyleTriggerPrototypes);
        return {};
    }
    static Base::Span<const Base::Ref<Base::Object>> StyleTriggerPrototypes(
        const FrameworkElement& element) noexcept {
        return AERO_CALL_METHOD0(element, FE_StyleTriggerPrototypes);
    }
    static void AddStyleBehaviorPrototype(
        FrameworkElement& element,
        Base::Ref<Base::Object> behavior) noexcept {
        AERO_CALL_METHOD(element, FE_AddStyleBehaviorPrototype, std::move(behavior));
    }
    static Base::Result<void> ClearStyleBehaviorPrototypes(
        FrameworkElement& element) noexcept {
        AERO_CALL_METHOD0(element, FE_ClearStyleBehaviorPrototypes);
        return {};
    }
    static Base::Span<const Base::Ref<Base::Object>> StyleBehaviorPrototypes(
        const FrameworkElement& element) noexcept {
        return AERO_CALL_METHOD0(element, FE_StyleBehaviorPrototypes);
    }
    static void AddAuthoredTrigger(
        FrameworkContentElement& element,
        Base::Ref<Base::Object> trigger) noexcept {
        AERO_CALL_METHOD(element, FCE_AddAuthoredTrigger, std::move(trigger));
    }
    static Base::Result<void> ClearAuthoredTriggers(
        FrameworkContentElement& element) noexcept {
        AERO_CALL_METHOD0(element, FCE_ClearAuthoredTriggers);
        return {};
    }
    static Base::Span<const Base::Ref<Base::Object>> AuthoredTriggers(
        const FrameworkContentElement& element) noexcept {
        return AERO_CALL_METHOD0(element, FCE_AuthoredTriggers);
    }

    // --- Panel / decorator ---
    static std::uint32_t PanelChildCount(const Controls::Panel& panel) noexcept {
        return AERO_CALL_METHOD0(panel, Panel_ChildCountCore);
    }
    static Base::Ref<Base::Object> PanelChildAt(
        const Controls::Panel& panel,
        std::uint32_t index) noexcept {
        return AERO_CALL_METHOD(panel, Panel_ChildAtCore, index);
    }
    static void PanelAddChild(
        Controls::Panel& panel,
        const Base::Ref<Base::Object>& owner,
        UIElement& child) noexcept {
        AERO_CALL_METHOD(panel, Panel_AddChildCore, owner, child);
    }
    static Base::Result<bool> PanelRemoveChild(
        Controls::Panel& panel,
        UIElement& child) noexcept {
        return AERO_CALL_METHOD(panel, Panel_RemoveChildCore, child);
    }
    static void PanelClearChildren(Controls::Panel& panel) noexcept {
        AERO_CALL_METHOD0(panel, Panel_ClearChildrenCore);
    }
    static const Base::Ref<Base::Object>& DecoratorOwnedChild(
        const Controls::Decorator& decorator) noexcept {
        return AERO_GET_FIELD(decorator, Decorator_ownedChild);
    }
    static Base::Result<void> DecoratorSetOwnedChild(
        Controls::Decorator& decorator,
        const Base::Ref<Base::Object>& owner,
        UIElement& child) noexcept {
        AERO_CALL_METHOD(decorator, Decorator_SetOwnedChild, owner, child);
        return {};
    }

    // --- Control / template / items ---
    static void SetMenuItemHighlighted(
        Controls::MenuItem& item, bool value) noexcept;
    static void SyncSelectorContainers(
        Controls::Primitives::Selector& selector) noexcept;
    static std::uint32_t TreeViewItemCount(
        const Controls::TreeViewItem& item) noexcept;
    static bool IsTemplateApplied(const Controls::Control& control) noexcept {
        return AERO_GET_FIELD(control, Control_templateHandleValue) != 0U;
    }
    static std::uint64_t TemplateGeneration(
        const Controls::Control& control) noexcept {
        return AERO_GET_FIELD(control, Control_templateGeneration);
    }
    static UIElement* TemplateRoot(const Controls::Control& control) noexcept {
        return AERO_GET_FIELD(control, Control_templateChild);
    }
    static Base::Result<void> SetTemplateRoot(
        Controls::Control& control, UIElement* child) noexcept {
        AERO_CALL_METHOD(control, Control_SetTemplateChildCore, child);
        return {};
    }
    static void NotifyTemplateApplied(
        Controls::Control& control, std::uint64_t handleValue) noexcept {
        AERO_CALL_METHOD(control, Control_NotifyTemplateApplied, handleValue);
    }
    static void NotifyTemplateDetached(Controls::Control& control) noexcept {
        AERO_CALL_METHOD0(control, Control_NotifyTemplateDetached);
    }
    static void InvokeTemplateApplied(Controls::Control& control) noexcept {
        AERO_CALL_METHOD0(control, Control_OnApplyTemplate);
    }
    static UIElement* ContentControlContent(
        const Controls::ContentControl& control) noexcept {
        return AERO_GET_FIELD(control, ContentControl_content);
    }
    static const Base::Ref<Base::Object>& OwnedContent(
        const Controls::ContentControl& control) noexcept {
        return AERO_GET_FIELD(control, ContentControl_ownedContent);
    }
    static const Base::Ref<Base::Object>& ContentValue(
        const Controls::ContentControl& control) noexcept {
        return AERO_GET_FIELD(control, ContentControl_contentValue);
    }
    static Base::Result<void> SetOwnedContent(
        Controls::ContentControl& control,
        const Base::Ref<Base::Object>& owner,
        UIElement& content) noexcept {
        AERO_CALL_METHOD(control, ContentControl_SetOwnedContent, owner, content);
        return {};
    }
    static Base::Result<void> SetGeneratedTextContent(
        Controls::ContentControl& container,
        const Base::Ref<Base::Object>& contentObject,
        UIElement& content) noexcept {
        AERO_CALL_METHOD(container, ContentControl_SetGeneratedTextContent, contentObject, content);
        return {};
    }
    static Base::Result<void> SetContentValue(
        Controls::ContentControl& control,
        Base::Ref<Base::Object> value) noexcept {
        AERO_CALL_METHOD(control, ContentControl_SetContentValueRef, std::move(value));
        return {};
    }
    static Base::Result<void> SetContentValue(
        Controls::ContentControl& control,
        Meta::Value value) noexcept {
        AERO_CALL_METHOD(control, ContentControl_SetContentValueVal, std::move(value));
        return {};
    }
    static void OnContentControlPropertyChanged(
        DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs& change) noexcept;
    static bool HasAttachedGenerator(
        const Controls::ItemsControl& control) noexcept {
        return AERO_GET_FIELD(control, ItemsControl_generator) != nullptr;
    }
    static void SetItemsSource(
        Controls::ItemsControl& control,
        Collections::IItemsSource* source) noexcept;
    static void SetItemsSource(
        Controls::ItemsControl& control,
        Base::Ref<Base::Object> source) noexcept;
    static void SetItemsSourceBorrowed(
        Controls::ItemsControl& control,
        Collections::IItemsSource* source) noexcept;
    static void SetItemTemplate(
        Controls::ItemsControl& control,
        const DataTemplate* value) noexcept;
    static void SetItemTemplateSelector(
        Controls::ItemsControl& control,
        const DataTemplateSelector* value) noexcept;
    static void SetItemsPanel(
        Controls::ItemsControl& control,
        const Controls::ItemsPanelTemplate* value) noexcept;
    static void SetItemContainerStyle(
        Controls::ItemsControl& control,
        const Style* value) noexcept;
    static void RefreshDisplayMemberPath(
        Controls::ItemsControl& control) noexcept;
    static Base::Result<Controls::ItemContainerGenerator*>
    CreateItemContainerGenerator(
        ElementTree& tree,
        LayoutEngine& layout,
        Meta::EffectiveValueEngine& values,
        StyleEngine* styles,
        Render::RenderTree* renderer,
        Controls::TemplateEngine* templates,
        Controls::ItemSubtreeCallback callback,
        void* context) noexcept;

    // --- Text ---
    static void AttachTextLayout(
        Controls::TextBlock& element,
        void* service,
        bool invalidate = false) noexcept;
    static void AttachTextLayout(
        Controls::TextBox& element,
        void* service,
        bool invalidate = false) noexcept;
    static void AttachTextLayout(
        Controls::PasswordBox& element,
        void* service,
        bool invalidate = false) noexcept;

    // --- ButtonBase ---
    static void Click(Controls::Primitives::ButtonBase& button) noexcept {
        AERO_CALL_METHOD0(button, ButtonBase_OnClick);
    }
