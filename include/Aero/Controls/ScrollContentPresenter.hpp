#pragma once

#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Controls/IScrollInfo.hpp>
#include <Aero/Events/ControlEventArgs.hpp>
#include <Aero/Input.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API ScrollContentPresenter
    : public ContentControl,
      public IScrollInfo {
    AERO_DECLARE_TYPE(ScrollContentPresenter, ContentControl)
public:
    ScrollContentPresenter() noexcept;
    ~ScrollContentPresenter() override = default;

    ScrollData GetData() const noexcept override;
    IScrollInfo* GetContentScrollInfo() const noexcept {
        return contentScrollInfo_;
    }
    void SetContentScrollInfo(
        IScrollInfo* value) noexcept;

    bool GetCanHorizontallyScroll() const noexcept;
    bool GetCanVerticallyScroll() const noexcept;
    bool GetCanContentScroll() const noexcept;
    void SetCanHorizontallyScroll(
        bool value) noexcept;
    void SetCanVerticallyScroll(
        bool value) noexcept;
    void SetCanContentScroll(
        bool value) noexcept;
    inline static constexpr DependencyProperty<bool> CanContentScrollProperty{"CanContentScroll"};

    void SetViewport(
        Size viewport) noexcept override;
    void SetHorizontalOffset(
        double value) noexcept override;
    void SetVerticalOffset(
        double value) noexcept override;
    Result<bool> LineHorizontal(
        double direction) noexcept override;
    Result<bool> LineVertical(
        double direction) noexcept override;
    Result<bool> PageHorizontal(
        double direction) noexcept override;
    Result<bool> PageVertical(
        double direction) noexcept override;
    Result<bool> ApplyScrollDelta(
        double deltaX,
        double deltaY,
        ScrollInputKind kind) noexcept;

    double GetLineScrollAmount() const noexcept {
        return lineScrollAmount_;
    }
    void SetLineScrollAmount(
        double value) noexcept;

protected:
    explicit ScrollContentPresenter(
        TypeId runtimeType) noexcept;
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;
    virtual void OnScrollDataChanged(
        const ScrollData& oldData,
        const ScrollData& newData,
        ScrollInputKind kind) noexcept;
    virtual bool GetAllowsHorizontalScroll() const noexcept;
    virtual bool GetAllowsVerticalScroll() const noexcept;
    virtual bool GetUsesContentScrolling() const noexcept;
    Result<bool> UpdateData(
        ScrollData value,
        ScrollInputKind kind,
        bool invalidateArrange) noexcept;

private:
    ScrollData data_;
    IScrollInfo* contentScrollInfo_ = nullptr;
    double lineScrollAmount_ = 16.0;
    bool canHorizontallyScroll_ = false;
    bool canVerticallyScroll_ = true;
    ScrollInputKind pendingInputKind_ = ScrollInputKind::Line;

    Result<bool> SyncLogicalData(
        ScrollInputKind kind) noexcept;
    IScrollInfo* ActiveContentScrollInfo() const noexcept;
};

} // namespace Aero::Controls
