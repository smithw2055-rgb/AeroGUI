from pathlib import Path
import re


def replace_once(path: str, old: str, new: str) -> None:
    file = Path(path)
    text = file.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, found {count}")
    file.write_text(text.replace(old, new, 1), encoding="utf-8")


def replace_all(path: str, old: str, new: str, minimum: int = 1) -> None:
    file = Path(path)
    text = file.read_text(encoding="utf-8")
    count = text.count(old)
    if count < minimum:
        raise RuntimeError(f"{path}: expected at least {minimum} matches, found {count}")
    file.write_text(text.replace(old, new), encoding="utf-8")


# Resource callbacks now receive the same generic Core::Value stored by the
# dictionary, so scalar value elements do not need an object wrapper.
replace_once(
    "include/Aero/Markup/XamlSchemaContext.hpp",
    "using XamlAddResourceCallback = Base::Result<void> (*)(\n"
    "    Base::Object& scopeOwner,\n"
    "    Base::StringView key,\n"
    "    Core::TypeId valueType,\n"
    "    const Base::Ref<Base::Object>& value,\n"
    "    void* context) noexcept;\n",
    "using XamlAddResourceCallback = Base::Result<void> (*)(\n"
    "    Base::Object& scopeOwner,\n"
    "    Base::StringView key,\n"
    "    const XamlValue& value,\n"
    "    void* context) noexcept;\n",
)
replace_once(
    "include/Aero/Markup/XamlSchemaContext.hpp",
    "    Base::Result<void> AddResource(\n"
    "        Core::TypeId scopeType,\n"
    "        Base::Object& scopeOwner,\n"
    "        Base::StringView key,\n"
    "        Core::TypeId valueType,\n"
    "        const Base::Ref<Base::Object>& value) const noexcept;\n",
    "    Base::Result<void> AddResource(\n"
    "        Core::TypeId scopeType,\n"
    "        Base::Object& scopeOwner,\n"
    "        Base::StringView key,\n"
    "        const XamlValue& value) const noexcept;\n",
)
replace_once(
    "src/markup/XamlSchemaContext.cpp",
    "Base::Result<void> XamlSchemaContext::AddResource(\n"
    "    Core::TypeId scopeType,\n"
    "    Base::Object& scopeOwner,\n"
    "    Base::StringView key,\n"
    "    Core::TypeId valueType,\n"
    "    const Base::Ref<Base::Object>& value) const noexcept {\n"
    "    const XamlTypeAdapterRegistration* adapter =\n"
    "        FindTypeAdapter(scopeType);\n"
    "    if (adapter == nullptr || adapter->addResource == nullptr) return {};\n"
    "    return adapter->addResource(\n"
    "        scopeOwner,\n"
    "        key,\n"
    "        valueType,\n"
    "        value,\n"
    "        adapter->context);\n"
    "}\n",
    "Base::Result<void> XamlSchemaContext::AddResource(\n"
    "    Core::TypeId scopeType,\n"
    "    Base::Object& scopeOwner,\n"
    "    Base::StringView key,\n"
    "    const XamlValue& value) const noexcept {\n"
    "    const XamlTypeAdapterRegistration* adapter =\n"
    "        FindTypeAdapter(scopeType);\n"
    "    if (adapter == nullptr || adapter->addResource == nullptr) return {};\n"
    "    return adapter->addResource(\n"
    "        scopeOwner,\n"
    "        key,\n"
    "        value,\n"
    "        adapter->context);\n"
    "}\n",
)
replace_once(
    "tests/markup/XamlDirectivesResourcesTests.cpp",
    "    friend Result<void> AddResource(\n"
    "        Object&,\n"
    "        StringView,\n"
    "        TypeId,\n"
    "        const Ref<Object>&,\n"
    "        void*) noexcept;\n",
    "    friend Result<void> AddResource(\n"
    "        Object&,\n"
    "        StringView,\n"
    "        const XamlValue&,\n"
    "        void*) noexcept;\n",
)
replace_once(
    "tests/markup/XamlDirectivesResourcesTests.cpp",
    "Result<void> AddResource(\n"
    "    Object& scopeOwner,\n"
    "    StringView key,\n"
    "    TypeId valueType,\n"
    "    const Ref<Object>& value,\n"
    "    void*) noexcept {\n"
    "    return static_cast<DirectiveNode&>(scopeOwner).resources_.TryAdd(\n"
    "        key,\n"
    "        valueType,\n"
    "        value);\n"
    "}\n",
    "Result<void> AddResource(\n"
    "    Object& scopeOwner,\n"
    "    StringView key,\n"
    "    const XamlValue& value,\n"
    "    void*) noexcept {\n"
    "    return static_cast<DirectiveNode&>(scopeOwner).resources_.TryAdd(\n"
    "        key,\n"
    "        value);\n"
    "}\n",
)

# Name scopes expose reverse lookup so template prototypes retain ordinary
# x:Name semantics without a theme-specific name table.
replace_once(
    "include/Aero/Markup/XamlNamesResources.hpp",
    "    Base::Object* Find(\n"
    "        Base::StringView name) const noexcept;\n\n"
    "    void Clear() noexcept;\n",
    "    Base::Object* Find(\n"
    "        Base::StringView name) const noexcept;\n"
    "    Base::StringView NameOf(\n"
    "        const Base::Object& object) const noexcept;\n\n"
    "    void Clear() noexcept;\n",
)
replace_once(
    "src/markup/XamlNamesResources.cpp",
    "Base::Object* NameScope::Find(Base::StringView name) const noexcept {\n"
    "    for (const Entry& entry : entries_) {\n"
    "        if (entry.name.View() == name) {\n"
    "            return entry.object;\n"
    "        }\n"
    "    }\n"
    "    return nullptr;\n"
    "}\n\n"
    "void NameScope::Clear() noexcept {\n",
    "Base::Object* NameScope::Find(Base::StringView name) const noexcept {\n"
    "    for (const Entry& entry : entries_) {\n"
    "        if (entry.name.View() == name) {\n"
    "            return entry.object;\n"
    "        }\n"
    "    }\n"
    "    return nullptr;\n"
    "}\n\n"
    "Base::StringView NameScope::NameOf(\n"
    "    const Base::Object& object) const noexcept {\n"
    "    for (const Entry& entry : entries_) {\n"
    "        if (entry.object == &object) return entry.name.View();\n"
    "    }\n"
    "    return {};\n"
    "}\n\n"
    "void NameScope::Clear() noexcept {\n",
)

# Named template roots participate in the same part lookup as descendants.
replace_once(
    "include/Aero/Controls/Templates.hpp",
    "    Base::Result<void> SetRoot(\n"
    "        Base::Ref<Base::Object> owner,\n"
    "        Visual& root) noexcept;\n",
    "    Base::Result<void> SetRoot(\n"
    "        Base::Ref<Base::Object> owner,\n"
    "        Visual& root) noexcept;\n"
    "    Base::Result<void> SetRoot(\n"
    "        Base::StringView name,\n"
    "        Base::Ref<Base::Object> owner,\n"
    "        Visual& root) noexcept;\n",
)
replace_once(
    "src/controls/Templates.cpp",
    "Base::Result<void> TemplateBuildContext::SetRoot(\n"
    "    Base::Ref<Base::Object> owner,\n"
    "    Visual& root) noexcept {\n",
    "Base::Result<void> TemplateBuildContext::SetRoot(\n"
    "    Base::Ref<Base::Object> owner,\n"
    "    Visual& root) noexcept {\n"
    "    return SetRoot({}, std::move(owner), root);\n"
    "}\n\n"
    "Base::Result<void> TemplateBuildContext::SetRoot(\n"
    "    Base::StringView name,\n"
    "    Base::Ref<Base::Object> owner,\n"
    "    Visual& root) noexcept {\n",
)
replace_once(
    "src/controls/Templates.cpp",
    "    Base::Result<void> added = AddOwnedPart(\n"
    "        {}, std::move(owner), root, mount);\n",
    "    Base::Result<void> added = AddOwnedPart(\n"
    "        name, std::move(owner), root, mount);\n",
)

# Value-type elements such as <Color x:Key=... Value=.../> are handled by
# the generic writer rather than an object wrapper.
replace_once(
    "include/Aero/Markup/XamlObjectWriter.hpp",
    "    enum class FrameKind : std::uint8_t {\n"
    "        Object = 0U,\n"
    "        Member,\n"
    "        Directive,\n"
    "        NullObject\n"
    "    };\n",
    "    enum class FrameKind : std::uint8_t {\n"
    "        Object = 0U,\n"
    "        Member,\n"
    "        Directive,\n"
    "        NullObject,\n"
    "        ValueObject,\n"
    "        ValueMember\n"
    "    };\n",
)
replace_once(
    "include/Aero/Markup/XamlObjectWriter.hpp",
    "        Base::Ref<Base::Object> object;\n"
    "        Core::TypeId type = Core::InvalidTypeId;\n"
    "        Base::String name;\n",
    "        Base::Ref<Base::Object> object;\n"
    "        XamlValue value;\n"
    "        Core::TypeId type = Core::InvalidTypeId;\n"
    "        Base::String name;\n",
)
replace_once(
    "include/Aero/Markup/XamlObjectWriter.hpp",
    "        bool resourceRegistered = false;\n"
    "    };\n",
    "        bool resourceRegistered = false;\n"
    "        bool valueElement = false;\n"
    "    };\n",
)
replace_once(
    "include/Aero/Markup/XamlObjectWriter.hpp",
    "    Base::Result<void> StartObject(\n"
    "        const XamlNode& node) noexcept;\n"
    "    Base::Result<void> StartNullObject(\n",
    "    Base::Result<void> StartObject(\n"
    "        const XamlNode& node) noexcept;\n"
    "    Base::Result<void> StartValueObject(\n"
    "        const XamlNode& node,\n"
    "        std::uint32_t bindingStart,\n"
    "        Core::TypeId type) noexcept;\n"
    "    Base::Result<void> StartNullObject(\n",
)
replace_once(
    "include/Aero/Markup/XamlObjectWriter.hpp",
    "    Base::Result<void> CompleteObject(\n"
    "        const XamlNode& node) noexcept;\n"
    "    Base::Result<void> CompleteNullObject(\n",
    "    Base::Result<void> CompleteObject(\n"
    "        const XamlNode& node) noexcept;\n"
    "    Base::Result<void> CompleteValueObject(\n"
    "        const XamlNode& node) noexcept;\n"
    "    Base::Result<void> CompleteNullObject(\n",
)
replace_once(
    "include/Aero/Markup/XamlObjectWriter.hpp",
    "    Base::Result<void> WriteObjectToParent(\n"
    "        std::uint32_t objectIndex,\n"
    "        Core::SourceSpan source) noexcept;\n",
    "    Base::Result<void> WriteValueToParent(\n"
    "        XamlValue&& value,\n"
    "        Core::SourceSpan source) noexcept;\n"
    "    Base::Result<void> WriteObjectToParent(\n"
    "        std::uint32_t objectIndex,\n"
    "        Core::SourceSpan source) noexcept;\n",
)

replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "    const Core::MetadataTypeDescriptor* type = typeResult.Value();\n"
    "    Base::Result<Base::Ref<Base::Object>> createResult =\n",
    "    const Core::MetadataTypeDescriptor* type = typeResult.Value();\n"
    "    if (HasTypeFlag(type->Flags(), Core::TypeFlags::ValueType)) {\n"
    "        return StartValueObject(node, bindingStart, type->Id());\n"
    "    }\n"
    "    Base::Result<Base::Ref<Base::Object>> createResult =\n",
)
replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "    return {};\n"
    "}\n\n"
    "Base::Result<void> XamlObjectWriter::StartNullObject(\n",
    "    return {};\n"
    "}\n\n"
    "Base::Result<void> XamlObjectWriter::StartValueObject(\n"
    "    const XamlNode& node,\n"
    "    std::uint32_t bindingStart,\n"
    "    Core::TypeId type) noexcept {\n"
    "    if (frames_.Empty() ||\n"
    "        (frames_.Back().kind != FrameKind::Object &&\n"
    "         frames_.Back().kind != FrameKind::Member)) {\n"
    "        return Failure(\n"
    "            Base::Status::Failure(\n"
    "                Base::ErrorCode::ValidationFailed,\n"
    "                MessageTypeMismatch.Data()),\n"
    "            XamlObjectWriterDiagnosticCodes::TypeMismatch,\n"
    "            MessageTypeMismatch,\n"
    "            node.Source());\n"
    "    }\n\n"
    "    CreatedObjectRecord record;\n"
    "    record.type = type;\n"
    "    record.valueElement = true;\n"
    "    const std::uint32_t objectIndex = created_.Size();\n"
    "    Base::Result<void> appended = created_.TryPushBack(\n"
    "        std::move(record));\n"
    "    if (!appended) return appended.GetStatus();\n\n"
    "    Frame frame;\n"
    "    frame.kind = FrameKind::ValueObject;\n"
    "    frame.objectIndex = objectIndex;\n"
    "    frame.namespaceBindingStart = bindingStart;\n"
    "    frame.source = node.Source();\n"
    "    appended = frames_.TryPushBack(frame);\n"
    "    if (!appended) return appended.GetStatus();\n"
    "    return {};\n"
    "}\n\n"
    "Base::Result<void> XamlObjectWriter::StartNullObject(\n",
)
replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "    Frame& frame = frames_.Back();\n"
    "    if (frame.kind == FrameKind::NullObject) {\n",
    "    Frame& frame = frames_.Back();\n"
    "    if (frame.kind == FrameKind::ValueObject) {\n"
    "        return CompleteValueObject(node);\n"
    "    }\n"
    "    if (frame.kind == FrameKind::NullObject) {\n",
)
replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "    if (frames_.Empty() || frames_.Back().kind != FrameKind::Object) {\n",
    "    if (frames_.Empty() ||\n"
    "        (frames_.Back().kind != FrameKind::Object &&\n"
    "         frames_.Back().kind != FrameKind::ValueObject)) {\n",
)
replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "    const Frame& objectFrame = frames_.Back();\n"
    "    if (objectFrame.objectIndex >= created_.Size()) {\n",
    "    const Frame& objectFrame = frames_.Back();\n"
    "    if (objectFrame.objectIndex >= created_.Size()) {\n",
)
# Insert the value-member branch after the shared record bounds check.
replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "    if (objectFrame.objectIndex >= created_.Size()) {\n"
    "        return Failure(\n"
    "            InvalidStateStatus(),\n"
    "            XamlObjectWriterDiagnosticCodes::InvalidWriterState,\n"
    "            MessageInvalidWriterState,\n"
    "            node.Source());\n"
    "    }\n\n"
    "    if (node.Name().NamespaceUri() == XamlLanguageNamespaceUri()) {\n",
    "    if (objectFrame.objectIndex >= created_.Size()) {\n"
    "        return Failure(\n"
    "            InvalidStateStatus(),\n"
    "            XamlObjectWriterDiagnosticCodes::InvalidWriterState,\n"
    "            MessageInvalidWriterState,\n"
    "            node.Source());\n"
    "    }\n\n"
    "    if (objectFrame.kind == FrameKind::ValueObject) {\n"
    "        if (node.Name().NamespaceUri() == XamlLanguageNamespaceUri()) {\n"
    "            if (IsXamlDirective(node.Name(), DirectiveKey)) {\n"
    "                return StartDirective(\n"
    "                    node, DirectiveKind::Key, objectFrame.objectIndex);\n"
    "            }\n"
    "            return Failure(\n"
    "                Base::Status::Failure(\n"
    "                    Base::ErrorCode::Unsupported,\n"
    "                    MessageInvalidDirective.Data()),\n"
    "                XamlObjectWriterDiagnosticCodes::InvalidDirective,\n"
    "                MessageInvalidDirective,\n"
    "                node.Source());\n"
    "        }\n"
    "        if (!node.IsFromAttribute() ||\n"
    "            node.Name().LocalName() != Base::StringView(\"Value\")) {\n"
    "            return Failure(\n"
    "                Base::Status::Failure(\n"
    "                    Base::ErrorCode::NotFound,\n"
    "                    MessageUnknownMember.Data()),\n"
    "                XamlObjectWriterDiagnosticCodes::UnknownMember,\n"
    "                MessageUnknownMember,\n"
    "                node.Source());\n"
    "        }\n"
    "        Frame frame;\n"
    "        frame.kind = FrameKind::ValueMember;\n"
    "        frame.targetObjectIndex = objectFrame.objectIndex;\n"
    "        frame.source = node.Source();\n"
    "        Base::Result<void> appended = frames_.TryPushBack(frame);\n"
    "        if (!appended) return appended.GetStatus();\n"
    "        return {};\n"
    "    }\n\n"
    "    if (node.Name().NamespaceUri() == XamlLanguageNamespaceUri()) {\n",
)
replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "    const Frame& frame = frames_.Back();\n"
    "    if (frame.kind == FrameKind::Directive) {\n",
    "    const Frame& frame = frames_.Back();\n"
    "    if (frame.kind == FrameKind::ValueMember) {\n"
    "        if (frame.valuesWritten != 1U) {\n"
    "            return Failure(\n"
    "                Base::Status::Failure(\n"
    "                    Base::ErrorCode::ValidationFailed,\n"
    "                    MessageMissingMemberValue.Data()),\n"
    "                XamlObjectWriterDiagnosticCodes::MissingMemberValue,\n"
    "                MessageMissingMemberValue,\n"
    "                node.Source());\n"
    "        }\n"
    "        frames_.PopBack();\n"
    "        return {};\n"
    "    }\n"
    "    if (frame.kind == FrameKind::Directive) {\n",
)
replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "    if (frame.kind == FrameKind::Directive) {\n"
    "        return WriteDirectiveText(frame, node);\n"
    "    }\n\n"
    "    if (frame.kind == FrameKind::Member) {\n",
    "    if (frame.kind == FrameKind::Directive) {\n"
    "        return WriteDirectiveText(frame, node);\n"
    "    }\n\n"
    "    if (frame.kind == FrameKind::ValueMember ||\n"
    "        frame.kind == FrameKind::ValueObject) {\n"
    "        const std::uint32_t objectIndex =\n"
    "            frame.kind == FrameKind::ValueMember\n"
    "                ? frame.targetObjectIndex\n"
    "                : frame.objectIndex;\n"
    "        if (objectIndex >= created_.Size() ||\n"
    "            !created_[objectIndex].valueElement ||\n"
    "            !created_[objectIndex].value.IsUnset() ||\n"
    "            frame.valuesWritten != 0U) {\n"
    "            return Failure(\n"
    "                Base::Status::Failure(\n"
    "                    Base::ErrorCode::AlreadyExists,\n"
    "                    MessageDuplicateMemberValue.Data()),\n"
    "                XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,\n"
    "                MessageDuplicateMemberValue,\n"
    "                node.Source());\n"
    "        }\n"
    "        Base::StringView extensionName;\n"
    "        Base::StringView argument;\n"
    "        const MarkupValueKind markup = ParseMarkupValue(\n"
    "            node.Value(), extensionName, argument);\n"
    "        if (markup != MarkupValueKind::Literal &&\n"
    "            markup != MarkupValueKind::EscapedLiteral) {\n"
    "            return Failure(\n"
    "                Base::Status::Failure(\n"
    "                    Base::ErrorCode::ValidationFailed,\n"
    "                    MessageInvalidMarkupExtension.Data()),\n"
    "                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,\n"
    "                MessageInvalidMarkupExtension,\n"
    "                node.Source());\n"
    "        }\n"
    "        Base::Result<XamlValue> converted = schema_->ConvertText(\n"
    "            created_[objectIndex].type,\n"
    "            markup == MarkupValueKind::EscapedLiteral\n"
    "                ? argument : node.Value());\n"
    "        if (!converted) {\n"
    "            return Failure(\n"
    "                converted.GetStatus(),\n"
    "                XamlObjectWriterDiagnosticCodes::InvalidValue,\n"
    "                MessageInvalidValue,\n"
    "                node.Source());\n"
    "        }\n"
    "        created_[objectIndex].value = std::move(converted).Value();\n"
    "        ++frame.valuesWritten;\n"
    "        return {};\n"
    "    }\n\n"
    "    if (frame.kind == FrameKind::Member) {\n",
)
replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "Base::Result<void> XamlObjectWriter::CompleteNullObject(\n",
    "Base::Result<void> XamlObjectWriter::CompleteValueObject(\n"
    "    const XamlNode& node) noexcept {\n"
    "    if (frames_.Empty() ||\n"
    "        frames_.Back().kind != FrameKind::ValueObject) {\n"
    "        return Failure(\n"
    "            InvalidStateStatus(),\n"
    "            XamlObjectWriterDiagnosticCodes::InvalidWriterState,\n"
    "            MessageInvalidWriterState,\n"
    "            node.Source());\n"
    "    }\n"
    "    const Frame frame = frames_.Back();\n"
    "    if (frame.objectIndex >= created_.Size() ||\n"
    "        created_[frame.objectIndex].value.IsUnset()) {\n"
    "        return Failure(\n"
    "            Base::Status::Failure(\n"
    "                Base::ErrorCode::ValidationFailed,\n"
    "                MessageMissingMemberValue.Data()),\n"
    "            XamlObjectWriterDiagnosticCodes::MissingMemberValue,\n"
    "            MessageMissingMemberValue,\n"
    "            node.Source());\n"
    "    }\n"
    "    frames_.PopBack();\n"
    "    PopNamespaceBindings(frame.namespaceBindingStart);\n"
    "    Base::Result<bool> resource = RegisterObjectResource(\n"
    "        frame.objectIndex, node.Source());\n"
    "    if (!resource) return resource.GetStatus();\n"
    "    if (resource.Value()) return {};\n"
    "    return WriteValueToParent(\n"
    "        std::move(created_[frame.objectIndex].value),\n"
    "        node.Source());\n"
    "}\n\n"
    "Base::Result<void> XamlObjectWriter::CompleteNullObject(\n",
)
replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "Base::Result<void> XamlObjectWriter::WriteObjectToParent(\n",
    "Base::Result<void> XamlObjectWriter::WriteValueToParent(\n"
    "    XamlValue&& value,\n"
    "    Core::SourceSpan source) noexcept {\n"
    "    if (frames_.Empty()) {\n"
    "        return Failure(\n"
    "            Base::Status::Failure(\n"
    "                Base::ErrorCode::ValidationFailed,\n"
    "                MessageTypeMismatch.Data()),\n"
    "            XamlObjectWriterDiagnosticCodes::TypeMismatch,\n"
    "            MessageTypeMismatch,\n"
    "            source);\n"
    "    }\n"
    "    Frame& parent = frames_.Back();\n"
    "    if (parent.kind == FrameKind::Member) {\n"
    "        return WriteValueToMember(parent, std::move(value), source);\n"
    "    }\n"
    "    if (parent.kind != FrameKind::Object ||\n"
    "        parent.objectIndex >= created_.Size()) {\n"
    "        return Failure(\n"
    "            InvalidStateStatus(),\n"
    "            XamlObjectWriterDiagnosticCodes::InvalidWriterState,\n"
    "            MessageInvalidWriterState,\n"
    "            source);\n"
    "    }\n"
    "    Base::Result<XamlResolvedMember> content =\n"
    "        schema_->ResolveContentMember(\n"
    "            created_[parent.objectIndex].type);\n"
    "    if (!content) {\n"
    "        return Failure(\n"
    "            content.GetStatus(),\n"
    "            XamlObjectWriterDiagnosticCodes::MissingContentProperty,\n"
    "            MessageMissingContentProperty,\n"
    "            source);\n"
    "    }\n"
    "    return WriteValue(\n"
    "        parent.objectIndex,\n"
    "        content.Value(),\n"
    "        std::move(value),\n"
    "        source);\n"
    "}\n\n"
    "Base::Result<void> XamlObjectWriter::WriteObjectToParent(\n",
)
replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "    ResourceScopeRecord& scope = resourceScopes_[scopeIndex];\n"
    "    Base::Result<void> localResult = scope.resources.TryAdd(\n"
    "        object.key.View(),\n"
    "        object.type,\n"
    "        object.object,\n"
    "        source);\n",
    "    ResourceScopeRecord& scope = resourceScopes_[scopeIndex];\n"
    "    XamlValue resourceValue = object.valueElement\n"
    "        ? object.value\n"
    "        : XamlValue::FromObject(object.type, object.object);\n"
    "    Base::Result<void> localResult = scope.resources.TryAdd(\n"
    "        object.key.View(),\n"
    "        resourceValue,\n"
    "        source);\n",
)
replace_once(
    "src/markup/XamlObjectWriter.cpp",
    "    Base::Result<void> callbackResult = schema_->AddResource(\n"
    "        owner.type,\n"
    "        *owner.object,\n"
    "        object.key.View(),\n"
    "        object.type,\n"
    "        object.object);\n",
    "    Base::Result<void> callbackResult = schema_->AddResource(\n"
    "        owner.type,\n"
    "        *owner.object,\n"
    "        object.key.View(),\n"
    "        resourceValue);\n",
)

# Add a focused generic value-element regression.
replace_once(
    "tests/markup/XamlDirectivesResourcesTests.cpp",
    "bool TestNullObjectElement() {\n",
    "bool TestValueElementResource() {\n"
    "    DirectiveNode::ResetCounters();\n"
    "    {\n"
    "        Fixture fixture;\n"
    "        CHECK(fixture.Build());\n"
    "        DiagnosticBag diagnostics;\n"
    "        Result<Ref<Object>> loaded = LoadDocument(\n"
    "            fixture,\n"
    "            StringView(\n"
    "                \"<Root xmlns=\\\"urn:directives\\\" \"\n"
    "                \"xmlns:x=\\\"http://schemas.microsoft.com/winfx/2006/xaml\\\">\"\n"
    "                \"<String x:Key=\\\"message\\\" Value=\\\"hello\\\"/>\"\n"
    "                \"<Leaf Title=\\\"{StaticResource message}\\\"/>\"\n"
    "                \"</Root>\"),\n"
    "            diagnostics);\n"
    "        CHECK(loaded);\n"
    "        CHECK(diagnostics.Size() == 0U);\n"
    "        Ref<Object> rootObject = std::move(loaded).Value();\n"
    "        auto* root = static_cast<DirectiveNode*>(rootObject.Get());\n"
    "        CHECK(root->Children().Size() == 1U);\n"
    "        CHECK(static_cast<DirectiveNode*>(\n"
    "            root->Children()[0].Get())->Title() == StringView(\"hello\"));\n"
    "        Result<XamlResourceValue> value = root->Resources().Lookup(\n"
    "            StringView(\"message\"));\n"
    "        CHECK(value);\n"
    "        CHECK(value.Value().Kind() == ValueKind::String);\n"
    "        CHECK(value.Value().AsString() == StringView(\"hello\"));\n"
    "    }\n"
    "    CHECK(DirectiveNode::LiveCount() == 0U);\n"
    "    return true;\n"
    "}\n\n"
    "bool TestNullObjectElement() {\n",
)
replace_once(
    "tests/markup/XamlDirectivesResourcesTests.cpp",
    "    if (!TestNamesResourcesStaticResourceAndServices()) return 1;\n"
    "    if (!TestNullObjectElement()) return 1;\n",
    "    if (!TestNamesResourcesStaticResourceAndServices()) return 1;\n"
    "    if (!TestValueElementResource()) return 1;\n"
    "    if (!TestNullObjectElement()) return 1;\n",
)

# Register the Markup metadata module as a normal built-in module.
replace_once(
    "src/BuiltinModules.cpp",
    "#include <Aero/Controls/Metadata.hpp>\n",
    "#include <Aero/Controls/Metadata.hpp>\n"
    "#include <Aero/Markup/Metadata.hpp>\n",
)
replace_once(
    "src/BuiltinModules.cpp",
    "    return Controls::TryRegisterBuiltInUiMetadata(domain);\n",
    "    Base::Result<void> registered =\n"
    "        Controls::TryRegisterBuiltInUiMetadata(domain);\n"
    "    if (!registered) return registered.GetStatus();\n"
    "    return Markup::TryRegisterMarkupMetadata(domain);\n",
)

# Normal translation units replace the deleted private parser/DTO source.
replace_once(
    "CMakeLists.txt",
    "    src/markup/XamlDynamicResource.cpp\n"
    "    src/markup/XamlStyle.cpp\n"
    "    src/markup/XamlTheme.cpp\n"
    "    src/markup/XamlThemeResources.cpp\n",
    "    src/markup/Metadata.cpp\n"
    "    src/markup/XamlDynamicResource.cpp\n"
    "    src/markup/XamlStyle.cpp\n"
    "    src/markup/XamlTheme.cpp\n"
    "    src/markup/XamlThemeObjectModel.cpp\n",
)

# Built-in theme XAML now uses the same namespace, properties and markup
# extensions as application XAML.
for theme in ("themes/Generic.xaml", "themes/Light.xaml", "themes/Dark.xaml"):
    replace_all(theme, 'xmlns="urn:aero/themes"', 'xmlns="urn:aero"')

generic_path = Path("themes/Generic.xaml")
generic = generic_path.read_text(encoding="utf-8")
generic = re.sub(
    r'BackgroundResource="([^"]+)"',
    r'Background="{StaticResource \1}"',
    generic,
)
generic = re.sub(
    r'BorderBrushResource="([^"]+)"',
    r'BorderBrush="{StaticResource \1}"',
    generic,
)
generic = re.sub(
    r'(<Setter\s+TargetName="[^"]+"\s+Property="[^"]+")\s+Resource="([^"]+)"',
    r'\1 Value="{StaticResource \2}"',
    generic,
)
generic = generic.replace("<VisualTree>", "<ControlTemplate.VisualTree>")
generic = generic.replace("</VisualTree>", "</ControlTemplate.VisualTree>")
generic = generic.replace(
    "<VisualStateGroups>",
    "<ControlTemplate.VisualStateGroups>",
)
generic = generic.replace(
    "</VisualStateGroups>",
    "</ControlTemplate.VisualStateGroups>",
)
if "Resource=\"" in generic or "BackgroundResource=" in generic or \
        "BorderBrushResource=" in generic:
    raise RuntimeError("Generic.xaml still contains private theme attributes")
generic_path.write_text(generic, encoding="utf-8")

# Theme callers provide the frozen metadata runtime used by ObjectWriter.
replace_once(
    "tests/markup/XamlThemeTests.cpp",
    "#include <Aero/Markup/XamlTheme.hpp>\n",
    "#include <Aero/Markup/Metadata.hpp>\n"
    "#include <Aero/Markup/XamlTheme.hpp>\n",
)
replace_once(
    "tests/markup/XamlThemeTests.cpp",
    "        CHECK(TryRegisterBuiltInUiMetadata(metadata));\n"
    "        CHECK(metadata.Seal());\n",
    "        CHECK(TryRegisterBuiltInUiMetadata(metadata));\n"
    "        CHECK(TryRegisterMarkupMetadata(metadata));\n"
    "        CHECK(metadata.Seal());\n",
)
replace_all(
    "tests/markup/XamlThemeTests.cpp",
    "fixture.metadata.DependencyProperties())",
    "*fixture.runtime)",
    minimum=3,
)
replace_once(
    "samples/ControlGallery/GalleryRuntime.cpp",
    "runtime.Metadata().DependencyProperties())",
    "*runtime.MetadataRuntime())",
)

# Architecture checks make the removal permanent.
replace_once(
    "cmake/CheckArchitecture.cmake",
    '    "src/markup/RuntimeServices.inc")\n',
    '    "src/markup/RuntimeServices.inc"\n'
    '    "src/markup/XamlThemeResources.hpp"\n'
    '    "src/markup/XamlThemeResources.cpp")\n',
)
replace_once(
    "cmake/CheckArchitecture.cmake",
    "aero_collect_matches(theme_object_model\n"
    "    \"ThemeResourceDictionaryObject\"\n"
    "    ${current_code})\n"
    "if(theme_object_model)\n"
    "    message(FATAL_ERROR\n"
    "        \"Theme bootstrap wrappers must not re-enter the public object model: ${theme_object_model}\")\n"
    "endif()\n",
    "aero_collect_matches(theme_private_pipeline\n"
    "    \"Theme(XamlDocument|ResourceDictionary|VisualNode)\"\n"
    "    ${current_code})\n"
    "if(theme_private_pipeline)\n"
    "    message(FATAL_ERROR\n"
    "        \"Built-in themes must use metadata objects and XamlObjectWriter: ${theme_private_pipeline}\")\n"
    "endif()\n",
)

# Record the final boundary rather than another planned adapter layer.
doc = Path("docs/MARKUP_RUNTIME_MODULE_REFACTOR.md")
text = doc.read_text(encoding="utf-8")
old = """13. `ResourceDictionary` stores `Core::Value`, so scalar, custom-value, null-object, and object resources share one lookup contract. Local document resources override the optional application/module dictionary supplied through `XamlLoadContext`.
14. The built-in theme parser and DTOs are private implementation details. They are compiled as a normal translation unit; public `ThemeResourceDictionaryObject` and `.cpp` inclusion shortcuts are prohibited by architecture checks.

Remaining theme work must remove the private bootstrap parser incrementally:

1. model `Style`, `Setter`, `Trigger`, `ControlTemplate`, visual states, and template content as metadata-created objects;
2. load `Generic.xaml` through `XamlObjectWriter` and the same `ResourceDictionary/Core::Value` pipeline as application XAML;
3. delete `XamlThemeResources.cpp` once the built-in catalog no longer needs materialization DTOs.

Do not add another theme DOM, resource wrapper, activation registry, or feature-local property system during that migration.
"""
new = """13. `ResourceDictionary` stores `Core::Value`, so scalar, custom-value, null-object, and object resources share one lookup contract. Local document resources override the optional application/module dictionary supplied through `XamlLoadContext`.
14. Metadata value types are valid XAML value elements. For example, `<Color x:Key=\"Accent\" Value=\"#FF0067C0\"/>` converts through the registered value converter and enters the ordinary resource dictionary without an object wrapper.
15. Built-in `ResourceDictionary`, `ControlTemplate`, `VisualStateGroup`, `VisualState`, and `Setter` objects are registered by the `Aero.Markup` metadata module. `Generic.xaml`, `Light.xaml`, and `Dark.xaml` are loaded by `XamlObjectWriter`; the private theme DOM/parser and `XamlThemeResources.cpp` are removed.
16. Template prototypes are compiled from metadata-created visual objects, ordinary dependency-property local values, content facets, and XAML name scopes. Runtime template instances are recreated through `MetadataRuntime::CreateObject`, not a control-kind switch.

The remaining theme work is feature expansion only: richer template bindings, style composition, transitions, and additional controls must extend these metadata objects and existing Presentation plans. Do not add another theme DOM, parser, resource wrapper, activation registry, or feature-local property system.
"""
if old not in text:
    raise RuntimeError("markup refactor document boundary text was not found")
doc.write_text(text.replace(old, new, 1), encoding="utf-8")

Path("src/markup/XamlThemeResources.hpp").unlink()
Path("src/markup/XamlThemeResources.cpp").unlink()
