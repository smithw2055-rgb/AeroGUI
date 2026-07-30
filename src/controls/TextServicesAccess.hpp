#pragma once

#include "TextLayoutService.hpp"

#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/TextBox.hpp>

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
