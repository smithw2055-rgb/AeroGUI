#pragma once

// Shared base-class implementation helpers.
//
// These are the small, file-local helpers that the WPF semantic kernel
// (Visual / UIElement / FrameworkElement / DependencyObject / Freezable)
// relies on. They used to be duplicated inside anonymous namespaces across
// several translation units. They are collected here so the relocated
// base-class method definitions in this directory can compile standalone
// while source files keep their own (internal-linkage) copies.

#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Events.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Base/Allocator.hpp>

#include <cstdint>
#include <climits>

namespace Aero {
namespace {

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status NotFound(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotFound, message);
}

constexpr Base::Status ReadOnlyStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ReadOnly,
        "Dependency property is read-only");
}

constexpr Base::Status ValidationFailedStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "Dependency property value validation failed");
}

constexpr std::uint32_t InvalidIndex = UINT32_MAX;

struct RoutedHandlerRecord {
    RoutedEventHandle event;
    Aero::RoutedHandlerStorage handler;
    std::uint64_t sequence = 0U;
    bool handledEventsToo = false;
};

struct UIElementHandlerState {
    Base::Vector<RoutedHandlerRecord> handlers;
    std::uint64_t nextSequence = 1U;
};

} // namespace
} // namespace Aero
