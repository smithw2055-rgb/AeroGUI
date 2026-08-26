#pragma once

#include <Aero/Documents/Inline.hpp>
#include <Aero/Documents/InlineCollection.hpp>

namespace Aero::Controls { struct TextBlockDocumentHelper; }

namespace Aero::Documents {

class AERO_GUI_API Span : public Inline {
    AERO_DECLARE_TYPE(Span, Inline)
public:
    Span() noexcept : Span(StaticTypeId()) {}
    ~Span() override;

    InlineCollection GetInlines() noexcept { return InlineCollection(*this); }
    InlineCollectionView GetInlines() const noexcept {
        return InlineCollectionView(*this);
    }
    Value GetMetadataInlines() const noexcept;
    void SetInlineValue(Value value) noexcept;
    Result<void> AddOwnedInline(Ref<Inline> value) noexcept;
    void ClearOwnedInlines() noexcept;

protected:
    explicit Span(Meta::TypeId runtimeType) noexcept
        : Inline(runtimeType), inlines_() {}
    std::uint32_t GetLogicalChildrenCount() const noexcept override {
        return inlines_.Size();
    }
    DependencyObject* GetLogicalChild(std::uint32_t index) const noexcept override {
        return index < inlines_.Size() ? inlines_[index].Get() : nullptr;
    }

private:
    friend class Aero::Controls::TextBlock;
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
    friend struct Aero::Controls::TextBlockDocumentHelper;
#endif
    Base::Vector<Ref<Inline>> inlines_;
    Ref<Inline> pendingInline_;
};

} // namespace Aero::Documents
