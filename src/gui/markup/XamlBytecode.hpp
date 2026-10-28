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
#include "gui/markup/MarkupExtensionContract.hpp"
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

struct TemplatePrototypeNode {
    Meta::TypeId type = Meta::InvalidTypeId;
    Base::String name;
    std::uint32_t parent = UINT32_MAX;
    Meta::MemberId contentMember = Meta::InvalidMemberId;
    Base::Vector<TemplatePrototypeProperty> properties;
    Base::Vector<::Aero::GridLength> gridColumns;
    Base::Vector<::Aero::GridLength> gridRows;
    Base::String streamGeometryData;
    Base::Vector<TemplatePrototypeGradientStop> gradientStops;
    Base::Vector<Base::Ref<Base::Object>> authoredBehaviors;
    Base::Vector<Base::Ref<Base::Object>> authoredTriggers;
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

struct CompiledTemplateBlueprint {
    ::Aero::Meta::Registry* runtime = nullptr;
    Meta::DependencyPropertyRegistry* properties = nullptr;
    Base::Vector<TemplatePrototypeNode> nodes;
    Base::Vector<TemplatePrototypeBinding> bindings;
    std::uint32_t contentPresenter = UINT32_MAX;
};

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

Base::Result<CompiledTemplateBlueprint>
CompileDeferredTemplateBlueprint(
    const Base::Ref<Base::Object>& visualTree,
    const Aero::NameScope* names,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties) noexcept;

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
using ::Aero::Markup::CompiledTemplateBlueprint;
using ::Aero::Markup::CompiledTemplateDefinition;
using ::Aero::Markup::BuildCompiledTemplate;
using ::Aero::Markup::BuildCompiledDeferredTemplate;
using ::Aero::Markup::CompileDeferredTemplateBlueprint;
using ::Aero::Markup::CompileControlTemplateDefinition;
using ::Aero::Markup::CompileInheritedControlTemplate;



// ============================================================================
// XamlBytecode OpCodes (Instruction Set Architecture)
// ============================================================================
// Linear, deterministic, zero-branch instruction set for UI tree instantiation
// aligned with modern high-performance retained-mode GUI bytecode VMs.
enum class XamlOpCode : std::uint8_t {
    Nop = 0,
    CreateObject,               // [slot: u16, typeId: u64]
    SetPropertyValue,           // [slot: u16, propertyHandle: u64, valueIndex: u32]
    SetPropertyObject,          // [slot: u16, propertyHandle: u64, valueType: u64, sourceSlot: u16]
    SetPropertyContent,         // [parentSlot: u16, memberId: u64, childSlot: u16, valueType: u64, isPropertyEdge: u8]
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
    ConnectEventHandler = 17,   // [slot: u16, eventMemberId: u64, handlerNameIndex: u32]
    ApplyMarkupExtension = 18,  // [slot: u16, propertyHandle: u64, extensionTypeId: u64, argStringIndex: u32]
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
        static_cast<void>(stringMap_.Insert(Base::String{}, 0U));
    }

    void SetSlotCount(std::uint32_t count) noexcept {
        slotCount_ = count;
    }

    std::uint32_t AddString(Base::StringView str) noexcept {
        if (str.Empty()) return 0U;
        Base::String s;
        static_cast<void>(s.Assign(str));
        if (const std::uint32_t* found = stringMap_.Find(s)) {
            return *found;
        }
        const std::uint32_t newIndex = stringPool_.Size();
        stringPool_.PushBack(s);
        static_cast<void>(stringMap_.Insert(std::move(s), newIndex));
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

    void EmitApplyMarkupExtension(
        std::uint16_t slot,
        Meta::DependencyPropertyHandle property,
        Meta::TypeId extensionTypeId,
        Base::StringView arguments) noexcept {
        const std::uint32_t argIndex = AddString(arguments);
        static_cast<void>(AppendU8(instructions_, static_cast<std::uint8_t>(XamlOpCode::ApplyMarkupExtension)));
        static_cast<void>(AppendU16(instructions_, slot));
        static_cast<void>(AppendU64(instructions_, property.value));
        static_cast<void>(AppendU64(instructions_, extensionTypeId));
        static_cast<void>(AppendU32(instructions_, argIndex));
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
    Base::HashMap<Base::String, std::uint32_t> stringMap_;
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

    // Copy associated blueprint dynamic metadata and slot names/types
    outProgram.blueprintBindings = blueprint.bindings;
    outProgram.contentPresenterSlot = blueprint.contentPresenter;
    outProgram.runtime = blueprint.runtime;
    outProgram.properties = blueprint.properties;
    outProgram.slotNames.Clear();
    outProgram.slotTypes.Clear();
    outProgram.slotNames.Reserve(blueprint.nodes.Size());
    outProgram.slotTypes.Reserve(blueprint.nodes.Size());
    for (const auto& node : blueprint.nodes) {
        Base::String nameCopy;
        static_cast<void>(nameCopy.Assign(node.name.View()));
        outProgram.slotNames.PushBack(std::move(nameCopy));
        outProgram.slotTypes.PushBack(node.type);
    }



    return {};
}

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
            if (slot >= outObjects.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in AdoptOrCreateRoot");
            }
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
            if (!reader.HasBytes(2 + 8)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated CreateObject");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint64_t typeId = reader.ReadU64();
            if (slot >= outObjects.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in CreateObject");
            }
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
            if (auto* depObj = ::Aero::TryCast<DependencyObject>(outObjects[slot].Get())) {
                if (context.templateBuilder != nullptr) {
                    depObj->SetTemplateValue(Meta::DependencyPropertyHandle{propHandle}, program.valuePool[valIdx]);
                } else {
                    depObj->SetValue(Meta::DependencyPropertyHandle{propHandle}, program.valuePool[valIdx]);
                }
            } else if (runtime != nullptr) {
                static_cast<void>(runtime->SetProperty(*outObjects[slot], propHandle, program.valuePool[valIdx]));
            }
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
            if (!reader.HasBytes(2 + 8 + 2 + 8 + 1)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated SetPropertyContent");
            }
            const std::uint16_t parentSlot = reader.ReadU16();
            const std::uint64_t memberId = reader.ReadU64();
            const std::uint16_t childSlot = reader.ReadU16();
            const std::uint64_t valueType = reader.ReadU64();
            const std::uint8_t isPropertyEdge = reader.ReadU8();
            if (parentSlot >= outObjects.Size() || childSlot >= outObjects.Size() ||
                !outObjects[parentSlot] || !outObjects[childSlot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in SetPropertyContent");
            }
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
            if (!reader.HasBytes(2 + 4)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated SetRootVisual");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint32_t nameIdx = reader.ReadU32();
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
            if (!reader.HasBytes(2 + 2 + 4)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated AddVisualChild");
            }
            const std::uint16_t parentSlot = reader.ReadU16();
            const std::uint16_t childSlot = reader.ReadU16();
            const std::uint32_t nameIdx = reader.ReadU32();
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
            if (!reader.HasBytes(2 + 4)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated AddObjectPart");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint32_t nameIdx = reader.ReadU32();
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
            if (!reader.HasBytes(2 + 4)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated RegisterDocumentName");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint32_t nameIdx = reader.ReadU32();
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
            if (!reader.HasBytes(2 + 4 + 2 + 4 + 2)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated SetGridDefinitions");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint32_t colStart = reader.ReadU32();
            const std::uint16_t colCount = reader.ReadU16();
            const std::uint32_t rowStart = reader.ReadU32();
            const std::uint16_t rowCount = reader.ReadU16();
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

        case XamlOpCode::SetStreamGeometryData: {
            if (!reader.HasBytes(2 + 4)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated SetStreamGeometryData");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint32_t strIdx = reader.ReadU32();
            if (slot >= outObjects.Size() || !outObjects[slot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in SetStreamGeometryData");
            }
            if (strIdx >= program.stringPool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "String index out of bounds in SetStreamGeometryData");
            }
            if (runtime != nullptr && runtime->Types().IsDerivedFrom(outObjects[slot]->RuntimeType(), Media::StreamGeometry::StaticTypeId())) {
                auto& geom = static_cast<Media::StreamGeometry&>(*outObjects[slot]);
                geom.SetData(program.stringPool[strIdx].View());
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
            if (!reader.HasBytes(2)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated ProjectContent");
            }
            const std::uint16_t presenterSlot = reader.ReadU16();
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
            if (!reader.HasBytes(2)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated PopulateContentPresenter");
            }
            const std::uint16_t slot = reader.ReadU16();
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
            if (!reader.HasBytes(2)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated PopulateItemsPresenter");
            }
            const std::uint16_t slot = reader.ReadU16();
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
            if (!reader.HasBytes(2 + 4 + 2 + 2)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated AttachBehaviorsAndTriggers");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint32_t itemStart = reader.ReadU32();
            const std::uint16_t bCount = reader.ReadU16();
            const std::uint16_t tCount = reader.ReadU16();
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
            if (!reader.HasBytes(2 + 8 + 4)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated ConnectEventHandler");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint64_t eventMemberId = reader.ReadU64();
            const std::uint32_t handlerNameIndex = reader.ReadU32();
            if (slot >= outObjects.Size() || !outObjects[slot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in ConnectEventHandler");
            }
            if (handlerNameIndex >= program.stringPool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "String index out of bounds in ConnectEventHandler");
            }
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
            if (!reader.HasBytes(2 + 8 + 8 + 4)) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Truncated ApplyMarkupExtension");
            }
            const std::uint16_t slot = reader.ReadU16();
            const std::uint64_t propHandle = reader.ReadU64();
            const std::uint64_t extTypeId = reader.ReadU64();
            const std::uint32_t argIdx = reader.ReadU32();
            if (slot >= outObjects.Size() || !outObjects[slot]) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in ApplyMarkupExtension");
            }
            if (argIdx >= program.stringPool.Size()) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "String index out of bounds in ApplyMarkupExtension");
            }
            if (context.schema == nullptr) {
                return Base::Status::Failure(Base::ErrorCode::InvalidState, "Schema missing in ApplyMarkupExtension");
            }
            if (slot >= outObjects.Size() || outObjects[slot].Get() == nullptr) {
                return Base::Status::Failure(Base::ErrorCode::InvalidState, "Invalid target slot or null object in ApplyMarkupExtension");
            }
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
                if (provided.Value().commit != nullptr) {
                    static_cast<void>(provided.Value().commit(provided.Value().rollbackContext));
                }
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
                                if (extName == "StaticResource") {
                                    extTypeId = Meta::MakeTypeId(Meta::AeroNamespaceUri(), Base::StringView("StaticResourceExtension"));
                                } else if (extName == "DynamicResource") {
                                    extTypeId = Meta::MakeTypeId(Meta::AeroNamespaceUri(), Base::StringView("DynamicResourceExtension"));
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

