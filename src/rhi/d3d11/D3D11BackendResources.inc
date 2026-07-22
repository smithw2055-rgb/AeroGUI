Base::Result<void> D3D11GraphicsBackend::ConfigureTexture(
    ResourceHandle handle,
    const TextureResourceDescriptor& descriptor) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized("D3D11 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    Base::Result<void> valid = ValidateTextureDescriptor(
        descriptor, QueryGraphicsCapabilities());
    if (!valid) {
        return valid;
    }
    if (descriptor.sampleCount != 1U) {
        return Unsupported("Initial D3D11 backend supports single-sample textures only");
    }

    Impl::ResourceRecord* record = impl_->Find(handle);
    if (record == nullptr ||
        (handle.type != ResourceType::Texture &&
         handle.type != ResourceType::RenderTarget)) {
        return NotFound("D3D11 texture resource was not found");
    }
    if (record->external) {
        return InvalidState("External D3D11 render targets cannot be reconfigured");
    }
    if (record->baseDescriptor.texture.width != descriptor.width ||
        record->baseDescriptor.texture.height != descriptor.height) {
        return InvalidArgument(
            "D3D11 rich texture dimensions do not match the base resource");
    }
    if (handle.type == ResourceType::RenderTarget &&
        !HasTextureUsage(descriptor.usage, TextureUsage::RenderTarget)) {
        return InvalidArgument(
            "D3D11 render-target resource requires RenderTarget usage");
    }

    D3D11_TEXTURE2D_DESC nativeDescriptor{};
    nativeDescriptor.Width = descriptor.width;
    nativeDescriptor.Height = descriptor.height;
    nativeDescriptor.MipLevels = descriptor.mipLevels;
    nativeDescriptor.ArraySize = descriptor.arrayLayers;
    nativeDescriptor.Format = ToDxgiFormat(descriptor.format);
    nativeDescriptor.SampleDesc.Count = descriptor.sampleCount;
    nativeDescriptor.Usage = D3D11_USAGE_DEFAULT;

    const bool depth =
        descriptor.format == GraphicsTextureFormat::Depth24Stencil8;
    if (HasTextureUsage(descriptor.usage, TextureUsage::Sampled)) {
        nativeDescriptor.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }
    if (HasTextureUsage(descriptor.usage, TextureUsage::RenderTarget)) {
        nativeDescriptor.BindFlags |= depth
            ? D3D11_BIND_DEPTH_STENCIL
            : D3D11_BIND_RENDER_TARGET;
    }

    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* shaderResourceView = nullptr;
    ID3D11RenderTargetView* renderTargetView = nullptr;
    ID3D11DepthStencilView* depthStencilView = nullptr;

    HRESULT result = impl_->device->CreateTexture2D(
        &nativeDescriptor, nullptr, &texture);
    if (SUCCEEDED(result) &&
        HasTextureUsage(descriptor.usage, TextureUsage::Sampled)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format = nativeDescriptor.Format;
        if (descriptor.arrayLayers > 1U) {
            view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            view.Texture2DArray.MostDetailedMip = 0U;
            view.Texture2DArray.MipLevels = descriptor.mipLevels;
            view.Texture2DArray.FirstArraySlice = 0U;
            view.Texture2DArray.ArraySize = descriptor.arrayLayers;
        } else {
            view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            view.Texture2D.MostDetailedMip = 0U;
            view.Texture2D.MipLevels = descriptor.mipLevels;
        }
        result = impl_->device->CreateShaderResourceView(
            texture, &view, &shaderResourceView);
    }
    if (SUCCEEDED(result) &&
        HasTextureUsage(descriptor.usage, TextureUsage::RenderTarget)) {
        if (depth) {
            result = impl_->device->CreateDepthStencilView(
                texture, nullptr, &depthStencilView);
        } else {
            result = impl_->device->CreateRenderTargetView(
                texture, nullptr, &renderTargetView);
        }
    }
    if (FAILED(result)) {
        ReleaseCom(depthStencilView);
        ReleaseCom(renderTargetView);
        ReleaseCom(shaderResourceView);
        ReleaseCom(texture);
        if (IsDeviceRemoval(result)) {
            impl_->deviceLost = true;
        }
        return StatusFromHResult(
            result,
            "D3D11 texture or view creation failed",
            "D3D11 device was lost while creating a texture");
    }

    record->ReleaseTextureObjects();
    record->texture = texture;
    record->shaderResourceView = shaderResourceView;
    record->renderTargetView = renderTargetView;
    record->depthStencilView = depthStencilView;
    record->textureDescriptor = descriptor;
    record->configured = true;
    return {};
}

Base::Result<void> D3D11GraphicsBackend::ConfigureSampler(
    ResourceHandle handle,
    const SamplerDescriptor& descriptor) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized("D3D11 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    Base::Result<void> valid = ValidateSamplerDescriptor(
        descriptor, QueryGraphicsCapabilities());
    if (!valid) {
        return valid;
    }

    Impl::ResourceRecord* record = impl_->Find(handle);
    if (record == nullptr || handle.type != ResourceType::Sampler) {
        return NotFound("D3D11 sampler resource was not found");
    }

    D3D11_SAMPLER_DESC nativeDescriptor{};
    nativeDescriptor.Filter = ToD3DFilter(descriptor);
    nativeDescriptor.AddressU = ToD3DAddress(descriptor.addressU);
    nativeDescriptor.AddressV = ToD3DAddress(descriptor.addressV);
    nativeDescriptor.AddressW = ToD3DAddress(descriptor.addressW);
    nativeDescriptor.MipLODBias = 0.0F;
    nativeDescriptor.MaxAnisotropy = descriptor.maxAnisotropy;
    nativeDescriptor.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    nativeDescriptor.MinLOD = descriptor.minLod;
    nativeDescriptor.MaxLOD = descriptor.maxLod;

    ID3D11SamplerState* sampler = nullptr;
    const HRESULT result = impl_->device->CreateSamplerState(
        &nativeDescriptor, &sampler);
    if (FAILED(result)) {
        if (IsDeviceRemoval(result)) {
            impl_->deviceLost = true;
        }
        return StatusFromHResult(
            result,
            "ID3D11Device::CreateSamplerState failed",
            "D3D11 device was lost while creating a sampler");
    }

    ReleaseCom(record->sampler);
    record->sampler = sampler;
    record->configured = true;
    return {};
}

Base::Result<void> D3D11GraphicsBackend::ConfigurePipeline(
    ResourceHandle handle,
    const PipelineDescriptor& descriptor) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized("D3D11 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    Base::Result<void> valid = ValidatePipelineDescriptor(
        descriptor, QueryGraphicsCapabilities());
    if (!valid) {
        return valid;
    }
    if (descriptor.vertexShader.language != ShaderLanguage::Dxbc ||
        descriptor.fragmentShader.language != ShaderLanguage::Dxbc) {
        return Unsupported("D3D11 pipelines require packaged DXBC shaders");
    }
    if (descriptor.sampleCount != 1U) {
        return Unsupported("Initial D3D11 backend supports single-sample pipelines only");
    }

    Impl::ResourceRecord* record = impl_->Find(handle);
    if (record == nullptr || handle.type != ResourceType::Pipeline) {
        return NotFound("D3D11 pipeline resource was not found");
    }

    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;
    ID3D11BlendState* blendState = nullptr;
    ID3D11RasterizerState* rasterizerState = nullptr;
    ID3D11DepthStencilState* depthStencilState = nullptr;

    HRESULT result = impl_->device->CreateVertexShader(
        descriptor.vertexShader.bytecode,
        descriptor.vertexShader.bytecodeSize,
        nullptr,
        &vertexShader);
    if (SUCCEEDED(result)) {
        result = impl_->device->CreatePixelShader(
            descriptor.fragmentShader.bytecode,
            descriptor.fragmentShader.bytecodeSize,
            nullptr,
            &pixelShader);
    }

    D3D11_INPUT_ELEMENT_DESC inputElements[MaxVertexAttributes]{};
    if (SUCCEEDED(result) && descriptor.vertexLayout.attributeCount > 0U) {
        for (std::uint32_t index = 0U;
             index < descriptor.vertexLayout.attributeCount;
             ++index) {
            const VertexAttribute& source =
                descriptor.vertexLayout.attributes[index];
            D3D11_INPUT_ELEMENT_DESC& target = inputElements[index];
            target.SemanticName = "ATTR";
            target.SemanticIndex = source.location;
            target.Format = ToDxgiVertexFormat(source.format);
            target.InputSlot = source.bufferSlot;
            target.AlignedByteOffset = source.offset;
            target.InputSlotClass =
                descriptor.vertexLayout.buffers[source.bufferSlot].stepMode ==
                    VertexStepMode::PerInstance
                ? D3D11_INPUT_PER_INSTANCE_DATA
                : D3D11_INPUT_PER_VERTEX_DATA;
            target.InstanceDataStepRate =
                target.InputSlotClass == D3D11_INPUT_PER_INSTANCE_DATA ? 1U : 0U;
        }
        result = impl_->device->CreateInputLayout(
            inputElements,
            descriptor.vertexLayout.attributeCount,
            descriptor.vertexShader.bytecode,
            descriptor.vertexShader.bytecodeSize,
            &inputLayout);
    }

    if (SUCCEEDED(result)) {
        D3D11_BLEND_DESC blend{};
        blend.AlphaToCoverageEnable = FALSE;
        blend.IndependentBlendEnable = FALSE;
        D3D11_RENDER_TARGET_BLEND_DESC& target = blend.RenderTarget[0];
        target.BlendEnable = descriptor.blend.enabled ? TRUE : FALSE;
        target.SrcBlend = ToD3DBlend(descriptor.blend.color.source);
        target.DestBlend = ToD3DBlend(descriptor.blend.color.destination);
        target.BlendOp = ToD3DBlendOperation(descriptor.blend.color.operation);
        target.SrcBlendAlpha = ToD3DBlend(descriptor.blend.alpha.source);
        target.DestBlendAlpha = ToD3DBlend(descriptor.blend.alpha.destination);
        target.BlendOpAlpha = ToD3DBlendOperation(descriptor.blend.alpha.operation);
        target.RenderTargetWriteMask = descriptor.blend.writeMask;
        result = impl_->device->CreateBlendState(&blend, &blendState);
    }

    if (SUCCEEDED(result)) {
        D3D11_RASTERIZER_DESC raster{};
        raster.FillMode = ToD3DFillMode(descriptor.raster.fillMode);
        raster.CullMode = ToD3DCullMode(descriptor.raster.cullMode);
        raster.FrontCounterClockwise =
            descriptor.raster.frontFace == FrontFace::CounterClockwise
            ? TRUE
            : FALSE;
        raster.DepthClipEnable = TRUE;
        raster.ScissorEnable = descriptor.raster.scissorEnabled ? TRUE : FALSE;
        result = impl_->device->CreateRasterizerState(
            &raster, &rasterizerState);
    }

    if (SUCCEEDED(result)) {
        const auto convertFace = [](const StencilFaceState& source) noexcept {
            D3D11_DEPTH_STENCILOP_DESC target{};
            target.StencilFailOp = ToD3DStencilOperation(source.fail);
            target.StencilDepthFailOp = ToD3DStencilOperation(source.depthFail);
            target.StencilPassOp = ToD3DStencilOperation(source.pass);
            target.StencilFunc = ToD3DCompare(source.compare);
            return target;
        };

        D3D11_DEPTH_STENCIL_DESC depth{};
        depth.DepthEnable = descriptor.depthStencil.depthTestEnabled ? TRUE : FALSE;
        depth.DepthWriteMask = descriptor.depthStencil.depthWriteEnabled
            ? D3D11_DEPTH_WRITE_MASK_ALL
            : D3D11_DEPTH_WRITE_MASK_ZERO;
        depth.DepthFunc = ToD3DCompare(descriptor.depthStencil.depthCompare);
        depth.StencilEnable = descriptor.depthStencil.stencilEnabled ? TRUE : FALSE;
        depth.StencilReadMask = descriptor.depthStencil.stencilReadMask;
        depth.StencilWriteMask = descriptor.depthStencil.stencilWriteMask;
        depth.FrontFace = convertFace(descriptor.depthStencil.front);
        depth.BackFace = convertFace(descriptor.depthStencil.back);
        result = impl_->device->CreateDepthStencilState(
            &depth, &depthStencilState);
    }

    if (FAILED(result)) {
        ReleaseCom(depthStencilState);
        ReleaseCom(rasterizerState);
        ReleaseCom(blendState);
        ReleaseCom(inputLayout);
        ReleaseCom(pixelShader);
        ReleaseCom(vertexShader);
        if (IsDeviceRemoval(result)) {
            impl_->deviceLost = true;
        }
        return StatusFromHResult(
            result,
            "D3D11 pipeline object creation failed",
            "D3D11 device was lost while creating a pipeline");
    }

    record->ReleasePipelineObjects();
    record->vertexShader = vertexShader;
    record->pixelShader = pixelShader;
    record->inputLayout = inputLayout;
    record->blendState = blendState;
    record->rasterizerState = rasterizerState;
    record->depthStencilState = depthStencilState;
    record->vertexLayout = descriptor.vertexLayout;
    record->topology = ToD3DTopology(descriptor.topology);
    record->configured = true;
    return {};
}

Base::Result<void> D3D11GraphicsBackend::ImportExternalRenderTarget(
    ResourceHandle handle,
    const D3D11ExternalRenderTargetDescriptor& descriptor) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized("D3D11 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    if (handle.type != ResourceType::RenderTarget ||
        descriptor.texture2D == 0U ||
        descriptor.texture.format == GraphicsTextureFormat::Depth24Stencil8 ||
        !HasTextureUsage(descriptor.texture.usage, TextureUsage::RenderTarget)) {
        return InvalidArgument("D3D11 external render-target descriptor is invalid");
    }
    Base::Result<void> valid = ValidateTextureDescriptor(
        descriptor.texture, QueryGraphicsCapabilities());
    if (!valid) {
        return valid;
    }

    Impl::ResourceRecord* record = impl_->Find(handle);
    if (record == nullptr) {
        return NotFound("D3D11 external target resource was not found");
    }
    if (record->configured) {
        return InvalidState("D3D11 resource is already configured");
    }

    auto* texture = reinterpret_cast<ID3D11Texture2D*>(descriptor.texture2D);
    ID3D11Device* textureDevice = nullptr;
    texture->GetDevice(&textureDevice);
    const bool sameDevice = textureDevice == impl_->device;
    ReleaseCom(textureDevice);
    if (!sameDevice) {
        return InvalidArgument(
            "D3D11 external render target belongs to another device");
    }

    D3D11_TEXTURE2D_DESC nativeDescriptor{};
    texture->GetDesc(&nativeDescriptor);
    if (nativeDescriptor.Width != descriptor.texture.width ||
        nativeDescriptor.Height != descriptor.texture.height ||
        nativeDescriptor.Format != ToDxgiFormat(descriptor.texture.format) ||
        nativeDescriptor.SampleDesc.Count != descriptor.texture.sampleCount) {
        return InvalidArgument(
            "D3D11 external texture does not match its Aero descriptor");
    }

    texture->AddRef();
    ID3D11RenderTargetView* renderTargetView = nullptr;
    ID3D11DepthStencilView* depthStencilView = nullptr;
    HRESULT result = S_OK;
    if (descriptor.renderTargetView != 0U) {
        renderTargetView = reinterpret_cast<ID3D11RenderTargetView*>(
            descriptor.renderTargetView);
        renderTargetView->AddRef();
    } else {
        result = impl_->device->CreateRenderTargetView(
            texture, nullptr, &renderTargetView);
    }
    if (SUCCEEDED(result) && descriptor.depthStencilView != 0U) {
        depthStencilView = reinterpret_cast<ID3D11DepthStencilView*>(
            descriptor.depthStencilView);
        depthStencilView->AddRef();
    }
    if (FAILED(result)) {
        ReleaseCom(depthStencilView);
        ReleaseCom(renderTargetView);
        ReleaseCom(texture);
        return StatusFromHResult(
            result,
            "Failed to create an RTV for the external D3D11 texture",
            "D3D11 device was lost while importing an external target");
    }

    record->texture = texture;
    record->renderTargetView = renderTargetView;
    record->depthStencilView = depthStencilView;
    record->textureDescriptor = descriptor.texture;
    record->configured = true;
    record->external = true;
    return {};
}
