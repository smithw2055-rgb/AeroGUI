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
#include "gui/markup/XamlSchema.hpp"
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Grid.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/ContentElement.hpp>

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
    std::uint32_t contentPresenterSlot = UINT32_MAX;
    Base::Vector<Base::String> slotNames;
    Base::Vector<Meta::TypeId> slotTypes;
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
    Base::HashMap<Base::HashCode, std::uint32_t> stringHashIndex_;
};

class XamlBytecodeContainer : public Base::Object {
public:
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
};

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
    PendingDeferredExtension() noexcept = default;
    explicit PendingDeferredExtension(const ProvidedValue& v) noexcept : value(v) {}
    ~PendingDeferredExtension() noexcept {
        if (value.cleanup != nullptr && value.rollbackContext != nullptr) {
            value.cleanup(value.rollbackContext);
            value.rollbackContext = nullptr;
        }
    }
    PendingDeferredExtension(PendingDeferredExtension&& other) noexcept : value(other.value) {
        other.value.cleanup = nullptr;
        other.value.rollbackContext = nullptr;
    }
    PendingDeferredExtension& operator=(PendingDeferredExtension&& other) noexcept {
        if (this != &other) {
            if (value.cleanup != nullptr && value.rollbackContext != nullptr) {
                value.cleanup(value.rollbackContext);
            }
            value = other.value;
            other.value.cleanup = nullptr;
            other.value.rollbackContext = nullptr;
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
            if (auto* depObj = ::Aero::TryCast<DependencyObject>(outObjects[slot].Get())) {
                if (context.templateBuilder != nullptr) {
                    depObj->SetTemplateValue(Meta::DependencyPropertyHandle{propHandle}, program.valuePool[ip->operandIdx]);
                } else {
                    depObj->SetValue(Meta::DependencyPropertyHandle{propHandle}, program.valuePool[ip->operandIdx]);
                }
            } else if (runtime != nullptr) {
                static_cast<void>(runtime->SetProperty(*outObjects[slot], propHandle, program.valuePool[ip->operandIdx]));
            }
            break;
        }

        case XamlOpCode::SetPropertyObject: {
            const std::uint16_t slot = ip->slot;
            const std::uint16_t srcSlot = ip->auxSlot;
            if (slot >= outObjects.Size() || srcSlot >= outObjects.Size() ||
                !outObjects[slot] || !outObjects[srcSlot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in SetPropertyObject");
            }
            if (ip->typeOrPropIdx >= program.propertyPool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid property index in SetPropertyObject");
            }
            if (ip->extraIdx >= program.typePool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid valueType index in SetPropertyObject");
            }
            const std::uint64_t propHandle = program.propertyPool[ip->typeOrPropIdx];
            const std::uint64_t valueType = program.typePool[ip->extraIdx];
            if (auto* depObj = ::Aero::TryCast<DependencyObject>(outObjects[slot].Get())) {
                if (context.templateBuilder != nullptr) {
                    depObj->SetTemplateValue(
                        Meta::DependencyPropertyHandle{propHandle},
                        Meta::Value::FromObject(valueType, outObjects[srcSlot]));
                } else {
                    depObj->SetValue(
                        Meta::DependencyPropertyHandle{propHandle},
                        Meta::Value::FromObject(valueType, outObjects[srcSlot]));
                }
            } else if (runtime != nullptr) {
                static_cast<void>(runtime->SetProperty(
                    *outObjects[slot], propHandle, Meta::Value::FromObject(valueType, outObjects[srcSlot])));
            }
            break;
        }

        case XamlOpCode::SetPropertyContent: {
            const std::uint16_t parentSlot = ip->slot;
            const std::uint16_t childSlot = ip->auxSlot;
            if (parentSlot >= outObjects.Size() || childSlot >= outObjects.Size() ||
                !outObjects[parentSlot] || !outObjects[childSlot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in SetPropertyContent");
            }
            if (ip->typeOrPropIdx >= program.propertyPool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid member index in SetPropertyContent");
            }
            if (ip->extraIdx >= program.typePool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid valueType index in SetPropertyContent");
            }
            const std::uint64_t memberId = program.propertyPool[ip->typeOrPropIdx];
            const std::uint64_t valueType = program.typePool[ip->extraIdx];
            const std::uint8_t isPropertyEdge = ip->flags;
            Base::Object* parent = outObjects[parentSlot].Get();
            Base::Ref<Base::Object> child = outObjects[childSlot];
            if (memberId != Meta::InvalidMemberId) {
                if (runtime == nullptr) {
                    return Base::Status::Failure(Base::ErrorCode::InvalidState, "Runtime registry missing in SetPropertyContent");
                }
                Base::Result<void> content = (isPropertyEdge != 0U)
                    ? runtime->SetProperty(
                        *parent,
                        memberId,
                        Meta::Value::FromObject(valueType, child))
                    : runtime->WriteContent(
                        *parent,
                        memberId,
                        child);
                if (!content) return content.GetStatus();
            } else {
                if (auto* panel = ::Aero::TryCast<Controls::Panel>(parent)) {
                    if (auto* uiChild = ::Aero::TryCast<UIElement>(child.Get())) {
                        panel->GetChildren().Add(Base::Ref<UIElement>::FromBorrowed(*uiChild));
                    }
                } else if (auto* contentCtrl = ::Aero::TryCast<Controls::ContentControl>(parent)) {
                    contentCtrl->SetContent(child);
                } else if (runtime != nullptr) {
                    const Meta::TypeInfo* typeInfo = runtime->Types().FindType(parent->RuntimeType());
                    if (typeInfo != nullptr && typeInfo->ContentMember() != Meta::InvalidMemberId) {
                        static_cast<void>(runtime->WriteContent(*parent, typeInfo->ContentMember(), child));
                    }
                }
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
                        FrameworkElementSeams::AddStyleBehaviorPrototype(*framework, objRef);
                    }
                }
                for (std::uint16_t i = 0U; i < tCount; ++i) {
                    const auto& objRef = program.objectRefs[itemStart + bCount + i];
                    if (objRef) {
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

            services.resources = ResourceResolver(
                +[](void* ctx, Base::StringView key) noexcept -> Base::Result<Aero::Value> {
                    auto* vmCtx = static_cast<const XamlVMContext*>(ctx);
                    if (vmCtx != nullptr && vmCtx->documentResources != nullptr) {
                        Aero::Value val = vmCtx->documentResources->Lookup(key);
                        if (val.Kind() != Meta::ValueKind::Unset) return val;
                    }
                    if (vmCtx != nullptr && vmCtx->loadContext != nullptr) {
                        if (vmCtx->loadContext->resources != nullptr) {
                            Aero::Value val = vmCtx->loadContext->resources->Lookup(key);
                            if (val.Kind() != Meta::ValueKind::Unset) return val;
                        }
                        if (vmCtx->loadContext->fallbackResources != nullptr) {
                            Aero::Value val = vmCtx->loadContext->fallbackResources->Lookup(key);
                            if (val.Kind() != Meta::ValueKind::Unset) return val;
                        }
                    }
                    return Base::Status::Failure(Base::ErrorCode::NotFound, "Resource not found");
                },
                const_cast<XamlVMContext*>(&context)
            );

            Base::Result<ProvidedValue> provided = context.schema->ProvideMarkupExtensionValue(
                extTypeId, arguments, services);
            if (!provided) {
                return provided.GetStatus();
            }
            if (provided.Value().kind == ProvidedValueKind::Value) {
                if (auto* depObj = ::Aero::TryCast<DependencyObject>(outObjects[slot].Get())) {
                    if (context.templateBuilder != nullptr) {
                        depObj->SetTemplateValue(Meta::DependencyPropertyHandle{propHandle}, provided.Value().value);
                    } else {
                        depObj->SetValue(Meta::DependencyPropertyHandle{propHandle}, provided.Value().value);
                    }
                } else if (runtime != nullptr) {
                    static_cast<void>(runtime->SetProperty(*outObjects[slot], propHandle, provided.Value().value));
                }
            } else if (provided.Value().kind == ProvidedValueKind::Deferred) {
                deferredExtensions.PushBack(PendingDeferredExtension(provided.Value()));
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

        case XamlOpCode::End: {
            for (PendingDeferredExtension& item : deferredExtensions) {
                if (item.value.bind != nullptr) {
                    EffectServices effectServices;
                    effectServices.effectiveValues = context.loadContext != nullptr ? context.loadContext->effectiveValues : nullptr;
                    effectServices.bindings = context.bindings != nullptr ? context.bindings : (context.loadContext != nullptr ? context.loadContext->bindings : nullptr);
                    effectServices.fallbackResources = context.documentResources != nullptr ? context.documentResources : (context.loadContext != nullptr ? context.loadContext->fallbackResources : nullptr);
                    Base::Result<void> bound = item.value.bind(item.value.rollbackContext, effectServices);
                    if (!bound) return bound.GetStatus();
                }
                if (item.value.prepare != nullptr && context.documentNames != nullptr) {
                    Base::Result<void> prepared = item.value.prepare(item.value.rollbackContext, *context.documentNames);
                    if (!prepared) return prepared.GetStatus();
                }
                if (item.value.commit != nullptr) {
                    Base::Result<std::uint64_t> committed = item.value.commit(item.value.rollbackContext);
                    if (!committed) return committed.GetStatus();
                }
            }
            return {};
        }

        default:
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Unknown XamlOpCode encountered during VM execution");
        }
    }
    for (PendingDeferredExtension& item : deferredExtensions) {
        if (item.value.bind != nullptr) {
            EffectServices effectServices;
            effectServices.effectiveValues = context.loadContext != nullptr ? context.loadContext->effectiveValues : nullptr;
            effectServices.bindings = context.bindings != nullptr ? context.bindings : (context.loadContext != nullptr ? context.loadContext->bindings : nullptr);
            effectServices.fallbackResources = context.documentResources != nullptr ? context.documentResources : (context.loadContext != nullptr ? context.loadContext->fallbackResources : nullptr);
            Base::Result<void> bound = item.value.bind(item.value.rollbackContext, effectServices);
            if (!bound) return bound.GetStatus();
        }
        if (item.value.prepare != nullptr && context.documentNames != nullptr) {
            Base::Result<void> prepared = item.value.prepare(item.value.rollbackContext, *context.documentNames);
            if (!prepared) return prepared.GetStatus();
        }
        if (item.value.commit != nullptr) {
            Base::Result<std::uint64_t> committed = item.value.commit(item.value.rollbackContext);
            if (!committed) return committed.GetStatus();
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

    Base::Vector<Base::Ref<Base::Object>> outObjects;
    Base::Result<void> executed = Execute(program, vmContext, outObjects, nullptr);
    if (!executed) return executed.GetStatus();

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
        bool isPropertyElement = false;
        std::size_t objectStackDepth = 0U;
    };

    Base::Vector<ObjectFrame> objectStack;
    Base::Vector<MemberFrame> memberStack;
    std::uint16_t nextSlot = 0U;

    for (const Node& node : document.Nodes()) {
        switch (node.Kind()) {
        case NodeKind::NamespaceDeclaration:
            break;

        case NodeKind::StartObject: {
            Base::StringView localName = node.Name().LocalName();
            if (localName == "ControlTemplate" ||
                localName == "DataTemplate" ||
                localName == "HierarchicalDataTemplate" ||
                localName == "ItemsPanelTemplate") {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Templates require cursor-based template compiler");
            }
            const void* dotPtr = localName.Data() != nullptr ? std::memchr(localName.Data(), '.', localName.SizeBytes()) : nullptr;
            if (dotPtr != nullptr && !objectStack.Empty()) {
                const std::uint16_t parentSlot = objectStack.Back().slot;
                MemberFrame mf;
                mf.parentSlot = parentSlot;
                mf.isPropertyElement = true;
                mf.objectStackDepth = objectStack.Size();
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
                 node.Name().NamespaceUri() == "http://schemas.microsoft.com/winfx/2006/xaml" ||
                 node.Name().NamespaceUri() == "http://schemas.microsoft.com/winfx/2006/xaml/presentation")) {
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
                builder.EmitConnectEventHandler(mf.parentSlot, mf.memberId, node.Value());
            } else {
                if (node.HasCompiledValue()) {
                    builder.EmitSetPropertyValue(
                        mf.parentSlot,
                        Meta::DependencyPropertyHandle{mf.memberId},
                        node.CompiledValue());
                } else if (!node.Value().Empty()) {
                    Base::StringView strVal = node.Value();
                    if (strVal.SizeBytes() >= 2U && strVal[0] == '{' && strVal[1] == '}') {
                        strVal = strVal.Substr(2U, strVal.SizeBytes() - 2U);
                    }
                    if (TrimAscii(strVal).Empty()) {
                        break;
                    }
                    if (strVal.SizeBytes() > 0U && strVal[0] == '{') {
                        Base::StringView trimmed = TrimAscii(strVal);
                        if (trimmed == "{x:Null}" || trimmed == "{Null}") {
                            Meta::TypeId targetType = (mf.valueType != Meta::InvalidTypeId)
                                ? mf.valueType
                                : Meta::TypeOf<Base::String>();
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
                                if (extName == "StaticResource" || extName == "StaticResourceExtension") {
                                    extTypeId = Meta::MakeTypeId(Meta::AeroNamespaceUri(), Base::StringView("StaticResourceExtension"));
                                } else if (extName == "DynamicResource" || extName == "DynamicResourceExtension") {
                                    extTypeId = Meta::MakeTypeId(Meta::AeroNamespaceUri(), Base::StringView("DynamicResource"));
                                } else if (extName == "Binding") {
                                    extTypeId = Meta::MakeTypeId(Meta::AeroNamespaceUri(), Base::StringView("Binding"));
                                } else if (extName == "TemplateBinding") {
                                    extTypeId = Meta::MakeTypeId(Meta::AeroNamespaceUri(), Base::StringView("TemplateBinding"));
                                } else if (extName == "x:Type" || extName == "Type") {
                                    extTypeId = Meta::MakeTypeId(Base::StringView("http://schemas.microsoft.com/winfx/2006/xaml"), Base::StringView("Type"));
                                } else if (extName == "x:Static" || extName == "Static") {
                                    extTypeId = Meta::MakeTypeId(Base::StringView("http://schemas.microsoft.com/winfx/2006/xaml"), Base::StringView("Static"));
                                } else if (extName == "aero:Loc" || extName == "Loc") {
                                    extTypeId = Meta::MakeTypeId(Meta::AeroNamespaceUri(), Base::StringView("Loc"));
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
                        // Complex dynamic markup extension: bail out to cursor engine for 100% conformance!
                        return Base::Status::Failure(
                            Base::ErrorCode::Unsupported,
                            "Dynamic markup extension bailout to cursor engine");
                    }
                    Meta::TypeId targetType = (mf.valueType != Meta::InvalidTypeId)
                        ? mf.valueType
                        : Meta::TypeOf<Base::String>();
                    if (targetType == Meta::TypeOf<Meta::TypeReference>()) {
                        Base::StringView typeName = TrimAscii(strVal);
                        Base::Result<const Meta::TypeInfo*> resolved = schema.ResolveType(
                            Meta::AeroNamespaceUri(), typeName);
                        if (!resolved) {
                            resolved = schema.ResolveType(Base::StringView("http://schemas.microsoft.com/winfx/2006/xaml/presentation"), typeName);
                        }
                        if (resolved && resolved.Value() != nullptr) {
                            Meta::TypeReference ref{resolved.Value()->Id()};
                            builder.EmitSetPropertyValue(
                                mf.parentSlot,
                                Meta::DependencyPropertyHandle{mf.memberId},
                                Meta::ValueCodec<Meta::TypeReference>::Encode(ref).Value());
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
            ObjectFrame finished = objectStack.Back();
            objectStack.PopBack();

            if (!objectStack.Empty()) {
                ObjectFrame& parent = objectStack.Back();
                if (!memberStack.Empty()) {
                    MemberFrame& mem = memberStack.Back();
                    if (mem.isPropertyElement) {
                        builder.EmitSetPropertyContent(
                            parent.slot,
                            mem.memberId,
                            finished.slot,
                            finished.typeId,
                            false);
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

