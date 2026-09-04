ScrollContentPresenter::ScrollContentPresenter() noexcept
    : ScrollContentPresenter(StaticTypeId()) {}

ScrollContentPresenter::ScrollContentPresenter(
    TypeId runtimeType) noexcept
    : ContentControl(runtimeType) {
    // WPF: ScrollContentPresenter clips; ScrollViewer does not. Aero's
    // ScrollViewer derives from ScrollContentPresenter, so only the actual
    // presenter type must opt in. Clipping the viewer as well nested two
    // stencil clips over the items host and, after Intro's offscreen fade,
    // discarded the list on the window target.
    if (runtimeType == ScrollContentPresenter::StaticTypeId()) {
        static_cast<void>(SetClipToBounds(true));
    }
}

IScrollInfo*
ScrollContentPresenter::ActiveContentScrollInfo() const noexcept {
    return GetUsesContentScrolling()
        ? contentScrollInfo_
        : nullptr;
}

ScrollData ScrollContentPresenter::GetData() const noexcept {
    IScrollInfo* logical = ActiveContentScrollInfo();
    return logical != nullptr ? logical->GetData() : data_;
}

void ScrollContentPresenter::SetContentScrollInfo(
    IScrollInfo* value) noexcept {
    if (contentScrollInfo_ == value) return;
    contentScrollInfo_ = value;
    (void)InvalidateMeasure();
    (void)SyncLogicalData(ScrollInputKind::Line);
}

bool ScrollContentPresenter::GetCanHorizontallyScroll() const noexcept {
    return GetAllowsHorizontalScroll();
}

bool ScrollContentPresenter::GetCanVerticallyScroll() const noexcept {
    return GetAllowsVerticalScroll();
}

bool ScrollContentPresenter::GetCanContentScroll() const noexcept {
    return GetUsesContentScrolling();
}

void ScrollContentPresenter::SetCanHorizontallyScroll(
    bool value) noexcept {
    if (canHorizontallyScroll_ == value) return;
    canHorizontallyScroll_ = value;
    (void)InvalidateMeasure();
}

void ScrollContentPresenter::SetCanVerticallyScroll(
    bool value) noexcept {
    if (canVerticallyScroll_ == value) return;
    canVerticallyScroll_ = value;
    (void)InvalidateMeasure();
}

void ScrollContentPresenter::SetCanContentScroll(
    bool value) noexcept {
    DependencyObject::SetValue(CanContentScrollProperty, value);
}

bool ScrollContentPresenter::GetAllowsHorizontalScroll() const noexcept {
    return canHorizontallyScroll_;
}

bool ScrollContentPresenter::GetAllowsVerticalScroll() const noexcept {
    return canVerticallyScroll_;
}

bool ScrollContentPresenter::GetUsesContentScrolling() const noexcept {
    return GetValueOr(
        CanContentScrollProperty, false);
}

void ScrollContentPresenter::SetLineScrollAmount(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return;
    }
    lineScrollAmount_ = value;
}

Base::Result<bool> ScrollContentPresenter::UpdateData(
    ScrollData value,
    ScrollInputKind kind,
    bool invalidateArrange) noexcept {
    if (!ValidData(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Scroll data must be finite and nonnegative");
    }
    value.horizontalOffset = ClampOffset(
        value.horizontalOffset,
        value.extentWidth,
        value.viewportWidth,
        GetAllowsHorizontalScroll());
    value.verticalOffset = ClampOffset(
        value.verticalOffset,
        value.extentHeight,
        value.viewportHeight,
        GetAllowsVerticalScroll());
    if (SameData(data_, value)) return false;
    const ScrollData oldData = data_;
    data_ = value;
    pendingInputKind_ = kind;
    if (invalidateArrange) {
        Base::Result<void> invalidated =
            InvalidateArrange();
        if (!invalidated) {
            data_ = oldData;
            return invalidated.GetStatus();
        }
    }
    OnScrollDataChanged(oldData, data_, kind);
    return true;
}

Base::Result<bool>
ScrollContentPresenter::SyncLogicalData(
    ScrollInputKind kind) noexcept {
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical == nullptr) return false;
    ScrollData value = logical->GetData();
    if (!ValidData(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Logical scrolling provider returned invalid data");
    }
    if (SameData(data_, value)) return false;
    const ScrollData oldData = data_;
    data_ = value;
    pendingInputKind_ = kind;
    OnScrollDataChanged(oldData, data_, kind);
    return true;
}

void ScrollContentPresenter::SetViewport(
    Size viewport) noexcept {
    if (!IsFinite(viewport) ||
        viewport.width < 0.0 ||
        viewport.height < 0.0) {
        return;
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        logical->SetViewport(viewport);
        (void)SyncLogicalData(pendingInputKind_);
        return;
    }
    ScrollData value = data_;
    value.viewportWidth = viewport.width;
    value.viewportHeight = viewport.height;
    (void)UpdateData(value, pendingInputKind_, true);
}

void ScrollContentPresenter::SetHorizontalOffset(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return;
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        logical->SetHorizontalOffset(value);
        (void)SyncLogicalData(pendingInputKind_);
        return;
    }
    ScrollData data = data_;
    data.horizontalOffset = value;
    (void)UpdateData(data, pendingInputKind_, true);
}

void ScrollContentPresenter::SetVerticalOffset(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return;
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        logical->SetVerticalOffset(value);
        (void)SyncLogicalData(pendingInputKind_);
        return;
    }
    ScrollData data = data_;
    data.verticalOffset = value;
    (void)UpdateData(data, pendingInputKind_, true);
}

Base::Result<bool>
ScrollContentPresenter::ApplyScrollDelta(
    double deltaX,
    double deltaY,
    ScrollInputKind kind) noexcept {
    if (!std::isfinite(deltaX) ||
        !std::isfinite(deltaY)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Scroll delta must be finite");
    }
    pendingInputKind_ = kind;
    const ScrollData current = GetData();
    SetHorizontalOffset(std::max(
        0.0, current.horizontalOffset + deltaX));
    pendingInputKind_ = kind;
    const ScrollData afterHorizontal = GetData();
    SetVerticalOffset(std::max(
        0.0, afterHorizontal.verticalOffset + deltaY));
    pendingInputKind_ = ScrollInputKind::Line;
    const ScrollData afterVertical = GetData();
    return afterHorizontal.horizontalOffset != afterVertical.horizontalOffset ||
        afterHorizontal.verticalOffset != afterVertical.verticalOffset;
}

Base::Result<bool>
ScrollContentPresenter::LineHorizontal(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Line scroll direction must be finite");
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        pendingInputKind_ = ScrollInputKind::Line;
        Base::Result<bool> changed =
            logical->LineHorizontal(direction);
        if (!changed) return changed.GetStatus();
        Base::Result<bool> synced =
            SyncLogicalData(ScrollInputKind::Line);
        pendingInputKind_ = ScrollInputKind::Line;
        if (!synced) return synced.GetStatus();
        return changed.Value() || synced.Value();
    }
    return ApplyScrollDelta(
        direction * lineScrollAmount_,
        0.0,
        ScrollInputKind::Line);
}

Base::Result<bool>
ScrollContentPresenter::LineVertical(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Line scroll direction must be finite");
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        pendingInputKind_ = ScrollInputKind::Line;
        Base::Result<bool> changed =
            logical->LineVertical(direction);
        if (!changed) return changed.GetStatus();
        Base::Result<bool> synced =
            SyncLogicalData(ScrollInputKind::Line);
        pendingInputKind_ = ScrollInputKind::Line;
        if (!synced) return synced.GetStatus();
        return changed.Value() || synced.Value();
    }
    return ApplyScrollDelta(
        0.0,
        direction * lineScrollAmount_,
        ScrollInputKind::Line);
}

Base::Result<bool>
ScrollContentPresenter::PageHorizontal(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Page scroll direction must be finite");
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        pendingInputKind_ = ScrollInputKind::Page;
        Base::Result<bool> changed =
            logical->PageHorizontal(direction);
        if (!changed) return changed.GetStatus();
        Base::Result<bool> synced =
            SyncLogicalData(ScrollInputKind::Page);
        pendingInputKind_ = ScrollInputKind::Line;
        if (!synced) return synced.GetStatus();
        return changed.Value() || synced.Value();
    }
    return ApplyScrollDelta(
        direction * GetData().viewportWidth,
        0.0,
        ScrollInputKind::Page);
}

Base::Result<bool>
ScrollContentPresenter::PageVertical(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Page scroll direction must be finite");
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    if (logical != nullptr) {
        pendingInputKind_ = ScrollInputKind::Page;
        Base::Result<bool> changed =
            logical->PageVertical(direction);
        if (!changed) return changed.GetStatus();
        Base::Result<bool> synced =
            SyncLogicalData(ScrollInputKind::Page);
        pendingInputKind_ = ScrollInputKind::Line;
        if (!synced) return synced.GetStatus();
        return changed.Value() || synced.Value();
    }
    return ApplyScrollDelta(
        0.0,
        direction * GetData().viewportHeight,
        ScrollInputKind::Page);
}

Size
ScrollContentPresenter::MeasureOverride(
    Size availableSize) noexcept {
    // ScrollViewer derives from this type and measures its chrome template
    // here. The presenter itself must still record extent from Content;
    // otherwise Auto scrollbars stay Collapsed and the thumb never appears.
    if (RuntimeType() != StaticTypeId() &&
        GetTemplateRoot() != nullptr) {
        return ContentControl::MeasureOverride(
            availableSize);
    }
    UIElement* child = ContentElement();
    if (child == nullptr) {
        ScrollData empty = data_;
        empty.extentWidth = 0.0;
        empty.extentHeight = 0.0;
        empty.viewportWidth = availableSize.width;
        empty.viewportHeight = availableSize.height;
        Base::Result<bool> updated = UpdateData(
            empty, pendingInputKind_, false);
        if (!updated) return Size{};
        return Size{};
    }

    IScrollInfo* logical = ActiveContentScrollInfo();
    Size childAvailable = availableSize;
    if (logical != nullptr) {
        logical->SetViewport(availableSize);
    } else {
        if (GetAllowsHorizontalScroll()) {
            childAvailable.width = LayoutInfinity;
        }
        if (GetAllowsVerticalScroll()) {
            childAvailable.height = LayoutInfinity;
        }
    }
    Base::Result<void> measured =
        MeasureChild(*child, childAvailable);
    if (!measured) return Size{};

    if (logical != nullptr) {
        Base::Result<bool> synced =
            SyncLogicalData(pendingInputKind_);
        if (!synced) return Size{};
    } else {
        ScrollData value = data_;
        value.extentWidth = child->GetDesiredSize().width;
        value.extentHeight = child->GetDesiredSize().height;
        value.viewportWidth = availableSize.width;
        value.viewportHeight = availableSize.height;
        Base::Result<bool> updated = UpdateData(
            value, pendingInputKind_, false);
        if (!updated) return Size{};
    }
    const ScrollData value = GetData();
    return Size{
        std::min(value.extentWidth, availableSize.width),
        std::min(value.extentHeight, availableSize.height)};
}

Size
ScrollContentPresenter::ArrangeOverride(
    Size finalSize) noexcept {
    if (RuntimeType() != StaticTypeId() &&
        GetTemplateRoot() != nullptr) {
        return ContentControl::ArrangeOverride(
            finalSize);
    }
    UIElement* child = ContentElement();
    if (child == nullptr) {
        SetViewport(finalSize);
        return finalSize;
    }
    IScrollInfo* logical = ActiveContentScrollInfo();
    SetViewport(finalSize);
    const ScrollData value = GetData();
    const Rect slot = logical != nullptr
        ? Rect{0.0, 0.0, finalSize.width, finalSize.height}
        : Rect{
            -value.horizontalOffset,
            -value.verticalOffset,
            std::max(value.extentWidth, finalSize.width),
            std::max(value.extentHeight, finalSize.height)};
    Base::Result<void> arranged =
        ArrangeChild(*child, slot);
    if (!arranged) return finalSize;
    return finalSize;
}

void ScrollContentPresenter::OnScrollDataChanged(
    const ScrollData&,
    const ScrollData& newData,
    ScrollInputKind kind) noexcept {
    DependencyObject* templatedParent =
        GetTemplatedParent();
    if (templatedParent == nullptr ||
        templatedParent == this ||
        !PropertyRegistry().Types().IsDerivedFrom(
            templatedParent->RuntimeType(),
            ScrollViewer::StaticTypeId())) {
        return;
    }
    static_cast<ScrollViewer*>(templatedParent)->
        AdoptPresenterData(*this, newData, kind);
}

