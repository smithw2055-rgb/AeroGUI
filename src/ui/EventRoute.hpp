#pragma once

#include "VisualAccess.hpp"
#include <utility>
#include <Aero/RoutedEvent.hpp>

namespace Aero::Detail {

inline Base::Result<void> BuildEventRoute(
    Visual& source,
    RoutingStrategy strategy,
    Base::Vector<VisualLease>& route) noexcept {
    route.Clear();

    Visual* current = &source;
    while (current != nullptr) {
        Base::Result<VisualLease> lease = VisualLease::Acquire(*current);
        if (!lease) return lease.GetStatus();
        Base::Result<void> appended = route.TryPushBack(std::move(lease).Value());
        if (!appended) return appended.GetStatus();
        if (strategy == RoutingStrategy::Direct) break;
        current = current->GetVisualParent() != nullptr
            ? current->GetVisualParent()
            : current->GetLogicalParent();
    }

    if (strategy == RoutingStrategy::Tunnel && route.Size() > 1U) {
        for (std::uint32_t left = 0U, right = route.Size() - 1U; left < right; ++left, --right) {
            VisualLease temporary = std::move(route[left]);
            route[left] = std::move(route[right]);
            route[right] = std::move(temporary);
        }
    }
    return {};
}

} // namespace Aero::Detail
