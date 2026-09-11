#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/ContentElement.hpp>
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Controls/Decorator.hpp>
#include <Aero/Controls/Image.hpp>
#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Controls/MenuItem.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/Primitives/ButtonBase.hpp>
#include <Aero/Controls/Primitives/Selector.hpp>
#include <Aero/Controls/TreeViewItem.hpp>
#include <Aero/DependencyObject.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Shapes/Path.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/Visual.hpp>

#include "gui/core/VisualHandle.hpp"
#include "gui/internal/PropertyStore.hpp"

#include <cstddef>
#include <cstdint>

namespace Aero::Internal {

template<typename Tag, typename Tag::type M>
struct PrivateMemberThief {
    friend typename Tag::type GetPrivateMember(Tag) noexcept { return M; }
};

#define AERO_DECLARE_FIELD_THIEF(TagPrefix, Class, Member, Type) \
    struct TagPrefix##_ThiefTag { \
        using type = Type (Class::*); \
        friend type GetPrivateMember(TagPrefix##_ThiefTag) noexcept; \
    }; \
    template struct ::Aero::Internal::PrivateMemberThief<TagPrefix##_ThiefTag, &Class::Member>;

#define AERO_DECLARE_METHOD_THIEF(TagPrefix, Class, Member, MethodType, MethodPtr) \
    struct TagPrefix##_ThiefTag { \
        using type = MethodType; \
        friend type GetPrivateMember(TagPrefix##_ThiefTag) noexcept; \
    }; \
    template struct ::Aero::Internal::PrivateMemberThief<TagPrefix##_ThiefTag, MethodPtr>;

#define AERO_DECLARE_STATIC_METHOD_THIEF(TagPrefix, Class, Member, MethodType) \
    struct TagPrefix##_ThiefTag { \
        using type = MethodType; \
        friend type GetPrivateMember(TagPrefix##_ThiefTag) noexcept; \
    }; \
    template struct ::Aero::Internal::PrivateMemberThief<TagPrefix##_ThiefTag, &Class::Member>;

#define AERO_GET_FIELD(Object, TagPrefix) \
    ((Object).*GetPrivateMember(::Aero::Internal::TagPrefix##_ThiefTag{}))

// C++17 + GCC -Wpedantic rejects an empty __VA_ARGS__ at the call site.
#define AERO_CALL_METHOD0(Object, TagPrefix) \
    (((Object).*GetPrivateMember(::Aero::Internal::TagPrefix##_ThiefTag{}))())

#define AERO_CALL_METHOD(Object, TagPrefix, ...) \
    (((Object).*GetPrivateMember(::Aero::Internal::TagPrefix##_ThiefTag{}))(__VA_ARGS__))

#define AERO_CALL_STATIC_METHOD(TagPrefix, ...) \
    (GetPrivateMember(::Aero::Internal::TagPrefix##_ThiefTag{})(__VA_ARGS__))

// --- Visual private fields and methods ---
AERO_DECLARE_FIELD_THIEF(Visual_handleIndex, ::Aero::Media::Visual, handleIndex_, std::uint32_t)
AERO_DECLARE_FIELD_THIEF(Visual_handleGeneration, ::Aero::Media::Visual, handleGeneration_, std::uint32_t)
AERO_DECLARE_FIELD_THIEF(Visual_renderNodeId, ::Aero::Media::Visual, renderNodeId_, Base::RenderNodeId)
AERO_DECLARE_FIELD_THIEF(Visual_visualFlags, ::Aero::Media::Visual, visualFlags_, std::uint8_t)
AERO_DECLARE_FIELD_THIEF(Visual_renderDirtyFlags, ::Aero::Media::Visual, renderDirtyFlags_, std::uint8_t)
AERO_DECLARE_FIELD_THIEF(Visual_renderRevision, ::Aero::Media::Visual, renderRevision_, std::uint64_t)
AERO_DECLARE_FIELD_THIEF(Visual_visualParent, ::Aero::Media::Visual, visualParent_, ::Aero::Media::Visual*)
AERO_DECLARE_FIELD_THIEF(Visual_tree, ::Aero::Media::Visual, tree_, ::Aero::ElementTree*)
AERO_DECLARE_METHOD_THIEF(Visual_AcquireLifetime, ::Aero::Media::Visual, AcquireLifetime,
    Base::Result<Base::Ref<Base::Object>> (::Aero::Media::Visual::*)() noexcept,
    &::Aero::Media::Visual::AcquireLifetime)
AERO_DECLARE_METHOD_THIEF(Visual_GetVisualChild, ::Aero::Media::Visual, GetVisualChild,
    ::Aero::Media::Visual* (::Aero::Media::Visual::*)(std::uint32_t) const noexcept,
    &::Aero::Media::Visual::GetVisualChild)
AERO_DECLARE_METHOD_THIEF(Visual_GetVisualChildrenCount, ::Aero::Media::Visual, GetVisualChildrenCount,
    std::uint32_t (::Aero::Media::Visual::*)() const noexcept,
    &::Aero::Media::Visual::GetVisualChildrenCount)

// --- UIElement private fields and methods ---
AERO_DECLARE_FIELD_THIEF(UIElement_layout, ::Aero::UIElement, layout_, ::Aero::UIElement::LayoutHot)
AERO_DECLARE_METHOD_THIEF(UIElement_SetMouseOverState, ::Aero::UIElement, SetMouseOverState,
    void (::Aero::UIElement::*)(bool) noexcept,
    &::Aero::UIElement::SetMouseOverState)
AERO_DECLARE_METHOD_THIEF(UIElement_SetPressedState, ::Aero::UIElement, SetPressedState,
    void (::Aero::UIElement::*)(bool) noexcept,
    &::Aero::UIElement::SetPressedState)
AERO_DECLARE_METHOD_THIEF(UIElement_SetKeyboardFocusedState, ::Aero::UIElement, SetKeyboardFocusedState,
    void (::Aero::UIElement::*)(bool) noexcept,
    &::Aero::UIElement::SetKeyboardFocusedState)
AERO_DECLARE_METHOD_THIEF(UIElement_SetKeyboardFocusWithinState, ::Aero::UIElement, SetKeyboardFocusWithinState,
    void (::Aero::UIElement::*)(bool) noexcept,
    &::Aero::UIElement::SetKeyboardFocusWithinState)
AERO_DECLARE_METHOD_THIEF(UIElement_InvokeHandlers, ::Aero::UIElement, InvokeHandlers,
    void (::Aero::UIElement::*)(RoutedEventHandle, RoutedEventArgs&) noexcept,
    &::Aero::UIElement::InvokeHandlers)
AERO_DECLARE_METHOD_THIEF(UIElement_AddHandlerErased, ::Aero::UIElement, AddHandlerErased,
    void (::Aero::UIElement::*)(RoutedEventHandle, const void*, std::size_t, std::size_t, Meta::TypeId, bool) noexcept,
    &::Aero::UIElement::AddHandlerErased)
AERO_DECLARE_METHOD_THIEF(UIElement_RemoveHandlerErased, ::Aero::UIElement, RemoveHandlerErased,
    bool (::Aero::UIElement::*)(RoutedEventHandle, const void*, std::size_t, std::size_t, Meta::TypeId) noexcept,
    &::Aero::UIElement::RemoveHandlerErased)
AERO_DECLARE_METHOD_THIEF(UIElement_MeasureOverride, ::Aero::UIElement, MeasureOverride,
    Size (::Aero::UIElement::*)(Size) noexcept,
    &::Aero::UIElement::MeasureOverride)
AERO_DECLARE_METHOD_THIEF(UIElement_ArrangeOverride, ::Aero::UIElement, ArrangeOverride,
    Size (::Aero::UIElement::*)(Size) noexcept,
    &::Aero::UIElement::ArrangeOverride)

// --- ContentElement private fields and methods ---
AERO_DECLARE_FIELD_THIEF(ContentElement_logicalParent, ::Aero::ContentElement, logicalParent_, DependencyObject*)
AERO_DECLARE_FIELD_THIEF(ContentElement_contentHost, ::Aero::ContentElement, contentHost_, UIElement*)
AERO_DECLARE_FIELD_THIEF(ContentElement_eventRouter, ::Aero::ContentElement, eventRouter_, void*)
AERO_DECLARE_METHOD_THIEF(ContentElement_InvokeHandlers, ::Aero::ContentElement, InvokeHandlers,
    void (::Aero::ContentElement::*)(RoutedEventHandle, RoutedEventArgs&) noexcept,
    &::Aero::ContentElement::InvokeHandlers)

// --- FrameworkContentElement private/protected methods ---
AERO_DECLARE_METHOD_THIEF(FCE_GetLogicalChildrenCount, ::Aero::FrameworkContentElement, GetLogicalChildrenCount,
    std::uint32_t (::Aero::FrameworkContentElement::*)() const noexcept,
    &::Aero::FrameworkContentElement::GetLogicalChildrenCount)
AERO_DECLARE_METHOD_THIEF(FCE_GetLogicalChild, ::Aero::FrameworkContentElement, GetLogicalChild,
    DependencyObject* (::Aero::FrameworkContentElement::*)(std::uint32_t) const noexcept,
    &::Aero::FrameworkContentElement::GetLogicalChild)
AERO_DECLARE_METHOD_THIEF(FCE_AddAuthoredTrigger, ::Aero::FrameworkContentElement, AddAuthoredTrigger,
    void (::Aero::FrameworkContentElement::*)(Ref<Base::Object>) noexcept,
    &::Aero::FrameworkContentElement::AddAuthoredTrigger)
AERO_DECLARE_METHOD_THIEF(FCE_ClearAuthoredTriggers, ::Aero::FrameworkContentElement, ClearAuthoredTriggers,
    void (::Aero::FrameworkContentElement::*)() noexcept,
    &::Aero::FrameworkContentElement::ClearAuthoredTriggers)
AERO_DECLARE_METHOD_THIEF(FCE_AuthoredTriggers, ::Aero::FrameworkContentElement, AuthoredTriggers,
    Span<const Ref<Base::Object>> (::Aero::FrameworkContentElement::*)() const noexcept,
    &::Aero::FrameworkContentElement::AuthoredTriggers)

// --- FrameworkElement private/protected methods ---
AERO_DECLARE_METHOD_THIEF(FE_SetTemplatedParent, ::Aero::FrameworkElement, SetTemplatedParent,
    void (::Aero::FrameworkElement::*)(DependencyObject*) noexcept,
    &::Aero::FrameworkElement::SetTemplatedParent)
AERO_DECLARE_METHOD_THIEF(FE_AddAuthoredTrigger, ::Aero::FrameworkElement, AddAuthoredTrigger,
    void (::Aero::FrameworkElement::*)(Ref<Base::Object>) noexcept,
    &::Aero::FrameworkElement::AddAuthoredTrigger)
AERO_DECLARE_METHOD_THIEF(FE_ClearAuthoredTriggers, ::Aero::FrameworkElement, ClearAuthoredTriggers,
    void (::Aero::FrameworkElement::*)() noexcept,
    &::Aero::FrameworkElement::ClearAuthoredTriggers)
AERO_DECLARE_METHOD_THIEF(FE_AuthoredTriggers, ::Aero::FrameworkElement, AuthoredTriggers,
    Span<const Ref<Base::Object>> (::Aero::FrameworkElement::*)() const noexcept,
    &::Aero::FrameworkElement::AuthoredTriggers)
AERO_DECLARE_METHOD_THIEF(FE_AddAuthoredBehavior, ::Aero::FrameworkElement, AddAuthoredBehavior,
    void (::Aero::FrameworkElement::*)(Ref<Base::Object>) noexcept,
    &::Aero::FrameworkElement::AddAuthoredBehavior)
AERO_DECLARE_METHOD_THIEF(FE_ClearAuthoredBehaviors, ::Aero::FrameworkElement, ClearAuthoredBehaviors,
    void (::Aero::FrameworkElement::*)() noexcept,
    &::Aero::FrameworkElement::ClearAuthoredBehaviors)
AERO_DECLARE_METHOD_THIEF(FE_AuthoredBehaviors, ::Aero::FrameworkElement, AuthoredBehaviors,
    Span<const Ref<Base::Object>> (::Aero::FrameworkElement::*)() const noexcept,
    &::Aero::FrameworkElement::AuthoredBehaviors)
AERO_DECLARE_METHOD_THIEF(FE_AddStyleBehaviorPrototype, ::Aero::FrameworkElement, AddStyleBehaviorPrototype,
    void (::Aero::FrameworkElement::*)(Ref<Base::Object>) noexcept,
    &::Aero::FrameworkElement::AddStyleBehaviorPrototype)
AERO_DECLARE_METHOD_THIEF(FE_ClearStyleBehaviorPrototypes, ::Aero::FrameworkElement, ClearStyleBehaviorPrototypes,
    void (::Aero::FrameworkElement::*)() noexcept,
    &::Aero::FrameworkElement::ClearStyleBehaviorPrototypes)
AERO_DECLARE_METHOD_THIEF(FE_StyleBehaviorPrototypes, ::Aero::FrameworkElement, StyleBehaviorPrototypes,
    Span<const Ref<Base::Object>> (::Aero::FrameworkElement::*)() const noexcept,
    &::Aero::FrameworkElement::StyleBehaviorPrototypes)
AERO_DECLARE_METHOD_THIEF(FE_AddStyleTriggerPrototype, ::Aero::FrameworkElement, AddStyleTriggerPrototype,
    void (::Aero::FrameworkElement::*)(Ref<Base::Object>) noexcept,
    &::Aero::FrameworkElement::AddStyleTriggerPrototype)
AERO_DECLARE_METHOD_THIEF(FE_ClearStyleTriggerPrototypes, ::Aero::FrameworkElement, ClearStyleTriggerPrototypes,
    void (::Aero::FrameworkElement::*)() noexcept,
    &::Aero::FrameworkElement::ClearStyleTriggerPrototypes)
AERO_DECLARE_METHOD_THIEF(FE_StyleTriggerPrototypes, ::Aero::FrameworkElement, StyleTriggerPrototypes,
    Span<const Ref<Base::Object>> (::Aero::FrameworkElement::*)() const noexcept,
    &::Aero::FrameworkElement::StyleTriggerPrototypes)
AERO_DECLARE_METHOD_THIEF(FE_OnRender, ::Aero::FrameworkElement, OnRender,
    void (::Aero::FrameworkElement::*)(::Aero::Media::DrawingContext&) noexcept,
    &::Aero::FrameworkElement::OnRender)

// --- Image private fields ---
AERO_DECLARE_FIELD_THIEF(Image_renderImage, ::Aero::Controls::Image, renderImage_, std::uint64_t)
AERO_DECLARE_FIELD_THIEF(Image_pixelWidth, ::Aero::Controls::Image, pixelWidth_, std::uint32_t)
AERO_DECLARE_FIELD_THIEF(Image_pixelHeight, ::Aero::Controls::Image, pixelHeight_, std::uint32_t)

// --- Control private fields and methods ---
AERO_DECLARE_FIELD_THIEF(Control_templateHandleValue, ::Aero::Controls::Control, templateHandleValue_, std::uint64_t)
AERO_DECLARE_FIELD_THIEF(Control_templateGeneration, ::Aero::Controls::Control, templateGeneration_, std::uint64_t)
AERO_DECLARE_FIELD_THIEF(Control_templateChild, ::Aero::Controls::Control, templateChild_, UIElement*)
AERO_DECLARE_METHOD_THIEF(Control_SetTemplateChildCore, ::Aero::Controls::Control, SetTemplateChildCore,
    void (::Aero::Controls::Control::*)(UIElement*) noexcept,
    &::Aero::Controls::Control::SetTemplateChildCore)
AERO_DECLARE_METHOD_THIEF(Control_NotifyTemplateApplied, ::Aero::Controls::Control, NotifyTemplateApplied,
    void (::Aero::Controls::Control::*)(std::uint64_t) noexcept,
    &::Aero::Controls::Control::NotifyTemplateApplied)
AERO_DECLARE_METHOD_THIEF(Control_NotifyTemplateDetached, ::Aero::Controls::Control, NotifyTemplateDetached,
    void (::Aero::Controls::Control::*)() noexcept,
    &::Aero::Controls::Control::NotifyTemplateDetached)
AERO_DECLARE_METHOD_THIEF(Control_OnApplyTemplate, ::Aero::Controls::Control, OnApplyTemplate,
    void (::Aero::Controls::Control::*)() noexcept,
    &::Aero::Controls::Control::OnApplyTemplate)

// --- ContentControl private fields and methods ---
AERO_DECLARE_FIELD_THIEF(ContentControl_content, ::Aero::Controls::ContentControl, content_, UIElement*)
AERO_DECLARE_FIELD_THIEF(ContentControl_ownedContent, ::Aero::Controls::ContentControl, ownedContent_, Base::Ref<Base::Object>)
AERO_DECLARE_FIELD_THIEF(ContentControl_contentValue, ::Aero::Controls::ContentControl, contentValue_, Base::Ref<Base::Object>)
AERO_DECLARE_METHOD_THIEF(ContentControl_SetOwnedContent, ::Aero::Controls::ContentControl, SetOwnedContent,
    void (::Aero::Controls::ContentControl::*)(const Base::Ref<Base::Object>&, UIElement&) noexcept,
    &::Aero::Controls::ContentControl::SetOwnedContent)
AERO_DECLARE_METHOD_THIEF(ContentControl_SetGeneratedTextContent, ::Aero::Controls::ContentControl, SetGeneratedTextContent,
    void (::Aero::Controls::ContentControl::*)(const Base::Ref<Base::Object>&, UIElement&) noexcept,
    &::Aero::Controls::ContentControl::SetGeneratedTextContent)
AERO_DECLARE_METHOD_THIEF(ContentControl_SetContentValueRef, ::Aero::Controls::ContentControl, SetContentValue,
    void (::Aero::Controls::ContentControl::*)(Base::Ref<Base::Object>) noexcept,
    static_cast<void (::Aero::Controls::ContentControl::*)(Base::Ref<Base::Object>) noexcept>(&::Aero::Controls::ContentControl::SetContentValue))
AERO_DECLARE_METHOD_THIEF(ContentControl_SetContentValueVal, ::Aero::Controls::ContentControl, SetContentValue,
    void (::Aero::Controls::ContentControl::*)(Meta::Value) noexcept,
    static_cast<void (::Aero::Controls::ContentControl::*)(Meta::Value) noexcept>(&::Aero::Controls::ContentControl::SetContentValue))
// --- Decorator private fields and methods ---
AERO_DECLARE_FIELD_THIEF(Decorator_ownedChild, ::Aero::Controls::Decorator, ownedChild_, Base::Ref<Base::Object>)
AERO_DECLARE_METHOD_THIEF(Decorator_SetOwnedChild, ::Aero::Controls::Decorator, SetOwnedChild,
    void (::Aero::Controls::Decorator::*)(const Base::Ref<Base::Object>&, UIElement&) noexcept,
    &::Aero::Controls::Decorator::SetOwnedChild)

// --- Panel private methods ---
AERO_DECLARE_METHOD_THIEF(Panel_ChildCountCore, ::Aero::Controls::Panel, ChildCountCore,
    std::uint32_t (::Aero::Controls::Panel::*)() const noexcept,
    &::Aero::Controls::Panel::ChildCountCore)
AERO_DECLARE_METHOD_THIEF(Panel_ChildAtCore, ::Aero::Controls::Panel, ChildAtCore,
    Base::Ref<Base::Object> (::Aero::Controls::Panel::*)(std::uint32_t) const noexcept,
    &::Aero::Controls::Panel::ChildAtCore)
AERO_DECLARE_METHOD_THIEF(Panel_AddChildCore, ::Aero::Controls::Panel, AddChildCore,
    void (::Aero::Controls::Panel::*)(const Base::Ref<Base::Object>&, UIElement&) noexcept,
    &::Aero::Controls::Panel::AddChildCore)
AERO_DECLARE_METHOD_THIEF(Panel_RemoveChildCore, ::Aero::Controls::Panel, RemoveChildCore,
    Base::Result<bool> (::Aero::Controls::Panel::*)(UIElement&) noexcept,
    &::Aero::Controls::Panel::RemoveChildCore)
AERO_DECLARE_METHOD_THIEF(Panel_ClearChildrenCore, ::Aero::Controls::Panel, ClearChildrenCore,
    void (::Aero::Controls::Panel::*)() noexcept,
    &::Aero::Controls::Panel::ClearChildrenCore)

// --- MenuItem private method ---
AERO_DECLARE_METHOD_THIEF(MenuItem_SetHighlightedState, ::Aero::Controls::MenuItem, SetHighlightedState,
    void (::Aero::Controls::MenuItem::*)(bool) noexcept,
    &::Aero::Controls::MenuItem::SetHighlightedState)

// --- ItemsControl private fields and methods ---
AERO_DECLARE_FIELD_THIEF(ItemsControl_generator, ::Aero::Controls::ItemsControl, generator_, ::Aero::Controls::ItemContainerGenerator*)
AERO_DECLARE_METHOD_THIEF(ItemsControl_PublishReset, ::Aero::Controls::ItemsControl, PublishReset,
    void (::Aero::Controls::ItemsControl::*)() noexcept,
    &::Aero::Controls::ItemsControl::PublishReset)
AERO_DECLARE_METHOD_THIEF(ItemsControl_SetItemsSourceCore, ::Aero::Controls::ItemsControl, SetItemsSourceCore,
    void (::Aero::Controls::ItemsControl::*)(Collections::IItemsSource*) noexcept,
    &::Aero::Controls::ItemsControl::SetItemsSourceCore)
AERO_DECLARE_METHOD_THIEF(ItemsControl_SetItemTemplateCore, ::Aero::Controls::ItemsControl, SetItemTemplateCore,
    void (::Aero::Controls::ItemsControl::*)(const DataTemplate*) noexcept,
    &::Aero::Controls::ItemsControl::SetItemTemplateCore)
AERO_DECLARE_METHOD_THIEF(ItemsControl_SetItemTemplateSelectorCore, ::Aero::Controls::ItemsControl, SetItemTemplateSelectorCore,
    void (::Aero::Controls::ItemsControl::*)(const DataTemplateSelector*) noexcept,
    &::Aero::Controls::ItemsControl::SetItemTemplateSelectorCore)
AERO_DECLARE_METHOD_THIEF(ItemsControl_SetItemsPanelCore, ::Aero::Controls::ItemsControl, SetItemsPanelCore,
    void (::Aero::Controls::ItemsControl::*)(const Controls::ItemsPanelTemplate*) noexcept,
    &::Aero::Controls::ItemsControl::SetItemsPanelCore)
AERO_DECLARE_METHOD_THIEF(ItemsControl_SetItemContainerStyleCore, ::Aero::Controls::ItemsControl, SetItemContainerStyleCore,
    void (::Aero::Controls::ItemsControl::*)(const Style*) noexcept,
    &::Aero::Controls::ItemsControl::SetItemContainerStyleCore)

// --- Selector private method ---
AERO_DECLARE_METHOD_THIEF(Selector_SyncContainers, ::Aero::Controls::Primitives::Selector, SyncContainers,
    void (::Aero::Controls::Primitives::Selector::*)() noexcept,
    &::Aero::Controls::Primitives::Selector::SyncContainers)

// --- Freezable private field and methods ---
AERO_DECLARE_FIELD_THIEF(Freezable_impl, ::Aero::Freezable, impl_, ::Aero::Freezable::Impl*)
AERO_DECLARE_METHOD_THIEF(Freezable_FreezeCore, ::Aero::Freezable, FreezeCore,
    bool (::Aero::Freezable::*)(bool) noexcept,
    &::Aero::Freezable::FreezeCore)
AERO_DECLARE_METHOD_THIEF(Freezable_EnsureState, ::Aero::Freezable, EnsureState,
    bool (::Aero::Freezable::*)() noexcept,
    &::Aero::Freezable::EnsureState)

// --- Path private methods ---
AERO_DECLARE_METHOD_THIEF(Path_ResetGeometry, ::Aero::Shapes::Path, ResetGeometry,
    void (::Aero::Shapes::Path::*)() noexcept,
    &::Aero::Shapes::Path::ResetGeometry)
AERO_DECLARE_METHOD_THIEF(Path_AttachMeshResources, ::Aero::Shapes::Path, AttachMeshResources,
    void (::Aero::Shapes::Path::*)(void*, bool) noexcept,
    &::Aero::Shapes::Path::AttachMeshResources)

// --- DependencyObject private fields and methods ---
AERO_DECLARE_FIELD_THIEF(DO_registry, ::Aero::DependencyObject, registry_, ::Aero::Meta::DependencyPropertyRegistry*)
AERO_DECLARE_FIELD_THIEF(DO_valueStore, ::Aero::DependencyObject, valueStore_, void*)
AERO_DECLARE_METHOD_THIEF(DO_CanonicalPropertyKey, ::Aero::DependencyObject, CanonicalPropertyKey,
    MemberId (::Aero::DependencyObject::*)(DependencyPropertyHandle) const noexcept,
    &::Aero::DependencyObject::CanonicalPropertyKey)
AERO_DECLARE_METHOD_THIEF(DO_FindStoredEntry, ::Aero::DependencyObject, FindStoredEntry,
    StoredValueEntry* (::Aero::DependencyObject::*)(DependencyPropertyHandle) noexcept,
    static_cast<StoredValueEntry* (::Aero::DependencyObject::*)(DependencyPropertyHandle) noexcept>(&::Aero::DependencyObject::FindStoredEntry))
AERO_DECLARE_METHOD_THIEF(DO_FindStoredEntryConst, ::Aero::DependencyObject, FindStoredEntry,
    const StoredValueEntry* (::Aero::DependencyObject::*)(DependencyPropertyHandle) const noexcept,
    static_cast<const StoredValueEntry* (::Aero::DependencyObject::*)(DependencyPropertyHandle) const noexcept>(&::Aero::DependencyObject::FindStoredEntry))
AERO_DECLARE_METHOD_THIEF(DO_EnsureStoredEntry, ::Aero::DependencyObject, EnsureStoredEntry,
    Result<StoredValueEntry*> (::Aero::DependencyObject::*)(DependencyPropertyHandle) noexcept,
    &::Aero::DependencyObject::EnsureStoredEntry)
AERO_DECLARE_METHOD_THIEF(DO_EnsureStoredEntryDirect, ::Aero::DependencyObject, EnsureStoredEntryDirect,
    Result<StoredValueEntry*> (::Aero::DependencyObject::*)(DependencyPropertyHandle, const PropertyMetadata&) noexcept,
    &::Aero::DependencyObject::EnsureStoredEntryDirect)
AERO_DECLARE_METHOD_THIEF(DO_RemoveStoredEntry, ::Aero::DependencyObject, RemoveStoredEntry,
    void (::Aero::DependencyObject::*)(MemberId) noexcept,
    &::Aero::DependencyObject::RemoveStoredEntry)

AERO_DECLARE_METHOD_THIEF(DO_ApplyProviderContribution, ::Aero::DependencyObject, ApplyProviderContributionInternal,
    Result<void> (::Aero::DependencyObject::*)(DependencyPropertyHandle, PropertyProviderToken, const PropertyValue&) noexcept,
    &::Aero::DependencyObject::ApplyProviderContributionInternal)
AERO_DECLARE_METHOD_THIEF(DO_ClearProviderContribution, ::Aero::DependencyObject, ClearProviderContributionInternal,
    Result<bool> (::Aero::DependencyObject::*)(DependencyPropertyHandle, PropertyProviderToken) noexcept,
    &::Aero::DependencyObject::ClearProviderContributionInternal)
AERO_DECLARE_METHOD_THIEF(DO_ClearProviderOrigin, ::Aero::DependencyObject, ClearProviderOriginInternal,
    Result<bool> (::Aero::DependencyObject::*)(DependencyPropertyHandle, std::uint32_t) noexcept,
    &::Aero::DependencyObject::ClearProviderOriginInternal)
AERO_DECLARE_METHOD_THIEF(DO_ApplyLocalExpression, ::Aero::DependencyObject, ApplyLocalExpressionInternal,
    Result<void> (::Aero::DependencyObject::*)(DependencyPropertyHandle, const PropertyExpression&) noexcept,
    &::Aero::DependencyObject::ApplyLocalExpressionInternal)
AERO_DECLARE_METHOD_THIEF(DO_ClearLocalExpression, ::Aero::DependencyObject, ClearLocalExpressionInternal,
    Result<bool> (::Aero::DependencyObject::*)(DependencyPropertyHandle) noexcept,
    &::Aero::DependencyObject::ClearLocalExpressionInternal)
AERO_DECLARE_METHOD_THIEF(DO_InvalidateBaseValue, ::Aero::DependencyObject, InvalidateBaseValueInternal,
    Result<bool> (::Aero::DependencyObject::*)(DependencyPropertyHandle) noexcept,
    &::Aero::DependencyObject::InvalidateBaseValueInternal)
AERO_DECLARE_METHOD_THIEF(DO_ApplyAnimationValue, ::Aero::DependencyObject, ApplyAnimationValueInternal,
    Result<void> (::Aero::DependencyObject::*)(DependencyPropertyHandle, const PropertyValue&) noexcept,
    &::Aero::DependencyObject::ApplyAnimationValueInternal)
AERO_DECLARE_METHOD_THIEF(DO_ClearAnimationValue, ::Aero::DependencyObject, ClearAnimationValueInternal,
    Result<bool> (::Aero::DependencyObject::*)(DependencyPropertyHandle) noexcept,
    &::Aero::DependencyObject::ClearAnimationValueInternal)
AERO_DECLARE_METHOD_THIEF(DO_GetAnimationBaseValue, ::Aero::DependencyObject, GetAnimationBaseValueInternal,
    Result<PropertyValue> (::Aero::DependencyObject::*)(DependencyPropertyHandle) noexcept,
    &::Aero::DependencyObject::GetAnimationBaseValueInternal)
AERO_DECLARE_METHOD_THIEF(DO_ApplyInheritedValue, ::Aero::DependencyObject, ApplyInheritedValueInternal,
    Result<void> (::Aero::DependencyObject::*)(DependencyPropertyHandle, const PropertyValue*) noexcept,
    &::Aero::DependencyObject::ApplyInheritedValueInternal)
AERO_DECLARE_METHOD_THIEF(DO_RecomputeEffectiveValue, ::Aero::DependencyObject, RecomputeEffectiveValueInternal,
    Result<void> (::Aero::DependencyObject::*)(DependencyPropertyHandle) noexcept,
    &::Aero::DependencyObject::RecomputeEffectiveValueInternal)
AERO_DECLARE_METHOD_THIEF(DO_DropEngineValueState, ::Aero::DependencyObject, DropEngineValueStateInternal,
    Result<void> (::Aero::DependencyObject::*)(DependencyPropertyHandle) noexcept,
    &::Aero::DependencyObject::DropEngineValueStateInternal)
AERO_DECLARE_METHOD_THIEF(DO_SetReadOnlyCurrentValue, ::Aero::DependencyObject, SetReadOnlyCurrentValue,
    void (::Aero::DependencyObject::*)(DependencyPropertyHandle, const PropertyValue&) noexcept,
    &::Aero::DependencyObject::SetReadOnlyCurrentValue)
AERO_DECLARE_METHOD_THIEF(DO_AccumulateInvalidations, ::Aero::DependencyObject, AccumulateInvalidations,
    Meta::PropertyInvalidationFlags (::Aero::DependencyObject::*)(Meta::PropertyMetadataFlags) noexcept,
    &::Aero::DependencyObject::AccumulateInvalidations)
AERO_DECLARE_METHOD_THIEF(DO_OnPropertyInvalidated, ::Aero::DependencyObject, OnPropertyInvalidated,
    void (::Aero::DependencyObject::*)(Meta::PropertyInvalidationFlags) noexcept,
    &::Aero::DependencyObject::OnPropertyInvalidated)

// --- ButtonBase protected methods ---
AERO_DECLARE_METHOD_THIEF(ButtonBase_OnClick, ::Aero::Controls::Primitives::ButtonBase, OnClick,
    void (::Aero::Controls::Primitives::ButtonBase::*)(),
    &::Aero::Controls::Primitives::ButtonBase::OnClick)

} // namespace Aero::Internal
