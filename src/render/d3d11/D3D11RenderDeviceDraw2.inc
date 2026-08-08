            if (state_->inRenderPass ||
                !Base::IsValidRect(renderPass.renderArea) ||
                renderPass.colorAttachmentCount > MaxColorAttachments) {
                return failSubmission(
                    InvalidState("D3D11 render-pass begin is invalid"));
            }

            ID3D11RenderTargetView* renderTargets[MaxColorAttachments]{};
            ID3D11Texture2D* colorTextures[MaxColorAttachments]{};
            for (std::uint32_t index = 0U;
                 index < renderPass.colorAttachmentCount;
                 ++index) {
                const ColorAttachmentDescriptor& attachment =
                    renderPass.colorAttachments[index];
                D3D11RenderDeviceState::ResourceRecord* record = state_->Find(attachment.target);
                if (record == nullptr || record->renderTargetView == nullptr) {
                    return failSubmission(
                        InvalidArgument("D3D11 color attachment is not configured"));
                }
                renderTargets[index] = record->renderTargetView;
                colorTextures[index] = record->texture;
            }

            ID3D11DepthStencilView* depthStencil = nullptr;
            ID3D11Texture2D* depthStencilTexture = nullptr;
            if (renderPass.hasDepthStencil) {
                D3D11RenderDeviceState::ResourceRecord* record = state_->Find(
                    renderPass.depthStencil.target);
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
            state_->ClearShaderResourceBindings();
            state_->submissionState.ResetPixelShaderResources();
            state_->context->OMSetRenderTargets(
                renderPass.colorAttachmentCount,
                renderTargets,
                depthStencil);

            for (std::uint32_t index = 0U;
                 index < renderPass.colorAttachmentCount;
                 ++index) {
                const ColorAttachmentDescriptor& attachment =
                    renderPass.colorAttachments[index];
                if (attachment.load == LoadOperation::Clear) {
                    const FLOAT color[4] = {
                        attachment.clearColor.red,
                        attachment.clearColor.green,
                        attachment.clearColor.blue,
                        attachment.clearColor.alpha};
                    state_->context->ClearRenderTargetView(
                        renderTargets[index], color);
                }
            }
            if (renderPass.hasDepthStencil) {
                UINT clearFlags = 0U;
                if (renderPass.depthStencil.depthLoad ==
                    LoadOperation::Clear) {
                    clearFlags |= D3D11_CLEAR_DEPTH;
                }
                if (renderPass.depthStencil.stencilLoad ==
                    LoadOperation::Clear) {
                    clearFlags |= D3D11_CLEAR_STENCIL;
                }
                if (clearFlags != 0U) {
                    state_->context->ClearDepthStencilView(
                        depthStencil,
                        clearFlags,
                        renderPass.depthStencil.clearDepth,
                        static_cast<UINT8>(
                            renderPass.depthStencil.clearStencil & 0xFFU));
                }
            }

            for (std::uint32_t index = 0U; index < MaxColorAttachments; ++index) {
                state_->activeColorTextures[index] = colorTextures[index];
            }
            state_->activeDepthStencilTexture = depthStencilTexture;
            D3D11_VIEWPORT viewport{};
            viewport.TopLeftX = static_cast<FLOAT>(renderPass.renderArea.x);
            viewport.TopLeftY = static_cast<FLOAT>(renderPass.renderArea.y);
            viewport.Width = static_cast<FLOAT>(renderPass.renderArea.width);
            viewport.Height = static_cast<FLOAT>(renderPass.renderArea.height);
            viewport.MinDepth = 0.0F;
            viewport.MaxDepth = 1.0F;
            const D3D11_VIEWPORT& cachedViewport = state_->submissionState.viewport;
            if (!state_->submissionState.hasViewport ||
                cachedViewport.TopLeftX != viewport.TopLeftX ||
                cachedViewport.TopLeftY != viewport.TopLeftY ||
                cachedViewport.Width != viewport.Width ||
                cachedViewport.Height != viewport.Height ||
                cachedViewport.MinDepth != viewport.MinDepth ||
                cachedViewport.MaxDepth != viewport.MaxDepth) {
                state_->context->RSSetViewports(1U, &viewport);
                state_->submissionState.viewport = viewport;
                state_->submissionState.hasViewport = true;
            }
            state_->inRenderPass = true;
            return {};
        };

        const auto endRenderPass = [&]() noexcept -> Base::Result<void> {
            if (!state_->inRenderPass) {
                return failSubmission(
                    InvalidState("D3D11 render-pass stack underflow"));
            }
            state_->context->OMSetRenderTargets(0U, nullptr, nullptr);
            state_->ClearShaderResourceBindings();
            state_->submissionState.ResetPixelShaderResources();
            state_->ClearActiveAttachments();
            state_->inRenderPass = false;
            return {};
        };

        const auto bindPipeline = [&](ResourceHandle pipelineHandle) noexcept
            -> Base::Result<void> {
            if (!state_->inRenderPass) {
                return failSubmission(
                    InvalidState("D3D11 pipeline binding requires a render pass"));
            }
            D3D11RenderDeviceState::ResourceRecord* record = state_->Find(pipelineHandle);
            if (record == nullptr || !record->configured ||
                record->vertexShader == nullptr || record->pixelShader == nullptr) {
                return failSubmission(
                    InvalidArgument("D3D11 pipeline is not configured"));
            }
            if (state_->submissionState.pipeline != record->handle) {
                state_->context->VSSetShader(record->vertexShader, nullptr, 0U);
                state_->context->PSSetShader(record->pixelShader, nullptr, 0U);
                state_->context->IASetInputLayout(record->inputLayout);
                state_->context->IASetPrimitiveTopology(record->topology);
                state_->context->RSSetState(record->rasterizerState);
                const FLOAT blendFactor[4] = {0.0F, 0.0F, 0.0F, 0.0F};
                state_->context->OMSetBlendState(
                    record->blendState, blendFactor, UINT_MAX);
                state_->context->OMSetDepthStencilState(
                    record->depthStencilState, 0U);
                state_->submissionState.pipeline = record->handle;
            }
            state_->currentPipeline = record->handle;
            return {};
        };

        const auto bindVertexBuffer = [&](std::uint32_t slot,
                                          ResourceHandle bufferHandle,
                                          std::uint64_t offsetBytes) noexcept
            -> Base::Result<void> {
            if (!state_->inRenderPass || !state_->currentPipeline.IsValid()) {
                return failSubmission(
                    InvalidState("D3D11 vertex-buffer binding requires a pipeline"));
            }
            D3D11RenderDeviceState::ResourceRecord* pipeline = state_->Find(state_->currentPipeline);
            D3D11RenderDeviceState::ResourceRecord* buffer = state_->Find(bufferHandle);
            if (pipeline == nullptr || buffer == nullptr || buffer->buffer == nullptr ||
                buffer->baseDescriptor.buffer.usage != BufferUsage::Vertex ||
                slot >= pipeline->vertexLayout.bufferCount) {
                return failSubmission(
                    InvalidArgument("D3D11 vertex-buffer binding is invalid"));
            }
            UINT offset = 0U;
            if (!CheckedUint(offsetBytes, offset)) {
                return failSubmission(
                    OutOfRange("D3D11 vertex-buffer offset exceeds UINT range"));
            }
            const UINT stride = pipeline->vertexLayout.buffers[slot].stride;
            ID3D11Buffer* nativeBuffer = buffer->buffer;
            if (state_->submissionState.vertexBuffers[slot] != nativeBuffer ||
                state_->submissionState.vertexStrides[slot] != stride ||
                state_->submissionState.vertexOffsets[slot] != offset) {
                state_->context->IASetVertexBuffers(
                    slot, 1U, &nativeBuffer, &stride, &offset);
                state_->submissionState.vertexBuffers[slot] = nativeBuffer;
                state_->submissionState.vertexStrides[slot] = stride;
                state_->submissionState.vertexOffsets[slot] = offset;
            }
            return {};
        };

        const auto bindIndexBuffer = [&](ResourceHandle bufferHandle,
                                         std::uint64_t offsetBytes,
                                         IndexType indexType) noexcept
            -> Base::Result<void> {
            if (!state_->inRenderPass) {
                return failSubmission(
                    InvalidState("D3D11 index-buffer binding requires a render pass"));
            }
            D3D11RenderDeviceState::ResourceRecord* buffer = state_->Find(bufferHandle);
            if (buffer == nullptr || buffer->buffer == nullptr ||
