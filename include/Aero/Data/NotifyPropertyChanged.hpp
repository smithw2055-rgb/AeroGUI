#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero::Data {

AERO_GUI_API Meta::MemberId FindNotifyPropertyMember(
    Base::Object& object,
    Base::StringView propertyName) noexcept;

// WPF INotifyPropertyChanged. View-models inherit
// `NotifyPropertyChanged<MyViewModel>` and call RaisePropertyChanged("Foo")
// from setters. Meta::Register<MyViewModel>(...).PropertyChangeNotifications()
// wires the standard metadata subscribe hooks.
template<class TDerived>
class NotifyPropertyChanged {
public:
    using PropertyChangedCallback = void (*)(
        Base::Object& object,
        Meta::MemberId property,
        void* context) noexcept;

    void RaisePropertyChanged(Base::StringView propertyName) noexcept {
        Base::Object& self = static_cast<TDerived&>(*this);
        Notify(self, FindNotifyPropertyMember(self, propertyName));
    }
    void RaisePropertyChanged(Meta::MemberId property) noexcept {
        Notify(static_cast<TDerived&>(*this), property);
    }

    static Base::Result<std::uint64_t> SubscribePropertyChanged(
        Base::Object& object,
        PropertyChangedCallback callback,
        void* callbackContext,
        void*) noexcept {
        return static_cast<TDerived&>(object).Subscribe(
            callback, callbackContext);
    }
    static Base::Result<bool> UnsubscribePropertyChanged(
        Base::Object& object,
        std::uint64_t subscription,
        void*) noexcept {
        return static_cast<TDerived&>(object).Unsubscribe(subscription);
    }

private:
    struct Handler {
        PropertyChangedCallback callback = nullptr;
        void* context = nullptr;
        std::uint64_t id = 0U;
    };

    Base::Result<std::uint64_t> Subscribe(
        PropertyChangedCallback callback,
        void* callbackContext) noexcept {
        if (callback == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "NotifyPropertyChanged subscribe callback is null");
        }
        const std::uint64_t id = nextId_++;
        if (nextId_ == 0U) {
            nextId_ = 1U;
        }
        Base::Result<void> stored = handlers_.PushBack({
            callback, callbackContext, id});
        if (!stored) return stored.GetStatus();
        return id;
    }
    Base::Result<bool> Unsubscribe(std::uint64_t subscription) noexcept {
        if (subscription == 0U) return false;
        for (std::uint32_t index = 0U; index < handlers_.Size(); ++index) {
            if (handlers_[index].id != subscription) {
                continue;
            }
            for (std::uint32_t current = index + 1U;
                 current < handlers_.Size(); ++current) {
                handlers_[current - 1U] = handlers_[current];
            }
            handlers_.PopBack();
            return true;
        }
        return false;
    }
    void Notify(
        Base::Object& self,
        Meta::MemberId property) noexcept {
        const std::uint32_t count = handlers_.Size();
        for (std::uint32_t index = 0U; index < count; ++index) {
            const Handler handler = handlers_[index];
            if (handler.callback != nullptr) {
                handler.callback(self, property, handler.context);
            }
        }
    }

    Base::Vector<Handler> handlers_;
    std::uint64_t nextId_ = 1U;
};

} // namespace Aero::Data
