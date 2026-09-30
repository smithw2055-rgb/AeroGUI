#pragma once

// Src-only free-function bridge for FrameworkElement / FrameworkContentElement
// private authored/style seams. Engine classes that are already friends should
// call those private methods directly instead of growing this helper.

#include <Aero/FrameworkElement.hpp>
#include <Aero/FrameworkContentElement.hpp>

namespace Aero {

class FrameworkElementSeams {
public:
    static void AddAuthoredTrigger(FrameworkElement& e, Ref<Base::Object> trigger) noexcept {
        e.AddAuthoredTrigger(std::move(trigger));
    }
    static void ClearAuthoredTriggers(FrameworkElement& e) noexcept {
        e.ClearAuthoredTriggers();
    }
    static Span<const Ref<Base::Object>> AuthoredTriggers(const FrameworkElement& e) noexcept {
        return e.AuthoredTriggers();
    }
    static void AddAuthoredBehavior(FrameworkElement& e, Ref<Base::Object> behavior) noexcept {
        e.AddAuthoredBehavior(std::move(behavior));
    }
    static void ClearAuthoredBehaviors(FrameworkElement& e) noexcept {
        e.ClearAuthoredBehaviors();
    }
    static Span<const Ref<Base::Object>> AuthoredBehaviors(const FrameworkElement& e) noexcept {
        return e.AuthoredBehaviors();
    }
    static void AddStyleBehaviorPrototype(FrameworkElement& e, Ref<Base::Object> behavior) noexcept {
        e.AddStyleBehaviorPrototype(std::move(behavior));
    }
    static void ClearStyleBehaviorPrototypes(FrameworkElement& e) noexcept {
        e.ClearStyleBehaviorPrototypes();
    }
    static Span<const Ref<Base::Object>> StyleBehaviorPrototypes(const FrameworkElement& e) noexcept {
        return e.StyleBehaviorPrototypes();
    }
    static void AddStyleTriggerPrototype(FrameworkElement& e, Ref<Base::Object> trigger) noexcept {
        e.AddStyleTriggerPrototype(std::move(trigger));
    }
    static void ClearStyleTriggerPrototypes(FrameworkElement& e) noexcept {
        e.ClearStyleTriggerPrototypes();
    }
    static Span<const Ref<Base::Object>> StyleTriggerPrototypes(const FrameworkElement& e) noexcept {
        return e.StyleTriggerPrototypes();
    }
};

class FrameworkContentElementSeams {
public:
    static void AddAuthoredTrigger(FrameworkContentElement& e, Ref<Base::Object> trigger) noexcept {
        e.AddAuthoredTrigger(std::move(trigger));
    }
    static void ClearAuthoredTriggers(FrameworkContentElement& e) noexcept {
        e.ClearAuthoredTriggers();
    }
    static Span<const Ref<Base::Object>> AuthoredTriggers(const FrameworkContentElement& e) noexcept {
        return e.AuthoredTriggers();
    }
};

} // namespace Aero
