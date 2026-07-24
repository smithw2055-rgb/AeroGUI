FenceValue D3D11GraphicsBackend::CompletedFence() const noexcept {
    if (impl_ == nullptr) {
        return 0U;
    }
    impl_->PollFences();
    return impl_->completedFence;
}

bool D3D11GraphicsBackend::IsDeviceLost() const noexcept {
    if (impl_ == nullptr || !impl_->initialized) {
        return true;
    }
    impl_->PollFences();
    return impl_->deviceLost;
}

Base::Result<void> D3D11GraphicsBackend::WaitForFence(
    FenceValue fence,
    std::uint32_t timeoutMilliseconds) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized("D3D11 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    if (fence == 0U || fence > impl_->lastSubmittedFence) {
        return InvalidArgument("D3D11 wait fence is unknown");
    }
    if (timeoutMilliseconds == 0U) {
        return InvalidArgument("D3D11 wait timeout must be non-zero");
    }

    impl_->context->Flush();
    const ULONGLONG start = GetTickCount64();
    while (CompletedFence() < fence) {
        if (IsDeviceLost()) {
            return InvalidState("D3D11 device was lost while waiting for a fence");
        }
        if (GetTickCount64() - start >= timeoutMilliseconds) {
            return InvalidState("Timed out while waiting for a D3D11 fence");
        }
        Sleep(0U);
    }
    return {};
}

Base::Result<void> D3D11GraphicsBackend::ReadbackTexture(
    ResourceHandle handle,
    Base::Span<std::uint8_t> destination,
    std::uint32_t destinationRowPitch) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized("D3D11 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    const Impl::ResourceRecord* record = impl_->Find(handle);
    if (record == nullptr || record->texture == nullptr ||
        !record->configured || record->textureDescriptor.sampleCount != 1U ||
        !HasTextureUsage(record->textureDescriptor.usage, TextureUsage::CopySource)) {
        return InvalidArgument("D3D11 texture is not eligible for readback");
    }

    const std::uint32_t bytesPerPixel = BytesPerPixel(
        record->textureDescriptor.format);
    if (bytesPerPixel == 0U ||
        record->textureDescriptor.width > UINT32_MAX / bytesPerPixel) {
        return OutOfRange("D3D11 readback row size overflow");
    }
    const std::uint32_t rowBytes =
        record->textureDescriptor.width * bytesPerPixel;
    if (destinationRowPitch < rowBytes ||
        record->textureDescriptor.height > UINT32_MAX / destinationRowPitch ||
        destination.Size() <
            record->textureDescriptor.height * destinationRowPitch) {
        return InvalidArgument("D3D11 readback destination is too small");
    }

    D3D11_TEXTURE2D_DESC stagingDescriptor{};
    stagingDescriptor.Width = record->textureDescriptor.width;
    stagingDescriptor.Height = record->textureDescriptor.height;
    stagingDescriptor.MipLevels = 1U;
    stagingDescriptor.ArraySize = 1U;
    stagingDescriptor.Format = ToDxgiFormat(record->textureDescriptor.format);
    stagingDescriptor.SampleDesc.Count = 1U;
    stagingDescriptor.Usage = D3D11_USAGE_STAGING;
    stagingDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ID3D11Texture2D* staging = nullptr;
    HRESULT result = impl_->device->CreateTexture2D(
        &stagingDescriptor, nullptr, &staging);
    if (SUCCEEDED(result)) {
        impl_->context->CopySubresourceRegion(
            staging,
            0U,
            0U,
            0U,
            0U,
            record->texture,
            0U,
            nullptr);
        impl_->context->Flush();
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(result)) {
        result = impl_->context->Map(
            staging,
            0U,
            D3D11_MAP_READ,
            0U,
            &mapped);
    }
    if (FAILED(result)) {
        ReleaseCom(staging);
        if (IsDeviceRemoval(result)) {
            impl_->deviceLost = true;
        }
        return StatusFromHResult(
            result,
            "D3D11 texture readback failed",
            "D3D11 device was lost during texture readback");
    }

    const auto* source = static_cast<const std::uint8_t*>(mapped.pData);
    for (std::uint32_t row = 0U;
         row < record->textureDescriptor.height;
         ++row) {
        std::memcpy(
            destination.Data() +
                static_cast<std::size_t>(row) * destinationRowPitch,
            source + static_cast<std::size_t>(row) * mapped.RowPitch,
            rowBytes);
    }
    impl_->context->Unmap(staging, 0U);
    ReleaseCom(staging);
    return {};
}

Base::Result<std::uint64_t>
D3D11GraphicsBackend::ReadbackTextureChecksum(
    ResourceHandle handle) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized("D3D11 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready.GetStatus();
    }
    const Impl::ResourceRecord* record = impl_->Find(handle);
    if (record == nullptr || record->texture == nullptr ||
        !record->configured || record->textureDescriptor.sampleCount != 1U ||
        !HasTextureUsage(record->textureDescriptor.usage, TextureUsage::CopySource)) {
        return InvalidArgument("D3D11 texture is not eligible for checksum readback");
    }

    const std::uint32_t bytesPerPixel = BytesPerPixel(
        record->textureDescriptor.format);
    if (bytesPerPixel == 0U ||
        record->textureDescriptor.width > UINT32_MAX / bytesPerPixel) {
        return OutOfRange("D3D11 checksum row size overflow");
    }
    const std::uint32_t rowBytes =
        record->textureDescriptor.width * bytesPerPixel;

    D3D11_TEXTURE2D_DESC stagingDescriptor{};
    stagingDescriptor.Width = record->textureDescriptor.width;
    stagingDescriptor.Height = record->textureDescriptor.height;
    stagingDescriptor.MipLevels = 1U;
    stagingDescriptor.ArraySize = 1U;
    stagingDescriptor.Format = ToDxgiFormat(record->textureDescriptor.format);
    stagingDescriptor.SampleDesc.Count = 1U;
    stagingDescriptor.Usage = D3D11_USAGE_STAGING;
    stagingDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ID3D11Texture2D* staging = nullptr;
    HRESULT result = impl_->device->CreateTexture2D(
        &stagingDescriptor, nullptr, &staging);
    if (SUCCEEDED(result)) {
        impl_->context->CopySubresourceRegion(
            staging,
            0U,
            0U,
            0U,
            0U,
            record->texture,
            0U,
            nullptr);
        impl_->context->Flush();
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(result)) {
        result = impl_->context->Map(
            staging,
            0U,
            D3D11_MAP_READ,
            0U,
            &mapped);
    }
    if (FAILED(result)) {
        ReleaseCom(staging);
        if (IsDeviceRemoval(result)) {
            impl_->deviceLost = true;
        }
        return StatusFromHResult(
            result,
            "D3D11 checksum readback failed",
            "D3D11 device was lost during checksum readback");
    }

    std::uint64_t hash = HashOffset;
    const auto* source = static_cast<const std::uint8_t*>(mapped.pData);
    for (std::uint32_t row = 0U;
         row < record->textureDescriptor.height;
         ++row) {
        HashBytes(
            hash,
            source + static_cast<std::size_t>(row) * mapped.RowPitch,
            rowBytes);
    }
    impl_->context->Unmap(staging, 0U);
    ReleaseCom(staging);
    return hash;
}

Base::Result<ResourceHandle> ImportD3D11ExternalRenderTarget(
    RhiDevice& device,
    D3D11GraphicsBackend& backend,
    const D3D11ExternalRenderTargetDescriptor& descriptor) noexcept {
    if (descriptor.texture.format == GraphicsTextureFormat::Depth24Stencil8) {
        return InvalidArgument("D3D11 external color target cannot use a depth format");
    }

    Base::Result<ResourceHandle> created =
        device.CreateExternalRenderTarget(descriptor.texture);
    if (!created) {
        return created.GetStatus();
    }

    const ResourceHandle handle = created.Value();
    Base::Result<void> imported = backend.ImportExternalRenderTarget(
        handle, descriptor);
    if (!imported) {
        static_cast<void>(device.DestroyResource(handle, 0U));
        static_cast<void>(device.CollectGarbage());
        return imported.GetStatus();
    }
    return handle;
}
