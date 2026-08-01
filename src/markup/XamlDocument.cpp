#include <Aero/Markup/XamlDocument.hpp>

#include <Aero/Base/Result.hpp>
#include <Aero/Resources.hpp>

#include "markup/XamlDocumentAccess.hpp"

#include <new>
#include <utility>

namespace Aero {

struct UiDocument::Impl final {
    explicit Impl(Markup::LoaderResult&& value) noexcept
        : result(std::move(value)) {}

    Markup::LoaderResult result;
};

UiDocument::~UiDocument() noexcept {
    Reset();
}

UiDocument::UiDocument(UiDocument&& other) noexcept
    : allocator_(other.allocator_), impl_(other.impl_) {
    other.allocator_ = nullptr;
    other.impl_ = nullptr;
}

UiDocument& UiDocument::operator=(UiDocument&& other) noexcept {
    if (this == &other) return *this;
    Reset();
    allocator_ = other.allocator_;
    impl_ = other.impl_;
    other.allocator_ = nullptr;
    other.impl_ = nullptr;
    return *this;
}

Base::Result<UiDocument> Aero::Detail::UiDocumentAccess::Adopt(
    Markup::LoaderResult&& result,
    Base::IAllocator& allocator) noexcept {
    if (!result.root) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "UI document requires a loaded root object");
    }
    void* memory = allocator.Allocate({
        sizeof(UiDocument::Impl),
        alignof(UiDocument::Impl),
        Base::MemoryTag::Markup});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "UI document allocation failed");
    }
    UiDocument document;
    document.allocator_ = &allocator;
    document.impl_ =
        new (memory) UiDocument::Impl(std::move(result));
    return document;
}

bool UiDocument::IsValid() const noexcept {
    return impl_ != nullptr && impl_->result.root;
}

const Base::Ref<Base::Object>& UiDocument::Root() const noexcept {
    static const Base::Ref<Base::Object> empty;
    return impl_ != nullptr ? impl_->result.root : empty;
}

Base::Object* UiDocument::FindNamedObject(
    Base::StringView name,
    Core::TypeId expectedType) noexcept {
    if (impl_ == nullptr || name.Empty()) return nullptr;
    Base::Object* object = impl_->result.names.Find(name);
    if (object == nullptr || expectedType == Core::InvalidTypeId) {
        return object;
    }
    // A document is schema-neutral. Exact type validation remains available
    // without retaining a MetadataRuntime; derived-type lookup is performed by
    // View after mounting.
    return object->RuntimeType() == expectedType ? object : nullptr;
}

std::uint32_t UiDocument::NamedObjectCount() const noexcept {
    return impl_ != nullptr ? impl_->result.names.Size() : 0U;
}

Aero::ResourceDictionary* UiDocument::Resources() noexcept {
    return impl_ != nullptr ? &impl_->result.resources : nullptr;
}

const Aero::ResourceDictionary* UiDocument::Resources() const noexcept {
    return impl_ != nullptr ? &impl_->result.resources : nullptr;
}

const Base::ResourceUri& UiDocument::CanonicalUri() const noexcept {
    static const Base::ResourceUri empty;
    return impl_ != nullptr ? impl_->result.canonicalUri : empty;
}

Base::Span<const Base::ResourceUri> UiDocument::Dependencies() const noexcept {
    return impl_ != nullptr
        ? Base::Span<const Base::ResourceUri>{
              impl_->result.dependencies.Data(),
              impl_->result.dependencies.Size()}
        : Base::Span<const Base::ResourceUri>{};
}

const Markup::EffectLifetime*
Aero::Detail::UiDocumentAccess::RuntimeLifetime(
    const UiDocument& document) noexcept {
    return document.impl_ != nullptr
        ? document.impl_->result.runtimeLifetime.Get()
        : nullptr;
}

Markup::LoaderResult Aero::Detail::UiDocumentAccess::Take(
    UiDocument& document) noexcept {
    if (document.impl_ == nullptr) {
        Markup::LoaderResult empty;
        return empty;
    }
    Markup::LoaderResult result =
        std::move(document.impl_->result);
    document.Reset();
    return result;
}

void UiDocument::Reset() noexcept {
    if (impl_ == nullptr) return;
    impl_->result.Clear();
    impl_->~Impl();
    allocator_->Deallocate(
        impl_,
        sizeof(UiDocument::Impl),
        alignof(UiDocument::Impl),
        Base::MemoryTag::Markup);
    impl_ = nullptr;
    allocator_ = nullptr;
}

} // namespace Aero
