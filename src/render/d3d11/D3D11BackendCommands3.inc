                buffer->baseDescriptor.buffer.usage != BufferUsage::Index) {
                return failSubmission(
                    InvalidArgument("D3D11 index-buffer binding is invalid"));
            }
            UINT offset = 0U;
            if (!CheckedUint(command.resourceOffset, offset)) {
                return failSubmission(
                    OutOfRange("D3D11 index-buffer offset exceeds UINT range"));
            }
            const DXGI_FORMAT format = command.indexType == IndexType::UInt16
                ? DXGI_FORMAT_R16_UINT
                : DXGI_FORMAT_R32_UINT;
            if (impl_->submissionState.indexBuffer != buffer->buffer ||
                impl_->submissionState.indexFormat != format ||
                impl_->submissionState.indexOffset != offset) {
                impl_->context->IASetIndexBuffer(buffer->buffer, format, offset);
                impl_->submissionState.indexBuffer = buffer->buffer;
                impl_->submissionState.indexFormat = format;
                impl_->submissionState.indexOffset = offset;
            }
            break;
        }
        case CommandKind::BindUniformBuffer: {
            if (!impl_->inRenderPass) {
                return failSubmission(Unsupported(
                    "D3D11 uniform-buffer binding requires a render pass"));
            }
            Impl::ResourceRecord* buffer = impl_->Find(command.resource0);
            if (buffer == nullptr || buffer->buffer == nullptr ||
                buffer->baseDescriptor.buffer.usage != BufferUsage::Uniform ||
                command.resourceSize == 0U ||
                (command.resourceOffset & 15U) != 0U ||
                (command.resourceSize & 15U) != 0U ||
                command.resourceOffset > buffer->baseDescriptor.buffer.sizeBytes ||
                command.resourceSize > buffer->baseDescriptor.buffer.sizeBytes -
                    command.resourceOffset ||
                command.slot >= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT) {
                return failSubmission(
                    InvalidArgument("D3D11 uniform-buffer binding is invalid"));
            }
            ID3D11Buffer* nativeBuffer = buffer->buffer;
            if (command.resourceOffset != 0U) {
                UINT sourceOffset = 0U;
                if (!CheckedUint(command.resourceOffset, sourceOffset) ||
                    sourceOffset > UINT_MAX - command.resourceSize) {
                    return failSubmission(OutOfRange(
                        "D3D11 uniform-buffer offset exceeds UINT range"));
                }
                ID3D11Buffer*& scratch =
                    impl_->uniformOffsetBuffers[command.slot];
                if (scratch == nullptr ||
                    impl_->uniformOffsetBufferSizes[command.slot] !=
                        command.resourceSize) {
                    ReleaseCom(scratch);
                    D3D11_BUFFER_DESC scratchDescriptor{};
                    scratchDescriptor.ByteWidth = command.resourceSize;
                    scratchDescriptor.Usage = D3D11_USAGE_DEFAULT;
                    scratchDescriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                    const HRESULT created = impl_->device->CreateBuffer(
                        &scratchDescriptor, nullptr, &scratch);
                    if (FAILED(created)) {
                        if (IsDeviceRemoval(created)) {
                            impl_->deviceLost = true;
                        }
                        return failSubmission(StatusFromHResult(
                            created,
                            "Failed to create a D3D11 uniform-offset scratch buffer",
                            "D3D11 device was lost while creating a uniform-offset scratch buffer"));
                    }
                    impl_->uniformOffsetBufferSizes[command.slot] =
                        command.resourceSize;
                }
                D3D11_BOX sourceRange{};
                sourceRange.left = sourceOffset;
                sourceRange.right = sourceOffset + command.resourceSize;
                sourceRange.top = 0U;
                sourceRange.bottom = 1U;
                sourceRange.front = 0U;
                sourceRange.back = 1U;
                impl_->context->CopySubresourceRegion(
                    scratch, 0U, 0U, 0U, 0U, buffer->buffer, 0U, &sourceRange);
                nativeBuffer = scratch;
            }
            if (impl_->submissionState.vertexConstantBuffers[command.slot] !=
                    nativeBuffer ||
                impl_->submissionState.pixelConstantBuffers[command.slot] !=
                    nativeBuffer) {
                impl_->context->VSSetConstantBuffers(
                    command.slot, 1U, &nativeBuffer);
                impl_->context->PSSetConstantBuffers(
                    command.slot, 1U, &nativeBuffer);
                impl_->submissionState.vertexConstantBuffers[command.slot] =
                    nativeBuffer;
                impl_->submissionState.pixelConstantBuffers[command.slot] =
                    nativeBuffer;
            }
            impl_->submissionState.uniformBufferSizes[command.slot] =
                command.resourceSize;
            break;
        }
        case CommandKind::BindTextureSampler: {
            if (!impl_->inRenderPass ||
                command.slot >= D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT ||
                command.slot >= D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT) {
                return failSubmission(
                    InvalidState("D3D11 texture binding requires a render pass"));
            }
            Impl::ResourceRecord* texture = impl_->Find(command.resource0);
            Impl::ResourceRecord* sampler = impl_->Find(command.resource1);
            if (texture == nullptr || sampler == nullptr ||
                texture->shaderResourceView == nullptr || sampler->sampler == nullptr) {
                return failSubmission(
                    InvalidArgument("D3D11 texture or sampler is not configured"));
            }
            if (impl_->IsActiveAttachment(texture->texture)) {
                return failSubmission(InvalidState(
                    "D3D11 texture cannot be sampled while bound as a render attachment"));
            }
            ID3D11ShaderResourceView* nativeView = texture->shaderResourceView;
            ID3D11SamplerState* nativeSampler = sampler->sampler;
            const D3D_SRV_DIMENSION textureDimension =
                texture->textureDescriptor.arrayLayers > 1U
                ? D3D_SRV_DIMENSION_TEXTURE2DARRAY
                : D3D_SRV_DIMENSION_TEXTURE2D;
            if (impl_->submissionState.pixelShaderResources[command.slot] !=
                nativeView) {
                impl_->context->PSSetShaderResources(
                    command.slot, 1U, &nativeView);
                impl_->submissionState.pixelShaderResources[command.slot] =
                    nativeView;
            }
            impl_->submissionState.pixelShaderResourceDimensions[command.slot] =
                textureDimension;
            if (impl_->submissionState.pixelSamplers[command.slot] != nativeSampler) {
                impl_->context->PSSetSamplers(
                    command.slot, 1U, &nativeSampler);
                impl_->submissionState.pixelSamplers[command.slot] = nativeSampler;
            }
            break;
        }
        case CommandKind::SetScissor: {
            if (!impl_->inRenderPass) {
                return failSubmission(
                    InvalidState("D3D11 scissor requires a render pass"));
            }
            Base::Result<D3D11_RECT> nativeRect = ToD3DRect(command.rect);
            if (!nativeRect) {
                return failSubmission(nativeRect.GetStatus());
            }
            const D3D11_RECT rect = nativeRect.Value();
            const D3D11_RECT& cachedScissor = impl_->submissionState.scissor;
            if (!impl_->submissionState.hasScissor ||
                cachedScissor.left != rect.left ||
                cachedScissor.top != rect.top ||
                cachedScissor.right != rect.right ||
                cachedScissor.bottom != rect.bottom) {
                impl_->context->RSSetScissorRects(1U, &rect);
                impl_->submissionState.scissor = rect;
                impl_->submissionState.hasScissor = true;
            }
            break;
        }
        case CommandKind::Draw: {
            if (!impl_->inRenderPass || !impl_->currentPipeline.IsValid() ||
                command.count == 0U || command.instanceCount == 0U) {
                return failSubmission(
                    InvalidState("D3D11 draw state is incomplete"));
            }
            const Impl::ResourceRecord* pipeline =
                impl_->Find(impl_->currentPipeline);
            if (pipeline == nullptr) {
                return failSubmission(
                    InvalidState("D3D11 draw pipeline is not configured"));
            }
            for (std::uint32_t slot = 0U;
                 slot < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
                 ++slot) {
                if (pipeline->uniformBufferMinimumSizes[slot] >
                    impl_->submissionState.uniformBufferSizes[slot]) {
                    return failSubmission(InvalidState(
                        "D3D11 draw is missing required constant-buffer data"));
                }
            }
            for (std::uint32_t slot = 0U;
                 slot < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
                 ++slot) {
                if (pipeline->pixelTextureSlots[slot] &&
                    impl_->submissionState.pixelShaderResources[slot] == nullptr) {
                    return failSubmission(InvalidState(
                        "D3D11 draw is missing a required texture binding"));
                }
                if (pipeline->pixelTextureSlots[slot] &&
                    pipeline->pixelTextureDimensions[slot] !=
                        impl_->submissionState.pixelShaderResourceDimensions[slot]) {
                    return failSubmission(InvalidArgument(
                        "D3D11 texture SRV dimension does not match the pipeline shader"));
                }
            }
            for (std::uint32_t slot = 0U;
                 slot < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
                 ++slot) {
                if (pipeline->pixelSamplerSlots[slot] &&
                    impl_->submissionState.pixelSamplers[slot] == nullptr) {
                    return failSubmission(InvalidState(
                        "D3D11 draw is missing a required sampler binding"));
                }
            }
            if (command.instanceCount == 1U && command.firstInstance == 0U) {
                impl_->context->Draw(command.count, command.first);
            } else {
                impl_->context->DrawInstanced(
                    command.count,
                    command.instanceCount,
                    command.first,
                    command.firstInstance);
            }
            break;
        }
        case CommandKind::DrawIndexed: {
            if (!impl_->inRenderPass || !impl_->currentPipeline.IsValid() ||
                command.count == 0U || command.instanceCount == 0U) {
                return failSubmission(
                    InvalidState("D3D11 indexed-draw state is incomplete"));
            }
            const Impl::ResourceRecord* pipeline =
                impl_->Find(impl_->currentPipeline);
            if (pipeline == nullptr) {
                return failSubmission(
                    InvalidState("D3D11 indexed-draw pipeline is not configured"));
            }
            for (std::uint32_t slot = 0U;
                 slot < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
                 ++slot) {
                if (pipeline->uniformBufferMinimumSizes[slot] >
                    impl_->submissionState.uniformBufferSizes[slot]) {
                    return failSubmission(InvalidState(
                        "D3D11 indexed draw is missing required constant-buffer data"));
                }
            }
            for (std::uint32_t slot = 0U;
                 slot < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
                 ++slot) {
                if (pipeline->pixelTextureSlots[slot] &&
                    impl_->submissionState.pixelShaderResources[slot] == nullptr) {
                    return failSubmission(InvalidState(
                        "D3D11 indexed draw is missing a required texture binding"));
                }
                if (pipeline->pixelTextureSlots[slot] &&
                    pipeline->pixelTextureDimensions[slot] !=
                        impl_->submissionState.pixelShaderResourceDimensions[slot]) {
                    return failSubmission(InvalidArgument(
                        "D3D11 texture SRV dimension does not match the pipeline shader"));
                }
            }
            for (std::uint32_t slot = 0U;
                 slot < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
                 ++slot) {
                if (pipeline->pixelSamplerSlots[slot] &&
                    impl_->submissionState.pixelSamplers[slot] == nullptr) {
                    return failSubmission(InvalidState(
                        "D3D11 indexed draw is missing a required sampler binding"));
                }
            }
            if (command.instanceCount == 1U && command.firstInstance == 0U) {
                impl_->context->DrawIndexed(
                    command.count,
                    command.first,
                    command.baseVertex);
            } else {
                impl_->context->DrawIndexedInstanced(
                    command.count,
                    command.instanceCount,
                    command.first,
                    command.baseVertex,
                    command.firstInstance);
            }
            break;
        }
        default:
            return failSubmission(
                InvalidArgument("D3D11 graphics command kind is invalid"));
        }
    }

    if (impl_->inRenderPass) {
        return failSubmission(
            InvalidState("D3D11 command buffer ended inside a render pass"));
    }

    impl_->context->End(query);
    preservedState.Restore(impl_->context);
    Impl::PendingFence pending;
    pending.value = signalFence;
    pending.query = query;
    query = nullptr;
    Base::Result<void> appended = impl_->pendingFences.PushBack(
        std::move(pending));
    if (!appended) {
        return appended;
    }

    impl_->lastSubmittedFence = signalFence;
    return {};
}
