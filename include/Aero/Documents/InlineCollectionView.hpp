#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>

#include <cstdint>

namespace Aero::Controls { class TextBlock; }

namespace Aero::Documents {

class Inline;
class Span;
class InlineCollection;

class AERO_GUI_API InlineCollectionView {
public:
    InlineCollectionView() noexcept = default;
    std::uint32_t GetCount() const noexcept;
    bool GetIsEmpty() const noexcept { return GetCount() == 0U; }
    const Inline* GetItem(std::uint32_t index) const noexcept;

private:
    friend class InlineCollection;
    friend class Span;
    friend class Aero::Controls::TextBlock;
    explicit InlineCollectionView(const Base::Object& owner) noexcept
        : owner_(&owner) {}
    const Base::Object* owner_ = nullptr;
};

} // namespace Aero::Documents
