#pragma once

// Private template compiler used by ObjectWriter finalization.

#include "DeferredContent.hpp"

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Markup/Schema.hpp>

#include <cstdint>

namespace Aero::Markup::Detail {

class XamlVisualStateObject final : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        XamlVisualStateObject,
        Base::Object,
        "urn:aero",
        "VisualState")
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::Result<void> SetName(Base::StringView value) noexcept {
        return name_.TryAssign(value);
    }
    Base::StringView Name() const noexcept {
        return name_.View();
    }
    Base::Result<void> AddSetter(
        const Base::Ref<Base::Object>& value) noexcept {
        return setters_.TryPushBack(value);
    }
    void ClearSetters() noexcept {
        setters_.Clear();
    }
    Base::Span<const Base::Ref<Base::Object>>
    Setters() const noexcept {
        return {setters_.Data(), setters_.Size()};
    }

private:
    Base::String name_;
    Base::Vector<Base::Ref<Base::Object>> setters_;
};

class XamlVisualStateGroupObject final
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        XamlVisualStateGroupObject,
        Base::Object,
        "urn:aero",
        "VisualStateGroup")
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::Result<void> SetName(Base::StringView value) noexcept {
        return name_.TryAssign(value);
    }
    Base::StringView Name() const noexcept {
        return name_.View();
    }
    Base::Result<void> AddState(
        const Base::Ref<Base::Object>& value) noexcept {
        return states_.TryPushBack(value);
    }
    void ClearStates() noexcept {
        states_.Clear();
    }
    Base::Span<const Base::Ref<Base::Object>>
    States() const noexcept {
        return {states_.Data(), states_.Size()};
    }

private:
    Base::String name_;
    Base::Vector<Base::Ref<Base::Object>> states_;
};

struct TemplatePrototypeProperty final {
    Core::DependencyPropertyHandle property;
    Core::Value value;
};

struct TemplatePrototypeNode final {
    Core::TypeId type = Core::InvalidTypeId;
    Base::String name;
    std::uint32_t parent = UINT32_MAX;
    Core::MemberId contentMember = Core::InvalidMemberId;
    Base::Vector<TemplatePrototypeProperty> properties;
};

struct CompiledTemplateBlueprint final {
    Core::MetadataRuntime* runtime = nullptr;
    Base::Vector<TemplatePrototypeNode> nodes;
    std::uint32_t contentPresenter = UINT32_MAX;
};

struct CompiledTemplateDefinition final {
    Core::TypeId targetType = Core::InvalidTypeId;
    CompiledTemplateBlueprint blueprint;
    Base::Vector<Controls::VisualStateGroup>
        visualStateGroups;
};

Base::Result<void> BuildCompiledTemplate(
    Controls::TemplateBuildContext& context,
    void* factoryContext) noexcept;

Base::Result<Base::Ref<Base::Object>>
BuildCompiledDeferredTemplate(
    const Base::Ref<Base::Object>& payload,
    void* factoryContext) noexcept;

Base::Result<CompiledTemplateBlueprint>
CompileDeferredTemplateBlueprint(
    const Base::Ref<Base::Object>& visualTree,
    Base::Span<const DeferredContentEdge> edges,
    Core::MetadataRuntime& runtime,
    Core::DependencyPropertyRegistry& properties) noexcept;

Base::Result<CompiledTemplateDefinition>
CompileControlTemplateDefinition(
    Controls::ControlTemplate& controlTemplate,
    Base::Span<const DeferredContentEdge> edges,
    Core::MetadataRuntime& runtime,
    Core::DependencyPropertyRegistry& properties) noexcept;

} // namespace Aero::Markup::Detail
