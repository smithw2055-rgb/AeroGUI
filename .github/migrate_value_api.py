from pathlib import Path

root = Path('.')


def read(path):
    return (root / path).read_text()


def write(path, content):
    (root / path).write_text(content)


def replace_once(content, old, new, label):
    count = content.count(old)
    if count != 1:
        raise RuntimeError(f'{label}: expected 1 occurrence, found {count}')
    return content.replace(old, new, 1)


for path in ['src/core/Controls.cpp', 'src/core/Layout.cpp']:
    content = read(path)
    content = content.replace(
        'PropertyRegistry().Types().TryCreateValue(',
        'MetadataRegistrationValues(PropertyRegistry().Types()).TryCreateValue(')
    write(path, content)

path = 'src/core/Presentation.cpp'
content = read(path)
content = content.replace(
    'return types->TryCreateValue(type, &length);',
    'return MetadataRegistrationValues(*types).TryCreateValue(type, &length);')
content = content.replace(
    'return static_cast<TypeRegistry*>(context)->TryCreateValue(\n        type, &parsed.Value());',
    'return MetadataRegistrationValues(\n        *static_cast<TypeRegistry*>(context)).TryCreateValue(\n            type, &parsed.Value());')
content = content.replace(
    'return static_cast<TypeRegistry*>(context)->TryCreateValue(\n        type, &color);',
    'return MetadataRegistrationValues(\n        *static_cast<TypeRegistry*>(context)).TryCreateValue(type, &color);')
content = content.replace(
    'context.types.TryCreateValue(',
    'context.Values().TryCreateValue(')
needle = '''    status = types.TryRegisterValueSemantics(BuiltinTypes::Length,
        {sizeof(Length), alignof(Length), nullptr, nullptr,
         &EqualLength, nullptr, true});'''
replacement = '''    MetadataRegistrationValues values(types);
    status = values.TryRegisterValueSemantics(BuiltinTypes::Length,
        {sizeof(Length), alignof(Length), nullptr, nullptr,
         &EqualLength, nullptr, true});'''
content = replace_once(
    content, needle, replacement, 'presentation service declaration')
content = content.replace(
    'types.TryRegisterValueSemantics(',
    'values.TryRegisterValueSemantics(')
content = content.replace(
    'types.TryRegisterTextConverter(converter)',
    'values.TryRegisterTextConverter(converter)')
write(path, content)

path = 'src/markup/XamlSchemaContext.cpp'
content = read(path)
content = replace_once(
    content,
    '#include <Aero/Markup/XamlSchemaContext.hpp>\n',
    '#include <Aero/Markup/XamlSchemaContext.hpp>\n\n'
    '#include <Aero/Core/MetadataRegistrationValues.hpp>\n',
    'xaml include')
content = replace_once(
    content,
    'Base::Result<Core::Value> reflected = types_->TryConvertText(type, text);',
    'Base::Result<Core::Value> reflected =\n'
    '        Core::MetadataRegistrationValues(*types_).TryConvertText(type, text);',
    'xaml conversion')
write(path, content)

path = 'tests/unit/TypeRegistryTests.cpp'
content = read(path)
content = replace_once(
    content,
    '#include <Aero/Core/TypeRegistry.hpp>\n',
    '#include <Aero/Core/TypeRegistry.hpp>\n'
    '#include <Aero/Core/MetadataRegistrationValues.hpp>\n',
    'registry test include')
content = content.replace(
    'return registry->TryCreateValue(type, &value);',
    'return MetadataRegistrationValues(*registry).TryCreateValue(type, &value);')
replacement = '''    CHECK(registry.TryRegisterType({ns, StringView("Object"), InvalidTypeId,
        TypeFlags::None, nullptr}));
    MetadataRegistrationValues values(registry);'''
content = replace_once(
    content,
    '''    CHECK(registry.TryRegisterType({ns, StringView("Object"), InvalidTypeId,
        TypeFlags::None, nullptr}));''',
    replacement,
    'registry test values declaration')
content = content.replace(
    'registry.TryRegisterValueSemantics(',
    'values.TryRegisterValueSemantics(')
content = content.replace(
    'registry.TryRegisterTextConverter(',
    'values.TryRegisterTextConverter(')
content = content.replace(
    'registry.TryCreateValue(',
    'values.TryCreateValue(')
content = content.replace(
    'registry.TryConvertText(',
    'values.TryConvertText(')
content = content.replace(
    'Unified Value and registry semantics',
    'Unified Value and registration service semantics')
write(path, content)

path = 'tests/unit/XamlCustomControlTests.cpp'
content = read(path)
content = replace_once(
    content,
    '#include <Aero/Core/MetadataRuntime.hpp>\n',
    '#include <Aero/Core/MetadataRuntime.hpp>\n'
    '#include <Aero/Core/MetadataRegistrationValues.hpp>\n',
    'custom test include')
content = content.replace(
    'return types->TryCreateValue(type, &radius);',
    'return MetadataRegistrationValues(*types).TryCreateValue(type, &radius);')
content = content.replace(
    'context.types.TryCreateValue(',
    'context.Values().TryCreateValue(')
needle = '''    Result<void> status = context.types.TryRegisterValueSemantics(
        cornerRadiusType,
        {sizeof(CornerRadius), alignof(CornerRadius), nullptr, nullptr,
         &EqualCornerRadius, nullptr, true});'''
replacement = '''    MetadataRegistrationValues values = context.Values();
    Result<void> status = values.TryRegisterValueSemantics(
        cornerRadiusType,
        {sizeof(CornerRadius), alignof(CornerRadius), nullptr, nullptr,
         &EqualCornerRadius, nullptr, true});'''
content = replace_once(
    content, needle, replacement, 'custom registration values')
content = content.replace(
    'status = context.types.TryRegisterTextConverter(',
    'status = values.TryRegisterTextConverter(')
write(path, content)

path = 'include/Aero/Core/TypeRegistry.hpp'
content = read(path)
content = replace_once(
    content,
    'class MetaRegistrationBuilder;\n',
    'class MetaRegistrationBuilder;\nclass MetadataRegistrationValues;\n',
    'service forward declaration')
public_block = '''    Base::Result<void> TryRegisterValueSemantics(
        TypeId type,
        const ValueTypeRegistration& registration) noexcept;
    Base::Result<void> TryRegisterTextConverter(
        const TextValueConverterRegistration& registration) noexcept;
    Base::Result<Value> TryCreateValue(
        TypeId type,
        const void* source) const noexcept;
    Base::Result<Value> TryConvertText(
        TypeId type,
        Base::StringView text) const noexcept;

'''
content = replace_once(
    content, public_block, '', 'remove public value methods')
lookup_block = '''    // Seal-time export surface used to transfer value behavior into owned
    // facets. Returned registrations remain immutable after Freeze().
    const Base::Ref<ValueTypeSemantics>* FindValueSemantics(
        TypeId type) const noexcept;
    const TextValueConverterRegistration* FindTextConverter(
        TypeId type) const noexcept;

'''
content = replace_once(
    content, lookup_block, '', 'remove public value lookups')
private_marker = '''private:
    struct MemberLocation final {'''
private_replacement = '''private:
    friend class MetadataRegistrationValues;

    // Storage backend for the explicit registration-domain value service.
    Base::Result<void> TryRegisterValueSemantics(
        TypeId type,
        const ValueTypeRegistration& registration) noexcept;
    Base::Result<void> TryRegisterTextConverter(
        const TextValueConverterRegistration& registration) noexcept;
    const Base::Ref<ValueTypeSemantics>* FindValueSemantics(
        TypeId type) const noexcept;
    const TextValueConverterRegistration* FindTextConverter(
        TypeId type) const noexcept;

    struct MemberLocation final {'''
content = replace_once(
    content, private_marker, private_replacement, 'private value backend')
write(path, content)

path = 'src/core/TypeRegistry.cpp'
content = read(path)
execution = '''Base::Result<Value> TypeRegistry::TryCreateValue(
    TypeId type,
    const void* source) const noexcept {
    const Base::Ref<ValueTypeSemantics>* semantics = FindValueSemantics(type);
    return semantics != nullptr
        ? Value::TryFromCustom(type, source, *semantics)
        : Base::Result<Value>(Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Value type semantics are not registered"));
}

Base::Result<Value> TypeRegistry::TryConvertText(
    TypeId type,
    Base::StringView text) const noexcept {
    const TextValueConverterRegistration* entry = FindTextConverter(type);
    if (entry == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Text value converter is not registered");
    }
    Base::Result<Value> converted = entry->convert(type, text, entry->context);
    if (converted && converted.Value().Type() != type) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Text converter returned a value with the wrong type");
    }
    return converted;
}

'''
content = replace_once(
    content, execution, '', 'remove registry execution definitions')
write(path, content)
