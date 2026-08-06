#pragma once

#include "private/RenderDevice.hpp"

// Source-only migration seam. The installed SDK exposes RenderDevice from the
// Aero namespace; existing backend implementation files may use the former
// Integration-qualified spelling until their next backend-specific cleanup.
namespace Aero::Integration {
using RenderDevice = ::Aero::RenderDevice;
using RenderDeviceMode = ::Aero::RenderDeviceMode;
using RenderPresentMode = ::Aero::RenderPresentMode;
using RenderDeviceState = ::Aero::RenderDeviceState;
using RenderDeviceStatistics = ::Aero::RenderDeviceStatistics;
using RenderFrameStatistics = ::Aero::RenderFrameStatistics;
}
