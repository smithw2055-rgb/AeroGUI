                buffer->baseDescriptor.buffer.usage != BufferUsage::Index) {
                return failSubmission(
                    InvalidArgument("D3D11 index-buffer binding is invalid"));
            }
            UINT offset = 0U;
            if (!CheckedUint(offsetBytes, offset)) {
                return failSubmission(
                    OutOfRange("D3D11 index-buffer offset exceeds UINT range"));
            }
            const DXGI_FORMAT format = indexType == IndexType::UInt16
                ? DXGI_FORMAT_R16_UINT
                : DXGI_FORMAT_R32_UINT;
            if (state_->submissionState.indexBuffer != buffer->buffer ||
                state_->submissionState.indexFormat != format ||
                state_->submissionState.indexOffset != offset) {
                state_->context->IASetIndexBuffer(buffer->buffer, format, offset);
                state_->submissionState.indexBuffer = buffer->buffer;
                state_->submissionState.indexFormat = format;
                state_->submissionState.indexOffset = offset;
            }
            return {};
        };

        const auto bindUniformBuffer = [&](std::uint32_t slot,
                                           ResourceHandle bufferHandle,
                                           std::uint64_t offsetBytes,
                                           std::uint32_t sizeBytes) noexcept
            -> Base::Result<void> {
            if (!state_->inRenderPass) {
                return failSubmission(Unsupported(
                    "D3D11 uniform-buffer binding requires a render pass"));
            }
            D3D11RenderDeviceState::ResourceRecord* buffer = state_->Find(bufferHandle);
            if (buffer == nullptr || buffer->buffer == nullptr ||
                buffer->baseDescriptor.buffer.usage != BufferUsage::Uniform ||
                sizeBytes == 0U ||
                (offsetBytes & 15U) != 0U ||
                (sizeBytes & 15U) != 0U ||
                offsetBytes > buffer->baseDescriptor.buffer.sizeBytes ||
                sizeBytes > buffer->baseDescriptor.buffer.sizeBytes - offsetBytes ||
                slot >= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT) {
                return failSubmission(
                    InvalidArgument("D3D11 uniform-buffer binding is invalid"));
            }
            ID3D11Buffer* nativeBuffer = buffer->buffer;
            if (offsetBytes != 0U) {
                UINT sourceOffset = 0U;
                if (!CheckedUint(offsetBytes, sourceOffset) ||
                    sourceOffset > UINT_MAX - sizeBytes) {
                    return failSubmission(OutOfRange(
                        "D3D11 uniform-buffer offset exceeds UINT range"));
                }
                ID3D11Buffer*& scratch =
                    state_->uniformOffsetBuffers[slot];
                if (scratch == nullptr ||
                    state_->uniformOffsetBufferSizes[slot] != sizeBytes) {
                    ReleaseCom(scratch);
                    D3D11_BUFFER_DESC scratchDescriptor{};
                    scratchDescriptor.ByteWidth = sizeBytes;
                    scratchDescriptor.Usage = D3D11_USAGE_DEFAULT;
                    scratchDescriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                    const HRESULT created = state_->device->CreateBuffer(
                        &scratchDescriptor, nullptr, &scratch);
                    if (FAILED(created)) {
                        if (IsDeviceRemoval(created)) {
                            state_->deviceLost = true;
                        }
                        return failSubmission(StatusFromHResult(
                            created,
                            "Failed to create a D3D11 uniform-offset scratch buffer",
                            "D3D11 device was lost while creating a uniform-offset scratch buffer"));
                    }
                    state_->uniformOffsetBufferSizes[slot] = sizeBytes;
                }
                D3D11_BOX sourceRange{};
                sourceRange.left = sourceOffset;
                sourceRange.right = sourceOffset + sizeBytes;
                sourceRange.top = 0U;
                sourceRange.bottom = 1U;
                sourceRange.front = 0U;
                sourceRange.back = 1U;
                state_->context->CopySubresourceRegion(
                    scratch, 0U, 0U, 0U, 0U, buffer->buffer, 0U, &sourceRange);
                nativeBuffer = scratch;
            }
            if (state_->submissionState.vertexConstantBuffers[slot] !=
                    nativeBuffer ||
                state_->submissionState.pixelConstantBuffers[slot] !=
                    nativeBuffer) {
                state_->context->VSSetConstantBuffers(
                    slot, 1U, &nativeBuffer);
                state_->context->PSSetConstantBuffers(
                    slot, 1U, &nativeBuffer);
                state_->submissionState.vertexConstantBuffers[slot] =
                    nativeBuffer;
                state_->submissionState.pixelConstantBuffers[slot] =
                    nativeBuffer;
            }
            state_->submissionState.uniformBufferSizes[slot] = sizeBytes;
            return {};
        };

        const auto bindTextureSampler = [&](std::uint32_t slot,
                                            ResourceHandle textureHandle,
                                            ResourceHandle samplerHandle) noexcept
            -> Base::Result<void> {
            if (!state_->inRenderPass ||
                slot >= D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT ||
                slot >= D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT) {
                return failSubmission(
                    InvalidState("D3D11 texture binding requires a render pass"));
            }
            D3D11RenderDeviceState::ResourceRecord* texture = state_->Find(textureHandle);
            D3D11RenderDeviceState::ResourceRecord* sampler = state_->Find(samplerHandle);
            if (texture == nullptr || sampler == nullptr ||
                texture->shaderResourceView == nullptr || sampler->sampler == nullptr) {
                return failSubmission(
                    InvalidArgument("D3D11 texture or sampler is not configured"));
            }
            if (state_->IsActiveAttachment(texture->texture)) {
                return failSubmission(InvalidState(
                    "D3D11 texture cannot be sampled while bound as a render attachment"));
            }
            ID3D11ShaderResourceView* nativeView = texture->shaderResourceView;
            ID3D11SamplerState* nativeSampler = sampler->sampler;
            const D3D_SRV_DIMENSION textureDimension =
                texture->textureDescriptor.arrayLayers > 1U
                ? D3D_SRV_DIMENSION_TEXTURE2DARRAY
                : D3D_SRV_DIMENSION_TEXTURE2D;
            if (state_->submissionState.pixelShaderResources[slot] !=
                nativeView) {
                state_->context->PSSetShaderResources(
                    slot, 1U, &nativeView);
                state_->submissionState.pixelShaderResources[slot] =
                    nativeView;
            }
            state_->submissionState.pixelShaderResourceDimensions[slot] =
                textureDimension;
            if (state_->submissionState.pixelSamplers[slot] != nativeSampler) {
                state_->context->PSSetSamplers(
                    slot, 1U, &nativeSampler);
                state_->submissionState.pixelSamplers[slot] = nativeSampler;
            }
            return {};
        };

        const auto setScissor = [&](const Base::Rect& requestedRect) noexcept
            -> Base::Result<void> {
            if (!state_->inRenderPass) {
                return failSubmission(
                    InvalidState("D3D11 scissor requires a render pass"));
            }
            Base::Result<D3D11_RECT> nativeRect = ToD3DRect(requestedRect);
            if (!nativeRect) {
                return failSubmission(nativeRect.GetStatus());
            }
            const D3D11_RECT rect = nativeRect.Value();
            const D3D11_RECT& cachedScissor = state_->submissionState.scissor;
            if (!state_->submissionState.hasScissor ||
                cachedScissor.left != rect.left ||
                cachedScissor.top != rect.top ||
                cachedScissor.right != rect.right ||
                cachedScissor.bottom != rect.bottom) {
                state_->context->RSSetScissorRects(1U, &rect);
                state_->submissionState.scissor = rect;
                state_->submissionState.hasScissor = true;
            }
            return {};
        };

        const auto draw = [&](std::uint32_t first,
                              std::uint32_t count,
                              std::uint32_t instanceCount,
                              std::uint32_t firstInstance) noexcept
            -> Base::Result<void> {
            if (!state_->inRenderPass || !state_->currentPipeline.IsValid() ||
                count == 0U || instanceCount == 0U) {
                return failSubmission(
                    InvalidState("D3D11 draw state is incomplete"));
            }
            const D3D11RenderDeviceState::ResourceRecord* pipeline =
                state_->Find(state_->currentPipeline);
            if (pipeline == nullptr) {
                return failSubmission(
                    InvalidState("D3D11 draw pipeline is not configured"));
            }
            for (std::uint32_t slot = 0U;
                 slot < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
                 ++slot) {
                if (pipeline->uniformBufferMinimumSizes[slot] >
                    state_->submissionState.uniformBufferSizes[slot]) {
                    return failSubmission(InvalidState(
                        "D3D11 draw is missing required constant-buffer data"));
                }
            }
            for (std::uint32_t slot = 0U;
                 slot < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
                 ++slot) {
                if (pipeline->pixelTextureSlots[slot] &&
                    state_->submissionState.pixelShaderResources[slot] == nullptr) {
                    return failSubmission(InvalidState(
                        "D3D11 draw is missing a required texture binding"));
                }
                if (pipeline->pixelTextureSlots[slot] &&
                    pipeline->pixelTextureDimensions[slot] !=
                        state_->submissionState.pixelShaderResourceDimensions[slot]) {
                    return failSubmission(InvalidArgument(
                        "D3D11 texture SRV dimension does not match the pipeline shader"));
                }
            }
            for (std::uint32_t slot = 0U;
                 slot < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
                 ++slot) {
                if (pipeline->pixelSamplerSlots[slot] &&
                    state_->submissionState.pixelSamplers[slot] == nullptr) {
                    return failSubmission(InvalidState(
                        "D3D11 draw is missing a required sampler binding"));
                }
            }
            if (instanceCount == 1U && firstInstance == 0U) {
                state_->context->Draw(count, first);
            } else {
                state_->context->DrawInstanced(
                    count,
                    instanceCount,
                    first,
                    firstInstance);
            }
            return {};
        };

        const auto drawIndexed = [&](std::uint32_t first,
                                     std::uint32_t count,
                                     std::uint32_t instanceCount,
                                     std::uint32_t firstInstance,
                                     std::int32_t baseVertex) noexcept
            -> Base::Result<void> {
            if (!state_->inRenderPass || !state_->currentPipeline.IsValid() ||
                count == 0U || instanceCount == 0U) {
                return failSubmission(
                    InvalidState("D3D11 indexed-draw state is incomplete"));
            }
            const D3D11RenderDeviceState::ResourceRecord* pipeline =
                state_->Find(state_->currentPipeline);
            if (pipeline == nullptr) {
                return failSubmission(
                    InvalidState("D3D11 indexed-draw pipeline is not configured"));
            }
            for (std::uint32_t slot = 0U;
                 slot < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
                 ++slot) {
                if (pipeline->uniformBufferMinimumSizes[slot] >
                    state_->submissionState.uniformBufferSizes[slot]) {
                    return failSubmission(InvalidState(
                        "D3D11 indexed draw is missing required constant-buffer data"));
                }
            }
            for (std::uint32_t slot = 0U;
                 slot < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
                 ++slot) {
                if (pipeline->pixelTextureSlots[slot] &&
                    state_->submissionState.pixelShaderResources[slot] == nullptr) {
                    return failSubmission(InvalidState(
                        "D3D11 indexed draw is missing a required texture binding"));
                }
                if (pipeline->pixelTextureSlots[slot] &&
                    pipeline->pixelTextureDimensions[slot] !=
                        state_->submissionState.pixelShaderResourceDimensions[slot]) {
                    return failSubmission(InvalidArgument(
                        "D3D11 texture SRV dimension does not match the pipeline shader"));
                }
            }
            for (std::uint32_t slot = 0U;
                 slot < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
                 ++slot) {
                if (pipeline->pixelSamplerSlots[slot] &&
                    state_->submissionState.pixelSamplers[slot] == nullptr) {
                    return failSubmission(InvalidState(
                        "D3D11 indexed draw is missing a required sampler binding"));
                }
            }
            if (instanceCount == 1U && firstInstance == 0U) {
                state_->context->DrawIndexed(
                    count,
                    first,
                    baseVertex);
            } else {
                state_->context->DrawIndexedInstanced(
                    count,
                    instanceCount,
                    first,
                    baseVertex,
                    firstInstance);
            }
            return {};
        };

    Base::Result<void> executed = beginRenderPass(batch.pass);
    if (!executed) return executed;

    executed = bindPipeline(pipeline);
    if (!executed) return executed;

    for (std::uint32_t slot = 0U; slot < MaxVertexBuffers; ++slot) {
        if (!batch.drawState.vertexBuffers[slot].IsValid()) continue;
        executed = bindVertexBuffer(
            slot,
            batch.drawState.vertexBuffers[slot],
            batch.drawState.vertexOffsets[slot]);
        if (!executed) return executed;
    }

    if (batch.indexed && batch.drawState.indexBuffer.IsValid()) {
        executed = bindIndexBuffer(
            batch.drawState.indexBuffer,
            batch.drawState.indexOffset,
            batch.drawState.indexType);
        if (!executed) return executed;
    }

    for (std::uint32_t slot = 0U; slot < 4U; ++slot) {
        if (!batch.drawState.uniformBuffers[slot].IsValid()) continue;
        executed = bindUniformBuffer(
            slot,
            batch.drawState.uniformBuffers[slot],
            batch.drawState.uniformOffsets[slot],
            batch.drawState.uniformSizes[slot]);
        if (!executed) return executed;
    }

    for (std::uint32_t slot = 0U; slot < 8U; ++slot) {
        if (!batch.drawState.textures[slot].IsValid() ||
            !batch.drawState.samplers[slot].IsValid()) continue;
        executed = bindTextureSampler(
            slot,
            batch.drawState.textures[slot],
            batch.drawState.samplers[slot]);
        if (!executed) return executed;
    }

    executed = setScissor(batch.drawState.scissor);
    if (!executed) return executed;

    executed = batch.indexed
        ? drawIndexed(
              batch.first,
              batch.count,
              batch.instanceCount,
              batch.firstInstance,
              batch.baseVertex)
        : draw(
              batch.first,
              batch.count,
              batch.instanceCount,
              batch.firstInstance);
    if (!executed) return executed;

    executed = endRenderPass();
    if (!executed) return executed;

    if (state_->inRenderPass) {
        return failSubmission(
            InvalidState("D3D11 batch ended inside a render pass"));
    }

    state_->context->End(query);
    preservedState.Restore(state_->context);
    D3D11RenderDeviceState::PendingFence pending;
    pending.value = signalFence;
    pending.query = query;
    query = nullptr;
    Base::Result<void> queuedFence = state_->pendingFences.PushBack(
        std::move(pending));
    if (!queuedFence) {
        return queuedFence;
    }

    state_->lastSubmittedFence = signalFence;
    return {};
}
