from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    file = Path(path)
    text = file.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, found {count}")
    file.write_text(text.replace(old, new, 1), encoding="utf-8")


path = "src/markup/XamlThemeResources.cpp"
replace_once(
    path,
    '#include <Aero/Markup/XamlNodeReader.hpp>\n#include <Aero/Markup/XmlTokenizer.hpp>\n\n#include <cmath>\n',
    '#include <Aero/Base/Ref.hpp>\n#include <Aero/Core/Metadata/MetadataDomain.hpp>\n#include <Aero/Core/Metadata/MetadataRuntime.hpp>\n#include <Aero/Markup/XamlNodeReader.hpp>\n#include <Aero/Markup/XamlObjectWriter.hpp>\n#include <Aero/Markup/XamlSchemaContext.hpp>\n#include <Aero/Markup/XmlTokenizer.hpp>\n\n#include <cmath>\n',
)

marker = '''Base::Status InvalidTheme(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed, message);
}

'''
insert = marker + r'''constexpr Base::StringView ThemeXamlNamespace("urn:aero/themes");
constexpr Base::StringView ThemeMetadataModuleName(
    "Aero.Markup.BuiltinThemePalette");

Core::TypeId ThemeObjectType() noexcept {
    return Core::MakeTypeId(ThemeXamlNamespace, Base::StringView("Object"));
}

Core::TypeId ThemeStringType() noexcept {
    return Core::MakeTypeId(ThemeXamlNamespace, Base::StringView("String"));
}

Core::TypeId ThemePaletteType() noexcept {
    return Core::MakeTypeId(
        ThemeXamlNamespace, Base::StringView("ResourceDictionary"));
}

Core::TypeId ThemeColorType() noexcept {
    return Core::MakeTypeId(ThemeXamlNamespace, Base::StringView("Color"));
}

Core::MemberId ThemeVariantMember() noexcept {
    return Core::MakeMemberId(
        ThemePaletteType(), Core::MemberKind::Property,
        Base::StringView("Variant"));
}

Core::MemberId ThemeColorValueMember() noexcept {
    return Core::MakeMemberId(
        ThemeColorType(), Core::MemberKind::Property,
        Base::StringView("Value"));
}

class ThemePaletteObject final : public Base::Object {
public:
    Base::MetaTypeId RuntimeType() const noexcept override {
        return ThemePaletteType();
    }

    Base::Result<void> SetVariant(Base::StringView name) noexcept {
        if (name == Base::StringView("Light")) {
            dictionary_.variant = ThemeVariant::Light;
            variantSet_ = true;
            return {};
        }
        if (name == Base::StringView("Dark")) {
            dictionary_.variant = ThemeVariant::Dark;
            variantSet_ = true;
            return {};
        }
        return InvalidTheme("Theme palette Variant must be Light or Dark");
    }

    Base::Result<void> AddColor(
        Base::StringView key,
        const Presentation::Color& value) noexcept {
        if (!variantSet_ || key.Empty() || dictionary_.FindColor(key) != nullptr) {
            return InvalidTheme(
                "Theme color key is missing, duplicated, or precedes Variant");
        }
        ThemeColorResource entry;
        Base::Result<void> assigned = entry.key.TryAssign(key);
        if (!assigned) return assigned.GetStatus();
        entry.value = value;
        return dictionary_.colors.TryPushBack(std::move(entry));
    }

    Base::Result<ThemeResourceDictionary> TakeDictionary() noexcept {
        if (!variantSet_) {
            return InvalidTheme("Theme palette requires Variant");
        }
        return std::move(dictionary_);
    }

private:
    ThemeResourceDictionary dictionary_;
    bool variantSet_ = false;
};

class ThemeColorObject final : public Base::Object {
public:
    Base::MetaTypeId RuntimeType() const noexcept override {
        return ThemeColorType();
    }

    Base::Result<void> SetValue(Base::StringView text) noexcept {
        Base::Result<Presentation::Color> parsed = ParseColor(text);
        if (!parsed) return parsed.GetStatus();
        value_ = parsed.Value();
        valueSet_ = true;
        return {};
    }

    bool HasValue() const noexcept { return valueSet_; }
    const Presentation::Color& Value() const noexcept { return value_; }

private:
    Presentation::Color value_;
    bool valueSet_ = false;
};

Base::Result<Base::Ref<Base::Object>> MakeThemePalette() noexcept {
    Base::Result<Base::Ref<ThemePaletteObject>> created =
        Base::MakeRef<ThemePaletteObject>();
    if (!created) return created.GetStatus();
    Base::Ref<ThemePaletteObject> typed = std::move(created).Value();
    return Base::Ref<Base::Object>(std::move(typed));
}

Base::Result<Base::Ref<Base::Object>> MakeThemeColor() noexcept {
    Base::Result<Base::Ref<ThemeColorObject>> created =
        Base::MakeRef<ThemeColorObject>();
    if (!created) return created.GetStatus();
    Base::Ref<ThemeColorObject> typed = std::move(created).Value();
    return Base::Ref<Base::Object>(std::move(typed));
}

Base::Result<Core::Value> ConvertThemeString(
    Core::TypeId type,
    Base::StringView text,
    void*) noexcept {
    return Core::Value::TryFromString(type, text);
}

Base::Result<void> RegisterThemePaletteMetadata(
    Core::MetaRegistrationContext& context,
    void*) noexcept {
    Core::MetadataRegistrationTypes types = context.Types();
    const Core::TypeRegistration registrations[] = {
        Core::TypeRegistration::Object(
            ThemeXamlNamespace, "Object"),
        Core::TypeRegistration::Primitive(
            ThemeXamlNamespace, "String"),
        Core::TypeRegistration::Object(
            ThemeXamlNamespace, "ResourceDictionary", ThemeObjectType(),
            Core::TypeFlags::Sealed, &MakeThemePalette),
        Core::TypeRegistration::Object(
            ThemeXamlNamespace, "Color", ThemeObjectType(),
            Core::TypeFlags::Sealed, &MakeThemeColor),
    };
    for (const Core::TypeRegistration& registration : registrations) {
        Base::Result<Core::TypeId> registered =
            types.TryRegisterType(registration);
        if (!registered) return registered.GetStatus();
    }
    Base::Result<void> converter =
        context.Values().TryRegisterTextConverter({
            ThemeStringType(), &ConvertThemeString, nullptr});
    if (!converter) return converter.GetStatus();

    Base::Result<Core::MemberId> member = types.TryRegisterProperty(
        ThemePaletteType(),
        {Base::StringView("Variant"), ThemeStringType(),
         Core::PropertyFlags::None});
    if (!member) return member.GetStatus();
    member = types.TryRegisterProperty(
        ThemeColorType(),
        {Base::StringView("Value"), ThemeStringType(),
         Core::PropertyFlags::None});
    if (!member) return member.GetStatus();
    return {};
}

Base::Result<void> SetThemeVariant(
    Base::Object& object,
    const XamlValue& value,
    void*) noexcept {
    if (object.RuntimeType() != ThemePaletteType() ||
        value.Kind() != XamlValueKind::String) {
        return InvalidTheme("Theme Variant value is invalid");
    }
    return static_cast<ThemePaletteObject&>(object).SetVariant(
        value.AsString());
}

Base::Result<void> SetThemeColorValue(
    Base::Object& object,
    const XamlValue& value,
    void*) noexcept {
    if (object.RuntimeType() != ThemeColorType() ||
        value.Kind() != XamlValueKind::String) {
        return InvalidTheme("Theme Color value is invalid");
    }
    return static_cast<ThemeColorObject&>(object).SetValue(
        value.AsString());
}

Base::Result<void> AddThemeResource(
    Base::Object& scopeOwner,
    Base::StringView key,
    Core::TypeId valueType,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (scopeOwner.RuntimeType() != ThemePaletteType() ||
        valueType != ThemeColorType() || !value ||
        value->RuntimeType() != ThemeColorType()) {
        return InvalidTheme("Theme palettes only accept Color resources");
    }
    const auto& color = static_cast<const ThemeColorObject&>(*value);
    if (!color.HasValue()) {
        return InvalidTheme("Theme Color requires Value");
    }
    return static_cast<ThemePaletteObject&>(scopeOwner).AddColor(
        key, color.Value());
}

Base::Result<ThemeResourceDictionary> LoadThemePalette(
    Base::StringView paletteXaml) noexcept {
    Core::MetadataDomain metadata;
    Base::Result<void> registered = metadata.TryRegisterModule({
        Core::MakeMetadataModuleId(ThemeMetadataModuleName),
        ThemeMetadataModuleName,
        1U,
        &RegisterThemePaletteMetadata,
        nullptr});
    if (registered) registered = metadata.Seal();
    if (!registered) return registered.GetStatus();

    Core::MetadataRuntime runtime(metadata);
    XamlSchemaContext schema(metadata, runtime);
    registered = schema.TryRegisterMemberAdapter({
        ThemeVariantMember(), XamlMemberWriteMode::SetOnce,
        &SetThemeVariant, nullptr});
    if (registered) {
        registered = schema.TryRegisterMemberAdapter({
            ThemeColorValueMember(), XamlMemberWriteMode::SetOnce,
            &SetThemeColorValue, nullptr});
    }
    if (registered) {
        registered = schema.TryRegisterTypeAdapter({
            ThemePaletteType(),
            nullptr, nullptr, nullptr, nullptr,
            false, true, nullptr, &AddThemeResource});
    }
    if (registered) registered = runtime.Freeze();
    if (registered) registered = schema.Freeze();
    if (!registered) return registered.GetStatus();

    Utf8XmlTokenizer tokenizer;
    registered = tokenizer.Reset(paletteXaml);
    if (!registered) return registered.GetStatus();
    XamlNodeReader reader(tokenizer);
    XamlObjectWriter writer(schema);
    Base::Result<Base::Ref<Base::Object>> loaded = writer.Load(reader);
    if (!loaded) return loaded.GetStatus();
    Base::Ref<Base::Object> root = std::move(loaded).Value();
    if (!root || root->RuntimeType() != ThemePaletteType()) {
        return InvalidTheme("Theme palette root must be ResourceDictionary");
    }
    return static_cast<ThemePaletteObject&>(*root).TakeDictionary();
}

'''
# ParseColor is declared later in the file; add a forward declaration before the
# metadata-created Color object uses it.
insert = insert.replace(
    'class ThemePaletteObject final',
    'Base::Result<Presentation::Color> ParseColor(\n'
    '    Base::StringView value) noexcept;\n\n'
    'class ThemePaletteObject final',
)
replace_once(path, marker, insert)

old_palette = '''Base::Result<void> ParsePalette(
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
        Base::Result<void> assigned = entry.key.TryAssign(key);
        if (!assigned) return assigned.GetStatus();
        entry.value = color.Value();
        assigned = dictionary.colors.TryPushBack(std::move(entry));
        if (!assigned) return assigned.GetStatus();
    }
    return {};
}

'''
replace_once(path, old_palette, "")

replace_once(
    path,
    '''    Base::Result<ThemeXamlDocument> generic = ReadThemeXaml(genericXaml);
    if (!generic) return generic.GetStatus();
    Base::Result<ThemeXamlDocument> palette = ReadThemeXaml(paletteXaml);
    if (!palette) return palette.GetStatus();

    ThemeResourceDictionary dictionary;
    Base::Result<void> parsed = ParsePalette(palette.Value(), dictionary);
    if (!parsed) return parsed.GetStatus();
    parsed = ParseGenericTheme(generic.Value(), dictionary);
''',
    '''    Base::Result<ThemeResourceDictionary> palette =
        LoadThemePalette(paletteXaml);
    if (!palette) return palette.GetStatus();
    Base::Result<ThemeXamlDocument> generic = ReadThemeXaml(genericXaml);
    if (!generic) return generic.GetStatus();

    ThemeResourceDictionary dictionary = std::move(palette).Value();
    Base::Result<void> parsed = ParseGenericTheme(generic.Value(), dictionary);
''',
)

# Update the architecture status document with the completed migration boundary.
doc = "docs/MARKUP_RUNTIME_MODULE_REFACTOR.md"
replace_once(
    doc,
    '14. The built-in theme parser and DTOs are private implementation details. They are compiled as a normal translation unit; public `ThemeResourceDictionaryObject` and `.cpp` inclusion shortcuts are prohibited by architecture checks.\n',
    '14. The built-in theme parser and DTOs are private implementation details. They are compiled as a normal translation unit; public `ThemeResourceDictionaryObject` and `.cpp` inclusion shortcuts are prohibited by architecture checks.\n'
    '15. Built-in palette XAML (`Light.xaml` and `Dark.xaml`) is loaded by `XamlObjectWriter` through private metadata-registered `ResourceDictionary` and `Color` objects. `x:Key`, resource scopes, member conversion, and duplicate-key validation are no longer implemented by the theme parser.\n',
)
replace_once(
    doc,
    '1. model `Style`, `Setter`, `Trigger`, `ControlTemplate`, visual states, and template content as metadata-created objects;\n2. load `Generic.xaml` through `XamlObjectWriter` and the same `ResourceDictionary/Core::Value` pipeline as application XAML;\n3. delete `XamlThemeResources.cpp` once the built-in catalog no longer needs materialization DTOs.\n',
    '1. model `ControlTemplate`, visual states, and template content as metadata-created objects;\n2. load `Generic.xaml` through `XamlObjectWriter` and the same `ResourceDictionary/Core::Value` pipeline already used by palette XAML;\n3. delete the remaining `ThemeXamlDocument` parser and then `XamlThemeResources.cpp` once template materialization no longer needs DTOs.\n',
)
