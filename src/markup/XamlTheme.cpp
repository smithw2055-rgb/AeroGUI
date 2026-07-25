#include <Aero/Markup/XamlTheme.hpp>

#include <Aero/Base/Ref.hpp>
#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <cmath>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

namespace Aero::Markup {
namespace {

using namespace Aero::Core;
using namespace Aero::Controls;
using namespace Aero::Presentation;

struct XmlAttributeValue final {
    Base::String name;
    Base::String value;
};

struct XmlElement final {
    Base::String name;
    Base::Vector<XmlAttributeValue> attributes;
    Base::Vector<std::uint32_t> children;
    std::uint32_t parent = UINT32_MAX;
};

struct XmlDocument final {
    Base::Vector<XmlElement> elements;
    std::uint32_t root = UINT32_MAX;
};

enum class ThemeNodeKind : std::uint8_t {
    Grid = 0U,
    StackPanel,
    Border,
    ContentPresenter,
};

struct ThemeNode final {
    ThemeNodeKind kind = ThemeNodeKind::Grid;
    Base::String name;
    std::uint32_t parent = UINT32_MAX;
    Color background;
    Color borderBrush;
    Thickness padding;
    double borderThickness = 0.0;
    Orientation orientation = Orientation::Vertical;
    bool hasBackground = false;
    bool hasBorderBrush = false;
    bool hasPadding = false;
    bool hasBorderThickness = false;
    bool hasOrientation = false;
};

struct ThemeBlueprint final {
    Base::Vector<ThemeNode> nodes;
};

struct PaletteEntry final {
    Base::String key;
    Color value;
};

struct ThemeEntry final {
    TypeId targetType = InvalidTypeId;
    ThemeBlueprint blueprint;
    std::unique_ptr<ControlTemplate> plan;
};

Base::Status InvalidTheme(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed, message);
}

Base::StringView Attribute(
    const XmlElement& element,
    Base::StringView name) noexcept {
    for (const XmlAttributeValue& attribute :
        element.attributes) {
        if (attribute.name.View() == name) {
            return attribute.value.View();
        }
    }
    return {};
}

bool IsWhitespace(Base::StringView value) noexcept {
    for (std::uint32_t index = 0U;
        index < value.SizeBytes(); ++index) {
        const char c = value[index];
        if (c != ' ' && c != '\t' &&
            c != '\r' && c != '\n') {
            return false;
        }
    }
    return true;
}

Base::Result<XmlDocument> ParseXml(
    Base::StringView text) noexcept {
    Utf8XmlTokenizer tokenizer;
    Base::Result<void> reset = tokenizer.Reset(text);
    if (!reset) return reset.GetStatus();

    XmlDocument document;
    Base::Vector<std::uint32_t> stack;
    XmlToken token;
    for (;;) {
        Base::Result<XmlTokenKind> read =
            tokenizer.Read(token);
        if (!read) return read.GetStatus();
        if (read.Value() == XmlTokenKind::EndOfDocument) {
            break;
        }
        if (read.Value() == XmlTokenKind::Text) {
            if (!IsWhitespace(token.Text())) {
                return InvalidTheme(
                    "Theme elements do not accept text content");
            }
            continue;
        }
        if (read.Value() == XmlTokenKind::EndElement) {
            if (stack.Empty()) {
                return InvalidTheme(
                    "Theme XML element stack is invalid");
            }
            stack.PopBack();
            continue;
        }
        if (read.Value() != XmlTokenKind::StartElement) {
            continue;
        }

        XmlElement element;
        Base::Result<void> named =
            element.name.TryAssign(token.Name());
        if (!named) return named.GetStatus();
        element.parent =
            stack.Empty() ? UINT32_MAX : stack.Back();
        for (const XmlAttribute& source :
            token.Attributes()) {
            if (source.Name() == Base::StringView("xmlns") ||
                source.Name() ==
                    Base::StringView("xmlns:x")) {
                continue;
            }
            XmlAttributeValue attribute;
            Base::Result<void> assigned =
                attribute.name.TryAssign(source.Name());
            if (assigned) {
                assigned =
                    attribute.value.TryAssign(source.Value());
            }
            if (!assigned) return assigned.GetStatus();
            assigned = element.attributes.TryPushBack(
                std::move(attribute));
            if (!assigned) return assigned.GetStatus();
        }
        const std::uint32_t index =
            document.elements.Size();
        Base::Result<void> appended =
            document.elements.TryPushBack(std::move(element));
        if (!appended) return appended.GetStatus();
        if (document.elements[index].parent == UINT32_MAX) {
            if (document.root != UINT32_MAX) {
                return InvalidTheme(
                    "Theme XML requires one document root");
            }
            document.root = index;
        } else {
            appended = document.elements[
                document.elements[index].parent]
                .children.TryPushBack(index);
            if (!appended) return appended.GetStatus();
        }
        if (!token.IsEmptyElement()) {
            appended = stack.TryPushBack(index);
            if (!appended) return appended.GetStatus();
        }
    }
    if (document.root == UINT32_MAX || !stack.Empty()) {
        return InvalidTheme(
            "Theme XML document is incomplete");
    }
    return document;
}

std::uint8_t HexNibble(char value) noexcept {
    if (value >= '0' && value <= '9') {
        return static_cast<std::uint8_t>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<std::uint8_t>(value - 'a' + 10);
    }
    if (value >= 'A' && value <= 'F') {
        return static_cast<std::uint8_t>(value - 'A' + 10);
    }
    return 0xFFU;
}

Base::Result<Color> ParseColor(
    Base::StringView value) noexcept {
    if (value.SizeBytes() != 9U || value[0] != '#') {
        return InvalidTheme(
            "Theme colors require #AARRGGBB");
    }
    std::uint8_t bytes[4]{};
    for (std::uint32_t index = 0U; index < 4U; ++index) {
        const std::uint8_t high =
            HexNibble(value[1U + index * 2U]);
        const std::uint8_t low =
            HexNibble(value[2U + index * 2U]);
        if (high == 0xFFU || low == 0xFFU) {
            return InvalidTheme(
                "Theme color contains an invalid hex digit");
        }
        bytes[index] =
            static_cast<std::uint8_t>((high << 4U) | low);
    }
    constexpr float scale = 1.0F / 255.0F;
    return Color{
        bytes[1] * scale,
        bytes[2] * scale,
        bytes[3] * scale,
        bytes[0] * scale};
}

Base::Result<double> ParseDouble(
    Base::StringView value) noexcept {
    Base::String text;
    Base::Result<void> assigned = text.TryAssign(value);
    if (!assigned) return assigned.GetStatus();
    char* end = nullptr;
    const double parsed =
        std::strtod(text.CStr(), &end);
    if (end == text.CStr() || *end != '\0' ||
        !std::isfinite(parsed)) {
        return InvalidTheme(
            "Theme number is invalid");
    }
    return parsed;
}

Base::Result<Thickness> ParseThickness(
    Base::StringView value) noexcept {
    Base::Result<double> uniform = ParseDouble(value);
    if (!uniform || uniform.Value() < 0.0) {
        return InvalidTheme(
            "Theme thickness must be a nonnegative number");
    }
    return Thickness{
        uniform.Value(), uniform.Value(),
        uniform.Value(), uniform.Value()};
}

const PaletteEntry* FindPalette(
    const Base::Vector<PaletteEntry>& palette,
    Base::StringView key) noexcept {
    for (const PaletteEntry& entry : palette) {
        if (entry.key.View() == key) return &entry;
    }
    return nullptr;
}

Base::Result<Base::Vector<PaletteEntry>> ParsePalette(
    const XmlDocument& document,
    ThemeVariant& variant) noexcept {
    const XmlElement& root =
        document.elements[document.root];
    if (root.name.View() !=
        Base::StringView("ResourceDictionary")) {
        return InvalidTheme(
            "Theme palette root must be ResourceDictionary");
    }
    const Base::StringView variantName =
        Attribute(root, "Variant");
    if (variantName == Base::StringView("Light")) {
        variant = ThemeVariant::Light;
    } else if (variantName ==
        Base::StringView("Dark")) {
        variant = ThemeVariant::Dark;
    } else {
        return InvalidTheme(
            "Theme palette Variant must be Light or Dark");
    }

    Base::Vector<PaletteEntry> palette;
    for (std::uint32_t childIndex :
        root.children) {
        const XmlElement& child =
            document.elements[childIndex];
        if (child.name.View() !=
            Base::StringView("Color")) {
            return InvalidTheme(
                "Theme palettes only accept Color entries");
        }
        Base::StringView key =
            Attribute(child, "x:Key");
        if (key.Empty()) key = Attribute(child, "Key");
        const Base::StringView raw =
            Attribute(child, "Value");
        if (key.Empty() || raw.Empty() ||
            FindPalette(palette, key) != nullptr) {
            return InvalidTheme(
                "Theme color key is missing or duplicated");
        }
        Base::Result<Color> color = ParseColor(raw);
        if (!color) return color.GetStatus();
        PaletteEntry entry;
        Base::Result<void> assigned =
            entry.key.TryAssign(key);
        if (!assigned) return assigned.GetStatus();
        entry.value = color.Value();
        assigned = palette.TryPushBack(
            std::move(entry));
        if (!assigned) return assigned.GetStatus();
    }
    return palette;
}

TypeId TargetTypeFromName(
    Base::StringView name) noexcept {
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

Base::Result<ThemeNodeKind> NodeKindFromName(
    Base::StringView name) noexcept {
    if (name == Base::StringView("Grid")) {
        return ThemeNodeKind::Grid;
    }
    if (name == Base::StringView("StackPanel")) {
        return ThemeNodeKind::StackPanel;
    }
    if (name == Base::StringView("Border")) {
        return ThemeNodeKind::Border;
    }
    if (name == Base::StringView("ContentPresenter")) {
        return ThemeNodeKind::ContentPresenter;
    }
    return InvalidTheme(
        "Theme visual tree contains an unsupported node");
}

Base::Result<void> ResolveColorAttribute(
    const XmlElement& element,
    Base::StringView attribute,
    const Base::Vector<PaletteEntry>& palette,
    Color& output,
    bool& present) noexcept {
    const Base::StringView key =
        Attribute(element, attribute);
    if (key.Empty()) return {};
    const PaletteEntry* entry =
        FindPalette(palette, key);
    if (entry == nullptr) {
        return InvalidTheme(
            "Theme visual references a missing color token");
    }
    output = entry->value;
    present = true;
    return {};
}

Base::Result<void> ParseVisualNode(
    const XmlDocument& document,
    std::uint32_t elementIndex,
    std::uint32_t parent,
    const Base::Vector<PaletteEntry>& palette,
    ThemeBlueprint& blueprint) noexcept {
    const XmlElement& element =
        document.elements[elementIndex];
    Base::Result<ThemeNodeKind> kind =
        NodeKindFromName(element.name.View());
    if (!kind) return kind.GetStatus();

    ThemeNode node;
    node.kind = kind.Value();
    node.parent = parent;
    Base::StringView name =
        Attribute(element, "x:Name");
    if (name.Empty()) name = Attribute(element, "Name");
    if (parent != UINT32_MAX && name.Empty()) {
        return InvalidTheme(
            "Non-root theme visuals require x:Name");
    }
    Base::Result<void> assigned =
        node.name.TryAssign(name);
    if (!assigned) return assigned.GetStatus();
    Base::Result<void> color = ResolveColorAttribute(
        element, "BackgroundResource", palette,
        node.background, node.hasBackground);
    if (!color) return color.GetStatus();
    color = ResolveColorAttribute(
        element, "BorderBrushResource", palette,
        node.borderBrush, node.hasBorderBrush);
    if (!color) return color.GetStatus();

    const Base::StringView borderThickness =
        Attribute(element, "BorderThickness");
    if (!borderThickness.Empty()) {
        Base::Result<double> parsed =
            ParseDouble(borderThickness);
        if (!parsed || parsed.Value() < 0.0) {
            return InvalidTheme(
                "BorderThickness must be nonnegative");
        }
        node.borderThickness = parsed.Value();
        node.hasBorderThickness = true;
    }
    const Base::StringView padding =
        Attribute(element, "Padding");
    if (!padding.Empty()) {
        Base::Result<Thickness> parsed =
            ParseThickness(padding);
        if (!parsed) return parsed.GetStatus();
        node.padding = parsed.Value();
        node.hasPadding = true;
    }
    const Base::StringView orientation =
        Attribute(element, "Orientation");
    if (!orientation.Empty()) {
        if (orientation == Base::StringView("Horizontal")) {
            node.orientation = Orientation::Horizontal;
        } else if (orientation ==
            Base::StringView("Vertical")) {
            node.orientation = Orientation::Vertical;
        } else {
            return InvalidTheme(
                "Theme StackPanel orientation is invalid");
        }
        node.hasOrientation = true;
    }

    const std::uint32_t nodeIndex =
        blueprint.nodes.Size();
    assigned = blueprint.nodes.TryPushBack(
        std::move(node));
    if (!assigned) return assigned.GetStatus();
    for (std::uint32_t child :
        element.children) {
        assigned = ParseVisualNode(
            document, child, nodeIndex,
            palette, blueprint);
        if (!assigned) return assigned.GetStatus();
    }
    return {};
}

const ThemeNode* FindNode(
    const ThemeBlueprint& blueprint,
    Base::StringView name) noexcept {
    for (const ThemeNode& node :
        blueprint.nodes) {
        if (node.name.View() == name) return &node;
    }
    return nullptr;
}

Base::Result<DependencyPropertyHandle> SetterProperty(
    const ThemeNode& target,
    Base::StringView property) noexcept {
    if (target.kind != ThemeNodeKind::Border) {
        return InvalidTheme(
            "Theme state setters currently target Border nodes");
    }
    if (property == Base::StringView("Background")) {
        return Border::BackgroundProperty;
    }
    if (property == Base::StringView("BorderBrush")) {
        return Border::BorderBrushProperty;
    }
    return InvalidTheme(
        "Theme state setter property is unsupported");
}

Base::Result<VisualStateGroup> ParseStateGroup(
    const XmlDocument& document,
    const XmlElement& groupElement,
    const Base::Vector<PaletteEntry>& palette,
    const ThemeBlueprint& blueprint) noexcept {
    VisualStateGroup group;
    Base::Result<void> assigned = group.name.TryAssign(
        Attribute(groupElement, "Name"));
    if (!assigned || group.name.Empty()) {
        return assigned
            ? InvalidTheme(
                "VisualStateGroup requires Name")
            : assigned.GetStatus();
    }
    for (std::uint32_t stateIndex :
        groupElement.children) {
        const XmlElement& stateElement =
            document.elements[stateIndex];
        if (stateElement.name.View() !=
            Base::StringView("VisualState")) {
            return InvalidTheme(
                "VisualStateGroup only accepts VisualState");
        }
        VisualState state;
        assigned = state.name.TryAssign(
            Attribute(stateElement, "Name"));
        if (!assigned || state.name.Empty()) {
            return assigned
                ? InvalidTheme(
                    "VisualState requires Name")
                : assigned.GetStatus();
        }
        for (std::uint32_t setterIndex :
            stateElement.children) {
            const XmlElement& setterElement =
                document.elements[setterIndex];
            if (setterElement.name.View() !=
                Base::StringView("Setter")) {
                return InvalidTheme(
                    "VisualState only accepts Setter");
            }
            const Base::StringView targetName =
                Attribute(setterElement, "TargetName");
            const ThemeNode* target =
                FindNode(blueprint, targetName);
            const PaletteEntry* resource = FindPalette(
                palette,
                Attribute(setterElement, "Resource"));
            if (target == nullptr || resource == nullptr) {
                return InvalidTheme(
                    "VisualState setter target or resource is missing");
            }
            Base::Result<DependencyPropertyHandle> property =
                SetterProperty(
                    *target,
                    Attribute(setterElement, "Property"));
            if (!property) return property.GetStatus();
            Base::Result<Value> value =
                TryCreateRuntimeValue(
                    BuiltinTypes::Color,
                    &resource->value);
            if (!value) return value.GetStatus();
            VisualStateSetter setter;
            assigned = setter.targetName.TryAssign(
                targetName);
            if (!assigned) return assigned.GetStatus();
            setter.property = property.Value();
            setter.value = value.Value();
            assigned = state.setters.TryPushBack(
                std::move(setter));
            if (!assigned) return assigned.GetStatus();
        }
        assigned = group.states.TryPushBack(
            std::move(state));
        if (!assigned) return assigned.GetStatus();
    }
    return group;
}

Base::Result<void> ConfigureNode(
    ThemeNodeKind kind,
    const ThemeNode& node,
    Visual& visual) noexcept {
    if (kind == ThemeNodeKind::Border) {
        auto& border = static_cast<Border&>(visual);
        Base::Result<void> status;
        if (node.hasBackground) {
            status = border.SetBackground(node.background);
        }
        if (status && node.hasBorderBrush) {
            status = border.SetBorderBrush(node.borderBrush);
        }
        if (status && node.hasBorderThickness) {
            status = border.SetBorderThickness(
                node.borderThickness);
        }
        if (status && node.hasPadding) {
            status = border.SetPadding(node.padding);
        }
        return status;
    }
    if (kind == ThemeNodeKind::StackPanel &&
        node.hasOrientation) {
        return static_cast<StackPanel&>(visual)
            .SetOrientation(node.orientation);
    }
    return {};
}

Base::Result<void> BuildThemeTemplate(
    TemplateBuildContext& context,
    void* factoryContext) noexcept {
    auto* blueprint =
        static_cast<ThemeBlueprint*>(factoryContext);
    if (blueprint == nullptr ||
        blueprint->nodes.Empty()) {
        return InvalidTheme(
            "Theme template blueprint is empty");
    }
    Base::Vector<Visual*> visuals;
    for (std::uint32_t index = 0U;
        index < blueprint->nodes.Size(); ++index) {
        const ThemeNode& node =
            blueprint->nodes[index];
        Base::Ref<Base::Object> owner;
        Visual* visual = nullptr;
        if (node.kind == ThemeNodeKind::Grid) {
            Base::Result<Base::Ref<Grid>> made =
                Base::MakeRef<Grid>();
            if (!made) return made.GetStatus();
            Base::Ref<Grid> typed =
                std::move(made).Value();
            visual = typed.Get();
            owner = Base::Ref<Base::Object>(
                std::move(typed));
        } else if (node.kind ==
            ThemeNodeKind::StackPanel) {
            Base::Result<Base::Ref<StackPanel>> made =
                Base::MakeRef<StackPanel>();
            if (!made) return made.GetStatus();
            Base::Ref<StackPanel> typed =
                std::move(made).Value();
            visual = typed.Get();
            owner = Base::Ref<Base::Object>(
                std::move(typed));
        } else if (node.kind ==
            ThemeNodeKind::Border) {
            Base::Result<Base::Ref<Border>> made =
                Base::MakeRef<Border>();
            if (!made) return made.GetStatus();
            Base::Ref<Border> typed =
                std::move(made).Value();
            visual = typed.Get();
            owner = Base::Ref<Base::Object>(
                std::move(typed));
        } else {
            Base::Result<Base::Ref<ContentPresenter>> made =
                Base::MakeRef<ContentPresenter>();
            if (!made) return made.GetStatus();
            Base::Ref<ContentPresenter> typed =
                std::move(made).Value();
            visual = typed.Get();
            owner = Base::Ref<Base::Object>(
                std::move(typed));
        }
        Base::Result<void> configured =
            ConfigureNode(node.kind, node, *visual);
        if (!configured) return configured.GetStatus();
        Base::Result<void> added;
        if (node.parent == UINT32_MAX) {
            added = context.SetRoot(
                std::move(owner), *visual);
        } else {
            if (node.parent >= visuals.Size()) {
                return InvalidTheme(
                    "Theme node parent is invalid");
            }
            added = context.AddPart(
                node.name.View(),
                *visuals[node.parent],
                std::move(owner), *visual);
        }
        if (!added) return added.GetStatus();
        added = visuals.TryPushBack(visual);
        if (!added) return added.GetStatus();
    }
    for (std::uint32_t index = 0U;
        index < blueprint->nodes.Size(); ++index) {
        if (blueprint->nodes[index].kind !=
            ThemeNodeKind::ContentPresenter) {
            continue;
        }
        Base::Result<bool> projected =
            context.ProjectContent(
                static_cast<ContentControl&>(
                    context.TemplatedParent()),
                static_cast<ContentPresenter&>(
                    *visuals[index]));
        if (!projected) return projected.GetStatus();
        break;
    }
    return {};
}

} // namespace

struct XamlTheme::Impl final {
    ThemeVariant variant = ThemeVariant::Light;
    Base::Vector<PaletteEntry> palette;
    Base::Vector<ThemeEntry> entries;
};

XamlTheme::XamlTheme(
    std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

XamlTheme::~XamlTheme() = default;

Base::Result<std::unique_ptr<XamlTheme>> XamlTheme::Load(
    Base::StringView genericXaml,
    Base::StringView paletteXaml,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Result<XmlDocument> generic =
        ParseXml(genericXaml);
    if (!generic) return generic.GetStatus();
    Base::Result<XmlDocument> paletteDocument =
        ParseXml(paletteXaml);
    if (!paletteDocument) {
        return paletteDocument.GetStatus();
    }
    std::unique_ptr<Impl> impl(
        new (std::nothrow) Impl());
    if (!impl) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Theme allocation failed");
    }
    Base::Result<Base::Vector<PaletteEntry>> palette =
        ParsePalette(
            paletteDocument.Value(), impl->variant);
    if (!palette) return palette.GetStatus();
    impl->palette = std::move(palette).Value();

    const XmlDocument& document = generic.Value();
    const XmlElement& root =
        document.elements[document.root];
    if (root.name.View() !=
        Base::StringView("ResourceDictionary")) {
        return InvalidTheme(
            "Generic theme root must be ResourceDictionary");
    }
    for (std::uint32_t templateIndex :
        root.children) {
        const XmlElement& templateElement =
            document.elements[templateIndex];
        if (templateElement.name.View() !=
            Base::StringView("ControlTemplate")) {
            return InvalidTheme(
                "Generic theme only accepts ControlTemplate");
        }
        ThemeEntry entry;
        entry.targetType = TargetTypeFromName(
            Attribute(templateElement, "TargetType"));
        if (entry.targetType == InvalidTypeId) {
            return InvalidTheme(
                "Theme template TargetType is unsupported");
        }
        for (const ThemeEntry& existing :
            impl->entries) {
            if (existing.targetType == entry.targetType) {
                return InvalidTheme(
                    "Theme template TargetType is duplicated");
            }
        }
        const XmlElement* visualTree = nullptr;
        const XmlElement* stateGroups = nullptr;
        for (std::uint32_t childIndex :
            templateElement.children) {
            const XmlElement& child =
                document.elements[childIndex];
            if (child.name.View() ==
                Base::StringView("VisualTree")) {
                visualTree = &child;
            } else if (child.name.View() ==
                Base::StringView("VisualStateGroups")) {
                stateGroups = &child;
            } else {
                return InvalidTheme(
                    "ControlTemplate child is unsupported");
            }
        }
        if (visualTree == nullptr ||
            visualTree->children.Size() != 1U ||
            stateGroups == nullptr) {
            return InvalidTheme(
                "ControlTemplate requires one visual root and states");
        }
        Base::Result<void> parsed = ParseVisualNode(
            document, visualTree->children[0],
            UINT32_MAX, impl->palette,
            entry.blueprint);
        if (!parsed) return parsed.GetStatus();
        parsed = impl->entries.TryPushBack(
            std::move(entry));
        if (!parsed) return parsed.GetStatus();
    }

    for (std::uint32_t entryIndex = 0U;
        entryIndex < impl->entries.Size(); ++entryIndex) {
        ThemeEntry& entry = impl->entries[entryIndex];
        entry.plan.reset(new (std::nothrow)
            ControlTemplate(
                entry.targetType,
                &BuildThemeTemplate,
                &entry.blueprint));
        if (!entry.plan) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Theme template allocation failed");
        }
        const XmlElement& templateElement =
            document.elements[root.children[entryIndex]];
        const XmlElement* stateGroups = nullptr;
        for (std::uint32_t childIndex :
            templateElement.children) {
            const XmlElement& child =
                document.elements[childIndex];
            if (child.name.View() ==
                Base::StringView("VisualStateGroups")) {
                stateGroups = &child;
                break;
            }
        }
        for (std::uint32_t groupIndex :
            stateGroups->children) {
            const XmlElement& groupElement =
                document.elements[groupIndex];
            if (groupElement.name.View() !=
                Base::StringView("VisualStateGroup")) {
                return InvalidTheme(
                    "VisualStateGroups only accepts groups");
            }
            Base::Result<VisualStateGroup> group =
                ParseStateGroup(
                    document, groupElement,
                    impl->palette,
                    entry.blueprint);
            if (!group) return group.GetStatus();
            Base::Result<void> added =
                entry.plan->TryAddVisualStateGroup(
                    std::move(group).Value());
            if (!added) return added.GetStatus();
        }
        Base::Result<void> sealed =
            entry.plan->Seal(properties);
        if (!sealed) return sealed.GetStatus();
    }
    std::unique_ptr<XamlTheme> theme(
        new (std::nothrow) XamlTheme(
            std::move(impl)));
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
        if (entry.targetType == targetType) {
            return entry.plan.get();
        }
    }
    return nullptr;
}

Base::Result<TemplateHandle> XamlTheme::Apply(
    TemplateManager& templates,
    Control& control) const noexcept {
    const ControlTemplate* plan =
        FindTemplate(control.RuntimeType());
    if (plan == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Default theme has no template for control type");
    }
    return templates.Apply(control, *plan);
}

Base::Result<Color> XamlTheme::ColorToken(
    Base::StringView key) const noexcept {
    const PaletteEntry* entry =
        FindPalette(impl_->palette, key);
    if (entry == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Theme color token was not found");
    }
    return entry->value;
}

} // namespace Aero::Markup
