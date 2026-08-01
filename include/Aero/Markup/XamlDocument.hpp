#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Meta/MetadataId.hpp>

#include <cstdint>

namespace Aero {

class ResourceDictionary;

namespace Detail {
class UiDocumentAccess;
}

// Move-only ownership for one successfully loaded XAML document. The document
// keeps names, resources, dependency URIs, and the declaration/mount plan alive
// independently from a View until it is mounted or discarded.
class AERO_API UiDocument final {
public:
    UiDocument() noexcept = default;
    ~UiDocument() noexcept;

    UiDocument(UiDocument&& other) noexcept;
    UiDocument& operator=(UiDocument&& other) noexcept;

    UiDocument(const UiDocument&) = delete;
    UiDocument& operator=(const UiDocument&) = delete;

    bool IsValid() const noexcept;
    const Base::Ref<Base::Object>& Root() const noexcept;
    Base::Object* FindNamedObject(
        Base::StringView name,
        Core::TypeId expectedType = Core::InvalidTypeId) noexcept;
    std::uint32_t NamedObjectCount() const noexcept;
    Aero::ResourceDictionary* Resources() noexcept;
    const Aero::ResourceDictionary* Resources() const noexcept;
    const Base::ResourceUri& CanonicalUri() const noexcept;
    Base::Span<const Base::ResourceUri> Dependencies() const noexcept;

private:
    friend class Aero::Detail::UiDocumentAccess;
    struct Impl;

    void Reset() noexcept;

    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero
