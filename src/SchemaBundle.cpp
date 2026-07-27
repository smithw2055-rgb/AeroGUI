#include <Aero/SchemaBundle.hpp>

#include <Aero/Base/Assert.hpp>
#include <Aero/Markup/Extensions/XamlDynamicResource.hpp>
#include <Aero/Markup/Resources/XamlPresentationObjectModel.hpp>
#include <Aero/Markup/Resources/XamlResources.hpp>
#include <Aero/Markup/Runtime/XamlContentWriter.hpp>
#include <Aero/Markup/Schema/XamlRegistrationContext.hpp>
#include <Aero/Markup/Schema/XamlSchemaContext.hpp>

#include <new>
#include <utility>

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
    Markup::XamlSchemaContext* schema = nullptr;
    Core::ActivationProviderRegistry* activation = nullptr;
    Markup::XamlDynamicResourceExtension* dynamicResource = nullptr;
    Markup::XamlPresentationObjectModel* presentation = nullptr;
    Markup::XamlContentWriter* contentWriter = nullptr;
    Markup::XamlResourceExtension resourceExtension;
    bool prepared = false;
    bool frozen = false;
    bool terminal = false;

    ~Impl() noexcept {
        Destroy(*allocator, Base::MemoryTag::Markup, contentWriter);
        Destroy(*allocator, Base::MemoryTag::Markup, presentation);
        Destroy(*allocator, Base::MemoryTag::Markup, dynamicResource);
        Destroy(*allocator, Base::MemoryTag::Markup, activation);
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
    const ModuleCatalog& modules,
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

    Base::Result<Markup::XamlSchemaContext*> schema =
        Create<Markup::XamlSchemaContext>(
            *impl_->allocator,
            Base::MemoryTag::Markup,
            impl_->metadata,
            *impl_->runtime);
    if (!schema) {
        impl_->terminal = true;
        return schema.GetStatus();
    }
    impl_->schema = schema.Value();

    Base::Result<Core::ActivationProviderRegistry*> activation =
        Create<Core::ActivationProviderRegistry>(
            *impl_->allocator,
            Base::MemoryTag::Markup,
            impl_->runtime->Descriptors());
    if (!activation) {
        impl_->terminal = true;
        return activation.GetStatus();
    }
    impl_->activation = activation.Value();

    Base::Result<Markup::XamlDynamicResourceExtension*> dynamicResource =
        Create<Markup::XamlDynamicResourceExtension>(
            *impl_->allocator,
            Base::MemoryTag::Markup,
            Markup::XamlDynamicResourceExtensionOptions{});
    if (!dynamicResource) {
        impl_->terminal = true;
        return dynamicResource.GetStatus();
    }
    impl_->dynamicResource = dynamicResource.Value();

    Base::Result<Markup::XamlPresentationObjectModel*> presentation =
        Create<Markup::XamlPresentationObjectModel>(
            *impl_->allocator,
            Base::MemoryTag::Markup,
            Markup::XamlPresentationObjectModelOptions{
                impl_->runtime,
                &impl_->metadata.DependencyProperties(),
                Core::InvalidTypeId,
                &programAllocator});
    if (!presentation) {
        impl_->terminal = true;
        return presentation.GetStatus();
    }
    impl_->presentation = presentation.Value();

    Base::Result<Markup::XamlContentWriter*> contentWriter =
        Create<Markup::XamlContentWriter>(
            *impl_->allocator,
            Base::MemoryTag::Markup);
    if (!contentWriter) {
        impl_->terminal = true;
        return contentWriter.GetStatus();
    }
    impl_->contentWriter = contentWriter.Value();

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
        status = impl_->presentation->Register(
            *impl_->schema,
            *impl_->activation);
    }
    if (status) {
        status = impl_->contentWriter->Register(*impl_->schema);
    }
    if (status) {
        Markup::XamlRegistrationContext context(
            *impl_->schema,
            *impl_->activation,
            *impl_->runtime,
            impl_->metadata.DependencyProperties(),
            programAllocator);
        status = modules.RegisterXaml(context);
    }
    if (status) status = impl_->runtime->Freeze();
    if (status) status = impl_->schema->Freeze();
    if (status) status = impl_->activation->Freeze();
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

Markup::XamlSchemaContext& SchemaBundle::XamlSchema() noexcept {
    AERO_ASSERT(impl_ != nullptr && impl_->schema != nullptr);
    return *impl_->schema;
}

const Markup::XamlSchemaContext& SchemaBundle::XamlSchema() const noexcept {
    AERO_ASSERT(impl_ != nullptr && impl_->schema != nullptr);
    return *impl_->schema;
}

Core::ActivationProviderRegistry& SchemaBundle::ActivationFacets() noexcept {
    AERO_ASSERT(impl_ != nullptr && impl_->activation != nullptr);
    return *impl_->activation;
}

const Core::ActivationProviderRegistry&
SchemaBundle::ActivationFacets() const noexcept {
    AERO_ASSERT(impl_ != nullptr && impl_->activation != nullptr);
    return *impl_->activation;
}

} // namespace Aero
