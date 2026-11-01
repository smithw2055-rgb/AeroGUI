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
#include <Aero/Resources.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Events.hpp>
#include "gui/templates/TemplateProgram.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/FrameworkElementSeams.hpp"
#include "gui/core/DependencyObjectAccess.hpp"
#include "gui/markup/XamlSchema.hpp"
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Grid.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/ContentElement.hpp>
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/Style.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/FrameworkTemplate.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/BindingOperations.hpp>
#include "gui/core/DependencyPropertyRegistry.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/styles/StyleEngine.hpp"

#include <cstdint>
#include <cstring>

namespace Aero {
class BindingEngine;
}

namespace Aero::Markup {

// ============================================================================
// XamlBinaryCodec (Inlined)
// ============================================================================


inline Base::Result<void> AppendU8(
    Base::Vector<std::uint8_t>& output,
    std::uint8_t value) noexcept {
    output.PushBack(value);
    return {};
}

inline Base::Result<void> AppendU16(
    Base::Vector<std::uint8_t>& output,
    std::uint16_t value) noexcept {
    output.PushBack(static_cast<std::uint8_t>(value));
    output.PushBack(static_cast<std::uint8_t>(value >> 8U));
    return {};
}

inline Base::Result<void> AppendU32(
    Base::Vector<std::uint8_t>& output,
    std::uint32_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
        output.PushBack(static_cast<std::uint8_t>(value >> shift));
    }
    return {};
}

inline Base::Result<void> AppendU64(
    Base::Vector<std::uint8_t>& output,
    std::uint64_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
        output.PushBack(static_cast<std::uint8_t>(value >> shift));
    }
    return {};
}

inline Base::Result<void> AppendVarU64(
    Base::Vector<std::uint8_t>& output,
    std::uint64_t value) noexcept {
    do {
        std::uint8_t byte = static_cast<std::uint8_t>(value & 0x7FU);
        value >>= 7U;
        if (value != 0U) {
            byte = static_cast<std::uint8_t>(byte | 0x80U);
        }
        output.PushBack(byte);
    } while (value != 0U);
    return {};
}

inline Base::Result<void> AppendVarU32(
    Base::Vector<std::uint8_t>& output,
    std::uint32_t value) noexcept {
    return AppendVarU64(output, value);
}

inline Base::Result<void> AppendString(
    Base::Vector<std::uint8_t>& output,
    Base::StringView value) noexcept {
    Base::Result<void> length = AppendU32(output, value.SizeBytes());
    if (!length) return length.GetStatus();
    for (std::uint32_t index = 0U; index < value.SizeBytes(); ++index) {
        output.PushBack(static_cast<std::uint8_t>(value[index]));
    }
    return {};
}

class Decoder {
public:
    explicit Decoder(
        Base::Span<const std::uint8_t> bytes,
        const char* truncatedMessage = "Binary payload is truncated") noexcept
        : bytes_(bytes), truncatedMessage_(truncatedMessage) {}

    Base::Result<std::uint8_t> ReadU8() noexcept {
        if (offset_ >= bytes_.Size()) return Truncated();
        return bytes_[offset_++];
    }

    Base::Result<std::uint16_t> ReadU16() noexcept {
        if (bytes_.Size() - offset_ < 2U) return Truncated();
        const std::uint16_t value = static_cast<std::uint16_t>(bytes_[offset_]) |
            static_cast<std::uint16_t>(bytes_[offset_ + 1U] << 8U);
        offset_ += 2U;
        return value;
    }

    Base::Result<std::uint32_t> ReadU32() noexcept {
        if (bytes_.Size() - offset_ < 4U) return Truncated();
        std::uint32_t value = 0U;
        for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
            value |= static_cast<std::uint32_t>(bytes_[offset_++]) << shift;
        }
        return value;
    }

    Base::Result<std::uint64_t> ReadU64() noexcept {
        if (bytes_.Size() - offset_ < 8U) return Truncated();
        std::uint64_t value = 0U;
        for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
            value |= static_cast<std::uint64_t>(bytes_[offset_++]) << shift;
        }
        return value;
    }

    Base::Result<std::uint64_t> ReadVarU64() noexcept {
        std::uint64_t value = 0U;
        for (std::uint32_t byteIndex = 0U; byteIndex < 10U; ++byteIndex) {
            Base::Result<std::uint8_t> byte = ReadU8();
            if (!byte) return byte.GetStatus();
            const std::uint8_t payload = static_cast<std::uint8_t>(byte.Value() & 0x7FU);
            if (byteIndex == 9U && payload > 1U) {
                return InvalidVarUInt();
            }
            value |= static_cast<std::uint64_t>(payload) << (byteIndex * 7U);
            if ((byte.Value() & 0x80U) == 0U) {
                return value;
            }
        }
        return InvalidVarUInt();
    }

    Base::Result<std::uint32_t> ReadVarU32() noexcept {
        Base::Result<std::uint64_t> value = ReadVarU64();
        if (!value) return value.GetStatus();
        if (value.Value() > UINT32_MAX) {
            return InvalidVarUInt();
        }
        return static_cast<std::uint32_t>(value.Value());
    }

    Base::Result<Base::String> ReadString(
        std::uint32_t& totalStringBytes,
        std::uint32_t maxStringBytes) noexcept {
        return ReadStringInternal(nullptr, totalStringBytes, maxStringBytes);
    }

    Base::Result<Base::String> ReadString(
        Base::IAllocator& allocator,
        std::uint32_t& totalStringBytes,
        std::uint32_t maxStringBytes) noexcept {
        return ReadStringInternal(&allocator, totalStringBytes, maxStringBytes);
    }

    bool AtEnd() const noexcept { return offset_ == bytes_.Size(); }
    std::uint32_t Offset() const noexcept { return offset_; }
    Base::Span<const std::uint8_t> RemainingBytes() const noexcept {
        return bytes_.Subspan(offset_, bytes_.Size() - offset_);
    }

private:
    Base::Result<Base::String> ReadStringInternal(
        Base::IAllocator* allocator,
        std::uint32_t& totalStringBytes,
        std::uint32_t maxStringBytes) noexcept {
        Base::Result<std::uint32_t> length = ReadU32();
        if (!length) return length.GetStatus();
        if (length.Value() > bytes_.Size() - offset_ ||
            length.Value() > maxStringBytes ||
            totalStringBytes > maxStringBytes - length.Value()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Binary string bounds are invalid");
        }
        Base::String value = allocator != nullptr ? Base::String(allocator) : Base::String();
        Base::Result<void> assigned = value.Assign(
            Base::StringView(
                reinterpret_cast<const char*>(bytes_.Data() + offset_),
                length.Value()));
        if (!assigned) return assigned.GetStatus();
        offset_ += length.Value();
        totalStringBytes += length.Value();
        return value;
    }

    Base::Status Truncated() const noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            truncatedMessage_);
    }

    static Base::Status InvalidVarUInt() noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Binary variable-length integer is invalid");
    }

    Base::Span<const std::uint8_t> bytes_;
    std::uint32_t offset_ = 0U;
    const char* truncatedMessage_ = "Binary payload is truncated";
};



// ============================================================================
// XamlEventConnection (Inlined)
// ============================================================================


class XamlEventConnection final : public Base::Object {
public:
    XamlEventConnection(
        Base::WeakRef<Base::Object> target,
        Meta::EventHandlerThunk thunk) noexcept
        : target_(std::move(target)),
          thunk_(thunk) {}

    void Invoke(
        Base::Object* sender,
        RoutedEventArgs& args) noexcept {
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

    void operator()(
        Base::Object* sender,
        RoutedEventArgs& args) const noexcept {
        if (connection) connection->Invoke(sender, args);
    }

    bool operator==(const XamlEventInvoker& other) const noexcept {
        return connection.Get() == other.connection.Get();
    }
};



// ============================================================================
// Template Blueprint & Compilation Contracts (Inlined)
// ============================================================================

struct DeferredContentEdge;
struct DeferredBindingEdge;



struct TemplatePrototypeProperty {
    Meta::DependencyPropertyHandle property;
    Meta::Value value;
    // A dependency-object property participating in a template Binding is
    // cloned as part of the instance graph rather than shared with the
    // authored prototype (for example SolidColorBrush.Color in a DataTemplate).
    std::uint32_t objectNode = UINT32_MAX;
};

struct TemplatePrototypeGradientStop {
    double offset = 0.0;
    ::Aero::Base::Color color{};
    // EnsureAuthoredName / x:Name for a TemplatedParent Binding on this stop.
    // Apply clones a new GradientStop into the brush and registers this name
    // so AttachMetadataBindings FindTarget can resolve the bound child.
    Base::String name;
};

struct TemplatePrototypeBinding {
    std::uint32_t target = UINT32_MAX;
    std::uint32_t source = UINT32_MAX;
    // resolved from the templated parent's document NameScope for each
    // ControlTemplate instance. Internal names continue to use source above.
    Base::String sourceName;
    // FindAncestor sources are instance-relative and cannot be captured from
    // the authored prototype graph. Resolve them after the cloned visual tree
    // has been connected.
    Base::String relativeAncestorType;
    std::uint32_t relativeAncestorLevel = 0U;
    ::Aero::Meta::Registry* metadata = nullptr;
    Meta::DependencyPropertyHandle targetProperty;
    Meta::DependencyPropertyHandle dataContextProperty;
    Base::String path;
    Base::String stringFormat;
    bool bindsToSource = false;
    Data::BindingMode mode =
        Data::BindingMode::Default;
    Meta::UpdateSourceTrigger updateSourceTrigger =
        Meta::UpdateSourceTrigger::PropertyChanged;
    Base::Ref<Data::IValueConverter> converter;
    Meta::PropertyValue converterParameter;
};

struct CompiledTriggerSetter {
    std::uint32_t target = 0U;
    Meta::DependencyPropertyHandle property;
    Meta::PropertyValue value;
};

struct CompiledTriggerPlan {
    Base::Ref<Aero::TriggerBase> trigger;
    Base::Vector<CompiledTriggerSetter> setters;
};

struct XamlBytecodeProgram;
class XamlBytecodeContainer;

struct CompiledTemplateDefinition {
    Meta::TypeId targetType = Meta::InvalidTypeId;
    Base::Ref<Base::Object> programOwner;
    XamlBytecodeProgram* program = nullptr;
    Base::Vector<Controls::TemplatePropertyTrigger>
        propertyTriggers;
    Base::Vector<Controls::TemplateBindingPlan>
        contentSourceBindings;
    Base::Vector<Controls::VisualStateGroupPlan>
        visualStateGroups;
};

Base::Result<CompiledTriggerPlan> CompileTriggerPlan(
    Base::Ref<Aero::TriggerBase> trigger,
    const XamlBytecodeProgram& program,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties) noexcept;

Base::Result<void> BuildCompiledTemplate(
    Controls::TemplateBuilder& context,
    void* factoryContext) noexcept;

Base::Result<Base::Ref<Base::Object>>
BuildCompiledDeferredTemplate(
    const Base::Ref<Base::Object>& payload,
    void* factoryContext,
    Aero::BindingEngine* bindings) noexcept;

Base::Result<void>
CompileDeferredTemplateToBytecode(
    const Base::Ref<Base::Object>& visualTree,
    const Aero::NameScope* names,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties,
    XamlBytecodeProgram& outProgram) noexcept;

Base::Result<CompiledTemplateDefinition>
CompileControlTemplateDefinition(
    Controls::ControlTemplate& controlTemplate,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties) noexcept;

Base::Result<Base::Vector<Controls::TemplatePropertyTrigger>>
CompilePropertyTriggers(
    Controls::ControlTemplate& controlTemplate,
    const XamlBytecodeProgram& program,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties) noexcept;

Base::Result<Base::Vector<Controls::VisualStateGroupPlan>>
CompileVisualStates(
    Controls::ControlTemplate& controlTemplate,
    const XamlBytecodeProgram& program,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties) noexcept;

// BasedOn inheritance for tree-less derived templates: compiles
// derived-authored property triggers / visual states against the sealed base
// program and stages them on the derived template. Seal() then inherits
// the base factory and prepends base plans.
Base::Result<void>
CompileInheritedControlTemplate(
    Controls::ControlTemplate& derivedTemplate,
    const XamlBytecodeProgram& baseProgram,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties) noexcept;

} // namespace Aero::Markup

// Markup implementation sources historically referred to the template
// compiler contracts through Markup. Keep that source-only bridge
// while the canonical declarations live in Aero::Base; no aliases are
// exposed by installed headers.
namespace Aero::Markup {
using ::Aero::Markup::CompiledTemplateDefinition;
using ::Aero::Markup::BuildCompiledTemplate;
using ::Aero::Markup::BuildCompiledDeferredTemplate;
using ::Aero::Markup::CompileDeferredTemplateToBytecode;
using ::Aero::Markup::CompileControlTemplateDefinition;
using ::Aero::Markup::CompileInheritedControlTemplate;



// ============================================================================
// XamlBytecode OpCodes (Instruction Set Architecture)
// ============================================================================
// Linear, deterministic, zero-branch instruction set for UI tree instantiation
// aligned with modern high-performance retained-mode GUI bytecode VMs.
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
// Self-contained, immutable binary program ready for fast VM execution.
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
    Base::Vector<TemplatePrototypeBinding> blueprintBindings;
    Base::Vector<CompiledTriggerPlan> dataTemplateTriggers;
    Base::Vector<CompiledTriggerPlan> controlTemplateDataTriggers;
    Base::Vector<Base::Ref<Media::Animation::EventTrigger>> controlTemplateEventTriggers;
    Base::Vector<Controls::TemplatePropertyTrigger> propertyTriggers;
    Base::Vector<Controls::TemplateBindingPlan> contentSourceBindings;
    Base::Vector<Controls::VisualStateGroupPlan> visualStateGroups;
    std::uint32_t contentPresenterSlot = UINT32_MAX;
    Base::Vector<Base::String> slotNames;
    Base::Vector<Meta::TypeId> slotTypes;
    Meta::TypeId ambientTargetType = Meta::InvalidTypeId;
    Meta::Registry* runtime = nullptr;
    Meta::DependencyPropertyRegistry* properties = nullptr;

    std::uint32_t FindSlotByName(Base::StringView name) const noexcept {
        for (std::uint32_t i = 0U; i < slotNames.Size(); ++i) {
            if (slotNames[i].View() == name) return i;
        }
        return UINT32_MAX;
    }

    Meta::TypeId GetSlotType(std::uint32_t slot) const noexcept {
        return (slot < slotTypes.Size()) ? slotTypes[slot] : Meta::InvalidTypeId;
    }

    bool IsValid() const noexcept {
        return slotCount > 0U && !instructions.Empty();
    }

    void Clear() noexcept {
        slotCount = 0U;
        instructions.Clear();
        typePool.Clear();
        propertyPool.Clear();
        stringPool.Clear();
        valuePool.Clear();
        gridLengths.Clear();
        gradientStops.Clear();
        objectRefs.Clear();
        subPrograms.Clear();
        blueprintBindings.Clear();
        dataTemplateTriggers.Clear();
        controlTemplateDataTriggers.Clear();
        controlTemplateEventTriggers.Clear();
        propertyTriggers.Clear();
        contentSourceBindings.Clear();
        visualStateGroups.Clear();
        contentPresenterSlot = UINT32_MAX;
        slotNames.Clear();
        slotTypes.Clear();
        runtime = nullptr;
        properties = nullptr;
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

    void EmitApplyMarkupExtension(
        std::uint16_t slot,
        Meta::DependencyPropertyHandle property,
        Meta::TypeId extensionTypeId,
        Base::StringView arguments) noexcept {
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
    Base::HashMap<Base::HashCode, std::uint32_t> stringHashIndex_;
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
    Schema* schema = nullptr;
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
};

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
    CommittedEffectPlan effects;
    bool hasDeferredStaticResources = false;
};

inline Base::Result<void> ApplyPropertyToObject(
    Base::Object& target,
    std::uint64_t propHandle,
    const Meta::Value& value,
    Meta::Registry* runtime,
    Controls::TemplateBuilder* templateBuilder) noexcept {
    if (auto* depObj = ::Aero::TryCast<DependencyObject>(&target)) {
        const Meta::DependencyPropertyHandle dpHandle{propHandle};
        if (DependencyObjectAccess::HasPropertyRegistry(*depObj)) {
            auto& dpRegistry = DependencyObjectAccess::PropertyRegistry(*depObj);
            if (dpRegistry.Find(dpHandle) != nullptr) {
                const bool isTemplateElement = templateBuilder != nullptr &&
                    (::Aero::TryCast<FrameworkElement>(depObj) != nullptr ||
                     ::Aero::TryCast<FrameworkContentElement>(depObj) != nullptr);
                if (isTemplateElement) {
                    depObj->SetTemplateValue(dpHandle, value);
                } else {
                    depObj->SetValue(dpHandle, value);
                }
                return {};
            }
        }
    }
    if (runtime != nullptr) {
        return runtime->SetProperty(target, propHandle, value);
    }
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Runtime registry missing in ApplyPropertyToObject");
}

inline Aero::ResourceDictionary* ResolveObjectResourceDictionary(Base::Object& obj) noexcept {
    if (auto* dict = ::Aero::TryCast<Aero::ResourceDictionary>(&obj)) {
        return dict;
    }
    if (auto* fe = ::Aero::TryCast<Aero::FrameworkElement>(&obj)) {
        return &fe->GetResources();
    }
    if (auto* fce = ::Aero::TryCast<Aero::FrameworkContentElement>(&obj)) {
        return &fce->GetResources();
    }
    if (auto* ft = ::Aero::TryCast<Aero::FrameworkTemplate>(&obj)) {
        return &ft->GetResources();
    }
    if (auto* style = ::Aero::TryCast<Aero::Style>(&obj)) {
        return &style->GetResources();
    }
    return nullptr;
}

inline Meta::TypeId ExtractImplicitResourceKeyType(const Meta::Value& itemVal) noexcept {
    if (itemVal.Kind() == Meta::ValueKind::Object && !itemVal.IsNullObject()) {
        if (const auto* obj = itemVal.AsObject().Get()) {
            if (const auto* style = ::Aero::TryCast<Aero::Style>(obj)) {
                return style->GetTargetType();
            }
            if (const auto* dt = ::Aero::TryCast<Aero::DataTemplate>(obj)) {
                return dt->GetDataType();
            }
        }
    }
    return Meta::InvalidTypeId;
}

inline void SynthesizeContentSourceBindings(
    XamlBytecodeProgram& subProg,
    Meta::TypeId controlTargetType,
    Meta::DependencyPropertyRegistry& properties,
    Meta::Registry& runtime) noexcept {
    if (controlTargetType == Meta::InvalidTypeId) return;

    const DependencyProperty* contentProperty =
        properties.Find(Controls::ContentPresenter::ContentProperty.Handle());
    const DependencyProperty* contentTemplateProperty =
        properties.Find(Controls::ContentPresenter::ContentTemplateProperty.Handle());
    if (contentProperty == nullptr) return;

    for (std::uint32_t slot = 0U; slot < subProg.slotTypes.Size(); ++slot) {
        const Meta::TypeId slotType = subProg.slotTypes[slot];
        if (slotType == Meta::InvalidTypeId ||
            !runtime.Types().IsDerivedFrom(slotType, Controls::ContentPresenter::StaticTypeId())) {
            continue;
        }

        Base::StringView nodeName = (slot < subProg.slotNames.Size()) ? subProg.slotNames[slot].View() : Base::StringView{};
        bool hasContentBinding = false;
        for (const auto& existing : subProg.contentSourceBindings) {
            if (existing.targetName.View() == nodeName && existing.targetProperty == contentProperty->Handle()) {
                hasContentBinding = true;
                break;
            }
        }
        if (hasContentBinding) continue;

        if (nodeName.Empty()) {
            char generatedName[48];
            const int written = std::snprintf(
                generatedName, sizeof(generatedName),
                "__ContentSource%u", slot);
            if (written > 0 && static_cast<std::size_t>(written) < sizeof(generatedName)) {
                if (slot >= subProg.slotNames.Size()) subProg.slotNames.Resize(slot + 1U);
                static_cast<void>(subProg.slotNames[slot].Assign(
                    Base::StringView(generatedName, static_cast<std::uint32_t>(written))));
                nodeName = subProg.slotNames[slot].View();
            }
        }

        Base::StringView contentSource("Content");
        const DependencyProperty* source = properties.Find(controlTargetType, contentSource);
        if (source != nullptr) {
            Controls::TemplateBindingPlan binding;
            static_cast<void>(binding.targetName.Assign(nodeName));
            binding.sourceProperty = source->Handle();
            binding.targetProperty = contentProperty->Handle();
            subProg.contentSourceBindings.PushBack(std::move(binding));

            if (contentTemplateProperty != nullptr) {
                const DependencyProperty* sourceTemplate = properties.Find(
                    controlTargetType, Base::StringView("ContentTemplate"));
                if (sourceTemplate != nullptr) {
                    Controls::TemplateBindingPlan tplBinding;
                    static_cast<void>(tplBinding.targetName.Assign(nodeName));
                    tplBinding.sourceProperty = sourceTemplate->Handle();
                    tplBinding.targetProperty = contentTemplateProperty->Handle();
                    subProg.contentSourceBindings.PushBack(std::move(tplBinding));
                }
            }
        }
    }
}

inline Base::Result<void> BindCompiledSubProgramToTemplate(
    Base::Object& target,
    const Base::Ref<Base::Object>& subProgramRef,
    Meta::Registry* runtime,
    Schema* /*schema*/) noexcept {
    auto* subContainer = ::Aero::TryCast<XamlBytecodeContainer>(subProgramRef.Get());
    if (subContainer == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "SubProgram is not a valid XamlBytecodeContainer");
    }
    XamlBytecodeProgram& subProg = subContainer->program;
    if (runtime != nullptr) {
        subProg.runtime = runtime;
    }
    if (auto* depObj = ::Aero::TryCast<DependencyObject>(&target)) {
        if (DependencyObjectAccess::HasPropertyRegistry(*depObj)) {
            subProg.properties = &DependencyObjectAccess::PropertyRegistry(*depObj);
        }
    }
    Meta::DependencyPropertyRegistry* properties = subProg.properties;

    if (auto* controlTemplate = ::Aero::TryCast<Controls::ControlTemplate>(&target)) {
        if (controlTemplate->GetTargetType() == Meta::InvalidTypeId) {
            Meta::TypeId tt = (subProg.ambientTargetType != Meta::InvalidTypeId)
                ? subProg.ambientTargetType
                : Controls::Control::StaticTypeId();
            static_cast<void>(::Aero::Controls::FrameworkTemplateState::SetTargetType(*controlTemplate, tt));
        }
        Base::Result<void> configured = ::Aero::Controls::FrameworkTemplateState::ConfigureFactory(
            *controlTemplate,
            &BuildCompiledTemplate,
            &subProg,
            subProgramRef);
        if (!configured) return configured.GetStatus();

        for (const auto& binding : subProg.contentSourceBindings) {
            configured = ::Aero::Controls::FrameworkTemplateState::AddTemplateBinding(
                *controlTemplate,
                binding.targetName.View(),
                binding.sourceProperty,
                binding.targetProperty);
            if (!configured) return configured.GetStatus();
        }
        for (const auto& group : subProg.visualStateGroups) {
            configured = ::Aero::Controls::FrameworkTemplateState::AddVisualStateGroup(
                *controlTemplate,
                group);
            if (!configured) return configured.GetStatus();
        }
        for (const auto& trigger : subProg.propertyTriggers) {
            configured = ::Aero::Controls::FrameworkTemplateState::AddPropertyTrigger(
                *controlTemplate,
                trigger);
            if (!configured) return configured.GetStatus();
        }
        if (runtime != nullptr && properties != nullptr) {
            if (subProg.propertyTriggers.Empty() &&
                !::Aero::Controls::FrameworkTemplateState::AuthoredTriggers(*controlTemplate).Empty()) {
                auto trigRes = CompilePropertyTriggers(*controlTemplate, subProg, *runtime, *properties);
                if (trigRes) {
                    for (auto& trigger : trigRes.Value()) {
                        configured = ::Aero::Controls::FrameworkTemplateState::AddPropertyTrigger(
                            *controlTemplate,
                            trigger);
                        if (!configured) return configured.GetStatus();
                    }
                }
            }
            if (subProg.visualStateGroups.Empty() &&
                !::Aero::Controls::FrameworkTemplateState::AuthoredVisualStateGroups(*controlTemplate).Empty()) {
                auto vsRes = CompileVisualStates(*controlTemplate, subProg, *runtime, *properties);
                if (vsRes) {
                    for (auto& group : vsRes.Value()) {
                        configured = ::Aero::Controls::FrameworkTemplateState::AddVisualStateGroup(
                            *controlTemplate,
                            group);
                        if (!configured) return configured.GetStatus();
                    }
                }
            }
        }
        if (properties != nullptr) {
            configured = ::Aero::Controls::FrameworkTemplateState::Seal(
                *controlTemplate,
                *properties);
            if (!configured) {
                return configured.GetStatus();
            }
        }
        return {};
    }

    if (auto* frameworkTemplate = ::Aero::TryCast<Aero::FrameworkTemplate>(&target)) {
        Base::Result<void> configured = ::Aero::Controls::FrameworkTemplateState::Configure(
            *frameworkTemplate,
            &BuildCompiledDeferredTemplate,
            &subProg,
            subProgramRef);
        if (!configured) return configured.GetStatus();

        configured = ::Aero::Controls::FrameworkTemplateState::Seal(*frameworkTemplate);
        if (!configured) return configured.GetStatus();
        return {};
    }

    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        "Target object is not a template in BindCompiledSubProgramToTemplate");
}

// ============================================================================
// XamlVM Executor
// ============================================================================
class XamlVM {
public:
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

struct PendingDeferredExtension {
    ProvidedValue value;
    Base::Ref<::Aero::DependencyObject> targetOwner;
    ::Aero::DependencyObject* target = nullptr;
    Meta::DependencyPropertyHandle property;

    PendingDeferredExtension() noexcept = default;
    explicit PendingDeferredExtension(const ProvidedValue& v) noexcept : value(v) {}
    ~PendingDeferredExtension() noexcept {
        if (value.cleanup != nullptr && value.rollbackContext != nullptr) {
            value.cleanup(value.rollbackContext);
            value.rollbackContext = nullptr;
        }
    }
    PendingDeferredExtension(PendingDeferredExtension&& other) noexcept
        : value(other.value),
          targetOwner(std::move(other.targetOwner)),
          target(other.target),
          property(other.property) {
        other.value.cleanup = nullptr;
        other.value.rollbackContext = nullptr;
        other.target = nullptr;
        other.property = {};
    }
    PendingDeferredExtension& operator=(PendingDeferredExtension&& other) noexcept {
        if (this != &other) {
            if (value.cleanup != nullptr && value.rollbackContext != nullptr) {
                value.cleanup(value.rollbackContext);
            }
            value = other.value;
            targetOwner = std::move(other.targetOwner);
            target = other.target;
            property = other.property;
            other.value.cleanup = nullptr;
            other.value.rollbackContext = nullptr;
            other.target = nullptr;
            other.property = {};
        }
        return *this;
    }
    PendingDeferredExtension(const PendingDeferredExtension&) = delete;
    PendingDeferredExtension& operator=(const PendingDeferredExtension&) = delete;
};

inline Base::Result<void> XamlVM::Execute(
    const XamlBytecodeProgram& program,
    const XamlVMContext& context,
    Base::Vector<Base::Ref<Base::Object>>& outObjects,
    Base::Vector<Media::Visual*>* outVisuals) noexcept {
    outObjects.Clear();
    if (program.slotCount == 0U || program.instructions.Empty()) {
        return {};
    }
    if (context.loadContext != nullptr && context.loadContext->maxObjects > 0U &&
        program.slotCount > context.loadContext->maxObjects) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "XAML object count exceeds configured limits");
    }
    outObjects.Resize(program.slotCount);

    Base::Vector<Meta::Value> outValues;
    outValues.Resize(program.slotCount);

    Base::Vector<const Aero::ResourceDictionary*> ambientDictStack;
    if (context.documentResources != nullptr) {
        ambientDictStack.PushBack(context.documentResources);
    }
    if (context.loadContext != nullptr && context.loadContext->resources != nullptr) {
        ambientDictStack.PushBack(context.loadContext->resources);
    }

    auto resolveResource = [&](Base::StringView key, const Aero::ResourceDictionary* localScope = nullptr) -> Base::Result<Aero::Value> {
        Base::StringView typeName = key;
        bool isTypeKey = false;
        if (key.SizeBytes() >= 2U && key[0] == '{' && key[key.SizeBytes() - 1U] == '}') {
            Base::StringView inner = TrimAscii(key.Substr(1U, key.SizeBytes() - 2U));
            if (inner.SizeBytes() >= 7U && inner.Substr(0U, 7U) == "x:Type ") {
                typeName = TrimAscii(inner.Substr(7U, inner.SizeBytes() - 7U));
                isTypeKey = true;
            } else if (inner.SizeBytes() >= 5U && inner.Substr(0U, 5U) == "Type ") {
                typeName = TrimAscii(inner.Substr(5U, inner.SizeBytes() - 5U));
                isTypeKey = true;
            }
        }
        Meta::TypeId resolvedTypeId = Meta::InvalidTypeId;
        if (isTypeKey && context.schema != nullptr) {
            Base::StringView localName = typeName;
            for (std::uint32_t i = 0U; i < typeName.SizeBytes(); ++i) {
                if (typeName[i] == ':') {
                    localName = TrimAscii(typeName.Substr(i + 1U, typeName.SizeBytes() - i - 1U));
                    break;
                }
            }
            Base::Result<const Meta::TypeInfo*> resolved = context.schema->ResolveType(Meta::AeroNamespaceUri(), localName);
            if (!resolved) {
                resolved = context.schema->ResolveType(Base::StringView("http://schemas.microsoft.com/winfx/2006/xaml/presentation"), localName);
            }
            if (resolved && resolved.Value() != nullptr) {
                resolvedTypeId = resolved.Value()->Id();
            }
        }

        auto lookupDict = [&](const Aero::ResourceDictionary* dict) -> Aero::Value {
            if (dict == nullptr) return Aero::Value{};
            if (resolvedTypeId != Meta::InvalidTypeId) {
                Aero::Value val = dict->Lookup(resolvedTypeId);
                if (val.Kind() != Meta::ValueKind::Unset) return val;
            }
            Aero::Value val = dict->Lookup(key);
            if (val.Kind() != Meta::ValueKind::Unset) return val;
            if (isTypeKey) {
                val = dict->Lookup(typeName);
                if (val.Kind() != Meta::ValueKind::Unset) return val;
            }
            return Aero::Value{};
        };

        if (localScope != nullptr) {
            Aero::Value val = lookupDict(localScope);
            if (val.Kind() != Meta::ValueKind::Unset) return val;
        }

        for (std::size_t i = ambientDictStack.Size(); i > 0U; --i) {
            const Aero::ResourceDictionary* dict = ambientDictStack[i - 1U];
            Aero::Value val = lookupDict(dict);
            if (val.Kind() != Meta::ValueKind::Unset) return val;
        }
        if (context.documentResources != nullptr) {
            Aero::Value val = lookupDict(context.documentResources);
            if (val.Kind() != Meta::ValueKind::Unset) return val;
        }
        if (context.loadContext != nullptr) {
            if (context.loadContext->resources != nullptr) {
                Aero::Value val = lookupDict(context.loadContext->resources);
                if (val.Kind() != Meta::ValueKind::Unset) return val;
            }
            if (context.loadContext->fallbackResources != nullptr) {
                Aero::Value val = lookupDict(context.loadContext->fallbackResources);
                if (val.Kind() != Meta::ValueKind::Unset) return val;
            }
        }
        if (context.existingRoot) {
            if (auto* fe = ::Aero::TryCast<FrameworkElement>(context.existingRoot.Get())) {
                Base::Result<Aero::ResourceValue> rv = fe->FindResource(key);
                if (rv && !rv.Value().IsUnset()) {
                    return rv.Value();
                }
            }
        }
        if (context.loadContext != nullptr && context.loadContext->existingRoot) {
            if (auto* fe = ::Aero::TryCast<FrameworkElement>(context.loadContext->existingRoot.Get())) {
                Base::Result<Aero::ResourceValue> rv = fe->FindResource(key);
                if (rv && !rv.Value().IsUnset()) {
                    return rv.Value();
                }
            }
        }
        return Base::Status::Failure(Base::ErrorCode::NotFound, "Resource not found");
    };

    struct DeferredStaticResourceWrite {
        std::uint16_t parentSlot = 0U;
        std::uint16_t childSlot = 0U;
        std::uint64_t memberId = Meta::InvalidMemberId;
        std::uint64_t valueType = Meta::InvalidTypeId;
        const Aero::ResourceDictionary* localScope = nullptr;
        bool isPropertyEdge = false;
        bool isContent = false;
    };
    Base::Vector<DeferredStaticResourceWrite> deferredWrites;
    Base::Vector<std::uint16_t> deferredSealSlots;

    auto resolveObjectIfStaticResource = [&](std::uint16_t slot, const Aero::ResourceDictionary* localScope = nullptr) -> Base::Result<bool> {
        if (slot >= outObjects.Size() || !outObjects[slot]) return true;
        if (auto* sro = ::Aero::TryCast<Markup::StaticResourceObject>(outObjects[slot].Get())) {
            Base::StringView rKey = sro->ResourceKey();
            if (!rKey.Empty()) {
                Base::Result<Aero::Value> res = resolveResource(rKey, localScope);
                if (res) {
                    if (res.Value().Kind() == Meta::ValueKind::Object) {
                        outObjects[slot] = res.Value().AsObject();
                    } else {
                        if (slot >= outValues.Size()) {
                            outValues.Resize(slot + 1U);
                        }
                        outValues[slot] = std::move(res).Value();
                        outObjects[slot].Reset();
                    }
                    return true;
                } else {
                    if (context.loadContext != nullptr && context.loadContext->deferUnresolvedStaticResources) {
                        if (context.hasDeferredStaticResources != nullptr) {
                            *context.hasDeferredStaticResources = true;
                        }
                        return false;
                    } else {
                        return res.GetStatus();
                    }
                }
            }
        }
        return true;
    };

    Base::Vector<PendingDeferredExtension> deferredExtensions;

    Base::Vector<Media::Visual*> localVisuals;
    Base::Vector<Media::Visual*>* visuals = outVisuals;
    if (visuals != nullptr) {
        visuals->Clear();
        visuals->Resize(program.slotCount);
    } else if (context.templateBuilder != nullptr) {
        localVisuals.Resize(program.slotCount);
        visuals = &localVisuals;
    }

    Meta::Registry* runtime = context.runtime;
    if (runtime == nullptr && context.schema != nullptr) {
        runtime = context.schema->Metadata();
    }

    auto setPropertyObject = [&](std::uint16_t slot, std::uint16_t srcSlot, std::uint64_t propHandle, std::uint64_t valueType) -> Base::Result<void> {
        if (slot >= outObjects.Size() || srcSlot >= outObjects.Size() || !outObjects[slot]) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in SetPropertyObject");
        }
        const std::uint64_t actualValueType = (outObjects[srcSlot] && (valueType == Markup::StaticResourceObject::StaticTypeId() || valueType == Meta::InvalidTypeId))
            ? outObjects[srcSlot]->RuntimeType() : valueType;
        Meta::Value val = outObjects[srcSlot]
            ? Meta::Value::FromObject(actualValueType, outObjects[srcSlot])
            : (srcSlot < outValues.Size() ? outValues[srcSlot] : Meta::Value{});
        static_cast<void>(ApplyPropertyToObject(
            *outObjects[slot], propHandle,
            val,
            runtime, context.templateBuilder));
        return {};
    };

    auto setPropertyContent = [&](std::uint16_t parentSlot, std::uint16_t childSlot, std::uint64_t memberId, std::uint64_t valueType, bool isPropertyEdge) -> Base::Result<void> {
        if (parentSlot >= outObjects.Size() || childSlot >= outObjects.Size() || !outObjects[parentSlot]) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in SetPropertyContent");
        }
        Base::Object* parent = outObjects[parentSlot].Get();
        Base::Ref<Base::Object> child = outObjects[childSlot];
        if (child) {
            const std::uint64_t actualValueType = (child && (valueType == Markup::StaticResourceObject::StaticTypeId() || valueType == Meta::InvalidTypeId))
                ? child->RuntimeType() : valueType;
            if (memberId != Meta::InvalidMemberId) {
                if (runtime == nullptr) {
                    return Base::Status::Failure(Base::ErrorCode::InvalidState, "Runtime registry missing in SetPropertyContent");
                }
                Base::Result<void> content = isPropertyEdge
                    ? ApplyPropertyToObject(
                        *parent,
                        memberId,
                        Meta::Value::FromObject(actualValueType, child),
                        runtime,
                        context.templateBuilder)
                    : runtime->WriteContent(
                        *parent,
                        memberId,
                        child);
                if (!content) {
                    return content.GetStatus();
                }
            } else {
                if (auto* panel = ::Aero::TryCast<Controls::Panel>(parent)) {
                    if (auto* uiChild = ::Aero::TryCast<UIElement>(child.Get())) {
                        panel->GetChildren().Add(Base::Ref<UIElement>::FromBorrowed(*uiChild));
                    }
                } else if (auto* contentCtrl = ::Aero::TryCast<Controls::ContentControl>(parent)) {
                    contentCtrl->SetContent(child);
                } else if (auto* decorator = ::Aero::TryCast<Controls::Decorator>(parent)) {
                    if (auto* uiChild = ::Aero::TryCast<UIElement>(child.Get())) {
                        decorator->SetOwnedChild(child, *uiChild);
                    }
                } else if (auto* setterTrigger = ::Aero::TryCast<SetterTriggerBase>(parent)) {
                    if (auto* setter = ::Aero::TryCast<Setter>(child.Get())) {
                        setterTrigger->AddAuthoredSetter(Base::Ref<Setter>::FromBorrowed(*setter));
                    }
                } else if (auto* style = ::Aero::TryCast<Style>(parent)) {
                    if (auto* setterBase = ::Aero::TryCast<SetterBase>(child.Get())) {
                        style->GetSetters().Add(Base::Ref<SetterBase>::FromBorrowed(*setterBase));
                    } else if (auto* triggerBase = ::Aero::TryCast<TriggerBase>(child.Get())) {
                        style->GetTriggers().Add(Base::Ref<TriggerBase>::FromBorrowed(*triggerBase));
                    }
                } else if (auto* ft = ::Aero::TryCast<FrameworkTemplate>(parent)) {
                    if (auto* triggerBase = ::Aero::TryCast<TriggerBase>(child.Get())) {
                        ::Aero::Controls::FrameworkTemplateState::AddAuthoredTrigger(*ft, Base::Ref<TriggerBase>::FromBorrowed(*triggerBase));
                    }
                } else if (runtime != nullptr) {
                    const Meta::MemberId contentMember = runtime->FindContentMember(parent->RuntimeType());
                    if (contentMember != Meta::InvalidMemberId) {
                        Base::Result<void> content = runtime->WriteContent(*parent, contentMember, child);
                        if (!content) {
                            return content.GetStatus();
                        }
                    }
                }
            }
        } else if (childSlot < outValues.Size() && !outValues[childSlot].IsUnset()) {
            if (memberId != Meta::InvalidMemberId) {
                if (runtime == nullptr) {
                    return Base::Status::Failure(Base::ErrorCode::InvalidState, "Runtime registry missing in SetPropertyContent");
                }
                Base::Result<void> content = isPropertyEdge
                    ? ApplyPropertyToObject(
                        *parent,
                        memberId,
                        outValues[childSlot],
                        runtime,
                        context.templateBuilder)
                    : (outValues[childSlot].Kind() == Meta::ValueKind::Object && !outValues[childSlot].IsNullObject()
                        ? runtime->WriteContent(*parent, memberId, outValues[childSlot].AsObject())
                        : Base::Result<void>{});
                if (!content) {
                    return content.GetStatus();
                }
            }
        }
        return {};
    };

    const XamlInstruction* ip = program.instructions.Data();
    const XamlInstruction* const end = ip + program.instructions.Size();

    for (; ip != end; ++ip) {
        switch (ip->op) {
        case XamlOpCode::Nop:
            break;

        case XamlOpCode::AdoptOrCreateRoot: {
            const std::uint16_t slot = ip->slot;
            if (slot >= outObjects.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in AdoptOrCreateRoot");
            }
            if (ip->typeOrPropIdx >= program.typePool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid type index in AdoptOrCreateRoot");
            }
            const Meta::TypeId typeId = program.typePool[ip->typeOrPropIdx];
            if (context.existingRoot && slot == 0U) {
                const Meta::TypeId actual = context.existingRoot->RuntimeType();
                if (actual != typeId && (runtime == nullptr || !runtime->Types().IsDerivedFrom(actual, typeId))) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidArgument,
                        "Existing root object type is incompatible with document root");
                }
                outObjects[0] = context.existingRoot;
            } else {
                if (runtime == nullptr) {
                    return Base::Status::Failure(Base::ErrorCode::InvalidState, "Runtime registry missing in AdoptOrCreateRoot");
                }
                Base::Result<Base::Ref<Base::Object>> created = runtime->CreateObject(typeId);
                if (!created) return created.GetStatus();
                outObjects[slot] = std::move(created).Value();
            }
            if (visuals != nullptr && slot < visuals->Size() && runtime != nullptr) {
                (*visuals)[slot] = runtime->Types().IsDerivedFrom(typeId, Media::Visual::StaticTypeId())
                    ? static_cast<Media::Visual*>(outObjects[slot].Get())
                    : nullptr;
            }
            if (slot == 0U && context.dataItemPayload && outObjects[0]) {
                if (auto* fe = ::Aero::TryCast<FrameworkElement>(outObjects[0].Get())) {
                    fe->SetDataContext(context.dataItemPayload);
                }
            }
            break;
        }

        case XamlOpCode::CreateObject: {
            const std::uint16_t slot = ip->slot;
            if (slot >= outObjects.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in CreateObject");
            }
            if (ip->typeOrPropIdx >= program.typePool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid type index in CreateObject");
            }
            const Meta::TypeId typeId = program.typePool[ip->typeOrPropIdx];
            if (runtime == nullptr) {
                return Base::Status::Failure(Base::ErrorCode::InvalidState, "Runtime registry missing in CreateObject");
            }
            Base::Result<Base::Ref<Base::Object>> created = runtime->CreateObject(typeId);
            if (!created) return created.GetStatus();
            Base::Ref<Base::Object> owner = std::move(created).Value();
            if (!owner || owner->RuntimeType() != typeId) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "CreateObject created an incompatible object");
            }
            if (context.templateBuilder != nullptr &&
                !runtime->Types().IsDerivedFrom(typeId, DependencyObject::StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled template created an incompatible non-DependencyObject");
            }
            if (visuals != nullptr && slot < visuals->Size()) {
                (*visuals)[slot] = runtime->Types().IsDerivedFrom(typeId, Media::Visual::StaticTypeId())
                    ? static_cast<Media::Visual*>(owner.Get())
                    : nullptr;
            }
            if (slot == 0U && context.dataItemPayload) {
                if (auto* fe = ::Aero::TryCast<FrameworkElement>(owner.Get())) {
                    fe->SetDataContext(context.dataItemPayload);
                }
            }
            outObjects[slot] = std::move(owner);
            break;
        }

        case XamlOpCode::SetPropertyValue: {
            const std::uint16_t slot = ip->slot;
            if (slot >= outObjects.Size() || !outObjects[slot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in SetPropertyValue");
            }
            if (ip->typeOrPropIdx >= program.propertyPool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid property index in SetPropertyValue");
            }
            if (ip->operandIdx >= program.valuePool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid valueIndex in SetPropertyValue");
            }
            const std::uint64_t propHandle = program.propertyPool[ip->typeOrPropIdx];
            static_cast<void>(ApplyPropertyToObject(
                *outObjects[slot], propHandle, program.valuePool[ip->operandIdx],
                runtime, context.templateBuilder));
            break;
        }

        case XamlOpCode::SetPropertyObject: {
            const std::uint16_t slot = ip->slot;
            const std::uint16_t srcSlot = ip->auxSlot;
            if (ip->typeOrPropIdx >= program.propertyPool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid property index in SetPropertyObject");
            }
            if (ip->extraIdx >= program.typePool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid valueType index in SetPropertyObject");
            }
            const std::uint64_t propHandle = program.propertyPool[ip->typeOrPropIdx];
            const std::uint64_t valueType = program.typePool[ip->extraIdx];

            Base::Result<bool> resStatus = resolveObjectIfStaticResource(srcSlot);
            if (!resStatus) return resStatus.GetStatus();
            if (!resStatus.Value()) {
                DeferredStaticResourceWrite dw;
                dw.isContent = false;
                dw.parentSlot = slot;
                dw.childSlot = srcSlot;
                dw.memberId = propHandle;
                dw.valueType = valueType;
                dw.localScope = !ambientDictStack.Empty() ? ambientDictStack.Back() : nullptr;
                deferredWrites.PushBack(std::move(dw));
                break;
            }
            if (auto status = setPropertyObject(slot, srcSlot, propHandle, valueType); !status) {
                return status;
            }
            break;
        }

        case XamlOpCode::SetPropertyContent: {
            const std::uint16_t parentSlot = ip->slot;
            const std::uint16_t childSlot = ip->auxSlot;
            if (ip->typeOrPropIdx >= program.propertyPool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid member index in SetPropertyContent");
            }
            if (ip->extraIdx >= program.typePool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid valueType index in SetPropertyContent");
            }
            const std::uint64_t memberId = program.propertyPool[ip->typeOrPropIdx];
            const std::uint64_t valueType = program.typePool[ip->extraIdx];
            const std::uint8_t isPropertyEdge = ip->flags;

            Base::Result<bool> resStatus = resolveObjectIfStaticResource(childSlot);
            if (!resStatus) return resStatus.GetStatus();
            if (!resStatus.Value()) {
                DeferredStaticResourceWrite dw;
                dw.isContent = true;
                dw.parentSlot = parentSlot;
                dw.childSlot = childSlot;
                dw.memberId = memberId;
                dw.valueType = valueType;
                dw.isPropertyEdge = (isPropertyEdge != 0U);
                dw.localScope = !ambientDictStack.Empty() ? ambientDictStack.Back() : nullptr;
                deferredWrites.PushBack(std::move(dw));
                break;
            }
            if (auto status = setPropertyContent(parentSlot, childSlot, memberId, valueType, isPropertyEdge != 0U); !status) {
                return status;
            }
            break;
        }

        case XamlOpCode::SetRootVisual: {
            const std::uint16_t slot = ip->slot;
            const std::uint32_t nameIdx = ip->operandIdx;
            if (slot >= outObjects.Size() || !outObjects[slot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in SetRootVisual");
            }
            if (context.templateBuilder != nullptr && visuals != nullptr && slot < visuals->Size() && (*visuals)[slot] != nullptr) {
                Base::StringView name = (nameIdx < program.stringPool.Size())
                    ? program.stringPool[nameIdx].View()
                    : Base::StringView{};
                Base::Result<void> added = context.templateBuilder->SetRoot(
                    name,
                    Base::Ref<Base::Object>::FromBorrowed(*outObjects[slot]),
                    *(*visuals)[slot]);
                if (!added) return added.GetStatus();
            }
            break;
        }

        case XamlOpCode::AddVisualChild: {
            const std::uint16_t parentSlot = ip->slot;
            const std::uint16_t childSlot = ip->auxSlot;
            const std::uint32_t nameIdx = ip->operandIdx;
            if (parentSlot >= outObjects.Size() || childSlot >= outObjects.Size() ||
                !outObjects[parentSlot] || !outObjects[childSlot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in AddVisualChild");
            }
            if (context.templateBuilder != nullptr && visuals != nullptr) {
                if (parentSlot >= visuals->Size() || (*visuals)[parentSlot] == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        "Visual template child has a non-Visual parent");
                }
                if (childSlot < visuals->Size() && (*visuals)[childSlot] != nullptr) {
                    Base::StringView name = (nameIdx < program.stringPool.Size())
                        ? program.stringPool[nameIdx].View()
                        : Base::StringView{};
                    Base::Result<void> added = context.templateBuilder->AddPart(
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
            const std::uint16_t slot = ip->slot;
            const std::uint32_t nameIdx = ip->operandIdx;
            if (slot >= outObjects.Size() || !outObjects[slot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in AddObjectPart");
            }
            if (context.templateBuilder != nullptr) {
                Base::StringView name = (nameIdx < program.stringPool.Size())
                    ? program.stringPool[nameIdx].View()
                    : Base::StringView{};
                if (!name.Empty()) {
                    if (auto* depObj = ::Aero::TryCast<DependencyObject>(outObjects[slot].Get())) {
                        Base::Result<void> named = context.templateBuilder->AddObjectPart(
                            name,
                            Base::Ref<Base::Object>::FromBorrowed(*outObjects[slot]),
                            *depObj);
                        if (!named) return named.GetStatus();
                    }
                }
            }
            break;
        }

        case XamlOpCode::RegisterDocumentName: {
            const std::uint16_t slot = ip->slot;
            const std::uint32_t nameIdx = ip->operandIdx;
            if (slot >= outObjects.Size() || !outObjects[slot] || nameIdx >= program.stringPool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot or nameIndex in RegisterDocumentName");
            }
            Base::StringView name = program.stringPool[nameIdx].View();
            if (!name.Empty()) {
                if (context.documentNames != nullptr) {
                    context.documentNames->Register(name, *outObjects[slot]);
                }
                if (!outObjects.Empty() && outObjects[0]) {
                    if (auto* rootFe = ::Aero::TryCast<FrameworkElement>(outObjects[0].Get())) {
                        rootFe->RegisterName(name, *outObjects[slot]);
                    }
                    ::Aero::Meta::ApplyBoundFields(*outObjects[0], name, outObjects[slot].Get());
                }
            }
            break;
        }

        case XamlOpCode::SetGridDefinitions: {
            const std::uint16_t slot = ip->slot;
            const std::uint32_t colStart = ip->operandIdx;
            const std::uint16_t colCount = ip->auxSlot;
            const std::uint32_t rowStart = ip->extraIdx;
            const std::uint16_t rowCount = ip->typeOrPropIdx;
            if (slot >= outObjects.Size() || !outObjects[slot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in SetGridDefinitions");
            }
            if (colStart + colCount > program.gridLengths.Size() ||
                rowStart + rowCount > program.gridLengths.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Grid length out of bounds in SetGridDefinitions");
            }
            if (outObjects[slot]->RuntimeType() == Controls::Grid::StaticTypeId()) {
                auto& grid = static_cast<Controls::Grid&>(*outObjects[slot]);
                if (colCount > 0U) {
                    grid.SetColumnDefinitions(Base::Span<const ::Aero::GridLength>(
                        &program.gridLengths[colStart], colCount));
                }
                if (rowCount > 0U) {
                    grid.SetRowDefinitions(Base::Span<const ::Aero::GridLength>(
                        &program.gridLengths[rowStart], rowCount));
                }
            }
            break;
        }

        case XamlOpCode::SetGradientStops: {
            const std::uint16_t slot = ip->slot;
            const std::uint32_t stopStart = ip->operandIdx;
            const std::uint16_t stopCount = ip->auxSlot;
            if (slot >= outObjects.Size() || !outObjects[slot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in SetGradientStops");
            }
            if (stopStart + stopCount > program.gradientStops.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Gradient stop index out of bounds in SetGradientStops");
            }
            if (runtime != nullptr && runtime->Types().IsDerivedFrom(outObjects[slot]->RuntimeType(), Media::GradientBrush::StaticTypeId())) {
                auto& gradient = static_cast<Media::GradientBrush&>(*outObjects[slot]);
                gradient.ClearGradientStops();
                for (std::uint16_t i = 0U; i < stopCount; ++i) {
                    const auto& stopRecord = program.gradientStops[stopStart + i];
                    auto stop = Base::MakeRef<Media::GradientStop>();
                    if (!stop) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM in SetGradientStops");
                    stop->SetOffset(stopRecord.offset);
                    stop->SetColor(stopRecord.color);
                    gradient.AddGradientStop(stop);
                    if (context.templateBuilder != nullptr && !stopRecord.name.Empty()) {
                        auto named = context.templateBuilder->AddObjectPart(
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
            const std::uint16_t presenterSlot = ip->slot;
            if (context.templateBuilder != nullptr && visuals != nullptr && presenterSlot < visuals->Size() && runtime != nullptr) {
                if (runtime->Types().IsDerivedFrom(
                        context.templateBuilder->TemplatedParent().RuntimeType(),
                        Controls::ContentControl::StaticTypeId())) {
                    Media::Visual* contentHost = (*visuals)[presenterSlot];
                    if (contentHost != nullptr) {
                        Base::Result<bool> projected = runtime->Types().IsDerivedFrom(
                            contentHost->RuntimeType(), Controls::ContentPresenter::StaticTypeId())
                            ? context.templateBuilder->ProjectContent(
                                static_cast<Controls::ContentControl&>(context.templateBuilder->TemplatedParent()),
                                static_cast<Controls::ContentPresenter&>(*contentHost))
                            : context.templateBuilder->ProjectContent(
                                static_cast<Controls::ContentControl&>(context.templateBuilder->TemplatedParent()),
                                static_cast<Controls::ContentControl&>(*contentHost));
                        if (!projected) return projected.GetStatus();
                    }
                }
            }
            break;
        }

        case XamlOpCode::PopulateContentPresenter: {
            const std::uint16_t slot = ip->slot;
            if (context.templateBuilder != nullptr && slot < outObjects.Size() && outObjects[slot] && runtime != nullptr) {
                if (runtime->Types().IsDerivedFrom(outObjects[slot]->RuntimeType(), Controls::ContentPresenter::StaticTypeId())) {
                    Base::Result<void> pop = context.templateBuilder->PopulateContentPresenter(
                        static_cast<Controls::ContentPresenter&>(*outObjects[slot]));
                    if (!pop) return pop.GetStatus();
                }
            }
            break;
        }

        case XamlOpCode::PopulateItemsPresenter: {
            const std::uint16_t slot = ip->slot;
            if (context.templateBuilder != nullptr && slot < outObjects.Size() && outObjects[slot] && runtime != nullptr) {
                if (runtime->Types().IsDerivedFrom(outObjects[slot]->RuntimeType(), Controls::ItemsPresenter::StaticTypeId())) {
                    const Controls::ItemsPanelTemplate* itemsPanel = context.itemsPanel;
                    if (itemsPanel == nullptr && runtime->Types().IsDerivedFrom(
                            context.templateBuilder->TemplatedParent().RuntimeType(), Controls::ItemsControl::StaticTypeId())) {
                        itemsPanel = static_cast<Controls::ItemsControl&>(
                            context.templateBuilder->TemplatedParent()).GetItemsPanel().Get();
                    }
                    Base::Result<void> pop = context.templateBuilder->PopulateItemsPresenter(
                        static_cast<Controls::ItemsPresenter&>(*outObjects[slot]), itemsPanel);
                    if (!pop) return pop.GetStatus();
                }
            }
            break;
        }

        case XamlOpCode::AttachBehaviorsAndTriggers: {
            const std::uint16_t slot = ip->slot;
            const std::uint32_t itemStart = ip->operandIdx;
            const std::uint16_t bCount = ip->auxSlot;
            const std::uint16_t tCount = ip->typeOrPropIdx;
            if (slot >= outObjects.Size() || !outObjects[slot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in AttachBehaviorsAndTriggers");
            }
            if (itemStart + bCount + tCount > program.objectRefs.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Object ref index out of bounds in AttachBehaviorsAndTriggers");
            }
            if (auto* framework = ::Aero::TryCast<FrameworkElement>(outObjects[slot].Get())) {
                for (std::uint16_t i = 0U; i < bCount; ++i) {
                    const auto& objRef = program.objectRefs[itemStart + i];
                    if (objRef) {
                        FrameworkElementSeams::AddAuthoredBehavior(*framework, objRef);
                        FrameworkElementSeams::AddStyleBehaviorPrototype(*framework, objRef);
                    }
                }
                for (std::uint16_t i = 0U; i < tCount; ++i) {
                    const auto& objRef = program.objectRefs[itemStart + bCount + i];
                    if (objRef) {
                        FrameworkElementSeams::AddAuthoredTrigger(*framework, objRef);
                        FrameworkElementSeams::AddStyleTriggerPrototype(*framework, objRef);
                    }
                }
            }
            break;
        }

        case XamlOpCode::ConnectEventHandler: {
            const std::uint16_t slot = ip->slot;
            if (slot >= outObjects.Size() || !outObjects[slot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in ConnectEventHandler");
            }
            if (ip->typeOrPropIdx >= program.propertyPool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid event member index in ConnectEventHandler");
            }
            if (ip->operandIdx >= program.stringPool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "String index out of bounds in ConnectEventHandler");
            }
            const std::uint64_t eventMemberId = program.propertyPool[ip->typeOrPropIdx];
            const std::uint32_t handlerNameIndex = ip->operandIdx;
            Base::StringView handlerName = program.stringPool[handlerNameIndex].View();
            if (!handlerName.Empty() && runtime != nullptr) {
                Base::Object* codeBehind = context.eventHandlerTarget != nullptr
                    ? context.eventHandlerTarget
                    : (!outObjects.Empty() ? outObjects[0].Get() : nullptr);
                if (codeBehind != nullptr) {
                    Meta::EventHandlerThunk thunk = runtime->Types().FindEventHandler(
                        codeBehind->RuntimeType(), handlerName);
                    if (thunk != nullptr) {
                        auto connection = Base::MakeRef<XamlEventConnection>(
                            Base::WeakRef<Base::Object>(Base::Ref<Base::Object>::FromBorrowed(*codeBehind)),
                            thunk);
                        if (!connection) {
                            return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM creating XamlEventConnection");
                        }
                        Base::Delegate<void(Base::Object*, RoutedEventArgs&)> handler(
                            XamlEventInvoker{std::move(connection)});
                        const RoutedEventHandle routedEvent{eventMemberId};
                        if (auto* uiElement = ::Aero::TryCast<UIElement>(outObjects[slot].Get())) {
                            uiElement->AddHandler(routedEvent, handler);
                        } else if (auto* contentElement = ::Aero::TryCast<ContentElement>(outObjects[slot].Get())) {
                            contentElement->AddHandler(routedEvent, handler);
                        }
                    } else if (context.loadContext != nullptr && context.loadContext->existingRoot) {
                        return Base::Status::Failure(
                            Base::ErrorCode::NotFound,
                            "XAML event handler is not registered on the x:Class type");
                    }
                }
            }
            break;
        }

        case XamlOpCode::ApplyMarkupExtension: {
            const std::uint16_t slot = ip->slot;
            if (slot >= outObjects.Size() || !outObjects[slot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in ApplyMarkupExtension");
            }
            if (ip->typeOrPropIdx >= program.propertyPool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid property index in ApplyMarkupExtension");
            }
            if (ip->extraIdx >= program.typePool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid extension type index in ApplyMarkupExtension");
            }
            if (ip->operandIdx >= program.stringPool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "String index out of bounds in ApplyMarkupExtension");
            }
            if (context.schema == nullptr) {
                return Base::Status::Failure(Base::ErrorCode::InvalidState, "Schema missing in ApplyMarkupExtension");
            }
            const std::uint64_t propHandle = program.propertyPool[ip->typeOrPropIdx];
            const std::uint64_t extTypeId = program.typePool[ip->extraIdx];
            const std::uint32_t argIdx = ip->operandIdx;
            Base::StringView arguments = program.stringPool[argIdx].View();
            ExtensionServices services;
            services.schema = context.schema;
            services.targetObject = outObjects[slot].Get();
            services.targetObjectType = outObjects[slot]->RuntimeType();
            services.targetMember = propHandle;
            services.rootObject = !outObjects.Empty() ? outObjects[0].Get() : nullptr;
            services.templatedParent = context.templateBuilder != nullptr
                ? &context.templateBuilder->TemplatedParent()
                : (context.loadContext != nullptr ? context.loadContext->templatedParent : nullptr);
            services.baseUri = context.loadContext != nullptr ? context.loadContext->baseUri : nullptr;
            services.nameScope = context.documentNames;
            services.effectiveValues = context.loadContext != nullptr ? context.loadContext->effectiveValues : nullptr;
            services.bindings = context.bindings != nullptr ? context.bindings : (context.loadContext != nullptr ? context.loadContext->bindings : nullptr);
            services.fallbackResources = context.documentResources != nullptr
                ? context.documentResources
                : (context.loadContext != nullptr ? context.loadContext->fallbackResources : nullptr);

            const ResourceDictionary* ambientDicts[2] = {
                context.documentResources,
                context.loadContext != nullptr ? context.loadContext->resources : nullptr
            };
            services.ambientResourceChain = Base::Span<const ResourceDictionary* const>(ambientDicts, 2);

            struct ResolverTrampoline {
                decltype(&resolveResource) fn;
            } trampoline{&resolveResource};

            services.resources = ResourceResolver(
                +[](void* ctx, Base::StringView key) noexcept -> Base::Result<Aero::Value> {
                    auto* t = static_cast<const ResolverTrampoline*>(ctx);
                    return (*t->fn)(key);
                },
                &trampoline
            );

            Base::Result<ProvidedValue> provided = context.schema->ProvideMarkupExtensionValue(
                extTypeId, arguments, services);
            if (!provided) {
                return provided.GetStatus();
            }
            if (provided.Value().kind == ProvidedValueKind::Value) {
                static_cast<void>(ApplyPropertyToObject(
                    *outObjects[slot], propHandle, provided.Value().value,
                    runtime, context.templateBuilder));
            } else if (provided.Value().kind == ProvidedValueKind::Deferred) {
                PendingDeferredExtension pending(provided.Value());
                if (slot < outObjects.Size() && outObjects[slot]) {
                    if (auto* depObj = ::Aero::TryCast<DependencyObject>(outObjects[slot].Get())) {
                        pending.targetOwner = Base::Ref<DependencyObject>::FromBorrowed(*depObj);
                        pending.target = depObj;
                    }
                }
                pending.property = Meta::DependencyPropertyHandle{propHandle};
                deferredExtensions.PushBack(std::move(pending));
            } else if (provided.Value().kind == ProvidedValueKind::Expression) {
                if (services.effectiveValues != nullptr && provided.Value().expression.IsValid()) {
                    if (auto* depObj = ::Aero::TryCast<DependencyObject>(outObjects[slot].Get())) {
                        static_cast<void>(services.effectiveValues->SetLocalExpression(
                            *depObj, Meta::DependencyPropertyHandle{propHandle}, provided.Value().expression));
                    }
                }
            }
            break;
        }

        case XamlOpCode::SetSlotValue: {
            const std::uint16_t slot = ip->slot;
            if (slot >= outValues.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in SetSlotValue");
            }
            if (ip->operandIdx >= program.valuePool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid valueIndex in SetSlotValue");
            }
            outValues[slot] = program.valuePool[ip->operandIdx];
            break;
        }

        case XamlOpCode::AddDictionaryEntry: {
            const std::uint16_t dictSlot = ip->slot;
            const std::uint16_t valSlot = ip->auxSlot;
            if (dictSlot >= outObjects.Size() || !outObjects[dictSlot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid dictSlot in AddDictionaryEntry");
            }
            if (auto status = resolveObjectIfStaticResource(valSlot); !status) {
                return status.GetStatus();
            }
            auto* dict = ResolveObjectResourceDictionary(*outObjects[dictSlot]);
            if (dict == nullptr) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Target object is not a ResourceDictionary");
            }
            Meta::Value itemVal;
            if (valSlot < outObjects.Size() && outObjects[valSlot]) {
                itemVal = Meta::Value::FromObject(outObjects[valSlot]->RuntimeType(), outObjects[valSlot]);
            } else if (valSlot < outValues.Size() && !outValues[valSlot].IsUnset()) {
                itemVal = outValues[valSlot];
            } else {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid valSlot in AddDictionaryEntry");
            }
            Base::StringView keyStr = (ip->operandIdx < program.stringPool.Size())
                ? program.stringPool[ip->operandIdx].View()
                : Base::StringView{};
            if (!keyStr.Empty()) {
                if (ip->flags == 1U) {
                    Base::Result<const Meta::TypeInfo*> resolved = context.schema != nullptr
                        ? context.schema->ResolveType(Meta::AeroNamespaceUri(), keyStr)
                        : Base::Status::Failure(Base::ErrorCode::NotFound, "No schema");
                    if (!resolved && context.schema != nullptr) {
                        resolved = context.schema->ResolveType(Base::StringView("http://schemas.microsoft.com/winfx/2006/xaml/presentation"), keyStr);
                    }
                    if (resolved && resolved.Value() != nullptr) {
                        dict->Set(resolved.Value()->Id(), itemVal);
                    } else {
                        dict->Set(keyStr, itemVal);
                    }
                } else {
                    dict->Set(keyStr, itemVal);
                }
            } else {
                Meta::TypeId implicitKey = ExtractImplicitResourceKeyType(itemVal);
                if (implicitKey != Meta::InvalidTypeId) {
                    dict->Set(implicitKey, itemVal);
                }
            }
            break;
        }

        case XamlOpCode::PushAmbientResources: {
            const std::uint16_t slot = ip->slot;
            if (slot < outObjects.Size() && outObjects[slot]) {
                if (auto* dict = ResolveObjectResourceDictionary(*outObjects[slot])) {
                    ambientDictStack.PushBack(dict);
                }
            }
            break;
        }

        case XamlOpCode::PopAmbientResources: {
            if (!ambientDictStack.Empty()) {
                ambientDictStack.PopBack();
            }
            break;
        }

        case XamlOpCode::BindTemplateProgram: {
            const std::uint16_t slot = ip->slot;
            const std::uint32_t subIdx = ip->operandIdx;
            if (slot >= outObjects.Size() || !outObjects[slot] || subIdx >= program.subPrograms.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot or subProgramIdx in BindTemplateProgram");
            }
            Base::Result<void> bound = BindCompiledSubProgramToTemplate(
                *outObjects[slot], program.subPrograms[subIdx], runtime, context.schema);
            if (!bound) {
                return bound.GetStatus();
            }
            break;
        }

        case XamlOpCode::SealOrInitializeObject: {
            const std::uint16_t slot = ip->slot;
            if (slot >= outObjects.Size() || !outObjects[slot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in SealOrInitializeObject");
            }
            bool hasPendingWrites = false;
            for (const auto& dw : deferredWrites) {
                if (dw.parentSlot == slot) {
                    hasPendingWrites = true;
                    break;
                }
            }
            if (hasPendingWrites) {
                deferredSealSlots.PushBack(slot);
                break;
            }
            if (auto* style = ::Aero::TryCast<Aero::Style>(outObjects[slot].Get())) {
                auto* propRegistry = Meta::CurrentObjectFactory().dependencyProperties;
                if (propRegistry != nullptr) {
                    static_cast<void>(Aero::SealStyle(*style, *propRegistry));
                }
            }
            break;
        }

        case XamlOpCode::ApplyObjectMarkupExtension: {
            const std::uint16_t slot = ip->slot;
            const std::uint16_t extSlot = ip->auxSlot;
            if (slot >= outObjects.Size() || !outObjects[slot] || extSlot >= outObjects.Size() || !outObjects[extSlot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in ApplyObjectMarkupExtension");
            }
            if (ip->typeOrPropIdx >= program.propertyPool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid property index in ApplyObjectMarkupExtension");
            }
            const std::uint64_t propHandle = program.propertyPool[ip->typeOrPropIdx];
            Base::Object* targetObj = outObjects[slot].Get();
            Base::Object* extObj = outObjects[extSlot].Get();

            if (auto* mb = ::Aero::TryCast<Data::MultiBinding>(extObj)) {
                ExtensionServices extServices;
                extServices.schema = context.schema;
                extServices.targetObject = targetObj;
                extServices.targetObjectType = targetObj->RuntimeType();
                extServices.targetMember = propHandle;
                extServices.rootObject = !outObjects.Empty() ? outObjects[0].Get() : nullptr;
                extServices.nameScope = context.documentNames;
                extServices.effectiveValues = context.loadContext != nullptr ? context.loadContext->effectiveValues : nullptr;
                extServices.bindings = context.bindings != nullptr ? context.bindings : (context.loadContext != nullptr ? context.loadContext->bindings : nullptr);
                Base::Result<ProvidedValue> prov = CreateMultiBindingValue(*mb, extServices);
                if (prov && prov.Value().kind == ProvidedValueKind::Deferred) {
                    PendingDeferredExtension pending(prov.Value());
                    if (slot < outObjects.Size() && outObjects[slot]) {
                        if (auto* depObj = ::Aero::TryCast<DependencyObject>(outObjects[slot].Get())) {
                            pending.targetOwner = Base::Ref<DependencyObject>::FromBorrowed(*depObj);
                            pending.target = depObj;
                        }
                    }
                    pending.property = Meta::DependencyPropertyHandle{propHandle};
                    deferredExtensions.PushBack(std::move(pending));
                }
            } else if (auto* b = ::Aero::TryCast<Data::Binding>(extObj)) {
                if (auto* depObj = ::Aero::TryCast<DependencyObject>(targetObj)) {
                    Data::BindingOperations::SetBinding(depObj, Meta::DependencyPropertyHandle{propHandle}, *b);
                }
            } else if (auto* ext = ::Aero::TryCast<MarkupExtension>(extObj)) {
                MarkupExtensionContext extCtx(
                    targetObj,
                    propHandle,
                    context.loadContext != nullptr ? context.loadContext->baseUri : nullptr);
                Base::Result<Base::Value> prov = ext->ProvideValue(extCtx);
                if (prov) {
                    static_cast<void>(ApplyPropertyToObject(
                        *targetObj, propHandle, prov.Value(), runtime, context.templateBuilder));
                }
            }
            break;
        }

        case XamlOpCode::End:
            goto finalize;

        default:
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Unknown XamlOpCode encountered during VM execution");
        }
    }
finalize:
    for (const auto& dw : deferredWrites) {
        Base::Result<bool> resStatus = resolveObjectIfStaticResource(dw.childSlot, dw.localScope);
        if (!resStatus) return resStatus.GetStatus();
        if (!resStatus.Value()) {
            return Base::Status::Failure(Base::ErrorCode::NotFound, "Deferred StaticResource key could not be resolved");
        }
        if (dw.isContent) {
            if (auto status = setPropertyContent(dw.parentSlot, dw.childSlot, dw.memberId, dw.valueType, dw.isPropertyEdge); !status) {
                return status;
            }
        } else {
            if (auto status = setPropertyObject(dw.parentSlot, dw.childSlot, dw.memberId, dw.valueType); !status) {
                return status;
            }
        }
    }
    for (std::uint16_t sealSlot : deferredSealSlots) {
        if (sealSlot < outObjects.Size() && outObjects[sealSlot]) {
            if (auto* style = ::Aero::TryCast<Aero::Style>(outObjects[sealSlot].Get())) {
                auto* propRegistry = Meta::CurrentObjectFactory().dependencyProperties;
                if (propRegistry != nullptr) {
                    static_cast<void>(Aero::SealStyle(*style, *propRegistry));
                }
            }
        }
    }
    for (PendingDeferredExtension& item : deferredExtensions) {
        EffectServices effectServices;
        effectServices.effectiveValues = context.loadContext != nullptr ? context.loadContext->effectiveValues : nullptr;
        effectServices.bindings = context.bindings != nullptr ? context.bindings : (context.loadContext != nullptr ? context.loadContext->bindings : nullptr);
        effectServices.fallbackResources = context.documentResources != nullptr ? context.documentResources : (context.loadContext != nullptr ? context.loadContext->fallbackResources : nullptr);

        const bool immediate = context.loadContext == nullptr || context.loadContext->effectCommitMode == EffectCommitMode::Immediate;
        const bool canBind = effectServices.bindings != nullptr;

        CommittedEffect effect;
        effect.lifetime = context.loadContext != nullptr ? context.loadContext->effectLifetime : Base::Ref<EffectLifetime>();
        effect.effectiveValues = effectServices.effectiveValues;
        effect.targetOwner = std::move(item.targetOwner);
        effect.target = item.target;
        effect.property = item.property;
        effect.context = item.value.rollbackContext;
        effect.prepare = item.value.prepare;
        effect.commit = item.value.commit;
        effect.rollback = item.value.rollback;
        effect.cleanup = item.value.cleanup;
        effect.bind = item.value.bind;

        if (immediate && canBind) {
            if (effect.bind != nullptr) {
                Base::Result<void> bound = effect.bind(effect.context, effectServices);
                if (!bound) return bound.GetStatus();
            }
            if (effect.prepare != nullptr && context.documentNames != nullptr) {
                Base::Result<void> prepared = effect.prepare(effect.context, *context.documentNames);
                if (!prepared) return prepared.GetStatus();
            }
            if (effect.commit != nullptr) {
                Base::Result<std::uint64_t> committed = effect.commit(effect.context);
                if (!committed) return committed.GetStatus();
                effect.token = committed.Value();
            }
            effect.committed = true;
            effect.bind = nullptr;
            effect.prepare = nullptr;
        }

        item.value.cleanup = nullptr;
        item.value.rollbackContext = nullptr;

        if (context.documentEffects != nullptr) {
            context.documentEffects->Items().PushBack(std::move(effect));
        }
    }
    return {};
}

inline Base::Result<void> XamlVM::Execute(
    const XamlBytecodeProgram& program,
    Meta::Registry& runtime,
    Controls::TemplateBuilder* context,
    Base::Vector<Base::Ref<Base::Object>>& outObjects,
    Base::Vector<Media::Visual*>* outVisuals) noexcept {
    XamlVMContext vmContext;
    vmContext.runtime = &runtime;
    vmContext.templateBuilder = context;
    return Execute(program, vmContext, outObjects, outVisuals);
}

inline Base::Result<void> XamlVM::ExecuteDocument(
    const XamlBytecodeProgram& program,
    const XamlDocumentExecutionContext& context,
    XamlDocumentExecutionResult& outResult) noexcept {
    XamlVMContext vmContext;
    vmContext.schema = context.schema;
    vmContext.loadContext = context.loadContext;
    vmContext.runtime = context.runtime != nullptr ? context.runtime : (context.schema != nullptr ? context.schema->Metadata() : nullptr);
    if (context.loadContext != nullptr && context.loadContext->existingRoot) {
        vmContext.existingRoot = context.loadContext->existingRoot;
        vmContext.eventHandlerTarget = context.loadContext->existingRoot.Get();
    }
    vmContext.documentNames = &outResult.names;
    vmContext.documentResources = &outResult.resources;
    vmContext.visualContent = &outResult.visualContent;
    vmContext.documentEffects = &outResult.effects;
    vmContext.hasDeferredStaticResources = &outResult.hasDeferredStaticResources;

    Base::Vector<Base::Ref<Base::Object>> outObjects;
    Base::Result<void> executed = Execute(program, vmContext, outObjects, nullptr);
    if (!executed) return executed.GetStatus();

    if (!outObjects.Empty() && outObjects[0]) {
        outResult.root = std::move(outObjects[0]);
        if (auto* dict = ::Aero::TryCast<Aero::ResourceDictionary>(outResult.root.Get())) {
            outResult.resources = dict->Share();
        }
    }
    return {};
}

inline Base::Result<void> CompileNodesToBytecode(
    Base::Span<const Node> nodes,
    std::size_t& nodeIndex,
    const Schema& schema,
    const Base::ResourceUri* baseUri,
    Meta::TypeId ambientTargetType,
    bool isSubProgram,
    XamlBytecodeProgram& outProgram) noexcept {
    outProgram.ambientTargetType = ambientTargetType;
    XamlBytecodeBuilder builder;

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
        Base::Ref<Base::Object> templateShell;
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

    Base::Vector<ObjectFrame> objectStack;
    Base::Vector<MemberFrame> memberStack;
    std::uint16_t nextSlot = 0U;

    for (; nodeIndex < nodes.Size(); ++nodeIndex) {
        const Node& node = nodes[static_cast<uint32_t>(nodeIndex)];
        switch (node.Kind()) {
        case NodeKind::NamespaceDeclaration:
            break;

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
                    if (!objectStack.Back().hasResourceScope) {
                        objectStack.Back().hasResourceScope = true;
                        builder.EmitPushAmbientResources(parentSlot);
                    }
                }
                if (localName.SizeBytes() >= 9U && localName.Substr(localName.SizeBytes() - 9U, 9U) == ".Triggers") {
                    mf.isTriggers = true;
                }
                if (node.HasCompiledMemberBinding()) {
                    const auto& mb = node.CompiledMember();
                    mf.memberId = mb.id;
                    mf.valueType = mb.valueType;
                    mf.kind = mb.kind;
                } else {
                    mf.memberId = node.CompiledMemberId();
                    if (mf.memberId == Meta::InvalidMemberId) {
                        Base::Result<ResolvedMember> resolved = schema.ResolveMember(
                            objectStack.Back().typeId, node.Name(), MemberSyntax::PropertyElement);
                        if (resolved) {
                            mf.memberId = resolved.Value().id;
                            mf.valueType = resolved.Value().valueType;
                            mf.kind = resolved.Value().kind;
                        }
                    }
                }
                memberStack.PushBack(mf);
                break;
            }

            if (!objectStack.Empty() && objectStack.Back().isTemplate &&
                (memberStack.Empty() || memberStack.Back().parentSlot != objectStack.Back().slot ||
                 (!memberStack.Back().isTriggers && !memberStack.Back().isResources))) {
                auto subContainer = Base::MakeRef<XamlBytecodeContainer>();
                Base::Result<void> subRes = CompileNodesToBytecode(
                    nodes, nodeIndex, schema, baseUri, objectStack.Back().templateTargetType, true, subContainer->program);
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
            if (isRoot && !isSubProgram) {
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
            if (isSubProgram) {
                for (std::size_t scanIdx = nodeIndex + 1U; scanIdx < nodes.Size(); ++scanIdx) {
                    const auto& scanNode = nodes[static_cast<std::uint32_t>(scanIdx)];
                    if (scanNode.Kind() == NodeKind::EndObject || scanNode.Kind() == NodeKind::StartObject) {
                        break;
                    }
                    if (scanNode.Kind() == NodeKind::StartMember && scanNode.Name().LocalName() == "Name") {
                        if (scanIdx + 1U < nodes.Size() && nodes[static_cast<std::uint32_t>(scanIdx + 1U)].Kind() == NodeKind::Value) {
                            partName = TrimAscii(nodes[static_cast<std::uint32_t>(scanIdx + 1U)].Value());
                        }
                        break;
                    }
                }
            }

            if (isRoot) {
                if (isSubProgram) {
                    builder.EmitCreateObject(slot, typeId);
                    builder.EmitSetRootVisual(slot, partName);
                } else {
                    builder.EmitAdoptOrCreateRoot(slot, typeId);
                }
            } else {
                builder.EmitCreateObject(slot, typeId);
                if (isSubProgram) {
                    if (schema.Types().IsDerivedFrom(typeId, Media::Visual::StaticTypeId())) {
                        builder.EmitAddVisualChild(parentSlot, slot, partName);
                    } else {
                        builder.EmitAddObjectPart(slot, partName);
                    }
                }
            }

            if (isDictionary) {
                builder.EmitPushAmbientResources(slot);
            }

            if (typeId == Controls::ContentPresenter::StaticTypeId() ||
                typeId == Controls::ScrollContentPresenter::StaticTypeId()) {
                builder.EmitProjectContent(slot);
                builder.EmitPopulateContentPresenter(slot);
            } else if (typeId == Controls::ItemsPresenter::StaticTypeId()) {
                builder.EmitPopulateItemsPresenter(slot);
            }

            ObjectFrame frame;
            frame.slot = slot;
            frame.typeId = typeId;
            frame.isRoot = isRoot;
            frame.isTemplate = isTemplate;
            frame.templateTargetType = (ambientTargetType != Meta::InvalidTypeId)
                ? ambientTargetType
                : (isControlTemplate ? Controls::Control::StaticTypeId() : Meta::InvalidTypeId);
            frame.isDictionary = isDictionary;
            frame.hasResourceScope = isDictionary;

            if (isControlTemplate) {
                frame.templateShell = Base::MakeRef<Controls::ControlTemplate>();
            } else if (isDataTemplate) {
                frame.templateShell = Base::MakeRef<Aero::DataTemplate>();
            }

            if (slot >= outProgram.slotTypes.Size()) {
                outProgram.slotTypes.Resize(slot + 1U);
            }
            outProgram.slotTypes[slot] = typeId;
            if (slot >= outProgram.slotNames.Size()) {
                outProgram.slotNames.Resize(slot + 1U);
            }
            static_cast<void>(outProgram.slotNames[slot].Assign(node.Name().LocalName()));

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
            mf.objectStackDepth = objectStack.Size();

            if (memName == "Key" &&
                (node.Name().NamespaceUri().Empty() ||
                 node.Name().NamespaceUri() == "http://schemas.microsoft.com/winfx/2006/xaml" ||
                 node.Name().NamespaceUri() == "http://schemas.microsoft.com/winfx/2006/xaml/presentation")) {
                mf.isKey = true;
            } else if (memName == "Name" &&
                (node.Name().NamespaceUri().Empty() ||
                 node.Name().NamespaceUri() == "http://schemas.microsoft.com/winfx/2006/xaml" ||
                 node.Name().NamespaceUri() == "http://schemas.microsoft.com/winfx/2006/xaml/presentation")) {
                mf.isName = true;
            } else if (memName == "Resources" || (memName.SizeBytes() >= 10U && memName.Substr(memName.SizeBytes() - 10U, 10U) == ".Resources")) {
                mf.isResources = true;
                if (!objectStack.Back().hasResourceScope) {
                    objectStack.Back().hasResourceScope = true;
                    builder.EmitPushAmbientResources(parentSlot);
                }
            } else if (memName == "Triggers" || (memName.SizeBytes() >= 9U && memName.Substr(memName.SizeBytes() - 9U, 9U) == ".Triggers")) {
                mf.isTriggers = true;
            } else if (memName == "TargetType") {
                mf.isTargetType = true;
                mf.memberId = node.CompiledMemberId();
                if (mf.memberId == Meta::InvalidMemberId && !objectStack.Empty()) {
                    Base::Result<ResolvedMember> resolved = schema.ResolveMember(
                        objectStack.Back().typeId, node.Name(), MemberSyntax::Attribute);
                    if (resolved) {
                        mf.memberId = resolved.Value().id;
                        mf.valueType = resolved.Value().valueType;
                        mf.kind = resolved.Value().kind;
                    }
                }
            } else if (node.HasCompiledMemberBinding()) {
                const auto& mb = node.CompiledMember();
                mf.memberId = mb.id;
                mf.valueType = mb.valueType;
                mf.kind = mb.kind;
                mf.isEvent = (mb.kind == Meta::MemberKind::Event);
            } else {
                mf.memberId = node.CompiledMemberId();
                if (mf.memberId == Meta::InvalidMemberId) {
                    Base::Result<ResolvedMember> resolved = schema.ResolveMember(
                        objectStack.Back().typeId, node.Name(), MemberSyntax::Attribute);
                    if (resolved) {
                        mf.memberId = resolved.Value().id;
                        mf.valueType = resolved.Value().valueType;
                        mf.kind = resolved.Value().kind;
                        mf.isEvent = (resolved.Value().kind == Meta::MemberKind::Event);
                    }
                }
            }
            memberStack.PushBack(mf);
            break;
        }

        case NodeKind::Value: {
            if (objectStack.Empty()) break;
            const bool hasMemberForCurrent = !memberStack.Empty() && memberStack.Back().parentSlot == objectStack.Back().slot;
            if (hasMemberForCurrent && memberStack.Back().isIgnored) break;

            if (!hasMemberForCurrent) {
                if (!node.IsFromAttribute() && !TrimAscii(node.Value()).Empty()) {
                    const ObjectFrame& currentObj = objectStack.Back();
                    if (currentObj.isValueNode) {
                        Base::Result<Meta::Value> converted = schema.ConvertText(
                            currentObj.typeId, node.Value(), nullptr);
                        if (converted) {
                            builder.EmitSetSlotValue(currentObj.slot, converted.Value());
                            break;
                        }
                    }
                    Base::Result<ResolvedMember> contentMember = schema.ResolveContentMember(currentObj.typeId);
                    if (contentMember) {
                        const ResolvedMember& mem = contentMember.Value();
                        Meta::TypeId targetType = (mem.valueType != Meta::InvalidTypeId) ? mem.valueType : Meta::TypeOf<Base::String>();
                        Base::Result<Meta::Value> converted = schema.ConvertText(targetType, node.Value(), nullptr);
                        if (converted) {
                            builder.EmitSetPropertyValue(currentObj.slot, Meta::DependencyPropertyHandle{mem.id}, converted.Value());
                            break;
                        }
                    }
                    const Meta::PropertyInfo* textProp = schema.Types().FindProperty(currentObj.typeId, Base::StringView("Text"), true);
                    if (textProp != nullptr) {
                        Meta::TypeId targetType = (textProp->ValueType() != Meta::InvalidTypeId) ? textProp->ValueType() : Meta::TypeOf<Base::String>();
                        Base::Result<Meta::Value> converted = schema.ConvertText(targetType, node.Value(), nullptr);
                        if (converted) {
                            builder.EmitSetPropertyValue(currentObj.slot, Meta::DependencyPropertyHandle{textProp->Id()}, converted.Value());
                            break;
                        }
                    }
                    const Meta::PropertyInfo* contentProp = schema.Types().FindProperty(currentObj.typeId, Base::StringView("Content"), true);
                    if (contentProp != nullptr) {
                        Meta::TypeId targetType = (contentProp->ValueType() != Meta::InvalidTypeId) ? contentProp->ValueType() : Meta::TypeOf<Base::String>();
                        Base::Result<Meta::Value> converted = schema.ConvertText(targetType, node.Value(), nullptr);
                        if (converted) {
                            builder.EmitSetPropertyValue(currentObj.slot, Meta::DependencyPropertyHandle{contentProp->Id()}, converted.Value());
                            break;
                        }
                    }
                }
                break;
            }

            MemberFrame& mf = memberStack.Back();
            if (mf.isKey) {
                static_cast<void>(objectStack.Back().key.Assign(node.Value()));
                Base::StringView str = node.Value();
                if (str.SizeBytes() >= 8U && str.Substr(0U, 8U) == "{x:Type ") {
                    objectStack.Back().isTypeKey = true;
                    Base::StringView typeName = TrimAscii(str.Substr(8U, str.SizeBytes() - 9U));
                    static_cast<void>(objectStack.Back().key.Assign(typeName));
                }
                break;
            }
            if (mf.isName) {
                builder.EmitRegisterDocumentName(mf.parentSlot, node.Value());
                if (mf.parentSlot < outProgram.slotNames.Size()) {
                    static_cast<void>(outProgram.slotNames[mf.parentSlot].Assign(node.Value()));
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
                break;
            }

            Base::StringView strVal = node.Value();
            if (strVal.SizeBytes() >= 2U && strVal[0] == '{' && strVal[1] == '}') {
                strVal = strVal.Substr(2U, strVal.SizeBytes() - 2U);
            }
            if (TrimAscii(strVal).Empty()) break;

            if (strVal.SizeBytes() > 0U && strVal[0] == '{') {
                Base::StringView trimmed = TrimAscii(strVal);
                if (trimmed == "{x:Null}" || trimmed == "{Null}") {
                    Meta::TypeId targetType = (mf.valueType != Meta::InvalidTypeId) ? mf.valueType : Meta::TypeOf<Base::String>();
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
                        char extWithSuffix[64];
                        const int extLen = std::snprintf(extWithSuffix, sizeof(extWithSuffix), "%.*sExtension",
                            static_cast<int>(extName.SizeBytes()), extName.Data());
                        const Base::StringView extWithSuffixView = (extLen > 0 && extLen < static_cast<int>(sizeof(extWithSuffix)))
                            ? Base::StringView(extWithSuffix, static_cast<std::uint32_t>(extLen))
                            : Base::StringView{};
                        Base::Result<const Meta::TypeInfo*> resolvedExt = schema.ResolveType(
                            Meta::AeroNamespaceUri(), extWithSuffixView);
                        if (!resolvedExt) {
                            resolvedExt = schema.ResolveType(
                                Base::StringView("http://schemas.microsoft.com/winfx/2006/xaml"), extWithSuffixView);
                        }
                        if (!resolvedExt) {
                            resolvedExt = schema.ResolveType(
                                Meta::AeroNamespaceUri(), extName);
                        }
                        if (!resolvedExt) {
                            resolvedExt = schema.ResolveType(
                                Base::StringView("http://schemas.microsoft.com/winfx/2006/xaml"), extName);
                        }
                        if (resolvedExt && resolvedExt.Value() != nullptr) {
                            extTypeId = resolvedExt.Value()->Id();
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

            const Meta::TypeId targetType = (mf.valueType != Meta::InvalidTypeId)
                ? mf.valueType
                : Meta::TypeOf<Base::String>();

            if (mf.isTargetType || targetType == Meta::TypeOf<Meta::TypeReference>()) {
                Base::StringView typeName = TrimAscii(strVal);
                if (typeName.SizeBytes() >= 2U && typeName[0] == '{' && typeName[typeName.SizeBytes() - 1U] == '}') {
                    typeName = TrimAscii(typeName.Substr(1U, typeName.SizeBytes() - 2U));
                    if (typeName.SizeBytes() >= 7U && typeName.Substr(0U, 7U) == "x:Type ") {
                        typeName = TrimAscii(typeName.Substr(7U, typeName.SizeBytes() - 7U));
                    } else if (typeName.SizeBytes() >= 5U && typeName.Substr(0U, 5U) == "Type ") {
                        typeName = TrimAscii(typeName.Substr(5U, typeName.SizeBytes() - 5U));
                    }
                }
                Base::StringView localName = typeName;
                for (std::uint32_t i = 0U; i < typeName.SizeBytes(); ++i) {
                    if (typeName[i] == ':') {
                        localName = TrimAscii(typeName.Substr(i + 1U, typeName.SizeBytes() - i - 1U));
                        break;
                    }
                }
                Base::Result<const Meta::TypeInfo*> resolved = schema.ResolveType(
                    Meta::AeroNamespaceUri(), localName);
                if (!resolved) {
                    resolved = schema.ResolveType(Base::StringView("http://schemas.microsoft.com/winfx/2006/xaml/presentation"), localName);
                }
                if (!resolved) {
                    resolved = schema.ResolveType(Meta::AeroNamespaceUri(), typeName);
                }
                if (resolved && resolved.Value() != nullptr) {
                    Meta::TypeReference ref{resolved.Value()->Id()};
                    builder.EmitSetPropertyValue(
                        mf.parentSlot,
                        Meta::DependencyPropertyHandle{mf.memberId},
                        Meta::ValueCodec<Meta::TypeReference>::Encode(ref).Value());
                    if (objectStack.Back().isTemplate) {
                        objectStack.Back().templateTargetType = resolved.Value()->Id();
                    }
                    break;
                }
            }

            Base::Result<Meta::Value> converted = schema.ConvertText(
                targetType, strVal, nullptr);
            if (converted) {
                builder.EmitSetPropertyValue(
                    mf.parentSlot,
                    Meta::DependencyPropertyHandle{mf.memberId},
                    converted.Value());
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

            if (finished.isTemplate) {
                if (finished.subProgramIdx != UINT32_MAX) {
                    if (finished.templateShell != nullptr && schema.Metadata() != nullptr) {
                        auto subProgramObj = builder.GetSubProgram(finished.subProgramIdx);
                        if (auto* subContainer = ::Aero::TryCast<XamlBytecodeContainer>(subProgramObj.Get())) {
                            XamlBytecodeProgram& subProg = subContainer->program;
                            if (auto* ct = ::Aero::TryCast<Controls::ControlTemplate>(finished.templateShell.Get())) {
                                auto* propRegistry = Meta::CurrentObjectFactory().dependencyProperties;
                                if (propRegistry != nullptr) {
                                    subProg.runtime = schema.Metadata();
                                    subProg.properties = propRegistry;
                                    if (finished.typeId == Controls::ControlTemplate::StaticTypeId() &&
                                        finished.templateTargetType != Meta::InvalidTypeId) {
                                        SynthesizeContentSourceBindings(
                                            subProg, finished.templateTargetType, *propRegistry, *schema.Metadata());
                                    }
                                    auto trigRes = CompilePropertyTriggers(*ct, subProg, *schema.Metadata(), *propRegistry);
                                    if (trigRes) subProg.propertyTriggers = std::move(trigRes).Value();
                                    auto vsRes = CompileVisualStates(*ct, subProg, *schema.Metadata(), *propRegistry);
                                    if (vsRes) subProg.visualStateGroups = std::move(vsRes).Value();
                                }
                            }
                        }
                    }
                    builder.EmitBindTemplateProgram(finished.slot, finished.subProgramIdx);
                }
            }

            if (finished.typeId == Aero::Style::StaticTypeId()) {
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

            if (isSubProgram && objectStack.Empty()) {
                builder.SetSlotCount(nextSlot);
                builder.EmitEnd();
                builder.Finish(outProgram);
                return {};
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

inline Base::Result<void> CompileDocumentToBytecode(
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

inline Base::Result<const XamlBytecodeProgram*> EnsureDocumentBytecode(
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

} // namespace Aero::Markup

