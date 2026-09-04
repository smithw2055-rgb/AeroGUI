Base::Result<void> TextBox::MoveCaretHorizontal(
    double direction,
    bool extend) noexcept {
    const TextSelection old =
        Model(model_).Selection();
    std::uint32_t next = old.caret;
    if (!extend && !old.GetIsEmpty()) {
        next = direction < 0.0
            ? old.GetStart() : old.GetEnd();
    } else if (direction < 0.0) {
        if (next != 0U) {
            --next;
        }
    } else if (next < Model(model_).GraphemeCount()) {
        ++next;
    }
    SetSelection(extend ? old.anchor : next, next);
    return {};
}

Base::Result<void>
TextBox::MoveCaretLineBoundary(
    bool end,
    bool extend) noexcept {
    const TextSelection old =
        Model(model_).Selection();
    std::uint32_t lineIndex = 0U;
    for (std::uint32_t line = 0U;
         line < Model(model_).LineCount(); ++line) {
        Base::Result<TextRange> range =
            Model(model_).LineRange(line);
        if (!range) {
            return range.GetStatus();
        }
        const std::uint32_t nextStart =
            line + 1U < Model(model_).LineCount()
            ? range.Value().GetEnd() + 1U
            : Model(model_).GraphemeCount() + 1U;
        if (old.caret < nextStart) {
            lineIndex = line;
            break;
        }
    }
    Base::Result<TextRange> range =
        Model(model_).LineRange(lineIndex);
    if (!range) {
        return range.GetStatus();
    }
    const std::uint32_t next = end
        ? range.Value().GetEnd()
        : range.Value().start;
    SetSelection(extend ? old.anchor : next, next);
    return {};
}

double TextBox::GetLineHeight() const noexcept {
    if (!caretStops_.Empty() &&
        caretStops_[0].height > 0.0) {
        return caretStops_[0].height;
    }
    return GetFontSize() * 1.6 /
        std::max(1.0, GetDpiScale());
}

Rect TextBox::GetCaretRectangle() const noexcept {
    if (caretStops_.Empty()) {
        return {
            -scroll_.horizontalOffset,
            -scroll_.verticalOffset,
            CaretWidth / std::max(1.0, GetDpiScale()),
            GetLineHeight()};
    }
    const std::uint32_t index =
        std::min(
            Model(GetActiveModel()).Caret(),
            caretStops_.Size() - 1U);
    const CaretStop& stop =
        caretStops_[index];
    return {
        stop.x - scroll_.horizontalOffset,
        stop.y - scroll_.verticalOffset,
        CaretWidth / std::max(1.0, GetDpiScale()),
        stop.height};
}

std::uint32_t TextBox::HitTestText(
    Point position) const noexcept {
    if (caretStops_.Empty()) {
        return 0U;
    }
    const double x =
        position.x -
            GetPadding().left +
            scroll_.horizontalOffset;
    const double y =
        position.y -
            GetPadding().top +
            scroll_.verticalOffset;
    std::uint32_t best = 0U;
    double bestDistance =
        std::numeric_limits<double>::infinity();
    for (std::uint32_t index = 0U;
         index < caretStops_.Size(); ++index) {
        const CaretStop& stop =
            caretStops_[index];
        const double vertical =
            y < stop.y
            ? stop.y - y
            : (y > stop.y + stop.height
                ? y - (stop.y + stop.height)
                : 0.0);
        const double distance =
            vertical * 10000.0 +
            std::abs(x - stop.x);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = index;
        }
    }
    return best;
}

Base::Result<void>
TextBox::RebuildCaretStops() noexcept {
    caretStops_.Clear();
    const auto& active = Model(GetActiveModel());
    const std::uint32_t graphemes =
        active.GraphemeCount();
    Base::Result<void> capacity =
        caretStops_.Reserve(
            graphemes + 1U);
    if (!capacity) {
        return capacity;
    }
    const std::uint32_t lines =
        std::max(1U, active.LineCount());
    std::uint32_t maximumLineLength = 0U;
    for (std::uint32_t line = 0U;
         line < lines; ++line) {
        Base::Result<TextRange> range =
            active.LineRange(line);
        if (!range) {
            return range.GetStatus();
        }
        maximumLineLength =
            std::max(
                maximumLineLength,
                range.Value().length);
    }
    std::uint32_t visualLines = 0U;
    for (std::uint32_t line = 0U;
         line < lines; ++line) {
        Base::Result<TextRange> range =
            active.LineRange(line);
        if (!range) {
            return range.GetStatus();
        }
        const std::uint32_t length =
            range.Value().length;
        const std::uint32_t wrapped =
            wrapColumns_ == UINT32_MAX
            ? 1U
            : std::max(
                  1U,
                  (length + wrapColumns_ - 1U) /
                      wrapColumns_);
        visualLines += wrapped;
    }
    visualLines = std::max(1U, visualLines);
    const double lineHeight =
        textSize_.height > 0.0
        ? textSize_.height /
            static_cast<double>(visualLines)
        : GetFontSize() * 1.6 /
            std::max(1.0, GetDpiScale());
    const double advance =
        wrapColumns_ != UINT32_MAX
        ? DefaultAdvance *
              GetFontSize() / 16.0 /
              std::max(1.0, GetDpiScale())
        : maximumLineLength != 0U &&
            textSize_.width > 0.0
        ? textSize_.width /
            static_cast<double>(
                maximumLineLength)
        : DefaultAdvance *
              GetFontSize() / 16.0 /
            std::max(1.0, GetDpiScale());

    Base::Result<void> initial =
        caretStops_.Resize(
            graphemes + 1U);
    if (!initial) {
        return initial;
    }
    std::uint32_t visualLineBase = 0U;
    for (std::uint32_t line = 0U;
         line < lines; ++line) {
        Base::Result<TextRange> range =
            active.LineRange(line);
        if (!range) {
            return range.GetStatus();
        }
        const std::uint32_t start =
            range.Value().start;
        const std::uint32_t end =
            range.Value().GetEnd();
        for (std::uint32_t index = start;
             index <= end; ++index) {
            const std::uint32_t offset =
                index - start;
            std::uint32_t visualLineOffset = 0U;
            std::uint32_t column = offset;
            if (wrapColumns_ != UINT32_MAX) {
                visualLineOffset =
                    offset / wrapColumns_;
                column = offset % wrapColumns_;
                if (offset == range.Value().length &&
                    offset != 0U &&
                    column == 0U) {
                    --visualLineOffset;
                    column = wrapColumns_;
                }
            }
            caretStops_[index] = {
                static_cast<double>(column) *
                    advance,
                static_cast<double>(
                    visualLineBase +
                    visualLineOffset) *
                    lineHeight,
                lineHeight,
                visualLineBase +
                    visualLineOffset};
        }
        const std::uint32_t wrappedLines =
            wrapColumns_ == UINT32_MAX
            ? 1U
            : std::max(
                  1U,
                  (range.Value().length +
                   wrapColumns_ - 1U) /
                      wrapColumns_);
        visualLineBase += wrappedLines;
        if (line + 1U < lines &&
            end < graphemes) {
            caretStops_[end + 1U] = {
                0.0,
                static_cast<double>(
                    visualLineBase) *
                    lineHeight,
                lineHeight,
                visualLineBase};
        }
    }
    return {};
}

Size TextBox::MeasureOverride(
    Size availableSize) noexcept {
    Base::Result<void> display =
        DisplayPolicy(displayPolicy_)->BuildDisplayText(
            Model(GetActiveModel()), displayText_);
    if (!display) {
        return Size{};
    }
    ReleaseGlyphRuns();
    textSize_ = {};
    wrapColumns_ = UINT32_MAX;
    showingPlaceholder_ =
        displayText_.Empty() &&
        !GetPlaceholder().Empty();
    if (showingPlaceholder_) {
        display = displayText_.Assign(
            GetPlaceholder());
        if (!display) {
            return Size{};
        }
    }
    const Thickness padding = GetPadding();
    const Size contentAvailable =
        Deflate(availableSize, padding);
    if (GetTextWrapping() !=
            TextWrapping::NoWrap &&
        contentAvailable.width > 0.0) {
        const double fallbackAdvance =
            DefaultAdvance *
            GetFontSize() / 16.0 /
            std::max(1.0, GetDpiScale());
        const double columns =
            std::floor(
                contentAvailable.width /
                fallbackAdvance);
        wrapColumns_ = static_cast<std::uint32_t>(
            std::min(
                static_cast<double>(UINT32_MAX),
                std::max(1.0, columns)));
    }
    auto* layoutService = LayoutService(*this);
    if (layoutService != nullptr &&
        !displayText_.Empty()) {
        TextLayoutRequest request;
        request.text = displayText_.View();
        request.availableSize =
            contentAvailable;
        request.dpiScale = GetDpiScale();
        request.pixelSize =
            static_cast<float>(GetFontSize());
        request.lineHeight =
            static_cast<float>(
                GetFontSize() * 1.6);
        const Base::Ref<Media::FontFamily> configuredFamily =
            GetFontFamily();
        Base::StringView family = configuredFamily
            ? configuredFamily->GetSource()
            : Base::StringView{};
        const bool defaultFamily =
            family.Empty() ||
            family == Base::StringView(
                "Segoe UI");
        if (defaultFamily) {
            const bool bold =
                GetFontWeight() ==
                    FontWeight::Bold ||
                GetFontWeight() ==
                    FontWeight::SemiBold;
            const bool italic =
                GetFontStyle() !=
                    FontStyle::Normal;
            if (bold && italic) {
                family = Base::StringView(
                    "Segoe UI Bold Italic");
            } else if (bold) {
                family = Base::StringView(
                    "Segoe UI Bold");
            } else if (italic) {
                family = Base::StringView(
                    "Segoe UI Italic");
            }
        }
        request.fontFamily = family;
        request.wrapping = GetTextWrapping();
        request.alignment = GetTextAlignment();
        request.direction = GetFlowDirection() == FlowDirection::RightToLeft
            ? Text::TextDirection::RightToLeft
            : Text::TextDirection::LeftToRight;
        TextLayoutResult result;
        Base::Result<void> prepared =
            layoutService->ShapeAndPrepare(
                request, result);
        if (!prepared) {
            return Size{};
        }
        for (RenderGlyphRunId glyph :
             result.glyphRuns) {
            if (glyph ==
                InvalidRenderGlyphRunId) {
                for (RenderGlyphRunId release :
                     result.glyphRuns) {
                    if (release !=
                        InvalidRenderGlyphRunId) {
                        layoutService->
                            ReleaseGlyphRun(release);
                    }
                }
                return Size{};
            }
        }
        glyphRuns_ =
            std::move(result.glyphRuns);
        serviceOwnsGlyphRuns_ =
            !glyphRuns_.Empty();
        textSize_ = result.desiredSize;
    } else {
        std::uint32_t maximumLine = 0U;
        std::uint32_t visualLineCount = 0U;
        const auto& active = Model(GetActiveModel());
        for (std::uint32_t line = 0U;
             line < active.LineCount();
             ++line) {
            Base::Result<TextRange> range =
                active.LineRange(line);
            if (!range) {
                return Size{};
            }
            maximumLine = std::max(
                maximumLine,
                range.Value().length);
            visualLineCount +=
                wrapColumns_ == UINT32_MAX
                ? 1U
                : std::max(
                      1U,
                      (range.Value().length +
                       wrapColumns_ - 1U) /
                          wrapColumns_);
        }
        const std::uint32_t visibleColumns =
            wrapColumns_ == UINT32_MAX
            ? maximumLine
            : std::min(
                  maximumLine,
                  wrapColumns_);
        textSize_ = {
            static_cast<double>(visibleColumns) *
                DefaultAdvance *
                GetFontSize() / 16.0 /
                std::max(1.0, GetDpiScale()),
            static_cast<double>(
                std::max(1U, visualLineCount)) *
                GetFontSize() * 1.6 /
                std::max(1.0, GetDpiScale())};
    }
    Base::Result<void> stops =
        RebuildCaretStops();
    if (!stops) {
        return Size{};
    }
    scroll_.extentWidth = textSize_.width;
    scroll_.extentHeight =
        std::max(textSize_.height, GetLineHeight());
    scroll_.horizontalOffset = ClampOffset(
        scroll_.horizontalOffset,
        scroll_.extentWidth,
        scroll_.viewportWidth);
    scroll_.verticalOffset = ClampOffset(
        scroll_.verticalOffset,
        scroll_.extentHeight,
        scroll_.viewportHeight);
    if (GetIsKeyboardFocused() ||
        compositionActive_) {
        Base::Result<void> visible =
            EnsureCaretVisible();
        if (!visible) {
            return Size{};
        }
    }
    static_cast<void>(
        UpdateCandidateWindow());
    const double minimumWidth =
        DefaultAdvance *
        GetFontSize() / 16.0 /
        std::max(1.0, GetDpiScale());
    Size desired{
        std::min(
            std::max(minimumWidth, textSize_.width),
            contentAvailable.width),
        std::min(
            std::max(GetLineHeight(), textSize_.height),
            contentAvailable.height)};
    const std::uint32_t maximumLines =
        GetMaxLines();
    const std::uint32_t minimumLines =
        GetMinLines();
    if (maximumLines != 0U) {
        const double lineBoxHeight =
            std::max(
                GetLineHeight(),
                GetFontSize() * 1.6);
        const double maximumHeight =
            lineBoxHeight *
            static_cast<double>(
                maximumLines);
        if (GetTextWrapping() !=
                TextWrapping::NoWrap &&
            !displayText_.Empty()) {
            desired.height = std::min(
                contentAvailable.height,
                maximumHeight);
        } else {
            desired.height = std::min(
                desired.height,
                maximumHeight);
        }
    }
    desired.height = std::max(
        desired.height,
        std::min(
            contentAvailable.height,
            std::max(
                GetLineHeight(),
                GetFontSize() * 1.6) *
                static_cast<double>(
                    minimumLines)));
    const Size textInflated = Inflate(desired, padding);
    Size templateSize{};
    if (GetTemplateRoot() != nullptr) {
        templateSize = Control::MeasureOverride(availableSize);
    }
    return Size{
        std::max(templateSize.width, textInflated.width),
        std::max(templateSize.height, textInflated.height)};
}

Size TextBox::ArrangeOverride(
    Size finalSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        Control::ArrangeOverride(finalSize);
    }
    const Size contentViewport =
        Deflate(finalSize, GetPadding());
    SetViewport(contentViewport);
    if (GetIsKeyboardFocused() ||
        compositionActive_) {
        Base::Result<void> visible =
            EnsureCaretVisible();
        if (!visible) {
            return finalSize;
        }
    }
    static_cast<void>(
        UpdateCandidateWindow());
    return finalSize;
}

void TextBox::OnApplyTemplate() noexcept {
    Control::OnApplyTemplate();
    DependencyObject* part = GetTemplateChild(Base::StringView("PART_ContentHost"));
    if (part != nullptr && PropertyRegistry().Types().IsDerivedFrom(
            part->RuntimeType(), ScrollViewer::StaticTypeId())) {
        static_cast<void>(AttachScrollViewer(static_cast<ScrollViewer*>(part)));
    } else {
        static_cast<void>(AttachScrollViewer(nullptr));
    }
}

void TextBox::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    if (GetTemplateRoot() != nullptr) return;
    auto& builder = Aero::Render::DrawingPrivate::Builder(context);
    const Rect bounds{
        0.0, 0.0,
        GetRenderSize().width,
        GetRenderSize().height};
    const Thickness border =
        GetBorderThickness();
    const double borderThickness = std::max(
        std::max(border.left, border.right),
        std::max(border.top, border.bottom));
    Color borderBrush =
        ::Aero::Media::SampleBrush(GetBorderBrush());
    if (GetIsKeyboardFocused() && GetIsEnabled()) {
        borderBrush = Color{
            11.0F / 255.0F,
            128.0F / 255.0F,
            193.0F / 255.0F,
            1.0F};
    } else if (GetIsMouseOver() && GetIsEnabled()) {
        borderBrush = Color{
            93.0F / 255.0F,
            100.0F / 255.0F,
            105.0F / 255.0F,
            1.0F};
    }
    Base::Result<void> chrome =
        builder.FillRoundedRect(
            bounds,
            ::Aero::Media::SampleBrush(GetBackground()),
            1.75);
    if (!chrome) {
        return;
    }
    if (borderThickness > 0.0 &&
        borderBrush.alpha > 0.0F) {
        chrome = builder.StrokeRect(
            bounds,
            borderBrush,
            GetIsKeyboardFocused()
                ? std::max(1.0, borderThickness)
                : borderThickness);
        if (!chrome) {
            return;
        }
    }
    static_cast<void>(RenderEditor(
        context,
        GetRenderSize(),
        GetIsKeyboardFocused()));
}

Base::Result<void>
TextBox::RenderEditor(
    ::Aero::Media::DrawingContext& context,
    Size viewport,
    bool drawCaret) noexcept {
    auto& builder = Aero::Render::DrawingPrivate::Builder(context);
    Thickness padding = GetPadding();
    if (scrollViewer_ != nullptr) {
        const Rect svSlot = scrollViewer_->GetLayoutSlot();
        padding.left += svSlot.x;
        padding.top += svSlot.y;
    }
    const Rect contentBounds{
        padding.left,
        padding.top,
        std::max(
            0.0,
            viewport.width -
                padding.left -
                padding.right),
        std::max(
            0.0,
            viewport.height -
                padding.top -
                padding.bottom)};
    Base::Result<void> clip =
        builder.PushClip(contentBounds);
    if (!clip) {
        return clip;
    }
    Base::Result<void> transform =
        builder.PushTransform(Transform2D{
            1.0, 0.0, 0.0, 1.0,
            padding.left -
                scroll_.horizontalOffset,
            padding.top -
                scroll_.verticalOffset});
    if (!transform) {
        return transform;
    }
    const TextSelection selection =
        Model(GetActiveModel()).Selection();
    if (!selection.GetIsEmpty() &&
        !caretStops_.Empty()) {
        const std::uint32_t begin =
            selection.GetStart();
        const std::uint32_t end =
            selection.GetEnd();
        std::uint32_t index = begin;
        while (index < end) {
            const CaretStop& first =
                caretStops_[index];
            std::uint32_t lineEnd =
                index + 1U;
            while (lineEnd < end &&
                lineEnd < caretStops_.Size() &&
                caretStops_[lineEnd].line ==
                    first.line) {
                ++lineEnd;
            }
            const CaretStop& last =
                caretStops_[
                    std::min(
                        lineEnd,
                        caretStops_.Size() - 1U)];
            const double width =
                last.line == first.line
                ? std::max(
                    0.0,
                    last.x - first.x)
                : std::max(
                    DefaultAdvance,
                    textSize_.width - first.x);
            Color selectionColor =
                ::Aero::Media::SampleBrush(
                    GetSelectionBrush(),
                    0.5,
                    Color{
                        46.0F / 255.0F,
                        174.0F / 255.0F,
                        235.0F / 255.0F,
                        1.0F});
            selectionColor.alpha *=
                static_cast<float>(
                    GetSelectionOpacity());
            Base::Result<void> filled =
                builder.FillRect(
                    {first.x, first.y,
                     width, first.height},
                    selectionColor);
            if (!filled) {
                return filled;
            }
            index = lineEnd;
        }
    }
    for (RenderGlyphRunId glyph :
         glyphRuns_) {
            Base::Result<void> drawn =
                builder.DrawGlyphRun(
                    glyph,
                    showingPlaceholder_
                    ? ::Aero::Media::SampleBrush(
                        GetPlaceholderForeground(),
                        0.5,
                        Color{
                            123.0F / 255.0F,
                            128.0F / 255.0F,
                            133.0F / 255.0F,
                            1.0F})
                    : ::Aero::Media::SampleBrush(
                        GetForeground(),
                        0.5,
                        Color{
                            0.0F, 0.0F, 0.0F, 1.0F}));
        if (!drawn) {
            return drawn;
        }
    }
    if (drawCaret &&
        !showingPlaceholder_) {
        Rect caret = GetCaretRectangle();
        caret.x += scroll_.horizontalOffset;
        caret.y += scroll_.verticalOffset;
        Base::Result<void> drawn =
            builder.FillRect(
                caret,
                ::Aero::Media::SampleBrush(
                    GetCaretBrush(),
                    0.5,
                    Color{
                        0.0F, 0.0F, 0.0F, 1.0F}));
        if (!drawn) {
            return drawn;
        }
    }
    Base::Result<void> popTransform =
        builder.PopTransform();
    if (!popTransform) {
        return popTransform;
    }
    return builder.PopClip();
}

void TextBox::ReleaseGlyphRuns() noexcept {
    auto* layoutService = LayoutService(*this);
    if (serviceOwnsGlyphRuns_ && layoutService != nullptr) {
        for (RenderGlyphRunId glyph :
             glyphRuns_) {
            layoutService->ReleaseGlyphRun(glyph);
        }
    }
    glyphRuns_.Clear();
    serviceOwnsGlyphRuns_ = false;
}

void TextBox::SetViewport(
    Size viewport) noexcept {
    if (!IsValidLayoutSize(viewport)) {
        return;
    }
    const ScrollData old = scroll_;
    scroll_.viewportWidth = viewport.width;
    scroll_.viewportHeight = viewport.height;
    scroll_.horizontalOffset = ClampOffset(
        scroll_.horizontalOffset,
        scroll_.extentWidth,
        scroll_.viewportWidth);
    scroll_.verticalOffset = ClampOffset(
        scroll_.verticalOffset,
        scroll_.extentHeight,
        scroll_.viewportHeight);
    const bool changed =
        old.viewportWidth !=
            scroll_.viewportWidth ||
        old.viewportHeight !=
            scroll_.viewportHeight ||
        old.horizontalOffset !=
            scroll_.horizontalOffset ||
        old.verticalOffset !=
            scroll_.verticalOffset;
    if (changed) {
        (void)InvalidateVisual();
    }
}

void TextBox::SetHorizontalOffset(
    double value) noexcept {
    if (!std::isfinite(value)) {
        return;
    }
    const double next = ClampOffset(
        value,
        scroll_.extentWidth,
        scroll_.viewportWidth);
    if (next == scroll_.horizontalOffset) {
        return;
    }
    scroll_.horizontalOffset = next;
    (void)InvalidateVisual();
}

void TextBox::SetVerticalOffset(
    double value) noexcept {
    if (!std::isfinite(value)) {
        return;
    }
    const double next = ClampOffset(
        value,
        scroll_.extentHeight,
        scroll_.viewportHeight);
    if (next == scroll_.verticalOffset) {
        return;
    }
    scroll_.verticalOffset = next;
    (void)InvalidateVisual();
}

Base::Result<bool> TextBox::LineHorizontal(
    double direction) noexcept {
    const double old = scroll_.horizontalOffset;
    SetHorizontalOffset(old + direction * ScrollLine);
    return old != scroll_.horizontalOffset;
}

Base::Result<bool> TextBox::LineVertical(
    double direction) noexcept {
    const double old = scroll_.verticalOffset;
    SetVerticalOffset(old + direction * GetLineHeight());
    return old != scroll_.verticalOffset;
}

Base::Result<bool> TextBox::PageHorizontal(
    double direction) noexcept {
    const double old = scroll_.horizontalOffset;
    SetHorizontalOffset(old + direction * scroll_.viewportWidth);
    return old != scroll_.horizontalOffset;
}

Base::Result<bool> TextBox::PageVertical(
    double direction) noexcept {
    const double old = scroll_.verticalOffset;
    SetVerticalOffset(old + direction * scroll_.viewportHeight);
    return old != scroll_.verticalOffset;
}

Base::Result<void>
TextBox::EnsureCaretVisible() noexcept {
    const Rect caret = GetCaretRectangle();
    double horizontal =
        scroll_.horizontalOffset;
    double vertical =
        scroll_.verticalOffset;
    const double contentX =
        caret.x + scroll_.horizontalOffset;
    const double contentY =
        caret.y + scroll_.verticalOffset;
    if (contentX < horizontal) {
        horizontal = contentX;
    } else if (contentX + caret.width >
        horizontal + scroll_.viewportWidth) {
        horizontal = contentX + caret.width -
            scroll_.viewportWidth;
    }
    if (contentY < vertical) {
        vertical = contentY;
    } else if (contentY + caret.height >
        vertical + scroll_.viewportHeight) {
        vertical = contentY + caret.height -
            scroll_.viewportHeight;
    }
    SetHorizontalOffset(horizontal);
    SetVerticalOffset(vertical);
    return {};
}

Base::Result<void>
TextBox::UpdateCandidateWindow() noexcept {
    if (inputMethodHost_ == nullptr ||
        !compositionActive_) {
        return {};
    }
    Input::ImeCandidateWindow candidate;
    Rect caret = GetCaretRectangle();
    caret.x += GetPadding().left;
    caret.y += GetPadding().top;
    UIElement& owner =
        coordinateOwner_ != nullptr
        ? *coordinateOwner_
        : static_cast<UIElement&>(*this);
    candidate.caret =
        ToRootRect(owner, caret);
    candidate.dpiScale = GetDpiScale();
    inputMethodHost_->SetCandidateWindow(candidate);
    return {};
}

