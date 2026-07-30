#include "SchemaBundle.hpp"

#include <Aero/Base/Assert.hpp>
#include "markup/Extensions.hpp"
#include "core/metadata/MetadataDomainAccess.hpp"
#include "markup/PresentationObjectModel.hpp"
#include "markup/ResourceSupport.hpp"
#include "markup/ObjectWriter.hpp"
#include <Aero/Markup/Schema.hpp>

#include <new>
#include <utility>
#include "presentation/RuntimeManagers.hpp"

namespace Aero {
namespace {

template<class T, class... TArgs>
Base::Result<T*> Create(
    Base::IAllocator& allocator,
    Base::MemoryTag tag,
    TArgs&&... arguments) noexcept {
    void* memory = allocator.Allocate({sizeof(T), alignof(T), tag});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Schema bundle service allocation failed");
    }
    return new (memory) T(std::forward<TArgs>(arguments)...);
}

template<class T>
void Destroy(
    Base::IAllocator& allocator,
    Base::MemoryTag tag,
    T*& value) noexcept {
    if (value == nullptr) return;
    value->~T();
    allocator.Deallocate(value, sizeof(T), alignof(T), tag);
    value = nullptr;
}

Base::Status InvalidBundleState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

} // namespace

struct SchemaBundle::Impl final {
    explicit Impl(Base::IAllocator& value) noexcept
        : allocator(&value) {}

    Base::IAllocator* allocator = nullptr;
    Core::MetadataDomain metadata;
    Core::MetadataRuntime* runtime = nullptr;
    Markup::Schema* schema = nullptr;
    Markup::DynamicResourceExtension* dynamicResource = nullptr;
    Markup::BindingExtension* binding = nullptr;
    Markup::PresentationObjectModel* presentation = nullptr;
    Markup::ResourceExtension resourceExtension;
    Markup::StaticExtension staticExtension;
    Markup::TypeExtension typeExtension;
    Markup::TemplateBindingExtension
        templateBindingExtension;
    bool prepared = false;
    bool frozen = false;
    bool terminal = false;

    ~Impl() noexcept {
        Destroy(*allocator, Base::MemoryTag::Markup, presentation);
        Destroy(*allocator, Base::MemoryTag::Markup, binding);
        Destroy(*allocator, Base::MemoryTag::Markup, dynamicResource);
        Destroy(*allocator, Base::MemoryTag::Markup, schema);
        Destroy(*allocator, Base::MemoryTag::Markup, runtime);
    }
};

SchemaBundle::SchemaBundle(Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    void* memory = allocator_->Allocate({
        sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
    }
    impl_ = new (memory) Impl(*allocator_);
}

SchemaBundle::~SchemaBundle() noexcept {
    if (impl_ == nullptr) return;
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
    impl_ = nullptr;
}

Base::Result<void> SchemaBundle::Prepare(
    const ModuleCatalog& modules) noexcept {
    if (impl_ == nullptr || impl_->terminal) {
        return InvalidBundleState("Schema bundle is terminal");
    }
    if (impl_->prepared) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Schema bundle metadata is already prepared");
    }
    Base::Result<void> status = modules.RegisterMetadata(impl_->metadata);
    if (status) status = impl_->metadata.Seal();
    if (!status) {
        impl_->terminal = true;
        return status.GetStatus();
    }
    Base::Result<Core::MetadataRuntime*> runtime = Create<Core::MetadataRuntime>(
        *impl_->allocator,
        Base::MemoryTag::Markup,
        impl_->metadata);
    if (!runtime) {
        impl_->terminal = true;
        return runtime.GetStatus();
    }
    impl_->runtime = runtime.Value();
    impl_->prepared = true;
    return {};
}

Base::Result<void> SchemaBundle::Finalize(
    const SchemaBundleServices& requested) noexcept {
    if (impl_ == nullptr || impl_->terminal || !impl_->prepared) {
        return InvalidBundleState(
            "Schema bundle metadata must be prepared before XAML finalization");
    }
    if (impl_->frozen) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Schema bundle is already frozen");
    }
    Base::IAllocator& programAllocator = requested.allocator != nullptr
        ? *requested.allocator
        : *impl_->allocator;

    Base::Result<Markup::Schema*> schema =
        Create<Markup::Schema>(
            *impl_->allocator,
            Base::MemoryTag::Markup,
            impl_->metadata,
            *impl_->runtime,
            impl_->allocator);
    if (!schema) {
        impl_->terminal = true;
        return schema.GetStatus();
    }
    impl_->schema = schema.Value();

    Base::Result<Markup::DynamicResourceExtension*> dynamicResource =
        Create<Markup::DynamicResourceExtension>(
            *impl_->allocator,
            Base::MemoryTag::Markup,
            Markup::DynamicResourceExtensionOptions{});
    if (!dynamicResource) {
        impl_->terminal = true;
        return dynamicResource.GetStatus();
    }
    impl_->dynamicResource = dynamicResource.Value();

    Base::Result<Markup::BindingExtension*> binding =
        Create<Markup::BindingExtension>(
            *impl_->allocator,
            Base::MemoryTag::Markup,
            Markup::BindingExtensionOptions{
                nullptr,
                Presentation::FrameworkElement::
                    DataContextProperty.Handle()});
    if (!binding) {
        impl_->terminal = true;
        return binding.GetStatus();
    }
    impl_->binding = binding.Value();

    Base::Result<Markup::PresentationObjectModel*> presentation =
        Create<Markup::PresentationObjectModel>(
            *impl_->allocator,
            Base::MemoryTag::Markup,
            Markup::PresentationObjectModelOptions{
                impl_->runtime,
                &Core::Detail::MetadataDomainAccess::
                    DependencyProperties(impl_->metadata),
                &programAllocator});
    if (!presentation) {
        impl_->terminal = true;
        return presentation.GetStatus();
    }
    impl_->presentation = presentation.Value();

    Base::Result<void> status =
        impl_->resourceExtension.Register(*impl_->schema);
    if (status) {
        status = impl_->dynamicResource->Register(
            *impl_->schema,
            Core::MakeTypeId(
                Base::StringView("urn:aero"),
                Base::StringView("DynamicResource")));
    }
    if (status) {
        status = impl_->binding->Register(
            *impl_->schema,
            Core::MakeTypeId(
                Base::StringView("urn:aero"),
                Base::StringView("Binding")));
    }
    if (status) {
        status = impl_->staticExtension.Register(
            *impl_->schema,
            Core::MakeTypeId(
                Markup::LanguageNamespaceUri(),
                Base::StringView("Static")));
    }
    if (status) {
        status = impl_->typeExtension.Register(
            *impl_->schema,
            Core::MakeTypeId(
                Markup::LanguageNamespaceUri(),
                Base::StringView("Type")));
    }
    if (status) {
        status =
            impl_->templateBindingExtension.Register(
                *impl_->schema,
                Core::MakeTypeId(
                    Core::AeroNamespaceUri(),
                    Base::StringView(
                        "TemplateBinding")));
    }
    if (status) {
        status = impl_->presentation->Register(
            *impl_->schema);
    }
    if (status) status = impl_->runtime->Freeze();
    if (status) status = impl_->schema->Freeze();
    if (!status) {
        impl_->terminal = true;
        return status.GetStatus();
    }
    impl_->frozen = true;
    return {};
}

bool SchemaBundle::IsPrepared() const noexcept {
    return impl_ != nullptr && impl_->prepared;
}

bool SchemaBundle::IsFrozen() const noexcept {
    return impl_ != nullptr && impl_->frozen;
}

Core::MetadataDomain& SchemaBundle::Metadata() noexcept {
    AERO_ASSERT(impl_ != nullptr && impl_->prepared);
    return impl_->metadata;
}

const Core::MetadataDomain& SchemaBundle::Metadata() const noexcept {
    AERO_ASSERT(impl_ != nullptr && impl_->prepared);
    return impl_->metadata;
}

Core::MetadataRuntime& SchemaBundle::Runtime() noexcept {
    AERO_ASSERT(impl_ != nullptr && impl_->runtime != nullptr);
    return *impl_->runtime;
}

const Core::MetadataRuntime& SchemaBundle::Runtime() const noexcept {
    AERO_ASSERT(impl_ != nullptr && impl_->runtime != nullptr);
    return *impl_->runtime;
}

Markup::Schema& SchemaBundle::Schema() noexcept {
    AERO_ASSERT(impl_ != nullptr && impl_->schema != nullptr);
    return *impl_->schema;
}

const Markup::Schema& SchemaBundle::Schema() const noexcept {
    AERO_ASSERT(impl_ != nullptr && impl_->schema != nullptr);
    return *impl_->schema;
}

} // namespace Aero
