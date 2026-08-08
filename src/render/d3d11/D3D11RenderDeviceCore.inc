D3D11RenderDevice::D3D11RenderDevice(
    const D3D11RenderDeviceOptions& options,
    Base::IAllocator* allocator) noexcept
    : Aero::RenderDevice::Access(
          allocator != nullptr ? *allocator : Base::GetDefaultAllocator()),
      options_(options),
      allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()) {}

D3D11RenderDevice::~D3D11RenderDevice() noexcept {
    Shutdown();
}

Base::Result<void> D3D11RenderDevice::Initialize() noexcept {
    if (state_ != nullptr && state_->initialized && AreResourcesReady()) {
        return {};
    }
    if (state_ == nullptr) {
        state_ = new (stateStorage_) D3D11RenderDeviceState(allocator_);
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

        state_->device = device;
        state_->context = context;
        state_->featureLevel = device->GetFeatureLevel();
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
                &state_->device,
                &state_->featureLevel,
                &state_->context);
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
                    &state_->device,
                    &state_->featureLevel,
                    &state_->context);
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
            ReleaseCom(state_->context);
            ReleaseCom(state_->device);
            result = createDevice(D3D_DRIVER_TYPE_WARP);
        }
        if (FAILED(result)) {
            state_->Reset();
            return StatusFromHResult(
                result,
                "D3D11CreateDevice failed",
                "D3D11 device creation reported device loss");
        }
    }

    if (state_->featureLevel < D3D_FEATURE_LEVEL_10_0) {
        state_->Reset();
        return Unsupported("D3D11 feature level 10_0 or newer is required");
    }

    D3D11_QUERY_DESC timestampQueryDescriptor{};
    timestampQueryDescriptor.Query = D3D11_QUERY_TIMESTAMP;
    ID3D11Query* timestampQuery = nullptr;
    const HRESULT timestampQueryResult = state_->device->CreateQuery(
        &timestampQueryDescriptor, &timestampQuery);
    if (IsDeviceRemoval(timestampQueryResult)) {
        state_->deviceLost = true;
        state_->Reset();
        return StatusFromHResult(
            timestampQueryResult,
            "D3D11 timestamp-query capability check failed",
            "D3D11 device was lost while querying timestamp support");
    }
    state_->supportsTimestampQueries = SUCCEEDED(timestampQueryResult);
    ReleaseCom(timestampQuery);

    state_->initialized = true;
    Base::Result<void> resources = InitializeResources();
    if (!resources) {
        Shutdown();
        return resources.GetStatus();
    }
    Base::Result<std::uint64_t> generation = AdvanceGeneration();
    if (!generation) {
        Shutdown();
        return generation.GetStatus();
    }
    deviceLost_ = false;
    return {};
}

FenceValue D3D11RenderDevice::LastSubmittedFence() const noexcept {
    return state_ != nullptr ? state_->lastSubmittedFence : 0U;
}

void D3D11RenderDevice::Shutdown() noexcept {
    ShutdownResources();
    if (state_ == nullptr) {
        return;
    }
    state_->~D3D11RenderDeviceState();
    state_ = nullptr;
}

void D3D11RenderDevice::MarkDeviceLost() noexcept {
    if (state_ != nullptr) {
        state_->deviceLost = true;
    }
    deviceLost_ = true;
}

bool D3D11RenderDevice::IsInitialized() const noexcept {
    return state_ != nullptr && state_->initialized;
}

Base::Result<FenceValue> D3D11RenderDevice::DrawBatch(
    ::Aero::Render::RenderBatch&& batch) noexcept {
    if (!IsInitialized() || !AreResourcesReady()) {
        return NotInitialized("D3D11 device is not initialized");
    }
    if (batch.Empty()) return FenceValue{0U};
    return SubmitBatch(batch);
}

void D3D11RenderDevice::NotifyDeviceLost() noexcept {
    if (deviceLost_) return;
    deviceLost_ = true;
    Shutdown();
}

Base::Result<void> D3D11RenderDevice::RestoreDevice() noexcept {
    if (!deviceLost_) {
        return InvalidState("D3D11 device is not lost");
    }
    return Initialize();
}

Base::Result<void> D3D11RenderDevice::WaitIdle(
    std::uint32_t timeoutMilliseconds) noexcept {
    const FenceValue fence = LastSubmittedFence();
    return fence != 0U
        ? WaitForFence(fence, timeoutMilliseconds)
        : Base::Result<void>();
}

::Aero::Render::BackendHealth
D3D11RenderDevice::GetDeviceHealth() const noexcept {
    if (deviceLost_ || IsDeviceLost()) {
        return ::Aero::Render::BackendHealth::DeviceLost;
    }
    return IsInitialized() && AreResourcesReady()
        ? ::Aero::Render::BackendHealth::Ready
        : ::Aero::Render::BackendHealth::Failed;
}

Base::Result<void> D3D11RenderDevice::ConfigureNativePipeline(
    ResourceHandle handle,
    ::Aero::Render::UiPipelineKey key) noexcept {
    return ConfigurePipeline(
        handle, ::Aero::Render::MakeD3D11UiPipeline(key));
}

std::uintptr_t D3D11RenderDevice::NativeDevice() const noexcept {
    return IsInitialized()
        ? reinterpret_cast<std::uintptr_t>(state_->device)
        : 0U;
}

std::uintptr_t D3D11RenderDevice::NativeImmediateContext() const noexcept {
    return IsInitialized()
        ? reinterpret_cast<std::uintptr_t>(state_->context)
        : 0U;
}

std::uint32_t D3D11RenderDevice::NativeFeatureLevel() const noexcept {
    return IsInitialized()
        ? static_cast<std::uint32_t>(state_->featureLevel)
        : 0U;
}

std::uint32_t D3D11RenderDevice::LiveResourceCount() const noexcept {
    return state_ != nullptr ? state_->resources.Size() : 0U;
}

DeviceCapabilities D3D11RenderDevice::Capabilities() const noexcept {
    DeviceCapabilities capabilities;
    if (!IsInitialized()) {
        capabilities.abiVersion = 0U;
        capabilities.maxFramesInFlight = 0U;
        capabilities.maxTextureDimension = 0U;
        return capabilities;
    }

    capabilities.maxFramesInFlight = 2U;
    capabilities.maxTextureDimension =
        state_->featureLevel >= D3D_FEATURE_LEVEL_11_0 ? 16384U : 8192U;
    capabilities.supportsTimestampQueries = state_->supportsTimestampQueries;
    return capabilities;
}

::Aero::Graphics::GraphicsCapabilities
D3D11RenderDevice::QueryGraphicsCapabilities() const noexcept {
    ::Aero::Graphics::GraphicsCapabilities capabilities;
    if (!IsInitialized()) {
        capabilities.abiVersion = 0U;
        capabilities.backendKind = NativeRenderBackendKind::Invalid;
        return capabilities;
    }

    capabilities.backendKind = NativeRenderBackendKind::D3D11;
    capabilities.features =
        FeatureBit(GraphicsFeature::TextureSampling) |
        FeatureBit(GraphicsFeature::RenderTargets) |
        FeatureBit(GraphicsFeature::VertexIndexBuffers) |
        FeatureBit(GraphicsFeature::UniformBuffers) |
        FeatureBit(GraphicsFeature::DepthStencil) |
        FeatureBit(GraphicsFeature::Instancing) |
        FeatureBit(GraphicsFeature::Scissor) |
        FeatureBit(GraphicsFeature::AnisotropicFiltering);
    if (state_->supportsTimestampQueries) {
        capabilities.features |= FeatureBit(GraphicsFeature::TimestampQueries);
    }
    capabilities.shaderLanguages = ShaderLanguageBit(ShaderLanguage::Dxbc);
    capabilities.maxColorAttachments = MaxColorAttachments;
    capabilities.maxVertexAttributes = MaxVertexAttributes;
    capabilities.maxSampledTextures = 16U;
    capabilities.uniformBufferAlignment = 16U;
    return capabilities;
}

Base::Result<void> D3D11RenderDevice::CreateResource(
    ResourceHandle handle,
    const ResourceDescriptor& descriptor) noexcept {
    if (state_ == nullptr) {
        return NotInitialized("D3D11 backend is not initialized");
    }
    Base::Result<void> ready = state_->VerifyReady();
    if (!ready) {
        return ready;
    }
    if (!handle.IsValid() || handle.type != descriptor.type) {
        return InvalidArgument("D3D11 resource handle or type is invalid");
    }
    if (state_->Find(handle) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "D3D11 resource handle already exists");
    }

    D3D11RenderDeviceState::ResourceRecord record;
    record.handle = handle;
    record.allocator = state_->allocator;
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
            void* uniformShadow = state_->allocator->Allocate({
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

        const HRESULT result = state_->device->CreateBuffer(
            &bufferDescriptor, nullptr, &record.buffer);
        if (FAILED(result)) {
            if (IsDeviceRemoval(result)) {
                state_->deviceLost = true;
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

    Base::Result<void> appended = state_->resources.PushBack(std::move(record));
    return appended ? Base::Result<void>() : Base::Result<void>(appended.GetStatus());
}

void D3D11RenderDevice::DestroyResource(ResourceHandle handle) noexcept {
    if (state_ == nullptr) {
        return;
    }
    const std::uint32_t index = state_->FindIndex(handle);
    if (index == UINT32_MAX) {
        return;
    }
    if (state_->currentPipeline == handle) {
        state_->currentPipeline = {};
    }
    state_->RemoveResourceAt(index);
}
