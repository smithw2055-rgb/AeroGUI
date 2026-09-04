#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/StreamGeometryContext.hpp>

namespace Aero::Media {

class AERO_GUI_API StreamGeometry : public Geometry {
    AERO_DECLARE_TYPE(StreamGeometry, Geometry)
public:
    StreamGeometry() noexcept : Geometry(StaticTypeId()) {}
    ~StreamGeometry() override = default;
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    StringView GetData() const noexcept { return data_.View(); }
    void SetData(StringView value) noexcept;
    Rect GetBounds() const noexcept override;
    void SetBounds(Rect value) noexcept {
        if (!WritePreamble() ||
            (bounds_.x == value.x && bounds_.y == value.y &&
             bounds_.width == value.width &&
             bounds_.height == value.height)) return;
        bounds_ = value;
        boundsValid_ = true;
        WritePostscript();
    }
    StreamGeometryContext Open() noexcept;

protected:
    Result<void> FlattenCore(FlattenSink& sink) const noexcept override;

private:
    friend class StreamGeometryContext;

    enum class CommandKind : std::uint8_t {
        BeginFigure = 0U,
        LineTo,
        BezierTo,
        QuadraticBezierTo,
        ArcTo,
        Close
    };

    struct Command {
        CommandKind kind = CommandKind::LineTo;
        bool filled = true;
        bool closed = false;
        bool isStroked = true;
        bool isSmoothJoin = false;
        bool largeArc = false;
        bool sweepClockwise = false;
        Point p0{};
        Point p1{};
        Point p2{};
        Size size{};
        double rotation = 0.0;
    };

    Result<void> AppendCommand(const Command& command) noexcept;
    Result<void> ReplayCommands(FlattenSink& sink) const noexcept;
    void InvalidateBounds() noexcept {
        bounds_ = {};
        boundsValid_ = false;
    }

    String data_;
    Base::Vector<Command> commands_;
    mutable Rect bounds_{};
    mutable bool boundsValid_ = false;
};

} // namespace Aero::Media
