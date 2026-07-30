#pragma once

#include <Aero/Core/Metadata/Describe.hpp>
#include <Aero/Core/Metadata/ValueCodec.hpp>
#include <Aero/Core/Metadata/ValueConversion.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Core/RoutedEvent.hpp>
#include <Aero/Module.hpp>

namespace Aero::Meta {

using Context = Core::MetadataContext;
using TypeId = Core::TypeId;
using MemberId = Core::MemberId;
using Routing = Core::RoutingStrategy;
using TypeFlags = Core::TypeFlags;
using PropertyFlags = Core::PropertyFlags;


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
using RoutedEvent = Core::RoutedEventRef<TOwner, TArgs>;

template<class TValue>
class PropertyOptions final
    : public Core::PropertyOptions<TValue> {
public:
    using Core::PropertyOptions<TValue>::PropertyOptions;
};

template<class TValue>
PropertyOptions(TValue) -> PropertyOptions<TValue>;

template<class T>
using ValueCodec = Core::ValueCodec<T>;

using Core::Describe;

} // namespace Aero::Meta
