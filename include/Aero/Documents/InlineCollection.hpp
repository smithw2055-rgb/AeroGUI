#pragma once

#include <Aero/Documents/InlineCollectionView.hpp>

namespace Aero::Documents {

class AERO_GUI_API InlineCollection {
public:
    InlineCollection() noexcept = default;
    std::uint32_t GetCount() const noexcept;
    bool GetIsEmpty() const noexcept { return GetCount() == 0U; }
    Inline* GetItem(std::uint32_t index) const noexcept;
    InlineCollectionView GetView() const noexcept;
    Result<void> Add(Ref<Inline> value) noexcept;
    Result<bool> Remove(Inline& value) noexcept;
    void Clear() noexcept;

private:
    friend class Span;
    friend class Aero::Controls::TextBlock;
    explicit InlineCollection(Base::Object& owner) noexcept : owner_(&owner) {}
    Base::Object* owner_ = nullptr;
};

} // namespace Aero::Documents
