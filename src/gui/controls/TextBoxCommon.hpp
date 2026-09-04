#pragma once

// Shared helpers / display policies for TextBox* translation units
// (formerly anonymous helpers + TextBoxPolicy.inl amalgamated into TextBox.cpp).

#include "gui/core/State.hpp"
#include "gui/text/EditableText.hpp"
#include "TextBlockLayout.hpp"

#include <Aero/UIElement.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>

namespace Aero::Controls {
namespace TextBoxSupport {

inline constexpr double DefaultAdvance = 8.0;
inline constexpr double DefaultLineHeight = 18.0;
inline constexpr double CaretWidth = 1.0;
inline constexpr double ScrollLine = 16.0;

inline std::uint32_t EffectiveMaximumLength(
    std::uint32_t value) noexcept {
    return value == 0U ? UINT32_MAX : value;
}

inline double ClampOffset(
    double value,
    double extent,
    double viewport) noexcept {
    const double maximum =
        std::max(0.0, extent - viewport);
    return std::clamp(value, 0.0, maximum);
}

inline double ClampOffset(
    double value,
    double extent,
    double viewport,
    bool enabled) noexcept {
    if (!enabled) return 0.0;
    return ClampOffset(value, extent, viewport);
}

inline Point ToLocalPoint(
    const UIElement& element,
    Point point) noexcept {
    const UIElement* current = &element;
    while (current != nullptr) {
        const Rect slot = current->GetLayoutSlot();
        point.x -= slot.x;
        point.y -= slot.y;
        current = current->LayoutParent();
    }
    return point;
}

inline Rect ToRootRect(
    const UIElement& element,
    Rect rect) noexcept {
    const UIElement* current = &element;
    while (current != nullptr) {
        const Rect slot = current->GetLayoutSlot();
        rect.x += slot.x;
        rect.y += slot.y;
        current = current->LayoutParent();
    }
    return rect;
}

} // namespace TextBoxSupport

using TextBoxSupport::DefaultAdvance;
using TextBoxSupport::DefaultLineHeight;
using TextBoxSupport::CaretWidth;
using TextBoxSupport::ScrollLine;
using TextBoxSupport::EffectiveMaximumLength;
using TextBoxSupport::ClampOffset;
using TextBoxSupport::ToLocalPoint;
using TextBoxSupport::ToRootRect;

class TextDisplayPolicy {
public:
    virtual ~TextDisplayPolicy() = default;
    virtual Base::Result<void> BuildDisplayText(
        const ::Aero::Text::EditableTextModel& model,
        Base::String& output) noexcept = 0;
    virtual bool AllowsCopy() const noexcept = 0;
    virtual bool AllowsCut() const noexcept = 0;
};

class PlainTextDisplayPolicy : public TextDisplayPolicy {
public:
    Base::Result<void> BuildDisplayText(
        const ::Aero::Text::EditableTextModel& model,
        Base::String& output) noexcept override {
        return model.Snapshot(output);
    }
    bool AllowsCopy() const noexcept override { return true; }
    bool AllowsCut() const noexcept override { return true; }
};

class PasswordTextDisplayPolicy : public TextDisplayPolicy {
public:
    explicit PasswordTextDisplayPolicy(
        Base::IAllocator* allocator = nullptr) noexcept
        : mask_(allocator) {
        static_cast<void>(mask_.Assign(
            Base::StringView(u8"\u2022")));
    }

    Base::Result<void> SetMask(Base::StringView value) noexcept {
        ::Aero::Text::EditableTextModel validation;
        Base::Result<void> assigned = validation.SetText(value);
        if (!assigned || validation.GraphemeCount() != 1U) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Password mask must be one grapheme cluster");
        }
        return mask_.Assign(value);
    }

    Base::StringView GetMask() const noexcept { return mask_.View(); }

    Base::Result<void> BuildDisplayText(
        const ::Aero::Text::EditableTextModel& model,
        Base::String& output) noexcept override {
        output.Clear();
        const std::uint32_t count = model.GraphemeCount();
        if (count != 0U && mask_.SizeBytes() > UINT32_MAX / count) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Password display text exceeds capacity");
        }
        Base::Result<void> reserved =
            output.Reserve(mask_.SizeBytes() * count);
        if (!reserved) return reserved;
        Base::String source;
        Base::Result<void> snapshot = model.Snapshot(source);
        if (!snapshot) return snapshot;
        for (std::uint32_t index = 0U; index < count; ++index) {
            Base::Result<std::uint32_t> begin =
                model.ByteOffsetForGrapheme(index);
            if (!begin) return begin.GetStatus();
            Base::Result<std::uint32_t> end =
                model.ByteOffsetForGrapheme(index + 1U);
            if (!end) return end.GetStatus();
            const Base::StringView cluster = source.View().Substr(
                begin.Value(), end.Value() - begin.Value());
            const bool newline = !cluster.Empty() &&
                (cluster[0] == '\r' || cluster[0] == '\n');
            Base::Result<void> appended = output.Append(
                newline ? cluster : mask_.View());
            if (!appended) return appended;
        }
        return {};
    }

    bool AllowsCopy() const noexcept override { return false; }
    bool AllowsCut() const noexcept override { return false; }

private:
    Base::String mask_;
};

inline ::Aero::Text::EditableTextModel& Model(
    void* value) noexcept {
    return *static_cast<::Aero::Text::EditableTextModel*>(value);
}

inline const ::Aero::Text::EditableTextModel& Model(
    const void* value) noexcept {
    return *static_cast<const ::Aero::Text::EditableTextModel*>(value);
}

inline TextDisplayPolicy* DisplayPolicy(
    void* value) noexcept {
    return static_cast<TextDisplayPolicy*>(value);
}

inline PasswordTextDisplayPolicy* PasswordPolicy(
    void* value) noexcept {
    return static_cast<PasswordTextDisplayPolicy*>(value);
}

inline TextBlockLayout* LayoutService(
    const ::Aero::Media::Visual& visual) noexcept {
    return AeroGuiInternal::TypedTextLayoutRuntime<TextBlockLayout>(visual);
}

} // namespace Aero::Controls
