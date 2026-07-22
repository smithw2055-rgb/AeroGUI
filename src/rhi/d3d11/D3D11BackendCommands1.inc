Base::Result<void> D3D11GraphicsBackend::Submit(
    const CommandBuffer&,
    FenceValue) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized("D3D11 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    return ready
        ? Base::Result<void>(Unsupported(
            "D3D11 backend does not implement the legacy rectangle command stream"))
        : ready;
}

Base::Result<void> D3D11GraphicsBackend::SubmitGraphics(
    const GraphicsCommandBuffer& commands,
    FenceValue signalFence) noexcept {
    if (impl_ == nullptr) {
        return NotInitialized("D3D11 backend is not initialized");
    }
    Base::Result<void> ready = impl_->VerifyReady();
    if (!ready) {
        return ready;
    }
    if (signalFence == 0U || signalFence <= impl_->lastSubmittedFence) {
        return InvalidArgument("D3D11 submission fence must increase monotonically");
    }

    Base::Result<void> reserve = impl_->pendingFences.TryReserve(
        impl_->pendingFences.Size() + 1U);
    if (!reserve) {
        return reserve;
    }

    D3D11_QUERY_DESC queryDescriptor{};
    queryDescriptor.Query = D3D11_QUERY_EVENT;
    ID3D11Query* query = nullptr;
    HRESULT result = impl_->device->CreateQuery(&queryDescriptor, &query);
    if (FAILED(result)) {
        if (IsDeviceRemoval(result)) {
            impl_->deviceLost = true;
        }
        return StatusFromHResult(
            result,
            "Failed to create a D3D11 fence query",
            "D3D11 device was lost while creating a fence query");
    }

    impl_->currentPipeline = {};
    impl_->inRenderPass = false;
    const Base::Span<const GraphicsCommand> commandSpan = commands.Commands();
    const Base::Span<const std::uint8_t> uploadSpan = commands.UploadBytes();

    auto failSubmission = [&](Base::Status status) noexcept -> Base::Result<void> {
        if (impl_->inRenderPass) {
            impl_->context->OMSetRenderTargets(0U, nullptr, nullptr);
            impl_->inRenderPass = false;
        }
        ReleaseCom(query);
        return status;
    };

    for (const GraphicsCommand& command : commandSpan) {
        switch (command.kind) {
        case GraphicsCommandKind::UploadBuffer: {
            if (impl_->inRenderPass ||
                command.uploadOffset > uploadSpan.Size() ||
                command.uploadSize > uploadSpan.Size() - command.uploadOffset) {
                return failSubmission(
                    InvalidState("D3D11 buffer upload command is invalid"));
            }
            Impl::ResourceRecord* record = impl_->Find(command.resource0);
            if (record == nullptr || record->buffer == nullptr ||
                command.resourceOffset > record->baseDescriptor.buffer.sizeBytes ||
                command.uploadSize >
                    record->baseDescriptor.buffer.sizeBytes - command.resourceOffset) {
                return failSubmission(
                    InvalidArgument("D3D11 buffer upload range is invalid"));
            }
            const Base::Span<const std::uint8_t> data = uploadSpan.Subspan(
                command.uploadOffset, command.uploadSize);
            if (record->baseDescriptor.buffer.usage == BufferUsage::Upload) {
                D3D11_MAPPED_SUBRESOURCE mapped{};
                result = impl_->context->Map(
                    record->buffer,
                    0U,
                    D3D11_MAP_WRITE,
                    0U,
                    &mapped);
                if (FAILED(result)) {
                    if (IsDeviceRemoval(result)) {
                        impl_->deviceLost = true;
                    }
                    return failSubmission(StatusFromHResult(
                        result,
                        "Failed to map a D3D11 upload buffer",
                        "D3D11 device was lost while mapping an upload buffer"));
                }
                std::memcpy(
                    static_cast<std::uint8_t*>(mapped.pData) +
                        static_cast<std::size_t>(command.resourceOffset),
                    data.Data(),
                    data.Size());
                impl_->context->Unmap(record->buffer, 0U);
            } else {
                UINT left = 0U;
                UINT size = 0U;
                if (!CheckedUint(command.resourceOffset, left) ||
                    !CheckedUint(command.uploadSize, size) ||
                    left > UINT_MAX - size) {
                    return failSubmission(
                        OutOfRange("D3D11 buffer upload exceeds UINT range"));
                }
                D3D11_BOX box{};
                box.left = left;
                box.right = left + size;
                box.top = 0U;
                box.bottom = 1U;
                box.front = 0U;
                box.back = 1U;
                impl_->context->UpdateSubresource(
                    record->buffer,
                    0U,
                    &box,
                    data.Data(),
                    0U,
                    0U);
            }
            break;
        }
        case GraphicsCommandKind::UploadTexture: {
            if (impl_->inRenderPass ||
                command.uploadOffset > uploadSpan.Size() ||
                command.uploadSize > uploadSpan.Size() - command.uploadOffset) {
                return failSubmission(
                    InvalidState("D3D11 texture upload command is invalid"));
            }
            Impl::ResourceRecord* record = impl_->Find(command.resource0);
            if (record == nullptr || record->texture == nullptr ||
                !record->configured || record->external ||
                !HasTextureUsage(
                    record->textureDescriptor.usage,
                    TextureUsage::CopyDestination)) {
                return failSubmission(
                    InvalidArgument("D3D11 texture is not uploadable"));
            }
            const TextureRegion& region = command.textureRegion;
            if (region.width == 0U || region.height == 0U ||
                region.mipLevel >= record->textureDescriptor.mipLevels ||
                region.arrayLayer >= record->textureDescriptor.arrayLayers) {
                return failSubmission(
                    InvalidArgument("D3D11 texture upload region is invalid"));
            }
            const std::uint32_t mipWidth =
                record->textureDescriptor.width >> region.mipLevel;
            const std::uint32_t mipHeight =
                record->textureDescriptor.height >> region.mipLevel;
            const std::uint32_t effectiveWidth = mipWidth == 0U ? 1U : mipWidth;
            const std::uint32_t effectiveHeight = mipHeight == 0U ? 1U : mipHeight;
            if (region.x > effectiveWidth || region.width > effectiveWidth - region.x ||
                region.y > effectiveHeight || region.height > effectiveHeight - region.y) {
                return failSubmission(
                    InvalidArgument("D3D11 texture upload exceeds the mip extent"));
            }
            const std::uint32_t bytesPerPixel = BytesPerPixel(
                record->textureDescriptor.format);
            if (bytesPerPixel == 0U ||
                region.width > UINT32_MAX / bytesPerPixel) {
                return failSubmission(
                    OutOfRange("D3D11 texture upload row size overflow"));
            }
