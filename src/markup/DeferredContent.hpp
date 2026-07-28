#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>

namespace Aero::Markup {

struct DeferredContentEdge final {
    Base::Object* owner = nullptr;
    Base::Object* parent = nullptr;
    Base::Ref<Base::Object> child;
    Core::MetadataRuntime* runtime = nullptr;
    Core::MemberId member = Core::InvalidMemberId;
};

class DeferredContentPlan final {
public:
    Base::Result<void> Stage(
        Base::Object& owner,
        Base::Object& parent,
        const Base::Ref<Base::Object>& child,
        Core::MetadataRuntime& runtime,
        Core::MemberId member) noexcept;
    Base::Result<void> CopyForOwner(
        const Base::Object& owner,
        Base::Vector<DeferredContentEdge>& output) const noexcept;
    void ReleaseOwner(Base::Object& owner) noexcept;
    void ReleaseAll() noexcept;
    bool Empty() const noexcept { return edges_.Empty(); }

private:
    Base::Vector<DeferredContentEdge> edges_;
};

} // namespace Aero::Markup
