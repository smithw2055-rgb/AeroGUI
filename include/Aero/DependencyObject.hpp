#pragma once

#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Core/Property/PropertyValueSource.hpp>

namespace Aero {

using DependencyObject = Core::DependencyObject;
using DependencyProperty = Core::DependencyProperty;
using DependencyPropertyKey = Core::DependencyPropertyKey;
using DependencyPropertyHandle = Core::DependencyPropertyHandle;
using DependencyPropertyChangedEventArgs =
    Core::DependencyPropertyChangedEventArgs;
using DependencyPropertyChangedEventHandler =
    Core::DependencyPropertyChangedEventHandler;
using PropertyMetadata = Core::PropertyMetadata;
using PropertyMetadataFlags = Core::PropertyMetadataFlags;
using PropertyValueRank = Core::PropertyValueRank;
using PropertyValueSourceInfo = Core::PropertyValueSourceInfo;
using UpdateSourceTrigger = Core::UpdateSourceTrigger;

} // namespace Aero
