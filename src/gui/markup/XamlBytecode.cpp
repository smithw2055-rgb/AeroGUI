#include "gui/markup/XamlBytecode.hpp"

#include "gui/controls/ItemsContainers.hpp"
#include "gui/core/DependencyObjectAccess.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/FrameworkElementSeams.hpp"
#include "gui/data/BindingCommon.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationModel.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/templates/DataTemplateTriggerInstance.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include "gui/triggers/TriggerPlan.hpp"
#include <Aero/ContentElement.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Documents.hpp>
#include <Aero/Controls/Grid.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/BindingOperations.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/FrameworkTemplate.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/VisualStateManager.hpp>

#include <cstdio>
#include <utility>

namespace Aero::Markup {

class XamlBytecodeBuilder {
public:
    XamlBytecodeBuilder() noexcept {
        stringPool_.PushBack(Base::String{});
        const Base::HashCode h = Base::DefaultHash<Base::StringView>{}(Base::StringView{});
        static_cast<void>(stringHashIndex_.Insert(h, 0U));
    }

    void SetSlotCount(std::uint32_t count) noexcept {
        slotCount_ = count;
    }

    std::uint16_t AddType(Meta::TypeId typeId) noexcept {
        for (std::uint16_t i = 0U; i < static_cast<std::uint16_t>(typePool_.Size()); ++i) {
            if (typePool_[i] == typeId) return i;
        }
        const auto idx = static_cast<std::uint16_t>(typePool_.Size());
        typePool_.PushBack(typeId);
        return idx;
    }

    std::uint16_t AddProperty(std::uint64_t propOrMember) noexcept {
        for (std::uint16_t i = 0U; i < static_cast<std::uint16_t>(propertyPool_.Size()); ++i) {
            if (propertyPool_[i] == propOrMember) return i;
        }
        const auto idx = static_cast<std::uint16_t>(propertyPool_.Size());
        propertyPool_.PushBack(propOrMember);
        return idx;
    }

    std::uint32_t AddString(Base::StringView str) noexcept {
        if (str.Empty()) return 0U;
        const Base::HashCode h = Base::DefaultHash<Base::StringView>{}(str);
        if (const std::uint32_t* found = stringHashIndex_.Find(h)) {
            if (*found < stringPool_.Size() && stringPool_[*found].View() == str) {
                return *found;
            }
            for (std::uint32_t i = 1U; i < stringPool_.Size(); ++i) {
                if (stringPool_[i].View() == str) return i;
            }
        }
        const std::uint32_t newIndex = stringPool_.Size();
        Base::String s;
        static_cast<void>(s.Assign(str));
        stringPool_.PushBack(std::move(s));
        static_cast<void>(stringHashIndex_.Insert(h, newIndex));
        return newIndex;
    }

    std::uint32_t AddValue(const Meta::Value& value) noexcept {
        switch (value.Kind()) {
        case Meta::ValueKind::Boolean:
        case Meta::ValueKind::SignedInteger:
        case Meta::ValueKind::UnsignedInteger:
        case Meta::ValueKind::Double:
        case Meta::ValueKind::String:
            for (std::uint32_t i = 0U; i < valuePool_.Size(); ++i) {
                if (valuePool_[i].Type() == value.Type() && valuePool_[i] == value) {
                    return i;
                }
            }
            break;
        default:
            break;
        }
        const std::uint32_t newIndex = valuePool_.Size();
        valuePool_.PushBack(value);
        return newIndex;
    }

    std::uint32_t AddGridLengths(Base::Span<const ::Aero::GridLength> lengths) noexcept {
        if (lengths.Empty()) return 0U;
        const std::uint32_t start = gridLengths_.Size();
        for (const ::Aero::GridLength& len : lengths) {
            gridLengths_.PushBack(len);
        }
        return start;
    }

    std::uint32_t AddGradientStops(Base::Span<const TemplatePrototypeGradientStop> stops) noexcept {
        if (stops.Empty()) return 0U;
        const std::uint32_t start = gradientStops_.Size();
        for (const TemplatePrototypeGradientStop& stop : stops) {
            gradientStops_.PushBack(stop);
        }
        return start;
    }

    std::uint32_t AddObjectRefs(Base::Span<const Base::Ref<Base::Object>> objects) noexcept {
        if (objects.Empty()) return 0U;
        const std::uint32_t start = objectRefs_.Size();
        for (const auto& obj : objects) {
            objectRefs_.PushBack(obj);
        }
        return start;
    }

    void EmitCreateObject(std::uint16_t slot, Meta::TypeId typeId) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::CreateObject;
        inst.slot = slot;
        inst.typeOrPropIdx = AddType(typeId);
        instructions_.PushBack(inst);
    }

    void EmitSetPropertyValue(
        std::uint16_t slot,
        Meta::DependencyPropertyHandle property,
        const Meta::Value& value) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::SetPropertyValue;
        inst.slot = slot;
        inst.typeOrPropIdx = AddProperty(property.value);
        inst.operandIdx = AddValue(value);
        instructions_.PushBack(inst);
    }

    void EmitSetPropertyObject(
        std::uint16_t slot,
        Meta::DependencyPropertyHandle property,
        Meta::TypeId valueType,
        std::uint16_t sourceSlot) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::SetPropertyObject;
        inst.slot = slot;
        inst.auxSlot = sourceSlot;
        inst.typeOrPropIdx = AddProperty(property.value);
        inst.extraIdx = AddType(valueType);
        instructions_.PushBack(inst);
    }

    void EmitSetPropertyContent(
        std::uint16_t parentSlot,
        Meta::MemberId memberId,
        std::uint16_t childSlot,
        Meta::TypeId valueType,
        bool isPropertyEdge) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::SetPropertyContent;
        inst.flags = isPropertyEdge ? 1U : 0U;
        inst.slot = parentSlot;
        inst.auxSlot = childSlot;
        inst.typeOrPropIdx = AddProperty(memberId);
        inst.extraIdx = AddType(valueType);
        instructions_.PushBack(inst);
    }

    void EmitSetRootVisual(std::uint16_t slot, Base::StringView name) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::SetRootVisual;
        inst.slot = slot;
        inst.operandIdx = AddString(name);
        instructions_.PushBack(inst);
    }

    void EmitAddVisualChild(
        std::uint16_t parentSlot,
        std::uint16_t childSlot,
        Base::StringView name) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::AddVisualChild;
        inst.slot = parentSlot;
        inst.auxSlot = childSlot;
        inst.operandIdx = AddString(name);
        instructions_.PushBack(inst);
    }

    void EmitAddObjectPart(std::uint16_t slot, Base::StringView name) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::AddObjectPart;
        inst.slot = slot;
        inst.operandIdx = AddString(name);
        instructions_.PushBack(inst);
    }

    void EmitSetGridDefinitions(
        std::uint16_t slot,
        Base::Span<const ::Aero::GridLength> cols,
        Base::Span<const ::Aero::GridLength> rows) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::SetGridDefinitions;
        inst.slot = slot;
        inst.auxSlot = static_cast<std::uint16_t>(cols.Size());
        inst.typeOrPropIdx = static_cast<std::uint16_t>(rows.Size());
        inst.operandIdx = AddGridLengths(cols);
        inst.extraIdx = AddGridLengths(rows);
        instructions_.PushBack(inst);
    }

    void EmitSetGradientStops(
        std::uint16_t slot,
        Base::Span<const TemplatePrototypeGradientStop> stops) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::SetGradientStops;
        inst.slot = slot;
        inst.auxSlot = static_cast<std::uint16_t>(stops.Size());
        inst.operandIdx = AddGradientStops(stops);
        instructions_.PushBack(inst);
    }

    void EmitProjectContent(std::uint16_t presenterSlot) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::ProjectContent;
        inst.slot = presenterSlot;
        instructions_.PushBack(inst);
    }

    void EmitPopulateContentPresenter(std::uint16_t slot) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::PopulateContentPresenter;
        inst.slot = slot;
        instructions_.PushBack(inst);
    }

    void EmitPopulateItemsPresenter(std::uint16_t slot) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::PopulateItemsPresenter;
        inst.slot = slot;
        instructions_.PushBack(inst);
    }

    void EmitAttachBehaviorsAndTriggers(
        std::uint16_t slot,
        Base::Span<const Base::Ref<Base::Object>> behaviors,
        Base::Span<const Base::Ref<Base::Object>> triggers) noexcept {
        const std::uint32_t itemStart = AddObjectRefs(behaviors);
        static_cast<void>(AddObjectRefs(triggers));
        XamlInstruction inst;
        inst.op = XamlOpCode::AttachBehaviorsAndTriggers;
        inst.slot = slot;
        inst.auxSlot = static_cast<std::uint16_t>(behaviors.Size());
        inst.typeOrPropIdx = static_cast<std::uint16_t>(triggers.Size());
        inst.operandIdx = itemStart;
        instructions_.PushBack(inst);
    }

    void EmitAdoptOrCreateRoot(std::uint16_t slot, Meta::TypeId typeId) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::AdoptOrCreateRoot;
        inst.slot = slot;
        inst.typeOrPropIdx = AddType(typeId);
        instructions_.PushBack(inst);
    }

    void EmitRegisterDocumentName(std::uint16_t slot, Base::StringView name) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::RegisterDocumentName;
        inst.slot = slot;
        inst.operandIdx = AddString(name);
        instructions_.PushBack(inst);
    }

    void EmitConnectEventHandler(
        std::uint16_t slot,
        Meta::MemberId eventMemberId,
        Base::StringView handlerName) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::ConnectEventHandler;
        inst.slot = slot;
        inst.typeOrPropIdx = AddProperty(eventMemberId);
        inst.operandIdx = AddString(handlerName);
        instructions_.PushBack(inst);
    }

    void PrecompileExtensionBindingPath(
        Meta::TypeId extensionTypeId,
        Base::StringView arguments) noexcept {
        if (extensionTypeId != Data::Binding::StaticTypeId()) {
            return;
        }
        Base::StringView trimmed = TrimAscii(arguments);
        if (trimmed.Empty()) return;
        Base::StringView candidatePath;
        std::uint32_t cursor = 0U;
        while (cursor < trimmed.SizeBytes()) {
            std::uint32_t nextComma = cursor;
            std::uint32_t braceDepth = 0U;
            while (nextComma < trimmed.SizeBytes()) {
                if (trimmed[nextComma] == '{') ++braceDepth;
                else if (trimmed[nextComma] == '}' && braceDepth > 0U) --braceDepth;
                else if (trimmed[nextComma] == ',' && braceDepth == 0U) break;
                ++nextComma;
            }
            Base::StringView clause = TrimAscii(trimmed.Substr(cursor, nextComma - cursor));
            std::uint32_t eqPos = UINT32_MAX;
            for (std::uint32_t i = 0U; i < clause.SizeBytes(); ++i) {
                if (clause[i] == '=') {
                    eqPos = i;
                    break;
                }
            }
            if (eqPos == UINT32_MAX) {
                if (cursor == 0U) {
                    candidatePath = clause;
                }
            } else {
                Base::StringView key = TrimAscii(clause.Substr(0U, eqPos));
                Base::StringView val = TrimAscii(clause.Substr(eqPos + 1U, clause.SizeBytes() - eqPos - 1U));
                if (key == "Path") {
                    candidatePath = val;
                    break;
                }
            }
            cursor = nextComma + 1U;
        }
        candidatePath = TrimAscii(candidatePath);
        if (candidatePath.Empty() || candidatePath == "." || candidatePath[0] == '{') {
            return;
        }
        for (const Base::String& existing : bindingPathStrings_) {
            if (existing.View() == candidatePath) {
                return;
            }
        }
        Data::CompiledBindingPath compiledPath;
        if (!compiledPath.PrecompileTokens(candidatePath)) {
            return;
        }
        Base::String storedPath;
        if (!storedPath.Assign(candidatePath)) {
            return;
        }
        Data::CompiledBindingPath::RegisterPrecompiled(candidatePath, compiledPath);
        bindingPathStrings_.PushBack(std::move(storedPath));
        bindingPaths_.PushBack(std::move(compiledPath));
    }

    void EmitApplyMarkupExtension(
        std::uint16_t slot,
        Meta::DependencyPropertyHandle property,
        Meta::TypeId extensionTypeId,
        Base::StringView arguments) noexcept {
        PrecompileExtensionBindingPath(extensionTypeId, arguments);
        XamlInstruction inst;
        inst.op = XamlOpCode::ApplyMarkupExtension;
        inst.slot = slot;
        inst.typeOrPropIdx = AddProperty(property.value);
        inst.extraIdx = AddType(extensionTypeId);
        inst.operandIdx = AddString(arguments);
        instructions_.PushBack(inst);
    }

    std::uint32_t AddSubProgram(Base::Ref<Base::Object> subProgram) noexcept {
        const std::uint32_t idx = subPrograms_.Size();
        subPrograms_.PushBack(std::move(subProgram));
        return idx;
    }

    Base::Ref<Base::Object> GetSubProgram(std::uint32_t idx) const noexcept {
        return (idx < subPrograms_.Size()) ? subPrograms_[idx] : Base::Ref<Base::Object>{};
    }

    void EmitSetSlotValue(std::uint16_t slot, const Meta::Value& value) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::SetSlotValue;
        inst.slot = slot;
        inst.operandIdx = AddValue(value);
        instructions_.PushBack(inst);
    }

    void EmitAddDictionaryEntry(
        std::uint16_t dictSlot,
        std::uint16_t valSlot,
        Base::StringView key,
        std::uint8_t keyKind = 0U) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::AddDictionaryEntry;
        inst.slot = dictSlot;
        inst.auxSlot = valSlot;
        inst.operandIdx = AddString(key);
        inst.flags = keyKind;
        instructions_.PushBack(inst);
    }

    void EmitPushAmbientResources(std::uint16_t dictSlot) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::PushAmbientResources;
        inst.slot = dictSlot;
        instructions_.PushBack(inst);
    }

    void EmitPopAmbientResources() noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::PopAmbientResources;
        instructions_.PushBack(inst);
    }

    void EmitBindTemplateProgram(std::uint16_t slot, std::uint32_t subProgramIdx) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::BindTemplateProgram;
        inst.slot = slot;
        inst.operandIdx = subProgramIdx;
        instructions_.PushBack(inst);
    }

    void EmitSealOrInitializeObject(std::uint16_t slot) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::SealOrInitializeObject;
        inst.slot = slot;
        instructions_.PushBack(inst);
    }

    void EmitApplyObjectMarkupExtension(
        std::uint16_t targetSlot,
        std::uint16_t extSlot,
        Meta::DependencyPropertyHandle property) noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::ApplyObjectMarkupExtension;
        inst.slot = targetSlot;
        inst.auxSlot = extSlot;
        inst.typeOrPropIdx = AddProperty(property.value);
        instructions_.PushBack(inst);
    }

    void EmitEnd() noexcept {
        XamlInstruction inst;
        inst.op = XamlOpCode::End;
        instructions_.PushBack(inst);
    }

    void Finish(XamlBytecodeProgram& outProgram) noexcept {
        outProgram.slotCount = slotCount_;
        outProgram.instructions = std::move(instructions_);
        outProgram.typePool = std::move(typePool_);
        outProgram.propertyPool = std::move(propertyPool_);
        outProgram.stringPool = std::move(stringPool_);
        outProgram.valuePool = std::move(valuePool_);
        outProgram.gridLengths = std::move(gridLengths_);
        outProgram.gradientStops = std::move(gradientStops_);
        outProgram.objectRefs = std::move(objectRefs_);
        outProgram.subPrograms = std::move(subPrograms_);
        outProgram.bindingPathStrings = std::move(bindingPathStrings_);
        outProgram.bindingPaths = std::move(bindingPaths_);
    }

private:
    std::uint32_t slotCount_ = 0U;
    Base::Vector<XamlInstruction> instructions_;
    Base::Vector<Meta::TypeId> typePool_;
    Base::Vector<std::uint64_t> propertyPool_;
    Base::Vector<Base::String> stringPool_;
    Base::Vector<Meta::Value> valuePool_;
    Base::Vector<::Aero::GridLength> gridLengths_;
    Base::Vector<TemplatePrototypeGradientStop> gradientStops_;
    Base::Vector<Base::Ref<Base::Object>> objectRefs_;
    Base::Vector<Base::Ref<Base::Object>> subPrograms_;
    Base::Vector<Base::String> bindingPathStrings_;
    Base::Vector<Data::CompiledBindingPath> bindingPaths_;
    Base::HashMap<Base::HashCode, std::uint32_t> stringHashIndex_;
};


const Meta::TypeInfo* ResolveBytecodeTypeByName(
    const Schema& schema,
    Base::Span<const XamlBytecodeProgram::NamespaceEntry> namespaces,
    Base::StringView rawName,
    bool allowLanguageNamespace,
    bool tryExtensionSuffix) noexcept {
    Base::StringView typeName = TrimAscii(rawName);
    if (typeName.Empty()) return nullptr;
    Base::StringView unwrappedType;
    if (TryUnwrapTypeExtensionLiteral(typeName, unwrappedType)) {
        typeName = unwrappedType;
    }
    Base::StringView prefix{};
    Base::StringView localName{};
    SplitRelaxedQualifiedXamlName(typeName, prefix, localName);
    char extWithSuffix[96];
    Base::StringView localWithSuffix{};
    if (tryExtensionSuffix) {
        const int extLen = std::snprintf(
            extWithSuffix, sizeof(extWithSuffix), "%.*sExtension",
            static_cast<int>(localName.SizeBytes()), localName.Data());
        if (extLen > 0 && extLen < static_cast<int>(sizeof(extWithSuffix))) {
            localWithSuffix = Base::StringView(extWithSuffix, static_cast<std::uint32_t>(extLen));
        }
    }
    if (!prefix.Empty()) {
        for (std::uint32_t nsIdx = namespaces.Size(); nsIdx > 0U; --nsIdx) {
            if (namespaces[nsIdx - 1U].prefix.View() == prefix) {
                const Base::StringView nsUri = namespaces[nsIdx - 1U].uri.View();
                if (!localWithSuffix.Empty()) {
                    Base::Result<const Meta::TypeInfo*> resolved =
                        schema.ResolveType(nsUri, localWithSuffix);
                    if (resolved && resolved.Value() != nullptr) {
                        return resolved.Value();
                    }
                }
                Base::Result<const Meta::TypeInfo*> resolved =
                    schema.ResolveType(nsUri, localName);
                if (resolved && resolved.Value() != nullptr) {
                    return resolved.Value();
                }
                break;
            }
        }
    }
    if (!localWithSuffix.Empty()) {
        Base::Result<const Meta::TypeInfo*> resolved =
            schema.ResolveType(Meta::AeroNamespaceUri(), localWithSuffix);
        if (resolved && resolved.Value() != nullptr) return resolved.Value();
        if (allowLanguageNamespace) {
            resolved = schema.ResolveType(LanguageNamespaceUri(), localWithSuffix);
            if (resolved && resolved.Value() != nullptr) return resolved.Value();
        }
        resolved = schema.ResolveType(WpfPresentationNamespace, localWithSuffix);
        if (resolved && resolved.Value() != nullptr) return resolved.Value();
    }
    Base::Result<const Meta::TypeInfo*> resolved =
        schema.ResolveType(Meta::AeroNamespaceUri(), localName);
    if (resolved && resolved.Value() != nullptr) return resolved.Value();
    if (allowLanguageNamespace) {
        resolved = schema.ResolveType(LanguageNamespaceUri(), localName);
        if (resolved && resolved.Value() != nullptr) return resolved.Value();
    }
    resolved = schema.ResolveType(WpfPresentationNamespace, localName);
    if (resolved && resolved.Value() != nullptr) return resolved.Value();
    resolved = schema.ResolveType(Meta::AeroNamespaceUri(), typeName);
    if (resolved && resolved.Value() != nullptr) return resolved.Value();
    return nullptr;
}

struct ParentTemplateVsmTarget {
    XamlBytecodeBuilder* builder = nullptr;
    XamlBytecodeProgram* program = nullptr;
    std::uint16_t* nextSlot = nullptr;
    std::uint16_t templateSlot = 0U;
};

Base::Result<void> CompileNodesToBytecode(
    Base::Span<const Node> nodes,
    std::size_t& nodeIndex,
    const Schema& schema,
    const Base::ResourceUri* baseUri,
    Meta::TypeId ambientTargetType,
    bool isSubProgram,
    XamlBytecodeProgram& outProgram,
    bool stopOnRootEnd = false,
    ParentTemplateVsmTarget* parentVsm = nullptr,
    XamlBytecodeBuilder* existingBuilder = nullptr,
    std::uint16_t* existingNextSlot = nullptr) noexcept {
    if (existingBuilder == nullptr) {
        outProgram.ambientTargetType = ambientTargetType;
        outProgram.schema = &schema;
        if (baseUri != nullptr) {
            outProgram.baseUri = *baseUri;
        }
    }
    XamlBytecodeBuilder localBuilder;
    XamlBytecodeBuilder& builder =
        existingBuilder != nullptr ? *existingBuilder : localBuilder;

    struct ObjectFrame {
        std::uint16_t slot = 0U;
        Meta::TypeId typeId = Meta::InvalidTypeId;
        bool isRoot = false;
        bool isValueNode = false;
        bool isTemplate = false;
        Meta::TypeId templateTargetType = Meta::InvalidTypeId;
        bool isDictionary = false;
        bool hasResourceScope = false;
        Base::String key;
        bool isTypeKey = false;
        std::uint32_t subProgramIdx = UINT32_MAX;
    };

    struct MemberFrame {
        std::uint16_t parentSlot = 0U;
        Meta::MemberId memberId = Meta::InvalidMemberId;
        Meta::TypeId valueType = Meta::InvalidTypeId;
        Meta::MemberKind kind = Meta::MemberKind::Property;
        bool isName = false;
        bool isKey = false;
        bool isEvent = false;
        bool isPropertyElement = false;
        bool isIgnored = false;
        bool isResources = false;
        bool isTriggers = false;
        bool isTargetType = false;
        std::size_t objectStackDepth = 0U;
    };

    struct NsDecl { Base::String prefix; Base::String uri; };
    Base::Vector<NsDecl> namespaces;
    Base::Vector<ObjectFrame> objectStack;
    Base::Vector<MemberFrame> memberStack;
    std::uint16_t localNextSlot = 0U;
    std::uint16_t& nextSlot =
        existingNextSlot != nullptr ? *existingNextSlot : localNextSlot;

    struct SubProgramSlotInfo {
        bool valid = false;
        bool isRoot = false;
        bool isVisual = false;
        std::uint16_t parentSlot = 0U;
        Meta::TypeId typeId = Meta::InvalidTypeId;
        Base::String partName;
        Base::String partContentSource;
    };
    Base::Vector<SubProgramSlotInfo> subProgramSlots;

    auto resolveMemberFrame = [&](
        const Node& currentNode,
        MemberFrame& frame,
        MemberSyntax syntax) noexcept {
        if (currentNode.HasCompiledMemberBinding()) {
            const auto& mb = currentNode.CompiledMember();
            frame.memberId = mb.id;
            frame.valueType = mb.valueType;
            frame.kind = mb.kind;
            frame.isEvent = (mb.kind == Meta::MemberKind::Event);
            return;
        }
        frame.memberId = currentNode.CompiledMemberId();
        if (frame.memberId == Meta::InvalidMemberId && !objectStack.Empty()) {
            Base::Result<ResolvedMember> resolved = schema.ResolveMember(
                objectStack.Back().typeId, currentNode.Name(), syntax);
            if (resolved) {
                frame.memberId = resolved.Value().id;
                frame.valueType = resolved.Value().valueType;
                frame.kind = resolved.Value().kind;
                frame.isEvent = (resolved.Value().kind == Meta::MemberKind::Event);
            }
        }
    };

    for (; nodeIndex < nodes.Size(); ++nodeIndex) {
        const Node& node = nodes[static_cast<uint32_t>(nodeIndex)];
        switch (node.Kind()) {
        case NodeKind::NamespaceDeclaration: {
            NsDecl decl;
            static_cast<void>(decl.prefix.Assign(node.NamespacePrefix()));
            static_cast<void>(decl.uri.Assign(node.NamespaceUri()));
            XamlBytecodeProgram::NamespaceEntry progEntry;
            static_cast<void>(progEntry.prefix.Assign(node.NamespacePrefix()));
            static_cast<void>(progEntry.uri.Assign(node.NamespaceUri()));
            outProgram.namespaces.PushBack(std::move(progEntry));
            namespaces.PushBack(std::move(decl));
            break;
        }

        case NodeKind::StartObject: {
            Base::StringView localName = node.Name().LocalName();
            const void* dotPtr = localName.Data() != nullptr ? std::memchr(localName.Data(), '.', localName.SizeBytes()) : nullptr;
            if (dotPtr != nullptr && !objectStack.Empty()) {
                const std::uint16_t parentSlot = objectStack.Back().slot;
                MemberFrame mf;
                mf.parentSlot = parentSlot;
                mf.isPropertyElement = true;
                mf.objectStackDepth = objectStack.Size();
                if (localName.SizeBytes() >= 10U && localName.Substr(localName.SizeBytes() - 10U, 10U) == ".Resources") {
                    mf.isResources = true;
                    if (!objectStack.Empty() && !objectStack.Back().hasResourceScope) {
                        objectStack.Back().hasResourceScope = true;
                        builder.EmitPushAmbientResources(parentSlot);
                    }
                }
                if ((localName.SizeBytes() >= 9U && localName.Substr(localName.SizeBytes() - 9U, 9U) == ".Triggers") ||
                    (localName.SizeBytes() >= 18U && localName.Substr(localName.SizeBytes() - 18U, 18U) == ".VisualStateGroups")) {
                    mf.isTriggers = true;
                }
                resolveMemberFrame(node, mf, MemberSyntax::PropertyElement);
                memberStack.PushBack(mf);
                break;
            }

            if (isSubProgram && !objectStack.Empty() && !memberStack.Empty() &&
                memberStack.Back().parentSlot == objectStack.Back().slot &&
                memberStack.Back().memberId == VisualStateManager::VisualStateGroupsProperty.Handle().value &&
                parentVsm != nullptr && parentVsm->builder != nullptr &&
                parentVsm->program != nullptr && parentVsm->nextSlot != nullptr) {
                parentVsm->program->CopyNamespacesFrom(outProgram);
                const std::uint16_t vsmRootSlot = *parentVsm->nextSlot;
                Base::Result<void> vsmRes = CompileNodesToBytecode(
                    nodes,
                    nodeIndex,
                    schema,
                    baseUri,
                    ambientTargetType,
                    false,
                    *parentVsm->program,
                    true,
                    nullptr,
                    parentVsm->builder,
                    parentVsm->nextSlot);
                if (!vsmRes) return vsmRes.GetStatus();
                const Meta::TypeId vsmRootTypeId =
                    parentVsm->program->GetSlotType(vsmRootSlot);
                parentVsm->builder->EmitSetPropertyContent(
                    parentVsm->templateSlot,
                    VisualStateManager::VisualStateGroupsProperty.Handle().value,
                    vsmRootSlot,
                    vsmRootTypeId != Meta::InvalidTypeId
                        ? vsmRootTypeId
                        : VisualStateGroup::StaticTypeId(),
                    /*isPropertyEdge=*/true);
                break;
            }

            if (!objectStack.Empty() && objectStack.Back().isTemplate &&
                (memberStack.Empty() || memberStack.Back().parentSlot != objectStack.Back().slot ||
                 (!memberStack.Back().isTriggers && !memberStack.Back().isResources))) {
                auto subContainer = Base::MakeRef<XamlBytecodeContainer>();
                subContainer->program.CopyNamespacesFrom(outProgram);
                ParentTemplateVsmTarget vsmTarget{
                    &builder,
                    &outProgram,
                    &nextSlot,
                    objectStack.Back().slot};
                Base::Result<void> subRes = CompileNodesToBytecode(
                    nodes,
                    nodeIndex,
                    schema,
                    baseUri,
                    objectStack.Back().templateTargetType,
                    true,
                    subContainer->program,
                    false,
                    objectStack.Back().typeId == Controls::ControlTemplate::StaticTypeId()
                        ? &vsmTarget
                        : nullptr);
                if (!subRes) return subRes.GetStatus();

                const std::uint32_t subProgIdx = builder.AddSubProgram(subContainer);
                objectStack.Back().subProgramIdx = subProgIdx;
                break;
            }

            Meta::TypeId typeId = node.CompiledTypeId();
            if (typeId == Meta::InvalidTypeId && node.HasCompiledTypeBinding()) {
                typeId = node.CompiledType().id;
            }
            if (typeId == Meta::InvalidTypeId) {
                Base::Result<const Meta::TypeInfo*> resolved = schema.ResolveType(
                    node.Name().NamespaceUri(), node.Name().LocalName());
                if (resolved && resolved.Value() != nullptr) {
                    typeId = resolved.Value()->Id();
                }
            }
            if (typeId == Meta::InvalidTypeId) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Unknown type in bytecode compiler");
            }

            const bool isRoot = objectStack.Empty();
            if (isRoot && !isSubProgram && !stopOnRootEnd) {
                for (std::size_t scanIdx = nodeIndex + 1U; scanIdx < nodes.Size(); ++scanIdx) {
                    const auto& scanNode = nodes[static_cast<std::uint32_t>(scanIdx)];
                    if (scanNode.Kind() == NodeKind::EndObject || scanNode.Kind() == NodeKind::StartObject) {
                        break;
                    }
                    if (scanNode.Kind() == NodeKind::StartMember && scanNode.Name().LocalName() == "Class") {
                        if (scanIdx + 1U < nodes.Size() && nodes[static_cast<std::uint32_t>(scanIdx + 1U)].Kind() == NodeKind::Value) {
                            Base::StringView className = TrimAscii(nodes[static_cast<std::uint32_t>(scanIdx + 1U)].Value());
                            std::uint32_t separator = static_cast<std::uint32_t>(className.SizeBytes());
                            for (std::uint32_t i = 0U; i < static_cast<std::uint32_t>(className.SizeBytes()); ++i) {
                                if (className[i] == '.') separator = i;
                            }
                            if (separator > 0U && separator + 1U < static_cast<std::uint32_t>(className.SizeBytes())) {
                                Base::String xamlNs;
                                Base::Result<void> nsRes = xamlNs.Assign("clr-namespace:");
                                if (nsRes) {
                                    nsRes = xamlNs.Append(className.Substr(0U, separator));
                                }
                                if (nsRes) {
                                    Base::StringView clsLocalName = className.Substr(separator + 1U, static_cast<std::uint32_t>(className.SizeBytes()) - separator - 1U);
                                    Base::Result<const Meta::TypeInfo*> classType = schema.ResolveType(xamlNs.View(), clsLocalName);
                                    if (classType && classType.Value() != nullptr &&
                                        schema.Types().IsDerivedFrom(classType.Value()->Id(), typeId)) {
                                        typeId = classType.Value()->Id();
                                    }
                                }
                            }
                        }
                        break;
                    }
                }
            }

            const Meta::TypeInfo* typeInfo = schema.Types().FindType(typeId);
            const bool isValueType = (typeInfo != nullptr &&
                (typeInfo->Kind() != Meta::MetadataTypeKind::Object ||
                 Meta::HasTypeFlag(typeInfo->Flags(), Meta::TypeFlags::Abstract)));

            const std::uint16_t slot = nextSlot++;
            const std::uint16_t parentSlot = !objectStack.Empty() ? objectStack.Back().slot : 0U;

            if (isValueType) {
                ObjectFrame frame;
                frame.slot = slot;
                frame.typeId = typeId;
                frame.isRoot = isRoot;
                frame.isValueNode = true;
                objectStack.PushBack(frame);
                break;
            }

            const bool isControlTemplate = (typeId == Controls::ControlTemplate::StaticTypeId());
            const bool isDataTemplate = (typeId == Aero::DataTemplate::StaticTypeId() ||
                                         typeId == Aero::HierarchicalDataTemplate::StaticTypeId());
            const bool isItemsPanelTemplate = (typeId == Controls::ItemsPanelTemplate::StaticTypeId());
            const bool isTemplate = isControlTemplate || isDataTemplate || isItemsPanelTemplate;
            const bool isDictionary = (typeId == Aero::ResourceDictionary::StaticTypeId());

            Base::StringView partName;
            Base::StringView partContentSource;
            if (isSubProgram) {
                for (std::size_t scanIdx = nodeIndex + 1U; scanIdx < nodes.Size(); ++scanIdx) {
                    const auto& scanNode = nodes[static_cast<std::uint32_t>(scanIdx)];
                    if (scanNode.Kind() == NodeKind::EndObject || scanNode.Kind() == NodeKind::StartObject) {
                        break;
                    }
                    if (scanNode.Kind() == NodeKind::StartMember) {
                        if (scanNode.Name().LocalName() == "Name") {
                            if (scanIdx + 1U < nodes.Size() && nodes[static_cast<std::uint32_t>(scanIdx + 1U)].Kind() == NodeKind::Value) {
                                partName = TrimAscii(nodes[static_cast<std::uint32_t>(scanIdx + 1U)].Value());
                            }
                        } else if (scanNode.Name().LocalName() == "ContentSource") {
                            if (scanIdx + 1U < nodes.Size() && nodes[static_cast<std::uint32_t>(scanIdx + 1U)].Kind() == NodeKind::Value) {
                                partContentSource = TrimAscii(nodes[static_cast<std::uint32_t>(scanIdx + 1U)].Value());
                            }
                        }
                    }
                }
            }

            if (isRoot && !isSubProgram && !stopOnRootEnd) {
                builder.EmitAdoptOrCreateRoot(slot, typeId);
            } else {
                builder.EmitCreateObject(slot, typeId);
            }

            if (isSubProgram) {
                if (slot >= subProgramSlots.Size()) {
                    subProgramSlots.Resize(slot + 1U);
                }
                SubProgramSlotInfo& slotInfo = subProgramSlots[slot];
                slotInfo.valid = true;
                slotInfo.isRoot = isRoot;
                slotInfo.isVisual = schema.Types().IsDerivedFrom(typeId, Media::Visual::StaticTypeId());
                slotInfo.parentSlot = parentSlot;
                slotInfo.typeId = typeId;
                if (!partName.Empty()) {
                    static_cast<void>(slotInfo.partName.Assign(partName));
                }
                if (!partContentSource.Empty()) {
                    static_cast<void>(slotInfo.partContentSource.Assign(partContentSource));
                }
            }

            if (isDictionary) {
                builder.EmitPushAmbientResources(slot);
            }

            ObjectFrame frame;
            frame.slot = slot;
            frame.typeId = typeId;
            frame.isRoot = isRoot;
            frame.isTemplate = isTemplate;
            Meta::TypeId inheritedTargetType = ambientTargetType;
            if (inheritedTargetType == Meta::InvalidTypeId && !objectStack.Empty()) {
                for (std::uint32_t i = objectStack.Size(); i > 0U; --i) {
                    if (objectStack[i - 1U].templateTargetType != Meta::InvalidTypeId) {
                        inheritedTargetType = objectStack[i - 1U].templateTargetType;
                        break;
                    }
                }
            }
            frame.templateTargetType = (inheritedTargetType != Meta::InvalidTypeId)
                ? inheritedTargetType
                : (isControlTemplate ? Controls::Control::StaticTypeId() : Meta::InvalidTypeId);
            frame.isDictionary = isDictionary;
            frame.hasResourceScope = isDictionary;

            if (slot >= outProgram.slotTypes.Size()) {
                outProgram.slotTypes.Resize(slot + 1U);
            }
            outProgram.slotTypes[slot] = typeId;
            if (slot >= outProgram.slotNames.Size()) {
                outProgram.slotNames.Resize(slot + 1U);
            }
            if (!partName.Empty()) {
                static_cast<void>(outProgram.slotNames[slot].Assign(partName));
            }

            objectStack.PushBack(std::move(frame));
            break;
        }

        case NodeKind::StartMember: {
            if (objectStack.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "StartMember without active object");
            }
            Base::StringView memName = node.Name().LocalName();
            if (memName == "Class" && (objectStack.Back().slot == 0U || objectStack.Back().isRoot)) {
                MemberFrame mf;
                mf.parentSlot = objectStack.Back().slot;
                mf.isIgnored = true;
                memberStack.PushBack(mf);
                break;
            }
            const std::uint16_t parentSlot = objectStack.Back().slot;
            MemberFrame mf;
            mf.parentSlot = parentSlot;
            mf.isPropertyElement = !node.IsFromAttribute();
            mf.objectStackDepth = objectStack.Size();
            const MemberSyntax memberSyntax = mf.isPropertyElement
                ? MemberSyntax::PropertyElement
                : MemberSyntax::Attribute;

            if (memName == "Key" &&
                node.Name().NamespaceUri() == "http://schemas.microsoft.com/winfx/2006/xaml") {
                mf.isKey = true;
            } else if (memName == "Name" &&
                (node.Name().NamespaceUri().Empty() ||
                 node.Name().NamespaceUri() == "http://schemas.microsoft.com/winfx/2006/xaml" ||
                 node.Name().NamespaceUri() == "http://schemas.microsoft.com/winfx/2006/xaml/presentation")) {
                mf.isName = true;
            } else {
                if (memName == "Resources" ||
                    (memName.SizeBytes() >= 10U && memName.Substr(memName.SizeBytes() - 10U, 10U) == ".Resources")) {
                    mf.isResources = true;
                    if (!objectStack.Empty() && !objectStack.Back().hasResourceScope) {
                        objectStack.Back().hasResourceScope = true;
                        builder.EmitPushAmbientResources(parentSlot);
                    }
                } else if (memName == "Triggers" ||
                           (memName.SizeBytes() >= 9U && memName.Substr(memName.SizeBytes() - 9U, 9U) == ".Triggers") ||
                           memName == "VisualStateGroups" ||
                           (memName.SizeBytes() >= 18U && memName.Substr(memName.SizeBytes() - 18U, 18U) == ".VisualStateGroups")) {
                    mf.isTriggers = true;
                } else if (memName == "TargetType") {
                    mf.isTargetType = true;
                }
                resolveMemberFrame(node, mf, memberSyntax);
                if (mf.isResources || mf.isTriggers || mf.isTargetType) {
                    mf.isEvent = false;
                }
            }
            memberStack.PushBack(mf);
            break;
        }

        case NodeKind::Value: {
            if (objectStack.Empty()) break;
            const bool hasMemberForCurrent = !memberStack.Empty() && memberStack.Back().parentSlot == objectStack.Back().slot;
            if (hasMemberForCurrent && memberStack.Back().isIgnored) break;

            auto makeConvServices = [&](Meta::TypeId targetObjType, Meta::MemberId targetMem) noexcept -> ExtensionServices {
                ExtensionServices convServices;
                convServices.schema = &schema;
                convServices.baseUri = baseUri;
                convServices.targetObjectType = targetObjType;
                convServices.targetMember = targetMem;
                convServices.namespaces = outProgram.MakeNamespaceScope();
                return convServices;
            };

            if (!hasMemberForCurrent) {
                if (!node.IsFromAttribute()) {
                    const ObjectFrame& currentObj = objectStack.Back();
                    if (node.HasCompiledValue()) {
                        if (currentObj.isValueNode) {
                            builder.EmitSetSlotValue(currentObj.slot, node.CompiledValue());
                            break;
                        }
                        if (Base::Result<ResolvedMember> contentMember = schema.ResolveContentMember(currentObj.typeId)) {
                            builder.EmitSetPropertyValue(
                                currentObj.slot,
                                Meta::DependencyPropertyHandle{contentMember.Value().id},
                                node.CompiledValue());
                            break;
                        }
                        if (const Meta::PropertyInfo* textProp = schema.Types().FindProperty(currentObj.typeId, Base::StringView("Text"), true)) {
                            builder.EmitSetPropertyValue(
                                currentObj.slot,
                                Meta::DependencyPropertyHandle{textProp->Id()},
                                node.CompiledValue());
                            break;
                        }
                        if (const Meta::PropertyInfo* contentProp = schema.Types().FindProperty(currentObj.typeId, Base::StringView("Content"), true)) {
                            builder.EmitSetPropertyValue(
                                currentObj.slot,
                                Meta::DependencyPropertyHandle{contentProp->Id()},
                                node.CompiledValue());
                            break;
                        }
                        builder.EmitSetSlotValue(currentObj.slot, node.CompiledValue());
                        break;
                    }
                    const bool isInlineCollectionHost =
                        currentObj.typeId == Controls::TextBlock::StaticTypeId() ||
                        schema.Types().IsDerivedFrom(currentObj.typeId, Documents::Span::StaticTypeId());
                    if (TrimAscii(node.Value()).Empty()) {
                        if (isInlineCollectionHost && !node.Value().Empty() &&
                            nodeIndex > 0U &&
                            nodes[static_cast<std::uint32_t>(nodeIndex - 1U)].Kind() == NodeKind::EndObject &&
                            nodeIndex + 1U < nodes.Size() &&
                            nodes[static_cast<std::uint32_t>(nodeIndex + 1U)].Kind() == NodeKind::StartObject) {
                            const Base::StringView nextLocal = nodes[static_cast<std::uint32_t>(nodeIndex + 1U)].Name().LocalName();
                            const bool nextIsPropertyElement =
                                nextLocal.Data() != nullptr &&
                                std::memchr(nextLocal.Data(), '.', nextLocal.SizeBytes()) != nullptr;
                            if (!nextIsPropertyElement) {
                                Base::Result<ResolvedMember> contentMember = schema.ResolveContentMember(currentObj.typeId);
                                if (contentMember) {
                                    Base::Result<Meta::Value> spaceVal =
                                        Meta::Value::TryFromString(Meta::TypeOf<Base::String>(), Base::StringView(" "));
                                    if (spaceVal) {
                                        builder.EmitSetPropertyValue(
                                            currentObj.slot,
                                            Meta::DependencyPropertyHandle{contentMember.Value().id},
                                            spaceVal.Value());
                                    }
                                }
                            }
                        }
                        break;
                    }
                    if (currentObj.isValueNode) {
                        ExtensionServices convServices = makeConvServices(currentObj.typeId, Meta::InvalidMemberId);
                        Base::Result<Meta::Value> converted = schema.ConvertText(
                            currentObj.typeId, node.Value(), &convServices);
                        if (converted) {
                            builder.EmitSetSlotValue(currentObj.slot, converted.Value());
                            break;
                        }
                    }
                    auto tryEmitConvertedProperty = [&](Meta::MemberId memberId, Meta::TypeId valueType) noexcept -> bool {
                        const Meta::TypeId resolvedType = (valueType != Meta::InvalidTypeId)
                            ? valueType
                            : Meta::TypeOf<Base::String>();
                        ExtensionServices convServices = makeConvServices(currentObj.typeId, memberId);
                        Base::Result<Meta::Value> converted = schema.ConvertText(resolvedType, node.Value(), &convServices);
                        if (!converted) return false;
                        builder.EmitSetPropertyValue(
                            currentObj.slot,
                            Meta::DependencyPropertyHandle{memberId},
                            converted.Value());
                        return true;
                    };
                    if (Base::Result<ResolvedMember> contentMember = schema.ResolveContentMember(currentObj.typeId)) {
                        if (tryEmitConvertedProperty(contentMember.Value().id, contentMember.Value().valueType)) {
                            break;
                        }
                    }
                    if (const Meta::PropertyInfo* textProp = schema.Types().FindProperty(currentObj.typeId, Base::StringView("Text"), true)) {
                        if (tryEmitConvertedProperty(textProp->Id(), textProp->ValueType())) {
                            break;
                        }
                    }
                    if (const Meta::PropertyInfo* contentProp = schema.Types().FindProperty(currentObj.typeId, Base::StringView("Content"), true)) {
                        if (tryEmitConvertedProperty(contentProp->Id(), contentProp->ValueType())) {
                            break;
                        }
                    }
                    {
                        ExtensionServices convServices = makeConvServices(currentObj.typeId, Meta::InvalidMemberId);
                        Base::Result<Meta::Value> converted = schema.ConvertText(
                            currentObj.typeId, node.Value(), &convServices);
                        if (converted) {
                            builder.EmitSetSlotValue(currentObj.slot, converted.Value());
                            break;
                        }
                    }
                }
                break;
            }

            MemberFrame& mf = memberStack.Back();
            if (mf.isKey) {
                Base::StringView unwrappedKeyType;
                if (TryUnwrapTypeExtensionLiteral(node.Value(), unwrappedKeyType)) {
                    objectStack.Back().isTypeKey = true;
                    static_cast<void>(objectStack.Back().key.Assign(unwrappedKeyType));
                } else {
                    static_cast<void>(objectStack.Back().key.Assign(node.Value()));
                }
                break;
            }
            if (mf.isName) {
                builder.EmitRegisterDocumentName(mf.parentSlot, node.Value());
                if (mf.parentSlot >= outProgram.slotNames.Size()) {
                    outProgram.slotNames.Resize(mf.parentSlot + 1U);
                }
                static_cast<void>(outProgram.slotNames[mf.parentSlot].Assign(node.Value()));
                if (isSubProgram && mf.parentSlot < subProgramSlots.Size() && subProgramSlots[mf.parentSlot].valid) {
                    static_cast<void>(subProgramSlots[mf.parentSlot].partName.Assign(TrimAscii(node.Value())));
                }
                break;
            }
            if (mf.isEvent) {
                builder.EmitConnectEventHandler(mf.parentSlot, mf.memberId, node.Value());
                break;
            }

            if (node.HasCompiledValue()) {
                builder.EmitSetPropertyValue(
                    mf.parentSlot,
                    Meta::DependencyPropertyHandle{mf.memberId},
                    node.CompiledValue());
                if ((mf.isTargetType || mf.valueType == Meta::TypeOf<Meta::TypeReference>()) &&
                    node.CompiledValue().Type() == Meta::TypeOf<Meta::TypeReference>()) {
                    if (Base::Result<Meta::TypeReference> decodedRef =
                            Meta::ValueCodec<Meta::TypeReference>::Decode(node.CompiledValue())) {
                        if (objectStack.Back().isTemplate ||
                            objectStack.Back().typeId == Style::StaticTypeId()) {
                            objectStack.Back().templateTargetType = decodedRef.Value().type;
                        }
                    }
                }
                break;
            }

            Base::StringView strVal = node.Value();
            if (strVal.SizeBytes() >= 2U && strVal[0] == '{' && strVal[1] == '}') {
                strVal = strVal.Substr(2U, strVal.SizeBytes() - 2U);
            }
            if (TrimAscii(strVal).Empty()) break;

            const Meta::TypeId targetType = (mf.valueType != Meta::InvalidTypeId)
                ? mf.valueType
                : Meta::TypeOf<Base::String>();

            if (mf.isTargetType || targetType == Meta::TypeOf<Meta::TypeReference>()) {
                if (const Meta::TypeInfo* resolved = ResolveBytecodeTypeByName(
                        schema, outProgram.namespaces.AsSpan(), strVal, false)) {
                    Meta::TypeReference ref{resolved->Id()};
                    builder.EmitSetPropertyValue(
                        mf.parentSlot,
                        Meta::DependencyPropertyHandle{mf.memberId},
                        Meta::ValueCodec<Meta::TypeReference>::Encode(ref).Value());
                    if (objectStack.Back().isTemplate ||
                        objectStack.Back().typeId == Style::StaticTypeId()) {
                        objectStack.Back().templateTargetType = resolved->Id();
                    }
                    break;
                }
            }

            if (strVal.SizeBytes() > 0U && strVal[0] == '{') {
                Base::StringView trimmed = TrimAscii(strVal);
                if (trimmed == "{x:Null}" || trimmed == "{Null}") {
                    builder.EmitSetPropertyValue(
                        mf.parentSlot,
                        Meta::DependencyPropertyHandle{mf.memberId},
                        Meta::Value::NullObject(targetType));
                    break;
                }
                if (trimmed.SizeBytes() >= 2U && trimmed[trimmed.SizeBytes() - 1U] == '}') {
                    Base::StringView inner = TrimAscii(trimmed.Substr(1U, trimmed.SizeBytes() - 2U));
                    std::uint32_t nameEnd = 0U;
                    while (nameEnd < inner.SizeBytes() &&
                           inner[nameEnd] != ' ' && inner[nameEnd] != '\t' &&
                           inner[nameEnd] != '\r' && inner[nameEnd] != '\n' &&
                           inner[nameEnd] != ',') {
                        ++nameEnd;
                    }
                    if (nameEnd > 0U) {
                        Base::StringView extName = inner.Substr(0U, nameEnd);
                        Base::StringView args = TrimAscii(inner.Substr(nameEnd, inner.SizeBytes() - nameEnd));
                        if (!args.Empty() && args[0] == ',') {
                            args = TrimAscii(args.Substr(1U, args.SizeBytes() - 1U));
                        }

                        Meta::TypeId extTypeId = Meta::InvalidTypeId;
                        if (const Meta::TypeInfo* resolvedExt = ResolveBytecodeTypeByName(
                                schema, outProgram.namespaces.AsSpan(), extName, true, true)) {
                            extTypeId = resolvedExt->Id();
                        }

                        if (extTypeId != Meta::InvalidTypeId) {
                            builder.EmitApplyMarkupExtension(
                                mf.parentSlot,
                                Meta::DependencyPropertyHandle{mf.memberId},
                                extTypeId,
                                args);
                            break;
                        }
                    }
                }
            }

            ExtensionServices convServices = makeConvServices(objectStack.Back().typeId, mf.memberId);
            Base::Result<Meta::Value> converted = schema.ConvertText(
                targetType, strVal, &convServices);
            if (converted) {
                builder.EmitSetPropertyValue(
                    mf.parentSlot,
                    Meta::DependencyPropertyHandle{mf.memberId},
                    converted.Value());
            } else if (!Meta::HasObjectFactory()) {
                if (Base::Result<Meta::Value> deferredStr =
                        Meta::Value::TryFromString(targetType, strVal)) {
                    builder.EmitSetPropertyValue(
                        mf.parentSlot,
                        Meta::DependencyPropertyHandle{mf.memberId},
                        deferredStr.Value());
                }
            }
            break;
        }

        case NodeKind::EndMember: {
            if (!memberStack.Empty()) {
                memberStack.PopBack();
            }
            break;
        }

        case NodeKind::EndObject: {
            if (!memberStack.Empty() && memberStack.Back().isPropertyElement &&
                objectStack.Size() == memberStack.Back().objectStackDepth) {
                memberStack.PopBack();
                break;
            }
            if (objectStack.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Unbalanced EndObject");
            }
            ObjectFrame finished = std::move(objectStack.Back());
            objectStack.PopBack();

            if (finished.hasResourceScope) {
                builder.EmitPopAmbientResources();
            }

            if (finished.isTemplate && finished.subProgramIdx != UINT32_MAX) {
                builder.EmitBindTemplateProgram(finished.slot, finished.subProgramIdx);
            }

            if (finished.typeId == Aero::Style::StaticTypeId() || finished.isTemplate) {
                builder.EmitSealOrInitializeObject(finished.slot);
            }

            if (!objectStack.Empty()) {
                ObjectFrame& parent = objectStack.Back();
                if (parent.isDictionary ||
                    (!memberStack.Empty() && memberStack.Back().parentSlot == parent.slot && memberStack.Back().isResources && finished.typeId != Aero::ResourceDictionary::StaticTypeId())) {
                    builder.EmitAddDictionaryEntry(
                        parent.slot, finished.slot, finished.key.View(), finished.isTypeKey ? 1U : 0U);
                } else if (!memberStack.Empty() && memberStack.Back().parentSlot == parent.slot) {
                    MemberFrame& mem = memberStack.Back();
                    const bool isMarkupExt = (finished.typeId == Data::MultiBinding::StaticTypeId() ||
                                              finished.typeId == Data::Binding::StaticTypeId() ||
                                              schema.Types().IsDerivedFrom(finished.typeId, MarkupExtension::StaticTypeId()));
                    if (isMarkupExt && mem.memberId != Meta::InvalidMemberId) {
                        builder.EmitApplyObjectMarkupExtension(
                            parent.slot,
                            finished.slot,
                            Meta::DependencyPropertyHandle{mem.memberId});
                    } else if (mem.isPropertyElement) {
                        const Meta::PropertyInfo* prop = schema.Types().FindProperty(mem.memberId);
                        const bool isPropertyEdge = prop != nullptr &&
                            (static_cast<std::uint32_t>(prop->Flags()) &
                             static_cast<std::uint32_t>(Meta::PropertyFlags::Collection)) == 0U &&
                            (schema.Metadata() != nullptr && schema.Metadata()->CanWriteProperty(mem.memberId));
                        const Meta::TypeId valueType = (prop != nullptr && prop->ValueType() != Meta::InvalidTypeId)
                            ? prop->ValueType()
                            : finished.typeId;
                        builder.EmitSetPropertyContent(
                            parent.slot,
                            mem.memberId,
                            finished.slot,
                            valueType,
                            isPropertyEdge);
                    } else {
                        builder.EmitSetPropertyObject(
                            parent.slot,
                            Meta::DependencyPropertyHandle{mem.memberId},
                            finished.typeId,
                            finished.slot);
                    }
                } else {
                    builder.EmitSetPropertyContent(
                        parent.slot,
                        Meta::InvalidMemberId,
                        finished.slot,
                        finished.typeId,
                        false);
                }
            }

            if ((isSubProgram || stopOnRootEnd) && objectStack.Empty()) {
                if (isSubProgram) {
                    std::uint16_t contentPresenterSlot = 0xFFFFU;
                    bool hasScrollContentPresenterSlot = false;
                    for (std::uint16_t s = 0U; s < subProgramSlots.Size(); ++s) {
                        SubProgramSlotInfo& info = subProgramSlots[s];
                        if (!info.valid) continue;
                        if (info.isRoot) {
                            builder.EmitSetRootVisual(s, info.partName.View());
                        } else if (info.isVisual) {
                            builder.EmitAddVisualChild(info.parentSlot, s, info.partName.View());
                        } else if (!info.partName.Empty()) {
                            builder.EmitAddObjectPart(s, info.partName.View());
                        }

                        const bool isScrollPresenter = (info.typeId == Controls::ScrollContentPresenter::StaticTypeId());
                        const bool isContentPresenter = isScrollPresenter ||
                            (info.typeId == Controls::ContentPresenter::StaticTypeId());
                        if (isContentPresenter) {
                            const bool isScrollPart = isScrollPresenter ||
                                (info.partName.View() == Base::StringView("PART_ScrollContentPresenter"));
                            const bool isHeaderSource = (info.partContentSource.View() == Base::StringView("Header"));
                            if (isScrollPart) {
                                if (!hasScrollContentPresenterSlot) {
                                    contentPresenterSlot = s;
                                    hasScrollContentPresenterSlot = true;
                                }
                            } else if (!isHeaderSource && contentPresenterSlot == 0xFFFFU) {
                                contentPresenterSlot = s;
                            }
                        }
                    }

                    if (contentPresenterSlot != 0xFFFFU) {
                        builder.EmitProjectContent(contentPresenterSlot);
                    }
                    for (std::uint16_t s = 0U; s < subProgramSlots.Size(); ++s) {
                        const SubProgramSlotInfo& info = subProgramSlots[s];
                        if (!info.valid) continue;
                        if (info.typeId == Controls::ContentPresenter::StaticTypeId() ||
                            info.typeId == Controls::ScrollContentPresenter::StaticTypeId()) {
                            builder.EmitPopulateContentPresenter(s);
                        } else if (info.typeId == Controls::ItemsPresenter::StaticTypeId()) {
                            builder.EmitPopulateItemsPresenter(s);
                        }
                    }
                }

                if (existingBuilder == nullptr) {
                    builder.SetSlotCount(nextSlot);
                    builder.EmitEnd();
                    builder.Finish(outProgram);
                }
                return {};
            }
            break;
        }

        case NodeKind::EndOfDocument:
            if (existingBuilder == nullptr) {
                builder.SetSlotCount(nextSlot);
                builder.EmitEnd();
                builder.Finish(outProgram);
            }
            return {};

        default:
            break;
        }
    }

    if (existingBuilder == nullptr && nextSlot > 0U && outProgram.slotCount == 0U) {
        builder.SetSlotCount(nextSlot);
        builder.EmitEnd();
        builder.Finish(outProgram);
    }
    return {};
}

Base::Result<void> CompileDocumentToBytecode(
    const CompiledDocument& document,
    const Schema& schema,
    XamlBytecodeProgram& outProgram) noexcept {
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Invalid compiled document");
    }
    std::size_t nodeIndex = 0U;
    return CompileNodesToBytecode(
        document.Nodes(),
        nodeIndex,
        schema,
        &document.OriginUri(),
        Meta::InvalidTypeId,
        false,
        outProgram);
}

Base::Result<const XamlBytecodeProgram*> EnsureDocumentBytecode(
    const CompiledDocument& document,
    const Schema& schema) noexcept {
    if (auto cached = document.BytecodeProgram()) {
        if (auto* container = ::Aero::TryCast<XamlBytecodeContainer>(cached.Get())) {
            return &container->program;
        }
    }
    auto container = Base::MakeRef<XamlBytecodeContainer>();
    if (!container) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Allocation failed for XamlBytecodeContainer");
    }
    Base::Result<void> compiled = CompileDocumentToBytecode(
        document, schema, container->program);
    if (!compiled) return compiled.GetStatus();

    const XamlBytecodeProgram* programPtr = &container->program;
    document.SetBytecodeProgram(std::move(container));
    return programPtr;
}

namespace {

using namespace Aero::Meta;
using namespace Aero::Controls;
using namespace Aero::Media;
using Controls::Primitives::Selector;
using namespace Data;
using namespace Media::Animation;

Base::Status InvalidTemplateCompiler(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::ValidationFailed, message);
}

Visual* WalkTemplateAncestor(
    Visual* start,
    Base::StringView ancestorType,
    std::uint32_t level,
    const Meta::TypeRegistry& types) noexcept {
    return ::Aero::Data::FindTemplateAncestor(start, ancestorType, level, types);
}

Base::Result<Value> ConvertTemplateTextValue(
    Meta::Registry& runtime,
    const DependencyProperty& property,
    Base::StringView text) noexcept {
    if ((property.Handle() == Selector::SelectedIndexProperty.Handle() ||
         property.Handle() == TabControl::SelectedIndexProperty.Handle()) &&
        text == Base::StringView("-1")) {
        return ValueCodec<std::uint32_t>::Encode(UINT32_MAX);
    }
    if (property.ValueType() == Meta::TypeOf<Base::Object>()) {
        return BoxXamlStringAsObject(text);
    }
    return runtime.TryConvertText(property.ValueType(), text);
}

inline const DependencyProperty* ResolveTemplateProperty(
    const DependencyPropertyRegistry& properties,
    TypeId targetType,
    Base::StringView name) noexcept {
    return ResolveXamlDependencyProperty(properties, targetType, name);
}

Base::Result<Value> ConvertAndValidateTemplateValue(
    Value value,
    TypeId targetType,
    const DependencyProperty& property,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties,
    const char* roleName) noexcept {
    if (value.IsUnset()) {
        thread_local char message[128];
        std::snprintf(message, sizeof(message), "%s requires Value", roleName);
        return InvalidTemplateCompiler(message);
    }
    if (IsPassthroughMarkupPropertyValue(value) || property.AcceptsAnyValue()) {
        return value;
    }
    if (value.Kind() == ValueKind::String &&
        value.Type() != property.ValueType()) {
        Base::Result<Value> converted =
            ConvertTemplateTextValue(runtime, property, value.AsString());
        if (!converted) {
            return converted.GetStatus();
        }
        value = std::move(converted).Value();
    }
    if (value.Kind() == ValueKind::Object && !value.IsNullObject() && value.AsObject() != nullptr) {
        const Meta::TypeId runtimeObjType = value.AsObject()->RuntimeType();
        if (runtimeObjType != Meta::InvalidTypeId && value.Type() != runtimeObjType) {
            value = Value::FromObject(runtimeObjType, value.AsObject());
        }
    }
    Base::Result<Value> coerced = CoerceXamlValueForProperty(
        property, value, runtime.Types());
    if (!coerced) return coerced.GetStatus();
    value = std::move(coerced).Value();

    const bool objectCompatible =
        value.Kind() == ValueKind::Object &&
        (value.IsNullObject() ||
         runtime.Types().IsDerivedFrom(value.Type(), property.ValueType()));
    if (value.Type() != property.ValueType() && !objectCompatible) {
        thread_local char message[256];
        const TypeInfo* expected = runtime.Types().FindType(property.ValueType());
        const TypeInfo* actual = runtime.Types().FindType(value.Type());
        std::snprintf(
            message, sizeof(message),
            "%s '%.*s' expects '%.*s' but received '%.*s'",
            roleName,
            static_cast<int>(property.Name().SizeBytes()), property.Name().Data(),
            expected != nullptr ? static_cast<int>(expected->Name().SizeBytes()) : 9,
            expected != nullptr ? expected->Name().Data() : "<unknown>",
            actual != nullptr ? static_cast<int>(actual->Name().SizeBytes()) : 9,
            actual != nullptr ? actual->Name().Data() : "<unknown>");
        return InvalidTemplateCompiler(message);
    }
    Base::Result<void> valid =
        properties.ValidateValueFor(property.Handle(), targetType, value);
    return valid
        ? Base::Result<Value>(value)
        : Base::Result<Value>(valid.GetStatus());
}

Base::Result<Value> ConvertSetterValue(
    const Setter& setter,
    TypeId targetType,
    const DependencyProperty& property,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    return ConvertAndValidateTemplateValue(
        setter.GetAuthoredValue(), targetType, property, runtime, properties, "Setter");
}

Base::Status MissingTemplateProperty(
    const char* role,
    Base::StringView property,
    TypeId targetType,
    const TypeRegistry& types) noexcept {
    return FormatMissingTargetPropertyStatus(
        "ControlTemplate",
        role,
        property,
        targetType,
        types,
        Base::ErrorCode::ValidationFailed);
}

Base::Result<Value> ConvertTriggerValue(
    Value value,
    TypeId targetType,
    const DependencyProperty& property,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    return ConvertAndValidateTemplateValue(
        value, targetType, property, runtime, properties, "Template Trigger");
}

struct ResolvedTemplateSetter {
    std::uint32_t slot = 0U;
    std::int32_t partIndex = -1;
    DependencyPropertyHandle property;
    Value value;
};

Base::Result<bool> ResolveAndConvertTemplateSetter(
    Setter& setter,
    const XamlBytecodeProgram& program,
    TypeId defaultTargetType,
    bool emptyTargetUsesRootSlot,
    const NameScope* fallbackAuthoredNames,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties,
    bool requireTargetName,
    const char* missingPropertyRequirementMessage,
    const char* missingTargetMessage,
    const char* missingPropertyMessage,
    ResolvedTemplateSetter& outResolved) noexcept {
    const Base::StringView targetName = setter.GetTargetName();
    const Base::StringView propertyName = setter.GetPropertyName();
    if (!emptyTargetUsesRootSlot &&
        (propertyName.Empty() || (requireTargetName && targetName.Empty()))) {
        return InvalidTemplateCompiler(missingPropertyRequirementMessage);
    }
    std::uint32_t slot = 0U;
    TypeId targetType = defaultTargetType;
    if (!targetName.Empty()) {
        slot = program.FindSlotByName(targetName);
        if (slot == UINT32_MAX || slot >= program.slotNames.Size()) {
            if (fallbackAuthoredNames != nullptr &&
                fallbackAuthoredNames->Find(targetName) != nullptr) {
                return false;
            }
            return InvalidTemplateCompiler(missingTargetMessage);
        }
        targetType = program.GetSlotType(slot);
    } else if (emptyTargetUsesRootSlot) {
        if (program.slotNames.Empty()) {
            return InvalidTemplateCompiler(missingTargetMessage);
        }
        slot = 0U;
        targetType = program.GetSlotType(0U);
    }
    const DependencyProperty* property =
        ResolveTemplateProperty(properties, targetType, propertyName);
    if (property == nullptr) {
        return InvalidTemplateCompiler(missingPropertyMessage);
    }
    Base::Result<Value> converted = ConvertSetterValue(
        setter, targetType, *property, runtime, properties);
    if (!converted) return converted.GetStatus();
    outResolved.slot = slot;
    outResolved.partIndex = !targetName.Empty()
        ? program.FindPartIndexByName(targetName)
        : -1;
    outResolved.property = property->Handle();
    outResolved.value = std::move(converted).Value();
    return true;
}

Base::Result<void> AttachCompiledTemplateTriggers(
    const XamlBytecodeProgram& program,
    const Base::Vector<Base::Ref<Base::Object>>& objects,
    const Base::Vector<CompiledTriggerPlan>& triggers,
    const Base::Vector<Base::Ref<EventTrigger>>& eventTriggers,
    FrameworkElement* templatedParent,
    const Base::Ref<Base::Object>& payload,
    bool registerNamesInRoot) noexcept {
    if (triggers.Empty() && eventTriggers.Empty()) {
        return {};
    }
    if (objects.Empty() ||
        !program.runtime->Types().IsDerivedFrom(
            objects[0U]->RuntimeType(),
            FrameworkElement::StaticTypeId())) {
        return InvalidTemplateCompiler(
            "Template root does not support runtime triggers");
    }
    Base::Result<Base::Ref<DataTemplateTriggerInstance>> created =
        Base::MakeRef<DataTemplateTriggerInstance>();
    if (!created) return created.GetStatus();
    Base::Ref<DataTemplateTriggerInstance> triggerContext =
        std::move(created).Value();
    auto* root = static_cast<FrameworkElement*>(objects[0U].Get());
    triggerContext->root = root;

    for (std::uint32_t index = 0U; index < objects.Size(); ++index) {
        if (index >= program.slotNames.Size() || program.slotNames[index].Empty()) continue;
        DataTemplateTriggerInstance::NamedObject named;
        Base::Result<void> namedAssigned = named.name.Assign(
            program.slotNames[index].View());
        if (!namedAssigned) return namedAssigned.GetStatus();
        named.object = Base::WeakRef<Base::Object>(
            Base::Ref<Base::Object>::FromBorrowed(*objects[index]));
        triggerContext->names.PushBack(std::move(named));
        if (registerNamesInRoot) {
            static_cast<void>(root->RegisterName(
                program.slotNames[index].View(),
                *objects[index]));
        }
    }

    auto sourceFor = [&](const Binding& binding, std::int32_t sourceSlot) noexcept -> Base::Object* {
        if (sourceSlot >= 0 && static_cast<std::uint32_t>(sourceSlot) < objects.Size()) {
            return objects[static_cast<std::uint32_t>(sourceSlot)].Get();
        }
        if (!binding.GetElementName().Empty()) {
            return triggerContext->FindName(binding.GetElementName());
        }
        if (templatedParent != nullptr) {
            const Base::Ref<RelativeSource> relative = binding.GetRelativeSource();
            if (!relative) {
                return templatedParent;
            }
            if (relative->GetMode() == RelativeSourceMode::FindAncestor) {
                return WalkTemplateAncestor(
                    templatedParent,
                    relative->GetAncestorType(),
                    relative->GetAncestorLevel(),
                    program.runtime->Types());
            }
            return templatedParent;
        }
        return payload.Get();
    };

    for (const CompiledTriggerPlan& plan : triggers) {
        const Base::Ref<TriggerBase>& authored = plan.trigger;
        if (!authored) continue;
        DataTemplatePropertyTrigger runtimeTrigger;
        if (authored->RuntimeType() == Trigger::StaticTypeId()) {
            const auto& property = static_cast<const Trigger&>(*authored);
            const CompiledTriggerCondition* precompiled =
                (!plan.conditions.Empty() && plan.conditions[0U].property.IsValid())
                    ? &plan.conditions[0U]
                    : nullptr;
            Base::Object* source = nullptr;
            if (precompiled != nullptr) {
                source = (precompiled->sourceSlot >= 0 &&
                          static_cast<std::uint32_t>(precompiled->sourceSlot) < objects.Size())
                    ? objects[static_cast<std::uint32_t>(precompiled->sourceSlot)].Get()
                    : (templatedParent != nullptr
                           ? static_cast<Base::Object*>(templatedParent)
                           : static_cast<Base::Object*>(root));
            } else if (!property.GetSourceName().Empty()) {
                source = triggerContext->FindName(property.GetSourceName());
            } else {
                source = templatedParent != nullptr
                    ? static_cast<Base::Object*>(templatedParent)
                    : static_cast<Base::Object*>(root);
            }
            if (source == nullptr ||
                !program.runtime->Types().IsDerivedFrom(
                    source->RuntimeType(),
                    DependencyObject::StaticTypeId())) {
                return InvalidTemplateCompiler(
                    "Template action trigger source was not found");
            }
            DataTemplateTriggerCondition condition;
            condition.source = Base::WeakRef<Base::Object>(
                Base::Ref<Base::Object>::FromBorrowed(*source));
            condition.dependencySource =
                Base::WeakRef<DependencyObject>(
                    Base::Ref<DependencyObject>::FromBorrowed(
                        *static_cast<DependencyObject*>(source)));
            if (precompiled != nullptr) {
                condition.property = precompiled->property;
                condition.value = precompiled->value;
            } else {
                const TypeId sourceType = source->RuntimeType();
                const DependencyProperty* sourceProperty =
                    ResolveTemplateProperty(
                        *program.properties,
                        sourceType,
                        property.GetPropertyName());
                if (sourceProperty == nullptr) {
                    return MissingTemplateProperty(
                        "Template action trigger property",
                        property.GetPropertyName(), sourceType,
                        program.runtime->Types());
                }
                Base::Result<Value> converted = ConvertTriggerValue(
                    property.GetAuthoredValue(), sourceType, *sourceProperty,
                    *program.runtime, *program.properties);
                if (!converted) return converted.GetStatus();
                condition.property = sourceProperty->Handle();
                condition.value = std::move(converted).Value();
            }
            runtimeTrigger.conditions.PushBack(std::move(condition));
        } else if (authored->RuntimeType() == DataTrigger::StaticTypeId() ||
                   authored->RuntimeType() == MultiDataTrigger::StaticTypeId()) {
            for (const CompiledTriggerCondition& cc : plan.conditions) {
                if (!cc.binding) {
                    return InvalidTemplateCompiler(
                        "Template DataTrigger requires Binding");
                }
                DataTemplateTriggerCondition condition;
                Base::Object* source = sourceFor(*cc.binding, cc.sourceSlot);
                if (source != nullptr) {
                    condition.source =
                        Base::WeakRef<Base::Object>(
                            Base::Ref<Base::Object>::FromBorrowed(*source));
                }
                condition.usesDataContext = cc.usesDataContext;
                condition.binding = cc.binding;
                condition.value = cc.value;
                runtimeTrigger.conditions.PushBack(std::move(condition));
            }
        } else {
            continue;
        }
        if (templatedParent == nullptr || authored->RuntimeType() != Trigger::StaticTypeId()) {
            for (const CompiledTriggerSetter& cs : plan.setters) {
                DataTemplateTriggerSetter runtimeSetter;
                runtimeSetter.target =
                    Base::WeakRef<DependencyObject>(
                        Base::Ref<DependencyObject>::FromBorrowed(
                            *static_cast<DependencyObject*>(objects[cs.target].Get())));
                runtimeSetter.property = cs.property;
                runtimeSetter.value = cs.value;
                runtimeTrigger.setters.PushBack(std::move(runtimeSetter));
            }
        }
        runtimeTrigger.enterActions.Append(authored->GetEnterActions());
        runtimeTrigger.exitActions.Append(authored->GetExitActions());
        triggerContext->triggers.PushBack(std::move(runtimeTrigger));
    }
    for (const Base::Ref<EventTrigger>& eventTrigger : eventTriggers) {
        if (!eventTrigger) continue;
        triggerContext->eventTriggers.PushBack(eventTrigger);
    }
    FrameworkElement* const triggerRoot = triggerContext->root;
    Base::Ref<Base::Object> triggerOwner(triggerContext);
    FrameworkElementSeams::AddAuthoredTrigger(*triggerRoot, std::move(triggerOwner));
    return {};
}

} // namespace

Base::Result<Base::Vector<TemplatePropertyTrigger>>
CompilePropertyTriggers(
    ControlTemplate& controlTemplate,
    const XamlBytecodeProgram& program,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Vector<TemplatePropertyTrigger> compiled;
    auto addCondition = [&controlTemplate, &program, &runtime, &properties](
        Base::StringView propertyName,
        Base::StringView sourceName,
        const Value& authoredValue,
        TemplatePropertyTrigger& trigger) -> Base::Result<void> {
        if (propertyName.Empty() || authoredValue.IsUnset()) {
            return InvalidTemplateCompiler(
                "ControlTemplate Trigger condition requires Property and Value");
        }
        TypeId targetType = controlTemplate.GetTargetType();
        if (!sourceName.Empty()) {
            const std::uint32_t slot = program.FindSlotByName(sourceName);
            if (slot == UINT32_MAX) {
                return InvalidTemplateCompiler(
                    "ControlTemplate Trigger source was not found");
            }
            targetType = program.GetSlotType(slot);
        }
        const DependencyProperty* property =
            ResolveTemplateProperty(properties, targetType, propertyName);
        if (property == nullptr) {
            return MissingTemplateProperty(
                "Trigger property", propertyName, targetType, runtime.Types());
        }
        Base::Result<Value> value = ConvertTriggerValue(
            authoredValue, targetType, *property, runtime, properties);
        if (!value) return value.GetStatus();
        TemplateTriggerCondition condition;
        Base::Result<void> assigned = condition.sourceName.Assign(sourceName);
        if (!assigned) return assigned.GetStatus();
        condition.sourcePartIndex = program.FindPartIndexByName(sourceName);
        condition.property = property->Handle();
        condition.value = std::move(value).Value();
        trigger.conditions.PushBack(std::move(condition));
        return {};
    };
    auto appendSetters = [&controlTemplate, &program, &runtime, &properties](
        Base::Span<const Base::Ref<Setter>> setters,
        TemplatePropertyTrigger& trigger) -> Base::Result<void> {
        const NameScope& fallbackNames = FrameworkTemplateState::AuthoredNames(controlTemplate);
        for (const Base::Ref<Setter>& setterObject : setters) {
            if (!setterObject) {
                return InvalidTemplateCompiler(
                    "ControlTemplate Trigger Setter requires Property");
            }
            ResolvedTemplateSetter resolved;
            Base::Result<bool> status = ResolveAndConvertTemplateSetter(
                *setterObject,
                program,
                controlTemplate.GetTargetType(),
                /*emptyTargetUsesRootSlot=*/false,
                &fallbackNames,
                runtime,
                properties,
                /*requireTargetName=*/false,
                "ControlTemplate Trigger Setter requires Property",
                "ControlTemplate Trigger Setter target was not found",
                "ControlTemplate Trigger Setter property was not found",
                resolved);
            if (!status) return status.GetStatus();
            if (!status.Value()) continue;
            TemplateTriggerSetter setter;
            Base::Result<void> assigned = setter.targetName.Assign(setterObject->GetTargetName());
            if (!assigned) return assigned.GetStatus();
            setter.targetPartIndex = resolved.partIndex;
            setter.property = resolved.property;
            setter.value = std::move(resolved.value);
            trigger.setters.PushBack(std::move(setter));
        }
        return {};
    };
    for (const Base::Ref<Base::Object>& object :
         FrameworkTemplateState::AuthoredTriggers(controlTemplate)) {
        TemplatePropertyTrigger trigger;
        Base::Result<void> configured;
        if (object && object->RuntimeType() == Trigger::StaticTypeId()) {
            const auto& source = static_cast<const Trigger&>(*object);
            configured = addCondition(source.GetPropertyName(), source.GetSourceName(),
                source.GetAuthoredValue(), trigger);
            if (configured) configured = appendSetters(source.GetAuthoredSetters(), trigger);
        } else if (object && object->RuntimeType() == MultiTrigger::StaticTypeId()) {
            const auto& source = static_cast<const MultiTrigger&>(*object);
            for (const Base::Ref<Condition>& condition : source.GetConditions()) {
                if (!condition) return InvalidTemplateCompiler(
                    "MultiTrigger contains a null Condition");
                configured = addCondition(condition->GetPropertyName(), condition->GetSourceName(),
                    condition->GetAuthoredValue(), trigger);
                if (!configured) return configured.GetStatus();
            }
            configured = appendSetters(source.GetAuthoredSetters(), trigger);
        } else {
            continue;
        }
        if (!configured) return configured.GetStatus();
        compiled.PushBack(std::move(trigger));
    }
    return compiled;
}

Base::Result<Base::Vector<VisualStateGroupPlan>>
CompileVisualStates(
    ControlTemplate& controlTemplate,
    const XamlBytecodeProgram& program,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Vector<VisualStateGroupPlan> groups;
    auto compileGroup = [&groups, &program, &runtime, &properties](
        const Base::Ref<Base::Object>& groupObject)
        -> Base::Result<void> {
        if (!groupObject ||
            groupObject->RuntimeType() != VisualStateGroup::StaticTypeId()) {
            return InvalidTemplateCompiler(
                "ControlTemplate VisualStateGroups contains an invalid object");
        }
        const auto& sourceGroup = static_cast<const VisualStateGroup&>(*groupObject);
        if (sourceGroup.GetName().Empty()) {
            return InvalidTemplateCompiler("VisualStateGroup requires Name");
        }
        VisualStateGroupPlan group;
        Base::Result<void> assigned = group.name.Assign(sourceGroup.GetName());
        if (!assigned) return assigned.GetStatus();

        for (const Base::Ref<VisualState>& stateObject : sourceGroup.GetStates()) {
            if (!stateObject ||
                stateObject->RuntimeType() != VisualState::StaticTypeId()) {
                return InvalidTemplateCompiler(
                    "VisualStateGroup contains an invalid VisualState");
            }
            const auto& sourceState = static_cast<const VisualState&>(*stateObject);
            if (sourceState.GetName().Empty()) {
                return InvalidTemplateCompiler("VisualState requires Name");
            }
            VisualStatePlan state;
            assigned = state.name.Assign(sourceState.GetName());
            if (!assigned) return assigned.GetStatus();
            if (auto srcStoryboard = sourceState.GetStoryboard()) {
                auto storyboardCopy = srcStoryboard->Clone();
                if (!storyboardCopy) {
                    storyboardCopy = Base::MakeRef<Storyboard>();
                }
                state.storyboard = std::move(storyboardCopy);
            } else {
                state.storyboard.Reset();
            }

            for (const Base::Ref<Base::Object>& setterObject : sourceState.GetSetters()) {
                if (!setterObject ||
                    setterObject->RuntimeType() != Setter::StaticTypeId()) {
                    return InvalidTemplateCompiler(
                        "VisualState contains an invalid Setter");
                }
                auto& sourceSetter = static_cast<Setter&>(*setterObject);
                ResolvedTemplateSetter resolved;
                Base::Result<bool> status = ResolveAndConvertTemplateSetter(
                    sourceSetter,
                    program,
                    Meta::InvalidTypeId,
                    /*emptyTargetUsesRootSlot=*/false,
                    nullptr,
                    runtime,
                    properties,
                    /*requireTargetName=*/true,
                    "Visual state Setter requires TargetName and Property",
                    "Visual state Setter target was not found",
                    "Visual state Setter property was not found",
                    resolved);
                if (!status) return status.GetStatus();

                VisualStateSetterPlan setter;
                assigned = setter.targetName.Assign(sourceSetter.GetTargetName());
                if (!assigned) return assigned.GetStatus();
                setter.targetPartIndex = resolved.partIndex;
                setter.property = resolved.property;
                setter.value = std::move(resolved.value);
                state.setters.PushBack(std::move(setter));
                sourceSetter.ReleaseAuthored();
            }
            group.states.PushBack(std::move(state));
        }
        for (const Base::Ref<VisualTransition>& transitionObject : sourceGroup.GetTransitions()) {
            if (!transitionObject ||
                transitionObject->RuntimeType() != VisualTransition::StaticTypeId()) {
                return InvalidTemplateCompiler(
                    "VisualStateGroup contains an invalid VisualTransition");
            }
            const auto& sourceTransition = static_cast<const VisualTransition&>(*transitionObject);
            if (sourceTransition.GetFrom().Empty() &&
                sourceTransition.GetTo().Empty() &&
                sourceTransition.GetGeneratedDuration().Empty() &&
                !sourceTransition.GetStoryboard()) {
                return InvalidTemplateCompiler(
                    "VisualTransition must specify a duration or Storyboard");
            }
            if (!sourceTransition.GetFrom().Empty()) {
                bool found = false;
                for (const VisualStatePlan& state : group.states) {
                    found = found || state.name.View() == sourceTransition.GetFrom();
                }
                if (!found) {
                    return InvalidTemplateCompiler(
                        "VisualTransition From state was not found");
                }
            }
            if (!sourceTransition.GetTo().Empty()) {
                bool found = false;
                for (const VisualStatePlan& state : group.states) {
                    found = found || state.name.View() == sourceTransition.GetTo();
                }
                if (!found) {
                    return InvalidTemplateCompiler(
                        "VisualTransition To state was not found");
                }
            }

            VisualTransitionPlan transition;
            assigned = transition.from.Assign(sourceTransition.GetFrom());
            if (assigned) {
                assigned = transition.to.Assign(sourceTransition.GetTo());
            }
            if (!assigned) return assigned.GetStatus();
            if (!sourceTransition.GetGeneratedDuration().Empty()) {
                Storyboard duration;
                duration.SetDuration(sourceTransition.GetGeneratedDuration());
                transition.generatedDurationMicroseconds =
                    Timing(duration).durationMicroseconds;
            }
            transition.generatedEasingFunction =
                sourceTransition.GetGeneratedEasingFunction();
            if (auto srcStoryboard = sourceTransition.GetStoryboard()) {
                auto storyboardCopy = Base::MakeRef<Storyboard>();
                if (!storyboardCopy) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM");
                for (const auto& tl : srcStoryboard->GetTimelines()) {
                    if (!tl) continue;
                    storyboardCopy->AddTimeline(tl);
                }
                transition.storyboard = std::move(storyboardCopy);
            } else {
                transition.storyboard.Reset();
            }
            group.transitions.PushBack(std::move(transition));
        }
        groups.PushBack(std::move(group));
        return {};
    };
    for (const Base::Ref<Base::Object>& groupObject :
         FrameworkTemplateState::AuthoredVisualStateGroups(controlTemplate)) {
        Base::Result<void> compiled = compileGroup(groupObject);
        if (!compiled) return compiled.GetStatus();
    }
    Base::Ref<Base::Object> authoredRoot =
        FrameworkTemplateState::AuthoredVisualTree(controlTemplate);
    if (authoredRoot &&
        runtime.Types().IsDerivedFrom(
            authoredRoot->RuntimeType(),
            DependencyObject::StaticTypeId())) {
        auto& root = static_cast<DependencyObject&>(*authoredRoot);
        Base::Ref<VisualStateGroupCollection> valueStore =
            root.GetValue(VisualStateManager::VisualStateGroupsProperty);
        if (valueStore) {
            for (const Base::Ref<VisualStateGroup>& groupObject : valueStore->GetItems()) {
                Base::Result<void> compiled = compileGroup(Base::Ref<Base::Object>(groupObject));
                if (!compiled) return compiled.GetStatus();
            }
        }
    }
    return groups;
}

Base::Result<CompiledTriggerPlan> CompileTriggerPlan(
    Base::Ref<Aero::TriggerBase> trigger,
    const XamlBytecodeProgram& program,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties,
    Meta::TypeId controlTargetType) noexcept {
    if (!trigger) {
        return InvalidTemplateCompiler("Null trigger");
    }
    CompiledTriggerPlan plan;
    plan.trigger = trigger;
    Base::Span<const Base::Ref<Setter>> setters;
    if (trigger->RuntimeType() == Trigger::StaticTypeId()) {
        const auto& propTrigger = static_cast<const Trigger&>(*trigger);
        setters = propTrigger.GetAuthoredSetters();
        std::int32_t sourceSlot = -1;
        if (!propTrigger.GetSourceName().Empty()) {
            const std::uint32_t slot = program.FindSlotByName(propTrigger.GetSourceName());
            if (slot >= program.slotNames.Size()) {
                return InvalidTemplateCompiler(
                    "Template action trigger source was not found");
            }
            sourceSlot = static_cast<std::int32_t>(slot);
        } else if (controlTargetType == Meta::InvalidTypeId) {
            sourceSlot = 0;
        }
        const TypeId sourceType = (sourceSlot >= 0)
            ? program.GetSlotType(static_cast<std::uint32_t>(sourceSlot))
            : controlTargetType;
        if (sourceType != Meta::InvalidTypeId) {
            if (!runtime.Types().IsDerivedFrom(
                    sourceType,
                    DependencyObject::StaticTypeId())) {
                return InvalidTemplateCompiler(
                    "Template action trigger source was not found");
            }
            const DependencyProperty* sourceProperty = ResolveTemplateProperty(
                properties, sourceType, propTrigger.GetPropertyName());
            if (sourceProperty == nullptr) {
                return MissingTemplateProperty(
                    "Template action trigger property",
                    propTrigger.GetPropertyName(), sourceType,
                    runtime.Types());
            }
            Base::Result<Value> converted = ConvertTriggerValue(
                propTrigger.GetAuthoredValue(), sourceType, *sourceProperty,
                runtime, properties);
            if (!converted) return converted.GetStatus();
            CompiledTriggerCondition compiledCondition;
            compiledCondition.sourceSlot = sourceSlot;
            compiledCondition.property = sourceProperty->Handle();
            compiledCondition.value = std::move(converted).Value();
            plan.conditions.PushBack(std::move(compiledCondition));
        }
    } else if (trigger->RuntimeType() == DataTrigger::StaticTypeId() ||
               trigger->RuntimeType() == MultiDataTrigger::StaticTypeId()) {
        auto addBindingCondition = [&](const Base::Ref<Binding>& binding, const Value& authoredValue) {
            CompiledTriggerCondition compiledCondition;
            if (!binding->GetElementName().Empty()) {
                const std::uint32_t slot = program.FindSlotByName(binding->GetElementName());
                if (slot < program.slotNames.Size()) {
                    compiledCondition.sourceSlot = static_cast<std::int32_t>(slot);
                }
            }
            compiledCondition.usesDataContext =
                binding->GetElementName().Empty() &&
                !binding->GetRelativeSource();
            if (!binding->GetPathText().Empty()) {
                Data::CompiledBindingPath precompiled;
                if (precompiled.PrecompileTokens(binding->GetPathText())) {
                    Data::CompiledBindingPath::RegisterPrecompiled(
                        binding->GetPathText(), precompiled);
                }
            }
            compiledCondition.binding = binding;
            compiledCondition.value = authoredValue;
            plan.conditions.PushBack(std::move(compiledCondition));
        };
        if (trigger->RuntimeType() == DataTrigger::StaticTypeId()) {
            const auto& data = static_cast<const DataTrigger&>(*trigger);
            setters = data.GetAuthoredSetters();
            if (!data.GetBinding()) {
                return InvalidTemplateCompiler(
                    "Template DataTrigger requires Binding");
            }
            addBindingCondition(data.GetBinding(), data.GetAuthoredValue());
        } else {
            const auto& multi = static_cast<const MultiDataTrigger&>(*trigger);
            setters = multi.GetAuthoredSetters();
            for (const Base::Ref<Condition>& authoredCondition : multi.GetConditions()) {
                if (!authoredCondition || !authoredCondition->GetBinding()) {
                    return InvalidTemplateCompiler(
                        "Template MultiDataTrigger requires complete Conditions");
                }
                addBindingCondition(authoredCondition->GetBinding(), authoredCondition->GetAuthoredValue());
            }
        }
    }
    for (const Base::Ref<Setter>& setter : setters) {
        if (!setter) continue;
        ResolvedTemplateSetter resolved;
        Base::Result<bool> status = ResolveAndConvertTemplateSetter(
            *setter,
            program,
            Meta::InvalidTypeId,
            /*emptyTargetUsesRootSlot=*/true,
            nullptr,
            runtime,
            properties,
            /*requireTargetName=*/false,
            "Template trigger Setter property was not found",
            "Template trigger Setter target was not found",
            "Template trigger Setter property was not found",
            resolved);
        if (!status) return status.GetStatus();
        CompiledTriggerSetter compiledSetter;
        compiledSetter.target = resolved.slot;
        compiledSetter.property = resolved.property;
        compiledSetter.value = std::move(resolved.value);
        plan.setters.PushBack(std::move(compiledSetter));
    }
    return plan;
}

Base::Result<void> CompileAuthoredTemplateTriggers(
    Aero::FrameworkTemplate& targetTemplate,
    XamlBytecodeProgram& subProg,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties,
    bool isControlTemplate) noexcept {
    if (isControlTemplate) {
        auto& ct = static_cast<Controls::ControlTemplate&>(targetTemplate);
        if (!subProg.controlTemplateDataTriggers.Empty() ||
            !subProg.controlTemplateEventTriggers.Empty() ||
            FrameworkTemplateState::AuthoredTriggers(ct).Empty()) {
            return {};
        }
        for (const Base::Ref<Base::Object>& authored :
             FrameworkTemplateState::AuthoredTriggers(ct)) {
            if (!authored) continue;
            const Meta::TypeId kind = authored->RuntimeType();
            if (kind == DataTrigger::StaticTypeId() ||
                kind == MultiDataTrigger::StaticTypeId() ||
                (kind == Trigger::StaticTypeId() &&
                 (!static_cast<const Trigger&>(*authored).GetEnterActions().Empty() ||
                  !static_cast<const Trigger&>(*authored).GetExitActions().Empty()))) {
                Base::Ref<TriggerBase> retained =
                    Base::Ref<TriggerBase>::TryFromBorrowed(
                        static_cast<TriggerBase&>(*authored));
                if (!retained) continue;
                Base::Result<CompiledTriggerPlan> plan = CompileTriggerPlan(
                    std::move(retained), subProg, runtime, properties, ct.GetTargetType());
                if (!plan) return plan.GetStatus();
                subProg.controlTemplateDataTriggers.PushBack(std::move(plan).Value());
            } else if (runtime.Types().IsDerivedFrom(
                           kind, Media::Animation::EventTrigger::StaticTypeId())) {
                Base::Ref<Media::Animation::EventTrigger> retained =
                    Base::Ref<Media::Animation::EventTrigger>::TryFromBorrowed(
                        static_cast<Media::Animation::EventTrigger&>(*authored));
                if (!retained) continue;
                subProg.controlTemplateEventTriggers.PushBack(std::move(retained));
            }
        }
        return {};
    }

    if (!subProg.dataTemplateTriggers.Empty() ||
        FrameworkTemplateState::AuthoredTriggers(targetTemplate).Empty()) {
        return {};
    }
    for (const Base::Ref<Base::Object>& authored :
         FrameworkTemplateState::AuthoredTriggers(targetTemplate)) {
        if (!authored) continue;
        const Meta::TypeId kind = authored->RuntimeType();
        if (kind == DataTrigger::StaticTypeId() ||
            kind == MultiDataTrigger::StaticTypeId() ||
            kind == Trigger::StaticTypeId()) {
            Base::Ref<TriggerBase> retained =
                Base::Ref<TriggerBase>::TryFromBorrowed(
                    static_cast<TriggerBase&>(*authored));
            if (!retained) continue;
            Base::Result<CompiledTriggerPlan> plan = CompileTriggerPlan(
                std::move(retained), subProg, runtime, properties);
            if (!plan) return plan.GetStatus();
            subProg.dataTemplateTriggers.PushBack(std::move(plan).Value());
        }
    }
    return {};
}

Base::Result<void>
CompileInheritedControlTemplate(
    ControlTemplate& derivedTemplate,
    const XamlBytecodeProgram& baseProgram,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    for (const Base::Ref<Base::Object>& object :
        FrameworkTemplateState::AuthoredTriggers(derivedTemplate)) {
        if (!object) continue;
        const Meta::TypeId kind = object->RuntimeType();
        if (kind == EventTrigger::StaticTypeId() ||
            kind == DataTrigger::StaticTypeId() ||
            kind == MultiDataTrigger::StaticTypeId()) {
            return InvalidTemplateCompiler(
                "Inherited ControlTemplate authoring an EventTrigger or DataTrigger requires its own VisualTree");
        }
        if (kind == Trigger::StaticTypeId()) {
            const auto& source = static_cast<const Trigger&>(*object);
            if (!source.GetEnterActions().Empty() ||
                !source.GetExitActions().Empty()) {
                return InvalidTemplateCompiler(
                    "Inherited ControlTemplate Trigger with Enter/ExitActions requires its own VisualTree");
            }
        }
    }
    Base::Result<Base::Vector<TemplatePropertyTrigger>> triggers =
        CompilePropertyTriggers(derivedTemplate, baseProgram, runtime, properties);
    if (!triggers) return triggers.GetStatus();
    for (TemplatePropertyTrigger& trigger : triggers.Value()) {
        Base::Result<void> added =
            FrameworkTemplateState::AddPropertyTrigger(derivedTemplate, std::move(trigger));
        if (!added) return added.GetStatus();
    }
    Base::Result<Base::Vector<VisualStateGroupPlan>> groups =
        CompileVisualStates(derivedTemplate, baseProgram, runtime, properties);
    if (!groups) return groups.GetStatus();
    for (VisualStateGroupPlan& group : groups.Value()) {
        Base::Result<void> added =
            FrameworkTemplateState::AddVisualStateGroup(derivedTemplate, std::move(group));
        if (!added) return added.GetStatus();
    }
    return {};
}

namespace {

Base::Result<void> ExecuteCompiledTemplateBytecode(
    const XamlBytecodeProgram& program,
    TemplateBuilder* templateBuilder,
    const Base::Ref<Base::Object>& payload,
    BindingEngine* bindings,
    Base::Vector<Base::Ref<Base::Object>>& outObjects) noexcept {
    if (templateBuilder != nullptr) {
        Base::Result<void> executed = XamlVM::Execute(
            program, *program.runtime, templateBuilder, outObjects, nullptr);
        if (!executed) return executed.GetStatus();
        return AttachCompiledTemplateTriggers(
            program, outObjects,
            program.controlTemplateDataTriggers,
            program.controlTemplateEventTriggers,
            &templateBuilder->TemplatedParent(), {}, false);
    }

    XamlVMContext vmContext;
    vmContext.runtime = program.runtime;
    vmContext.schema = program.schema;
    vmContext.dataItemPayload = payload;
    vmContext.bindings = bindings;

    Base::Result<void> executed = XamlVM::Execute(
        program, vmContext, outObjects, nullptr);
    if (!executed) return executed.GetStatus();

    static const Base::Vector<Base::Ref<EventTrigger>> noEventTriggers;
    return AttachCompiledTemplateTriggers(
        program, outObjects,
        program.dataTemplateTriggers,
        noEventTriggers, nullptr, payload, true);
}

} // namespace

Base::Result<void> BuildCompiledTemplate(
    TemplateBuilder& context,
    void* factoryContext) noexcept {
    const auto* program = static_cast<const XamlBytecodeProgram*>(factoryContext);
    if (program == nullptr || !program->IsValid()) {
        return InvalidTemplateCompiler("Compiled template bytecode is invalid");
    }
    Base::Vector<Base::Ref<Base::Object>> objects;
    return ExecuteCompiledTemplateBytecode(*program, &context, {}, nullptr, objects);
}

Base::Result<Base::Ref<Base::Object>>
BuildCompiledDeferredTemplate(
    const Base::Ref<Base::Object>& payload,
    void* factoryContext,
    BindingEngine* bindings) noexcept {
    const auto* program = static_cast<const XamlBytecodeProgram*>(factoryContext);
    if (program == nullptr || !program->IsValid()) {
        return InvalidTemplateCompiler("Deferred template bytecode is invalid");
    }
    Base::Vector<Base::Ref<Base::Object>> objects;
    Base::Result<void> executed =
        ExecuteCompiledTemplateBytecode(*program, nullptr, payload, bindings, objects);
    if (!executed) return executed.GetStatus();
    return objects[0U];
}

namespace {

enum class BytecodeValueTag : std::uint8_t {
    Unset = 0U,
    Boolean = 1U,
    SignedInteger = 2U,
    UnsignedInteger = 3U,
    Double = 4U,
    String = 5U,
    DynamicResource = 6U,
    NullObject = 7U,
    Length = 8U,
    Thickness = 9U,
    CornerRadius = 10U,
    Color = 11U,
    Point = 12U,
    Transform2D = 13U,
    GridLength = 14U,
    TypeReference = 15U,
    BoxedString = 16U,
    SolidColorBrush = 17U,
    ResourceUri = 18U,
    NullableBoolean = 19U,
    Rect = 20U,
    Size = 21U,
    Transform3 = 22U
};

template<class TTo, class TFrom>
TTo BitCastValue(const TFrom& source) noexcept {
    static_assert(sizeof(TTo) == sizeof(TFrom), "BitCastValue requires equal sizes");
    TTo dest{};
    std::memcpy(&dest, &source, sizeof(TTo));
    return dest;
}

Base::Result<void> AppendFloat32(
    Base::Vector<std::uint8_t>& output,
    float value) noexcept {
    return AppendU32(output, BitCastValue<std::uint32_t>(value));
}

Base::Result<float> ReadFloat32(Decoder& decoder) noexcept {
    Base::Result<std::uint32_t> bits = decoder.ReadU32();
    if (!bits) return bits.GetStatus();
    return BitCastValue<float>(bits.Value());
}

Base::Result<void> AppendFloat64(
    Base::Vector<std::uint8_t>& output,
    double value) noexcept {
    return AppendU64(output, BitCastValue<std::uint64_t>(value));
}

Base::Result<double> ReadFloat64(Decoder& decoder) noexcept {
    Base::Result<std::uint64_t> bits = decoder.ReadU64();
    if (!bits) return bits.GetStatus();
    return BitCastValue<double>(bits.Value());
}

template<class T>
Base::Result<Meta::Value> EncodeCustomBytecodeValue(
    Meta::TypeId type,
    const T& value) noexcept {
    static const Meta::ValueTypeRegistration s_reg =
        Meta::MakeValueTypeRegistration<T>();
    return Meta::Value::TryFromCustom(type, &value, &s_reg);
}

Base::Result<void> SerializeBytecodeValue(
    const Meta::Value& value,
    Base::Vector<std::uint8_t>& output) noexcept {
    if (value.IsUnset()) {
        return AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::Unset));
    }
    if (value.IsDynamicResource()) {
        Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::DynamicResource));
        if (!r) return r;
        return AppendString(output, value.AsDynamicResourceKey());
    }
    switch (value.Kind()) {
    case Meta::ValueKind::Boolean: {
        Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::Boolean));
        if (!r) return r;
        r = AppendU64(output, value.Type());
        if (!r) return r;
        return AppendU8(output, value.AsBoolean() ? 1U : 0U);
    }
    case Meta::ValueKind::SignedInteger: {
        Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::SignedInteger));
        if (!r) return r;
        r = AppendU64(output, value.Type());
        if (!r) return r;
        return AppendU64(output, BitCastValue<std::uint64_t>(value.AsSignedInteger()));
    }
    case Meta::ValueKind::UnsignedInteger: {
        Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::UnsignedInteger));
        if (!r) return r;
        r = AppendU64(output, value.Type());
        if (!r) return r;
        return AppendU64(output, value.AsUnsignedInteger());
    }
    case Meta::ValueKind::Double: {
        Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::Double));
        if (!r) return r;
        r = AppendU64(output, value.Type());
        if (!r) return r;
        return AppendFloat64(output, value.AsDouble());
    }
    case Meta::ValueKind::String: {
        Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::String));
        if (!r) return r;
        r = AppendU64(output, value.Type());
        if (!r) return r;
        return AppendString(output, value.AsString());
    }
    case Meta::ValueKind::Custom: {
        if (value.Type() == Meta::TypeOf<::Aero::Length>()) {
            auto decoded = Meta::ValueCodec<::Aero::Length>::Decode(value);
            if (!decoded) return decoded.GetStatus();
            Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::Length));
            if (!r) return r;
            r = AppendFloat64(output, decoded.Value().value);
            if (!r) return r;
            return AppendU8(output, decoded.Value().isAuto ? 1U : 0U);
        }
        if (value.Type() == Meta::TypeOf<Base::Thickness>()) {
            auto decoded = Meta::ValueCodec<Base::Thickness>::Decode(value);
            if (!decoded) return decoded.GetStatus();
            Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::Thickness));
            if (!r) return r;
            r = AppendFloat32(output, decoded.Value().left);
            if (r) r = AppendFloat32(output, decoded.Value().top);
            if (r) r = AppendFloat32(output, decoded.Value().right);
            if (r) r = AppendFloat32(output, decoded.Value().bottom);
            return r;
        }
        if (value.Type() == Meta::TypeOf<Base::CornerRadius>()) {
            auto decoded = Meta::ValueCodec<Base::CornerRadius>::Decode(value);
            if (!decoded) return decoded.GetStatus();
            Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::CornerRadius));
            if (!r) return r;
            r = AppendFloat32(output, decoded.Value().topLeft);
            if (r) r = AppendFloat32(output, decoded.Value().topRight);
            if (r) r = AppendFloat32(output, decoded.Value().bottomRight);
            if (r) r = AppendFloat32(output, decoded.Value().bottomLeft);
            return r;
        }
        if (value.Type() == Meta::TypeOf<Base::Color>()) {
            auto decoded = Meta::ValueCodec<Base::Color>::Decode(value);
            if (!decoded) return decoded.GetStatus();
            Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::Color));
            if (!r) return r;
            r = AppendFloat32(output, decoded.Value().red);
            if (r) r = AppendFloat32(output, decoded.Value().green);
            if (r) r = AppendFloat32(output, decoded.Value().blue);
            if (r) r = AppendFloat32(output, decoded.Value().alpha);
            return r;
        }
        if (value.Type() == Meta::TypeOf<Base::Point>()) {
            auto decoded = Meta::ValueCodec<Base::Point>::Decode(value);
            if (!decoded) return decoded.GetStatus();
            Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::Point));
            if (!r) return r;
            r = AppendFloat32(output, decoded.Value().x);
            if (r) r = AppendFloat32(output, decoded.Value().y);
            return r;
        }
        if (value.Type() == Meta::TypeOf<Base::Transform2D>()) {
            auto decoded = Meta::ValueCodec<Base::Transform2D>::Decode(value);
            if (!decoded) return decoded.GetStatus();
            Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::Transform2D));
            if (!r) return r;
            r = AppendFloat32(output, decoded.Value().m11);
            if (r) r = AppendFloat32(output, decoded.Value().m12);
            if (r) r = AppendFloat32(output, decoded.Value().m21);
            if (r) r = AppendFloat32(output, decoded.Value().m22);
            if (r) r = AppendFloat32(output, decoded.Value().dx);
            if (r) r = AppendFloat32(output, decoded.Value().dy);
            return r;
        }
        if (value.Type() == Meta::TypeOf<::Aero::GridLength>()) {
            auto decoded = Meta::ValueCodec<::Aero::GridLength>::Decode(value);
            if (!decoded) return decoded.GetStatus();
            Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::GridLength));
            if (!r) return r;
            r = AppendFloat64(output, decoded.Value().value);
            if (r) r = AppendU8(output, static_cast<std::uint8_t>(decoded.Value().unit));
            return r;
        }
        if (value.Type() == Meta::TypeOf<Meta::TypeReference>()) {
            auto decoded = Meta::ValueCodec<Meta::TypeReference>::Decode(value);
            if (!decoded) return decoded.GetStatus();
            Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::TypeReference));
            if (!r) return r;
            return AppendU64(output, decoded.Value().type);
        }
        if (value.Type() == Meta::TypeOf<Base::ResourceUri>()) {
            auto decoded = Meta::ValueCodec<Base::ResourceUri>::Decode(value);
            if (!decoded) return decoded.GetStatus();
            Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::ResourceUri));
            if (!r) return r;
            return AppendString(output, decoded.Value().Canonical());
        }
        if (value.Type() == Meta::TypeOf<::Aero::Nullable<bool>>()) {
            auto decoded = Meta::ValueCodec<::Aero::Nullable<bool>>::Decode(value);
            if (!decoded) return decoded.GetStatus();
            Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::NullableBoolean));
            if (!r) return r;
            const std::uint8_t encoded = !decoded.Value().GetHasValue()
                ? 0U
                : (decoded.Value().GetValue() ? 2U : 1U);
            return AppendU8(output, encoded);
        }
        if (value.Type() == Meta::TypeOf<Base::Rect>()) {
            auto decoded = Meta::ValueCodec<Base::Rect>::Decode(value);
            if (!decoded) return decoded.GetStatus();
            Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::Rect));
            if (!r) return r;
            r = AppendFloat64(output, decoded.Value().x);
            if (r) r = AppendFloat64(output, decoded.Value().y);
            if (r) r = AppendFloat64(output, decoded.Value().width);
            if (r) r = AppendFloat64(output, decoded.Value().height);
            return r;
        }
        if (value.Type() == Meta::TypeOf<Base::Size>()) {
            auto decoded = Meta::ValueCodec<Base::Size>::Decode(value);
            if (!decoded) return decoded.GetStatus();
            Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::Size));
            if (!r) return r;
            r = AppendFloat64(output, decoded.Value().width);
            if (r) r = AppendFloat64(output, decoded.Value().height);
            return r;
        }
        if (value.Type() == Meta::TypeOf<Base::Transform3>()) {
            auto decoded = Meta::ValueCodec<Base::Transform3>::Decode(value);
            if (!decoded) return decoded.GetStatus();
            Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::Transform3));
            if (!r) return r;
            const Base::Transform3& m = decoded.Value();
            r = AppendFloat64(output, m.m11);
            if (r) r = AppendFloat64(output, m.m12);
            if (r) r = AppendFloat64(output, m.m13);
            if (r) r = AppendFloat64(output, m.m21);
            if (r) r = AppendFloat64(output, m.m22);
            if (r) r = AppendFloat64(output, m.m23);
            if (r) r = AppendFloat64(output, m.m31);
            if (r) r = AppendFloat64(output, m.m32);
            if (r) r = AppendFloat64(output, m.m33);
            if (r) r = AppendFloat64(output, m.dx);
            if (r) r = AppendFloat64(output, m.dy);
            if (r) r = AppendFloat64(output, m.dz);
            return r;
        }

        return AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::Unset));
    }
    case Meta::ValueKind::Object: {
        if (value.IsNullObject() || !value.AsObject()) {
            Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::NullObject));
            if (!r) return r;
            return AppendU64(output, value.Type());
        }
        Base::Object* obj = value.AsObject().Get();
        if (obj->RuntimeType() == Controls::BoxedItemValue::StaticTypeId()) {
            const auto& boxed = static_cast<const Controls::BoxedItemValue&>(*obj);
            if (boxed.Value().Kind() == Meta::ValueKind::String) {
                Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::BoxedString));
                if (!r) return r;
                return AppendString(output, boxed.Value().AsString());
            }
        }
        if (auto* ff = ::Aero::TryCast<Media::FontFamily>(obj)) {
            Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::String));
            if (!r) return r;
            r = AppendU64(output, value.Type());
            if (!r) return r;
            return AppendString(output, ff->GetSource());
        }
        if (auto* brush = ::Aero::TryCast<Media::SolidColorBrush>(obj)) {
            const Base::Color c = brush->GetColor();
            Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::SolidColorBrush));
            if (!r) return r;
            r = AppendFloat32(output, c.red);
            if (r) r = AppendFloat32(output, c.green);
            if (r) r = AppendFloat32(output, c.blue);
            if (r) r = AppendFloat32(output, c.alpha);
            return r;
        }
        Base::Result<void> r = AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::NullObject));
        if (!r) return r;
        return AppendU64(output, value.Type());
    }
    default:
        return AppendU8(output, static_cast<std::uint8_t>(BytecodeValueTag::Unset));
    }
}

Base::Result<Meta::Value> DeserializeBytecodeValue(
    Decoder& decoder,
    const ::Aero::Meta::Registry& domain,
    std::uint32_t& totalStringBytes,
    std::uint32_t maxStringBytes) noexcept {
    Base::Result<std::uint8_t> rawTag = decoder.ReadU8();
    if (!rawTag) return rawTag.GetStatus();
    const auto tag = static_cast<BytecodeValueTag>(rawTag.Value());
    switch (tag) {
    case BytecodeValueTag::Unset:
        return Meta::Value::Unset();
    case BytecodeValueTag::Boolean: {
        Base::Result<std::uint64_t> typeId = decoder.ReadU64();
        if (!typeId) return typeId.GetStatus();
        Base::Result<std::uint8_t> flag = decoder.ReadU8();
        if (!flag) return flag.GetStatus();
        if (flag.Value() > 1U || domain.Types().FindType(typeId.Value()) == nullptr) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 Boolean value is invalid");
        }
        return Meta::Value::FromBoolean(typeId.Value(), flag.Value() != 0U);
    }
    case BytecodeValueTag::SignedInteger: {
        Base::Result<std::uint64_t> typeId = decoder.ReadU64();
        if (!typeId) return typeId.GetStatus();
        Base::Result<std::uint64_t> bits = decoder.ReadU64();
        if (!bits) return bits.GetStatus();
        if (domain.Types().FindType(typeId.Value()) == nullptr) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 SignedInteger type is invalid");
        }
        return Meta::Value::FromSignedInteger(typeId.Value(), BitCastValue<std::int64_t>(bits.Value()));
    }
    case BytecodeValueTag::UnsignedInteger: {
        Base::Result<std::uint64_t> typeId = decoder.ReadU64();
        if (!typeId) return typeId.GetStatus();
        Base::Result<std::uint64_t> val = decoder.ReadU64();
        if (!val) return val.GetStatus();
        if (domain.Types().FindType(typeId.Value()) == nullptr) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 UnsignedInteger type is invalid");
        }
        return Meta::Value::FromUnsignedInteger(typeId.Value(), val.Value());
    }
    case BytecodeValueTag::Double: {
        Base::Result<std::uint64_t> typeId = decoder.ReadU64();
        if (!typeId) return typeId.GetStatus();
        Base::Result<double> val = ReadFloat64(decoder);
        if (!val) return val.GetStatus();
        if (domain.Types().FindType(typeId.Value()) == nullptr) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 Double type is invalid");
        }
        return Meta::Value::FromDouble(typeId.Value(), val.Value());
    }
    case BytecodeValueTag::String: {
        Base::Result<std::uint64_t> typeId = decoder.ReadU64();
        if (!typeId) return typeId.GetStatus();
        Base::Result<Base::String> str = decoder.ReadString(totalStringBytes, maxStringBytes);
        if (!str) return str.GetStatus();
        if (typeId.Value() != Meta::InvalidTypeId &&
            domain.Types().FindType(typeId.Value()) == nullptr) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 String type is invalid");
        }
        return Meta::Value::TryFromString(
            typeId.Value() != Meta::InvalidTypeId ? typeId.Value() : Meta::TypeOf<Base::String>(),
            str.Value().View());
    }
    case BytecodeValueTag::DynamicResource: {
        Base::Result<Base::String> key = decoder.ReadString(totalStringBytes, maxStringBytes);
        if (!key) return key.GetStatus();
        return Meta::Value::FromDynamicResourceKey(key.Value().View());
    }
    case BytecodeValueTag::NullObject: {
        Base::Result<std::uint64_t> typeId = decoder.ReadU64();
        if (!typeId) return typeId.GetStatus();
        return Meta::Value::NullObject(typeId.Value());
    }
    case BytecodeValueTag::Length: {
        Base::Result<double> val = ReadFloat64(decoder);
        if (!val) return val.GetStatus();
        Base::Result<std::uint8_t> isAuto = decoder.ReadU8();
        if (!isAuto) return isAuto.GetStatus();
        const ::Aero::Length len = isAuto.Value() != 0U
            ? ::Aero::Length::Auto()
            : ::Aero::Length::Pixels(val.Value());
        return EncodeCustomBytecodeValue(Meta::TypeOf<::Aero::Length>(), len);
    }
    case BytecodeValueTag::Thickness: {
        auto l = ReadFloat32(decoder);
        auto t = ReadFloat32(decoder);
        auto r = ReadFloat32(decoder);
        auto b = ReadFloat32(decoder);
        if (!l || !t || !r || !b) return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 Thickness truncated");
        return EncodeCustomBytecodeValue(
            Meta::TypeOf<Base::Thickness>(),
            Base::Thickness{l.Value(), t.Value(), r.Value(), b.Value()});
    }
    case BytecodeValueTag::CornerRadius: {
        auto tl = ReadFloat32(decoder);
        auto tr = ReadFloat32(decoder);
        auto br = ReadFloat32(decoder);
        auto bl = ReadFloat32(decoder);
        if (!tl || !tr || !br || !bl) return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 CornerRadius truncated");
        return EncodeCustomBytecodeValue(
            Meta::TypeOf<Base::CornerRadius>(),
            Base::CornerRadius{tl.Value(), tr.Value(), br.Value(), bl.Value()});
    }
    case BytecodeValueTag::Color: {
        auto r = ReadFloat32(decoder);
        auto g = ReadFloat32(decoder);
        auto b = ReadFloat32(decoder);
        auto a = ReadFloat32(decoder);
        if (!r || !g || !b || !a) return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 Color truncated");
        return EncodeCustomBytecodeValue(
            Meta::TypeOf<Base::Color>(),
            Base::Color{r.Value(), g.Value(), b.Value(), a.Value()});
    }
    case BytecodeValueTag::Point: {
        auto x = ReadFloat32(decoder);
        auto y = ReadFloat32(decoder);
        if (!x || !y) return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 Point truncated");
        return EncodeCustomBytecodeValue(
            Meta::TypeOf<Base::Point>(),
            Base::Point{x.Value(), y.Value()});
    }
    case BytecodeValueTag::Transform2D: {
        auto m11 = ReadFloat32(decoder);
        auto m12 = ReadFloat32(decoder);
        auto m21 = ReadFloat32(decoder);
        auto m22 = ReadFloat32(decoder);
        auto dx = ReadFloat32(decoder);
        auto dy = ReadFloat32(decoder);
        if (!m11 || !m12 || !m21 || !m22 || !dx || !dy) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 Transform2D truncated");
        }
        return EncodeCustomBytecodeValue(
            Meta::TypeOf<Base::Transform2D>(),
            Base::Transform2D{m11.Value(), m12.Value(), m21.Value(), m22.Value(), dx.Value(), dy.Value()});
    }
    case BytecodeValueTag::GridLength: {
        auto val = ReadFloat64(decoder);
        auto unit = decoder.ReadU8();
        if (!val || !unit || unit.Value() > static_cast<std::uint8_t>(::Aero::GridUnitType::Star)) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 GridLength invalid");
        }
        return EncodeCustomBytecodeValue(
            Meta::TypeOf<::Aero::GridLength>(),
            ::Aero::GridLength{val.Value(), static_cast<::Aero::GridUnitType>(unit.Value())});
    }
    case BytecodeValueTag::TypeReference: {
        Base::Result<std::uint64_t> refType = decoder.ReadU64();
        if (!refType) return refType.GetStatus();
        if (domain.Types().FindType(refType.Value()) == nullptr) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 TypeReference invalid");
        }
        return EncodeCustomBytecodeValue(
            Meta::TypeOf<Meta::TypeReference>(),
            Meta::TypeReference{refType.Value()});
    }
    case BytecodeValueTag::BoxedString: {
        Base::Result<Base::String> str = decoder.ReadString(totalStringBytes, maxStringBytes);
        if (!str) return str.GetStatus();
        if (Meta::HasObjectFactory()) {
            return BoxXamlStringAsObject(str.Value().View());
        }
        return Meta::Value::TryFromString(Meta::TypeOf<Base::Object>(), str.Value().View());
    }
    case BytecodeValueTag::SolidColorBrush: {
        auto r = ReadFloat32(decoder);
        auto g = ReadFloat32(decoder);
        auto b = ReadFloat32(decoder);
        auto a = ReadFloat32(decoder);
        if (!r || !g || !b || !a) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 SolidColorBrush truncated");
        }
        const auto toByte = [](float c) noexcept -> unsigned int {
            const float clamped = c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c);
            return static_cast<unsigned int>(clamped * 255.0f + 0.5f);
        };
        char hex[16];
        const int len = std::snprintf(
            hex, sizeof(hex), "#%02X%02X%02X%02X",
            toByte(a.Value()), toByte(r.Value()), toByte(g.Value()), toByte(b.Value()));
        Base::StringView hexView(hex, len > 0 ? static_cast<std::uint32_t>(len) : 0U);
        if (Meta::HasObjectFactory() && domain.IsReady()) {
            if (auto conv = domain.TryConvertText(Meta::TypeOf<Media::Brush>(), hexView)) {
                return conv.Value();
            }
        }
        return Meta::Value::TryFromString(Meta::TypeOf<Media::Brush>(), hexView);
    }
    case BytecodeValueTag::ResourceUri: {
        Base::Result<Base::String> str = decoder.ReadString(totalStringBytes, maxStringBytes);
        if (!str) return str.GetStatus();
        Base::Result<Base::ResourceUri> parsed =
            Base::ResourceUri::Parse(str.Value().View());
        if (!parsed) return parsed.GetStatus();
        return EncodeCustomBytecodeValue(
            Meta::TypeOf<Base::ResourceUri>(),
            parsed.Value());
    }
    case BytecodeValueTag::NullableBoolean: {
        Base::Result<std::uint8_t> encoded = decoder.ReadU8();
        if (!encoded || encoded.Value() > 2U) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 NullableBoolean invalid");
        }
        ::Aero::Nullable<bool> val(nullptr);
        if (encoded.Value() == 1U) val = false;
        else if (encoded.Value() == 2U) val = true;
        return EncodeCustomBytecodeValue(
            Meta::TypeOf<::Aero::Nullable<bool>>(),
            val);
    }
    case BytecodeValueTag::Rect: {
        auto x = ReadFloat64(decoder);
        auto y = ReadFloat64(decoder);
        auto w = ReadFloat64(decoder);
        auto h = ReadFloat64(decoder);
        if (!x || !y || !w || !h) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 Rect truncated");
        }
        return EncodeCustomBytecodeValue(
            Meta::TypeOf<Base::Rect>(),
            Base::Rect{x.Value(), y.Value(), w.Value(), h.Value()});
    }
    case BytecodeValueTag::Size: {
        auto w = ReadFloat64(decoder);
        auto h = ReadFloat64(decoder);
        if (!w || !h) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 Size truncated");
        }
        return EncodeCustomBytecodeValue(
            Meta::TypeOf<Base::Size>(),
            Base::Size{w.Value(), h.Value()});
    }
    case BytecodeValueTag::Transform3: {
        auto m11 = ReadFloat64(decoder);
        auto m12 = ReadFloat64(decoder);
        auto m13 = ReadFloat64(decoder);
        auto m21 = ReadFloat64(decoder);
        auto m22 = ReadFloat64(decoder);
        auto m23 = ReadFloat64(decoder);
        auto m31 = ReadFloat64(decoder);
        auto m32 = ReadFloat64(decoder);
        auto m33 = ReadFloat64(decoder);
        auto dx = ReadFloat64(decoder);
        auto dy = ReadFloat64(decoder);
        auto dz = ReadFloat64(decoder);
        if (!m11 || !m12 || !m13 || !m21 || !m22 || !m23 ||
            !m31 || !m32 || !m33 || !dx || !dy || !dz) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 Transform3 truncated");
        }
        return EncodeCustomBytecodeValue(
            Meta::TypeOf<Base::Transform3>(),
            Base::Transform3{
                m11.Value(), m12.Value(), m13.Value(),
                m21.Value(), m22.Value(), m23.Value(),
                m31.Value(), m32.Value(), m33.Value(),
                dx.Value(), dy.Value(), dz.Value()});
    }
    default:
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 value tag is invalid");
    }
}

} // namespace

Base::Result<void> SerializeBytecodeProgram(
    const XamlBytecodeProgram& program,
    Base::Vector<std::uint8_t>& output) noexcept {
    Base::Result<void> r = AppendU32(output, program.slotCount);
    if (!r) return r;
    r = AppendU32(output, program.contentPresenterSlot);
    if (!r) return r;
    r = AppendU64(output, program.ambientTargetType);
    if (!r) return r;
    r = AppendString(output, program.baseUri.Canonical());
    if (!r) return r;

    r = AppendU32(output, program.namespaces.Size());
    if (!r) return r;
    for (const auto& ns : program.namespaces) {
        r = AppendString(output, ns.prefix.View());
        if (!r) return r;
        r = AppendString(output, ns.uri.View());
        if (!r) return r;
    }

    r = AppendU32(output, program.slotNames.Size());
    if (!r) return r;
    for (const auto& name : program.slotNames) {
        r = AppendString(output, name.View());
        if (!r) return r;
    }

    r = AppendU32(output, program.slotTypes.Size());
    if (!r) return r;
    for (Meta::TypeId t : program.slotTypes) {
        r = AppendU64(output, t);
        if (!r) return r;
    }

    r = AppendU32(output, program.typePool.Size());
    if (!r) return r;
    for (Meta::TypeId t : program.typePool) {
        r = AppendU64(output, t);
        if (!r) return r;
    }

    r = AppendU32(output, program.propertyPool.Size());
    if (!r) return r;
    for (std::uint64_t p : program.propertyPool) {
        r = AppendU64(output, p);
        if (!r) return r;
    }

    r = AppendU32(output, program.stringPool.Size());
    if (!r) return r;
    for (const auto& s : program.stringPool) {
        r = AppendString(output, s.View());
        if (!r) return r;
    }

    r = AppendU32(output, program.valuePool.Size());
    if (!r) return r;
    for (const auto& v : program.valuePool) {
        r = SerializeBytecodeValue(v, output);
        if (!r) return r;
    }

    r = AppendU32(output, program.gridLengths.Size());
    if (!r) return r;
    for (const auto& gl : program.gridLengths) {
        r = AppendFloat64(output, gl.value);
        if (!r) return r;
        r = AppendU8(output, static_cast<std::uint8_t>(gl.unit));
        if (!r) return r;
    }

    r = AppendU32(output, program.gradientStops.Size());
    if (!r) return r;
    for (const auto& gs : program.gradientStops) {
        r = AppendFloat64(output, gs.offset);
        if (r) r = AppendFloat32(output, gs.color.red);
        if (r) r = AppendFloat32(output, gs.color.green);
        if (r) r = AppendFloat32(output, gs.color.blue);
        if (r) r = AppendFloat32(output, gs.color.alpha);
        if (r) r = AppendString(output, gs.name.View());
        if (!r) return r;
    }

    const std::uint32_t bpCount = program.bindingPaths.Size() < program.bindingPathStrings.Size()
        ? program.bindingPaths.Size()
        : program.bindingPathStrings.Size();
    r = AppendU32(output, bpCount);
    if (!r) return r;
    for (std::uint32_t i = 0U; i < bpCount; ++i) {
        r = AppendString(output, program.bindingPathStrings[i].View());
        if (!r) return r;
        const auto steps = program.bindingPaths[i].Steps();
        r = AppendU32(output, steps.Size());
        if (!r) return r;
        for (const Data::CompiledPathStep& step : steps) {
            r = AppendU8(output, static_cast<std::uint8_t>(step.kind));
            if (r) r = AppendU32(output, step.collectionIndex);
            if (r) r = AppendString(output, step.propertyName.View());
            if (r) r = AppendString(output, step.attachedOwnerName.View());
            if (r) r = AppendString(output, step.rawToken.View());
            if (r) r = AppendU8(output, step.attached ? 1U : 0U);
            if (r) r = AppendU8(output, step.hasCollectionIndexAfter ? 1U : 0U);
            if (!r) return r;
        }
    }

    r = AppendU32(output, program.visualStateGroups.Size());
    if (!r) return r;
    for (const Controls::VisualStateGroupPlan& group : program.visualStateGroups) {
        r = AppendString(output, group.name.View());
        if (!r) return r;
        r = AppendU32(output, group.states.Size());
        if (!r) return r;
        for (const Controls::VisualStatePlan& state : group.states) {
            r = AppendString(output, state.name.View());
            if (!r) return r;
            r = AppendU32(output, state.setters.Size());
            if (!r) return r;
            for (const Controls::VisualStateSetterPlan& setter : state.setters) {
                r = AppendString(output, setter.targetName.View());
                if (r) r = AppendU32(output, static_cast<std::uint32_t>(setter.targetPartIndex));
                if (r) r = AppendU64(output, setter.property.value);
                if (r) r = SerializeBytecodeValue(setter.value, output);
                if (!r) return r;
            }
        }
        r = AppendU32(output, group.transitions.Size());
        if (!r) return r;
        for (const Controls::VisualTransitionPlan& trans : group.transitions) {
            r = AppendString(output, trans.from.View());
            if (r) r = AppendString(output, trans.to.View());
            if (r) r = AppendU64(output, trans.generatedDurationMicroseconds);
            if (!r) return r;
        }
    }

    r = AppendU32(output, program.instructions.Size());
    if (!r) return r;
    for (const XamlInstruction& inst : program.instructions) {
        r = AppendU8(output, static_cast<std::uint8_t>(inst.op));
        if (r) r = AppendU8(output, inst.flags);
        if (r) r = AppendU16(output, inst.slot);
        if (r) r = AppendU16(output, inst.auxSlot);
        if (r) r = AppendU16(output, inst.typeOrPropIdx);
        if (r) r = AppendU32(output, inst.operandIdx);
        if (r) r = AppendU32(output, inst.extraIdx);
        if (!r) return r;
    }

    r = AppendU32(output, program.subPrograms.Size());
    if (!r) return r;
    for (const auto& subObj : program.subPrograms) {
        const auto* subContainer = ::Aero::TryCast<const XamlBytecodeContainer>(subObj.Get());
        if (subContainer == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "AXB3 subProgram is not a XamlBytecodeContainer");
        }
        r = SerializeBytecodeProgram(subContainer->program, output);
        if (!r) return r;
    }

    return {};
}

Base::Result<void> DeserializeBytecodeProgram(
    Decoder& decoder,
    const ::Aero::Meta::Registry& domain,
    const CompiledDocumentLimits& limits,
    std::uint32_t& totalStringBytes,
    XamlBytecodeProgram& outProgram) noexcept {
    outProgram.Clear();
    auto slotCount = decoder.ReadU32();
    auto presenterSlot = decoder.ReadU32();
    auto ambientType = decoder.ReadU64();
    if (!slotCount || !presenterSlot || !ambientType) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 program header truncated");
    }
    if (slotCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange, "AXB3 slotCount exceeds limits");
    }
    outProgram.slotCount = slotCount.Value();
    outProgram.contentPresenterSlot = presenterSlot.Value();
    outProgram.ambientTargetType = ambientType.Value();
    if (outProgram.ambientTargetType != Meta::InvalidTypeId &&
        domain.Types().FindType(outProgram.ambientTargetType) == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 ambientTargetType is invalid");
    }

    Base::Result<Base::String> baseUriStr = decoder.ReadString(totalStringBytes, limits.maxStringBytes);
    if (!baseUriStr) return baseUriStr.GetStatus();
    if (!baseUriStr.Value().Empty()) {
        Base::Result<Base::ResourceUri> parsedUri =
            Base::ResourceUri::Parse(baseUriStr.Value().View());
        if (!parsedUri) return parsedUri.GetStatus();
        outProgram.baseUri = std::move(parsedUri).Value();
    }

    Base::Result<std::uint32_t> nsCount = decoder.ReadU32();
    if (!nsCount) return nsCount.GetStatus();
    if (nsCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange, "AXB3 namespace count exceeds limits");
    }
    outProgram.namespaces.Reserve(nsCount.Value());
    for (std::uint32_t i = 0U; i < nsCount.Value(); ++i) {
        auto prefix = decoder.ReadString(totalStringBytes, limits.maxStringBytes);
        if (!prefix) return prefix.GetStatus();
        auto uri = decoder.ReadString(totalStringBytes, limits.maxStringBytes);
        if (!uri) return uri.GetStatus();
        XamlBytecodeProgram::NamespaceEntry entry;
        entry.prefix = std::move(prefix).Value();
        entry.uri = std::move(uri).Value();
        outProgram.namespaces.PushBack(std::move(entry));
    }

    Base::Result<std::uint32_t> slotNameCount = decoder.ReadU32();
    if (!slotNameCount) return slotNameCount.GetStatus();
    if (slotNameCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange, "AXB3 slotNames count exceeds limits");
    }
    outProgram.slotNames.Reserve(slotNameCount.Value());
    for (std::uint32_t i = 0U; i < slotNameCount.Value(); ++i) {
        auto s = decoder.ReadString(totalStringBytes, limits.maxStringBytes);
        if (!s) return s.GetStatus();
        outProgram.slotNames.PushBack(std::move(s).Value());
    }

    Base::Result<std::uint32_t> slotTypeCount = decoder.ReadU32();
    if (!slotTypeCount) return slotTypeCount.GetStatus();
    if (slotTypeCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange, "AXB3 slotTypes count exceeds limits");
    }
    outProgram.slotTypes.Reserve(slotTypeCount.Value());
    for (std::uint32_t i = 0U; i < slotTypeCount.Value(); ++i) {
        auto t = decoder.ReadU64();
        if (!t) return t.GetStatus();
        if (t.Value() != Meta::InvalidTypeId && domain.Types().FindType(t.Value()) == nullptr) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 slotType is unknown");
        }
        outProgram.slotTypes.PushBack(t.Value());
    }

    Base::Result<std::uint32_t> typeCount = decoder.ReadU32();
    if (!typeCount) return typeCount.GetStatus();
    if (typeCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange, "AXB3 typePool count exceeds limits");
    }
    outProgram.typePool.Reserve(typeCount.Value());
    for (std::uint32_t i = 0U; i < typeCount.Value(); ++i) {
        auto t = decoder.ReadU64();
        if (!t) return t.GetStatus();
        if (t.Value() == Meta::InvalidTypeId || domain.Types().FindType(t.Value()) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB3 type table references an unknown frozen type id");
        }
        outProgram.typePool.PushBack(t.Value());
    }

    Base::Result<std::uint32_t> propCount = decoder.ReadU32();
    if (!propCount) return propCount.GetStatus();
    if (propCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange, "AXB3 propertyPool count exceeds limits");
    }
    outProgram.propertyPool.Reserve(propCount.Value());
    for (std::uint32_t i = 0U; i < propCount.Value(); ++i) {
        auto p = decoder.ReadU64();
        if (!p) return p.GetStatus();
        if (p.Value() != Meta::InvalidMemberId &&
            domain.Types().FindProperty(p.Value()) == nullptr &&
            domain.Types().FindEvent(p.Value()) == nullptr &&
            domain.DependencyProperties().Find(Meta::DependencyPropertyHandle{p.Value()}) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB3 member table references an unknown frozen member id");
        }
        outProgram.propertyPool.PushBack(p.Value());
    }

    Base::Result<std::uint32_t> strCount = decoder.ReadU32();
    if (!strCount) return strCount.GetStatus();
    if (strCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange, "AXB3 stringPool count exceeds limits");
    }
    outProgram.stringPool.Reserve(strCount.Value());
    for (std::uint32_t i = 0U; i < strCount.Value(); ++i) {
        auto s = decoder.ReadString(totalStringBytes, limits.maxStringBytes);
        if (!s) return s.GetStatus();
        outProgram.stringPool.PushBack(std::move(s).Value());
    }

    Base::Result<std::uint32_t> valCount = decoder.ReadU32();
    if (!valCount) return valCount.GetStatus();
    if (valCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange, "AXB3 valuePool count exceeds limits");
    }
    outProgram.valuePool.Reserve(valCount.Value());
    for (std::uint32_t i = 0U; i < valCount.Value(); ++i) {
        auto v = DeserializeBytecodeValue(decoder, domain, totalStringBytes, limits.maxStringBytes);
        if (!v) return v.GetStatus();
        outProgram.valuePool.PushBack(std::move(v).Value());
    }

    Base::Result<std::uint32_t> glCount = decoder.ReadU32();
    if (!glCount) return glCount.GetStatus();
    if (glCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange, "AXB3 gridLengths count exceeds limits");
    }
    outProgram.gridLengths.Reserve(glCount.Value());
    for (std::uint32_t i = 0U; i < glCount.Value(); ++i) {
        auto v = ReadFloat64(decoder);
        auto u = decoder.ReadU8();
        if (!v || !u || u.Value() > static_cast<std::uint8_t>(::Aero::GridUnitType::Star)) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 gridLength invalid");
        }
        outProgram.gridLengths.PushBack({v.Value(), static_cast<::Aero::GridUnitType>(u.Value())});
    }

    Base::Result<std::uint32_t> gsCount = decoder.ReadU32();
    if (!gsCount) return gsCount.GetStatus();
    if (gsCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange, "AXB3 gradientStops count exceeds limits");
    }
    outProgram.gradientStops.Reserve(gsCount.Value());
    for (std::uint32_t i = 0U; i < gsCount.Value(); ++i) {
        auto off = ReadFloat64(decoder);
        auto r = ReadFloat32(decoder);
        auto g = ReadFloat32(decoder);
        auto b = ReadFloat32(decoder);
        auto a = ReadFloat32(decoder);
        auto name = decoder.ReadString(totalStringBytes, limits.maxStringBytes);
        if (!off || !r || !g || !b || !a || !name) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 gradientStop invalid");
        }
        TemplatePrototypeGradientStop stop;
        stop.offset = off.Value();
        stop.color = Base::Color{r.Value(), g.Value(), b.Value(), a.Value()};
        stop.name = std::move(name).Value();
        outProgram.gradientStops.PushBack(std::move(stop));
    }

    Base::Result<std::uint32_t> bpCount = decoder.ReadU32();
    if (!bpCount) return bpCount.GetStatus();
    if (bpCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange, "AXB3 bindingPaths count exceeds limits");
    }
    outProgram.bindingPathStrings.Reserve(bpCount.Value());
    outProgram.bindingPaths.Reserve(bpCount.Value());
    for (std::uint32_t i = 0U; i < bpCount.Value(); ++i) {
        auto pathStr = decoder.ReadString(totalStringBytes, limits.maxStringBytes);
        if (!pathStr) return pathStr.GetStatus();
        auto stepCount = decoder.ReadU32();
        if (!stepCount || stepCount.Value() > limits.maxNodes) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 bindingPath steps invalid");
        }
        Base::Vector<Data::CompiledPathStep> steps;
        steps.Reserve(stepCount.Value());
        for (std::uint32_t s = 0U; s < stepCount.Value(); ++s) {
            auto kind = decoder.ReadU8();
            auto collIdx = decoder.ReadU32();
            auto propName = decoder.ReadString(totalStringBytes, limits.maxStringBytes);
            auto ownerName = decoder.ReadString(totalStringBytes, limits.maxStringBytes);
            auto rawTok = decoder.ReadString(totalStringBytes, limits.maxStringBytes);
            auto attached = decoder.ReadU8();
            auto hasIdx = decoder.ReadU8();
            if (!kind || !collIdx || !propName || !ownerName || !rawTok || !attached || !hasIdx) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 bindingPath step truncated");
            }
            Data::CompiledPathStep step;
            step.kind = static_cast<Meta::BindingPathSegmentKind>(kind.Value());
            step.collectionIndex = collIdx.Value();
            step.propertyName = std::move(propName).Value();
            step.attachedOwnerName = std::move(ownerName).Value();
            step.rawToken = std::move(rawTok).Value();
            step.attached = attached.Value() != 0U;
            step.hasCollectionIndexAfter = hasIdx.Value() != 0U;
            if (step.kind == Meta::BindingPathSegmentKind::CollectionIndex) {
                step.outputType = Meta::TypeOf<Base::Object>();
                step.readable = true;
                step.writable = true;
                step.accessorReadable = true;
                step.accessorWritable = true;
                step.cachedReadable = true;
                step.cachedWritable = true;
            }
            steps.PushBack(std::move(step));
        }
        Data::CompiledBindingPath compiledPath;
        compiledPath.SetPrecompiledSteps(std::move(steps));
        Data::CompiledBindingPath::RegisterPrecompiled(pathStr.Value().View(), compiledPath);
        outProgram.bindingPathStrings.PushBack(std::move(pathStr).Value());
        outProgram.bindingPaths.PushBack(std::move(compiledPath));
    }

    Base::Result<std::uint32_t> vsgCount = decoder.ReadU32();
    if (!vsgCount) return vsgCount.GetStatus();
    if (vsgCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange, "AXB3 visualStateGroups count exceeds limits");
    }
    outProgram.visualStateGroups.Reserve(vsgCount.Value());
    for (std::uint32_t g = 0U; g < vsgCount.Value(); ++g) {
        Controls::VisualStateGroupPlan group;
        auto gName = decoder.ReadString(totalStringBytes, limits.maxStringBytes);
        auto stateCount = decoder.ReadU32();
        if (!gName || !stateCount || stateCount.Value() > limits.maxNodes) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 VisualStateGroup invalid");
        }
        group.name = std::move(gName).Value();
        group.states.Reserve(stateCount.Value());
        for (std::uint32_t s = 0U; s < stateCount.Value(); ++s) {
            Controls::VisualStatePlan state;
            auto sName = decoder.ReadString(totalStringBytes, limits.maxStringBytes);
            auto setterCount = decoder.ReadU32();
            if (!sName || !setterCount || setterCount.Value() > limits.maxNodes) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 VisualState invalid");
            }
            state.name = std::move(sName).Value();
            state.setters.Reserve(setterCount.Value());
            for (std::uint32_t st = 0U; st < setterCount.Value(); ++st) {
                Controls::VisualStateSetterPlan setter;
                auto tName = decoder.ReadString(totalStringBytes, limits.maxStringBytes);
                auto partIdx = decoder.ReadU32();
                auto prop = decoder.ReadU64();
                auto val = DeserializeBytecodeValue(decoder, domain, totalStringBytes, limits.maxStringBytes);
                if (!tName || !partIdx || !prop || !val) {
                    return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 VisualStateSetter invalid");
                }
                setter.targetName = std::move(tName).Value();
                setter.targetPartIndex = static_cast<std::int32_t>(partIdx.Value());
                setter.property = Meta::DependencyPropertyHandle{prop.Value()};
                setter.value = std::move(val).Value();
                state.setters.PushBack(std::move(setter));
            }
            group.states.PushBack(std::move(state));
        }
        auto transCount = decoder.ReadU32();
        if (!transCount || transCount.Value() > limits.maxNodes) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 VisualTransitions invalid");
        }
        group.transitions.Reserve(transCount.Value());
        for (std::uint32_t tr = 0U; tr < transCount.Value(); ++tr) {
            Controls::VisualTransitionPlan trans;
            auto from = decoder.ReadString(totalStringBytes, limits.maxStringBytes);
            auto to = decoder.ReadString(totalStringBytes, limits.maxStringBytes);
            auto dur = decoder.ReadU64();
            if (!from || !to || !dur) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 VisualTransition invalid");
            }
            trans.from = std::move(from).Value();
            trans.to = std::move(to).Value();
            trans.generatedDurationMicroseconds = dur.Value();
            group.transitions.PushBack(std::move(trans));
        }
        outProgram.visualStateGroups.PushBack(std::move(group));
    }

    Base::Result<std::uint32_t> instCount = decoder.ReadU32();
    if (!instCount) return instCount.GetStatus();
    if (instCount.Value() == 0U || instCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange, "AXB3 instructions count exceeds limits");
    }
    outProgram.instructions.Reserve(instCount.Value());
    for (std::uint32_t i = 0U; i < instCount.Value(); ++i) {
        auto op = decoder.ReadU8();
        auto flags = decoder.ReadU8();
        auto slot = decoder.ReadU16();
        auto auxSlot = decoder.ReadU16();
        auto typeOrProp = decoder.ReadU16();
        auto operand = decoder.ReadU32();
        auto extra = decoder.ReadU32();
        if (!op || !flags || !slot || !auxSlot || !typeOrProp || !operand || !extra) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 instruction truncated");
        }
        XamlInstruction inst;
        inst.op = static_cast<XamlOpCode>(op.Value());
        inst.flags = flags.Value();
        inst.slot = slot.Value();
        inst.auxSlot = auxSlot.Value();
        inst.typeOrPropIdx = typeOrProp.Value();
        inst.operandIdx = operand.Value();
        inst.extraIdx = extra.Value();
        outProgram.instructions.PushBack(inst);
    }

    Base::Result<std::uint32_t> subCount = decoder.ReadU32();
    if (!subCount) return subCount.GetStatus();
    if (subCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange, "AXB3 subPrograms count exceeds limits");
    }
    outProgram.subPrograms.Reserve(subCount.Value());
    for (std::uint32_t i = 0U; i < subCount.Value(); ++i) {
        auto subContainer = Base::MakeRef<XamlBytecodeContainer>();
        if (!subContainer) {
            return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "AXB3 subProgram allocation failed");
        }
        Base::Result<void> subRes = DeserializeBytecodeProgram(
            decoder, domain, limits, totalStringBytes, subContainer->program);
        if (!subRes) return subRes.GetStatus();
        outProgram.subPrograms.PushBack(std::move(subContainer));
    }

    if (!outProgram.IsValid()) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB3 deserialized bytecode program is invalid");
    }
    return {};
}

} // namespace Aero::Markup

