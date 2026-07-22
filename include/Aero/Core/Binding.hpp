#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/Dispatcher.hpp>

#include <cstdint>

namespace Aero::Core {

enum class BindingMode : std::uint8_t {
    OneTime = 0U,
    OneWay,
    TwoWay,
    OneWayToSource
};

struct BindingHandle final {
    std::uint64_t value = 0U;

    AERO_NODISCARD constexpr bool IsValid() const noexcept {
        return value != 0U;
    }
};

struct BindingDescriptor final {
    DependencyObject* source = nullptr;
    DependencyPropertyHandle sourceProperty;
    DependencyObject* target = nullptr;
    DependencyPropertyHandle targetProperty;
    BindingMode mode = BindingMode::OneWay;
};

// A host-owned binding scheduler. This first slice supports direct
// DependencyObject-to-DependencyObject bindings. It samples source/target
// values during DispatcherFramePhase::DataBind; path accessors, DataContext
// selection, and property-change subscriptions build on this contract in later
// slices. TwoWay conflicts in the same phase are resolved source-to-target.
// Binding records are non-owning: a host must Detach() the binding or call
// Shutdown() before either source or target object is destroyed.
class AERO_API BindingManager final {
public:
    explicit BindingManager(
        Dispatcher& dispatcher,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~BindingManager() noexcept;

    BindingManager(const BindingManager&) = delete;
    BindingManager& operator=(const BindingManager&) = delete;

    AERO_NODISCARD Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    AERO_NODISCARD Base::Result<BindingHandle> Attach(
        const BindingDescriptor& descriptor) noexcept;
    AERO_NODISCARD Base::Result<bool> Detach(BindingHandle handle) noexcept;

    // Removes every binding whose source or target is object. Tree/object
    // ownership code uses this before destroying a DependencyObject.
    AERO_NODISCARD Base::Result<std::uint32_t> DetachObject(
        DependencyObject& object) noexcept;

    // Flush is also exposed for deterministic headless tests. Normal hosts run
    // it through the DataBind frame phase registered by Initialize().
    AERO_NODISCARD Base::Result<std::uint32_t> Flush() noexcept;

    AERO_NODISCARD bool IsInitialized() const noexcept {
        return hook_.IsValid();
    }
    AERO_NODISCARD std::uint32_t BindingCount() const noexcept {
        return bindings_.Size();
    }
    AERO_NODISCARD Base::Status LastError() const noexcept {
        return lastError_;
    }

private:
    struct BindingRecord final {
        BindingHandle handle;
        BindingDescriptor descriptor;
        PropertyValue lastSourceValue;
        PropertyValue lastTargetValue;
        DependencyPropertyChangeSubscription sourceSubscription;
        DependencyPropertyChangeSubscription targetSubscription;
        bool applied = false;
        bool sourceDirty = true;
        bool targetDirty = true;
    };

    Dispatcher* dispatcher_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<BindingRecord> bindings_;
    DispatcherFrameHookHandle hook_;
    std::uint64_t nextHandle_ = 1U;
    bool flushing_ = false;
    Base::Status lastError_;

    static void DataBindHook(void* context) noexcept;
    static void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args,
        void* context) noexcept;
    AERO_NODISCARD Base::Result<void> VerifyDescriptor(
        const BindingDescriptor& descriptor) const noexcept;
    void RemoveAt(std::uint32_t index) noexcept;
};

} // namespace Aero::Core
