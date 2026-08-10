#pragma once

#include "DisplayList.hpp"

#include "FrameEncoder.hpp"
#include "render/RenderResources.hpp"
#include "render/RenderDeviceState.hpp"

#include <cmath>
#include <limits>

namespace Aero::Render {

class MeshGpuResources {
public:
    MeshGpuResources(
        Aero::Render::RenderDeviceBase& device,
        UiFrameEncoder& encoder,
        std::uint64_t generation,
        Base::IAllocator& allocator) noexcept
        : device_(&device),
          encoder_(&encoder),
          allocator_(&allocator),
          resources_(&allocator) {
        table_.generation = generation;
        table_.context = this;
        table_.create =
            [](void* context,
               Base::Span<const Aero::Point> vertices,
               Base::Span<const std::uint32_t> indices) noexcept
                -> Base::Result<
                    Render::RenderMeshId> {
                return static_cast<MeshGpuResources*>(
                    context)->Create(vertices, indices);
            };
        table_.release =
            [](void* context,
               Render::RenderMeshId mesh) noexcept {
                static_cast<MeshGpuResources*>(
                    context)->Release(mesh);
            };
    }

    ~MeshGpuResources() noexcept {
        Shutdown();
    }

    Aero::Render::MeshResources&
    Table() noexcept {
        return table_;
    }

    void CollectRetired(
        Base::Span<const Render::RenderCommand> liveCommands) noexcept {
        for (std::uint32_t index = 0U;
             index < resources_.Size();) {
            Resource& resource = resources_[index];
            if (!resource.retired) {
                ++index;
                continue;
            }
            bool referenced = false;
            for (const Render::RenderCommand& command : liveCommands) {
                if (command.kind == Render::RenderCommandKind::DrawMesh &&
                    command.mesh == resource.id) {
                    referenced = true;
                    break;
                }
            }
            if (referenced) {
                ++index;
                continue;
            }
            Destroy(resource);
            for (std::uint32_t next = index + 1U;
                 next < resources_.Size(); ++next) {
                resources_[next - 1U] = resources_[next];
            }
            resources_.PopBack();
        }
    }

    void Shutdown() noexcept {
        for (Resource& resource : resources_) {
            Destroy(resource);
        }
        resources_.Clear();
    }

private:
    struct Vertex {
        float x = 0.0F;
        float y = 0.0F;
        float red = 1.0F;
        float green = 1.0F;
        float blue = 1.0F;
        float alpha = 1.0F;
        float coverage = 1.0F;
    };

    struct Weld {
        std::int64_t x = 0;
        std::int64_t y = 0;
        float normalX = 0.0F;
        float normalY = 0.0F;
    };

    struct BoundaryEdge {
        std::uint32_t a = 0U;
        std::uint32_t b = 0U;
        std::uint32_t weldA = 0U;
        std::uint32_t weldB = 0U;
        float normalX = 0.0F;
        float normalY = 0.0F;
        std::uint32_t uses = 0U;
    };

    struct Resource {
        Render::RenderMeshId id =
            Render::InvalidRenderMeshId;
        Graphics::ResourceHandle vertexBuffer;
        Graphics::ResourceHandle indexBuffer;
        bool retired = false;
    };

    template<class T>
    static Base::Span<const std::uint8_t> AsBytes(
        Base::Span<const T> values) noexcept {
        return {
            reinterpret_cast<const std::uint8_t*>(
                values.Data()),
            values.Size() *
                static_cast<std::uint32_t>(sizeof(T))};
    }

    // This mirrors the original renderer's path-AA strategy: retain the
    // filled mesh, then append a transparent one-pixel coverage fringe only
    // along its topological boundary. It is independent of MSAA and avoids
    // seams on internal tessellation edges.
    Base::Result<void> BuildAntialiasedGeometry(
        Base::Span<const Aero::Point> points,
        Base::Span<const std::uint32_t> sourceIndices,
        Base::Vector<Vertex>& vertices,
        Base::Vector<std::uint32_t>& outputIndices) noexcept {
        Base::Vector<Weld> welds(allocator_);
        Base::Vector<BoundaryEdge> edges(allocator_);
        Base::Vector<std::uint32_t> weldIds(allocator_);
        Base::Result<void> reserved = vertices.Reserve(points.Size());
        if (!reserved) return reserved.GetStatus();
        reserved = outputIndices.Reserve(sourceIndices.Size());
        if (!reserved) return reserved.GetStatus();
        reserved = weldIds.Reserve(points.Size());
        if (!reserved) return reserved.GetStatus();

        constexpr double Quantization = 100000.0;
        auto findWeld = [&](const Aero::Point& point)
            noexcept -> Base::Result<std::uint32_t> {
            const std::int64_t x = static_cast<std::int64_t>(
                std::llround(point.x * Quantization));
            const std::int64_t y = static_cast<std::int64_t>(
                std::llround(point.y * Quantization));
            for (std::uint32_t index = 0U; index < welds.Size(); ++index) {
                if (welds[index].x == x && welds[index].y == y) {
                    return index;
                }
            }
            Base::Result<Weld*> added = welds.EmplaceBack();
            if (!added) return added.GetStatus();
            added.Value()->x = x;
            added.Value()->y = y;
            return welds.Size() - 1U;
        };

        for (const Aero::Point point : points) {
            Base::Result<std::uint32_t> weld = findWeld(point);
            if (!weld) return weld.GetStatus();
            Base::Result<void> added = weldIds.PushBack(weld.Value());
            if (!added) return added.GetStatus();
            added = vertices.PushBack({
                static_cast<float>(point.x), static_cast<float>(point.y),
                1.0F, 1.0F, 1.0F, 1.0F, 1.0F});
            if (!added) return added.GetStatus();
        }
        Base::Result<void> copied = outputIndices.Append(sourceIndices);
        if (!copied) return copied.GetStatus();

        for (std::uint32_t index = 0U;
             index < sourceIndices.Size(); index += 3U) {
            const std::uint32_t triangle[] = {
                sourceIndices[index], sourceIndices[index + 1U],
                sourceIndices[index + 2U]};
            const Aero::Point& a = points[triangle[0]];
            const Aero::Point& b = points[triangle[1]];
            const Aero::Point& c = points[triangle[2]];
            const double area = (b.x - a.x) * (c.y - a.y) -
                (b.y - a.y) * (c.x - a.x);
            if (std::abs(area) <= 1.0e-12) continue;
            const bool clockwiseInScreenSpace = area > 0.0;
            for (std::uint32_t edgeIndex = 0U;
                 edgeIndex < 3U; ++edgeIndex) {
                const std::uint32_t first = triangle[edgeIndex];
                const std::uint32_t second = triangle[
                    (edgeIndex + 1U) % 3U];
                const std::uint32_t weldFirst = weldIds[first];
                const std::uint32_t weldSecond = weldIds[second];
                const std::uint32_t lower = std::min(weldFirst, weldSecond);
                const std::uint32_t upper = std::max(weldFirst, weldSecond);
                BoundaryEdge* existing = nullptr;
                for (BoundaryEdge& edge : edges) {
                    if (edge.weldA == lower && edge.weldB == upper) {
                        existing = &edge;
                        break;
                    }
                }
                if (existing != nullptr) {
                    ++existing->uses;
                    continue;
                }
                const Aero::Point& start = points[first];
                const Aero::Point& end = points[second];
                const double dx = end.x - start.x;
                const double dy = end.y - start.y;
                const double length = std::hypot(dx, dy);
                if (length <= 1.0e-12) continue;
                Base::Result<BoundaryEdge*> added = edges.EmplaceBack();
                if (!added) return added.GetStatus();
                BoundaryEdge& edge = *added.Value();
                edge.a = first;
                edge.b = second;
                edge.weldA = lower;
                edge.weldB = upper;
                edge.normalX = static_cast<float>(
                    (clockwiseInScreenSpace ? dy : -dy) / length);
                edge.normalY = static_cast<float>(
                    (clockwiseInScreenSpace ? -dx : dx) / length);
                edge.uses = 1U;
            }
        }

        std::uint32_t boundaryCount = 0U;
        for (const BoundaryEdge& edge : edges) {
            if (edge.uses != 1U) continue;
            ++boundaryCount;
            welds[edge.weldA].normalX += edge.normalX;
            welds[edge.weldA].normalY += edge.normalY;
            welds[edge.weldB].normalX += edge.normalX;
            welds[edge.weldB].normalY += edge.normalY;
        }
        if (boundaryCount == 0U) return {};
        for (Weld& weld : welds) {
            const float length = std::hypot(weld.normalX, weld.normalY);
            if (length > 1.0e-6F) {
                weld.normalX /= length;
                weld.normalY /= length;
            }
        }
        Base::Result<void> expanded = vertices.Reserve(
            vertices.Size() + boundaryCount * 2U);
        if (!expanded) return expanded.GetStatus();
        expanded = outputIndices.Reserve(
            outputIndices.Size() + boundaryCount * 6U);
        if (!expanded) return expanded.GetStatus();
        constexpr float FringeWidth = 0.5F;
        for (const BoundaryEdge& edge : edges) {
            if (edge.uses != 1U) continue;
            const Weld& a = welds[weldIds[edge.a]];
            const Weld& b = welds[weldIds[edge.b]];
            const std::uint32_t fringeA = vertices.Size();
            Base::Result<void> added = vertices.PushBack({
                static_cast<float>(points[edge.a].x) + a.normalX * FringeWidth,
                static_cast<float>(points[edge.a].y) + a.normalY * FringeWidth,
                1.0F, 1.0F, 1.0F, 1.0F, 0.0F});
            if (!added) return added.GetStatus();
            const std::uint32_t fringeB = vertices.Size();
            added = vertices.PushBack({
                static_cast<float>(points[edge.b].x) + b.normalX * FringeWidth,
                static_cast<float>(points[edge.b].y) + b.normalY * FringeWidth,
                1.0F, 1.0F, 1.0F, 1.0F, 0.0F});
            if (!added) return added.GetStatus();
            const std::uint32_t fringeIndices[] = {
                edge.a, edge.b, fringeB, edge.a, fringeB, fringeA};
            added = outputIndices.Append({fringeIndices, 6U});
            if (!added) return added.GetStatus();
        }
        return {};
    }

    Base::Result<Render::RenderMeshId>
    Create(
        Base::Span<const Aero::Point> points,
        Base::Span<const std::uint32_t> indices) noexcept {
        if (device_ == nullptr ||
            encoder_ == nullptr ||
            points.Empty() ||
            indices.Empty() ||
            indices.Size() % 3U != 0U) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Mesh service requires triangle-list geometry");
        }
        if (points.Size() >
                UINT32_MAX /
                    static_cast<std::uint32_t>(
                        sizeof(Vertex)) ||
            indices.Size() >
                UINT32_MAX /
                    static_cast<std::uint32_t>(
                        sizeof(std::uint32_t))) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Mesh service upload exceeds 32-bit buffer limits");
        }
        for (std::uint32_t index : indices) {
            if (index >= points.Size()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Mesh service index is outside the vertex buffer");
            }
        }

        for (const Aero::Point point : points) {
            if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
                std::abs(point.x) > std::numeric_limits<float>::max() ||
                std::abs(point.y) > std::numeric_limits<float>::max()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Mesh service vertex must be finite float geometry");
            }
        }
        Base::Vector<Vertex> vertices(allocator_);
        Base::Vector<std::uint32_t> antialiasedIndices(allocator_);
        Base::Result<void> built = BuildAntialiasedGeometry(
            points, indices, vertices, antialiasedIndices);
        if (!built) return built.GetStatus();

        Resource resource;
        Graphics::BufferDescriptor vertexDescriptor;
        vertexDescriptor.sizeBytes =
            static_cast<std::uint64_t>(
                vertices.Size()) *
            sizeof(Vertex);
        vertexDescriptor.usage =
            Graphics::BufferUsage::Vertex;
        Base::Result<Graphics::ResourceHandle> vertex =
            device_->CreateBuffer(vertexDescriptor);
        if (!vertex) return vertex.GetStatus();
        resource.vertexBuffer = vertex.Value();

        Graphics::BufferDescriptor indexDescriptor;
        indexDescriptor.sizeBytes =
            static_cast<std::uint64_t>(
                antialiasedIndices.Size()) *
            sizeof(std::uint32_t);
        indexDescriptor.usage =
            Graphics::BufferUsage::Index;
        Base::Result<Graphics::ResourceHandle> index =
            device_->CreateBuffer(indexDescriptor);
        if (!index) {
            static_cast<void>(
                device_->DestroyResource(
                    resource.vertexBuffer,
                    device_->LastSubmittedFence()));
            return index.GetStatus();
        }
        resource.indexBuffer = index.Value();

        ::Aero::Render::UiDrawContext encoder(*device_, allocator_);
        Base::Result<void> uploaded =
            encoder.UploadBuffer(
                resource.vertexBuffer, 0U,
                AsBytes(Base::Span<const Vertex>{
                    vertices.Data(),
                    vertices.Size()}));
        if (uploaded) {
            uploaded = encoder.UploadBuffer(
                resource.indexBuffer, 0U,
                AsBytes(Base::Span<
                    const std::uint32_t>{
                        antialiasedIndices.Data(),
                        antialiasedIndices.Size()}));
        }
        if (!uploaded) {
            DestroyBuffers(resource);
            return uploaded.GetStatus();
        }
        Base::Result<Graphics::FenceValue> submitted =
            encoder.Finish();
        if (!submitted) {
            DestroyBuffers(resource);
            return submitted.GetStatus();
        }

        if (nextMesh_ ==
            Render::InvalidRenderMeshId) {
            DestroyBuffers(
                resource, submitted.Value());
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Mesh service ID space is exhausted");
        }
        resource.id = nextMesh_++;
        Base::Result<void> registered =
            encoder_->RegisterMesh(
                resource.id,
                resource.vertexBuffer,
                resource.indexBuffer,
                antialiasedIndices.Size(),
                Graphics::IndexType::UInt32);
        if (!registered) {
            DestroyBuffers(
                resource, submitted.Value());
            return registered.GetStatus();
        }
        Base::Result<void> stored =
            resources_.PushBack(resource);
        if (!stored) {
            static_cast<void>(
                encoder_->UnregisterMesh(
                    resource.id));
            DestroyBuffers(
                resource, submitted.Value());
            return stored.GetStatus();
        }
        return resource.id;
    }

    void Release(
        Render::RenderMeshId mesh) noexcept {
        for (Resource& resource : resources_) {
            if (resource.id == mesh) {
                resource.retired = true;
                return;
            }
        }
    }

    void Destroy(Resource& resource) noexcept {
        if (encoder_ != nullptr &&
            resource.id !=
                Render::InvalidRenderMeshId) {
            static_cast<void>(
                encoder_->UnregisterMesh(
                    resource.id));
        }
        DestroyBuffers(resource);
        resource.id =
            Render::InvalidRenderMeshId;
    }

    void DestroyBuffers(
        Resource& resource,
        Graphics::FenceValue fence = 0U) noexcept {
        if (device_ == nullptr) return;
        if (fence == 0U) {
            fence = device_->LastSubmittedFence();
        }
        if (device_->IsAlive(
                resource.vertexBuffer)) {
            static_cast<void>(
                device_->DestroyResource(
                    resource.vertexBuffer,
                    fence));
        }
        if (device_->IsAlive(
                resource.indexBuffer)) {
            static_cast<void>(
                device_->DestroyResource(
                    resource.indexBuffer,
                    fence));
        }
        resource.vertexBuffer = {};
        resource.indexBuffer = {};
    }

    Aero::Render::RenderDeviceBase* device_ = nullptr;
    UiFrameEncoder* encoder_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<Resource> resources_;
    Render::RenderMeshId nextMesh_ =
        UINT64_C(1) << 48U;
    Aero::Render::MeshResources table_;
};

} // namespace Aero::Render
