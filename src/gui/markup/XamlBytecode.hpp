#pragma once

#include <Aero/Meta.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Controls/GridLength.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Events.hpp>
#include "gui/templates/TemplateProgram.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/DependencyPropertyRegistry.hpp"
#include "gui/markup/XamlSchema.hpp"

#include <cstdint>
#include <cstring>

namespace Aero {
class BindingEngine;
}

namespace Aero::Markup {

// ============================================================================
// XamlBinaryCodec
// ============================================================================

Base::Result<void> AppendU8(Base::Vector<std::uint8_t>& output, std::uint8_t value) noexcept;
Base::Result<void> AppendU16(Base::Vector<std::uint8_t>& output, std::uint16_t value) noexcept;
Base::Result<void> AppendU32(Base::Vector<std::uint8_t>& output, std::uint32_t value) noexcept;
Base::Result<void> AppendU64(Base::Vector<std::uint8_t>& output, std::uint64_t value) noexcept;
Base::Result<void> AppendVarU64(Base::Vector<std::uint8_t>& output, std::uint64_t value) noexcept;
Base::Result<void> AppendVarU32(Base::Vector<std::uint8_t>& output, std::uint32_t value) noexcept;
Base::Result<void> AppendString(Base::Vector<std::uint8_t>& output, Base::StringView value) noexcept;

class Decoder {
public:
    explicit Decoder(
        Base::Span<const std::uint8_t> bytes,
        const char* truncatedMessage = "Binary payload is truncated") noexcept
        : bytes_(bytes), truncatedMessage_(truncatedMessage) {}

    Base::Result<std::uint8_t> ReadU8() noexcept;
    Base::Result<std::uint16_t> ReadU16() noexcept;
    Base::Result<std::uint32_t> ReadU32() noexcept;
    Base::Result<std::uint64_t> ReadU64() noexcept;
    Base::Result<std::uint64_t> ReadVarU64() noexcept;
    Base::Result<std::uint32_t> ReadVarU32() noexcept;
    Base::Result<Base::String> ReadString(
        std::uint32_t& totalStringBytes,
        std::uint32_t maxStringBytes) noexcept;
    Base::Result<Base::String> ReadString(
        Base::IAllocator& allocator,
        std::uint32_t& totalStringBytes,
        std::uint32_t maxStringBytes) noexcept;

    bool AtEnd() const noexcept { return offset_ == bytes_.Size(); }
    std::uint32_t Offset() const noexcept { return offset_; }
    Base::Span<const std::uint8_t> RemainingBytes() const noexcept {
        return bytes_.Subspan(offset_, bytes_.Size() - offset_);
    }

private:
    Base::Result<Base::String> ReadStringInternal(
        Base::IAllocator* allocator,
        std::uint32_t& totalStringBytes,
        std::uint32_t maxStringBytes) noexcept;
    Base::Status Truncated() const noexcept;
    static Base::Status InvalidVarUInt() noexcept;

    Base::Span<const std::uint8_t> bytes_;
    std::uint32_t offset_ = 0U;
    const char* truncatedMessage_ = "Binary payload is truncated";
};

// ============================================================================
// XamlEventConnection
// ============================================================================

class XamlEventConnection final : public Base::Object {
public:
    XamlEventConnection(
        Base::WeakRef<Base::Object> target,
        Meta::EventHandlerThunk thunk) noexcept
        : target_(std::move(target)), thunk_(thunk) {}

    void Invoke(Base::Object* sender, RoutedEventArgs& args) noexcept {
        Base::Ref<Base::Object> target = target_.Lock();
        if (target && thunk_ != nullptr) {
            thunk_(target.Get(), sender, args);
        }
    }

private:
    Base::WeakRef<Base::Object> target_;
    Meta::EventHandlerThunk thunk_ = nullptr;
};

struct XamlEventInvoker {
    Base::Ref<XamlEventConnection> connection;

    void operator()(Base::Object* sender, RoutedEventArgs& args) const noexcept {
        if (connection) connection->Invoke(sender, args);
    }
    bool operator==(const XamlEventInvoker& other) const noexcept {
        return connection.Get() == other.connection.Get();
    }
};

// ============================================================================
// Template Compilation Contracts
// ============================================================================

struct TemplatePrototypeGradientStop {
    double offset = 0.0;
    ::Aero::Base::Color color{};
    Base::String name;
};

struct CompiledTriggerSetter {
    std::uint32_t target = 0U;
    Meta::DependencyPropertyHandle property;
    Meta::PropertyValue value;
};

struct CompiledTriggerCondition {
    std::uint32_t sourceSlot = UINT32_MAX;
    Meta::DependencyPropertyHandle property;
    Base::Ref<Data::Binding> binding;
    Meta::PropertyValue value;
    bool usesDataContext = false;
};

struct CompiledTriggerPlan {
    Base::Ref<Aero::TriggerBase> trigger;
    Base::Vector<CompiledTriggerCondition> conditions;
    Base::Vector<CompiledTriggerSetter> setters;
};

struct XamlBytecodeProgram;
class XamlBytecodeContainer;

Base::Result<CompiledTriggerPlan> CompileTriggerPlan(
    Base::Ref<Aero::TriggerBase> trigger,
    const XamlBytecodeProgram& program,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties,
    Meta::TypeId controlTargetType = Meta::InvalidTypeId) noexcept;

Base::Result<void> BuildCompiledTemplate(
    Controls::TemplateBuilder& context,
    void* factoryContext) noexcept;

Base::Result<Base::Ref<Base::Object>> BuildCompiledDeferredTemplate(
    const Base::Ref<Base::Object>& payload,
    void* factoryContext,
    Aero::BindingEngine* bindings) noexcept;

Base::Result<Base::Vector<Controls::TemplatePropertyTrigger>> CompilePropertyTriggers(
    Controls::ControlTemplate& controlTemplate,
    const XamlBytecodeProgram& program,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties) noexcept;

Base::Result<Base::Vector<Controls::VisualStateGroupPlan>> CompileVisualStates(
    Controls::ControlTemplate& controlTemplate,
    const XamlBytecodeProgram& program,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties) noexcept;

Base::Result<void> CompileInheritedControlTemplate(
    Controls::ControlTemplate& derivedTemplate,
    const XamlBytecodeProgram& baseProgram,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties) noexcept;

// ============================================================================
// XamlBytecode OpCodes (Instruction Set Architecture)
// ============================================================================
enum class XamlOpCode : std::uint8_t {
    Nop = 0,
    CreateObject,               // [slot: targetSlot, typeOrPropIdx: typePoolIdx]
    SetPropertyValue,           // [slot: targetSlot, typeOrPropIdx: propPoolIdx, operandIdx: valuePoolIdx]
    SetPropertyObject,          // [slot: targetSlot, auxSlot: sourceSlot, typeOrPropIdx: propPoolIdx, extraIdx: typePoolIdx]
    SetPropertyContent,         // [slot: parentSlot, auxSlot: childSlot, typeOrPropIdx: propPoolIdx, extraIdx: typePoolIdx, flags: isPropertyEdge]
    SetRootVisual,              // [slot: targetSlot, operandIdx: stringPoolIdx]
    AddVisualChild,             // [slot: parentSlot, auxSlot: childSlot, operandIdx: stringPoolIdx]
    AddObjectPart,              // [slot: targetSlot, operandIdx: stringPoolIdx]
    SetGridDefinitions,         // [slot: targetSlot, auxSlot: colCount, typeOrPropIdx: rowCount, operandIdx: colStart, extraIdx: rowStart]
    SetGradientStops = 10,      // [slot: targetSlot, auxSlot: stopCount, operandIdx: stopStart]
    ProjectContent,             // [slot: presenterSlot]
    PopulateContentPresenter,   // [slot: targetSlot]
    PopulateItemsPresenter,     // [slot: targetSlot]
    AttachBehaviorsAndTriggers, // [slot: targetSlot, auxSlot: bCount, typeOrPropIdx: tCount, operandIdx: itemStart]
    AdoptOrCreateRoot = 15,     // [slot: targetSlot, typeOrPropIdx: typePoolIdx]
    RegisterDocumentName = 16,  // [slot: targetSlot, operandIdx: stringPoolIdx]
    ConnectEventHandler = 17,   // [slot: targetSlot, typeOrPropIdx: propPoolIdx, operandIdx: stringPoolIdx]
    ApplyMarkupExtension = 18,  // [slot: targetSlot, typeOrPropIdx: propPoolIdx, extraIdx: typePoolIdx, operandIdx: stringPoolIdx]
    SetSlotValue = 19,          // [slot: targetSlot, operandIdx: valuePoolIdx]
    AddDictionaryEntry = 20,    // [slot: dictSlot, auxSlot: valueSlot, operandIdx: keyStringIdx, flags: keyKind]
    PushAmbientResources = 21,  // [slot: dictSlot]
    PopAmbientResources = 22,   // []
    BindTemplateProgram = 23,   // [slot: templateSlot, operandIdx: subProgramIdx]
    SealOrInitializeObject = 24,// [slot: targetSlot]
    ApplyObjectMarkupExtension = 25, // [slot: targetSlot, auxSlot: extSlot, typeOrPropIdx: propPoolIdx]
    End = 0xFF
};

// ============================================================================
// XamlInstruction (16-Byte Aligned Fixed-Length Instruction)
// ============================================================================
struct alignas(8) XamlInstruction {
    XamlOpCode op = XamlOpCode::Nop;     // 1 byte
    std::uint8_t flags = 0U;             // 1 byte (flags, e.g. isPropertyEdge)
    std::uint16_t slot = 0U;             // 2 bytes (primary target / parent / presenter slot)
    std::uint16_t auxSlot = 0U;          // 2 bytes (secondary / child / source slot, or count)
    std::uint16_t typeOrPropIdx = 0U;    // 2 bytes (index into typePool or propertyPool, or count)
    std::uint32_t operandIdx = 0U;       // 4 bytes (index into valuePool, stringPool, or offset)
    std::uint32_t extraIdx = 0U;         // 4 bytes (secondary index, e.g. second type, offset)
};
static_assert(sizeof(XamlInstruction) == 16U, "XamlInstruction must be exactly 16 bytes");

// ============================================================================
// XamlBytecodeProgram Container
// ============================================================================
struct XamlBytecodeProgram {
    std::uint32_t slotCount = 0U;
    Base::Vector<XamlInstruction> instructions;
    Base::Vector<Meta::TypeId> typePool;
    Base::Vector<std::uint64_t> propertyPool;
    Base::Vector<Base::String> stringPool;
    Base::Vector<Meta::Value> valuePool;
    Base::Vector<::Aero::GridLength> gridLengths;
    Base::Vector<TemplatePrototypeGradientStop> gradientStops;
    Base::Vector<Base::Ref<Base::Object>> objectRefs;
    Base::Vector<Base::Ref<Base::Object>> subPrograms;

    // Associated template plans (triggers, bindings, states)
    Base::Vector<CompiledTriggerPlan> dataTemplateTriggers;
    Base::Vector<CompiledTriggerPlan> controlTemplateDataTriggers;
    Base::Vector<Base::Ref<Media::Animation::EventTrigger>> controlTemplateEventTriggers;
    Base::Vector<Base::Ref<Base::Object>> vsmPrograms;
    std::uint32_t contentPresenterSlot = UINT32_MAX;
    Base::Vector<Base::String> slotNames;
    Base::Vector<Meta::TypeId> slotTypes;
    Meta::TypeId ambientTargetType = Meta::InvalidTypeId;
    Meta::Registry* runtime = nullptr;
    Meta::DependencyPropertyRegistry* properties = nullptr;
    const Schema* schema = nullptr;
    struct NamespaceEntry {
        Base::String prefix;
        Base::String uri;
    };
    Base::ResourceUri baseUri;
    Base::Vector<NamespaceEntry> namespaces;
    Base::Vector<Base::WeakRef<Base::Object>> ambientResourceOwners;
    Base::Vector<Aero::ResourceDictionary> ambientDictionaries;
    Aero::ResourceDictionary fallbackResources;
    Base::Object* templateOwner = nullptr;

    std::uint32_t FindSlotByName(Base::StringView name) const noexcept;
    std::int32_t FindPartIndexByName(Base::StringView name) const noexcept;
    Meta::TypeId GetSlotType(std::uint32_t slot) const noexcept;
    bool IsValid() const noexcept;
    void Clear() noexcept;
};

class XamlBytecodeContainer : public Base::Object {
public:
    static constexpr Base::MetaTypeId StaticTypeId() noexcept {
        return Base::MakeMetaTypeId("XamlBytecodeContainer");
    }
    Base::MetaTypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    XamlBytecodeProgram program;
};

struct XamlVMContext {
    Meta::Registry* runtime = nullptr;
    const Schema* schema = nullptr;
    const LoadState* loadContext = nullptr;
    Controls::TemplateBuilder* templateBuilder = nullptr;
    const Controls::ItemsPanelTemplate* itemsPanel = nullptr;
    Base::Ref<Base::Object> existingRoot;
    Base::Ref<Base::Object> dataItemPayload;
    Base::Object* eventHandlerTarget = nullptr;
    ::Aero::BindingEngine* bindings = nullptr;
    Aero::NameScope* documentNames = nullptr;
    Aero::ResourceDictionary* documentResources = nullptr;
    VisualContentPlan* visualContent = nullptr;
    CommittedEffectPlan* documentEffects = nullptr;
    bool* hasDeferredStaticResources = nullptr;
    Base::Object* deferredContentOwner = nullptr;
    Base::ResourceUri* finalizedCanonicalUri = nullptr;
    Base::Vector<Base::ResourceUri>* finalizedDependencies = nullptr;
    bool* finalizedByVM = nullptr;
    bool ignoreUnresolvedResources = false;
};

struct XamlDocumentExecutionContext {
    const Schema* schema = nullptr;
    const LoadState* loadContext = nullptr;
    Meta::Registry* runtime = nullptr;
};

struct XamlDocumentExecutionResult {
    Base::Ref<Base::Object> root;
    Aero::NameScope names;
    Aero::ResourceDictionary resources;
    VisualContentPlan visualContent;
    CommittedEffectPlan effects;
    Base::ResourceUri canonicalUri;
    Base::Vector<Base::ResourceUri> dependencies;
    bool hasDeferredStaticResources = false;
    bool finalizedByVM = false;
};

// ============================================================================
// XamlVM Executor
// ============================================================================
class XamlVM {
public:
    static Aero::ResourceDictionary* ResolveResourceScope(
        const Schema& schema,
        Meta::TypeId scopeType,
        Base::Object& scopeOwner) noexcept {
        return schema.ResolveResourceScope(scopeType, scopeOwner);
    }

    static Base::Result<void> Execute(
        const XamlBytecodeProgram& program,
        const XamlVMContext& context,
        Base::Vector<Base::Ref<Base::Object>>& outObjects,
        Base::Vector<Media::Visual*>* outVisuals = nullptr) noexcept;

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

Base::Result<const XamlBytecodeProgram*> EnsureDocumentBytecode(
    const CompiledDocument& document,
    const Schema& schema) noexcept;

} // namespace Aero::Markup
