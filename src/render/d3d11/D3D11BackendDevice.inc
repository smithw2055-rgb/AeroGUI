D3D11GraphicsBackend::D3D11GraphicsBackend(
    const D3D11BackendOptions& options,
    Base::IAllocator* allocator) noexcept
    : options_(options),
      allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()) {}

D3D11GraphicsBackend::~D3D11GraphicsBackend() noexcept {
    Shutdown();
}

Base::Result<void> D3D11GraphicsBackend::Initialize() noexcept {
    if (impl_ != nullptr && impl_->initialized) {
        return {};
    }
    if (impl_ == nullptr) {
        void* memory = allocator_->Allocate({
            sizeof(Impl), alignof(Impl), Base::MemoryTag::Render});
        if (memory == nullptr) {
            return OutOfMemory("Failed to allocate D3D11 backend state");
        }
        impl_ = new (memory) Impl(allocator_);
    }

    if (options_.deviceMode == D3D11DeviceMode::Borrowed) {
        if (options_.borrowedDevice == 0U ||
            options_.borrowedImmediateContext == 0U) {
            return InvalidArgument(
                "Borrowed D3D11 mode requires device and immediate-context handles");
        }

        auto* device = reinterpret_cast<ID3D11Device*>(options_.borrowedDevice);
        auto* context = reinterpret_cast<ID3D11DeviceContext*>(
            options_.borrowedImmediateContext);
        device->AddRef();
        context->AddRef();

        ID3D11Device* contextDevice = nullptr;
        context->GetDevice(&contextDevice);
        const bool matches = contextDevice == device;
        ReleaseCom(contextDevice);
        if (!matches) {
            ReleaseCom(context);
            ReleaseCom(device);
            return InvalidArgument(
                "Borrowed D3D11 context does not belong to the supplied device");
        }

        impl_->device = device;
        impl_->context = context;
        impl_->featureLevel = device->GetFeatureLevel();
    } else {
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        if (options_.enableDebugLayer) {
            flags |= D3D11_CREATE_DEVICE_DEBUG;
        }

        const D3D_FEATURE_LEVEL levelsWith11_1[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0};
        const D3D_FEATURE_LEVEL levelsWithout11_1[] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0};

        auto createDevice = [&](D3D_DRIVER_TYPE driverType) noexcept -> HRESULT {
            HRESULT result = D3D11CreateDevice(
                nullptr,
                driverType,
                nullptr,
                flags,
                levelsWith11_1,
                static_cast<UINT>(sizeof(levelsWith11_1) / sizeof(levelsWith11_1[0])),
                D3D11_SDK_VERSION,
                &impl_->device,
                &impl_->featureLevel,
                &impl_->context);
            if (result == E_INVALIDARG) {
                result = D3D11CreateDevice(
                    nullptr,
                    driverType,
                    nullptr,
                    flags,
                    levelsWithout11_1,
                    static_cast<UINT>(
                        sizeof(levelsWithout11_1) / sizeof(levelsWithout11_1[0])),
                    D3D11_SDK_VERSION,
                    &impl_->device,
                    &impl_->featureLevel,
                    &impl_->context);
            }
            return result;
        };

        const D3D_DRIVER_TYPE requestedDriver =
            options_.deviceMode == D3D11DeviceMode::Warp
            ? D3D_DRIVER_TYPE_WARP
            : D3D_DRIVER_TYPE_HARDWARE;
        HRESULT result = createDevice(requestedDriver);
        if (FAILED(result) &&
            requestedDriver == D3D_DRIVER_TYPE_HARDWARE &&
            options_.allowWarpFallback) {
            ReleaseCom(impl_->context);
            ReleaseCom(impl_->device);
            result = createDevice(D3D_DRIVER_TYPE_WARP);
        }
        if (FAILED(result)) {
            impl_->Reset();
            return StatusFromHResult(
                result,
                "D3D11CreateDevice failed",
                "D3D11 device creation reported device loss");
        }
    }

    if (impl_->featureLevel < D3D_FEATURE_LEVEL_10_0) {
        impl_->Reset();
        return Unsupported("D3D11 feature level 10_0 or newer is required");
    }

    D3D11_QUERY_DESC timestampQueryDescriptor{};
    timestampQueryDescriptor.Query = D3D11_QUERY_TIMESTAMP;
    ID3D11Query* timestampQuery = nullptr;
    const HRESULT timestampQueryResult = impl_->device->CreateQuery(
        &timestampQueryDescriptor, &timestampQuery);
    if (IsDeviceRemoval(timestampQueryResult)) {
        impl_->deviceLost = true;
        impl_->Reset();
        return StatusFromHResult(
            timestampQueryResult,
            "D3D11 timestamp-query capability check failed",
            "D3D11 device was lost while querying timestamp support");
    }
    impl_->supportsTimestampQueries = SUCCEEDED(timestampQueryResult);
    ReleaseCom(timestampQuery);

    impl_->initialized = true;
    return {};
}

void D3D11GraphicsBackend::Shutdown() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Render);
    impl_ = nullptr;
}

void D3D11GraphicsBackend::MarkDeviceLost() noexcept {
    if (impl_ != nullptr) {
        impl_->deviceLost = true;
    }
}

bool D3D11GraphicsBackend::IsInitialized() const noexcept {
    return impl_ != nullptr && impl_->initialized;
}

std::uintptr_t D3D11GraphicsBackend::NativeDevice() const noexcept {
    return IsInitialized()
        ? reinterpret_cast<std::uintptr_t>(impl_->device)
        : 0U;
}

std::uintptr_t D3D11GraphicsBackend::NativeImmediateContext() const noexcept {
    return IsInitialized()
        ? reinterpret_cast<std::uintptr_t>(impl_->context)
        : 0U;
}

std::uint32_t D3D11GraphicsBackend::NativeFeatureLevel() const noexcept {
    return IsInitialized()
        ? static_cast<std::uint32_t>(impl_->featureLevel)
        : 0U;
}

std::uint32_t D3D11GraphicsBackend::LiveResourceCount() const noexcept {
    return impl_ != nullptr ? impl_->resources.Size() : 0U;
}

DeviceCapabilities D3D11GraphicsBackend::Capabilities() const noexcept {
    DeviceCapabilities capabilities;
    if (!IsInitialized()) {
        capabilities.abiVersion = 0U;
        capabilities.maxFramesInFlight = 0U;
        capabilities.maxTextureDimension = 0U;
        return capabilities;
    }

    capabilities.maxFramesInFlight = 2U;
    capabilities.maxTextureDimension =
        impl_->featureLevel >= D3D_FEATURE_LEVEL_11_0 ? 16384U : 8192U;
    capabilities.supportsTimestampQueries = impl_->supportsTimestampQueries;
    return capabilities;
}

GraphicsCapabilities
D3D11GraphicsBackend::QueryGraphicsCapabilities() const noexcept {
    GraphicsCapabilities capabilities;
    if (!IsInitialized()) {
        capabilities.abiVersion = 0U;
        capabilities.backendKind = GraphicsBackendKind::Invalid;
        return capabilities;
    }

    capabilities.backendKind = GraphicsBackendKind::D3D11;
    capabilities.features =
        FeatureBit(GraphicsFeature::TextureSampling) |
        FeatureBit(GraphicsFeature::RenderTargets) |
        FeatureBit(GraphicsFeature::VertexIndexBuffers) |
        FeatureBit(GraphicsFeature::UniformBuffers) |
        FeatureBit(GraphicsFeature::DepthStencil) |
        FeatureBit(GraphicsFeature::Instancing) |
        FeatureBit(GraphicsFeature::Scissor) |
        FeatureBit(GraphicsFeature::AnisotropicFiltering);
    if (impl_->supportsTimestampQueries) {
        capabilities.features |= FeatureBit(GraphicsFeature::TimestampQueries);
    }
    capabilities.shaderLanguages = ShaderLanguageBit(ShaderLanguage::Dxbc);
    capabilities.maxColorAttachments = MaxColorAttachments;
    capabilities.maxVertexAttributes = MaxVertexAttributes;
    capabilities.maxSampledTextures = 16U;
    capabilities.uniformBufferAlignment = 16U;
    return capabilities;
}

Base::Result<void> D3D11GraphicsBackend::CreateResource(
    ResourceHandle handle,
    const ResourceDescriptor& descriptor) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized("D3D11 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    if (!handle.IsValid() || handle.type != descriptor.type) {
        return InvalidArgument("D3D11 resource handle or type is invalid");
    }
    if (impl_->Find(handle) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "D3D11 resource handle already exists");
    }

    Impl::ResourceRecord record;
    record.handle = handle;
    record.allocator = impl_->allocator;
    record.baseDescriptor = descriptor;

    if (descriptor.type == ResourceType::Buffer) {
        UINT byteWidth = 0U;
        if (descriptor.buffer.sizeBytes == 0U ||
            !CheckedUint(descriptor.buffer.sizeBytes, byteWidth)) {
            return InvalidArgument("D3D11 buffer size is invalid");
        }

        D3D11_BUFFER_DESC bufferDescriptor{};
        bufferDescriptor.ByteWidth = byteWidth;
        switch (descriptor.buffer.usage) {
        case BufferUsage::Vertex:
            bufferDescriptor.Usage = D3D11_USAGE_DEFAULT;
            bufferDescriptor.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            break;
        case BufferUsage::Index:
            bufferDescriptor.Usage = D3D11_USAGE_DEFAULT;
            bufferDescriptor.BindFlags = D3D11_BIND_INDEX_BUFFER;
            break;
        case BufferUsage::Uniform: {
            if ((byteWidth & 15U) != 0U) {
                return InvalidArgument(
                    "D3D11 uniform-buffer size must be a multiple of 16 bytes");
            }
            bufferDescriptor.Usage = D3D11_USAGE_DEFAULT;
            bufferDescriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            void* uniformShadow = impl_->allocator->Allocate({
                byteWidth, alignof(std::uint8_t), Base::MemoryTag::Render});
            if (uniformShadow == nullptr) {
                return OutOfMemory("Failed to allocate D3D11 uniform-buffer shadow");
            }
            record.uniformShadow = static_cast<std::uint8_t*>(uniformShadow);
            record.uniformShadowSize = byteWidth;
            std::memset(record.uniformShadow, 0, byteWidth);
            break;
        }
        case BufferUsage::Upload:
            bufferDescriptor.Usage = D3D11_USAGE_STAGING;
            bufferDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            break;
        }

        const HRESULT result = impl_->device->CreateBuffer(
            &bufferDescriptor, nullptr, &record.buffer);
        if (FAILED(result)) {
            if (IsDeviceRemoval(result)) {
                impl_->deviceLost = true;
            }
            return StatusFromHResult(
                result,
                "ID3D11Device::CreateBuffer failed",
                "D3D11 device was lost while creating a buffer");
        }
        record.configured = true;
    } else if (descriptor.type != ResourceType::Texture &&
               descriptor.type != ResourceType::RenderTarget &&
               descriptor.type != ResourceType::Sampler &&
               descriptor.type != ResourceType::Pipeline) {
        return InvalidArgument("D3D11 resource type is unsupported");
    }

    Base::Result<void> appended = impl_->resources.PushBack(std::move(record));
    return appended ? Base::Result<void>() : Base::Result<void>(appended.GetStatus());
}

void D3D11GraphicsBackend::DestroyResource(ResourceHandle handle) noexcept {
    if (impl_ == nullptr) {
        return;
    }
    const std::uint32_t index = impl_->FindIndex(handle);
    if (index == UINT32_MAX) {
        return;
    }
    if (impl_->currentPipeline == handle) {
        impl_->currentPipeline = {};
    }
    impl_->RemoveResourceAt(index);
}
