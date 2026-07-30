#include "ScoreboardModel.hpp"

#include <Aero/App.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Scroll.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Presentation/Transforms.hpp>
#include <Aero/RuntimeEnvironment.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

namespace {

void LogBrush(
    const char* label,
    const Aero::Base::Ref<
        Aero::Presentation::Brush>& brush) noexcept {
    if (!brush) {
        std::fprintf(stderr, " %s=null", label);
        return;
    }
    if (brush->RuntimeType() ==
        Aero::Presentation::SolidColorBrush::
            StaticTypeId()) {
        const Aero::Presentation::Color color =
            static_cast<
                Aero::Presentation::SolidColorBrush*>(
                    brush.Get())->GetColor();
        std::fprintf(
            stderr,
            " %s=solid(%.3f,%.3f,%.3f,%.3f;opacity=%.3f)",
            label,
            color.red,
            color.green,
            color.blue,
            color.alpha,
            brush->Opacity());
        return;
    }
    if (brush->RuntimeType() ==
            Aero::Presentation::LinearGradientBrush::
                StaticTypeId() ||
        brush->RuntimeType() ==
            Aero::Presentation::RadialGradientBrush::
                StaticTypeId()) {
        const auto& gradient =
            *static_cast<Aero::Presentation::GradientBrush*>(
                brush.Get());
        std::fprintf(
            stderr,
            " %s=%s(stops=%u;opacity=%.3f",
            label,
            brush->RuntimeType() ==
                    Aero::Presentation::
                        LinearGradientBrush::
                            StaticTypeId()
                ? "linear"
                : "radial",
            gradient.GradientStops().Size(),
            brush->Opacity());
        for (const auto& stop :
             gradient.GradientStops()) {
            if (!stop) continue;
            const Aero::Presentation::Color color =
                stop->GetColor();
            std::fprintf(
                stderr,
                ";%.3f=%.3f,%.3f,%.3f,%.3f",
                stop->Offset(),
                color.red,
                color.green,
                color.blue,
                color.alpha);
        }
        std::fputc(')', stderr);
        return;
    }
    std::fprintf(
        stderr,
        " %s=brush(type=%llu;opacity=%.3f)",
        label,
        static_cast<unsigned long long>(
            brush->RuntimeType()),
        brush->Opacity());
}

template<class T>
T* FindVisualOfType(
    Aero::Presentation::Visual& visual) noexcept {
    Aero::Presentation::UIElement* element =
        visual.AsUIElement();
    if (element != nullptr &&
        element->PropertyRegistry().Types().
            IsDerivedFrom(
                element->RuntimeType(),
                T::StaticTypeId())) {
        return static_cast<T*>(element);
    }
    for (Aero::Presentation::Visual* child :
         visual.VisualChildren()) {
        if (child == nullptr) continue;
        if (T* found =
                FindVisualOfType<T>(*child)) {
            return found;
        }
    }
    return nullptr;
}

template<class T>
T* FindExactVisualType(
    Aero::Presentation::Visual& visual) noexcept {
    Aero::Presentation::UIElement* element =
        visual.AsUIElement();
    if (element != nullptr &&
        element->RuntimeType() ==
            T::StaticTypeId()) {
        return static_cast<T*>(element);
    }
    for (Aero::Presentation::Visual* child :
         visual.VisualChildren()) {
        if (child == nullptr) continue;
        if (T* found =
                FindExactVisualType<T>(*child)) {
            return found;
        }
    }
    return nullptr;
}

void LogVisual(
    const char* group,
    const Aero::Presentation::Visual& visual,
    std::uint32_t depth,
    std::uint32_t maxDepth) noexcept {
    const Aero::Presentation::UIElement* element =
        visual.AsUIElement();
    if (element == nullptr) return;
    const auto* type =
        element->PropertyRegistry().Types().FindType(
            element->RuntimeType());
    const Aero::Base::StringView typeName =
        type != nullptr
        ? type->Name()
        : Aero::Base::StringView("?");
    const Aero::Presentation::Rect slot =
        element->LayoutSlot();
    const Aero::Presentation::Size desired =
        element->DesiredSize();
    const Aero::Presentation::Size rendered =
        element->RenderSize();
    std::fprintf(
        stderr,
        "Scoreboard diagnostic visual group=%s depth=%u type=%.*s slot=%.1f,%.1f,%.1fx%.1f desired=%.1fx%.1f render=%.1fx%.1f",
        group,
        depth,
        static_cast<int>(typeName.SizeBytes()),
        typeName.Data(),
        slot.x,
        slot.y,
        slot.width,
        slot.height,
        desired.width,
        desired.height,
        rendered.width,
        rendered.height);
    if (element->RuntimeType() ==
        Aero::Controls::TextBlock::StaticTypeId()) {
        const auto& text =
            static_cast<const Aero::Controls::TextBlock&>(
                *element);
        const Aero::Base::StringView value = text.Text();
        const Aero::Base::StringView family =
            text.FontFamily();
        std::fprintf(
            stderr,
            " text=%.*s font=%.*s size=%.1f",
            static_cast<int>(value.SizeBytes()),
            value.Data(),
            static_cast<int>(family.SizeBytes()),
            family.Data(),
            text.FontSize());
        LogBrush("foreground", text.ForegroundBrush());
    }
    if (element->PropertyRegistry().Types().
            IsDerivedFrom(
                element->RuntimeType(),
                Aero::Controls::Control::
                    StaticTypeId())) {
        const auto& control =
            static_cast<const Aero::Controls::Control&>(
                *element);
        std::fprintf(
            stderr,
            " control-font=%.1f",
            control.FontSize());
        LogBrush(
            "control-foreground",
            control.ForegroundBrush());
    }
    if (element->RuntimeType() ==
        Aero::Controls::Border::StaticTypeId()) {
        const auto& border =
            static_cast<const Aero::Controls::Border&>(
                *element);
        LogBrush(
            "background", border.BackgroundBrush());
        LogBrush(
            "border", border.BorderBrushObject());
    }
    if (element->RuntimeType() ==
        Aero::Controls::Rectangle::StaticTypeId()) {
        const auto& rectangle =
            static_cast<
                const Aero::Controls::Rectangle&>(
                    *element);
        LogBrush("fill", rectangle.FillBrush());
    }
    std::fputc('\n', stderr);
    if (depth >= maxDepth) return;
    for (Aero::Presentation::Visual* child :
         visual.VisualChildren()) {
        if (child != nullptr) {
            LogVisual(
                group, *child, depth + 1U, maxDepth);
        }
    }
}

void LogPointerParents(
    Aero::Presentation::UIElement* element) noexcept {
    std::fprintf(stderr, "Scoreboard diagnostic pointer-route");
    Aero::Presentation::Visual* current = element;
    for (std::uint32_t depth = 0U;
         current != nullptr && depth < 16U;
         ++depth) {
        Aero::Presentation::UIElement* currentElement =
            current->AsUIElement();
        const auto* type =
            currentElement != nullptr
            ? currentElement->PropertyRegistry().Types().
                  FindType(currentElement->RuntimeType())
            : nullptr;
        const Aero::Base::StringView name =
            type != nullptr
            ? type->Name()
            : Aero::Base::StringView("?");
        std::fprintf(
            stderr,
            " %.*s(over=%u,enabled=%u)",
            static_cast<int>(name.SizeBytes()),
            name.Data(),
            currentElement != nullptr &&
                    currentElement->IsMouseOver()
                ? 1U
                : 0U,
            currentElement != nullptr &&
                    currentElement->IsEnabled()
                ? 1U
                : 0U);
        current = current->VisualParent();
    }
    std::fputc('\n', stderr);
}

Aero::Base::Result<void> InitializeScoreboard(
    Aero::App::Application&,
    Aero::App::Window& window,
    void*) noexcept {
    Aero::Base::Result<
        Aero::Base::Ref<
            Aero::Samples::Scoreboard::Game>>
        created =
            Aero::Samples::Scoreboard::
                CreateScoreboardGame();
    if (!created) return created.GetStatus();
    return window.SetDataContext(
        Aero::Base::Ref<Aero::Base::Object>(
            std::move(created).Value()));
}

struct ScoreboardDiagnostics final {
    std::uint32_t initialWaitFrames = 0U;
    bool initialStateReported = false;
    bool wheelDispatched = false;
    bool hoverValidated = false;
    std::uint32_t hoverWaitFrames = 0U;
    Aero::Presentation::FrameworkElement*
        hoveredRow = nullptr;
    bool comboClickDispatched = false;
    bool popupHoverDispatched = false;
    std::uint32_t popupHoverWaitFrames = 0U;
    bool comboOpenValidated = false;
    bool selectionChanged = false;
    bool sawDisabled = false;
    std::uint32_t filterPhase = 0U;
};

Aero::Presentation::Point RootPoint(
    Aero::Presentation::UIElement& element,
    Aero::Presentation::Point point) noexcept {
    Aero::Presentation::Visual* current = &element;
    while (current != nullptr) {
        Aero::Presentation::UIElement* currentElement =
            current->AsUIElement();
        if (currentElement != nullptr) {
            Aero::Presentation::FrameworkElement* framework =
                currentElement->AsFrameworkElement();
            if (framework != nullptr) {
                point = Aero::Presentation::TransformPoint(
                    framework->LocalVisualTransform(),
                    point);
            }
            const Aero::Presentation::Rect slot =
                currentElement->LayoutSlot();
            point.x += slot.x;
            point.y += slot.y;
        }
        current = current->VisualParent();
    }
    return point;
}

bool ScoreboardDiagnosticsEnabled() noexcept {
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t size = 0U;
    if (_dupenv_s(
            &value,
            &size,
            "AERO_SCOREBOARD_DIAGNOSTICS") != 0) {
        return false;
    }
    const bool enabled = value != nullptr;
    std::free(value);
    return enabled;
#else
    return std::getenv(
        "AERO_SCOREBOARD_DIAGNOSTICS") != nullptr;
#endif
}

Aero::Base::Result<void> DiagnoseScoreboardFrame(
    Aero::App::Application& application,
    Aero::App::Window& window,
    std::uint64_t,
    void* context) noexcept {
    auto* diagnostics =
        static_cast<ScoreboardDiagnostics*>(context);
    Aero::View* view = window.HostedView();
    if (diagnostics == nullptr || view == nullptr) {
        return Aero::Base::Status::Failure(
            Aero::Base::ErrorCode::InvalidState,
            "Scoreboard diagnostics require a hosted View");
    }
    auto* selector =
        view->FindNamed<Aero::Controls::ComboBox>(
            "VisibleTeam");
    auto* scroll =
        view->FindNamed<Aero::Controls::ScrollViewer>(
            "PlayersScroll");
    auto* players =
        view->FindNamed<Aero::Controls::ItemsControl>(
            "Players");
    auto* layoutRoot =
        view->FindNamed<Aero::Presentation::FrameworkElement>(
            "LayoutRoot");
    if (selector == nullptr || scroll == nullptr ||
        players == nullptr || layoutRoot == nullptr) {
        return Aero::Base::Status::Failure(
            Aero::Base::ErrorCode::NotFound,
            "Scoreboard diagnostic targets were not found");
    }
    if (!diagnostics->initialStateReported) {
        Aero::Base::Result<
            Aero::Base::Ref<Aero::Base::Object>>
            dataContext = window.GetDataContext();
        if (!dataContext ||
            !dataContext.Value() ||
            dataContext.Value()->RuntimeType() !=
                Aero::Samples::Scoreboard::Game::
                    StaticTypeId()) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::ValidationFailed,
                "Scoreboard diagnostic data context is invalid");
        }
        const auto& game = static_cast<
            const Aero::Samples::Scoreboard::Game&>(
                *dataContext.Value());
        if ((players->ItemCount() != 10U ||
             players->RealizedItemCount() != 10U) &&
            diagnostics->initialWaitFrames < 16U) {
            ++diagnostics->initialWaitFrames;
            std::fprintf(
                stderr,
                "Scoreboard diagnostic waiting frame=%u items=%u realized=%u\n",
                diagnostics->initialWaitFrames,
                players->ItemCount(),
                players->RealizedItemCount());
            return {};
        }
        if (players->ItemCount() != 10U ||
            players->RealizedItemCount() != 10U ||
            game.AllianceScore() != 1522 ||
            game.HordeScore() != 1491 ||
            game.ElapsedTime() != 16) {
            std::fprintf(
                stderr,
                "Scoreboard diagnostic mismatch items=%u realized=%u alliance=%d horde=%d elapsed=%d\n",
                players->ItemCount(),
                players->RealizedItemCount(),
                game.AllianceScore(),
                game.HordeScore(),
                game.ElapsedTime());
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::ValidationFailed,
                "Scoreboard model or generated rows differ from the reference");
        }
        const Aero::Presentation::Size rootSize =
            layoutRoot->RenderSize();
        const Aero::Presentation::Size scrollSize =
            scroll->RenderSize();
        std::fprintf(
            stderr,
            "Scoreboard diagnostic model players=%u realized=%u alliance=%d horde=%d elapsed=%d\n",
            players->ItemCount(),
            players->RealizedItemCount(),
            game.AllianceScore(),
            game.HordeScore(),
            game.ElapsedTime());
        std::fprintf(
            stderr,
            "Scoreboard diagnostic layout root=%.0fx%.0f scroll=%.0fx%.0f window-font=%.*s\n",
            rootSize.width,
            rootSize.height,
            scrollSize.width,
            scrollSize.height,
            static_cast<int>(
                window.FontFamily().SizeBytes()),
            window.FontFamily().Data());
        if (Aero::Controls::Panel* itemsHost =
                players->ItemsHost()) {
            std::uint32_t index = 0U;
            double previousBottom = 0.0;
            for (Aero::Presentation::Visual* visual :
                 itemsHost->VisualChildren()) {
                Aero::Presentation::UIElement* child =
                    visual != nullptr
                    ? visual->AsUIElement()
                    : nullptr;
                if (child == nullptr) continue;
                std::fprintf(
                    stderr,
                    "Scoreboard diagnostic item index=%u\n",
                    index);
                LogVisual(
                    "item",
                    *child,
                    0U,
                    index < 2U ? 5U : 0U);
                const Aero::Presentation::Rect itemSlot =
                    child->LayoutSlot();
                if (itemSlot.height < 25.0 ||
                    itemSlot.height > 50.0 ||
                    (index != 0U &&
                     (itemSlot.y < previousBottom - 0.5 ||
                      itemSlot.y > previousBottom + 0.5))) {
                    return Aero::Base::Status::Failure(
                        Aero::Base::ErrorCode::
                            ValidationFailed,
                        "Scoreboard item rows are not compact and contiguous");
                }
                previousBottom =
                    itemSlot.y + itemSlot.height;
                if (index == 0U) {
                    std::uint32_t textCount = 0U;
                    double previousTextX = -1.0;
                    for (Aero::Presentation::Visual*
                             itemVisual :
                         visual->VisualChildren()) {
                        if (itemVisual == nullptr) continue;
                        for (Aero::Presentation::Visual*
                                 part :
                             itemVisual->VisualChildren()) {
                            if (part == nullptr ||
                                part->AsUIElement() ==
                                    nullptr) {
                                continue;
                            }
                            auto* partElement =
                                part->AsUIElement();
                            if (partElement->RuntimeType() ==
                                    Aero::Controls::Path::
                                        StaticTypeId() &&
                                (partElement->RenderSize().
                                         width > 30.0 ||
                                 partElement->RenderSize().
                                         height > 30.0)) {
                                return Aero::Base::Status::
                                    Failure(
                                        Aero::Base::
                                            ErrorCode::
                                                ValidationFailed,
                                        "Scoreboard player icon did not preserve its aspect ratio");
                            }
                            if (partElement->RuntimeType() !=
                                    Aero::Controls::
                                        TextBlock::
                                            StaticTypeId()) {
                                continue;
                            }
                            const double x =
                                partElement->LayoutSlot().x;
                            if (x <= previousTextX) {
                                return Aero::Base::Status::
                                    Failure(
                                        Aero::Base::
                                            ErrorCode::
                                                ValidationFailed,
                                        "Scoreboard player fields overlap instead of using Grid columns");
                            }
                            previousTextX = x;
                            ++textCount;
                            const auto& text =
                                static_cast<
                                    const Aero::Controls::
                                        TextBlock&>(
                                            *partElement);
                            if (textCount == 5U &&
                                text.Text() !=
                                    Aero::Base::StringView(
                                        "8,134.12 K")) {
                                return Aero::Base::Status::
                                    Failure(
                                        Aero::Base::
                                            ErrorCode::
                                                ValidationFailed,
                                        "Scoreboard Damage StringFormat differs from the reference");
                            }
                            if (textCount == 6U &&
                                text.Text() !=
                                    Aero::Base::StringView(
                                        "1.83 K")) {
                                return Aero::Base::Status::
                                    Failure(
                                        Aero::Base::
                                            ErrorCode::
                                                ValidationFailed,
                                        "Scoreboard Heal StringFormat differs from the reference");
                            }
                        }
                    }
                    if (textCount != 6U) {
                        return Aero::Base::Status::Failure(
                            Aero::Base::ErrorCode::
                                ValidationFailed,
                            "Scoreboard player row does not contain all six fields");
                    }
                }
                ++index;
            }
            if (index != 10U) {
                return Aero::Base::Status::Failure(
                    Aero::Base::ErrorCode::
                        ValidationFailed,
                    "Scoreboard item host does not contain ten rows");
            }
        }
        if (auto* emblem =
                view->FindNamed<Aero::Controls::Path>(
                    "path")) {
            if (Aero::Presentation::UIElement* parent =
                    emblem->LayoutParent()) {
                LogVisual(
                    "score-header", *parent, 0U, 1U);
            }
        }
        LogVisual(
            "team-selector", *selector, 0U, 6U);
        LogBrush(
            "selector-foreground",
            selector->ForegroundBrush());
        std::fputc('\n', stderr);
        auto* verticalScrollBar =
            FindVisualOfType<Aero::Controls::ScrollBar>(
                *scroll);
        if (verticalScrollBar == nullptr) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::NotFound,
                "Scoreboard vertical ScrollBar template part was not found");
        }
        std::fprintf(
            stderr,
            "Scoreboard diagnostic scrollbar orientation=%u minimum=%.2f maximum=%.2f value=%.2f viewport=%.2f visible=%u outer-extent=%.1fx%.1f outer-viewport=%.1fx%.1f outer-scrollable=%.1fx%.1f\n",
            static_cast<unsigned>(
                verticalScrollBar->GetOrientation()),
            verticalScrollBar->Minimum(),
            verticalScrollBar->Maximum(),
            verticalScrollBar->Value(),
            verticalScrollBar->ViewportSize(),
            verticalScrollBar->GetVisibility() ==
                    Aero::Presentation::Visibility::Visible
                ? 1U
                : 0U,
            scroll->ExtentWidth(),
            scroll->ExtentHeight(),
            scroll->ViewportWidth(),
            scroll->ViewportHeight(),
            scroll->ScrollableWidth(),
            scroll->ScrollableHeight());
        auto* contentPresenter = FindExactVisualType<
            Aero::Controls::ScrollContentPresenter>(
                *scroll);
        if (contentPresenter != nullptr) {
            const Aero::Controls::ScrollData data =
                contentPresenter->Data();
            std::fprintf(
                stderr,
                "Scoreboard diagnostic scroll-presenter extent=%.1fx%.1f viewport=%.1fx%.1f offset=%.1f,%.1f templated-parent=%llu\n",
                data.extentWidth,
                data.extentHeight,
                data.viewportWidth,
                data.viewportHeight,
                data.horizontalOffset,
                data.verticalOffset,
                static_cast<unsigned long long>(
                    contentPresenter->TemplatedParent() !=
                            nullptr
                        ? contentPresenter->
                            TemplatedParent()->
                                RuntimeType()
                        : Aero::Core::InvalidTypeId));
        }
        LogVisual(
            "vertical-scrollbar",
            *verticalScrollBar,
            0U,
            8U);
        auto* thumb =
            FindVisualOfType<Aero::Controls::Thumb>(
                *verticalScrollBar);
        if (verticalScrollBar->GetVisibility() !=
                Aero::Presentation::Visibility::Visible ||
            verticalScrollBar->Maximum() < 1.0 ||
            verticalScrollBar->ViewportSize() < 1.0 ||
            verticalScrollBar->RenderSize().width < 14.0 ||
            thumb == nullptr ||
            thumb->RenderSize().height < 20.0 ||
            thumb->LayoutSlot().y > 1.0) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::
                    ValidationFailed,
                "Scoreboard vertical ScrollBar range, track, or thumb differs from the reference");
        }
        const double bottom =
            scroll->ScrollableHeight();
        Aero::Base::Result<bool> moved =
            scroll->SetVerticalOffset(bottom);
        if (!moved ||
            !moved.Value() ||
            std::fabs(
                scroll->VerticalOffset() - bottom) >
                0.01) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::
                    ValidationFailed,
                "Scoreboard ScrollViewer did not propagate its vertical offset to the template presenter");
        }
        Aero::Base::Result<bool> restored =
            scroll->SetVerticalOffset(0.0);
        if (!restored ||
            !restored.Value() ||
            std::fabs(scroll->VerticalOffset()) >
                0.01) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::
                    ValidationFailed,
                "Scoreboard ScrollViewer did not restore its vertical offset");
        }
        diagnostics->initialStateReported = true;
        return {};
    }
    if (!diagnostics->wheelDispatched) {
        auto* firstItem =
            FindVisualOfType<Aero::Controls::ItemContainer>(
                *players);
        if (!scroll->IsEnabled() ||
            firstItem == nullptr ||
            !firstItem->IsEnabled()) {
            return {};
        }
        Aero::Presentation::PointerInput move;
        move.pointerId = 1U;
        move.action =
            Aero::Presentation::PointerAction::Move;
        move.position = RootPoint(
            *scroll,
            {scroll->RenderSize().width * 0.5,
             scroll->RenderSize().height * 0.5});
        Aero::Base::Result<
            Aero::Presentation::PointerDispatchResult>
            hovered = view->DispatchPointer(move);
        if (!hovered) return hovered.GetStatus();
        LogPointerParents(hovered.Value().hit.target);
        Aero::Presentation::PointerInput wheel = move;
        wheel.action =
            Aero::Presentation::PointerAction::Wheel;
        wheel.wheelDeltaY = -1.0;
        Aero::Base::Result<
            Aero::Presentation::PointerDispatchResult>
            scrolled = view->DispatchPointer(wheel);
        if (!scrolled) return scrolled.GetStatus();
        LogPointerParents(scrolled.Value().hit.target);
        Aero::Presentation::Visual* hoveredVisual =
            scrolled.Value().hit.target;
        while (hoveredVisual != nullptr &&
               hoveredVisual->VisualParent() != nullptr) {
            Aero::Presentation::Visual* parent =
                hoveredVisual->VisualParent();
            Aero::Presentation::UIElement*
                parentElement =
                    parent->AsUIElement();
            if (parentElement != nullptr &&
                parentElement->PropertyRegistry().
                    Types().IsDerivedFrom(
                        parentElement->RuntimeType(),
                        Aero::Controls::
                            ItemContainer::
                                StaticTypeId())) {
                diagnostics->hoveredRow =
                    hoveredVisual->
                        AsFrameworkElement();
                break;
            }
            hoveredVisual = parent;
        }
        Aero::Base::Result<std::uint32_t> advanced =
            view->AdvanceAnimationTime(250U);
        if (!advanced) return advanced.GetStatus();
        diagnostics->wheelDispatched = true;
        std::fprintf(
            stderr,
            "Scoreboard diagnostic wheel hit=%llu offset=%.2f\n",
            static_cast<unsigned long long>(
                scrolled.Value().hit.target != nullptr
                    ? scrolled.Value().hit.target->
                          RuntimeType()
                    : Aero::Core::InvalidTypeId),
            scroll->VerticalOffset());
        return {};
    }
    if (!diagnostics->hoverValidated &&
        scroll->VerticalOffset() <= 0.0) {
        return Aero::Base::Status::Failure(
            Aero::Base::ErrorCode::ValidationFailed,
            "Scoreboard mouse wheel did not scroll the player list");
    }
    if (!diagnostics->hoverValidated) {
        const double translation =
            diagnostics->hoveredRow != nullptr
            ? diagnostics->hoveredRow->
                  LocalVisualTransform().dx
            : 0.0;
        std::fprintf(
            stderr,
            "Scoreboard diagnostic hover row=%u transform-x=%.2f triggers=%u\n",
            diagnostics->hoveredRow != nullptr &&
                    diagnostics->hoveredRow->
                        IsMouseOver()
                ? 1U : 0U,
            translation,
            diagnostics->hoveredRow != nullptr
                ? diagnostics->hoveredRow->
                      AuthoredTriggers().Size()
                : 0U);
        if (diagnostics->hoveredRow != nullptr &&
            diagnostics->hoveredRow->IsMouseOver() &&
            translation <= 0.0 &&
            diagnostics->hoverWaitFrames++ < 8U) {
            return {};
        }
        if (diagnostics->hoveredRow == nullptr ||
            !diagnostics->hoveredRow->IsMouseOver() ||
            translation <= 0.0) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::ValidationFailed,
                "Scoreboard DataTemplate IsMouseOver Trigger did not start its row animation");
        }
        diagnostics->hoverValidated = true;
        return {};
    }
    if (!diagnostics->comboClickDispatched) {
        Aero::Presentation::PointerInput pointer;
        pointer.pointerId = 1U;
        pointer.position = RootPoint(
            *selector,
            {selector->RenderSize().width * 0.5,
             selector->RenderSize().height * 0.5});
        pointer.action =
            Aero::Presentation::PointerAction::Down;
        Aero::Base::Result<
            Aero::Presentation::PointerDispatchResult>
            down = view->DispatchPointer(pointer);
        if (!down) return down.GetStatus();
        LogPointerParents(down.Value().hit.target);
        pointer.action =
            Aero::Presentation::PointerAction::Up;
        Aero::Base::Result<
            Aero::Presentation::PointerDispatchResult>
            up = view->DispatchPointer(pointer);
        if (!up) return up.GetStatus();
        diagnostics->comboClickDispatched = true;
        std::fprintf(
            stderr,
            "Scoreboard diagnostic combo-click hit=%llu open=%u\n",
            static_cast<unsigned long long>(
                down.Value().hit.target != nullptr
                    ? down.Value().hit.target->
                          RuntimeType()
                    : Aero::Core::InvalidTypeId),
            selector->IsDropDownOpen() ? 1U : 0U);
        return {};
    }
    if (!diagnostics->comboOpenValidated) {
        if (!selector->IsDropDownOpen()) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::ValidationFailed,
                "Scoreboard ComboBox did not open from pointer input");
        }
        auto* popup =
            FindVisualOfType<Aero::Controls::Popup>(
                *selector);
        Aero::Presentation::UIElement* popupChild =
            popup != nullptr &&
                    !popup->VisualChildren().Empty()
            ? popup->VisualChildren()[0U]->
                  AsUIElement()
            : nullptr;
        const Aero::Presentation::Point selectorOrigin =
            RootPoint(*selector, {});
        const Aero::Presentation::Point expectedOrigin =
            RootPoint(
                *selector,
                {popup != nullptr
                     ? popup->HorizontalOffset()
                     : 0.0,
                 selector->RenderSize().height +
                     (popup != nullptr
                          ? popup->VerticalOffset()
                          : 0.0)});
        const Aero::Presentation::Point popupOrigin =
            popupChild != nullptr
            ? RootPoint(*popupChild, {})
            : Aero::Presentation::Point{};
        std::fprintf(
            stderr,
            "Scoreboard diagnostic popup open=%u child=%.1fx%.1f selector=%.1f,%.1f expected=%.1f,%.1f actual=%.1f,%.1f delta=%.1f,%.1f\n",
            popup != nullptr && popup->IsOpen()
                ? 1U : 0U,
            popupChild != nullptr
                ? popupChild->RenderSize().width
                : 0.0,
            popupChild != nullptr
                ? popupChild->RenderSize().height
                : 0.0,
            selectorOrigin.x,
            selectorOrigin.y,
            expectedOrigin.x,
            expectedOrigin.y,
            popupOrigin.x,
            popupOrigin.y,
            popupOrigin.x - expectedOrigin.x,
            popupOrigin.y - expectedOrigin.y);
        if (popupChild != nullptr) {
            LogVisual("popup", *popupChild, 0U, 6U);
        }
        if (popup == nullptr ||
            !popup->IsOpen() ||
            popupChild == nullptr ||
            popupChild->RenderSize().width <= 0.0 ||
            popupChild->RenderSize().height <= 0.0 ||
            std::abs(popupOrigin.x - expectedOrigin.x) >
                0.5 ||
            std::abs(popupOrigin.y - expectedOrigin.y) >
                0.5) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::ValidationFailed,
                "Scoreboard ComboBox Popup layout or placement is incorrect");
        }
        Aero::Controls::Panel* popupItemsHost =
            selector->ItemsHost();
        Aero::Controls::TextBlock* popupText =
            popupItemsHost != nullptr &&
                    !popupItemsHost->VisualChildren().Empty() &&
                    popupItemsHost->VisualChildren()[0U] !=
                        nullptr
            ? FindVisualOfType<Aero::Controls::TextBlock>(
                  *popupItemsHost->VisualChildren()[0U])
            : nullptr;
        const Aero::Presentation::Color popupTextColor =
            popupText != nullptr
            ? Aero::Presentation::SampleBrush(
                  popupText->ForegroundBrush())
            : Aero::Presentation::Color{};
        if (popupText == nullptr ||
            std::fabs(popupTextColor.red - 1.0F) >
                0.01F ||
            std::fabs(
                popupTextColor.green -
                (220.0F / 255.0F)) > 0.01F ||
            std::fabs(
                popupTextColor.blue -
                (100.0F / 255.0F)) > 0.01F) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::ValidationFailed,
                "Scoreboard ComboBox generated text did not inherit Foreground");
        }
        if (!diagnostics->popupHoverDispatched) {
            if (popupItemsHost == nullptr ||
                popupItemsHost->VisualChildren().Size() <
                    2U ||
                popupItemsHost->VisualChildren()[1U] ==
                    nullptr) {
                return Aero::Base::Status::Failure(
                    Aero::Base::ErrorCode::NotFound,
                    "Scoreboard ComboBox popup item was not generated");
            }
            auto* popupItem = static_cast<
                Aero::Controls::ComboBoxItem*>(
                    popupItemsHost->VisualChildren()[1U]->
                        AsUIElement());
            Aero::Presentation::PointerInput move;
            move.pointerId = 1U;
            move.action =
                Aero::Presentation::PointerAction::Move;
            // Popup children are arranged in the overlay's root coordinate
            // space. Walking back through PlacementTarget would apply the
            // Viewbox transform a second time, so compose from the validated
            // popup origin and the item-panel slot instead.
            const Aero::Presentation::Rect popupItemSlot =
                popupItem->LayoutSlot();
            move.position = {
                popupOrigin.x + 1.0 +
                    popupItemSlot.x +
                    popupItem->RenderSize().width * 0.5,
                popupOrigin.y + 1.0 +
                    popupItemSlot.y +
                    popupItem->RenderSize().height * 0.5};
            Aero::Base::Result<
                Aero::Presentation::PointerDispatchResult>
                moved = view->DispatchPointer(move);
            if (!moved) return moved.GetStatus();
            std::fprintf(
                stderr,
                "Scoreboard diagnostic popup-hover-dispatch point=%.1f,%.1f hit=%llu\n",
                move.position.x,
                move.position.y,
                static_cast<unsigned long long>(
                    moved.Value().hit.target != nullptr
                    ? moved.Value().hit.target->
                          RuntimeType()
                    : Aero::Core::InvalidTypeId));
            LogPointerParents(moved.Value().hit.target);
            diagnostics->popupHoverDispatched = true;
            return {};
        }
        Aero::Controls::ComboBoxItem*
            activePopupItem = nullptr;
        if (popupItemsHost != nullptr) {
            for (Aero::Presentation::Visual* child :
                 popupItemsHost->VisualChildren()) {
                Aero::Presentation::UIElement* element =
                    child != nullptr
                    ? child->AsUIElement()
                    : nullptr;
                if (element != nullptr &&
                    element->PropertyRegistry().Types().
                        IsDerivedFrom(
                            element->RuntimeType(),
                            Aero::Controls::ComboBoxItem::
                                StaticTypeId()) &&
                    element->IsMouseOver()) {
                    activePopupItem = static_cast<
                        Aero::Controls::ComboBoxItem*>(
                            element);
                    break;
                }
            }
        }
        Aero::Controls::Border* popupItemBorder =
            activePopupItem != nullptr
            ? FindVisualOfType<Aero::Controls::Border>(
                  *activePopupItem)
            : nullptr;
        const Aero::Presentation::Color hoverColor =
            popupItemBorder != nullptr
            ? Aero::Presentation::SampleBrush(
                  popupItemBorder->BackgroundBrush())
            : Aero::Presentation::Color{};
        const Aero::Presentation::Color itemHoverColor =
            activePopupItem != nullptr
            ? Aero::Presentation::SampleBrush(
                  activePopupItem->BackgroundBrush())
            : Aero::Presentation::Color{};
        const Aero::Base::Ref<
            Aero::Controls::ControlTemplate> popupItemTemplate =
                activePopupItem != nullptr
                ? activePopupItem->GetValueOr(
                      Aero::Controls::Control::
                          TemplateProperty,
                      Aero::Base::Ref<
                          Aero::Controls::
                              ControlTemplate>{})
                : Aero::Base::Ref<
                      Aero::Controls::ControlTemplate>{};
        Aero::Core::EffectiveValueProvider
            itemBackgroundProvider =
                Aero::Core::EffectiveValueProvider::Default;
        Aero::Core::EffectiveValueProvider
            borderBackgroundProvider =
                Aero::Core::EffectiveValueProvider::Default;
        if (view->EffectiveValues() != nullptr &&
            activePopupItem != nullptr) {
            Aero::Base::Result<
                Aero::Core::EffectiveValueDiagnostics>
                valueDiagnostics =
                    view->EffectiveValues()->Diagnostics(
                        *activePopupItem,
                        Aero::Controls::Control::
                            BackgroundProperty.Handle());
            if (valueDiagnostics) {
                itemBackgroundProvider =
                    valueDiagnostics.Value().provider;
            }
        }
        if (view->EffectiveValues() != nullptr &&
            popupItemBorder != nullptr) {
            Aero::Base::Result<
                Aero::Core::EffectiveValueDiagnostics>
                valueDiagnostics =
                    view->EffectiveValues()->Diagnostics(
                        *popupItemBorder,
                        Aero::Controls::Border::
                            BackgroundProperty.Handle());
            if (valueDiagnostics) {
                borderBackgroundProvider =
                    valueDiagnostics.Value().provider;
            }
        }
        std::fprintf(
            stderr,
            "Scoreboard diagnostic popup-hover over=%u bindings=%u triggers=%u item-provider=%u border-provider=%u item-background=%.3f,%.3f,%.3f,%.3f border-background=%.3f,%.3f,%.3f,%.3f\n",
            activePopupItem != nullptr
                ? 1U : 0U,
            popupItemTemplate
                ? popupItemTemplate->Bindings().Size()
                : 0U,
            popupItemTemplate
                ? popupItemTemplate->Triggers().Size()
                : 0U,
            static_cast<unsigned>(
                itemBackgroundProvider),
            static_cast<unsigned>(
                borderBackgroundProvider),
            itemHoverColor.red,
            itemHoverColor.green,
            itemHoverColor.blue,
            itemHoverColor.alpha,
            hoverColor.red,
            hoverColor.green,
            hoverColor.blue,
            hoverColor.alpha);
        const bool popupHoverReady =
            activePopupItem != nullptr &&
            popupItemBorder != nullptr &&
            std::fabs(
                hoverColor.red - (93.0F / 255.0F)) <
                0.01F &&
            std::fabs(
                hoverColor.green - (96.0F / 255.0F)) <
                0.01F &&
            std::fabs(
                hoverColor.blue - (100.0F / 255.0F)) <
                0.01F;
        if (!popupHoverReady &&
            diagnostics->popupHoverWaitFrames++ < 8U) {
            return {};
        }
        if (!popupHoverReady) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::ValidationFailed,
                "Scoreboard ComboBoxItem hover Trigger did not apply its background");
        }
        static_cast<void>(
            selector->SetIsDropDownOpen(false));
        diagnostics->comboOpenValidated = true;
        return {};
    }
    if (!diagnostics->selectionChanged) {
        Aero::Base::Result<bool> selected =
            selector->SetSelectedIndex(1U);
        if (!selected) return selected.GetStatus();
        diagnostics->selectionChanged = true;
        diagnostics->filterPhase = 1U;
        std::fprintf(
            stderr,
            "Scoreboard diagnostic selection=%u enabled=%u\n",
            selector->SelectedIndex(),
            scroll->IsEnabled() ? 1U : 0U);
    }
    if (diagnostics->filterPhase == 2U &&
        scroll->IsEnabled()) {
        diagnostics->sawDisabled = false;
        diagnostics->filterPhase = 3U;
        Aero::Base::Result<bool> selected =
            selector->SetSelectedIndex(2U);
        if (!selected) return selected.GetStatus();
        std::fprintf(
            stderr,
            "Scoreboard diagnostic selection=%u enabled=%u\n",
            selector->SelectedIndex(),
            scroll->IsEnabled() ? 1U : 0U);
        return {};
    }
    if (!scroll->IsEnabled()) {
        if (!diagnostics->sawDisabled) {
            diagnostics->sawDisabled = true;
            std::fprintf(
                stderr,
                "Scoreboard diagnostic transition=disabled\n");
        }
        return {};
    }
    if (diagnostics->sawDisabled) {
        std::uint32_t visibleRows = 0U;
        std::uint32_t collapsedRows = 0U;
        if (Aero::Controls::Panel* itemsHost =
                players->ItemsHost()) {
            for (Aero::Presentation::Visual* container :
                 itemsHost->VisualChildren()) {
                if (container == nullptr ||
                    container->VisualChildren().Empty() ||
                    container->VisualChildren()[0U] ==
                        nullptr) {
                    continue;
                }
                Aero::Presentation::UIElement* row =
                    container->VisualChildren()[0U]->
                        AsUIElement();
                if (row == nullptr) continue;
                if (row->GetVisibility() ==
                    Aero::Presentation::Visibility::
                        Collapsed) {
                    ++collapsedRows;
                } else {
                    ++visibleRows;
                }
            }
        }
        std::fprintf(
            stderr,
            "Scoreboard diagnostic transition=reenabled selection=%u visible=%u collapsed=%u\n",
            selector->SelectedIndex(),
            visibleRows,
            collapsedRows);
        if (visibleRows != 5U ||
            collapsedRows != 5U) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::ValidationFailed,
                "Scoreboard MultiDataTrigger did not filter the player rows");
        }
        if (diagnostics->filterPhase == 1U) {
            diagnostics->sawDisabled = false;
            diagnostics->filterPhase = 2U;
            return {};
        }
        application.Shutdown(0);
    }
    return {};
}

} // namespace

int main(int argc, char** argv) {
    Aero::App::ApplicationHostOptions options;
    Aero::Core::DiagnosticBag diagnostics;
    options.diagnostics = &diagnostics;
    options.startup = &InitializeScoreboard;
    ScoreboardDiagnostics scoreboardDiagnostics;
    if (ScoreboardDiagnosticsEnabled()) {
        options.visible = false;
        options.frame = &DiagnoseScoreboardFrame;
        options.frameContext =
            &scoreboardDiagnostics;
    }
    options.applicationFile =
        argc > 1 && argv[1] != nullptr
        ? Aero::Base::StringView(
              argv[1],
              static_cast<std::uint32_t>(
                  std::strlen(argv[1])))
        : Aero::Base::StringView(
              AERO_SCOREBOARD_APP_FILE);

    Aero::App::ApplicationHost host(options);
    Aero::Base::Result<void> module =
        host.AddModule(
            Aero::Samples::Scoreboard::
                MakeScoreboardModuleManifest());
    if (!module) {
        std::fprintf(
            stderr,
            "Scoreboard module registration failed: %s\n",
            module.GetStatus().message);
        return 1;
    }

    Aero::Base::Result<int> result = host.Run();
    if (!result) {
        std::fprintf(
            stderr,
            "Scoreboard application failed: %s\n",
            result.GetStatus().message);
        for (const Aero::Core::Diagnostic& diagnostic :
             diagnostics.Items()) {
            std::fprintf(
                stderr,
                "  %u:%u: %.*s\n",
                diagnostic.Source().begin.line,
                diagnostic.Source().begin.column,
                static_cast<int>(
                    diagnostic.Message().SizeBytes()),
                diagnostic.Message().Data());
        }
        return 1;
    }
    return result.Value();
}
