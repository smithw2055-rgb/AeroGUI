#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/DependencyObject.hpp>
#include <Aero/Resources.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Style.hpp>

#include <cstddef>
#include <new>

namespace Aero::Detail {
class ContentElementAccess;
class EventRouter;
}

namespace Aero {

class UIElement;

// Non-visual WPF content node. ContentElement participates in dependency
// properties and routed events without becoming a Visual or UIElement.
class AERO_API ContentElement : public DependencyObject {
    AERO_DECLARE_TYPE(ContentElement, DependencyObject)
public:
    template<class TArgs>
    class Event final {
    public:
        using Handler = Base::Delegate<void(Base::Object*, TArgs&)>;

        Event(ContentElement& element, RoutedEventHandle event) noexcept
            : element_(&element), event_(event) {}

        Base::Result<void> TryAdd(
            const Handler& handler,
            bool handledEventsToo = false) noexcept {
            return element_->TryAddHandler(event_, handler, handledEventsToo);
        }

        void Add(
            const Handler& handler,
            bool handledEventsToo = false) noexcept {
            Base::Result<void> result = TryAdd(handler, handledEventsToo);
            if (!result) {
                Base::ReportOutOfMemory(
                    sizeof(Handler),
                    alignof(Handler),
                    Base::MemoryTag::General);
            }
        }

        void operator+=(const Handler& handler) noexcept { Add(handler); }

        bool Remove(const Handler& handler) noexcept {
            return element_->RemoveHandler(event_, handler);
        }

        void operator-=(const Handler& handler) noexcept {
            static_cast<void>(Remove(handler));
        }

    private:
        ContentElement* element_ = nullptr;
        RoutedEventHandle event_;
    };

    template<class TOwner, class TArgs>
    Event<TArgs> GetEvent(
        const RoutedEventRef<TOwner, TArgs>& event) noexcept {
        return Event<TArgs>(*this, event.Handle());
    }

    explicit ContentElement(Core::TypeId runtimeType) noexcept;
    ~ContentElement() override;

    DependencyObject* GetParent() const noexcept { return logicalParent_; }
    UIElement* GetContentHost() const noexcept { return contentHost_; }

    template<class TArgs>
    Base::Result<void> TryAddHandler(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler,
        bool handledEventsToo = false) noexcept {
        if (handler.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Routed event handler must not be empty");
        }
        return TryAddHandlerCore(event, DescribeHandler(handler), handledEventsToo);
    }

    template<class TArgs>
    void AddHandler(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler,
        bool handledEventsToo = false) noexcept {
        Base::Result<void> added = TryAddHandler(event, handler, handledEventsToo);
        if (!added) {
            Base::ReportOutOfMemory(
                sizeof(handler),
                alignof(decltype(handler)),
                Base::MemoryTag::General);
        }
    }

    template<class TArgs>
    bool RemoveHandler(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler) noexcept {
        return RemoveHandlerCore(event, DescribeHandler(handler));
    }

protected:
    Base::Result<void> RaiseEvent(
        RoutedEventHandle event,
        RoutedEventArgs* args = nullptr) noexcept;

private:
    friend class Aero::Detail::ContentElementAccess;
    friend class Aero::Detail::EventRouter;

    struct HandlerOperations final {
        std::size_t size = 0U;
        std::size_t alignment = 0U;
        void (*copy)(void*, const void*) noexcept = nullptr;
        void (*destroy)(void*) noexcept = nullptr;
        bool (*equals)(const void*, const void*) noexcept = nullptr;
        void (*invoke)(const void*, Base::Object*, RoutedEventArgs&) noexcept = nullptr;
    };

    struct HandlerDescriptor final {
        const void* value = nullptr;
        const HandlerOperations* operations = nullptr;
        Core::TypeId argsType = Core::InvalidTypeId;
    };

    template<class TArgs>
    static HandlerDescriptor DescribeHandler(
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler) noexcept {
        using Handler = Base::Delegate<void(Base::Object*, TArgs&)>;
        static const HandlerOperations operations{
            sizeof(Handler),
            alignof(Handler),
            [](void* destination, const void* source) noexcept {
                new (destination) Handler(*static_cast<const Handler*>(source));
            },
            [](void* value) noexcept {
                static_cast<Handler*>(value)->~Handler();
            },
            [](const void* left, const void* right) noexcept {
                return *static_cast<const Handler*>(left) ==
                    *static_cast<const Handler*>(right);
            },
            [](const void* value,
               Base::Object* sender,
               RoutedEventArgs& args) noexcept {
                static_cast<const Handler*>(value)->Invoke(
                    sender, static_cast<TArgs&>(args));
            }};
        return {&handler, &operations, TArgs::StaticTypeId()};
    }

    Base::Result<void> TryAddHandlerCore(
        RoutedEventHandle event,
        const HandlerDescriptor& handler,
        bool handledEventsToo) noexcept;
    bool RemoveHandlerCore(
        RoutedEventHandle event,
        const HandlerDescriptor& handler) noexcept;
    void InvokeHandlers(
        RoutedEventHandle event,
        RoutedEventArgs& args) noexcept;
    void CleanupHandlers() noexcept;

    DependencyObject* logicalParent_ = nullptr;
    UIElement* contentHost_ = nullptr;
    void* eventRouter_ = nullptr;
    void* handlerState_ = nullptr;
};

// WPF-shaped non-visual content node with resources, DataContext, Style and
// logical-tree participation. TextElement and other document nodes derive here.
class AERO_API FrameworkContentElement : public ContentElement {
    AERO_DECLARE_TYPE(FrameworkContentElement, ContentElement)
public:
    explicit FrameworkContentElement(Core::TypeId runtimeType) noexcept;
    ~FrameworkContentElement() override;

    ResourceDictionary& GetResources() noexcept { return resources_; }
    const ResourceDictionary& GetResources() const noexcept { return resources_; }
    Base::Result<void> SetResources(Base::Ref<ResourceDictionary> value) noexcept;

    Base::Ref<Base::Object> GetDataContext() const noexcept {
        return GetValueOr(DataContextProperty, Base::Ref<Base::Object>{});
    }
    Base::Result<void> SetDataContext(Base::Ref<Base::Object> value) noexcept {
        return SetValue(DataContextProperty, std::move(value));
    }
    Base::Result<void> ClearDataContext() noexcept {
        return ClearValue(DataContextProperty.Handle());
    }

    Base::Ref<Style> GetStyle() const noexcept {
        return GetValueOr(StyleProperty, Base::Ref<Style>{});
    }
    Base::Result<void> SetStyle(Base::Ref<Style> value) noexcept {
        return SetValue(StyleProperty, std::move(value));
    }

    inline static constexpr Members::Property<Base::Ref<Base::Object>> DataContextProperty{"DataContext"};
    inline static constexpr Members::Property<Base::Ref<Style>> StyleProperty{"Style"};
    inline static constexpr Members::Property<Core::Value> TagProperty{"Tag"};

protected:
    virtual std::uint32_t GetLogicalChildrenCount() const noexcept { return 0U; }
    virtual DependencyObject* GetLogicalChild(std::uint32_t) const noexcept { return nullptr; }

private:
    friend class Aero::Detail::ContentElementAccess;
    ResourceDictionary resources_;
};

} // namespace Aero
