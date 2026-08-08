            const std::uint32_t tightRow = region.width * bytesPerPixel;
            if (region.bytesPerRow < tightRow ||
                region.height > UINT32_MAX / region.bytesPerRow ||
                command.uploadSize < region.height * region.bytesPerRow) {
                return failSubmission(
                    InvalidArgument("D3D11 texture upload payload is too small"));
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
            const Base::Span<const std::uint8_t> data = uploadSpan.Subspan(
                command.uploadOffset, command.uploadSize);
            impl_->context->UpdateSubresource(
                record->texture,
                subresource,
                &box,
                data.Data(),
                region.bytesPerRow,
                region.bytesPerRow * region.height);
            break;
        }
        case SubmissionStepKind::BeginRenderPass: {
            if (impl_->inRenderPass ||
                !Base::IsValidRect(command.renderPass.renderArea) ||
                command.renderPass.colorAttachmentCount > MaxColorAttachments) {
                return failSubmission(
                    InvalidState("D3D11 render-pass begin is invalid"));
            }

            ID3D11RenderTargetView* renderTargets[MaxColorAttachments]{};
            ID3D11Texture2D* colorTextures[MaxColorAttachments]{};
            for (std::uint32_t index = 0U;
                 index < command.renderPass.colorAttachmentCount;
                 ++index) {
                const ColorAttachmentDescriptor& attachment =
                    command.renderPass.colorAttachments[index];
                Impl::ResourceRecord* record = impl_->Find(attachment.target);
                if (record == nullptr || record->renderTargetView == nullptr) {
                    return failSubmission(
                        InvalidArgument("D3D11 color attachment is not configured"));
                }
                renderTargets[index] = record->renderTargetView;
                colorTextures[index] = record->texture;
            }

            ID3D11DepthStencilView* depthStencil = nullptr;
            ID3D11Texture2D* depthStencilTexture = nullptr;
            if (command.renderPass.hasDepthStencil) {
                Impl::ResourceRecord* record = impl_->Find(
                    command.renderPass.depthStencil.target);
                if (record == nullptr || record->depthStencilView == nullptr) {
                    return failSubmission(
                        InvalidArgument("D3D11 depth-stencil attachment is not configured"));
                }
                depthStencil = record->depthStencilView;
                depthStencilTexture = record->texture;
            }

            // Explicitly clear every shader-stage SRV before writing an attachment. This
            // keeps the D3D11 runtime from implicitly resolving RT/SRV hazards
            // (and keeps the debug layer clean when the host previously used
            // this immediate context).
            impl_->ClearShaderResourceBindings();
            impl_->submissionState.ResetPixelShaderResources();
            impl_->context->OMSetRenderTargets(
                command.renderPass.colorAttachmentCount,
                renderTargets,
                depthStencil);

            for (std::uint32_t index = 0U;
                 index < command.renderPass.colorAttachmentCount;
                 ++index) {
                const ColorAttachmentDescriptor& attachment =
                    command.renderPass.colorAttachments[index];
                if (attachment.load == LoadOperation::Clear) {
                    const FLOAT color[4] = {
                        attachment.clearColor.red,
                        attachment.clearColor.green,
                        attachment.clearColor.blue,
                        attachment.clearColor.alpha};
                    impl_->context->ClearRenderTargetView(
                        renderTargets[index], color);
                }
            }
            if (command.renderPass.hasDepthStencil) {
                UINT clearFlags = 0U;
                if (command.renderPass.depthStencil.depthLoad ==
                    LoadOperation::Clear) {
                    clearFlags |= D3D11_CLEAR_DEPTH;
                }
                if (command.renderPass.depthStencil.stencilLoad ==
                    LoadOperation::Clear) {
                    clearFlags |= D3D11_CLEAR_STENCIL;
                }
                if (clearFlags != 0U) {
                    impl_->context->ClearDepthStencilView(
                        depthStencil,
                        clearFlags,
                        command.renderPass.depthStencil.clearDepth,
                        static_cast<UINT8>(
                            command.renderPass.depthStencil.clearStencil & 0xFFU));
                }
            }

            for (std::uint32_t index = 0U; index < MaxColorAttachments; ++index) {
                impl_->activeColorTextures[index] = colorTextures[index];
            }
            impl_->activeDepthStencilTexture = depthStencilTexture;
            D3D11_VIEWPORT viewport{};
            viewport.TopLeftX = static_cast<FLOAT>(command.renderPass.renderArea.x);
            viewport.TopLeftY = static_cast<FLOAT>(command.renderPass.renderArea.y);
            viewport.Width = static_cast<FLOAT>(command.renderPass.renderArea.width);
            viewport.Height = static_cast<FLOAT>(command.renderPass.renderArea.height);
            viewport.MinDepth = 0.0F;
            viewport.MaxDepth = 1.0F;
            const D3D11_VIEWPORT& cachedViewport = impl_->submissionState.viewport;
            if (!impl_->submissionState.hasViewport ||
                cachedViewport.TopLeftX != viewport.TopLeftX ||
                cachedViewport.TopLeftY != viewport.TopLeftY ||
                cachedViewport.Width != viewport.Width ||
                cachedViewport.Height != viewport.Height ||
                cachedViewport.MinDepth != viewport.MinDepth ||
                cachedViewport.MaxDepth != viewport.MaxDepth) {
                impl_->context->RSSetViewports(1U, &viewport);
                impl_->submissionState.viewport = viewport;
                impl_->submissionState.hasViewport = true;
            }
            impl_->inRenderPass = true;
            break;
        }
        case SubmissionStepKind::EndRenderPass: {
            if (!impl_->inRenderPass) {
                return failSubmission(
                    InvalidState("D3D11 render-pass stack underflow"));
            }
            impl_->context->OMSetRenderTargets(0U, nullptr, nullptr);
            impl_->ClearShaderResourceBindings();
            impl_->submissionState.ResetPixelShaderResources();
            impl_->ClearActiveAttachments();
            impl_->inRenderPass = false;
            break;
        }
        case SubmissionStepKind::BindPipeline: {
            if (!impl_->inRenderPass) {
                return failSubmission(
                    InvalidState("D3D11 pipeline binding requires a render pass"));
            }
            Impl::ResourceRecord* record = impl_->Find(command.resource0);
            if (record == nullptr || !record->configured ||
                record->vertexShader == nullptr || record->pixelShader == nullptr) {
                return failSubmission(
                    InvalidArgument("D3D11 pipeline is not configured"));
            }
            if (impl_->submissionState.pipeline != record->handle) {
                impl_->context->VSSetShader(record->vertexShader, nullptr, 0U);
                impl_->context->PSSetShader(record->pixelShader, nullptr, 0U);
                impl_->context->IASetInputLayout(record->inputLayout);
                impl_->context->IASetPrimitiveTopology(record->topology);
                impl_->context->RSSetState(record->rasterizerState);
                const FLOAT blendFactor[4] = {0.0F, 0.0F, 0.0F, 0.0F};
                impl_->context->OMSetBlendState(
                    record->blendState, blendFactor, UINT_MAX);
                impl_->context->OMSetDepthStencilState(
                    record->depthStencilState, 0U);
                impl_->submissionState.pipeline = record->handle;
            }
            impl_->currentPipeline = record->handle;
            break;
        }
        case SubmissionStepKind::BindVertexBuffer: {
            if (!impl_->inRenderPass || !impl_->currentPipeline.IsValid()) {
                return failSubmission(
                    InvalidState("D3D11 vertex-buffer binding requires a pipeline"));
            }
            Impl::ResourceRecord* pipeline = impl_->Find(impl_->currentPipeline);
            Impl::ResourceRecord* buffer = impl_->Find(command.resource0);
            if (pipeline == nullptr || buffer == nullptr || buffer->buffer == nullptr ||
                buffer->baseDescriptor.buffer.usage != BufferUsage::Vertex ||
                command.slot >= pipeline->vertexLayout.bufferCount) {
                return failSubmission(
                    InvalidArgument("D3D11 vertex-buffer binding is invalid"));
            }
            UINT offset = 0U;
            if (!CheckedUint(command.resourceOffset, offset)) {
                return failSubmission(
                    OutOfRange("D3D11 vertex-buffer offset exceeds UINT range"));
            }
            const UINT stride = pipeline->vertexLayout.buffers[command.slot].stride;
            ID3D11Buffer* nativeBuffer = buffer->buffer;
            if (impl_->submissionState.vertexBuffers[command.slot] != nativeBuffer ||
                impl_->submissionState.vertexStrides[command.slot] != stride ||
                impl_->submissionState.vertexOffsets[command.slot] != offset) {
                impl_->context->IASetVertexBuffers(
                    command.slot, 1U, &nativeBuffer, &stride, &offset);
                impl_->submissionState.vertexBuffers[command.slot] = nativeBuffer;
                impl_->submissionState.vertexStrides[command.slot] = stride;
                impl_->submissionState.vertexOffsets[command.slot] = offset;
            }
            break;
        }
        case SubmissionStepKind::BindIndexBuffer: {
            if (!impl_->inRenderPass) {
                return failSubmission(
                    InvalidState("D3D11 index-buffer binding requires a render pass"));
            }
            Impl::ResourceRecord* buffer = impl_->Find(command.resource0);
            if (buffer == nullptr || buffer->buffer == nullptr ||
