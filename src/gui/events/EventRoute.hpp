#pragma once

#include "gui/tree/VisualAccess.hpp"
#include <Aero/RoutedEvent.hpp>

#include <utility>

namespace Aero::Detail {

class EventRoute final {
public:
    explicit EventRoute(Base::IAllocator* allocator = nullptr) noexcept
        : nodes_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()) {}

    Base::Result<void> Build(Visual& source, RoutingStrategy strategy) noexcept {
        nodes_.Clear();

        Visual* current = &source;
        while (current != nullptr) {
            Base::Result<VisualLease> lease = VisualLease::Acquire(*current);
            if (!lease) return lease.GetStatus();
            Base::Result<void> appended = nodes_.TryPushBack(std::move(lease).Value());
            if (!appended) return appended.GetStatus();
            if (strategy == RoutingStrategy::Direct) break;
            current = current->GetVisualParent() != nullptr
                ? current->GetVisualParent()
                : current->GetLogicalParent();
        }

        if (strategy == RoutingStrategy::Tunnel && nodes_.Size() > 1U) {
            for (std::uint32_t left = 0U, right = nodes_.Size() - 1U;
                 left < right; ++left, --right) {
                VisualLease temporary = std::move(nodes_[left]);
                nodes_[left] = std::move(nodes_[right]);
                nodes_[right] = std::move(temporary);
            }
        }
        return {};
    }

    Base::Span<const VisualLease> Nodes() const noexcept { return nodes_.AsSpan(); }
    std::uint32_t Size() const noexcept { return nodes_.Size(); }
    bool Empty() const noexcept { return nodes_.Empty(); }

private:
    Base::Vector<VisualLease> nodes_;
};

} // namespace Aero::Detail
