Base::Result<void> D3D11RenderDevice::UpdateNativeBuffer(
    ResourceHandle buffer,
    std::uint64_t destinationOffset,
    Base::Span<const std::uint8_t> data) noexcept {
    if (state_ == nullptr) {
        return NotInitialized("D3D11 device is not initialized");
    }
    Base::Result<void> ready = state_->VerifyReady();
    if (!ready) return ready;
    D3D11RenderDeviceState::ResourceRecord* record = state_->Find(buffer);
    if (record == nullptr || record->buffer == nullptr || data.Empty() ||
        destinationOffset > record->baseDescriptor.buffer.sizeBytes ||
        data.Size() > record->baseDescriptor.buffer.sizeBytes - destinationOffset) {
        return InvalidArgument("D3D11 buffer update range is invalid");
    }

    if (record->baseDescriptor.buffer.usage == BufferUsage::Upload) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const HRESULT result = state_->context->Map(
            record->buffer, 0U, D3D11_MAP_WRITE, 0U, &mapped);
        if (FAILED(result)) {
            if (IsDeviceRemoval(result)) state_->deviceLost = true;
            return StatusFromHResult(
                result,
                "Failed to map a D3D11 upload buffer",
                "D3D11 device was lost while mapping an upload buffer");
        }
        std::memcpy(
            static_cast<std::uint8_t*>(mapped.pData) +
                static_cast<std::size_t>(destinationOffset),
            data.Data(), data.Size());
        state_->context->Unmap(record->buffer, 0U);
        return {};
    }
    if (record->baseDescriptor.buffer.usage == BufferUsage::Uniform) {
        if (record->uniformShadow == nullptr ||
            record->uniformShadowSize != record->baseDescriptor.buffer.sizeBytes) {
            return InvalidState("D3D11 uniform-buffer shadow is unavailable");
        }
        std::memcpy(
            record->uniformShadow + destinationOffset,
            data.Data(), data.Size());
        state_->context->UpdateSubresource(
            record->buffer, 0U, nullptr, record->uniformShadow, 0U, 0U);
        return {};
    }

    UINT left = 0U;
    UINT size = 0U;
    if (!CheckedUint(destinationOffset, left) ||
        !CheckedUint(data.Size(), size) || left > UINT_MAX - size) {
        return OutOfRange("D3D11 buffer update exceeds UINT range");
    }
    D3D11_BOX box{};
    box.left = left;
    box.right = left + size;
    box.top = 0U;
    box.bottom = 1U;
    box.front = 0U;
    box.back = 1U;
    state_->context->UpdateSubresource(
        record->buffer, 0U, &box, data.Data(), 0U, 0U);
    return {};
}

Base::Result<void> D3D11RenderDevice::UpdateNativeTexture(
    ResourceHandle texture,
    const TextureRegion& region,
    Base::Span<const std::uint8_t> data) noexcept {
    if (state_ == nullptr) {
        return NotInitialized("D3D11 device is not initialized");
    }
    Base::Result<void> ready = state_->VerifyReady();
    if (!ready) return ready;
    D3D11RenderDeviceState::ResourceRecord* record = state_->Find(texture);
    if (record == nullptr || record->texture == nullptr ||
        !record->configured || record->external || data.Empty() ||
        !HasTextureUsage(
            record->textureDescriptor.usage,
            TextureUsage::CopyDestination) ||
        region.width == 0U || region.height == 0U ||
        region.mipLevel >= record->textureDescriptor.mipLevels ||
        region.arrayLayer >= record->textureDescriptor.arrayLayers) {
        return InvalidArgument("D3D11 texture update is invalid");
    }
    const std::uint32_t mipWidth =
        record->textureDescriptor.width >> region.mipLevel;
    const std::uint32_t mipHeight =
        record->textureDescriptor.height >> region.mipLevel;
    const std::uint32_t effectiveWidth = mipWidth == 0U ? 1U : mipWidth;
    const std::uint32_t effectiveHeight = mipHeight == 0U ? 1U : mipHeight;
    if (region.x > effectiveWidth ||
        region.width > effectiveWidth - region.x ||
        region.y > effectiveHeight ||
        region.height > effectiveHeight - region.y) {
        return InvalidArgument("D3D11 texture update exceeds the mip extent");
    }
    const std::uint32_t bytesPerPixel =
        BytesPerPixel(record->textureDescriptor.format);
    if (bytesPerPixel == 0U ||
        region.width > UINT32_MAX / bytesPerPixel) {
        return OutOfRange("D3D11 texture update row size overflow");
    }
    const std::uint32_t tightRow = region.width * bytesPerPixel;
    if (region.bytesPerRow < tightRow ||
        region.height > UINT32_MAX / region.bytesPerRow ||
        data.Size() < region.height * region.bytesPerRow) {
        return InvalidArgument("D3D11 texture update payload is too small");
    }
    D3D11_BOX box{};
    box.left = region.x;
    box.top = region.y;
    box.front = 0U;
    box.right = region.x + region.width;
    box.bottom = region.y + region.height;
    box.back = 1U;
    const UINT subresource = D3D11CalcSubresource(
        region.mipLevel,
        region.arrayLayer,
        record->textureDescriptor.mipLevels);
    state_->context->UpdateSubresource(
        record->texture,
        subresource,
        &box,
        data.Data(),
        region.bytesPerRow,
        region.bytesPerRow * region.height);
    return {};
}

Base::Result<void> D3D11RenderDevice::Submit(
    const ::Aero::Render::RenderBatch& batch,
    ResourceHandle pipeline,
    FenceValue signalFence) noexcept {
    if (state_ == nullptr) {
        return NotInitialized("D3D11 backend is not initialized");
    }
    Base::Result<void> ready = state_->VerifyReady();
    if (!ready) {
        return ready;
    }
    if (signalFence == 0U || signalFence <= state_->lastSubmittedFence) {
        return InvalidArgument("D3D11 submission fence must increase monotonically");
    }

    Base::Result<void> reserve = state_->pendingFences.Reserve(
        state_->pendingFences.Size() + 1U);
    if (!reserve) {
        return reserve;
    }

    D3D11_QUERY_DESC queryDescriptor{};
    queryDescriptor.Query = D3D11_QUERY_EVENT;
    ID3D11Query* query = nullptr;
    HRESULT result = state_->device->CreateQuery(&queryDescriptor, &query);
    if (FAILED(result)) {
        if (IsDeviceRemoval(result)) {
            state_->deviceLost = true;
        }
        return StatusFromHResult(
            result,
            "Failed to create a D3D11 fence query",
            "D3D11 device was lost while creating a fence query");
    }

    state_->currentPipeline = {};
    state_->inRenderPass = false;
    state_->submissionState.Reset();
    D3D11RenderDeviceState::RequiredStateSnapshot preservedState;
    if (options_.statePolicy == D3D11StatePolicy::PreserveRequiredState) {
        preservedState.Capture(state_->context);
    }

    auto failSubmission = [&](Base::Status status) noexcept -> Base::Result<void> {
        if (state_->inRenderPass) {
            state_->context->OMSetRenderTargets(0U, nullptr, nullptr);
            state_->inRenderPass = false;
        }
        // A failed draw can occur after an SRV has been bound but before its
        // matching render-pass end. Always release those bindings before
        // returning so HostResetsState cannot carry a failed pass's SRV into
        // the next render-target bind. PreserveRequiredState restores the
        // host's captured bindings immediately below.
        state_->ClearShaderResourceBindings();
        state_->submissionState.ResetPixelShaderResources();
        state_->ClearActiveAttachments();
        preservedState.Restore(state_->context);
        ReleaseCom(query);
        return status;
    };

    const auto beginRenderPass = [&](const RenderPassDescriptor& renderPass) noexcept
        -> Base::Result<void> {
