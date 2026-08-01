#pragma once

#include "runtime/MeshResourceContract.hpp"
#include <Aero/Controls/Panels.hpp>
#include "TextLayoutService.hpp"
#include <Aero/Controls/Text.hpp>

namespace Aero::Controls::Detail {

class PathServicesAccess final {
public:
    static void InvalidateGeometry(
        Path& path) noexcept {
        path.ResetGeometry();
    }

    static void Attach(
        Path& path,
        Aero::Detail::MeshBackendServices* services,
        bool invalidate = false) noexcept {
        path.AttachMeshServices(
            services, invalidate);
        if (invalidate) {
            static_cast<void>(path.InvalidateRender());
        }
    }
};

} // namespace Aero::Controls::Detail

namespace Aero::Controls::Detail {

class TextServicesAccess final {
public:
    static void Attach(
        TextBlock& text,
        TextLayoutService* service,
        bool invalidate = false) noexcept {
        if (text.layoutService_ == service && !invalidate) {
            return;
        }
        text.ReleaseServiceGlyphRun();
        text.layoutService_ = service;
        text.glyphRuns_.Clear();
        text.glyphRunSize_ = {};
        if (invalidate) {
            static_cast<void>(text.InvalidateMeasure());
            static_cast<void>(text.InvalidateRender());
        }
    }

    static void Attach(
        TextBox& text,
        TextLayoutService* service,
        bool invalidate = false) noexcept {
        if (text.layoutService_ == service && !invalidate) {
            return;
        }
        text.ReleaseGlyphRuns();
        text.layoutService_ = service;
        if (invalidate) {
            static_cast<void>(text.InvalidateMeasure());
            static_cast<void>(text.InvalidateRender());
        }
    }

    static void Attach(
        PasswordBox& password,
        TextLayoutService* service,
        bool invalidate = false) noexcept {
        Attach(
            password.editor_,
            service,
            invalidate);
        if (invalidate) {
            static_cast<void>(
                password.InvalidateMeasure());
            static_cast<void>(
                password.InvalidateRender());
        }
    }
};

} // namespace Aero::Controls::Detail
