#include <AeroRender/RenderDevice.hpp>
#include <AeroRender/RenderTarget.hpp>
#include <Aero/Diagnostics.hpp>

namespace Aero {

namespace {

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotInitialized, message);
}

class HeadlessTexture final : public Texture {
public:
    HeadlessTexture(uint32_t width, uint32_t height) noexcept
        : width_(width), height_(height) {}
    ~HeadlessTexture() override = default;

    uint32_t GetWidth() const noexcept override { return width_; }
    uint32_t GetHeight() const noexcept override { return height_; }
    bool HasMipMaps() const noexcept override { return false; }
    bool IsInverted() const noexcept override { return false; }
    bool HasAlpha() const noexcept override { return true; }

private:
    uint32_t width_ = 0U;
    uint32_t height_ = 0U;
};

class HeadlessRenderTarget final : public RenderTarget {
public:
    HeadlessRenderTarget(Ref<RenderDevice> device, uint32_t width, uint32_t height) noexcept
        : texture_(Base::MakeRef<HeadlessTexture>(width, height)),
          width_(width), height_(height) {
        kind_ = RenderTargetKind::Embedded;
        state_ = RenderTargetState::Ready;
        device_ = std::move(device);
    }
    ~HeadlessRenderTarget() override = default;

    Texture* GetTexture() noexcept override { return texture_.Get(); }

    uint32_t Width() const noexcept { return width_; }
    uint32_t Height() const noexcept { return height_; }

protected:
    Result<void> ResizeBackend(uint32_t width, uint32_t height) noexcept override {
        width_ = width;
        height_ = height;
        texture_ = Base::MakeRef<HeadlessTexture>(width, height);
        return {};
    }

private:
    Ref<HeadlessTexture> texture_;
    uint32_t width_ = 0U;
    uint32_t height_ = 0U;
};

class HeadlessRenderDevice final : public RenderDevice {
public:
    explicit HeadlessRenderDevice(Base::IAllocator* allocator) noexcept
        : allocator_(allocator) {
        backend_ = RenderBackendKind::Headless;
    }
    ~HeadlessRenderDevice() override = default;

    const DeviceCaps& GetCaps() const noexcept override {
        static DeviceCaps caps{};
        return caps;
    }

    Ref<RenderTarget> CreateRenderTarget(
        const char* label, uint32_t width, uint32_t height,
        uint32_t sampleCount, bool needsStencil) noexcept override {
        static_cast<void>(label);
        static_cast<void>(sampleCount);
        static_cast<void>(needsStencil);
        Ref<RenderDevice> self = Ref<RenderDevice>::FromBorrowed(*this);
        return Base::MakeRef<HeadlessRenderTarget>(std::move(self), width, height);
    }

    Ref<RenderTarget> CloneRenderTarget(
        const char* label, RenderTarget* surface) noexcept override {
        static_cast<void>(label);
        uint32_t w = surface != nullptr ? static_cast<HeadlessRenderTarget*>(surface)->Width() : 800U;
        uint32_t h = surface != nullptr ? static_cast<HeadlessRenderTarget*>(surface)->Height() : 600U;
        return CreateRenderTarget(label, w, h, 1, false);
    }

    Ref<Texture> CreateTexture(
        const char* label, uint32_t width, uint32_t height,
        uint32_t numLevels, TextureFormat::Enum format, const void** data) noexcept override {
        static_cast<void>(label);
        static_cast<void>(numLevels);
        static_cast<void>(format);
        static_cast<void>(data);
        return Base::MakeRef<HeadlessTexture>(width, height);
    }

    void UpdateTexture(
        Texture* texture, uint32_t level, uint32_t x, uint32_t y,
        uint32_t width, uint32_t height, const void* data) noexcept override {
        static_cast<void>(texture);
        static_cast<void>(level);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(width);
        static_cast<void>(height);
        static_cast<void>(data);
    }

    void BeginOffscreenRender() noexcept override {}
    void EndOffscreenRender() noexcept override {}
    void BeginOnscreenRender() noexcept override {}
    void EndOnscreenRender() noexcept override {}
    void SetRenderTarget(RenderTarget* surface) noexcept override { static_cast<void>(surface); }
    void BeginTile(RenderTarget* surface, const Tile& tile) noexcept override {
        static_cast<void>(surface);
        static_cast<void>(tile);
    }
    void EndTile(RenderTarget* surface) noexcept override { static_cast<void>(surface); }
    void ResolveRenderTarget(
        RenderTarget* surface, const Tile* tiles, uint32_t numTiles) noexcept override {
        static_cast<void>(surface);
        static_cast<void>(tiles);
        static_cast<void>(numTiles);
    }

    void* MapVertices(uint32_t bytes) noexcept override {
        static_cast<void>(bytes);
        return dummyVertices_;
    }
    void UnmapVertices() noexcept override {}
    void* MapIndices(uint32_t bytes) noexcept override {
        static_cast<void>(bytes);
        return dummyIndices_;
    }
    void UnmapIndices() noexcept override {}

    void DrawBatch(const Batch& batch) noexcept override {
        static_cast<void>(batch);
    }

private:
    Base::IAllocator* allocator_ = nullptr;
    alignas(16) uint8_t dummyVertices_[DYNAMIC_VB_SIZE]{};
    alignas(16) uint8_t dummyIndices_[DYNAMIC_IB_SIZE]{};
};

} // namespace

RenderDevice::~RenderDevice() noexcept = default;

void RenderDevice::NotifyDeviceLost() noexcept {
    if (state_ != RenderDeviceState::Ready) return;
    state_ = RenderDeviceState::DeviceLost;
    ++generation_;
    NotifyBackendDeviceLost();
}

Result<void> RenderDevice::Restore() noexcept {
    if (state_ != RenderDeviceState::DeviceLost) {
        return InvalidState("Only a lost render device can be restored");
    }
    Result<void> result = RestoreBackendDevice();
    if (result) {
        state_ = RenderDeviceState::Ready;
    } else {
        state_ = RenderDeviceState::Failed;
    }
    return result;
}

Result<void> RenderDevice::WaitIdle(std::uint32_t timeoutMilliseconds) noexcept {
    return WaitBackendIdle(timeoutMilliseconds);
}

void RenderDevice::BeginUpdatingTextures() noexcept {}

void RenderDevice::EndUpdatingTextures(Texture** textures, uint32_t count) noexcept {
    static_cast<void>(textures);
    static_cast<void>(count);
}


bool RenderDevice::IsValidState(Shader shader, RenderState state) noexcept {
    return IsValidBlendMode(shader, static_cast<RenderBlendMode::Enum>(state.f.blendMode)) &&
           IsValidStencilMode(shader, static_cast<StencilMode::Enum>(state.f.stencilMode)) &&
           IsValidColorEnable(shader, state.f.colorEnable != 0) &&
           IsValidWireframe(shader, state.f.wireframe != 0);
}

bool RenderDevice::IsValidBlendMode(Shader shader, RenderBlendMode::Enum blendMode) noexcept {
    if (shader == Shader::Clear) return blendMode == RenderBlendMode::Src;
    if (shader == Shader::Mask) return blendMode == RenderBlendMode::Src;
    return blendMode < RenderBlendMode::Count;
}

bool RenderDevice::IsValidStencilMode(Shader shader, StencilMode::Enum stencilMode) noexcept {
    if (shader == Shader::Clear) return stencilMode == StencilMode::Disabled;
    return stencilMode < StencilMode::Count;
}

bool RenderDevice::IsValidColorEnable(Shader shader, bool colorEnable) noexcept {
    if (shader == Shader::Mask) return !colorEnable;
    return true;
}

bool RenderDevice::IsValidWireframe(Shader shader, bool wireframe) noexcept {
    if (wireframe && (shader == Shader::Clear || shader == Shader::Mask)) return false;
    return true;
}

namespace Render {

Base::Result<Base::Ref<Aero::RenderDevice>> CreateHeadlessRenderDevice(
    Base::IAllocator* allocator) noexcept {
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    Base::Ref<HeadlessRenderDevice> made =
        Base::MakeRefWithAllocator<HeadlessRenderDevice>(selected, &selected);
    if (!made) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM");
    return Base::Ref<Aero::RenderDevice>(std::move(made));
}

} // namespace Render

RenderTarget::~RenderTarget() noexcept = default;

Result<void> RenderTarget::Resize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    if (!device_) return NotInitialized("Render target is not initialized");
    if (width == 0U || height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render target dimensions must be nonzero");
    }
    if (device_->State() != RenderDeviceState::Ready ||
        state_ != RenderTargetState::Ready) {
        return InvalidState("Render target cannot resize in its current state");
    }
    Result<void> idle = device_->WaitIdle();
    if (!idle) return idle.GetStatus();
    return ResizeBackend(width, height);
}

void RenderTarget::NotifyLost() noexcept {
    if (!device_ || device_->State() != RenderDeviceState::Ready ||
        state_ != RenderTargetState::Ready) {
        return;
    }
    state_ = RenderTargetState::Lost;
    NotifyBackendLost();
}

Result<void> RenderTarget::Restore() noexcept {
    if (!device_) return NotInitialized("Render target is not initialized");
    if (device_->State() != RenderDeviceState::Ready ||
        state_ != RenderTargetState::Lost) {
        return InvalidState("Only a lost render target can be restored");
    }
    Result<void> restored = RestoreBackend();
    if (restored) {
        state_ = RenderTargetState::Ready;
    }
    return restored;
}

} // namespace Aero

namespace Aero::Diagnostics {

RenderDeviceStatistics GetRenderDeviceStatistics(
    const Aero::RenderDevice& device) noexcept {
    RenderDeviceStatistics stats;
    stats.generation = device.Generation();
    return stats;
}

RenderFrameStatistics GetLastRenderFrameStatistics(
    const Aero::RenderDevice& device) noexcept {
    static_cast<void>(device);
    return {};
}

} // namespace Aero::Diagnostics
