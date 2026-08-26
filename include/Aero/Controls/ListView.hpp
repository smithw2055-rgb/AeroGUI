#pragma once

#include <Aero/Controls/ListBox.hpp>
#include <Aero/Controls/ListViewItem.hpp>
#include <Aero/Controls/GridView.hpp>
#include <Aero/Controls/GridViewHeaderRowPresenter.hpp>
#include <Aero/Controls/GridViewRowPresenter.hpp>
#include <Aero/Controls/TextBlock.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API ListView
    : public ListBox {
    AERO_DECLARE_TYPE(ListView, ListBox)
public:
    ListView() noexcept
        : ListBox(StaticTypeId()) {}
    ~ListView() override = default;

    Ref<GridView> GetView() const noexcept;
    void SetView(
        Ref<GridView> value) noexcept;

    inline static constexpr DependencyProperty<Ref<GridView>> ViewProperty{"View"};

protected:
    void
        OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    Result<Ref<FrameworkElement>>
        CreateContainer(
            const Ref<Base::Object>& item)
            noexcept override;

private:
    TextBlock* columnHeaders_ = nullptr;
    Result<void>
        SynchronizeColumnHeaders() noexcept;
};
} // namespace Aero::Controls
