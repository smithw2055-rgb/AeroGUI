#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Markup/XamlNamesResources.hpp>
#include <Aero/Markup/XamlTheme.hpp>

#include <cstdint>

namespace Aero::Markup::Detail {

class ResourceDictionaryObject final : public Base::Object {
    AERO_TYPED_META_NAMED(
        ResourceDictionaryObject,
        Base::Object,
        "urn:aero",
        "ResourceDictionary")
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::Result<void> SetVariant(Base::StringView value) noexcept {
        return variant_.TryAssign(value);
    }
    Base::StringView Variant() const noexcept { return variant_.View(); }

    Base::Result<void> AddEntry(
        const Base::Ref<Base::Object>& value) noexcept {
        return entries_.TryPushBack(value);
    }
    void ClearEntries() noexcept { entries_.Clear(); }
    Base::Span<const Base::Ref<Base::Object>> Entries() const noexcept {
        return {entries_.Data(), entries_.Size()};
    }

private:
    Base::String variant_;
    Base::Vector<Base::Ref<Base::Object>> entries_;
};

class ThemeSetterObject final : public Base::Object {
    AERO_TYPED_META_NAMED(
        ThemeSetterObject,
        Base::Object,
        "urn:aero",
        "Setter")
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::Result<void> SetTargetName(Base::StringView value) noexcept {
        return targetName_.TryAssign(value);
    }
    Base::Result<void> SetProperty(Base::StringView value) noexcept {
        return property_.TryAssign(value);
    }
    Base::Result<void> SetValue(const Core::Value& value) noexcept {
        if (value.IsUnset()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Setter value must not be unset");
        }
        value_ = value;
        return {};
    }

    Base::StringView TargetName() const noexcept { return targetName_.View(); }
    Base::StringView Property() const noexcept { return property_.View(); }
    const Core::Value& Value() const noexcept { return value_; }

private:
    Base::String targetName_;
    Base::String property_;
    Core::Value value_;
};

class ThemeVisualStateObject final : public Base::Object {
    AERO_TYPED_META_NAMED(
        ThemeVisualStateObject,
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
    Base::StringView Name() const noexcept { return name_.View(); }

    Base::Result<void> AddSetter(
        const Base::Ref<Base::Object>& value) noexcept {
        return setters_.TryPushBack(value);
    }
    void ClearSetters() noexcept { setters_.Clear(); }
    Base::Span<const Base::Ref<Base::Object>> Setters() const noexcept {
        return {setters_.Data(), setters_.Size()};
    }

private:
    Base::String name_;
    Base::Vector<Base::Ref<Base::Object>> setters_;
};

class ThemeVisualStateGroupObject final : public Base::Object {
    AERO_TYPED_META_NAMED(
        ThemeVisualStateGroupObject,
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
    Base::StringView Name() const noexcept { return name_.View(); }

    Base::Result<void> AddState(
        const Base::Ref<Base::Object>& value) noexcept {
        return states_.TryPushBack(value);
    }
    void ClearStates() noexcept { states_.Clear(); }
    Base::Span<const Base::Ref<Base::Object>> States() const noexcept {
        return {states_.Data(), states_.Size()};
    }

private:
    Base::String name_;
    Base::Vector<Base::Ref<Base::Object>> states_;
};

class ThemeControlTemplateObject final : public Base::Object {
    AERO_TYPED_META_NAMED(
        ThemeControlTemplateObject,
        Base::Object,
        "urn:aero",
        "ControlTemplate")
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::Result<void> SetTargetType(Base::StringView value) noexcept {
        return targetType_.TryAssign(value);
    }
    Base::StringView TargetType() const noexcept { return targetType_.View(); }

    Base::Result<void> SetVisualTree(
        const Base::Ref<Base::Object>& value) noexcept {
        if (!value) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "ControlTemplate visual tree must not be null");
        }
        visualTree_ = value;
        return {};
    }
    const Base::Ref<Base::Object>& VisualTree() const noexcept {
        return visualTree_;
    }

    Base::Result<void> AddVisualStateGroup(
        const Base::Ref<Base::Object>& value) noexcept {
        return visualStateGroups_.TryPushBack(value);
    }
    void ClearVisualStateGroups() noexcept { visualStateGroups_.Clear(); }
    Base::Span<const Base::Ref<Base::Object>> VisualStateGroups() const noexcept {
        return {visualStateGroups_.Data(), visualStateGroups_.Size()};
    }

    Base::Result<void> RegisterName(
        Base::StringView name,
        Base::Object& value) noexcept {
        return names_.TryRegister(name, value);
    }
    const NameScope& Names() const noexcept { return names_; }

private:
    Base::String targetType_;
    Base::Ref<Base::Object> visualTree_;
    Base::Vector<Base::Ref<Base::Object>> visualStateGroups_;
    NameScope names_;
};

struct ThemePrototypeProperty final {
    Core::DependencyPropertyHandle property;
    Core::Value value;
};

struct ThemePrototypeNode final {
    Core::TypeId type = Core::InvalidTypeId;
    Base::String name;
    std::uint32_t parent = UINT32_MAX;
    Base::Vector<ThemePrototypeProperty> properties;
};

struct ThemeTemplateBlueprint final {
    Core::MetadataRuntime* runtime = nullptr;
    Base::Vector<ThemePrototypeNode> nodes;
    std::uint32_t contentPresenter = UINT32_MAX;
};

struct ThemeTemplateDefinition final {
    Core::TypeId targetType = Core::InvalidTypeId;
    ThemeTemplateBlueprint blueprint;
    Base::Vector<Controls::VisualStateGroup> visualStateGroups;
};

struct ThemeObjectModel final {
    ThemeVariant variant = ThemeVariant::Light;
    ResourceDictionary resources;
    Base::Vector<ThemeTemplateDefinition> templates;
};

Base::Result<ThemeObjectModel> LoadThemeObjectModel(
    Base::StringView genericXaml,
    Base::StringView paletteXaml,
    Core::MetadataRuntime& runtime) noexcept;

Base::Result<void> BuildThemeTemplate(
    Controls::TemplateBuildContext& context,
    void* factoryContext) noexcept;

} // namespace Aero::Markup::Detail
