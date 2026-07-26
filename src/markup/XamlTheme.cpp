#include <Aero/Markup/XamlTheme.hpp>

#include "XamlThemeObjectModel.hpp"

#include <memory>
#include <new>
#include <utility>

namespace Aero::Markup {
namespace {

using namespace Aero::Core;
using namespace Aero::Controls;
using namespace Aero::Presentation;

struct ThemeEntry final {
    TypeId targetType = InvalidTypeId;
    Detail::ThemeTemplateBlueprint blueprint;
    Base::Vector<VisualStateGroup> visualStateGroups;
    std::unique_ptr<ControlTemplate> plan;
};

Base::Status ThemeError(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

} // namespace

struct XamlTheme::Impl final {
    ThemeVariant variant = ThemeVariant::Light;
    ResourceDictionary resources;
    Base::Vector<ThemeEntry> entries;
};

XamlTheme::XamlTheme(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

XamlTheme::~XamlTheme() = default;

Base::Result<std::unique_ptr<XamlTheme>> XamlTheme::Load(
    Base::StringView genericXaml,
    Base::StringView paletteXaml,
    MetadataRuntime& runtime) noexcept {
    Base::Result<Detail::ThemeObjectModel> loaded =
        Detail::LoadThemeObjectModel(
            genericXaml,
            paletteXaml,
            runtime);
    if (!loaded) return loaded.GetStatus();

    std::unique_ptr<Impl> impl(new (std::nothrow) Impl());
    if (!impl) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Theme allocation failed");
    }

    Detail::ThemeObjectModel model = std::move(loaded).Value();
    impl->variant = model.variant;
    impl->resources = std::move(model.resources);
    for (Detail::ThemeTemplateDefinition& definition : model.templates) {
        ThemeEntry entry;
        entry.targetType = definition.targetType;
        entry.blueprint = std::move(definition.blueprint);
        entry.visualStateGroups = std::move(definition.visualStateGroups);
        Base::Result<void> appended = impl->entries.TryPushBack(
            std::move(entry));
        if (!appended) return appended.GetStatus();
    }

    DependencyPropertyRegistry& properties =
        runtime.Domain().DependencyProperties();
    for (ThemeEntry& entry : impl->entries) {
        entry.plan.reset(new (std::nothrow) ControlTemplate(
            entry.targetType,
            &Detail::BuildThemeTemplate,
            &entry.blueprint));
        if (!entry.plan) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Theme template allocation failed");
        }
        for (VisualStateGroup& group : entry.visualStateGroups) {
            Base::Result<void> added = entry.plan->TryAddVisualStateGroup(
                std::move(group));
            if (!added) return added.GetStatus();
        }
        entry.visualStateGroups.Clear();
        Base::Result<void> sealed = entry.plan->Seal(properties);
        if (!sealed) return sealed.GetStatus();
    }

    std::unique_ptr<XamlTheme> theme(
        new (std::nothrow) XamlTheme(std::move(impl)));
    if (!theme) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Theme allocation failed");
    }
    return theme;
}

ThemeVariant XamlTheme::Variant() const noexcept {
    return impl_->variant;
}

const ControlTemplate* XamlTheme::FindTemplate(
    TypeId targetType) const noexcept {
    for (const ThemeEntry& entry : impl_->entries) {
        if (entry.targetType == targetType) return entry.plan.get();
    }
    return nullptr;
}

Base::Result<TemplateHandle> XamlTheme::Apply(
    TemplateManager& templates,
    Control& control) const noexcept {
    const ControlTemplate* plan = FindTemplate(control.RuntimeType());
    if (plan == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Default theme has no template for control type");
    }
    return templates.Apply(control, *plan);
}

Base::Result<Color> XamlTheme::ColorToken(
    Base::StringView key) const noexcept {
    Base::Result<XamlResourceValue> value = impl_->resources.Lookup(key);
    if (!value || value.Value().Type() != TypeOf<Color>() ||
        value.Value().Kind() != ValueKind::Custom ||
        value.Value().AsCustom() == nullptr) {
        return value
            ? ThemeError("Theme color token has an incompatible value")
            : value.GetStatus();
    }
    return *static_cast<const Color*>(value.Value().AsCustom());
}

} // namespace Aero::Markup
