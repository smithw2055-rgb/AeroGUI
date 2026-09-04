#pragma once

#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Data/SortDescription.hpp>
#include <Aero/Events/ControlEventArgs.hpp>
#include <cstdint>

namespace Aero::Data {

using CollectionViewFilter =
    Base::Delegate<bool(const Base::Object*)>;
using CurrentChangedHandler = Base::Delegate<void()>;

class AERO_GUI_API CollectionView :
    public Base::Object,
    public Collections::IItemsSource {
    AERO_DECLARE_TYPE(CollectionView, Base::Object)
public:
    explicit CollectionView(
        Collections::IItemsSource* source) noexcept;
    ~CollectionView() override;

    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::Object* AsObject() noexcept override {
        return this;
    }

    Collections::IItemsSource* GetSource() const noexcept {
        return inner_;
    }

    std::uint32_t GetCount() const noexcept override;
    Ref<Base::Object> GetItem(
        std::uint32_t index) const noexcept override;
    void AddItemsChanged(
        const Collections::ItemsChangedHandler& handler) noexcept override;
    bool RemoveItemsChanged(
        const Collections::ItemsChangedHandler& handler) noexcept override;

    void SetFilter(CollectionViewFilter filter) noexcept;
    const CollectionViewFilter& GetFilter() const noexcept { return filter_; }
    void SortBy(
        StringView propertyName,
        ListSortDirection direction = ListSortDirection::Ascending) noexcept;
    void ClearSort() noexcept;
    void Refresh() noexcept;

    Ref<Base::Object> GetCurrentItem() const noexcept;
    std::uint32_t GetCurrentPosition() const noexcept {
        return currentPosition_;
    }
    bool MoveCurrentTo(const Base::Object* item) noexcept;
    bool MoveCurrentToPosition(std::uint32_t index) noexcept;
    bool MoveCurrentToFirst() noexcept;
    bool MoveCurrentToLast() noexcept;
    bool MoveCurrentToNext() noexcept;
    bool MoveCurrentToPrevious() noexcept;

    void AddCurrentChanged(
        const CurrentChangedHandler& handler) noexcept;
    bool RemoveCurrentChanged(
        const CurrentChangedHandler& handler) noexcept;

private:
    void Rebuild() noexcept;
    bool Passes(const Base::Object* item) const noexcept;
    void OnInnerChanged(
        const Collections::ItemsChangedEvent& event) noexcept;
    void NotifyReset() noexcept;
    void RaiseCurrentChanged() noexcept;

    Collections::IItemsSource* inner_ = nullptr;
    Base::WeakRef<Base::Object> innerObj_;
    Base::Vector<std::uint32_t> map_;
    CollectionViewFilter filter_;
    String sortProperty_;
    ListSortDirection sortDirection_ = ListSortDirection::Ascending;
    bool hasSort_ = false;
    std::uint32_t currentPosition_ = UINT32_MAX;
    Collections::ItemsChangedHandler innerHandler_;
    Collections::ItemsChangedHandler changed_;
    CurrentChangedHandler currentChanged_;
};

} // namespace Aero::Data
