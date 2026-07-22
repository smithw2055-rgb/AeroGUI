                buffer->baseDescriptor.buffer.usage != BufferUsage::Index) {
                return failSubmission(
                    InvalidArgument("D3D11 index-buffer binding is invalid"));
            }
            UINT offset = 0U;
            if (!CheckedUint(command.resourceOffset, offset)) {
                return failSubmission(
                    OutOfRange("D3D11 index-buffer offset exceeds UINT range"));
            }
            impl_->context->IASetIndexBuffer(
                buffer->buffer,
                command.indexType == IndexType::UInt16
                    ? DXGI_FORMAT_R16_UINT
                    : DXGI_FORMAT_R32_UINT,
                offset);
            break;
        }
        case GraphicsCommandKind::BindUniformBuffer: {
            if (!impl_->inRenderPass || command.resourceOffset != 0U) {
                return failSubmission(Unsupported(
                    "Initial D3D11 backend requires zero uniform-buffer offset"));
            }
            Impl::ResourceRecord* buffer = impl_->Find(command.resource0);
            if (buffer == nullptr || buffer->buffer == nullptr ||
                buffer->baseDescriptor.buffer.usage != BufferUsage::Uniform ||
                command.resourceSize == 0U ||
                command.resourceSize > buffer->baseDescriptor.buffer.sizeBytes ||
                command.slot >= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT) {
                return failSubmission(
                    InvalidArgument("D3D11 uniform-buffer binding is invalid"));
            }
            ID3D11Buffer* nativeBuffer = buffer->buffer;
            impl_->context->VSSetConstantBuffers(
                command.slot, 1U, &nativeBuffer);
            impl_->context->PSSetConstantBuffers(
                command.slot, 1U, &nativeBuffer);
            break;
        }
        case GraphicsCommandKind::BindTextureSampler: {
            if (!impl_->inRenderPass ||
                command.slot >= D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) {
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
            ID3D11ShaderResourceView* nativeView = texture->shaderResourceView;
            ID3D11SamplerState* nativeSampler = sampler->sampler;
            impl_->context->PSSetShaderResources(
                command.slot, 1U, &nativeView);
            impl_->context->PSSetSamplers(
                command.slot, 1U, &nativeSampler);
            break;
        }
        case GraphicsCommandKind::SetScissor: {
            if (!impl_->inRenderPass) {
                return failSubmission(
                    InvalidState("D3D11 scissor requires a render pass"));
            }
            Base::Result<D3D11_RECT> nativeRect = ToD3DRect(command.rect);
            if (!nativeRect) {
                return failSubmission(nativeRect.GetStatus());
            }
            const D3D11_RECT rect = nativeRect.Value();
            impl_->context->RSSetScissorRects(1U, &rect);
            break;
        }
        case GraphicsCommandKind::Draw: {
            if (!impl_->inRenderPass || !impl_->currentPipeline.IsValid() ||
                command.count == 0U || command.instanceCount == 0U) {
                return failSubmission(
                    InvalidState("D3D11 draw state is incomplete"));
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
        case GraphicsCommandKind::DrawIndexed: {
            if (!impl_->inRenderPass || !impl_->currentPipeline.IsValid() ||
                command.count == 0U || command.instanceCount == 0U) {
                return failSubmission(
                    InvalidState("D3D11 indexed-draw state is incomplete"));
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
    Impl::PendingFence pending;
    pending.value = signalFence;
    pending.query = query;
    query = nullptr;
    Base::Result<void> appended = impl_->pendingFences.TryPushBack(
        std::move(pending));
    if (!appended) {
        return appended;
    }

    impl_->lastSubmittedFence = signalFence;
    return {};
}
