#include <Aero/UiDocument.hpp>

#include <Aero/Base/Result.hpp>
#include <Aero/Markup/Runtime/XamlLoadResult.hpp>
#include <Aero/Presentation/Resources.hpp>

#include <new>
#include <utility>

namespace Aero {

struct UiDocument::Impl final {
    explicit Impl(Markup::XamlLoadResult&& value) noexcept
        : result(std::move(value)) {}

    Markup::XamlLoadResult result;
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

Base::Result<UiDocument> UiDocument::Adopt(
    Markup::XamlLoadResult&& result,
    Base::IAllocator& allocator) noexcept {
    if (!result.root) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "UI document requires a loaded root object");
    }
    void* memory = allocator.Allocate({
        sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "UI document allocation failed");
    }
    UiDocument document;
    document.allocator_ = &allocator;
    document.impl_ = new (memory) Impl(std::move(result));
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
    // RuntimeHost after mounting.
    return object->RuntimeType() == expectedType ? object : nullptr;
}

std::uint32_t UiDocument::NamedObjectCount() const noexcept {
    return impl_ != nullptr ? impl_->result.names.Size() : 0U;
}

Presentation::ResourceDictionary* UiDocument::Resources() noexcept {
    return impl_ != nullptr ? &impl_->result.resources : nullptr;
}

const Presentation::ResourceDictionary* UiDocument::Resources() const noexcept {
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

const Markup::XamlEffectLifetime*
UiDocument::RuntimeLifetime() const noexcept {
    return impl_ != nullptr
        ? impl_->result.runtimeLifetime.Get()
        : nullptr;
}

Markup::XamlLoadResult UiDocument::TakeResult() noexcept {
    if (impl_ == nullptr) {
        Markup::XamlLoadResult empty;
        return empty;
    }
    Markup::XamlLoadResult result = std::move(impl_->result);
    Reset();
    return result;
}

void UiDocument::Reset() noexcept {
    if (impl_ == nullptr) return;
    impl_->result.Clear();
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
    impl_ = nullptr;
    allocator_ = nullptr;
}

} // namespace Aero
