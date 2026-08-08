struct D3D11RenderContext::Impl {
    IDXGISwapChain* swapChain = nullptr;
    ID3D11Texture2D* activeBackBuffer = nullptr;
    WindowRenderContextDescriptor descriptor;
    std::uint64_t activeFrameSerial = 0U;
    bool ownsSwapChain = false;
    bool lost = false;
    bool deviceLost = false;
    bool initialized = false;

    ~Impl() noexcept {
        Reset();
    }

    void ReleaseActiveFrame() noexcept {
        ReleaseCom(activeBackBuffer);
        activeFrameSerial = 0U;
    }

    void ResetSwapChain() noexcept {
        ReleaseActiveFrame();
        ReleaseCom(swapChain);
        descriptor = {};
        ownsSwapChain = false;
        initialized = false;
    }

    void Reset() noexcept {
        ResetSwapChain();
        lost = false;
        deviceLost = false;
    }

    void MarkDeviceLost() noexcept {
        lost = true;
        deviceLost = true;
    }

    Base::Result<void> VerifyBackBuffer(
        ID3D11Device& device,
        const WindowRenderContextDescriptor& candidate) noexcept {
        if (swapChain == nullptr) {
            return NotInitialized("D3D11 swap chain is not available");
        }

        ID3D11Device* swapChainDevice = nullptr;
        HRESULT result = swapChain->GetDevice(
            __uuidof(ID3D11Device),
            reinterpret_cast<void**>(&swapChainDevice));
        if (FAILED(result)) {
            if (IsDeviceRemoval(result)) {
                MarkDeviceLost();
            }
            return StatusFromHResult(
                result,
                "Failed to query the D3D11 swap-chain device",
                "D3D11 device was lost while querying the swap chain");
        }
        const bool sameDevice = swapChainDevice == &device;
        ReleaseCom(swapChainDevice);
        if (!sameDevice) {
            return InvalidArgument(
                "D3D11 swap chain belongs to a different device");
        }

        DXGI_SWAP_CHAIN_DESC swapChainDescriptor{};
        result = swapChain->GetDesc(&swapChainDescriptor);
        if (FAILED(result)) {
            if (IsDeviceRemoval(result)) {
                MarkDeviceLost();
            }
            return StatusFromHResult(
                result,
                "Failed to query the D3D11 swap-chain descriptor",
                "D3D11 device was lost while querying the swap-chain descriptor");
        }
        if (!swapChainDescriptor.Windowed ||
            (candidate.d3d11.window != 0U &&
             swapChainDescriptor.OutputWindow != reinterpret_cast<HWND>(
                 candidate.d3d11.window))) {
            return InvalidArgument(
                "D3D11 swap chain is not attached to the requested window");
        }

        ID3D11Texture2D* backBuffer = nullptr;
        result = swapChain->GetBuffer(
            0U,
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(&backBuffer));
        if (FAILED(result)) {
            if (IsDeviceRemoval(result)) {
                MarkDeviceLost();
            }
            return StatusFromHResult(
                result,
                "Failed to acquire the D3D11 swap-chain backbuffer",
                "D3D11 device was lost while acquiring the swap-chain backbuffer");
        }

        D3D11_TEXTURE2D_DESC nativeDescriptor{};
        backBuffer->GetDesc(&nativeDescriptor);
        ReleaseCom(backBuffer);
        if (nativeDescriptor.Width != candidate.width ||
            nativeDescriptor.Height != candidate.height ||
            nativeDescriptor.Format != ToDxgiFormat(candidate.colorFormat) ||
            nativeDescriptor.SampleDesc.Count != candidate.sampleCount) {
            return InvalidArgument(
                "D3D11 swap-chain backbuffer does not match the surface descriptor");
        }
        return {};
    }

    Base::Result<void> Create(
        D3D11CommandQueue& graphics,
        const WindowRenderContextDescriptor& candidate) noexcept {
        if (!graphics.IsInitialized() || graphics.IsDeviceLost()) {
            return NotInitialized("D3D11 graphics backend is not ready");
        }
        if (initialized || swapChain != nullptr) {
            return InvalidState("D3D11 swap-chain surface is already initialized");
        }
        if (candidate.kind != WindowRenderContextKind::D3D11) {
            return Unsupported("D3D11 swap-chain surface requires D3D11Window kind");
        }
        if (candidate.sampleCount != 1U) {
            return Unsupported("Initial D3D11 swap-chain surfaces are single-sample");
        }
        if (candidate.presentMode == PresentMode::Mailbox) {
            return Unsupported("Legacy D3D11 swap chains do not support mailbox presentation");
        }
        if (candidate.colorFormat != GraphicsTextureFormat::Rgba8Unorm &&
            candidate.colorFormat != GraphicsTextureFormat::Bgra8Unorm) {
            return Unsupported("D3D11 swap-chain color format is unsupported");
        }
        if (candidate.d3d11.device != graphics.NativeDevice() ||
            candidate.d3d11.immediateContext !=
                graphics.NativeImmediateContext()) {
            return InvalidArgument(
                "D3D11 surface device/context do not match the graphics backend");
        }

        auto* device = reinterpret_cast<ID3D11Device*>(graphics.NativeDevice());
        HRESULT result = S_OK;
        IDXGISwapChain* createdSwapChain = nullptr;
        bool createdInternally = false;

        if (candidate.d3d11.swapChain != 0U) {
            createdSwapChain = reinterpret_cast<IDXGISwapChain*>(
                candidate.d3d11.swapChain);
            createdSwapChain->AddRef();
        } else {
            if (candidate.d3d11.window == 0U) {
                return InvalidArgument(
                    "D3D11 owned swap-chain creation requires a window handle");
            }

            IDXGIDevice* dxgiDevice = nullptr;
            IDXGIAdapter* adapter = nullptr;
            IDXGIFactory* factory = nullptr;
            result = device->QueryInterface(
                __uuidof(IDXGIDevice),
                reinterpret_cast<void**>(&dxgiDevice));
            if (SUCCEEDED(result)) {
                result = dxgiDevice->GetAdapter(&adapter);
            }
            if (SUCCEEDED(result)) {
                result = adapter->GetParent(
                    __uuidof(IDXGIFactory),
                    reinterpret_cast<void**>(&factory));
            }

            if (SUCCEEDED(result)) {
                DXGI_SWAP_CHAIN_DESC nativeDescriptor{};
                nativeDescriptor.BufferDesc.Width = candidate.width;
                nativeDescriptor.BufferDesc.Height = candidate.height;
                nativeDescriptor.BufferDesc.RefreshRate.Numerator = 0U;
                nativeDescriptor.BufferDesc.RefreshRate.Denominator = 1U;
                nativeDescriptor.BufferDesc.Format = ToDxgiFormat(
                    candidate.colorFormat);
                nativeDescriptor.BufferDesc.ScanlineOrdering =
                    DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
                nativeDescriptor.BufferDesc.Scaling =
                    DXGI_MODE_SCALING_UNSPECIFIED;
                nativeDescriptor.SampleDesc.Count = 1U;
                nativeDescriptor.SampleDesc.Quality = 0U;
                nativeDescriptor.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                nativeDescriptor.BufferCount = 2U;
                nativeDescriptor.OutputWindow = reinterpret_cast<HWND>(
                    candidate.d3d11.window);
                nativeDescriptor.Windowed = TRUE;
                nativeDescriptor.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
                nativeDescriptor.Flags = 0U;
                result = factory->CreateSwapChain(
                    device,
                    &nativeDescriptor,
                    &createdSwapChain);
                if (SUCCEEDED(result)) {
                    static_cast<void>(factory->MakeWindowAssociation(
                        nativeDescriptor.OutputWindow,
                        DXGI_MWA_NO_ALT_ENTER));
                }
            }

            ReleaseCom(factory);
            ReleaseCom(adapter);
            ReleaseCom(dxgiDevice);
            if (SUCCEEDED(result)) {
                createdInternally = true;
            }
        }

        if (FAILED(result) || createdSwapChain == nullptr) {
            ReleaseCom(createdSwapChain);
            if (IsDeviceRemoval(result)) {
                MarkDeviceLost();
            }
            return StatusFromHResult(
                result,
                "Failed to create or borrow a D3D11 swap chain",
                "D3D11 device was lost while creating the swap chain");
        }

        swapChain = createdSwapChain;
        ownsSwapChain = createdInternally;
        Base::Result<void> verified = VerifyBackBuffer(*device, candidate);
        if (!verified) {
            ResetSwapChain();
            return verified;
        }

        descriptor = candidate;
        descriptor.d3d11.swapChain = reinterpret_cast<std::uintptr_t>(swapChain);
        initialized = true;
        lost = false;
        return {};
    }
};

FenceValue D3D11CommandQueue::LastSubmittedFence() const noexcept {
    return impl_ != nullptr ? impl_->lastSubmittedFence : 0U;
}

D3D11RenderContext::D3D11RenderContext(
    D3D11CommandQueue& graphics,
    Base::IAllocator* allocator) noexcept
    : graphics_(&graphics),
      allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()) {}

D3D11RenderContext::~D3D11RenderContext() noexcept {
    Shutdown();
}

std::uintptr_t D3D11RenderContext::NativeSwapChain() const noexcept {
    return impl_ != nullptr && impl_->swapChain != nullptr
        ? reinterpret_cast<std::uintptr_t>(impl_->swapChain)
        : 0U;
}

bool D3D11RenderContext::OwnsSwapChain() const noexcept {
    return impl_ != nullptr && impl_->ownsSwapChain;
}

WindowRenderContextCaps
D3D11RenderContext::Caps() const noexcept {
    WindowRenderContextCaps capabilities;
    capabilities.supportedKinds = WindowRenderContextKindBit(WindowRenderContextKind::D3D11);
    capabilities.supportsResize = true;
    capabilities.supportsPresent = true;
    capabilities.supportsContextLossRecovery = true;
    if (graphics_ != nullptr && graphics_->IsInitialized()) {
        capabilities.maxWidth = graphics_->Capabilities().maxTextureDimension;
        capabilities.maxHeight = graphics_->Capabilities().maxTextureDimension;
    }
    return capabilities;
}

Base::Result<void> D3D11RenderContext::Create(
    const WindowRenderContextDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidateWindowRenderContextDescriptor(
        descriptor,
        Caps());
    if (!valid) {
        return valid;
    }
    if (impl_ == nullptr) {
        void* memory = allocator_->Allocate({
            sizeof(Impl), alignof(Impl), Base::MemoryTag::Render});
        if (memory == nullptr) {
            return OutOfMemory("Failed to allocate D3D11 swap-chain state");
        }
        impl_ = new (memory) Impl();
    }
    Base::Result<void> created = impl_->Create(*graphics_, descriptor);
    if (!created && impl_->deviceLost) {
        graphics_->MarkDeviceLost();
    }
    return created;
}

void D3D11RenderContext::Shutdown() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Render);
    impl_ = nullptr;
}

Base::Result<void> D3D11RenderContext::Resize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    if (impl_ == nullptr || !impl_->initialized || impl_->swapChain == nullptr) {
        return NotInitialized("D3D11 swap-chain surface is not initialized");
    }
    if (IsLost()) {
        return InvalidState("D3D11 swap-chain surface is lost");
    }
    if (impl_->activeFrameSerial != 0U) {
        return InvalidState("Cannot resize a D3D11 swap chain with an active frame");
    }
    const WindowRenderContextCaps capabilities = Caps();
    if (width == 0U || height == 0U ||
        width > capabilities.maxWidth || height > capabilities.maxHeight) {
        return InvalidArgument("D3D11 swap-chain resize dimensions are invalid");
    }

    impl_->ReleaseActiveFrame();
    const HRESULT result = impl_->swapChain->ResizeBuffers(
        0U,
        width,
        height,
        DXGI_FORMAT_UNKNOWN,
        0U);
    if (FAILED(result)) {
        if (IsDeviceRemoval(result)) {
            impl_->MarkDeviceLost();
            graphics_->MarkDeviceLost();
        }
        return StatusFromHResult(
            result,
            "IDXGISwapChain::ResizeBuffers failed",
            "D3D11 device was lost while resizing the swap chain");
    }

    WindowRenderContextDescriptor candidate = impl_->descriptor;
    candidate.width = width;
    candidate.height = height;
    auto* device = reinterpret_cast<ID3D11Device*>(graphics_->NativeDevice());
    Base::Result<void> verified = impl_->VerifyBackBuffer(*device, candidate);
    if (!verified) {
        impl_->lost = true;
        if (impl_->deviceLost) {
            graphics_->MarkDeviceLost();
        }
        return verified;
    }
    impl_->descriptor = candidate;
    return {};
}

Base::Result<RenderTargetBinding>
D3D11RenderContext::AcquireTarget(
    std::uint64_t frameSerial) noexcept {
    if (impl_ == nullptr || !impl_->initialized || impl_->swapChain == nullptr) {
        return NotInitialized("D3D11 swap-chain surface is not initialized");
    }
    if (IsLost()) {
        return InvalidState("D3D11 swap-chain surface is lost");
    }
    if (frameSerial == 0U || impl_->activeFrameSerial != 0U) {
        return InvalidState("D3D11 swap-chain frame acquisition is invalid");
    }

    ID3D11Texture2D* backBuffer = nullptr;
    const HRESULT result = impl_->swapChain->GetBuffer(
        0U,
        __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(&backBuffer));
    if (FAILED(result)) {
        if (IsDeviceRemoval(result)) {
            impl_->MarkDeviceLost();
            graphics_->MarkDeviceLost();
        }
        return StatusFromHResult(
            result,
            "Failed to acquire the D3D11 swap-chain backbuffer",
            "D3D11 device was lost while acquiring the swap-chain backbuffer");
    }

    D3D11_TEXTURE2D_DESC nativeDescriptor{};
    backBuffer->GetDesc(&nativeDescriptor);
    if (nativeDescriptor.Width != impl_->descriptor.width ||
        nativeDescriptor.Height != impl_->descriptor.height ||
        nativeDescriptor.Format != ToDxgiFormat(impl_->descriptor.colorFormat)) {
        ReleaseCom(backBuffer);
        return InvalidState(
            "D3D11 swap-chain backbuffer changed without a surface resize");
    }

    impl_->activeBackBuffer = backBuffer;
    impl_->activeFrameSerial = frameSerial;

    RenderTargetBinding target;
    target.colorTarget = reinterpret_cast<std::uintptr_t>(backBuffer);
    target.width = nativeDescriptor.Width;
    target.height = nativeDescriptor.Height;
    target.colorFormat = impl_->descriptor.colorFormat;
    target.depthStencilFormat = impl_->descriptor.depthStencilFormat;
    target.sampleCount = static_cast<std::uint8_t>(
        nativeDescriptor.SampleDesc.Count);
    target.defaultFramebuffer = false;
    target.stableId = impl_->descriptor.stableId;
    return target;
}

Base::Result<void> D3D11RenderContext::Present(
    std::uint64_t frameSerial,
    FenceValue signalFence) noexcept {
    if (impl_ == nullptr || !impl_->initialized || impl_->swapChain == nullptr) {
        return NotInitialized("D3D11 swap-chain surface is not initialized");
    }
    if (IsLost()) {
        return InvalidState("D3D11 swap-chain surface is lost");
    }
    if (frameSerial == 0U || frameSerial != impl_->activeFrameSerial ||
        impl_->activeBackBuffer == nullptr || signalFence == 0U) {
        return InvalidState("D3D11 swap-chain present frame is invalid");
    }

    const UINT syncInterval =
        impl_->descriptor.presentMode == PresentMode::Fifo ? 1U : 0U;
    const HRESULT result = impl_->swapChain->Present(syncInterval, 0U);
    impl_->ReleaseActiveFrame();
    if (FAILED(result)) {
        if (IsDeviceRemoval(result)) {
            impl_->MarkDeviceLost();
            graphics_->MarkDeviceLost();
        }
        return StatusFromHResult(
            result,
            "IDXGISwapChain::Present failed",
            "D3D11 device was lost while presenting the swap chain");
    }
    return {};
}

void D3D11RenderContext::DiscardFrame(
    std::uint64_t frameSerial) noexcept {
    if (impl_ != nullptr && frameSerial != 0U &&
        frameSerial == impl_->activeFrameSerial) {
        impl_->ReleaseActiveFrame();
    }
}

void D3D11RenderContext::NotifyLost() noexcept {
    if (impl_ != nullptr) {
        impl_->ReleaseActiveFrame();
        impl_->lost = true;
    }
}

Base::Result<void> D3D11RenderContext::Restore(
    const WindowRenderContextDescriptor& descriptor) noexcept {
    if (impl_ == nullptr || !impl_->lost) {
        return InvalidState("D3D11 swap-chain surface is not in the lost state");
    }
    if (graphics_ == nullptr || graphics_->IsDeviceLost()) {
        return InvalidState("Cannot restore a D3D11 surface after device loss");
    }

    impl_->ResetSwapChain();
    impl_->lost = true;
    Base::Result<void> restored = impl_->Create(*graphics_, descriptor);
    if (!restored) {
        impl_->lost = true;
        return restored;
    }
    return {};
}

bool D3D11RenderContext::IsLost() const noexcept {
    if (graphics_ == nullptr || graphics_->IsDeviceLost()) {
        return true;
    }
    return impl_ != nullptr && impl_->lost;
}
