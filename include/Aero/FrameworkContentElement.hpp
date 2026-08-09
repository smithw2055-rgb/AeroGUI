#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyObject.hpp>
#include <Aero/Events/Event.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Resources.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Style.hpp>
#include <Aero/Visual.hpp>

#include <cstddef>
#include <new>

namespace Aero {

class UIElement;

// Non-visual WPF content node. ContentElement participates in dependency
// properties and routed events without becoming a ::Aero::Media::Visual or UIElement.
class AERO_GUI_API ContentElement : public DependencyObject {
    AERO_DECLARE_TYPE(ContentElement, DependencyObject)
public:
    template<class TArgs>
    using Event = ::Aero::Event<ContentElement, TArgs>;

    template<class TOwner, class TArgs>
    Event<TArgs> GetEvent(
        const RoutedEventRef<TOwner, TArgs>& event) noexcept {
        return Event<TArgs>(*this, event.Handle());
    }

    explicit ContentElement(Meta::TypeId runtimeType) noexcept;
    ~ContentElement() override;

    DependencyObject* GetParent() const noexcept { return logicalParent_; }
    UIElement* GetContentHost() const noexcept { return contentHost_; }

    template<class TArgs>
    Result<void> AddHandlerChecked(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler,
        bool handledEventsToo = false) noexcept {
        if (handler.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Routed event handler must not be empty");
        }
        return AddHandlerCore(event, DescribeHandler(handler), handledEventsToo);
    }

    template<class TArgs>
    void AddHandler(
        RoutedEventHandle event,
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler,
        bool handledEventsToo = false) noexcept {
        Result<void> added = AddHandlerChecked(event, handler, handledEventsToo);
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
    void RaiseEvent(
        RoutedEventHandle event,
        RoutedEventArgs* args = nullptr) noexcept;

private:
#if defined(AERO_GUI_IMPLEMENTATION)
    friend struct ::Aero::Media::Visual::Access;
#endif

    struct HandlerOperations {
        std::size_t size = 0U;
        std::size_t alignment = 0U;
        void (*copy)(void*, const void*) noexcept = nullptr;
        void (*destroy)(void*) noexcept = nullptr;
        bool (*equals)(const void*, const void*) noexcept = nullptr;
        void (*invoke)(const void*, Base::Object*, RoutedEventArgs&) noexcept = nullptr;
    };

    struct HandlerDescriptor {
        const void* value = nullptr;
        const HandlerOperations* operations = nullptr;
        Meta::TypeId argsType = Meta::InvalidTypeId;
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

    Result<void> AddHandlerCore(
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
    void* routedHandlers_ = nullptr;
};

// WPF-shaped non-visual content node with resources, DataContext, Style and
// logical-tree participation. TextElement and other document nodes derive here.
class AERO_GUI_API FrameworkContentElement : public ContentElement {
    AERO_DECLARE_TYPE(FrameworkContentElement, ContentElement)
public:
    explicit FrameworkContentElement(Meta::TypeId runtimeType) noexcept;
    ~FrameworkContentElement() override;

    ResourceDictionary& GetResources() noexcept { return resources_; }
    const ResourceDictionary& GetResources() const noexcept { return resources_; }
    void SetResources(Ref<ResourceDictionary> value) noexcept;

    Value GetDataContext() const noexcept {
        return GetValueOr(
            DataContextProperty,
            Value::NullObject(Meta::TypeOf<Base::Object>()));
    }
    void SetDataContext(Value value) noexcept {
        SetValue(DataContextProperty, std::move(value));
    }
    void SetDataContext(Ref<Base::Object> value) noexcept {
        SetDataContext(Value::FromObject(
            Meta::TypeOf<Base::Object>(), std::move(value)));
    }
    void ClearDataContext() noexcept {
        ClearValue(DataContextProperty);
    }

    Ref<Style> GetStyle() const noexcept {
        return GetValueOr(StyleProperty, Ref<Style>{});
    }
    void SetStyle(Ref<Style> value) noexcept {
        SetValue(StyleProperty, std::move(value));
    }

    bool GetIsEnabled() const noexcept {
        return GetValueOr(IsEnabledProperty, true);
    }
    void SetIsEnabled(bool value) noexcept {
        SetValue(IsEnabledProperty, value);
    }
    bool GetIsMouseOver() const noexcept {
        return GetValueOr(IsMouseOverProperty, false);
    }
    StringView GetCursor() const noexcept {
        return GetValueOr(CursorProperty, StringView{});
    }
    void SetCursor(StringView value) noexcept {
        SetValue(CursorProperty, value);
    }
    bool GetOverridesDefaultStyle() const noexcept {
        return GetValueOr(OverridesDefaultStyleProperty, false);
    }
    void SetOverridesDefaultStyle(bool value) noexcept {
        SetValue(OverridesDefaultStyleProperty, value);
    }

    inline static constexpr DependencyProperty<Value> DataContextProperty{"DataContext"};
    inline static constexpr DependencyProperty<Ref<Style>> StyleProperty{"Style"};
    inline static constexpr DependencyProperty<Value> TagProperty{"Tag"};
    inline static constexpr DependencyProperty<bool> IsEnabledProperty{"IsEnabled"};
    inline static constexpr ReadOnlyDependencyProperty<bool> IsMouseOverProperty{"IsMouseOver"};
    inline static constexpr DependencyProperty<String> CursorProperty{"Cursor"};
    inline static constexpr DependencyProperty<bool> OverridesDefaultStyleProperty{"OverridesDefaultStyle"};

protected:
    virtual std::uint32_t GetLogicalChildrenCount() const noexcept { return 0U; }
    virtual DependencyObject* GetLogicalChild(std::uint32_t) const noexcept { return nullptr; }

private:
#if defined(AERO_GUI_IMPLEMENTATION)
    friend struct ::Aero::Media::Visual::Access;
#endif
    Result<void> AddAuthoredTrigger(
        Ref<Base::Object> trigger) noexcept;
    void ClearAuthoredTriggers() noexcept;
    Span<const Ref<Base::Object>>
    AuthoredTriggers() const noexcept {
        return authoredTriggers_.AsSpan();
    }
    ResourceDictionary resources_;
    Base::Vector<Ref<Base::Object>> authoredTriggers_;
};

} // namespace Aero
