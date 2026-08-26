#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Media/GradientStop.hpp>

namespace Aero::Media {

class AERO_GUI_API GradientStopCollection :
    public Freezable,
    public Collections::IItemsSource {
    AERO_DECLARE_TYPE(GradientStopCollection, Freezable)
public:
    GradientStopCollection() noexcept
        : Freezable(StaticTypeId()),
          stops_(&Base::GetDefaultAllocator()) {}
    ~GradientStopCollection() override;
    Span<const Ref<GradientStop>>
    GetItems() const noexcept {
        return stops_.AsSpan();
    }
    std::uint32_t GetCount() const noexcept override {
        return stops_.Size();
    }
    Ref<Base::Object> GetItem(
        std::uint32_t index) const noexcept override {
        return index < stops_.Size()
            ? Ref<Base::Object>(stops_[index])
            : Ref<Base::Object>{};
    }
    void AddItemsChanged(
        const Collections::ItemsChangedHandler& handler) noexcept override {
        if (IsFrozen()) return;
        changed_.Add(handler);
    }
    bool RemoveItemsChanged(
        const Collections::ItemsChangedHandler& handler) noexcept override {
        return changed_.Remove(handler);
    }
    Result<void> Add(
        Ref<GradientStop> stop) noexcept;
    void Clear() noexcept;
protected:
    bool FreezeCore(bool isChecking) noexcept override;
private:
    void OnStopChanged(Freezable&) noexcept;
    Base::Vector<Ref<GradientStop>> stops_;
    Collections::ItemsChangedHandler changed_;
    FreezableChangedHandler stopChangedHandler_;
};
} // namespace Aero::Media
