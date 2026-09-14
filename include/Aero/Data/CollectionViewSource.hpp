#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Data/CollectionView.hpp>

namespace Aero::Data {

class AERO_GUI_API CollectionViewSource : public Base::Object {
    AERO_DECLARE_TYPE(CollectionViewSource, Base::Object)
public:
    CollectionViewSource() noexcept = default;

    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    // Cached default view for an IItemsSource. A CollectionView argument is
    // returned as-is. SelectedItem remains Selector's selection authority;
    // CollectionView.CurrentItem is view currency only.
    static CollectionView* GetDefaultView(
        Collections::IItemsSource* source) noexcept;
};

} // namespace Aero::Data
