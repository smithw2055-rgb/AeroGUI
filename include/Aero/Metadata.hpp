#pragma once

#include <Aero/Meta.hpp>

#include <Aero/Core/Metadata/Describe.hpp>
#include <Aero/Core/Metadata/ValueCodec.hpp>
#include <Aero/Core/Metadata/ValueConversion.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Core/RoutedEvent.hpp>
#include <Aero/Module.hpp>

namespace Aero {

using MetadataContext = Core::MetadataContext;
using Routing = Core::RoutingStrategy;

template<class TOwner, class TValue>
using DependencyPropertyRef =
    Core::DependencyPropertyRef<TOwner, TValue>;

template<class TOwner, class TValue>
using AttachedPropertyRef =
    Core::AttachedPropertyRef<TOwner, TValue>;

template<class TOwner, class TValue>
using ReadOnlyPropertyRef =
    Core::ReadOnlyPropertyRef<TOwner, TValue>;

template<class TOwner, class TArgs>
using RoutedEventRef =
    Core::RoutedEventRef<TOwner, TArgs>;

template<class TValue>
using PropertyOptions = Meta::PropertyOptions<TValue>;

template<class T>
using ValueCodec = Core::ValueCodec<T>;

using Core::Describe;

} // namespace Aero
