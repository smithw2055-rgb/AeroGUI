#pragma once

#include <Aero/Meta/Describe.hpp>
#include <Aero/Meta/Registry.hpp>
#include <Aero/Meta/ValueCodec.hpp>
#include <Aero/Meta/ValueConversion.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Module.hpp>

#include <utility>

namespace Aero::Meta {

using TypeId = Core::TypeId;
using MemberId = Core::MemberId;
using Routing = Aero::RoutingStrategy;
using TypeFlags = Core::TypeFlags;
using PropertyFlags = Core::PropertyFlags;
using FrameworkPropertyMetadataOptions = Core::FrameworkPropertyMetadataOptions;


template<class TOwner, class TValue>
using DependencyProperty =
    Core::DependencyPropertyRef<TOwner, TValue>;

template<class TOwner, class TValue>
using AttachedProperty =
    Core::AttachedPropertyRef<TOwner, TValue>;

template<class TOwner, class TValue>
using ReadOnlyProperty =
    Core::ReadOnlyPropertyRef<TOwner, TValue>;

template<class TOwner, class TArgs>
using RoutedEvent = Aero::RoutedEventRef<TOwner, TArgs>;

template<class TValue>
class PropertyOptions final
    : public Core::PropertyOptions<TValue> {
public:
    using Core::PropertyOptions<TValue>::PropertyOptions;
};

template<class TValue>
PropertyOptions(TValue) -> PropertyOptions<TValue>;

template<class TValue>
class FrameworkPropertyMetadata final
    : public Core::PropertyOptions<TValue> {
public:
    explicit FrameworkPropertyMetadata(
        TValue defaultValue,
        Core::FrameworkPropertyMetadataOptions options =
            Core::FrameworkPropertyMetadataOptions::None) noexcept
        : Core::PropertyOptions<TValue>(std::move(defaultValue)) {
        this->Apply(options);
    }
};

template<class TValue>
FrameworkPropertyMetadata(TValue) -> FrameworkPropertyMetadata<TValue>;

template<class TValue>
FrameworkPropertyMetadata(TValue, Core::FrameworkPropertyMetadataOptions)
    -> FrameworkPropertyMetadata<TValue>;

template<class T>
using ValueCodec = Core::ValueCodec<T>;


} // namespace Aero::Meta
