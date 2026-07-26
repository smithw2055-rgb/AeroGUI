#include <Aero/Markup/XamlTheme.hpp>

#include <Aero/Base/Ref.hpp>
#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/ObjectServices.hpp>

#include "XamlThemeResources.hpp"

#include <memory>
#include <new>
#include <utility>

namespace Aero::Markup {
namespace {

using namespace Aero::Core;
using namespace Aero::Controls;
using namespace Aero::Presentation;

struct ThemeBlueprint final {
    Base::Vector<ThemeVisualNode> nodes;
};

struct ThemeEntry final {
    TypeId targetType = InvalidTypeId;
    ThemeBlueprint blueprint;
    std::unique_ptr<ControlTemplate> plan;
};

Base::Status ThemeMaterializerError(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed, message);
}

TypeId TargetTypeFromName(Base::StringView name) noexcept {
    if (name == Base::StringView("Button")) {
        return Button::StaticTypeId();
    }
    if (name == Base::StringView("RepeatButton")) {
        return RepeatButton::StaticTypeId();
    }
    if (name == Base::StringView("ToggleButton")) {
        return ToggleButton::StaticTypeId();
    }
    if (name == Base::StringView("CheckBox")) {
        return CheckBox::StaticTypeId();
    }
    if (name == Base::StringView("RadioButton")) {
        return RadioButton::StaticTypeId();
    }
    if (name == Base::StringView("ListBox")) {
        return ListBox::StaticTypeId();
    }
    if (name == Base::StringView("ListBoxItem")) {
        return ListBoxItem::StaticTypeId();
    }
    return InvalidTypeId;
}

const ThemeVisualNode* FindNode(
    const ThemeBlueprint& blueprint,
    Base::StringView name) noexcept {
    for (const ThemeVisualNode& node : blueprint.nodes) {
        if (node.name.View() == name) return &node;
    }
    return nullptr;
}

Base::Result<DependencyPropertyHandle> SetterProperty(
    const ThemeVisualNode& target,
    Base::StringView property) noexcept {
    if (target.kind != ThemeVisualKind::Border) {
        return ThemeMaterializerError(
            "Theme state setters currently target Border nodes");
    }
    if (property == Base::StringView("Background")) {
        return Border::BackgroundProperty;
    }
    if (property == Base::StringView("BorderBrush")) {
        return Border::BorderBrushProperty;
    }
    return ThemeMaterializerError(
        "Theme state setter property is unsupported");
}

Base::Result<VisualStateGroup> BuildVisualStateGroup(
    const ThemeVisualStateGroupResource& source,
    const ThemeResourceDictionary& dictionary,
    const ThemeBlueprint& blueprint) noexcept {
    VisualStateGroup group;
    Base::Result<void> assigned = group.name.TryAssign(source.name.View());
    if (!assigned || group.name.Empty()) {
        return assigned
            ? ThemeMaterializerError("VisualStateGroup requires Name")
            : assigned.GetStatus();
    }
    for (const ThemeVisualStateResource& sourceState : source.states) {
        VisualState state;
        assigned = state.name.TryAssign(sourceState.name.View());
        if (!assigned || state.name.Empty()) {
            return assigned
                ? ThemeMaterializerError("VisualState requires Name")
                : assigned.GetStatus();
        }
        for (const ThemeSetterResource& sourceSetter : sourceState.setters) {
            const ThemeVisualNode* target = FindNode(
                blueprint, sourceSetter.targetName.View());
            const ThemeColorResource* resource = dictionary.FindColor(
                sourceSetter.resource.View());
            if (target == nullptr || resource == nullptr) {
                return ThemeMaterializerError(
                    "VisualState setter target or resource is missing");
            }
            Base::Result<DependencyPropertyHandle> property = SetterProperty(
                *target, sourceSetter.property.View());
            if (!property) return property.GetStatus();
            Base::Result<Value> value = TryCreateRuntimeValue(
                BuiltinTypes::Color,
                &resource->value);
            if (!value) return value.GetStatus();

            VisualStateSetter setter;
            assigned = setter.targetName.TryAssign(
                sourceSetter.targetName.View());
            if (!assigned) return assigned.GetStatus();
            setter.property = property.Value();
            setter.value = value.Value();
            assigned = state.setters.TryPushBack(std::move(setter));
            if (!assigned) return assigned.GetStatus();
        }
        assigned = group.states.TryPushBack(std::move(state));
        if (!assigned) return assigned.GetStatus();
    }
    return group;
}

Base::Result<void> ConfigureNode(
    ThemeVisualKind kind,
    const ThemeVisualNode& node,
    Visual& visual) noexcept {
    if (kind == ThemeVisualKind::Border) {
        auto& border = static_cast<Border&>(visual);
        Base::Result<void> status;
        if (node.hasBackground) {
            status = border.SetBackground(node.background);
        }
        if (status && node.hasBorderBrush) {
            status = border.SetBorderBrush(node.borderBrush);
        }
        if (status && node.hasBorderThickness) {
            status = border.SetBorderThickness(node.borderThickness);
        }
        if (status && node.hasPadding) {
            status = border.SetPadding(node.padding);
        }
        return status;
    }
    if (kind == ThemeVisualKind::StackPanel && node.hasOrientation) {
        return static_cast<StackPanel&>(visual).SetOrientation(node.orientation);
    }
    return {};
}

Base::Result<void> BuildThemeTemplate(
    TemplateBuildContext& context,
    void* factoryContext) noexcept {
    auto* blueprint = static_cast<ThemeBlueprint*>(factoryContext);
    if (blueprint == nullptr || blueprint->nodes.Empty()) {
        return ThemeMaterializerError("Theme template blueprint is empty");
    }
    Base::Vector<Visual*> visuals;
    for (std::uint32_t index = 0U; index < blueprint->nodes.Size(); ++index) {
        const ThemeVisualNode& node = blueprint->nodes[index];
        Base::Ref<Base::Object> owner;
        Visual* visual = nullptr;
        if (node.kind == ThemeVisualKind::Grid) {
            Base::Result<Base::Ref<Grid>> made = Base::MakeRef<Grid>();
            if (!made) return made.GetStatus();
            Base::Ref<Grid> typed = std::move(made).Value();
            visual = typed.Get();
            owner = Base::Ref<Base::Object>(std::move(typed));
        } else if (node.kind == ThemeVisualKind::StackPanel) {
            Base::Result<Base::Ref<StackPanel>> made = Base::MakeRef<StackPanel>();
            if (!made) return made.GetStatus();
            Base::Ref<StackPanel> typed = std::move(made).Value();
            visual = typed.Get();
            owner = Base::Ref<Base::Object>(std::move(typed));
        } else if (node.kind == ThemeVisualKind::Border) {
            Base::Result<Base::Ref<Border>> made = Base::MakeRef<Border>();
            if (!made) return made.GetStatus();
            Base::Ref<Border> typed = std::move(made).Value();
            visual = typed.Get();
            owner = Base::Ref<Base::Object>(std::move(typed));
        } else {
            Base::Result<Base::Ref<ContentPresenter>> made =
                Base::MakeRef<ContentPresenter>();
            if (!made) return made.GetStatus();
            Base::Ref<ContentPresenter> typed = std::move(made).Value();
            visual = typed.Get();
            owner = Base::Ref<Base::Object>(std::move(typed));
        }
        Base::Result<void> configured = ConfigureNode(node.kind, node, *visual);
        if (!configured) return configured.GetStatus();
        Base::Result<void> added;
        if (node.parent == UINT32_MAX) {
            added = context.SetRoot(std::move(owner), *visual);
        } else {
            if (node.parent >= visuals.Size()) {
                return ThemeMaterializerError("Theme node parent is invalid");
            }
            added = context.AddPart(
                node.name.View(),
                *visuals[node.parent],
                std::move(owner),
                *visual);
        }
        if (!added) return added.GetStatus();
        added = visuals.TryPushBack(visual);
        if (!added) return added.GetStatus();
    }
    for (std::uint32_t index = 0U; index < blueprint->nodes.Size(); ++index) {
        if (blueprint->nodes[index].kind != ThemeVisualKind::ContentPresenter) {
            continue;
        }
        Base::Result<bool> projected = context.ProjectContent(
            static_cast<ContentControl&>(context.TemplatedParent()),
            static_cast<ContentPresenter&>(*visuals[index]));
        if (!projected) return projected.GetStatus();
        break;
    }
    return {};
}

} // namespace

struct XamlTheme::Impl final {
    ThemeResourceDictionary resources;
    Base::Vector<ThemeEntry> entries;
};

XamlTheme::XamlTheme(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

XamlTheme::~XamlTheme() = default;

Base::Result<std::unique_ptr<XamlTheme>> XamlTheme::Load(
    Base::StringView genericXaml,
    Base::StringView paletteXaml,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Result<ThemeResourceDictionary> loadedResources =
        LoadThemeResourceDictionary(genericXaml, paletteXaml);
    if (!loadedResources) return loadedResources.GetStatus();

    std::unique_ptr<Impl> impl(new (std::nothrow) Impl());
    if (!impl) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Theme allocation failed");
    }
    impl->resources = std::move(loadedResources).Value();

    for (std::uint32_t resourceIndex = 0U;
         resourceIndex < impl->resources.templates.Size();
         ++resourceIndex) {
        ThemeControlTemplateResource& templateResource =
            impl->resources.templates[resourceIndex];
        ThemeEntry entry;
        entry.targetType = TargetTypeFromName(templateResource.targetType.View());
        if (entry.targetType == InvalidTypeId) {
            return ThemeMaterializerError("Theme template TargetType is unsupported");
        }
        for (const ThemeEntry& existing : impl->entries) {
            if (existing.targetType == entry.targetType) {
                return ThemeMaterializerError("Theme template TargetType is duplicated");
            }
        }
        entry.blueprint.nodes = std::move(templateResource.visualTree);
        Base::Result<void> appended = impl->entries.TryPushBack(std::move(entry));
        if (!appended) return appended.GetStatus();
    }

    for (std::uint32_t entryIndex = 0U;
         entryIndex < impl->entries.Size();
         ++entryIndex) {
        ThemeEntry& entry = impl->entries[entryIndex];
        entry.plan.reset(new (std::nothrow) ControlTemplate(
            entry.targetType,
            &BuildThemeTemplate,
            &entry.blueprint));
        if (!entry.plan) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Theme template allocation failed");
        }
        const ThemeControlTemplateResource& templateResource =
            impl->resources.templates[entryIndex];
        for (const ThemeVisualStateGroupResource& stateGroup :
             templateResource.visualStateGroups) {
            Base::Result<VisualStateGroup> group = BuildVisualStateGroup(
                stateGroup,
                impl->resources,
                entry.blueprint);
            if (!group) return group.GetStatus();
            Base::Result<void> added = entry.plan->TryAddVisualStateGroup(
                std::move(group).Value());
            if (!added) return added.GetStatus();
        }
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
    return impl_->resources.variant;
}

const ControlTemplate* XamlTheme::FindTemplate(TypeId targetType) const noexcept {
    for (const ThemeEntry& entry : impl_->entries) {
        if (entry.targetType == targetType) {
            return entry.plan.get();
        }
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

Base::Result<Color> XamlTheme::ColorToken(Base::StringView key) const noexcept {
    const ThemeColorResource* entry = impl_->resources.FindColor(key);
    if (entry == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Theme color token was not found");
    }
    return entry->value;
}

} // namespace Aero::Markup
