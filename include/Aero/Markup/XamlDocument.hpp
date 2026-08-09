#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Meta.hpp>

#include <cstdint>
#include <type_traits>

namespace Aero {
class ResourceDictionary;
}

namespace Aero::Markup {

struct XamlDocumentState;

// Move-only ownership for one successfully loaded XAML document. The document
// keeps names, resources, dependency URIs, and the declaration/mount plan alive
// independently from a View until it is mounted or discarded.
class AERO_GUI_API XamlDocument {
public:
    XamlDocument() noexcept = default;
    ~XamlDocument() noexcept;

    XamlDocument(XamlDocument&& other) noexcept;
    XamlDocument& operator=(XamlDocument&& other) noexcept;

    XamlDocument(const XamlDocument&) = delete;
    XamlDocument& operator=(const XamlDocument&) = delete;

    bool IsValid() const noexcept;
    const Ref<Base::Object>& Root() const noexcept;
    template<class T>
    T* Root() noexcept {
        static_assert(std::is_base_of_v<Base::Object, T>,
            "XamlDocument::Root<T> requires an Aero object type");
        return static_cast<T*>(RootObject(Meta::TypeOf<T>()));
    }
    template<class T>
    const T* Root() const noexcept {
        return const_cast<XamlDocument*>(this)->Root<T>();
    }
    Base::Object* FindName(
        StringView name,
        Meta::TypeId expectedType = Meta::InvalidTypeId) noexcept;
    template<class T>
    T* FindName(StringView name) noexcept {
        static_assert(std::is_base_of_v<Base::Object, T>,
            "XamlDocument::FindName<T> requires an Aero object type");
        return static_cast<T*>(FindName(name, Meta::TypeOf<T>()));
    }
    std::uint32_t NamedObjectCount() const noexcept;
    Aero::ResourceDictionary* Resources() noexcept;
    const Aero::ResourceDictionary* Resources() const noexcept;
    const Base::ResourceUri& CanonicalUri() const noexcept;
    Span<const Base::ResourceUri> Dependencies() const noexcept;

private:
    friend struct XamlDocumentState;

    void Reset() noexcept;
    Base::Object* RootObject(Meta::TypeId expectedType) noexcept;

    Base::IAllocator* allocator_ = nullptr;
    void* state_ = nullptr;
};

} // namespace Aero::Markup
