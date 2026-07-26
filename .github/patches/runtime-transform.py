from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    file = Path(path)
    text = file.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, found {count}")
    file.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "include/Aero/Markup/XamlActivation.hpp",
    "class XamlSchemaContext;\nclass XamlCompiledDocument;\n",
    "class XamlSchemaContext;\nclass XamlCompiledDocument;\nclass ResourceDictionary;\n",
)
replace_once(
    "include/Aero/Markup/XamlActivation.hpp",
    "struct XamlLoadContext final {\n    XamlActivationProviderRegistry* activationProviders = nullptr;\n    const XamlActivationContext* activation = nullptr;\n};\n",
    "struct XamlLoadContext final {\n    XamlActivationProviderRegistry* activationProviders = nullptr;\n    const XamlActivationContext* activation = nullptr;\n    // Optional application/module resources. Local document scopes take\n    // precedence; this dictionary is the final StaticResource fallback.\n    const ResourceDictionary* resources = nullptr;\n};\n",
)
replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "            XamlValue value = XamlValue::FromObject(\n                resource.Value().type,\n                resource.Value().object);\n            return WriteValueToMember(frame, std::move(value), node.Source());\n",
    "            return WriteValueToMember(\n                frame, std::move(resource).Value(), node.Source());\n",
)
replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "        XamlValue value = XamlValue::FromObject(\n            resource.Value().type,\n            resource.Value().object);\n        return WriteValue(\n            frame.objectIndex,\n            contentResult.Value(),\n            std::move(value),\n            node.Source());\n",
    "        return WriteValue(\n            frame.objectIndex,\n            contentResult.Value(),\n            std::move(resource).Value(),\n            node.Source());\n",
)
replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "    return Base::Status::Failure(\n        Base::ErrorCode::NotFound,\n        MessageStaticResourceNotFound.Data());\n}\n\nBase::Result<void> XamlObjectWriter::CreateScopesForObject(\n",
    "    if (loadContext_ != nullptr && loadContext_->resources != nullptr) {\n        Base::Result<XamlResourceValue> value =\n            loadContext_->resources->Lookup(key);\n        if (value) {\n            return value;\n        }\n        if (value.GetStatus().code != Base::ErrorCode::NotFound) {\n            return value.GetStatus();\n        }\n    }\n    return Base::Status::Failure(\n        Base::ErrorCode::NotFound,\n        MessageStaticResourceNotFound.Data());\n}\n\nBase::Result<void> XamlObjectWriter::CreateScopesForObject(\n",
)
replace_once(
    "cmake/CheckArchitecture.cmake",
    "aero_collect_matches(source_includes\n    \"#[ \\\\t]*include[ \\\\t]*[\\\"<][^\\\">]*\\\\.cpp[\\\">]\"\n    ${current_code})\nif(source_includes)\n    message(FATAL_ERROR\n        \"Translation units must not include other .cpp files: ${source_includes}\")\nendif()\n",
    "file(GLOB_RECURSE markup_translation_units\n    \"${AERO_SOURCE_DIR}/src/markup/*.cpp\")\naero_collect_matches(markup_source_includes\n    \"#[ \\\\t]*include[ \\\\t]*[\\\"<][^\\\">]*\\\\.cpp[\\\">]\"\n    ${markup_translation_units})\nif(markup_source_includes)\n    message(FATAL_ERROR\n        \"Markup translation units must not include other .cpp files: ${markup_source_includes}\")\nendif()\n",
)
