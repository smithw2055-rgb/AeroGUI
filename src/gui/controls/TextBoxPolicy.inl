
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

} // namespace Aero::Controls

namespace Aero::Controls {

using namespace Primitives;
using namespace ::Aero::Render;
} // namespace Aero::Controls

namespace Aero::Controls {
using ::Aero::Controls::TextDisplayPolicy;
using ::Aero::Controls::PlainTextDisplayPolicy;
using ::Aero::Controls::PasswordTextDisplayPolicy;
using ::Aero::Controls::TextLayoutRequest;
using ::Aero::Controls::TextLayoutResult;
} // namespace Aero::Controls

namespace Aero::Controls {
using namespace Primitives;
