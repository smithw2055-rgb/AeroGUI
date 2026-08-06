#pragma once

#include <Aero/Integration/RenderSurface.hpp>
#include "private/RenderDevice.hpp"

// Source-only migration seam for backend implementation files. Installed
// headers expose RenderDevice at Aero scope and presentation through
// Integration::RenderSurface.
namespace Aero::Integration {
using RenderDevice = ::Aero::RenderDevice;
using RenderDeviceMode = Detail::RenderDeviceMode;
using RenderPresentMode = PresentMode;
using RenderDeviceState = ::Aero::RenderDeviceState;
using RenderDeviceStatistics = ::Aero::RenderDeviceStatistics;
using RenderFrameStatistics = ::Aero::RenderFrameStatistics;

struct D3D11EmbeddedSurfaceOptions;
struct D3D11WindowSurfaceOptions;
using D3D11EmbeddedDeviceOptions = D3D11EmbeddedSurfaceOptions;
using D3D11WindowDeviceOptions = D3D11WindowSurfaceOptions;

struct OpenGL33EmbeddedSurfaceOptions;
struct OpenGL33WindowSurfaceOptions;
using OpenGL33EmbeddedDeviceOptions = OpenGL33EmbeddedSurfaceOptions;
using OpenGL33WindowDeviceOptions = OpenGL33WindowSurfaceOptions;
}
