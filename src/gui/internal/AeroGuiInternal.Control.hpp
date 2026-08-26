// Included from AeroGuiInternal.hpp inside class AeroGuiInternal.
// Control / template / items, panel, authored lists, text.

    // --- FrameworkElement interaction lists ---
    static Base::Result<void> SetTemplatedParent(
        FrameworkElement& element,
        DependencyObject* value) noexcept {
        element.templatedParent_ = value;
        return {};
    }
    static Base::Result<void> AddAuthoredTrigger(
        FrameworkElement& element,
        Base::Ref<Base::Object> trigger) noexcept {
        return element.AddAuthoredTrigger(std::move(trigger));
    }
    static Base::Result<void> ClearAuthoredTriggers(
        FrameworkElement& element) noexcept {
        element.ClearAuthoredTriggers();
        return {};
    }
    static Base::Span<const Base::Ref<Base::Object>> AuthoredTriggers(
        const FrameworkElement& element) noexcept {
        return element.AuthoredTriggers();
    }
    static Base::Result<void> AddAuthoredBehavior(
        FrameworkElement& element,
        Base::Ref<Base::Object> behavior) noexcept {
        return element.AddAuthoredBehavior(std::move(behavior));
    }
    static Base::Result<void> ClearAuthoredBehaviors(
        FrameworkElement& element) noexcept {
        element.ClearAuthoredBehaviors();
        return {};
    }
    static Base::Span<const Base::Ref<Base::Object>> AuthoredBehaviors(
        const FrameworkElement& element) noexcept {
        return element.AuthoredBehaviors();
    }
    static Base::Result<void> AddStyleTriggerPrototype(
        FrameworkElement& element,
        Base::Ref<Base::Object> trigger) noexcept {
        return element.AddStyleTriggerPrototype(std::move(trigger));
    }
    static Base::Result<void> ClearStyleTriggerPrototypes(
        FrameworkElement& element) noexcept {
        element.ClearStyleTriggerPrototypes();
        return {};
    }
    static Base::Span<const Base::Ref<Base::Object>> StyleTriggerPrototypes(
        const FrameworkElement& element) noexcept {
        return element.StyleTriggerPrototypes();
    }
    static Base::Result<void> AddStyleBehaviorPrototype(
        FrameworkElement& element,
        Base::Ref<Base::Object> behavior) noexcept {
        return element.AddStyleBehaviorPrototype(std::move(behavior));
    }
    static Base::Result<void> ClearStyleBehaviorPrototypes(
        FrameworkElement& element) noexcept {
        element.ClearStyleBehaviorPrototypes();
        return {};
    }
    static Base::Span<const Base::Ref<Base::Object>> StyleBehaviorPrototypes(
        const FrameworkElement& element) noexcept {
        return element.StyleBehaviorPrototypes();
    }
    static Base::Result<void> AddAuthoredTrigger(
        FrameworkContentElement& element,
        Base::Ref<Base::Object> trigger) noexcept {
        return element.AddAuthoredTrigger(std::move(trigger));
    }
    static Base::Result<void> ClearAuthoredTriggers(
        FrameworkContentElement& element) noexcept {
        element.ClearAuthoredTriggers();
        return {};
    }
    static Base::Span<const Base::Ref<Base::Object>> AuthoredTriggers(
        const FrameworkContentElement& element) noexcept {
        return element.AuthoredTriggers();
    }

    // --- Panel / decorator ---
    static std::uint32_t PanelChildCount(const Controls::Panel& panel) noexcept {
        return panel.ChildCountCore();
    }
    static Base::Ref<Base::Object> PanelChildAt(
        const Controls::Panel& panel,
        std::uint32_t index) noexcept {
        return panel.ChildAtCore(index);
    }
    static Base::Result<void> PanelAddChild(
        Controls::Panel& panel,
        const Base::Ref<Base::Object>& owner,
        UIElement& child) noexcept {
        return panel.AddChildCore(owner, child);
    }
    static Base::Result<bool> PanelRemoveChild(
        Controls::Panel& panel,
        UIElement& child) noexcept {
        return panel.RemoveChildCore(child);
    }
    static void PanelClearChildren(Controls::Panel& panel) noexcept {
        panel.ClearChildrenCore();
    }
    static const Base::Ref<Base::Object>& DecoratorOwnedChild(
        const Controls::Decorator& decorator) noexcept {
        return decorator.ownedChild_;
    }
    static Base::Result<void> DecoratorSetOwnedChild(
        Controls::Decorator& decorator,
        const Base::Ref<Base::Object>& owner,
        UIElement& child) noexcept {
        decorator.SetOwnedChild(owner, child);
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
        return control.templateHandleValue_ != 0U;
    }
    static std::uint64_t TemplateGeneration(
        const Controls::Control& control) noexcept {
        return control.templateGeneration_;
    }
    static UIElement* TemplateRoot(const Controls::Control& control) noexcept {
        return control.templateChild_;
    }
    static Base::Result<void> SetTemplateRoot(
        Controls::Control& control, UIElement* child) noexcept {
        control.SetTemplateChildCore(child);
        return {};
    }
    static void NotifyTemplateApplied(
        Controls::Control& control, std::uint64_t handleValue) noexcept {
        control.NotifyTemplateApplied(handleValue);
    }
    static void NotifyTemplateDetached(Controls::Control& control) noexcept {
        control.NotifyTemplateDetached();
    }
    static void InvokeTemplateApplied(Controls::Control& control) noexcept {
        control.OnApplyTemplate();
    }
    static UIElement* ContentControlContent(
        const Controls::ContentControl& control) noexcept {
        return control.content_;
    }
    static const Base::Ref<Base::Object>& OwnedContent(
        const Controls::ContentControl& control) noexcept {
        return control.ownedContent_;
    }
    static const Base::Ref<Base::Object>& ContentValue(
        const Controls::ContentControl& control) noexcept {
        return control.contentValue_;
    }
    static Base::Result<void> SetOwnedContent(
        Controls::ContentControl& control,
        const Base::Ref<Base::Object>& owner,
        UIElement& content) noexcept {
        control.SetOwnedContent(owner, content);
        return {};
    }
    static Base::Result<void> SetGeneratedTextContent(
        Controls::ContentControl& container,
        const Base::Ref<Base::Object>& contentObject,
        UIElement& content) noexcept {
        container.SetGeneratedTextContent(contentObject, content);
        return {};
    }
    static Base::Result<void> SetContentValue(
        Controls::ContentControl& control,
        Base::Ref<Base::Object> value) noexcept {
        control.SetContentValue(std::move(value));
        return {};
    }
    static Base::Result<void> SetContentValue(
        Controls::ContentControl& control,
        Meta::Value value) noexcept {
        control.SetContentValue(std::move(value));
        return {};
    }
    static void OnContentControlPropertyChanged(
        DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs& change) noexcept;
    static bool HasAttachedGenerator(
        const Controls::ItemsControl& control) noexcept {
        return control.generator_ != nullptr;
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
