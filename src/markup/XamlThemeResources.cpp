#include <Aero/Markup/XamlThemeResources.hpp>

#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <cmath>
#include <cstdlib>
#include <utility>

namespace Aero::Markup {
namespace {

struct ThemeXamlAttribute final {
    Base::String name;
    Base::String value;
};

struct ThemeXamlElement final {
    Base::String name;
    Base::Vector<ThemeXamlAttribute> attributes;
    Base::Vector<std::uint32_t> children;
    std::uint32_t parent = UINT32_MAX;
};

struct ThemeXamlDocument final {
    Base::Vector<ThemeXamlElement> elements;
    std::uint32_t root = UINT32_MAX;
};

Base::Status InvalidTheme(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed, message);
}

Base::StringView Attribute(
    const ThemeXamlElement& element,
    Base::StringView name) noexcept {
    for (const ThemeXamlAttribute& attribute : element.attributes) {
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
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            return false;
        }
    }
    return true;
}

Base::Result<void> AssignQualifiedName(
    Base::String& output,
    const XamlQualifiedName& name,
    bool includePrefix) noexcept {
    output.Clear();
    if (includePrefix && !name.Prefix().Empty()) {
        Base::Result<void> assigned = output.TryAssign(name.Prefix());
        if (!assigned) return assigned.GetStatus();
        assigned = output.TryAppend(Base::StringView(":"));
        if (!assigned) return assigned.GetStatus();
        return output.TryAppend(name.LocalName());
    }
    return output.TryAssign(name.LocalName());
}

Base::Result<void> AddAttributeFromMember(
    ThemeXamlElement& element,
    const XamlNode& member,
    XamlNodeReader& reader) noexcept {
    ThemeXamlAttribute attribute;
    Base::Result<void> named = AssignQualifiedName(
        attribute.name, member.Name(), true);
    if (!named) return named.GetStatus();

    XamlNode value;
    Base::Result<XamlNodeKind> read = reader.Read(value);
    if (!read) return read.GetStatus();
    if (read.Value() != XamlNodeKind::Value ||
        !value.IsFromAttribute()) {
        return InvalidTheme(
            "Built-in theme XAML members must be attribute values");
    }
    Base::Result<void> assigned = attribute.value.TryAssign(value.Value());
    if (!assigned) return assigned.GetStatus();

    XamlNode end;
    read = reader.Read(end);
    if (!read) return read.GetStatus();
    if (read.Value() != XamlNodeKind::EndMember ||
        !end.IsFromAttribute()) {
        return InvalidTheme(
            "Built-in theme XAML attribute member is incomplete");
    }
    return element.attributes.TryPushBack(std::move(attribute));
}

Base::Result<ThemeXamlDocument> ReadThemeXaml(
    Base::StringView text) noexcept {
    Utf8XmlTokenizer tokenizer;
    Base::Result<void> reset = tokenizer.Reset(text);
    if (!reset) return reset.GetStatus();

    XamlNodeReader reader(tokenizer);
    ThemeXamlDocument document;
    Base::Vector<std::uint32_t> stack;
    XamlNode node;

    for (;;) {
        Base::Result<XamlNodeKind> read = reader.Read(node);
        if (!read) return read.GetStatus();
        switch (read.Value()) {
        case XamlNodeKind::NamespaceDeclaration:
            break;
        case XamlNodeKind::StartObject: {
            ThemeXamlElement element;
            Base::Result<void> named = AssignQualifiedName(
                element.name, node.Name(), false);
            if (!named) return named.GetStatus();
            element.parent = stack.Empty() ? UINT32_MAX : stack.Back();
            const std::uint32_t index = document.elements.Size();
            Base::Result<void> appended = document.elements.TryPushBack(
                std::move(element));
            if (!appended) return appended.GetStatus();
            if (document.elements[index].parent == UINT32_MAX) {
                if (document.root != UINT32_MAX) {
                    return InvalidTheme(
                        "Theme XAML requires one document root");
                }
                document.root = index;
            } else {
                appended = document.elements[
                    document.elements[index].parent]
                    .children.TryPushBack(index);
                if (!appended) return appended.GetStatus();
            }
            appended = stack.TryPushBack(index);
            if (!appended) return appended.GetStatus();
            break;
        }
        case XamlNodeKind::StartMember:
            if (stack.Empty()) {
                return InvalidTheme(
                    "Theme XAML attribute member has no owner");
            }
            reset = AddAttributeFromMember(
                document.elements[stack.Back()], node, reader);
            if (!reset) return reset.GetStatus();
            break;
        case XamlNodeKind::Value:
            if (!IsWhitespace(node.Value())) {
                return InvalidTheme(
                    "Theme elements do not accept text content");
            }
            break;
        case XamlNodeKind::EndObject:
            if (stack.Empty()) {
                return InvalidTheme(
                    "Theme XAML element stack is invalid");
            }
            stack.PopBack();
            break;
        case XamlNodeKind::EndMember:
            return InvalidTheme(
                "Theme XAML encountered an unmatched member end");
        case XamlNodeKind::EndOfDocument:
            if (document.root == UINT32_MAX || !stack.Empty()) {
                return InvalidTheme(
                    "Theme XAML document is incomplete");
            }
            return document;
        case XamlNodeKind::None:
            return InvalidTheme(
                "Theme XAML node stream is invalid");
        }
    }
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

Base::Result<Presentation::Color> ParseColor(
    Base::StringView value) noexcept {
    if (value.SizeBytes() != 9U || value[0] != '#') {
        return InvalidTheme("Theme colors require #AARRGGBB");
    }
    std::uint8_t bytes[4]{};
    for (std::uint32_t index = 0U; index < 4U; ++index) {
        const std::uint8_t high = HexNibble(value[1U + index * 2U]);
        const std::uint8_t low = HexNibble(value[2U + index * 2U]);
        if (high == 0xFFU || low == 0xFFU) {
            return InvalidTheme(
                "Theme color contains an invalid hex digit");
        }
        bytes[index] = static_cast<std::uint8_t>((high << 4U) | low);
    }
    constexpr float scale = 1.0F / 255.0F;
    return Presentation::Color{
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
    const double parsed = std::strtod(text.CStr(), &end);
    if (end == text.CStr() || *end != '\0' || !std::isfinite(parsed)) {
        return InvalidTheme("Theme number is invalid");
    }
    return parsed;
}

Base::Result<Presentation::Thickness> ParseThickness(
    Base::StringView value) noexcept {
    Base::Result<double> uniform = ParseDouble(value);
    if (!uniform || uniform.Value() < 0.0) {
        return InvalidTheme(
            "Theme thickness must be a nonnegative number");
    }
    return Presentation::Thickness{
        uniform.Value(), uniform.Value(),
        uniform.Value(), uniform.Value()};
}

Base::Result<void> ParsePalette(
    const ThemeXamlDocument& document,
    ThemeResourceDictionary& dictionary) noexcept {
    const ThemeXamlElement& root = document.elements[document.root];
    if (root.name.View() != Base::StringView("ResourceDictionary")) {
        return InvalidTheme("Theme palette root must be ResourceDictionary");
    }
    const Base::StringView variantName = Attribute(root, "Variant");
    if (variantName == Base::StringView("Light")) {
        dictionary.variant = ThemeVariant::Light;
    } else if (variantName == Base::StringView("Dark")) {
        dictionary.variant = ThemeVariant::Dark;
    } else {
        return InvalidTheme("Theme palette Variant must be Light or Dark");
    }

    for (std::uint32_t childIndex : root.children) {
        const ThemeXamlElement& child = document.elements[childIndex];
        if (child.name.View() != Base::StringView("Color")) {
            return InvalidTheme("Theme palettes only accept Color entries");
        }
        Base::StringView key = Attribute(child, "x:Key");
        if (key.Empty()) key = Attribute(child, "Key");
        const Base::StringView raw = Attribute(child, "Value");
        if (key.Empty() || raw.Empty() || dictionary.FindColor(key) != nullptr) {
            return InvalidTheme("Theme color key is missing or duplicated");
        }
        Base::Result<Presentation::Color> color = ParseColor(raw);
        if (!color) return color.GetStatus();
        ThemeColorResource entry;
        assigned:
        Base::Result<void> assigned = entry.key.TryAssign(key);
        if (!assigned) return assigned.GetStatus();
        entry.value = color.Value();
        assigned = dictionary.colors.TryPushBack(std::move(entry));
        if (!assigned) return assigned.GetStatus();
    }
    return {};
}

Base::Result<ThemeVisualKind> NodeKindFromName(
    Base::StringView name) noexcept {
    if (name == Base::StringView("Grid")) {
        return ThemeVisualKind::Grid;
    }
    if (name == Base::StringView("StackPanel")) {
        return ThemeVisualKind::StackPanel;
    }
    if (name == Base::StringView("Border")) {
        return ThemeVisualKind::Border;
    }
    if (name == Base::StringView("ContentPresenter")) {
        return ThemeVisualKind::ContentPresenter;
    }
    return InvalidTheme("Theme visual tree contains an unsupported node");
}

Base::Result<void> ResolveColorAttribute(
    const ThemeXamlElement& element,
    Base::StringView attribute,
    const ThemeResourceDictionary& dictionary,
    Presentation::Color& output,
    bool& present) noexcept {
    const Base::StringView key = Attribute(element, attribute);
    if (key.Empty()) return {};
    const ThemeColorResource* entry = dictionary.FindColor(key);
    if (entry == nullptr) {
        return InvalidTheme(
            "Theme visual references a missing color token");
    }
    output = entry->value;
    present = true;
    return {};
}

Base::Result<void> ParseVisualNode(
    const ThemeXamlDocument& document,
    std::uint32_t elementIndex,
    std::uint32_t parent,
    const ThemeResourceDictionary& dictionary,
    ThemeControlTemplateResource& controlTemplate) noexcept {
    const ThemeXamlElement& element = document.elements[elementIndex];
    Base::Result<ThemeVisualKind> kind = NodeKindFromName(element.name.View());
    if (!kind) return kind.GetStatus();

    ThemeVisualNode node;
    node.kind = kind.Value();
    node.parent = parent;
    Base::StringView name = Attribute(element, "x:Name");
    if (name.Empty()) name = Attribute(element, "Name");
    if (parent != UINT32_MAX && name.Empty()) {
        return InvalidTheme("Non-root theme visuals require x:Name");
    }
    Base::Result<void> assigned = node.name.TryAssign(name);
    if (!assigned) return assigned.GetStatus();
    Base::Result<void> color = ResolveColorAttribute(
        element, "BackgroundResource", dictionary,
        node.background, node.hasBackground);
    if (!color) return color.GetStatus();
    color = ResolveColorAttribute(
        element, "BorderBrushResource", dictionary,
        node.borderBrush, node.hasBorderBrush);
    if (!color) return color.GetStatus();

    const Base::StringView borderThickness = Attribute(element, "BorderThickness");
    if (!borderThickness.Empty()) {
        Base::Result<double> parsed = ParseDouble(borderThickness);
        if (!parsed || parsed.Value() < 0.0) {
            return InvalidTheme("BorderThickness must be nonnegative");
        }
        node.borderThickness = parsed.Value();
        node.hasBorderThickness = true;
    }
    const Base::StringView padding = Attribute(element, "Padding");
    if (!padding.Empty()) {
        Base::Result<Presentation::Thickness> parsed = ParseThickness(padding);
        if (!parsed) return parsed.GetStatus();
        node.padding = parsed.Value();
        node.hasPadding = true;
    }
    const Base::StringView orientation = Attribute(element, "Orientation");
    if (!orientation.Empty()) {
        if (orientation == Base::StringView("Horizontal")) {
            node.orientation = Controls::Orientation::Horizontal;
        } else if (orientation == Base::StringView("Vertical")) {
            node.orientation = Controls::Orientation::Vertical;
        } else {
            return InvalidTheme("Theme StackPanel orientation is invalid");
        }
        node.hasOrientation = true;
    }

    const std::uint32_t nodeIndex = controlTemplate.visualTree.Size();
    assigned = controlTemplate.visualTree.TryPushBack(std::move(node));
    if (!assigned) return assigned.GetStatus();
    for (std::uint32_t child : element.children) {
        assigned = ParseVisualNode(
            document, child, nodeIndex, dictionary, controlTemplate);
        if (!assigned) return assigned.GetStatus();
    }
    return {};
}

Base::Result<ThemeSetterResource> ParseSetter(
    const ThemeXamlElement& element) noexcept {
    ThemeSetterResource setter;
    Base::StringView targetName = Attribute(element, "TargetName");
    Base::StringView property = Attribute(element, "Property");
    Base::StringView resource = Attribute(element, "Resource");
    if (targetName.Empty() || property.Empty() || resource.Empty()) {
        return InvalidTheme(
            "VisualState setter target, property, or resource is missing");
    }
    Base::Result<void> assigned = setter.targetName.TryAssign(targetName);
    if (!assigned) return assigned.GetStatus();
    assigned = setter.property.TryAssign(property);
    if (!assigned) return assigned.GetStatus();
    assigned = setter.resource.TryAssign(resource);
    if (!assigned) return assigned.GetStatus();
    return setter;
}

Base::Result<ThemeVisualStateGroupResource> ParseStateGroup(
    const ThemeXamlDocument& document,
    const ThemeXamlElement& groupElement) noexcept {
    ThemeVisualStateGroupResource group;
    Base::Result<void> assigned = group.name.TryAssign(
        Attribute(groupElement, "Name"));
    if (!assigned || group.name.Empty()) {
        return assigned
            ? InvalidTheme("VisualStateGroup requires Name")
            : assigned.GetStatus();
    }
    for (std::uint32_t stateIndex : groupElement.children) {
        const ThemeXamlElement& stateElement = document.elements[stateIndex];
        if (stateElement.name.View() != Base::StringView("VisualState")) {
            return InvalidTheme("VisualStateGroup only accepts VisualState");
        }
        ThemeVisualStateResource state;
        assigned = state.name.TryAssign(Attribute(stateElement, "Name"));
        if (!assigned || state.name.Empty()) {
            return assigned
                ? InvalidTheme("VisualState requires Name")
                : assigned.GetStatus();
        }
        for (std::uint32_t setterIndex : stateElement.children) {
            const ThemeXamlElement& setterElement = document.elements[setterIndex];
            if (setterElement.name.View() != Base::StringView("Setter")) {
                return InvalidTheme("VisualState only accepts Setter");
            }
            Base::Result<ThemeSetterResource> setter = ParseSetter(setterElement);
            if (!setter) return setter.GetStatus();
            assigned = state.setters.TryPushBack(std::move(setter).Value());
            if (!assigned) return assigned.GetStatus();
        }
        assigned = group.states.TryPushBack(std::move(state));
        if (!assigned) return assigned.GetStatus();
    }
    return group;
}

Base::Result<void> ParseGenericTheme(
    const ThemeXamlDocument& document,
    ThemeResourceDictionary& dictionary) noexcept {
    const ThemeXamlElement& root = document.elements[document.root];
    if (root.name.View() != Base::StringView("ResourceDictionary")) {
        return InvalidTheme("Generic theme root must be ResourceDictionary");
    }
    for (std::uint32_t templateIndex : root.children) {
        const ThemeXamlElement& templateElement = document.elements[templateIndex];
        if (templateElement.name.View() != Base::StringView("ControlTemplate")) {
            return InvalidTheme("Generic theme only accepts ControlTemplate");
        }
        ThemeControlTemplateResource resource;
        Base::StringView targetType = Attribute(templateElement, "TargetType");
        if (targetType.Empty()) {
            return InvalidTheme("ControlTemplate requires TargetType");
        }
        Base::Result<void> assigned = resource.targetType.TryAssign(targetType);
        if (!assigned) return assigned.GetStatus();
        for (const ThemeControlTemplateResource& existing : dictionary.templates) {
            if (existing.targetType.View() == targetType) {
                return InvalidTheme("Theme template TargetType is duplicated");
            }
        }
        const ThemeXamlElement* visualTree = nullptr;
        const ThemeXamlElement* stateGroups = nullptr;
        for (std::uint32_t childIndex : templateElement.children) {
            const ThemeXamlElement& child = document.elements[childIndex];
            if (child.name.View() == Base::StringView("VisualTree")) {
                visualTree = &child;
            } else if (child.name.View() == Base::StringView("VisualStateGroups")) {
                stateGroups = &child;
            } else {
                return InvalidTheme("ControlTemplate child is unsupported");
            }
        }
        if (visualTree == nullptr || visualTree->children.Size() != 1U ||
            stateGroups == nullptr) {
            return InvalidTheme(
                "ControlTemplate requires one visual root and states");
        }
        assigned = ParseVisualNode(
            document, visualTree->children[0], UINT32_MAX,
            dictionary, resource);
        if (!assigned) return assigned.GetStatus();
        for (std::uint32_t groupIndex : stateGroups->children) {
            const ThemeXamlElement& groupElement = document.elements[groupIndex];
            if (groupElement.name.View() != Base::StringView("VisualStateGroup")) {
                return InvalidTheme("VisualStateGroups only accepts groups");
            }
            Base::Result<ThemeVisualStateGroupResource> group =
                ParseStateGroup(document, groupElement);
            if (!group) return group.GetStatus();
            assigned = resource.visualStateGroups.TryPushBack(
                std::move(group).Value());
            if (!assigned) return assigned.GetStatus();
        }
        assigned = dictionary.templates.TryPushBack(std::move(resource));
        if (!assigned) return assigned.GetStatus();
    }
    return {};
}

} // namespace

const ThemeColorResource* ThemeResourceDictionary::FindColor(
    Base::StringView key) const noexcept {
    for (const ThemeColorResource& entry : colors) {
        if (entry.key.View() == key) return &entry;
    }
    return nullptr;
}

ThemeResourceDictionaryObject::ThemeResourceDictionaryObject(
    Base::MetaTypeId runtimeType) noexcept
    : runtimeType_(runtimeType), dictionary_() {}

Base::MetaTypeId ThemeResourceDictionaryObject::RuntimeType() const noexcept {
    return runtimeType_;
}

Base::Result<void> ThemeResourceDictionaryObject::AddColor(
    ThemeColorResource color) noexcept {
    return dictionary_.colors.TryPushBack(std::move(color));
}

Base::Result<void> ThemeResourceDictionaryObject::AddTemplate(
    ThemeControlTemplateResource controlTemplate) noexcept {
    return dictionary_.templates.TryPushBack(std::move(controlTemplate));
}

ThemeResourceDictionary ThemeResourceDictionaryObject::TakeDictionary() noexcept {
    return std::move(dictionary_);
}

Base::Result<ThemeResourceDictionary> LoadThemeResourceDictionary(
    Base::StringView genericXaml,
    Base::StringView paletteXaml) noexcept {
    Base::Result<ThemeXamlDocument> generic = ReadThemeXaml(genericXaml);
    if (!generic) return generic.GetStatus();
    Base::Result<ThemeXamlDocument> palette = ReadThemeXaml(paletteXaml);
    if (!palette) return palette.GetStatus();

    ThemeResourceDictionary dictionary;
    Base::Result<void> parsed = ParsePalette(palette.Value(), dictionary);
    if (!parsed) return parsed.GetStatus();
    parsed = ParseGenericTheme(generic.Value(), dictionary);
    if (!parsed) return parsed.GetStatus();
    return dictionary;
}

} // namespace Aero::Markup
