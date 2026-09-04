#include <Aero/Controls.hpp>
#include <Aero/Controls/ItemsPresenter.hpp>
#include <Aero/VisualTreeHelper.hpp>
#include <Aero/Data/CollectionViewSource.hpp>
#include <Aero/DataTemplateSelector.hpp>
#include <Aero/HierarchicalDataTemplate.hpp>
#include <Aero/TryCast.hpp>
#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/controls/State.hpp" 
#include "gui/templates/TemplateState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"

#include <Aero/FrameworkElement.hpp>

#include <algorithm>
#include <cstdio>
#include <new>
#include <utility>
#include "ControlBehavior.hpp"

namespace Aero::Controls {

using namespace ::Aero;
using namespace ::Aero::Controls;
using namespace ::Aero::Controls;
using namespace ::Aero;

struct ItemContainerGeneratorRuntime {
public:
    ItemContainerGeneratorRuntime(
        ItemContainerGenerator& facade,
        ElementTree& tree,
        LayoutEngine& layout,
        EffectiveValueEngine& values,
        StyleEngine* styles,
        ::Aero::Render::RenderTree* renderer,
        TemplateEngine* templates,
        ItemSubtreeCallback subtreeCallback,
        void* subtreeContext) noexcept;
    ~ItemContainerGeneratorRuntime() noexcept;

    Base::Result<void> Attach(ItemsControl& owner, Panel& itemsHost) noexcept;
    Base::Result<void> AttachVirtualized(
        ItemsControl& owner,
        VirtualizingStackPanel& itemsHost) noexcept;
    Base::Result<bool> Detach() noexcept;
    Base::Result<void> Refresh() noexcept;
    Base::Result<bool> SetRealizationRange(
        std::uint32_t firstIndex,
        std::uint32_t count) noexcept;
    FrameworkElement* ContainerFromIndex(std::uint32_t index) const noexcept;
    std::uint32_t IndexFromContainer(
        const FrameworkElement& container) const noexcept;
    Base::Ref<Base::Object> ItemFromContainer(
        const FrameworkElement& container) const noexcept;

    std::uint32_t GetGeneratedCount() const noexcept { return records_.Size(); }
    std::uint32_t GetFirstGeneratedIndex() const noexcept {
        return firstGeneratedIndex_;
    }
    std::uint32_t GetCreatedContainerCount() const noexcept {
        return createdContainerCount_;
    }
    std::uint32_t GetRecycledContainerUseCount() const noexcept {
        return recycledContainerUseCount_;
    }
    Base::Status LastError() const noexcept { return lastError_; }

private:
    struct Record {
        Base::Ref<Base::Object> item;
        Base::Ref<FrameworkElement> container;
        Base::Ref<Base::Object> content;
        ElementAttachment containerMount;
        ElementAttachment contentMount;
        Base::Vector<ElementAttachment> subtreeMounts;
        const Style* appliedStyle = nullptr;
        bool itemIsOwnContainer = false;
        bool generatedTextContent = false;
        bool generatedHeader = false;
        bool subtreeMounted = false;
    };

    ItemContainerGenerator* facade_ = nullptr;
    ElementTree* tree_ = nullptr;
    LayoutEngine* layout_ = nullptr;
    EffectiveValueEngine* values_ = nullptr;
    StyleEngine* styles_ = nullptr;
    ::Aero::Render::RenderTree* renderer_ = nullptr;
    TemplateEngine* templates_ = nullptr;
    ItemSubtreeCallback subtreeCallback_ = nullptr;
    void* subtreeContext_ = nullptr;
    ItemsControl* owner_ = nullptr;
    Panel* host_ = nullptr;
    VirtualizingStackPanel* virtualizingHost_ = nullptr;
    Base::Vector<Record> records_;
    Base::Vector<Base::Ref<FrameworkElement>> recycledContainers_;
    ItemsChangedHandler changedHandler_;
    DependencyPropertyChangedEventHandler
        generatedHeaderChangedHandler_;
    std::uint32_t firstGeneratedIndex_ = 0U;
    std::uint32_t targetRealizationCount_ = 0U;
    std::uint32_t createdContainerCount_ = 0U;
    std::uint32_t recycledContainerUseCount_ = 0U;
    Base::Status lastError_;
    bool inRealizationRange_ = false;
    bool virtualized_ = false;

    Base::Result<Record> CreateRecord(
        std::uint32_t index) noexcept;
    Base::Result<void> AttachRecord(
        Record& record,
        std::uint32_t index) noexcept;
    Base::Result<void> DetachRecord(
        Record& record,
        bool recycleContainer = false) noexcept;
    Base::Result<void> AttachOwnedSubtree(
        Record& record,
        Aero::Media::Visual& root) noexcept;
    Base::Result<void> DetachOwnedSubtree(
        Record& record) noexcept;
    Base::Result<void> UpdateGeneratedHeader(
        Record& record) noexcept;
    Base::Result<void> ProjectGeneratedContent(
        Record& record) noexcept;
    void OnGeneratedHeaderChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void OnItemsChanged(
        const ItemsChangedEvent& event) noexcept;
    Base::Result<void> ApplyChange(
        const ItemsChangedEvent& event) noexcept;
    Base::Result<void> InsertRecord(
        std::uint32_t index,
        Record record) noexcept;
    void RemoveRecordAt(
        std::uint32_t index) noexcept;
    Base::Result<void> ReorderVisuals() noexcept;
    Base::Result<bool> SetRealizationRangeInternal(
        std::uint32_t firstIndex,
        std::uint32_t count,
        bool pruneRecycled) noexcept;
    Base::Result<void> ReleaseRecycledContainers() noexcept;
};

ItemContainerGeneratorRuntime::ItemContainerGeneratorRuntime(
    ItemContainerGenerator& facade,
    ElementTree& tree,
    LayoutEngine& layout,
    EffectiveValueEngine& values,
    StyleEngine* styles,
    ::Aero::Render::RenderTree* renderer,
    TemplateEngine* templates,
    ItemSubtreeCallback subtreeCallback,
    void* subtreeContext) noexcept
    : facade_(&facade),
      tree_(&tree),
      layout_(&layout),
      values_(&values),
      styles_(styles),
      renderer_(renderer),
      templates_(templates),
      subtreeCallback_(subtreeCallback),
      subtreeContext_(subtreeContext),
      changedHandler_(
          this,
          &ItemContainerGeneratorRuntime::OnItemsChanged),
      generatedHeaderChangedHandler_(
          this,
          &ItemContainerGeneratorRuntime::OnGeneratedHeaderChanged) {}

ItemContainerGeneratorRuntime::~ItemContainerGeneratorRuntime() noexcept {
    static_cast<void>(Detach());
}

Base::Result<void> ItemContainerGeneratorRuntime::Attach(
    ItemsControl& owner,
    Panel& itemsHost) noexcept {
    if (owner_ != nullptr ||
        owner.generator_ != nullptr ||
        owner.GetTree() != tree_ ||
        itemsHost.GetTree() != tree_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemContainerGenerator attach state is invalid");
    }
    owner.AddItemsChanged(changedHandler_);
    owner_ = &owner;
    host_ = &itemsHost;
    virtualizingHost_ = nullptr;
    firstGeneratedIndex_ = 0U;
    owner.generator_ = facade_;
    Base::Result<void> refreshed = Refresh();
    if (!refreshed) {
        static_cast<void>(
            owner.RemoveItemsChanged(
                changedHandler_));
        for (std::uint32_t index = records_.Size();
            index > 0U; --index) {
            static_cast<void>(
                DetachRecord(records_[index - 1U]));
        }
        records_.Clear();
        owner_ = nullptr;
        host_ = nullptr;
        owner.generator_ = nullptr;
        return refreshed.GetStatus();
    }
    return {};
}

Base::Result<void>
ItemContainerGeneratorRuntime::AttachVirtualized(
    ItemsControl& owner,
    VirtualizingStackPanel& itemsHost) noexcept {
    if (owner_ != nullptr ||
        owner.generator_ != nullptr ||
        owner.GetTree() != tree_ ||
        itemsHost.GetTree() != tree_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Virtualized item generator attach state is invalid");
    }
    owner.AddItemsChanged(changedHandler_);
    owner_ = &owner;
    host_ = &itemsHost;
    virtualizingHost_ = &itemsHost;
    firstGeneratedIndex_ = 0U;
    owner.generator_ = facade_;
    Base::Result<void> attached =
        itemsHost.AttachGenerator(*facade_, owner.GetCount());
    if (!attached) {
        static_cast<void>(
            owner.RemoveItemsChanged(changedHandler_));
        owner.generator_ = nullptr;
        owner_ = nullptr;
        host_ = nullptr;
        virtualizingHost_ = nullptr;
        return attached.GetStatus();
    }
    Base::Result<bool> realized =
        SetRealizationRangeInternal(
            itemsHost.desiredFirstIndex_,
            itemsHost.desiredCount_,
            true);
    if (!realized) {
        itemsHost.DetachGenerator(*facade_);
        static_cast<void>(
            owner.RemoveItemsChanged(changedHandler_));
        owner.generator_ = nullptr;
        owner_ = nullptr;
        host_ = nullptr;
        virtualizingHost_ = nullptr;
        return realized.GetStatus();
    }
    owner.OnContainersChanged();
    return {};
}

Base::Result<bool> ItemContainerGeneratorRuntime::Detach() noexcept {
    if (owner_ == nullptr) return false;
    static_cast<void>(
        owner_->RemoveItemsChanged(
            changedHandler_));
    Base::Status firstError;
    for (std::uint32_t index = records_.Size();
        index > 0U; --index) {
        Base::Result<void> detached =
            DetachRecord(records_[index - 1U]);
        if (!detached && firstError.IsOk()) {
            firstError = detached.GetStatus();
        }
    }
    records_.Clear();
    Base::Result<void> released =
        ReleaseRecycledContainers();
    if (!released && firstError.IsOk()) {
        firstError = released.GetStatus();
    }
    if (virtualizingHost_ != nullptr) {
        virtualizingHost_->DetachGenerator(*facade_);
    }
    owner_->generator_ = nullptr;
    owner_ = nullptr;
    host_ = nullptr;
    virtualizingHost_ = nullptr;
    firstGeneratedIndex_ = 0U;
    return firstError.IsOk()
        ? Base::Result<bool>(true)
        : Base::Result<bool>(firstError);
}

Base::Result<ItemContainerGeneratorRuntime::Record>
ItemContainerGeneratorRuntime::CreateRecord(
    std::uint32_t index) noexcept {
    if (owner_ == nullptr ||
        index >= owner_->GetCount()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Item generation index is out of range");
    }
    Record record;
    record.item = owner_->GetItem(index);
    if (!record.item) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemsSource returned null");
    }
    if (owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.item->RuntimeType(),
            FrameworkElement::StaticTypeId())) {
        record.container =
            Base::Ref<FrameworkElement>::FromBorrowed(
                *static_cast<FrameworkElement*>(
                    record.item.Get()));
        record.itemIsOwnContainer = true;
        return record;
    }
    Base::Ref<DataTemplate> itemTemplate =
        owner_->ResolveItemTemplate(record.item, index);
    if (itemTemplate) {
        Base::Result<Base::Ref<Base::Object>>
            content =
                DataTemplateRuntime::Instantiate(
                    *itemTemplate, record.item,
                    AeroGuiInternal::BindingEngineOf(*owner_));
        if (!content) return content.GetStatus();
        record.content =
            std::move(content).Value();
    } else if (!owner_->GetDisplayMemberPath().Empty()) {
        Aero::BindingEngine* bindings =
            AeroGuiInternal::BindingEngineOf(*owner_);
        Meta::Registry* metadata =
            bindings != nullptr ? bindings->Metadata() : nullptr;
        if (metadata == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "DisplayMemberPath metadata services are unavailable");
        }
        const Meta::PropertyInfo* property =
            metadata->Types().FindProperty(
                record.item->RuntimeType(),
                owner_->GetDisplayMemberPath(),
                true);
        if (property == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "DisplayMemberPath property was not found on the item type");
        }
        Base::Result<Meta::Value> value =
            metadata->GetProperty(
                *record.item, property->Id());
        if (!value) return value.GetStatus();
        Base::String display;
        Base::Result<void> formatted;
        switch (value.Value().Kind()) {
        case Meta::ValueKind::String:
            formatted = display.Assign(value.Value().AsString());
            break;
        case Meta::ValueKind::Boolean:
            formatted = display.Assign(
                value.Value().AsBoolean()
                    ? Base::StringView("True")
                    : Base::StringView("False"));
            break;
        case Meta::ValueKind::SignedInteger: {
            char buffer[48]{};
            const int length = std::snprintf(
                buffer, sizeof(buffer), "%lld",
                static_cast<long long>(value.Value().AsSignedInteger()));
            formatted = length > 0
                ? display.Assign(Base::StringView(
                      buffer, static_cast<std::uint32_t>(length)))
                : Base::Result<void>(Base::Status::Failure(
                      Base::ErrorCode::ValidationFailed,
                      "DisplayMemberPath integer formatting failed"));
            break;
        }
        case Meta::ValueKind::UnsignedInteger: {
            char buffer[48]{};
            const int length = std::snprintf(
                buffer, sizeof(buffer), "%llu",
                static_cast<unsigned long long>(
                    value.Value().AsUnsignedInteger()));
            formatted = length > 0
                ? display.Assign(Base::StringView(
                      buffer, static_cast<std::uint32_t>(length)))
                : Base::Result<void>(Base::Status::Failure(
                      Base::ErrorCode::ValidationFailed,
                      "DisplayMemberPath unsigned formatting failed"));
            break;
        }
        case Meta::ValueKind::Double: {
            char buffer[64]{};
            const int length = std::snprintf(
                buffer, sizeof(buffer), "%.15g", value.Value().AsDouble());
            formatted = length > 0
                ? display.Assign(Base::StringView(
                      buffer, static_cast<std::uint32_t>(length)))
                : Base::Result<void>(Base::Status::Failure(
                      Base::ErrorCode::ValidationFailed,
                      "DisplayMemberPath numeric formatting failed"));
            break;
        }
        default:
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "DisplayMemberPath value has no text representation");
        }
        if (!formatted) return formatted.GetStatus();
        Base::Result<Base::Ref<TextBlock>> text =
            Base::MakeRef<TextBlock>();
        if (!text) return text.GetStatus();
        text.Value()->SetText(display.View());
        record.content = Base::Ref<Base::Object>(
            std::move(text).Value());
        record.generatedTextContent = true;
    } else if (
        record.item->RuntimeType() ==
            ::Aero::Controls::BoxedItemValue::StaticTypeId()) {
        const Meta::Value& value =
            static_cast<const ::Aero::Controls::BoxedItemValue&>(
                *record.item).Value();
        Base::String display;
        Base::Result<void> formatted;
        switch (value.Kind()) {
        case Meta::ValueKind::String:
            formatted = display.Assign(value.AsString());
            break;
        case Meta::ValueKind::Boolean:
            formatted = display.Assign(
                value.AsBoolean() ? Base::StringView("True")
                                  : Base::StringView("False"));
            break;
        case Meta::ValueKind::SignedInteger: {
            char buffer[48]{};
            const int length = std::snprintf(
                buffer, sizeof(buffer), "%lld",
                static_cast<long long>(value.AsSignedInteger()));
            formatted = length > 0
                ? display.Assign(Base::StringView(
                      buffer, static_cast<std::uint32_t>(length)))
                : Base::Result<void>(Base::Status::Failure(
                      Base::ErrorCode::ValidationFailed,
                      "Boxed integer item formatting failed"));
            break;
        }
        case Meta::ValueKind::UnsignedInteger: {
            char buffer[48]{};
            const int length = std::snprintf(
                buffer, sizeof(buffer), "%llu",
                static_cast<unsigned long long>(value.AsUnsignedInteger()));
            formatted = length > 0
                ? display.Assign(Base::StringView(
                      buffer, static_cast<std::uint32_t>(length)))
                : Base::Result<void>(Base::Status::Failure(
                      Base::ErrorCode::ValidationFailed,
                      "Boxed unsigned item formatting failed"));
            break;
        }
        case Meta::ValueKind::Double: {
            char buffer[64]{};
            const int length = std::snprintf(
                buffer, sizeof(buffer), "%.15g", value.AsDouble());
            formatted = length > 0
                ? display.Assign(Base::StringView(
                      buffer, static_cast<std::uint32_t>(length)))
                : Base::Result<void>(Base::Status::Failure(
                      Base::ErrorCode::ValidationFailed,
                      "Boxed numeric item formatting failed"));
            break;
        }
        default:
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Boxed data item has no default text representation");
        }
        if (!formatted) return formatted.GetStatus();
        Base::Result<Base::Ref<TextBlock>> text =
            Base::MakeRef<TextBlock>();
        if (!text) return text.GetStatus();
        text.Value()->SetText(display.View());
        record.content =
            Base::Ref<Base::Object>(
                std::move(text).Value());
        record.generatedTextContent = true;
    } else if (owner_->PropertyRegistry().Types()
        .IsDerivedFrom(
            record.item->RuntimeType(),
            UIElement::StaticTypeId())) {
        record.content = record.item;
    } else if (owner_->PropertyRegistry().Types().IsDerivedFrom(
                   owner_->RuntimeType(),
                   ListView::StaticTypeId()) &&
               static_cast<ListView*>(owner_)->GetView()) {
        // GridView presents the data item through GridViewRowPresenter and
        // column bindings. Keep a zero-content visual for the container's
        // ordinary ContentControl contract while DataContext carries the item.
        Base::Result<Base::Ref<TextBlock>> placeholder =
            Base::MakeRef<TextBlock>();
        if (!placeholder) return placeholder.GetStatus();
        record.content = Base::Ref<Base::Object>(
            std::move(placeholder).Value());
        record.generatedTextContent = true;
    } else {
        // No ItemTemplate, DisplayMemberPath, or recognized value wrapper was
        // supplied. Mirror WPF's ToString() fallback by realizing the item
        // inside a default text element so the container is still generated
        // instead of dropping the item entirely.
        Base::Result<Base::Ref<TextBlock>> text =
            Base::MakeRef<TextBlock>();
        if (!text) return text.GetStatus();
        record.content = Base::Ref<Base::Object>(
            std::move(text).Value());
        record.generatedTextContent = true;
    }
    if (!record.content ||
        !owner_->PropertyRegistry().Types()
            .IsDerivedFrom(
                record.content->RuntimeType(),
                UIElement::StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemTemplate must create a UIElement");
    }
    if (!recycledContainers_.Empty()) {
        record.container =
            std::move(recycledContainers_.Back());
        recycledContainers_.PopBack();
        ++recycledContainerUseCount_;
    } else {
        Base::Result<Base::Ref<FrameworkElement>> made =
            owner_->CreateContainer(record.item);
        if (!made || !made.Value()) {
            if (!made) return made.GetStatus();
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "ItemsControl returned null container");
        }
        record.container =
            std::move(made).Value();
        ++createdContainerCount_;
    }

    return record;
}

Base::Result<void>
ItemContainerGeneratorRuntime::AttachOwnedSubtree(
    Record& record,
    Aero::Media::Visual& root) noexcept {
    Base::Vector<Aero::Media::Visual*> pending;
    Base::Result<void> pushed =
        pending.PushBack(&root);
    if (!pushed) return pushed.GetStatus();

    const auto attachChild =
        [this, &record, &pending](
            Aero::Media::Visual& parent,
            const Base::Ref<Base::Object>& owned)
            noexcept -> Base::Result<void> {
        if (!owned ||
            !owner_->PropertyRegistry().Types().
                IsDerivedFrom(
                    owned->RuntimeType(),
                    UIElement::StaticTypeId())) {
            return {};
        }
        auto& child =
            *static_cast<Aero::Media::Visual*>(
                owned.Get());
        auto* childElement = static_cast<UIElement*>(owned.Get());
        if (child.GetVisualParent() == &parent &&
            child.GetTree() == tree_ &&
            childElement->GetIsLayoutAttached()) {
            return pending.PushBack(&child);
        }
        if (child.GetVisualParent() == &parent) {
            if (child.GetTree() == nullptr &&
                child.GetLogicalParent() == nullptr) {
                Base::Result<ElementAttachment> mounted =
                    tree_->AttachElement(parent, child);
                if (!mounted) return mounted.GetStatus();
                Base::Result<void> tracked =
                    record.subtreeMounts.PushBack(std::move(mounted).Value());
                if (!tracked) return tracked.GetStatus();
            } else if (!childElement->GetIsLayoutAttached()) {
                Base::Result<Aero::VisualAttachment> visual =
                    tree_->AttachVisualChild(parent, child);
                if (!visual) return visual.GetStatus();
                ElementAttachment edge;
                edge.logicalParent = &parent;
                edge.visualParent = &parent;
                edge.child = &child;
                edge.logicalAttached = false;
                edge.visualAttached = visual.Value().visualAttached;
                edge.layoutAttached = visual.Value().layoutAttached;
                edge.renderAttached = visual.Value().renderAttached;
                Base::Result<void> tracked =
                    record.subtreeMounts.PushBack(std::move(edge));
                if (!tracked) return tracked.GetStatus();
            }
            return pending.PushBack(&child);
        }
        if (child.GetVisualParent() != nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Owned item-template child is already mounted elsewhere");
        }
        if (child.GetLogicalParent() != nullptr &&
            child.GetLogicalParent() != &parent) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Owned item-template child is already mounted elsewhere");
        }
        if (child.GetLogicalParent() != nullptr ||
            child.GetTree() != nullptr) {
            Base::Result<Aero::VisualAttachment> visual =
                tree_->AttachVisualChild(parent, child);
            if (!visual) return visual.GetStatus();
            ElementAttachment edge;
            edge.logicalParent = &parent;
            edge.visualParent = &parent;
            edge.child = &child;
            edge.logicalAttached = false;
            edge.visualAttached = visual.Value().visualAttached;
            edge.layoutAttached = visual.Value().layoutAttached;
            edge.renderAttached = visual.Value().renderAttached;
            Base::Result<void> tracked =
                record.subtreeMounts.PushBack(std::move(edge));
            if (!tracked) {
                (void)tree_->DetachVisual(visual.Value());
                return tracked.GetStatus();
            }
            return pending.PushBack(&child);
        }
        Base::Result<ElementAttachment> mounted =
            tree_->AttachElement(parent, child);
        if (!mounted) return mounted.GetStatus();
        ElementAttachment edge =
            std::move(mounted).Value();
        Base::Result<void> tracked =
            record.subtreeMounts.PushBack(
                std::move(edge));
        if (!tracked) {
            (void)tree_->DetachElement(edge);
            return tracked.GetStatus();
        }
        Base::Result<void> queued =
            pending.PushBack(&child);
        if (!queued) {
            ElementAttachment rollback =
                std::move(record.subtreeMounts.Back());
            record.subtreeMounts.PopBack();
            (void)tree_->DetachElement(rollback);
            return queued.GetStatus();
        }
        return {};
    };

    while (!pending.Empty()) {
        Aero::Media::Visual* current =
            pending.Back();
        pending.PopBack();
        if (current == nullptr) continue;
        const Meta::TypeId type =
            current->RuntimeType();
        if (owner_->PropertyRegistry().Types().
                IsDerivedFrom(
                    type, Panel::StaticTypeId())) {
            auto& panel =
                *static_cast<Panel*>(current);
            for (std::uint32_t index = 0U;
                 index < AeroGuiInternal::PanelChildCount(panel);
                 ++index) {
                Base::Result<void> attached =
                    attachChild(
                        panel,
                        AeroGuiInternal::PanelChildAt(panel, index));
                if (!attached) {
                    (void)DetachOwnedSubtree(record);
                    return attached.GetStatus();
                }
            }
        } else if (owner_->PropertyRegistry().Types().
                       IsDerivedFrom(
                           type,
                           Decorator::StaticTypeId())) {
            auto& decorator =
                *static_cast<Decorator*>(current);
            Base::Result<void> attached =
                attachChild(
                    decorator,
                    AeroGuiInternal::DecoratorOwnedChild(decorator));
            if (!attached) {
                (void)DetachOwnedSubtree(record);
                return attached.GetStatus();
            }
        } else if (owner_->PropertyRegistry().Types().
                       IsDerivedFrom(
                           type,
                           ContentControl::StaticTypeId())) {
            auto& content =
                *static_cast<ContentControl*>(current);
            Base::Result<void> attached =
                attachChild(
                    content,
                    AeroGuiInternal::OwnedContent(content));
            if (!attached) {
                (void)DetachOwnedSubtree(record);
                return attached.GetStatus();
            }
        } else if (owner_->PropertyRegistry().Types().
                       IsDerivedFrom(
                           type,
                           ContentPresenter::StaticTypeId())) {
            auto& presenter =
                *static_cast<ContentPresenter*>(current);
            Base::Result<void> attached =
                attachChild(
                    presenter,
                    presenter.GetOwnedContent());
            if (!attached) {
                (void)DetachOwnedSubtree(record);
                return attached.GetStatus();
            }
        }
    }
    return {};
}

Base::Result<void>
ItemContainerGeneratorRuntime::DetachOwnedSubtree(
    Record& record) noexcept {
    Base::Status firstError;
    for (std::uint32_t index =
             record.subtreeMounts.Size();
         index > 0U; --index) {
        Base::Result<void> detached =
            tree_->DetachElement(
                record.subtreeMounts[index - 1U]);
        if (!detached && firstError.IsOk()) {
            firstError = detached.GetStatus();
        }
    }
    record.subtreeMounts.Clear();
    return firstError.IsOk()
        ? Base::Result<void>()
        : Base::Result<void>(firstError);
}

Base::Result<void>
ItemContainerGeneratorRuntime::AttachRecord(
    Record& record,
    std::uint32_t index) noexcept {
    FrameworkElement& container = *record.container;

    Base::Result<ElementAttachment> containerMounted =
        tree_->AttachElement(*owner_, *host_, container);
    if (!containerMounted) return containerMounted.GetStatus();
    record.containerMount = std::move(containerMounted).Value();

    // ItemContainerStyle must be present before activating the generated
    // subtree. Activation resolves implicit styles and materializes the
    // control template; applying the explicit container style afterwards
    // leaves the already-instantiated implicit template in place.
    const Style* style = owner_->GetItemContainerStyle();
    if (style != nullptr && styles_ != nullptr) {
        Base::Result<Base::Ref<Style>> retained =
            owner_->GetValue(ItemsControl::ItemContainerStyleProperty);
        if (!retained || !retained.Value()) {
            (void)tree_->DetachElement(record.containerMount);
            return retained
                ? Base::Status::Failure(
                      Base::ErrorCode::InvalidState,
                      "ItemContainerStyle is not retained")
                : retained.GetStatus();
        }
        container.SetValue(
            FrameworkElement::StyleProperty,
            std::move(retained).Value());
        record.appliedStyle = style;
    }

    ContentControl* contentControl = nullptr;
    ContentPresenter* contentPresenter = nullptr;
    HeaderedItemsControl* headeredItemsControl = nullptr;
    if (owner_->PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            ContentControl::StaticTypeId())) {
        contentControl = static_cast<ContentControl*>(&container);
    }
    if (owner_->PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            ContentPresenter::StaticTypeId())) {
        contentPresenter = static_cast<ContentPresenter*>(&container);
    }
    if (owner_->PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            HeaderedItemsControl::StaticTypeId())) {
        headeredItemsControl = static_cast<HeaderedItemsControl*>(&container);
    }
    if (!record.itemIsOwnContainer && headeredItemsControl != nullptr &&
        record.content.Get() != nullptr &&
        owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.content->RuntimeType(), UIElement::StaticTypeId())) {
        if (owner_->PropertyRegistry().Types().IsDerivedFrom(
                record.content->RuntimeType(), TextBlock::StaticTypeId())) {
            Base::Result<void> subscribed =
                static_cast<TextBlock*>(record.content.Get())
                    ->AddValueChangedHandler(
                        TextBlock::TextProperty,
                        generatedHeaderChangedHandler_);
            if (!subscribed) {
                (void)tree_->DetachElement(record.containerMount);
                return subscribed.GetStatus();
            }
            record.generatedHeader = true;
            Base::Result<void> assigned = UpdateGeneratedHeader(record);
            if (!assigned) {
                static_cast<void>(
                    static_cast<TextBlock*>(record.content.Get())
                        ->RemoveValueChangedHandler(
                            TextBlock::TextProperty,
                            generatedHeaderChangedHandler_));
                record.generatedHeader = false;
                (void)tree_->DetachElement(record.containerMount);
                return assigned.GetStatus();
            }
        } else {
            // Complex ItemTemplates (gallery SampleTemplate is a StackPanel)
            // must be published as Header so ContentSource=Header presenters
            // display them. Extracting TextBlock.Text only covers string headers.
            const Value header = Value::FromObject(
                record.content->RuntimeType(), record.content);
            if (owner_->PropertyRegistry().Types().IsDerivedFrom(
                    container.RuntimeType(),
                    TreeViewItem::StaticTypeId())) {
                static_cast<TreeViewItem&>(container).SetHeader(header);
            } else {
                headeredItemsControl->SetHeader(header);
            }
            record.generatedHeader = true;
        }
    } else if (!record.itemIsOwnContainer && contentControl != nullptr) {
        auto& content =
            *static_cast<UIElement*>(
                record.content.Get());
        Base::Result<void> selected =
            record.generatedTextContent
            ? AeroGuiInternal::
                      SetGeneratedTextContent(
                      *contentControl, record.content, content)
            : AeroGuiInternal::SetOwnedContent(*contentControl,
                  record.content, content);
        if (!selected) {
            (void)tree_->DetachElement(record.containerMount);
            return selected.GetStatus();
        }
        if (record.generatedTextContent &&
            owner_->PropertyRegistry().Types().IsDerivedFrom(
                container.RuntimeType(), Control::StaticTypeId())) {
            auto* text = static_cast<TextBlock*>(record.content.Get());
            auto& hostControl = static_cast<Control&>(container);
            text->SetValue(
                TextBlock::FontSizeProperty, hostControl.GetFontSize());
        }
        Base::Result<ElementAttachment> contentMounted =
            tree_->AttachElement(container, content);
        if (!contentMounted) {
            (void)tree_->DetachElement(record.containerMount);
            return contentMounted.GetStatus();
        }
        record.contentMount =
            std::move(contentMounted).Value();
    } else if (!record.itemIsOwnContainer && contentPresenter != nullptr &&
               record.content.Get() != nullptr) {
        auto& content =
            *static_cast<UIElement*>(
                record.content.Get());
        Base::Result<ElementAttachment> contentMounted =
            tree_->AttachElement(container, content);
        if (!contentMounted) {
            (void)tree_->DetachElement(record.containerMount);
            return contentMounted.GetStatus();
        }
        record.contentMount =
            std::move(contentMounted).Value();
        contentPresenter->SetOwnedContent(record.content, content);
        if (contentPresenter->GetContent() != &content) {
            (void)tree_->DetachElement(record.contentMount);
            (void)tree_->DetachElement(record.containerMount);
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "ContentPresenter rejected generated item content");
        }
    }
    // UI activation starts at the generated container. This is
    // required for implicit container styles and control templates (for
    // example ComboBoxItem), while traversal still reaches DataTemplate
    // content mounted beneath it.
    Aero::Media::Visual& subtree =
        static_cast<Aero::Media::Visual&>(container);
    Base::Result<void> subtreeAttached =
        AttachOwnedSubtree(record, subtree);
    if (!subtreeAttached) {
        (void)DetachRecord(record);
        return subtreeAttached.GetStatus();
    }
    Base::Result<void> prepared = owner_->PrepareContainer(container, record.item, index);
    if (!prepared) { (void)DetachRecord(record); return prepared.GetStatus(); }
    Base::Result<void> projected = ProjectGeneratedContent(record);
    if (!projected) {
        (void)DetachRecord(record);
        return projected.GetStatus();
    }
    if (record.generatedHeader) {
        Base::Result<void> synchronized = UpdateGeneratedHeader(record);
        if (!synchronized) {
            (void)DetachRecord(record);
            return synchronized.GetStatus();
        }
    }
    if (subtreeCallback_ != nullptr) {
        Base::Result<void> presented =
            subtreeCallback_(
                subtree,
                ItemSubtreeChange::Mounted,
                subtreeContext_);
        if (!presented) {
            (void)DetachRecord(record);
            return presented.GetStatus();
        }
        record.subtreeMounted = true;
    }
    // Template apply during activation may recreate PART_Header; re-host.
    projected = ProjectGeneratedContent(record);
    if (!projected) {
        (void)DetachRecord(record);
        return projected.GetStatus();
    }
    if (subtreeCallback_ != nullptr &&
        record.generatedHeader &&
        record.content &&
        owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.content->RuntimeType(), UIElement::StaticTypeId()) &&
        !owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.content->RuntimeType(), TextBlock::StaticTypeId())) {
        auto& headerVisual =
            *static_cast<Aero::Media::Visual*>(record.content.Get());
        if (headerVisual.GetTree() != nullptr) {
            Base::Result<void> headerPresented =
                subtreeCallback_(
                    headerVisual,
                    ItemSubtreeChange::Mounted,
                    subtreeContext_);
            if (!headerPresented) {
                (void)DetachRecord(record);
                return headerPresented.GetStatus();
            }
        }
    }
    return {};
}

Base::Result<void>
ItemContainerGeneratorRuntime::ProjectGeneratedContent(
    Record& record) noexcept {
    if (record.itemIsOwnContainer || !record.content || !record.container ||
        !owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.content->RuntimeType(), UIElement::StaticTypeId())) {
        return {};
    }
    auto& content = *static_cast<UIElement*>(record.content.Get());
    // String headers already live on Header; do not also mount the
    // extracted TextBlock into PART_Header.
    if (record.generatedHeader &&
        owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.content->RuntimeType(), TextBlock::StaticTypeId())) {
        return {};
    }
    if (record.generatedTextContent &&
        owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.content->RuntimeType(), TextBlock::StaticTypeId()) &&
        owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.container->RuntimeType(), Control::StaticTypeId())) {
        auto* text = static_cast<TextBlock*>(record.content.Get());
        auto& hostControl = static_cast<Control&>(*record.container);
        text->SetValue(
            TextBlock::FontSizeProperty, hostControl.GetFontSize());
    }
    if (!owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.container->RuntimeType(), Control::StaticTypeId())) {
        return {};
    }
    auto& container = static_cast<Control&>(*record.container);
    // Prefer the instance PART_Header. FindPart(ContentPresenter) and
    // templates_->FindName can hit a shared expander content host and steal
    // sibling SampleTemplate visuals onto one presenter.
    DependencyObject* part = nullptr;
    if (templates_ != nullptr) {
        const TemplateHandle handle = templates_->AppliedHandle(container);
        if (handle.IsValid()) {
            part = templates_->FindName(
                handle, Base::StringView("PART_Header"));
        }
    }
    auto isHeaderPresenter = [&](DependencyObject* node) noexcept -> bool {
        return node != nullptr &&
            owner_->PropertyRegistry().Types().IsDerivedFrom(
                node->RuntimeType(), ContentPresenter::StaticTypeId()) &&
            static_cast<ContentPresenter*>(node)->GetContentSource() ==
                Base::StringView("Header");
    };
    if (!isHeaderPresenter(part)) {
        ContentPresenter* walked = nullptr;
        const auto consider = [&](ContentPresenter* presenter) noexcept {
            if (presenter != nullptr &&
                presenter->GetContentSource() == Base::StringView("Header")) {
                walked = presenter;
            }
        };
        const auto walk = [&](auto&& self, ::Aero::Media::Visual& node) -> void {
            if (walked != nullptr) {
                return;
            }
            if (&node != &container) {
                if (::Aero::TryCast<ItemsPresenter>(&node) != nullptr ||
                    ::Aero::TryCast<TreeViewItem>(&node) != nullptr) {
                    return;
                }
            }
            if (auto* presenter = ::Aero::TryCast<ContentPresenter>(&node)) {
                consider(presenter);
            }
            const std::uint32_t count =
                ::Aero::Media::VisualTreeHelper::GetChildrenCount(node);
            for (std::uint32_t index = 0U; index < count; ++index) {
                ::Aero::Media::Visual* child =
                    ::Aero::Media::VisualTreeHelper::GetChild(node, index);
                if (child != nullptr) {
                    self(self, *child);
                }
            }
        };
        walk(walk, container);
        part = walked;
    }
    if (!isHeaderPresenter(part)) {
        return {};
    }
    auto& presenter = *static_cast<ContentPresenter*>(part);
    UIElement* existing = presenter.GetContent();
    if (existing != nullptr && existing != &content) {
        // ContentSource presenters are pre-filled with a TextBlock host for
        // string Header/Content. Replace it so a DataTemplate visual (gallery
        // SampleTemplate StackPanel) can occupy PART_Header. Detach layout
        // before visual: LayoutParent() is the visual parent.
        ::Aero::VisualAttachment state;
        state.visualParent = &presenter;
        state.child = existing;
        state.visualAttached = existing->GetVisualParent() == &presenter;
        state.layoutAttached =
            existing->GetIsLayoutAttached() &&
            existing->LayoutParent() == &presenter;
        state.renderAttached = false;
        if (state.IsAttached()) {
            Base::Result<void> detached = tree_->DetachVisual(state);
            if (!detached) {
                return detached.GetStatus();
            }
        }
        presenter.SetContent(nullptr);
    }
    if (content.GetVisualParent() != &presenter) {
        if (content.GetVisualParent() != nullptr &&
            content.GetVisualParent() != record.container.Get()) {
            Base::Result<void> detached = tree_->DetachVisual(
                *content.GetVisualParent(),
                static_cast<::Aero::Media::Visual&>(content));
            if (!detached) {
                return detached.GetStatus();
            }
        }
        if (record.contentMount.visualAttached ||
            record.contentMount.layoutAttached ||
            record.contentMount.renderAttached) {
            Base::Result<void> detached =
                tree_->DetachVisual(record.contentMount);
            if (!detached) {
                return detached.GetStatus();
            }
        }
        if (record.contentMount.child == &content) {
            Base::Result<void> attached =
                tree_->AttachVisual(record.contentMount, presenter);
            if (!attached) {
                return attached.GetStatus();
            }
        } else if (content.GetTree() == nullptr &&
                   content.GetLogicalParent() == nullptr) {
            Base::Result<ElementAttachment> mounted =
                tree_->AttachElement(presenter, content);
            if (!mounted) {
                return mounted.GetStatus();
            }
            record.contentMount = std::move(mounted).Value();
        } else {
            Base::Result<Aero::VisualAttachment> visual =
                tree_->AttachVisualChild(presenter, content);
            if (!visual) {
                return visual.GetStatus();
            }
            record.contentMount.visualParent = &presenter;
            record.contentMount.child = &content;
            record.contentMount.visualAttached = visual.Value().visualAttached;
            record.contentMount.layoutAttached = visual.Value().layoutAttached;
            record.contentMount.renderAttached = visual.Value().renderAttached;
        }
    }
    presenter.SetOwnedContent(record.content, content);
    return AttachOwnedSubtree(record, content);
}

Base::Result<void>
ItemContainerGeneratorRuntime::UpdateGeneratedHeader(
    Record& record) noexcept {
    if (!record.generatedHeader || !record.content || !record.container ||
        !owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.content->RuntimeType(), TextBlock::StaticTypeId()) ||
        !owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.container->RuntimeType(),
            HeaderedItemsControl::StaticTypeId())) {
        return {};
    }
    const Base::StringView text =
        static_cast<TextBlock*>(record.content.Get())->GetText();
    if (owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.container->RuntimeType(),
            TreeViewItem::StaticTypeId())) {
        return static_cast<TreeViewItem*>(record.container.Get())
            ->SetHeader(text);
    }
    return static_cast<HeaderedItemsControl*>(record.container.Get())
        ->SetHeader(text);
}

void ItemContainerGeneratorRuntime::OnGeneratedHeaderChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&) noexcept {
    for (Record& record : records_) {
        if (record.generatedHeader && record.content.Get() == &object) {
            Base::Result<void> synchronized = UpdateGeneratedHeader(record);
            if (!synchronized) lastError_ = synchronized.GetStatus();
            return;
        }
    }
}

Base::Result<void>
ItemContainerGeneratorRuntime::DetachRecord(
    Record& record,
    bool recycleContainer) noexcept {
    if (!record.container) return {};
    FrameworkElement& container = *record.container;
    ContentControl* contentControl = nullptr;
    ContentPresenter* contentPresenter = nullptr;
    HeaderedItemsControl* headeredItemsControl = nullptr;
    if (owner_->PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            ContentControl::StaticTypeId())) {
        contentControl = static_cast<ContentControl*>(&container);
    }
    if (owner_->PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            ContentPresenter::StaticTypeId())) {
        contentPresenter = static_cast<ContentPresenter*>(&container);
    }
    if (owner_->PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(),
            HeaderedItemsControl::StaticTypeId())) {
        headeredItemsControl = static_cast<HeaderedItemsControl*>(&container);
    }
    Base::Status firstError;
    const auto capture = [&firstError](const Base::Result<void>& result) noexcept {
        if (!result && firstError.IsOk()) firstError = result.GetStatus();
    };
    if (record.subtreeMounted &&
        subtreeCallback_ != nullptr) {
        Aero::Media::Visual& subtree =
            static_cast<Aero::Media::Visual&>(
                container);
        capture(subtreeCallback_(
            subtree,
            ItemSubtreeChange::Unmounting,
            subtreeContext_));
        record.subtreeMounted = false;
    }
    // Headered item containers consume a DataTemplate TextBlock as a string
    // header instead of mounting it below the container. Its bindings are
    // nevertheless activated when the template is instantiated, so it needs
    // an explicit teardown because the container subtree walk cannot reach it.
    if (record.generatedHeader && record.content &&
        subtreeCallback_ != nullptr &&
        owner_->PropertyRegistry().Types().IsDerivedFrom(
            record.content->RuntimeType(), UIElement::StaticTypeId())) {
        capture(subtreeCallback_(
            *static_cast<Aero::Media::Visual*>(record.content.Get()),
            ItemSubtreeChange::Unmounting,
            subtreeContext_));
    }
    capture(DetachOwnedSubtree(record));
    owner_->ClearContainer(container);
    if (record.generatedHeader && headeredItemsControl != nullptr) {
        if (record.content && owner_->PropertyRegistry().Types().IsDerivedFrom(
                record.content->RuntimeType(), TextBlock::StaticTypeId())) {
            static_cast<void>(
                static_cast<TextBlock*>(record.content.Get())
                    ->RemoveValueChangedHandler(
                        TextBlock::TextProperty,
                        generatedHeaderChangedHandler_));
        }
        const auto clearHeader = [&]() noexcept {
            if (owner_->PropertyRegistry().Types().IsDerivedFrom(
                    container.RuntimeType(), TreeViewItem::StaticTypeId())) {
                static_cast<TreeViewItem&>(container).SetHeader(
                    Value::NullObject(Meta::TypeOf<Base::Object>()));
                return;
            }
            headeredItemsControl->SetHeader(
                Value::NullObject(Meta::TypeOf<Base::Object>()));
        };
        clearHeader();
        record.generatedHeader = false;
    }
    if (record.appliedStyle != nullptr && styles_ != nullptr) {
        capture(styles_->Clear(container, *record.appliedStyle));
        container.ClearValue(FrameworkElement::StyleProperty);
        record.appliedStyle = nullptr;
    }
    if (!record.itemIsOwnContainer &&
        templates_ != nullptr &&
        owner_->PropertyRegistry().Types().IsDerivedFrom(
            container.RuntimeType(), Control::StaticTypeId()) &&
        AeroGuiInternal::IsTemplateApplied(
            static_cast<Control&>(container))) {
        Base::Result<bool> cleared =
            templates_->Clear(static_cast<Control&>(container));
        if (!cleared) {
            capture(Base::Result<void>(
                cleared.GetStatus()));
        }
    }
    UIElement* content = nullptr;
    if (!record.itemIsOwnContainer && contentControl != nullptr) {
        content = AeroGuiInternal::ContentControlContent(
            *contentControl);
    } else if (!record.itemIsOwnContainer && contentPresenter != nullptr) {
        content = contentPresenter->GetContent();
    }
    if (content != nullptr) {
        capture(tree_->DetachElement(record.contentMount));
        if (contentControl != nullptr) {
            contentControl->SetContent(nullptr);
        } else if (contentPresenter != nullptr) {
            contentPresenter->SetContent(nullptr);
        }
        tree_->InvalidateNodeHandle(*content);
    }
    capture(tree_->DetachElement(record.containerMount));
    if (!record.itemIsOwnContainer &&
        recycleContainer && firstError.IsOk()) {
        Base::Result<void> recycled = recycledContainers_.PushBack(std::move(record.container));
        if (!recycled) {
            tree_->InvalidateNodeHandle(container);
            if (firstError.IsOk()) firstError = recycled.GetStatus();
        }
    } else {
        tree_->InvalidateNodeHandle(container);
    }
    record.item.Reset();
    record.content.Reset();
    record.containerMount = {};
    record.contentMount = {};
    record.itemIsOwnContainer = false;
    record.generatedTextContent = false;
    record.generatedHeader = false;
    record.subtreeMounted = false;
    return firstError.IsOk() ? Base::Result<void>() : Base::Result<void>(firstError);
}

Base::Result<void>
ItemContainerGeneratorRuntime::ReleaseRecycledContainers() noexcept {
    Base::Status firstError;
    for (Base::Ref<FrameworkElement>& container :
        recycledContainers_) {
        if (!container) continue;
        tree_->InvalidateNodeHandle(*container);
    }
    recycledContainers_.Clear();
    return firstError.IsOk()
        ? Base::Result<void>()
        : Base::Result<void>(firstError);
}

Base::Result<void>
ItemContainerGeneratorRuntime::InsertRecord(
    std::uint32_t index,
    Record record) noexcept {
    if (index > records_.Size()) {
        static_cast<void>(DetachRecord(record));
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Generated container insert is out of range");
    }
    Base::Result<void> reserved =
        records_.Reserve(records_.Size() + 1U);
    if (!reserved) {
        const Base::Status error = reserved.GetStatus();
        static_cast<void>(DetachRecord(record));
        return error;
    }
    Base::Result<void> appended =
        records_.PushBack(std::move(record));
    if (!appended) {
        const Base::Status error = appended.GetStatus();
        static_cast<void>(DetachRecord(record));
        return error;
    }
    if (index + 1U == records_.Size()) return {};
    Record moving = std::move(records_.Back());
    for (std::uint32_t current =
            records_.Size() - 1U;
        current > index; --current) {
        records_[current] =
            std::move(records_[current - 1U]);
    }
    records_[index] = std::move(moving);
    return {};
}

void ItemContainerGeneratorRuntime::RemoveRecordAt(
    std::uint32_t index) noexcept {
    for (std::uint32_t current = index;
        current + 1U < records_.Size(); ++current) {
        records_[current] =
            std::move(records_[current + 1U]);
    }
    records_.PopBack();
}

Base::Result<void>
ItemContainerGeneratorRuntime::ReorderVisuals() noexcept {
    for (Record& record : records_) {
        Base::Result<void> detached = tree_->DetachVisual(record.containerMount);
        if (!detached) return detached.GetStatus();
    }
    for (Record& record : records_) {
        Base::Result<void> attached = tree_->AttachVisual(record.containerMount, *host_);
        if (!attached) return attached.GetStatus();
    }
    return {};
}

Base::Result<bool>
ItemContainerGeneratorRuntime::SetRealizationRangeInternal(
    std::uint32_t firstIndex,
    std::uint32_t count,
    bool force) noexcept {
    if (owner_ == nullptr ||
        host_ == nullptr ||
        virtualizingHost_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Item realization range requires a virtualizing host");
    }
    const std::uint32_t itemCount =
        owner_->GetCount();
    firstIndex = std::min(firstIndex, itemCount);
    count = std::min(count, itemCount - firstIndex);
    if (!force &&
        firstGeneratedIndex_ == firstIndex &&
        records_.Size() == count) {
        return false;
    }

    Base::Status firstError;
    for (std::uint32_t index = records_.Size();
        index > 0U; --index) {
        Base::Result<void> detached =
            DetachRecord(records_[index - 1U], true);
        if (!detached && firstError.IsOk()) {
            firstError = detached.GetStatus();
        }
    }
    records_.Clear();
    firstGeneratedIndex_ = firstIndex;
    if (!firstError.IsOk()) {
        return firstError;
    }

    Base::Result<void> reserved =
        records_.Reserve(count);
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t offset = 0U;
        offset < count; ++offset) {
        const std::uint32_t index =
            firstIndex + offset;
        Base::Result<Record> made =
            CreateRecord(index);
        if (!made) {
            firstError = made.GetStatus();
            break;
        }
        Record record = std::move(made).Value();
        Base::Result<void> attached =
            AttachRecord(record, index);
        if (!attached) {
            firstError = attached.GetStatus();
            break;
        }
        Base::Result<void> added =
            records_.PushBack(std::move(record));
        if (!added) {
            firstError = added.GetStatus();
            static_cast<void>(
                DetachRecord(record, true));
            break;
        }
    }
    if (!firstError.IsOk()) {
        for (std::uint32_t index = records_.Size();
            index > 0U; --index) {
            static_cast<void>(
                DetachRecord(
                    records_[index - 1U], true));
        }
        records_.Clear();
        return firstError;
    }
    return true;
}

Base::Result<bool>
ItemContainerGeneratorRuntime::SetRealizationRange(
    std::uint32_t firstIndex,
    std::uint32_t count) noexcept {
    Base::Result<bool> changed =
        SetRealizationRangeInternal(
            firstIndex, count, false);
    lastError_ = changed
        ? Base::Status{}
        : changed.GetStatus();
    if (changed && changed.Value() &&
        owner_ != nullptr) {
        owner_->OnContainersChanged();
    }
    return changed;
}

Base::Result<void> ItemContainerGeneratorRuntime::Refresh() noexcept {
    if (owner_ == nullptr || host_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemContainerGenerator is not attached");
    }
    if (virtualizingHost_ != nullptr) {
        Base::Result<bool> realized =
            SetRealizationRangeInternal(
                virtualizingHost_->desiredFirstIndex_,
                virtualizingHost_->desiredCount_,
                true);
        return realized
            ? Base::Result<void>()
            : Base::Result<void>(
                realized.GetStatus());
    }
    firstGeneratedIndex_ = 0U;
    for (std::uint32_t index = records_.Size();
        index > 0U; --index) {
        Base::Result<void> detached =
            DetachRecord(records_[index - 1U]);
        if (!detached) return detached.GetStatus();
    }
    records_.Clear();
    Base::Result<void> reserved =
        records_.Reserve(owner_->GetCount());
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t index = 0U;
        index < owner_->GetCount(); ++index) {
        Base::Result<Record> made =
            CreateRecord(index);
        if (!made) {
            const Base::Status error = made.GetStatus();
            for (std::uint32_t cleanup = records_.Size();
                cleanup > 0U; --cleanup) {
                static_cast<void>(
                    DetachRecord(records_[cleanup - 1U]));
            }
            records_.Clear();
            return error;
        }
        Record record = std::move(made).Value();
        Base::Result<void> attached =
            AttachRecord(record, index);
        if (!attached) {
            const Base::Status error = attached.GetStatus();
            for (std::uint32_t cleanup = records_.Size();
                cleanup > 0U; --cleanup) {
                static_cast<void>(
                    DetachRecord(records_[cleanup - 1U]));
            }
            records_.Clear();
            return error;
        }
        Base::Result<void> added =
            records_.PushBack(
                std::move(record));
        if (!added) {
            const Base::Status error = added.GetStatus();
            static_cast<void>(DetachRecord(record));
            for (std::uint32_t cleanup = records_.Size();
                cleanup > 0U; --cleanup) {
                static_cast<void>(
                    DetachRecord(records_[cleanup - 1U]));
            }
            records_.Clear();
            return error;
        }
    }
    return {};
}

Base::Result<void> ItemContainerGeneratorRuntime::ApplyChange(
    const ItemsChangedEvent& event) noexcept {
    if (event.action == ItemsChangeAction::Reset) {
        return Refresh();
    }
    if (event.action == ItemsChangeAction::Add) {
        for (std::uint32_t offset = 0U;
            offset < event.newCount; ++offset) {
            const std::uint32_t index =
                event.newIndex + offset;
            Base::Result<Record> made =
                CreateRecord(index);
            if (!made) return made.GetStatus();
            Record record = std::move(made).Value();
            Base::Result<void> attached =
                AttachRecord(record, index);
            if (!attached) return attached.GetStatus();
            Base::Result<void> inserted =
                InsertRecord(
                    index, std::move(record));
            if (!inserted) {
                return inserted.GetStatus();
            }
        }
        return ReorderVisuals();
    }
    if (event.action == ItemsChangeAction::Remove) {
        if (event.oldIndex > records_.Size() ||
            event.oldCount >
                records_.Size() - event.oldIndex) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Items removal delta is invalid");
        }
        for (std::uint32_t count = 0U;
            count < event.oldCount; ++count) {
            Base::Result<void> detached =
                DetachRecord(
                    records_[event.oldIndex]);
            if (!detached) return detached.GetStatus();
            RemoveRecordAt(event.oldIndex);
        }
        return ReorderVisuals();
    }
    if (event.action == ItemsChangeAction::Replace) {
        if (event.oldIndex > records_.Size() ||
            event.oldCount >
                records_.Size() - event.oldIndex) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Items replacement delta is invalid");
        }
        for (std::uint32_t count = 0U;
            count < event.oldCount; ++count) {
            Base::Result<void> detached =
                DetachRecord(
                    records_[event.oldIndex]);
            if (!detached) return detached.GetStatus();
            RemoveRecordAt(event.oldIndex);
        }
        for (std::uint32_t offset = 0U;
            offset < event.newCount; ++offset) {
            const std::uint32_t index =
                event.newIndex + offset;
            Base::Result<Record> made =
                CreateRecord(index);
            if (!made) return made.GetStatus();
            Record record = std::move(made).Value();
            Base::Result<void> attached =
                AttachRecord(record, index);
            if (!attached) return attached.GetStatus();
            Base::Result<void> inserted =
                InsertRecord(
                    index, std::move(record));
            if (!inserted) {
                return inserted.GetStatus();
            }
        }
        return ReorderVisuals();
    }
    if (event.action == ItemsChangeAction::Move &&
        event.oldCount == 1U &&
        event.newCount == 1U &&
        event.oldIndex < records_.Size() &&
        event.newIndex < records_.Size()) {
        Record moving =
            std::move(records_[event.oldIndex]);
        if (event.oldIndex < event.newIndex) {
            for (std::uint32_t index = event.oldIndex;
                index < event.newIndex; ++index) {
                records_[index] =
                    std::move(records_[index + 1U]);
            }
        } else {
            for (std::uint32_t index = event.oldIndex;
                index > event.newIndex; --index) {
                records_[index] =
                    std::move(records_[index - 1U]);
            }
        }
        records_[event.newIndex] =
            std::move(moving);
        return ReorderVisuals();
    }
    return Refresh();
}

void ItemContainerGeneratorRuntime::OnItemsChanged(
    const ItemsChangedEvent& event) noexcept {
    Base::Result<void> applied;
    if (virtualizingHost_ != nullptr &&
        owner_ != nullptr) {
        applied =
            virtualizingHost_->HandleItemsChanged(
                event, owner_->GetCount());
        if (applied) {
            Base::Result<bool> realized =
                SetRealizationRangeInternal(
                    virtualizingHost_->
                        desiredFirstIndex_,
                    virtualizingHost_->
                        desiredCount_,
                    true);
            if (!realized) {
                applied = realized.GetStatus();
            }
        }
    } else {
        applied = ApplyChange(event);
    }
    lastError_ = applied
        ? Base::Status{}
        : applied.GetStatus();
    if (applied && owner_ != nullptr) {
        owner_->OnContainersChanged();
    }
}

FrameworkElement*
ItemContainerGeneratorRuntime::ContainerFromIndex(
    std::uint32_t index) const noexcept {
    return index >= firstGeneratedIndex_ &&
        index - firstGeneratedIndex_ <
            records_.Size()
        ? records_[
            index - firstGeneratedIndex_]
              .container.Get()
        : nullptr;
}

std::uint32_t
ItemContainerGeneratorRuntime::IndexFromContainer(
    const FrameworkElement& container) const noexcept {
    for (std::uint32_t index = 0U;
        index < records_.Size(); ++index) {
        if (records_[index].container.Get() ==
            &container) {
            return firstGeneratedIndex_ + index;
        }
    }
    return UINT32_MAX;
}

Base::Ref<Base::Object>
ItemContainerGeneratorRuntime::ItemFromContainer(
    const FrameworkElement& container) const noexcept {
    const std::uint32_t index =
        IndexFromContainer(container);
    return index != UINT32_MAX
        ? records_[
            index - firstGeneratedIndex_].item
        : Base::Ref<Base::Object>();
}

} // namespace Aero::Controls

namespace Aero::Controls {

ItemContainerGenerator::~ItemContainerGenerator() noexcept {
    delete static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    impl_ = nullptr;
}

Base::Result<void> ItemContainerGenerator::Attach(
    ItemsControl& owner,
    Panel& itemsHost) noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr
        ? runtime->Attach(owner, itemsHost)
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::NotInitialized,
              "ItemContainerGenerator is not initialized"));
}

Base::Result<void> ItemContainerGenerator::AttachVirtualized(
    ItemsControl& owner,
    VirtualizingStackPanel& itemsHost) noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr
        ? runtime->AttachVirtualized(owner, itemsHost)
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::NotInitialized,
              "ItemContainerGenerator is not initialized"));
}

Base::Result<bool> ItemContainerGenerator::Detach() noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->Detach() : Base::Result<bool>(false);
}

Base::Result<void> ItemContainerGenerator::Refresh() noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr
        ? runtime->Refresh()
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::NotInitialized,
              "ItemContainerGenerator is not initialized"));
}

void ItemContainerGenerator::SetRealizationRange(
    std::uint32_t firstIndex,
    std::uint32_t count) noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    if (runtime != nullptr) (void)runtime->SetRealizationRange(firstIndex, count);
}

std::uint32_t ItemContainerGenerator::GetGeneratedCount() const noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->GetGeneratedCount() : 0U;
}

std::uint32_t ItemContainerGenerator::GetFirstGeneratedIndex() const noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->GetFirstGeneratedIndex() : 0U;
}

std::uint32_t ItemContainerGenerator::GetCreatedContainerCount() const noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->GetCreatedContainerCount() : 0U;
}

std::uint32_t ItemContainerGenerator::GetRecycledContainerUseCount() const noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->GetRecycledContainerUseCount() : 0U;
}

FrameworkElement* ItemContainerGenerator::ContainerFromIndex(
    std::uint32_t index) const noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->ContainerFromIndex(index) : nullptr;
}

std::uint32_t ItemContainerGenerator::IndexFromContainer(
    const FrameworkElement& container) const noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr ? runtime->IndexFromContainer(container) : UINT32_MAX;
}

Base::Ref<Base::Object> ItemContainerGenerator::ItemFromContainer(
    const FrameworkElement& container) const noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr
        ? runtime->ItemFromContainer(container)
        : Base::Ref<Base::Object>{};
}

Base::Status ItemContainerGenerator::LastError() const noexcept {
    auto* runtime = static_cast<::Aero::Controls::ItemContainerGeneratorImpl*>(impl_);
    return runtime != nullptr
        ? runtime->LastError()
        : Base::Status::Failure(
              Base::ErrorCode::NotInitialized,
              "ItemContainerGenerator is not initialized");
}

} // namespace Aero::Controls

namespace Aero {

Base::Result<Controls::ItemContainerGenerator*>
AeroGuiInternal::CreateItemContainerGenerator(
    ElementTree& tree,
    Aero::LayoutEngine& layout,
    Meta::EffectiveValueEngine& values,
    Aero::StyleEngine* styles,
    ::Aero::Render::RenderTree* renderer,
    Controls::TemplateEngine* templates,
    Controls::ItemSubtreeCallback subtreeCallback,
    void* subtreeContext) noexcept {
    auto* generator = new (std::nothrow) Controls::ItemContainerGenerator();
    if (generator == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "ItemContainerGenerator allocation failed");
    }
    generator->impl_ = new (std::nothrow) Controls::ItemContainerGeneratorImpl(
        *generator,
        tree,
        layout,
        values,
        styles,
        renderer,
        templates,
        subtreeCallback,
        subtreeContext);
    if (generator->impl_ == nullptr) {
        delete generator;
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "ItemContainerGenerator runtime allocation failed");
    }
    return generator;
}

} // namespace Aero
