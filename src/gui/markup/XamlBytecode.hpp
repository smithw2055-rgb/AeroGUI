#pragma once

#include <Aero/Meta.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Controls/GridLength.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Events.hpp>
#include "gui/markup/XamlBinaryCodec.hpp"
#include "gui/templates/TemplateProgram.hpp"
#include "gui/templates/TemplateBlueprint.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/FrameworkElementSeams.hpp"
#include "gui/markup/XamlSchema.hpp"
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Grid.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Geometries.hpp>

#include <cstdint>
#include <cstring>

namespace Aero::Markup {

// ============================================================================
// XamlBytecode OpCodes (Instruction Set Architecture)
// ============================================================================
// Linear, deterministic, zero-branch instruction set for UI tree instantiation
// aligned with modern high-performance retained-mode GUI bytecode VMs.
enum class XamlOpCode : std::uint8_t {
    Nop = 0,
    CreateObject,               // [slot: u16, typeId: u64]
    SetPropertyValue,           // [slot: u16, propertyHandle: u64, valueIndex: u32]
    SetPropertyObject,          // [slot: u16, propertyHandle: u64, sourceSlot: u16]
    SetPropertyContent,         // [parentSlot: u16, memberId: u64, childSlot: u16, isPropertyEdge: u8]
    SetRootVisual,              // [slot: u16, nameIndex: u32]
    AddVisualChild,             // [parentSlot: u16, childSlot: u16, nameIndex: u32]
    AddObjectPart,              // [slot: u16, nameIndex: u32]
    SetGridDefinitions,         // [slot: u16, colStart: u32, colCount: u16, rowStart: u32, rowCount: u16]
    SetStreamGeometryData,      // [slot: u16, stringIndex: u32]
    SetGradientStops,           // [slot: u16, stopStart: u32, stopCount: u16]
    ProjectContent,             // [presenterSlot: u16]
    PopulateContentPresenter,   // [slot: u16]
    PopulateItemsPresenter,     // [slot: u16]
    AttachBehaviorsAndTriggers, // [slot: u16, itemStart: u32, behaviorCount: u16, triggerCount: u16]
    AdoptOrCreateRoot = 15,     // [slot: u16, typeId: u64]
    RegisterDocumentName = 16,  // [slot: u16, nameIndex: u32]
    SetDirectPropertyValue = 17,// [slot: u16, propertyHandle: u64, valueIndex: u32]
    SetDirectPropertyObject = 18,// [slot: u16, propertyHandle: u64, valueType: u64, sourceSlot: u16]
    AttachChildElement = 19,    // [parentSlot: u16, childSlot: u16, memberId: u64, valueType: u64]
    ConnectEventHandler = 20,   // [slot: u16, eventMemberId: u64, handlerNameIndex: u32]
    End = 0xFF
};

// ============================================================================
// XamlBytecodeProgram Container
// ============================================================================
// Self-contained, immutable binary program ready for fast VM execution.
struct XamlBytecodeProgram {
    std::uint32_t slotCount = 0U;
    Base::Vector<std::uint8_t> instructions;
    Base::Vector<Base::String> stringPool;
    Base::Vector<Meta::Value> valuePool;
    Base::Vector<::Aero::GridLength> gridLengths;
    Base::Vector<TemplatePrototypeGradientStop> gradientStops;
    Base::Vector<Base::Ref<Base::Object>> objectRefs;

    // Associated template plans (triggers, bindings, states)
    Base::Vector<TemplatePrototypeBinding> blueprintBindings;
    Base::Vector<Base::Ref<Aero::TriggerBase>> dataTemplateTriggers;
    Base::Vector<Base::Ref<Aero::TriggerBase>> controlTemplateDataTriggers;
    Base::Vector<Base::Ref<Media::Animation::EventTrigger>> controlTemplateEventTriggers;
    std::uint32_t contentPresenterSlot = UINT32_MAX;

    bool IsValid() const noexcept {
        return slotCount > 0U && !instructions.Empty();
    }

    void Clear() noexcept {
        slotCount = 0U;
        instructions.Clear();
        stringPool.Clear();
        valuePool.Clear();
        gridLengths.Clear();
        gradientStops.Clear();
        objectRefs.Clear();
        blueprintBindings.Clear();
        dataTemplateTriggers.Clear();
        controlTemplateDataTriggers.Clear();
        controlTemplateEventTriggers.Clear();
        contentPresenterSlot = UINT32_MAX;
    }
};

// ============================================================================
// XamlBytecodeBuilder
// ============================================================================
// Linear emitter that builds XamlBytecodeProgram with constant deduplication.
class XamlBytecodeBuilder {
public:
    XamlBytecodeBuilder() noexcept {
        // Index 0 in string pool is empty string
        stringPool_.PushBack(Base::String{});
    }

    void SetSlotCount(std::uint32_t count) noexcept {
        slotCount_ = count;
    }

    std::uint32_t AddString(Base::StringView str) noexcept {
        if (str.Empty()) return 0U;
        for (std::uint32_t index = 1U; index < stringPool_.Size(); ++index) {
            if (stringPool_[index].View() == str) {
                return index;
            }
        }
        const std::uint32_t newIndex = stringPool_.Size();
        Base::String s;
        static_cast<void>(s.Assign(str));
        stringPool_.PushBack(std::move(s));
        return newIndex;
    }

    std::uint32_t AddValue(const Meta::Value& value) noexcept {
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
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::CreateObject)));
        static_cast<void>(AppendU16(instructions_, slot));
        static_cast<void>(AppendU64(instructions_, typeId));
    }

    void EmitSetPropertyValue(
        std::uint16_t slot,
        Meta::DependencyPropertyHandle property,
        const Meta::Value& value) noexcept {
        const std::uint32_t valueIndex = AddValue(value);
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::SetPropertyValue)));
        static_cast<void>(AppendU16(instructions_, slot));
        static_cast<void>(AppendU64(instructions_, property.value));
        static_cast<void>(AppendU32(instructions_, valueIndex));
    }

    void EmitSetPropertyObject(
        std::uint16_t slot,
        Meta::DependencyPropertyHandle property,
        Meta::TypeId valueType,
        std::uint16_t sourceSlot) noexcept {
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::SetPropertyObject)));
        static_cast<void>(AppendU16(instructions_, slot));
        static_cast<void>(AppendU64(instructions_, property.value));
        static_cast<void>(AppendU64(instructions_, valueType));
        static_cast<void>(AppendU16(instructions_, sourceSlot));
    }

    void EmitSetPropertyContent(
        std::uint16_t parentSlot,
        Meta::MemberId memberId,
        std::uint16_t childSlot,
        Meta::TypeId valueType,
        bool isPropertyEdge) noexcept {
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::SetPropertyContent)));
        static_cast<void>(AppendU16(instructions_, parentSlot));
        static_cast<void>(AppendU64(instructions_, memberId));
        static_cast<void>(AppendU16(instructions_, childSlot));
        static_cast<void>(AppendU64(instructions_, valueType));
        static_cast<void>(AppendU8(instructions_, isPropertyEdge ? 1U : 0U));
    }

    void EmitSetRootVisual(std::uint16_t slot, Base::StringView name) noexcept {
        const std::uint32_t nameIndex = AddString(name);
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::SetRootVisual)));
        static_cast<void>(AppendU16(instructions_, slot));
        static_cast<void>(AppendU32(instructions_, nameIndex));
    }

    void EmitAddVisualChild(
        std::uint16_t parentSlot,
        std::uint16_t childSlot,
        Base::StringView name) noexcept {
        const std::uint32_t nameIndex = AddString(name);
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::AddVisualChild)));
        static_cast<void>(AppendU16(instructions_, parentSlot));
        static_cast<void>(AppendU16(instructions_, childSlot));
        static_cast<void>(AppendU32(instructions_, nameIndex));
    }

    void EmitAddObjectPart(std::uint16_t slot, Base::StringView name) noexcept {
        const std::uint32_t nameIndex = AddString(name);
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::AddObjectPart)));
        static_cast<void>(AppendU16(instructions_, slot));
        static_cast<void>(AppendU32(instructions_, nameIndex));
    }

    void EmitSetGridDefinitions(
        std::uint16_t slot,
        Base::Span<const ::Aero::GridLength> cols,
        Base::Span<const ::Aero::GridLength> rows) noexcept {
        const std::uint32_t colStart = AddGridLengths(cols);
        const std::uint16_t colCount = static_cast<std::uint16_t>(cols.Size());
        const std::uint32_t rowStart = AddGridLengths(rows);
        const std::uint16_t rowCount = static_cast<std::uint16_t>(rows.Size());

        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::SetGridDefinitions)));
        static_cast<void>(AppendU16(instructions_, slot));
        static_cast<void>(AppendU32(instructions_, colStart));
        static_cast<void>(AppendU16(instructions_, colCount));
        static_cast<void>(AppendU32(instructions_, rowStart));
        static_cast<void>(AppendU16(instructions_, rowCount));
    }

    void EmitSetStreamGeometryData(std::uint16_t slot, Base::StringView data) noexcept {
        const std::uint32_t strIndex = AddString(data);
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::SetStreamGeometryData)));
        static_cast<void>(AppendU16(instructions_, slot));
        static_cast<void>(AppendU32(instructions_, strIndex));
    }

    void EmitSetGradientStops(
        std::uint16_t slot,
        Base::Span<const TemplatePrototypeGradientStop> stops) noexcept {
        const std::uint32_t stopStart = AddGradientStops(stops);
        const std::uint16_t stopCount = static_cast<std::uint16_t>(stops.Size());

        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::SetGradientStops)));
        static_cast<void>(AppendU16(instructions_, slot));
        static_cast<void>(AppendU32(instructions_, stopStart));
        static_cast<void>(AppendU16(instructions_, stopCount));
    }

    void EmitProjectContent(std::uint16_t presenterSlot) noexcept {
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::ProjectContent)));
        static_cast<void>(AppendU16(instructions_, presenterSlot));
    }

    void EmitPopulateContentPresenter(std::uint16_t slot) noexcept {
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::PopulateContentPresenter)));
        static_cast<void>(AppendU16(instructions_, slot));
    }

    void EmitPopulateItemsPresenter(std::uint16_t slot) noexcept {
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::PopulateItemsPresenter)));
        static_cast<void>(AppendU16(instructions_, slot));
    }

    void EmitAttachBehaviorsAndTriggers(
        std::uint16_t slot,
        Base::Span<const Base::Ref<Base::Object>> behaviors,
        Base::Span<const Base::Ref<Base::Object>> triggers) noexcept {
        const std::uint32_t itemStart = AddObjectRefs(behaviors);
        static_cast<void>(AddObjectRefs(triggers));
        const std::uint16_t behaviorCount = static_cast<std::uint16_t>(behaviors.Size());
        const std::uint16_t triggerCount = static_cast<std::uint16_t>(triggers.Size());

        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::AttachBehaviorsAndTriggers)));
        static_cast<void>(AppendU16(instructions_, slot));
        static_cast<void>(AppendU32(instructions_, itemStart));
        static_cast<void>(AppendU16(instructions_, behaviorCount));
        static_cast<void>(AppendU16(instructions_, triggerCount));
    }

    void EmitAdoptOrCreateRoot(std::uint16_t slot, Meta::TypeId typeId) noexcept {
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::AdoptOrCreateRoot)));
        static_cast<void>(AppendU16(instructions_, slot));
        static_cast<void>(AppendU64(instructions_, typeId));
    }

    void EmitRegisterDocumentName(std::uint16_t slot, Base::StringView name) noexcept {
        const std::uint32_t nameIndex = AddString(name);
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::RegisterDocumentName)));
        static_cast<void>(AppendU16(instructions_, slot));
        static_cast<void>(AppendU32(instructions_, nameIndex));
    }

    void EmitSetDirectPropertyValue(
        std::uint16_t slot,
        Meta::DependencyPropertyHandle property,
        const Meta::Value& value) noexcept {
        const std::uint32_t valueIndex = AddValue(value);
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::SetDirectPropertyValue)));
        static_cast<void>(AppendU16(instructions_, slot));
        static_cast<void>(AppendU64(instructions_, property.value));
        static_cast<void>(AppendU32(instructions_, valueIndex));
    }

    void EmitSetDirectPropertyObject(
        std::uint16_t slot,
        Meta::DependencyPropertyHandle property,
        Meta::TypeId valueType,
        std::uint16_t sourceSlot) noexcept {
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::SetDirectPropertyObject)));
        static_cast<void>(AppendU16(instructions_, slot));
        static_cast<void>(AppendU64(instructions_, property.value));
        static_cast<void>(AppendU64(instructions_, valueType));
        static_cast<void>(AppendU16(instructions_, sourceSlot));
    }

    void EmitAttachChildElement(
        std::uint16_t parentSlot,
        std::uint16_t childSlot,
        Meta::MemberId memberId,
        Meta::TypeId valueType) noexcept {
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::AttachChildElement)));
        static_cast<void>(AppendU16(instructions_, parentSlot));
        static_cast<void>(AppendU16(instructions_, childSlot));
        static_cast<void>(AppendU64(instructions_, memberId));
        static_cast<void>(AppendU64(instructions_, valueType));
    }

    void EmitConnectEventHandler(
        std::uint16_t slot,
        Meta::MemberId eventMemberId,
        Base::StringView handlerName) noexcept {
        const std::uint32_t handlerNameIndex = AddString(handlerName);
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::ConnectEventHandler)));
        static_cast<void>(AppendU16(instructions_, slot));
        static_cast<void>(AppendU64(instructions_, eventMemberId));
        static_cast<void>(AppendU32(instructions_, handlerNameIndex));
    }

    void EmitEnd() noexcept {
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::End)));
    }

    void Finish(XamlBytecodeProgram& outProgram) noexcept {
        outProgram.slotCount = slotCount_;
        outProgram.instructions = std::move(instructions_);
        outProgram.stringPool = std::move(stringPool_);
        outProgram.valuePool = std::move(valuePool_);
        outProgram.gridLengths = std::move(gridLengths_);
        outProgram.gradientStops = std::move(gradientStops_);
        outProgram.objectRefs = std::move(objectRefs_);
    }

private:
    std::uint32_t slotCount_ = 0U;
    Base::Vector<std::uint8_t> instructions_;
    Base::Vector<Base::String> stringPool_;
    Base::Vector<Meta::Value> valuePool_;
    Base::Vector<::Aero::GridLength> gridLengths_;
    Base::Vector<TemplatePrototypeGradientStop> gradientStops_;
    Base::Vector<Base::Ref<Base::Object>> objectRefs_;
};

// ============================================================================
// XamlBytecode Reader & Container
// ============================================================================
struct XamlBytecodeReader {
    const std::uint8_t* ip = nullptr;
    const std::uint8_t* end = nullptr;

    bool HasBytes(std::size_t n) const noexcept {
        return static_cast<std::size_t>(end - ip) >= n;
    }

    std::uint8_t ReadU8() noexcept {
        return *ip++;
    }

    std::uint16_t ReadU16() noexcept {
        std::uint16_t v = 0;
        std::memcpy(&v, ip, sizeof(v));
        ip += sizeof(v);
        return v;
    }

    std::uint32_t ReadU32() noexcept {
        std::uint32_t v = 0;
        std::memcpy(&v, ip, sizeof(v));
        ip += sizeof(v);
        return v;
    }

    std::uint64_t ReadU64() noexcept {
        std::uint64_t v = 0;
        std::memcpy(&v, ip, sizeof(v));
        ip += sizeof(v);
        return v;
    }
};

class XamlBytecodeContainer : public Base::Object {
public:
    XamlBytecodeProgram program;
};

// ============================================================================
// Blueprint -> Bytecode Compiler (Bridge)
// ============================================================================
inline Base::Result<void> CompileBlueprintToBytecode(
    const CompiledTemplateBlueprint& blueprint,
    XamlBytecodeProgram& outProgram) noexcept {
    outProgram.Clear();
    outProgram.slotCount = blueprint.nodes.Size();
    if (blueprint.nodes.Empty()) {
        return {};
    }

    XamlBytecodeBuilder builder;
    builder.SetSlotCount(blueprint.nodes.Size());

    // 1. Create all objects
    for (std::uint16_t slot = 0U; slot < blueprint.nodes.Size(); ++slot) {
        const TemplatePrototypeNode& node = blueprint.nodes[slot];
        builder.EmitCreateObject(slot, node.type);
    }

    // 2. Set properties, grids, geometries, stops, behaviors/triggers
    for (std::uint16_t slot = 0U; slot < blueprint.nodes.Size(); ++slot) {
        const TemplatePrototypeNode& node = blueprint.nodes[slot];
        for (const TemplatePrototypeProperty& prop : node.properties) {
            if (prop.objectNode != UINT32_MAX) {
                builder.EmitSetPropertyObject(
                    slot, prop.property, prop.value.Type(), static_cast<std::uint16_t>(prop.objectNode));
            } else {
                builder.EmitSetPropertyValue(slot, prop.property, prop.value);
            }
        }
        if (!node.gridColumns.Empty() || !node.gridRows.Empty()) {
            builder.EmitSetGridDefinitions(
                slot, node.gridColumns.AsSpan(), node.gridRows.AsSpan());
        }
        if (!node.streamGeometryData.Empty()) {
            builder.EmitSetStreamGeometryData(slot, node.streamGeometryData.View());
        }
        if (!node.gradientStops.Empty()) {
            builder.EmitSetGradientStops(slot, node.gradientStops.AsSpan());
        }
        if (!node.authoredBehaviors.Empty() || !node.authoredTriggers.Empty()) {
            builder.EmitAttachBehaviorsAndTriggers(
                slot, node.authoredBehaviors.AsSpan(), node.authoredTriggers.AsSpan());
        }
    }

    // 3. Set parent-child structural/content edges
    for (std::uint16_t slot = 0U; slot < blueprint.nodes.Size(); ++slot) {
        const TemplatePrototypeNode& node = blueprint.nodes[slot];
        if (node.parent == UINT32_MAX) continue;
        const PropertyInfo* structuralProperty = blueprint.runtime != nullptr
            ? blueprint.runtime->Types().FindProperty(node.contentMember)
            : nullptr;
        const bool propertyEdge = structuralProperty != nullptr &&
            (static_cast<std::uint32_t>(structuralProperty->Flags()) &
             static_cast<std::uint32_t>(PropertyFlags::Structural)) != 0U &&
            (static_cast<std::uint32_t>(structuralProperty->Flags()) &
             static_cast<std::uint32_t>(PropertyFlags::Collection)) == 0U &&
            blueprint.runtime->CanWriteProperty(node.contentMember);
        const Meta::TypeId valueType = structuralProperty != nullptr
            ? structuralProperty->ValueType()
            : Meta::InvalidTypeId;
        builder.EmitSetPropertyContent(
            static_cast<std::uint16_t>(node.parent),
            node.contentMember,
            slot,
            valueType,
            propertyEdge);
    }

    // 4. Register visual tree structure and named parts
    for (std::uint16_t slot = 0U; slot < blueprint.nodes.Size(); ++slot) {
        const TemplatePrototypeNode& node = blueprint.nodes[slot];
        const bool isVisual = blueprint.runtime != nullptr &&
            blueprint.runtime->Types().IsDerivedFrom(node.type, Visual::StaticTypeId());
        if (!isVisual) {
            if (!node.name.Empty()) {
                builder.EmitAddObjectPart(slot, node.name.View());
            }
            continue;
        }
        if (node.parent == UINT32_MAX) {
            builder.EmitSetRootVisual(slot, node.name.View());
        } else {
            builder.EmitAddVisualChild(
                static_cast<std::uint16_t>(node.parent), slot, node.name.View());
        }
    }

    // 5. Presenters
    if (blueprint.contentPresenter != UINT32_MAX) {
        builder.EmitProjectContent(
            static_cast<std::uint16_t>(blueprint.contentPresenter));
    }
    for (std::uint16_t slot = 0U; slot < blueprint.nodes.Size(); ++slot) {
        const TemplatePrototypeNode& node = blueprint.nodes[slot];
        if (blueprint.runtime != nullptr) {
            if (blueprint.runtime->Types().IsDerivedFrom(
                    node.type, Aero::Controls::ContentPresenter::StaticTypeId())) {
                builder.EmitPopulateContentPresenter(slot);
            } else if (blueprint.runtime->Types().IsDerivedFrom(
                           node.type, Aero::Controls::ItemsPresenter::StaticTypeId())) {
                builder.EmitPopulateItemsPresenter(slot);
            }
        }
    }

    // 6. Terminate
    builder.EmitEnd();
    builder.Finish(outProgram);

    // Copy associated blueprint dynamic metadata
    outProgram.blueprintBindings = blueprint.bindings;
    outProgram.dataTemplateTriggers = blueprint.dataTemplateTriggers;
    outProgram.controlTemplateDataTriggers = blueprint.controlTemplateDataTriggers;
    outProgram.controlTemplateEventTriggers = blueprint.controlTemplateEventTriggers;
    outProgram.contentPresenterSlot = blueprint.contentPresenter;

    return {};
}

// ============================================================================
// EnsureBytecode Helper
// ============================================================================
inline XamlBytecodeProgram& EnsureBytecode(const CompiledTemplateBlueprint& blueprint) noexcept {
    if (!blueprint.bytecodeProgram) {
        auto container = Base::MakeRef<XamlBytecodeContainer>();
        if (container) {
            static_cast<void>(CompileBlueprintToBytecode(blueprint, container->program));
            blueprint.bytecodeProgram = container;
        }
        return container->program;
    }
    return static_cast<XamlBytecodeContainer*>(blueprint.bytecodeProgram.Get())->program;
}

struct XamlDocumentExecutionContext {
    Schema* schema = nullptr;
    const LoadState* loadContext = nullptr;
    Meta::Registry* runtime = nullptr;
};

struct XamlDocumentExecutionResult {
    Base::Ref<Base::Object> root;
    Aero::NameScope names;
    Aero::ResourceDictionary resources;
    VisualContentPlan visualContent;
};

// ============================================================================
// XamlVM Executor
// ============================================================================
class XamlVM {
public:
    static Base::Result<void> Execute(
        const XamlBytecodeProgram& program,
        Meta::Registry& runtime,
        Controls::TemplateBuilder* context,
        Base::Vector<Base::Ref<Base::Object>>& outObjects,
        Base::Vector<Media::Visual*>* outVisuals = nullptr) noexcept;

    static Base::Result<void> ExecuteDocument(
        const XamlBytecodeProgram& program,
        const XamlDocumentExecutionContext& context,
        XamlDocumentExecutionResult& outResult) noexcept;
};

inline Base::Result<void> XamlVM::Execute(
    const XamlBytecodeProgram& program,
    Meta::Registry& runtime,
    Controls::TemplateBuilder* context,
    Base::Vector<Base::Ref<Base::Object>>& outObjects,
    Base::Vector<Media::Visual*>* outVisuals) noexcept {
    outObjects.Clear();
    if (program.slotCount == 0U || program.instructions.Empty()) {
        return {};
    }
    outObjects.Resize(program.slotCount);

    Base::Vector<Media::Visual*> localVisuals;
    Base::Vector<Media::Visual*>* visuals = outVisuals;
    if (visuals != nullptr) {
        visuals->Clear();
        visuals->Resize(program.slotCount);
    } else if (context != nullptr) {
        localVisuals.Resize(program.slotCount);
        visuals = &localVisuals;
    }

    XamlBytecodeReader reader{program.instructions.Data(), program.instructions.Data() + program.instructions.Size()};

    while (reader.ip < reader.end) {
        const auto opcode = static_cast<XamlOpCode>(reader.ReadU8());
        switch (opcode) {
        case XamlOpCode::Nop:
            break;

        case XamlOpCode::CreateObject: {
            if (!reader.HasBytes(2 + 8)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated CreateObject");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint64_t typeId = reader.ReadU64();
            Base::Result<Base::Ref<Base::Object>> created = runtime.CreateObject(typeId);
            if (!created) return created.GetStatus();
            Base::Ref<Base::Object> owner = std::move(created).Value();
            if (!owner || owner->RuntimeType() != typeId ||
                !runtime.Types().IsDerivedFrom(typeId, DependencyObject::StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled template created an incompatible object");
            }
            if (visuals != nullptr && slot < visuals->Size()) {
                Media::Visual* visual = runtime.Types().IsDerivedFrom(
                    typeId, Media::Visual::StaticTypeId())
                    ? static_cast<Media::Visual*>(owner.Get())
                    : nullptr;
                (*visuals)[slot] = visual;
            }
            if (slot < outObjects.Size()) {
                outObjects[slot] = std::move(owner);
            }
            break;
        }

        case XamlOpCode::SetPropertyValue: {
            if (!reader.HasBytes(2 + 8 + 4)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated SetPropertyValue");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint64_t propHandle = reader.ReadU64();
            const std::uint32_t valIdx = reader.ReadU32();
            if (slot >= outObjects.Size() || !outObjects[slot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in SetPropertyValue");
            }
            if (valIdx >= program.valuePool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid valueIndex in SetPropertyValue");
            }
            auto& depObj = static_cast<DependencyObject&>(*outObjects[slot]);
            depObj.SetTemplateValue(Meta::DependencyPropertyHandle{propHandle}, program.valuePool[valIdx]);
            break;
        }

        case XamlOpCode::SetPropertyObject: {
            if (!reader.HasBytes(2 + 8 + 8 + 2)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated SetPropertyObject");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint64_t propHandle = reader.ReadU64();
            const std::uint64_t valueType = reader.ReadU64();
            const std::uint16_t srcSlot = reader.ReadU16();
            if (slot >= outObjects.Size() || srcSlot >= outObjects.Size() ||
                !outObjects[slot] || !outObjects[srcSlot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in SetPropertyObject");
            }
            auto& depObj = static_cast<DependencyObject&>(*outObjects[slot]);
            depObj.SetTemplateValue(
                Meta::DependencyPropertyHandle{propHandle},
                Meta::Value::FromObject(valueType, outObjects[srcSlot]));
            break;
        }

        case XamlOpCode::SetPropertyContent: {
            if (!reader.HasBytes(2 + 8 + 2 + 8 + 1)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated SetPropertyContent");
            }
            const std::uint16_t parentSlot = reader.ReadU16();
            const std::uint64_t memberId = reader.ReadU64();
            const std::uint16_t childSlot = reader.ReadU16();
            const std::uint64_t valueType = reader.ReadU64();
            const std::uint8_t isPropertyEdge = reader.ReadU8();
            if (parentSlot >= outObjects.Size() || childSlot >= outObjects.Size() ||
                !outObjects[parentSlot] || !outObjects[childSlot] || memberId == Meta::InvalidMemberId) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in SetPropertyContent");
            }
            Base::Result<void> content = (isPropertyEdge != 0U)
                ? runtime.SetProperty(
                    *outObjects[parentSlot],
                    memberId,
                    Meta::Value::FromObject(valueType, outObjects[childSlot]))
                : runtime.WriteContent(
                    *outObjects[parentSlot],
                    memberId,
                    outObjects[childSlot]);
            if (!content) return content.GetStatus();
            break;
        }

        case XamlOpCode::SetRootVisual: {
            if (!reader.HasBytes(2 + 4)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated SetRootVisual");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint32_t nameIdx = reader.ReadU32();
            if (context != nullptr && visuals != nullptr && slot < visuals->Size() && (*visuals)[slot] != nullptr) {
                Base::StringView name = (nameIdx < program.stringPool.Size())
                    ? program.stringPool[nameIdx].View()
                    : Base::StringView{};
                Base::Result<void> added = context->SetRoot(
                    name,
                    Base::Ref<Base::Object>::FromBorrowed(*outObjects[slot]),
                    *(*visuals)[slot]);
                if (!added) return added.GetStatus();
            }
            break;
        }

        case XamlOpCode::AddVisualChild: {
            if (!reader.HasBytes(2 + 2 + 4)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated AddVisualChild");
            }
            const std::uint16_t parentSlot = reader.ReadU16();
            const std::uint16_t childSlot = reader.ReadU16();
            const std::uint32_t nameIdx = reader.ReadU32();
            if (context != nullptr && visuals != nullptr) {
                if (parentSlot >= visuals->Size() || (*visuals)[parentSlot] == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        "Visual template child has a non-Visual parent");
                }
                if (childSlot < visuals->Size() && (*visuals)[childSlot] != nullptr) {
                    Base::StringView name = (nameIdx < program.stringPool.Size())
                        ? program.stringPool[nameIdx].View()
                        : Base::StringView{};
                    Base::Result<void> added = context->AddPart(
                        name,
                        *(*visuals)[parentSlot],
                        Base::Ref<Base::Object>::FromBorrowed(*outObjects[childSlot]),
                        *(*visuals)[childSlot]);
                    if (!added) return added.GetStatus();
                }
            }
            break;
        }

        case XamlOpCode::AddObjectPart: {
            if (!reader.HasBytes(2 + 4)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated AddObjectPart");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint32_t nameIdx = reader.ReadU32();
            if (context != nullptr && slot < outObjects.Size() && outObjects[slot]) {
                Base::StringView name = (nameIdx < program.stringPool.Size())
                    ? program.stringPool[nameIdx].View()
                    : Base::StringView{};
                if (!name.Empty()) {
                    Base::Result<void> named = context->AddObjectPart(
                        name,
                        Base::Ref<Base::Object>::FromBorrowed(*outObjects[slot]),
                        *static_cast<DependencyObject*>(outObjects[slot].Get()));
                    if (!named) return named.GetStatus();
                }
            }
            break;
        }

        case XamlOpCode::SetGridDefinitions: {
            if (!reader.HasBytes(2 + 4 + 2 + 4 + 2)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated SetGridDefinitions");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint32_t colStart = reader.ReadU32();
            const std::uint16_t colCount = reader.ReadU16();
            const std::uint32_t rowStart = reader.ReadU32();
            const std::uint16_t rowCount = reader.ReadU16();
            if (slot < outObjects.Size() && outObjects[slot] &&
                outObjects[slot]->RuntimeType() == Controls::Grid::StaticTypeId()) {
                auto& grid = static_cast<Controls::Grid&>(*outObjects[slot]);
                if (colCount > 0U && colStart + colCount <= program.gridLengths.Size()) {
                    grid.SetColumnDefinitions(Base::Span<const ::Aero::GridLength>(
                        &program.gridLengths[colStart], colCount));
                }
                if (rowCount > 0U && rowStart + rowCount <= program.gridLengths.Size()) {
                    grid.SetRowDefinitions(Base::Span<const ::Aero::GridLength>(
                        &program.gridLengths[rowStart], rowCount));
                }
            }
            break;
        }

        case XamlOpCode::SetStreamGeometryData: {
            if (!reader.HasBytes(2 + 4)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated SetStreamGeometryData");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint32_t strIdx = reader.ReadU32();
            if (slot < outObjects.Size() && outObjects[slot] &&
                runtime.Types().IsDerivedFrom(outObjects[slot]->RuntimeType(), Media::StreamGeometry::StaticTypeId())) {
                auto& geom = static_cast<Media::StreamGeometry&>(*outObjects[slot]);
                if (strIdx < program.stringPool.Size()) {
                    geom.SetData(program.stringPool[strIdx].View());
                }
            }
            break;
        }

        case XamlOpCode::SetGradientStops: {
            if (!reader.HasBytes(2 + 4 + 2)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated SetGradientStops");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint32_t stopStart = reader.ReadU32();
            const std::uint16_t stopCount = reader.ReadU16();
            if (slot < outObjects.Size() && outObjects[slot] &&
                runtime.Types().IsDerivedFrom(outObjects[slot]->RuntimeType(), Media::GradientBrush::StaticTypeId())) {
                auto& gradient = static_cast<Media::GradientBrush&>(*outObjects[slot]);
                gradient.ClearGradientStops();
                for (std::uint16_t i = 0U; i < stopCount; ++i) {
                    const auto& stopRecord = program.gradientStops[stopStart + i];
                    auto stop = Base::MakeRef<Media::GradientStop>();
                    if (!stop) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM");
                    stop->SetOffset(stopRecord.offset);
                    stop->SetColor(stopRecord.color);
                    gradient.AddGradientStop(stop);
                    if (context != nullptr && !stopRecord.name.Empty()) {
                        auto named = context->AddObjectPart(
                            stopRecord.name.View(),
                            Base::Ref<Base::Object>(stop),
                            *stop);
                        if (!named) return named.GetStatus();
                    }
                }
            }
            break;
        }

        case XamlOpCode::ProjectContent: {
            if (!reader.HasBytes(2)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated ProjectContent");
            }
            const std::uint16_t presenterSlot = reader.ReadU16();
            if (context != nullptr && visuals != nullptr && presenterSlot < visuals->Size()) {
                if (runtime.Types().IsDerivedFrom(
                        context->TemplatedParent().RuntimeType(),
                        Controls::ContentControl::StaticTypeId())) {
                    Media::Visual* contentHost = (*visuals)[presenterSlot];
                    if (contentHost != nullptr) {
                        Base::Result<bool> projected = runtime.Types().IsDerivedFrom(
                            contentHost->RuntimeType(), Controls::ContentPresenter::StaticTypeId())
                            ? context->ProjectContent(
                                static_cast<Controls::ContentControl&>(context->TemplatedParent()),
                                static_cast<Controls::ContentPresenter&>(*contentHost))
                            : context->ProjectContent(
                                static_cast<Controls::ContentControl&>(context->TemplatedParent()),
                                static_cast<Controls::ContentControl&>(*contentHost));
                        if (!projected) return projected.GetStatus();
                    }
                }
            }
            break;
        }

        case XamlOpCode::PopulateContentPresenter: {
            if (!reader.HasBytes(2)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated PopulateContentPresenter");
            }
            (void)reader.ReadU16();
            break;
        }

        case XamlOpCode::PopulateItemsPresenter: {
            if (!reader.HasBytes(2)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated PopulateItemsPresenter");
            }
            (void)reader.ReadU16();
            break;
        }

        case XamlOpCode::AttachBehaviorsAndTriggers: {
            if (!reader.HasBytes(2 + 4 + 2 + 2)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated AttachBehaviorsAndTriggers");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint32_t itemStart = reader.ReadU32();
            const std::uint16_t bCount = reader.ReadU16();
            const std::uint16_t tCount = reader.ReadU16();
            if (slot < outObjects.Size() && outObjects[slot]) {
                if (auto* framework = ::Aero::TryCast<FrameworkElement>(outObjects[slot].Get())) {
                    for (std::uint16_t i = 0U; i < bCount; ++i) {
                        FrameworkElementSeams::AddStyleBehaviorPrototype(
                            *framework, program.objectRefs[itemStart + i]);
                    }
                    for (std::uint16_t i = 0U; i < tCount; ++i) {
                        FrameworkElementSeams::AddStyleTriggerPrototype(
                            *framework, program.objectRefs[itemStart + bCount + i]);
                    }
                }
            }
            break;
        }

        case XamlOpCode::End:
            return {};

        default:
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Unknown XamlOpCode encountered during VM execution");
        }
    }
    return {};
}

inline Base::Result<void> XamlVM::ExecuteDocument(
    const XamlBytecodeProgram& program,
    const XamlDocumentExecutionContext& context,
    XamlDocumentExecutionResult& outResult) noexcept {
    if (program.slotCount == 0U || program.instructions.Empty()) {
        return {};
    }
    Base::Vector<Base::Ref<Base::Object>> outObjects;
    outObjects.Resize(program.slotCount);

    XamlBytecodeReader reader{program.instructions.Data(), program.instructions.Data() + program.instructions.Size()};

    while (reader.ip < reader.end) {
        const auto opcode = static_cast<XamlOpCode>(reader.ReadU8());
        switch (opcode) {
        case XamlOpCode::Nop:
            break;

        case XamlOpCode::AdoptOrCreateRoot: {
            if (!reader.HasBytes(2 + 8)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated AdoptOrCreateRoot");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint64_t typeId = reader.ReadU64();
            if (context.loadContext != nullptr && context.loadContext->existingRoot && slot == 0U) {
                const Meta::TypeId actual = context.loadContext->existingRoot->RuntimeType();
                if (actual != typeId && (context.runtime == nullptr || !context.runtime->Types().IsDerivedFrom(actual, typeId))) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidArgument,
                        "Existing root object type is incompatible with document root");
                }
                outObjects[0] = context.loadContext->existingRoot;
            } else {
                if (context.runtime == nullptr) {
                    return Base::Status::Failure(Base::ErrorCode::InvalidState, "Runtime registry missing in ExecuteDocument");
                }
                Base::Result<Base::Ref<Base::Object>> created = context.runtime->CreateObject(typeId);
                if (!created) return created.GetStatus();
                if (slot < outObjects.Size()) {
                    outObjects[slot] = std::move(created).Value();
                }
            }
            break;
        }

        case XamlOpCode::CreateObject: {
            if (!reader.HasBytes(2 + 8)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated CreateObject");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint64_t typeId = reader.ReadU64();
            if (context.runtime == nullptr) {
                return Base::Status::Failure(Base::ErrorCode::InvalidState, "Runtime registry missing in ExecuteDocument");
            }
            Base::Result<Base::Ref<Base::Object>> created = context.runtime->CreateObject(typeId);
            if (!created) return created.GetStatus();
            if (slot < outObjects.Size()) {
                outObjects[slot] = std::move(created).Value();
            }
            break;
        }

        case XamlOpCode::RegisterDocumentName: {
            if (!reader.HasBytes(2 + 4)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated RegisterDocumentName");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint32_t nameIdx = reader.ReadU32();
            if (slot < outObjects.Size() && outObjects[slot] && nameIdx < program.stringPool.Size()) {
                Base::StringView name = program.stringPool[nameIdx].View();
                if (!name.Empty()) {
                    outResult.names.Register(name, *outObjects[slot]);
                    if (outObjects[0]) {
                        if (auto* rootFe = ::Aero::TryCast<FrameworkElement>(outObjects[0].Get())) {
                            rootFe->RegisterName(name, *outObjects[slot]);
                        }
                    }
                }
            }
            break;
        }

        case XamlOpCode::SetDirectPropertyValue: {
            if (!reader.HasBytes(2 + 8 + 4)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated SetDirectPropertyValue");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint64_t propHandle = reader.ReadU64();
            const std::uint32_t valIdx = reader.ReadU32();
            if (slot < outObjects.Size() && outObjects[slot] && valIdx < program.valuePool.Size()) {
                if (auto* depObj = ::Aero::TryCast<DependencyObject>(outObjects[slot].Get())) {
                    depObj->SetValue(
                        Meta::DependencyPropertyHandle{propHandle},
                        program.valuePool[valIdx]);
                } else if (context.runtime != nullptr) {
                    static_cast<void>(context.runtime->SetProperty(
                        *outObjects[slot],
                        propHandle,
                        program.valuePool[valIdx]));
                }
            }
            break;
        }

        case XamlOpCode::SetDirectPropertyObject: {
            if (!reader.HasBytes(2 + 8 + 8 + 2)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated SetDirectPropertyObject");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint64_t propHandle = reader.ReadU64();
            const std::uint64_t valueType = reader.ReadU64();
            const std::uint16_t srcSlot = reader.ReadU16();
            if (slot < outObjects.Size() && srcSlot < outObjects.Size() &&
                outObjects[slot] && outObjects[srcSlot]) {
                if (auto* depObj = ::Aero::TryCast<DependencyObject>(outObjects[slot].Get())) {
                    depObj->SetValue(
                        Meta::DependencyPropertyHandle{propHandle},
                        Meta::Value::FromObject(valueType, outObjects[srcSlot]));
                } else if (context.runtime != nullptr) {
                    static_cast<void>(context.runtime->SetProperty(
                        *outObjects[slot],
                        propHandle,
                        Meta::Value::FromObject(valueType, outObjects[srcSlot])));
                }
            }
            break;
        }

        case XamlOpCode::AttachChildElement: {
            if (!reader.HasBytes(2 + 2 + 8 + 8)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated AttachChildElement");
            }
            const std::uint16_t parentSlot = reader.ReadU16();
            const std::uint16_t childSlot = reader.ReadU16();
            const std::uint64_t memberId = reader.ReadU64();
            const std::uint64_t valueType = reader.ReadU64();
            if (parentSlot < outObjects.Size() && childSlot < outObjects.Size() &&
                outObjects[parentSlot] && outObjects[childSlot]) {
                Base::Object* parent = outObjects[parentSlot].Get();
                Base::Ref<Base::Object> child = outObjects[childSlot];
                if (memberId != Meta::InvalidMemberId && context.runtime != nullptr) {
                    Base::Result<void> res = context.runtime->WriteContent(*parent, memberId, child);
                    if (!res) {
                        res = context.runtime->SetProperty(
                            *parent, memberId, Meta::Value::FromObject(valueType, child));
                    }
                } else {
                    if (auto* panel = ::Aero::TryCast<Controls::Panel>(parent)) {
                        if (auto* uiChild = ::Aero::TryCast<UIElement>(child.Get())) {
                            panel->GetChildren().Add(Base::Ref<UIElement>::FromBorrowed(*uiChild));
                        }
                    } else if (auto* contentCtrl = ::Aero::TryCast<Controls::ContentControl>(parent)) {
                        contentCtrl->SetContent(child);
                    } else if (context.runtime != nullptr) {
                        const Meta::TypeInfo* typeInfo = context.runtime->Types().FindType(parent->RuntimeType());
                        if (typeInfo != nullptr && typeInfo->ContentMember() != Meta::InvalidMemberId) {
                            static_cast<void>(context.runtime->WriteContent(*parent, typeInfo->ContentMember(), child));
                        }
                    }
                }
            }
            break;
        }

        case XamlOpCode::ConnectEventHandler: {
            if (!reader.HasBytes(2 + 8 + 4)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated ConnectEventHandler");
            }
            (void)reader.ReadU16();
            (void)reader.ReadU64();
            (void)reader.ReadU32();
            break;
        }

        case XamlOpCode::SetGridDefinitions: {
            if (!reader.HasBytes(2 + 4 + 2 + 4 + 2)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated SetGridDefinitions");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint32_t colStart = reader.ReadU32();
            const std::uint16_t colCount = reader.ReadU16();
            const std::uint32_t rowStart = reader.ReadU32();
            const std::uint16_t rowCount = reader.ReadU16();
            if (slot < outObjects.Size() && outObjects[slot] &&
                outObjects[slot]->RuntimeType() == Controls::Grid::StaticTypeId()) {
                auto& grid = static_cast<Controls::Grid&>(*outObjects[slot]);
                if (colCount > 0U && colStart + colCount <= program.gridLengths.Size()) {
                    grid.SetColumnDefinitions(Base::Span<const ::Aero::GridLength>(
                        &program.gridLengths[colStart], colCount));
                }
                if (rowCount > 0U && rowStart + rowCount <= program.gridLengths.Size()) {
                    grid.SetRowDefinitions(Base::Span<const ::Aero::GridLength>(
                        &program.gridLengths[rowStart], rowCount));
                }
            }
            break;
        }

        case XamlOpCode::SetStreamGeometryData: {
            if (!reader.HasBytes(2 + 4)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated SetStreamGeometryData");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint32_t strIdx = reader.ReadU32();
            if (slot < outObjects.Size() && outObjects[slot] && context.runtime != nullptr &&
                context.runtime->Types().IsDerivedFrom(outObjects[slot]->RuntimeType(), Media::StreamGeometry::StaticTypeId())) {
                auto& geom = static_cast<Media::StreamGeometry&>(*outObjects[slot]);
                if (strIdx < program.stringPool.Size()) {
                    geom.SetData(program.stringPool[strIdx].View());
                }
            }
            break;
        }

        case XamlOpCode::SetGradientStops: {
            if (!reader.HasBytes(2 + 4 + 2)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated SetGradientStops");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint32_t stopStart = reader.ReadU32();
            const std::uint16_t stopCount = reader.ReadU16();
            if (slot < outObjects.Size() && outObjects[slot] && context.runtime != nullptr &&
                context.runtime->Types().IsDerivedFrom(outObjects[slot]->RuntimeType(), Media::GradientBrush::StaticTypeId())) {
                auto& gradient = static_cast<Media::GradientBrush&>(*outObjects[slot]);
                gradient.ClearGradientStops();
                for (std::uint16_t i = 0U; i < stopCount; ++i) {
                    const auto& stopRecord = program.gradientStops[stopStart + i];
                    auto stop = Base::MakeRef<Media::GradientStop>();
                    if (!stop) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM");
                    stop->SetOffset(stopRecord.offset);
                    stop->SetColor(stopRecord.color);
                    gradient.AddGradientStop(stop);
                }
            }
            break;
        }

        case XamlOpCode::End:
            if (!outObjects.Empty() && outObjects[0]) {
                outResult.root = std::move(outObjects[0]);
            }
            return {};

        default:
            break;
        }
    }

    if (!outObjects.Empty() && outObjects[0]) {
        outResult.root = std::move(outObjects[0]);
    }
    return {};
}

inline Base::Result<void> CompileDocumentToBytecode(
    const CompiledDocument& document,
    const Schema& schema,
    XamlBytecodeProgram& outProgram) noexcept {
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Invalid compiled document");
    }

    XamlBytecodeBuilder builder;

    struct ObjectFrame {
        std::uint16_t slot = 0U;
        Meta::TypeId typeId = Meta::InvalidTypeId;
        bool isRoot = false;
    };

    struct MemberFrame {
        std::uint16_t parentSlot = 0U;
        Meta::MemberId memberId = Meta::InvalidMemberId;
        Meta::TypeId valueType = Meta::InvalidTypeId;
        Meta::MemberKind kind = Meta::MemberKind::Property;
        bool isName = false;
        bool isEvent = false;
    };

    Base::Vector<ObjectFrame> objectStack;
    Base::Vector<MemberFrame> memberStack;
    std::uint16_t nextSlot = 0U;

    for (const Node& node : document.Nodes()) {
        switch (node.Kind()) {
        case NodeKind::NamespaceDeclaration:
            break;

        case NodeKind::StartObject: {
            const std::uint16_t slot = nextSlot++;
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
            if (isRoot) {
                builder.EmitAdoptOrCreateRoot(slot, typeId);
            } else {
                builder.EmitCreateObject(slot, typeId);
            }
            objectStack.PushBack(ObjectFrame{slot, typeId, isRoot});
            break;
        }

        case NodeKind::StartMember: {
            if (objectStack.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "StartMember without active object");
            }
            const std::uint16_t parentSlot = objectStack.Back().slot;
            MemberFrame mf;
            mf.parentSlot = parentSlot;

            if (node.Name().LocalName() == "Name" &&
                (node.Name().NamespaceUri().Empty() ||
                 node.Name().NamespaceUri() == "http://schemas.microsoft.com/winfx/2006/xaml")) {
                mf.isName = true;
            } else if (node.HasCompiledMemberBinding()) {
                const auto& mb = node.CompiledMember();
                mf.memberId = mb.id;
                mf.valueType = mb.valueType;
                mf.kind = mb.kind;
                mf.isEvent = (mb.kind == Meta::MemberKind::Event);
            } else {
                mf.memberId = node.CompiledMemberId();
            }
            memberStack.PushBack(mf);
            break;
        }

        case NodeKind::Value: {
            if (objectStack.Empty() || memberStack.Empty()) {
                break;
            }
            MemberFrame& mf = memberStack.Back();
            if (mf.isName) {
                builder.EmitRegisterDocumentName(mf.parentSlot, node.Value());
            } else if (mf.isEvent) {
                // Event handlers fallback to cursor state machine for complete conformance
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Event handler connection bailout to cursor engine");
            } else {
                if (node.HasCompiledValue()) {
                    builder.EmitSetDirectPropertyValue(
                        mf.parentSlot,
                        Meta::DependencyPropertyHandle{mf.memberId},
                        node.CompiledValue());
                } else if (!node.Value().Empty()) {
                    Base::StringView strVal = node.Value();
                    if (strVal.SizeBytes() > 0U && strVal[0] == '{') {
                        // Complex dynamic markup extension: bail out to cursor engine for 100% conformance!
                        return Base::Status::Failure(
                            Base::ErrorCode::Unsupported,
                            "Dynamic markup extension bailout to cursor engine");
                    }
                    Meta::TypeId targetType = (mf.valueType != Meta::InvalidTypeId)
                        ? mf.valueType
                        : Meta::TypeOf<Base::String>();
                    Base::Result<Meta::Value> converted = schema.ConvertText(
                        targetType, strVal, nullptr);
                    if (converted) {
                        builder.EmitSetDirectPropertyValue(
                            mf.parentSlot,
                            Meta::DependencyPropertyHandle{mf.memberId},
                            converted.Value());
                    } else {
                        return Base::Status::Failure(
                            Base::ErrorCode::Unsupported,
                            "Text conversion fallback");
                    }
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
            if (objectStack.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Unbalanced EndObject");
            }
            ObjectFrame finished = objectStack.Back();
            objectStack.PopBack();

            if (!objectStack.Empty()) {
                ObjectFrame& parent = objectStack.Back();
                if (!memberStack.Empty()) {
                    MemberFrame& mem = memberStack.Back();
                    builder.EmitSetDirectPropertyObject(
                        parent.slot,
                        Meta::DependencyPropertyHandle{mem.memberId},
                        finished.typeId,
                        finished.slot);
                } else {
                    builder.EmitAttachChildElement(
                        parent.slot,
                        finished.slot,
                        Meta::InvalidMemberId,
                        finished.typeId);
                }
            }
            break;
        }

        case NodeKind::EndOfDocument:
            builder.SetSlotCount(nextSlot);
            builder.EmitEnd();
            builder.Finish(outProgram);
            return {};

        default:
            break;
        }
    }

    if (nextSlot > 0U && outProgram.slotCount == 0U) {
        builder.SetSlotCount(nextSlot);
        builder.EmitEnd();
        builder.Finish(outProgram);
    }
    return {};
}

inline Base::Result<void> EnsureDocumentBytecode(
    const CompiledDocument& document,
    const Schema& schema,
    XamlBytecodeProgram& outProgram) noexcept {
    if (auto cached = document.BytecodeProgram()) {
        if (auto* container = ::Aero::TryCast<XamlBytecodeContainer>(cached.Get())) {
            outProgram = container->program;
            return {};
        }
    }
    Base::Result<void> compiled = CompileDocumentToBytecode(
        document, schema, outProgram);
    if (!compiled) return compiled.GetStatus();

    auto container = Base::MakeRef<XamlBytecodeContainer>();
    if (container) {
        container->program = outProgram;
        document.SetBytecodeProgram(std::move(container));
    }
    return {};
}

} // namespace Aero::Markup

