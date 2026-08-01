#pragma once

#include <Aero/DependencyProperty.hpp>
#include <Aero/Diagnostics/PropertyValueSource.hpp>

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
using FrameworkPropertyMetadata = Core::FrameworkPropertyMetadata;
using FrameworkPropertyMetadataOptions = Core::FrameworkPropertyMetadataOptions;
using PropertyMetadataFlags = Core::PropertyMetadataFlags;
using UpdateSourceTrigger = Core::UpdateSourceTrigger;

} // namespace Aero
