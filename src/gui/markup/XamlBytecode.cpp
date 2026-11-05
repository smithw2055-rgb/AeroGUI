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

Base::Result<void> AppendU8(
    Base::Vector<std::uint8_t>& output,
    std::uint8_t value) noexcept {
    output.PushBack(value);
    return {};
}

Base::Result<void> AppendU16(
    Base::Vector<std::uint8_t>& output,
    std::uint16_t value) noexcept {
    output.PushBack(static_cast<std::uint8_t>(value));
    output.PushBack(static_cast<std::uint8_t>(value >> 8U));
    return {};
}

Base::Result<void> AppendU32(
    Base::Vector<std::uint8_t>& output,
    std::uint32_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
        output.PushBack(static_cast<std::uint8_t>(value >> shift));
    }
    return {};
}

Base::Result<void> AppendU64(
    Base::Vector<std::uint8_t>& output,
    std::uint64_t value) noexcept {
    for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
        output.PushBack(static_cast<std::uint8_t>(value >> shift));
    }
    return {};
}

Base::Result<void> AppendVarU64(
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

Base::Result<void> AppendVarU32(
    Base::Vector<std::uint8_t>& output,
    std::uint32_t value) noexcept {
    return AppendVarU64(output, value);
}

Base::Result<void> AppendString(
    Base::Vector<std::uint8_t>& output,
    Base::StringView value) noexcept {
    Base::Result<void> length = AppendU32(output, value.SizeBytes());
    if (!length) return length.GetStatus();
    for (std::uint32_t index = 0U; index < value.SizeBytes(); ++index) {
        output.PushBack(static_cast<std::uint8_t>(value[index]));
    }
    return {};
}

Base::Result<std::uint8_t> Decoder::ReadU8() noexcept {
    if (offset_ >= bytes_.Size()) return Truncated();
    return bytes_[offset_++];
}

Base::Result<std::uint16_t> Decoder::ReadU16() noexcept {
    if (bytes_.Size() - offset_ < 2U) return Truncated();
    const std::uint16_t value = static_cast<std::uint16_t>(bytes_[offset_]) |
        static_cast<std::uint16_t>(bytes_[offset_ + 1U] << 8U);
    offset_ += 2U;
    return value;
}

Base::Result<std::uint32_t> Decoder::ReadU32() noexcept {
    if (bytes_.Size() - offset_ < 4U) return Truncated();
    std::uint32_t value = 0U;
    for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
        value |= static_cast<std::uint32_t>(bytes_[offset_++]) << shift;
    }
    return value;
}

Base::Result<std::uint64_t> Decoder::ReadU64() noexcept {
    if (bytes_.Size() - offset_ < 8U) return Truncated();
    std::uint64_t value = 0U;
    for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
        value |= static_cast<std::uint64_t>(bytes_[offset_++]) << shift;
    }
    return value;
}

Base::Result<std::uint64_t> Decoder::ReadVarU64() noexcept {
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

Base::Result<std::uint32_t> Decoder::ReadVarU32() noexcept {
    Base::Result<std::uint64_t> value = ReadVarU64();
    if (!value) return value.GetStatus();
    if (value.Value() > UINT32_MAX) {
        return InvalidVarUInt();
    }
    return static_cast<std::uint32_t>(value.Value());
}

Base::Result<Base::String> Decoder::ReadString(
    std::uint32_t& totalStringBytes,
    std::uint32_t maxStringBytes) noexcept {
    return ReadStringInternal(nullptr, totalStringBytes, maxStringBytes);
}

Base::Result<Base::String> Decoder::ReadString(
    Base::IAllocator& allocator,
    std::uint32_t& totalStringBytes,
    std::uint32_t maxStringBytes) noexcept {
    return ReadStringInternal(&allocator, totalStringBytes, maxStringBytes);
}

Base::Result<Base::String> Decoder::ReadStringInternal(
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

Base::Status Decoder::Truncated() const noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfRange,
        truncatedMessage_);
}

Base::Status Decoder::InvalidVarUInt() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "Binary variable-length integer is invalid");
}

std::uint32_t XamlBytecodeProgram::FindSlotByName(Base::StringView name) const noexcept {
    for (std::uint32_t i = 0U; i < slotNames.Size(); ++i) {
        if (slotNames[i].View() == name) return i;
    }
    return UINT32_MAX;
}

std::int32_t XamlBytecodeProgram::FindPartIndexByName(Base::StringView name) const noexcept {
    if (name.Empty()) return -1;
    std::int32_t partIndex = 0;
    for (const auto& inst : instructions) {
        switch (inst.op) {
        case XamlOpCode::SetGradientStops: {
            const std::uint32_t stopStart = inst.operandIdx;
            const std::uint16_t stopCount = inst.auxSlot;
            if (stopStart + stopCount <= gradientStops.Size()) {
                for (std::uint16_t i = 0U; i < stopCount; ++i) {
                    const auto& stop = gradientStops[stopStart + i];
                    if (!stop.name.Empty()) {
                        if (stop.name.View() == name) return partIndex;
                        ++partIndex;
                    }
                }
            }
            break;
        }
        case XamlOpCode::SetRootVisual:
        case XamlOpCode::AddVisualChild: {
            const Base::StringView partName = (inst.operandIdx < stringPool.Size())
                ? stringPool[inst.operandIdx].View()
                : Base::StringView{};
            if (!partName.Empty() && partName == name) {
                return partIndex;
            }
            ++partIndex;
            break;
        }
        case XamlOpCode::AddObjectPart: {
            const Base::StringView partName = (inst.operandIdx < stringPool.Size())
                ? stringPool[inst.operandIdx].View()
                : Base::StringView{};
            if (!partName.Empty()) {
                if (partName == name) return partIndex;
                ++partIndex;
            }
            break;
        }
        default:
            break;
        }
    }
    return -1;
}

Meta::TypeId XamlBytecodeProgram::GetSlotType(std::uint32_t slot) const noexcept {
    return (slot < slotTypes.Size()) ? slotTypes[slot] : Meta::InvalidTypeId;
}

bool XamlBytecodeProgram::IsValid() const noexcept {
    return slotCount > 0U && !instructions.Empty();
}

void XamlBytecodeProgram::Clear() noexcept {
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
    dataTemplateTriggers.Clear();
    controlTemplateDataTriggers.Clear();
    controlTemplateEventTriggers.Clear();
    vsmPrograms.Clear();
    contentPresenterSlot = UINT32_MAX;
    slotNames.Clear();
    slotTypes.Clear();
    runtime = nullptr;
    properties = nullptr;
    schema = nullptr;
    baseUri = {};
    namespaces.Clear();
    ambientResourceOwners.Clear();
    fallbackResources.Clear();
    templateOwner = nullptr;
    ambientTargetType = Meta::InvalidTypeId;
}

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

Base::Result<void> BindCompiledSubProgramToTemplate(
    Base::Object& target,
    const Base::Ref<Base::Object>& subProgramRef,
    Meta::Registry* runtime,
    const Schema* schema) noexcept;

Base::Result<void> MaterializeCompiledVsmPrograms(
    Controls::ControlTemplate& controlTemplate,
    XamlBytecodeProgram& subProg,
    Meta::Registry& runtime,
    const Schema* schema,
    bool ignoreUnresolvedResources) noexcept;

Base::Result<void> ApplyPropertyToObject(
    Base::Object& target,
    std::uint64_t propHandle,
    const Meta::Value& value,
    Meta::Registry* runtime,
    Controls::TemplateBuilder* templateBuilder) noexcept {
    if (propHandle == VisualStateManager::VisualStateGroupsProperty.Handle().value) {
        if (auto* ct = ::Aero::TryCast<Controls::ControlTemplate>(&target)) {
            if (value.AsObject()) {
                if (value.AsObject()->RuntimeType() == VisualStateGroup::StaticTypeId()) {
                    ::Aero::Controls::FrameworkTemplateState::AddAuthoredVisualStateGroup(*ct, value.AsObject());
                    return {};
                }
                if (auto* vsgColl = ::Aero::TryCast<VisualStateGroupCollection>(value.AsObject().Get())) {
                    for (const auto& item : vsgColl->GetItems()) {
                        if (item) {
                            ::Aero::Controls::FrameworkTemplateState::AddAuthoredVisualStateGroup(
                                *ct, Base::Ref<Base::Object>(item));
                        }
                    }
                    return {};
                }
            }
        }
    }
    if (auto* depObj = ::Aero::TryCast<DependencyObject>(&target)) {
        const Meta::DependencyPropertyHandle dpHandle{propHandle};
        if (DependencyObjectAccess::HasPropertyRegistry(*depObj)) {
            auto& dpRegistry = DependencyObjectAccess::PropertyRegistry(*depObj);
            if (const auto* dp = dpRegistry.Find(dpHandle)) {
                if (propHandle == VisualStateManager::VisualStateGroupsProperty.Handle().value &&
                    value.AsObject() &&
                    value.AsObject()->RuntimeType() == VisualStateGroup::StaticTypeId()) {
                    Base::Ref<VisualStateGroupCollection> valueStore = depObj->GetValue(VisualStateManager::VisualStateGroupsProperty);
                    if (!valueStore) {
                        Base::Result<Base::Ref<VisualStateGroupCollection>> created =
                            Base::MakeRef<VisualStateGroupCollection>();
                        if (!created) return created.GetStatus();
                        valueStore = std::move(created).Value();
                        const bool isTemplateElement = templateBuilder != nullptr &&
                            (::Aero::TryCast<FrameworkElement>(depObj) != nullptr ||
                             ::Aero::TryCast<FrameworkContentElement>(depObj) != nullptr);
                        if (isTemplateElement) {
                            depObj->SetTemplateValue(VisualStateManager::VisualStateGroupsProperty, valueStore);
                        } else {
                            depObj->SetValue(VisualStateManager::VisualStateGroupsProperty, valueStore);
                        }
                    }
                    (void)valueStore->Add(
                        Base::Ref<VisualStateGroup>::FromBorrowed(
                            *static_cast<VisualStateGroup*>(value.AsObject().Get())));
                    return {};
                }
                Meta::Value effectiveValue = value;
                if (!effectiveValue.IsDynamicResource() && effectiveValue.Type() != dp->ValueType()) {
                    Base::Result<Meta::PropertyValue> normalized =
                        ::Aero::NormalizeValueForProperty(runtime, *dp, effectiveValue);
                    if (normalized) {
                        effectiveValue = std::move(normalized).Value();
                    }
                }
                const bool isTemplateElement = templateBuilder != nullptr &&
                    (::Aero::TryCast<FrameworkElement>(depObj) != nullptr ||
                     ::Aero::TryCast<FrameworkContentElement>(depObj) != nullptr);
                if (isTemplateElement) {
                    depObj->SetTemplateValue(dpHandle, effectiveValue);
                } else {
                    depObj->SetValue(dpHandle, effectiveValue);
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

Aero::ResourceDictionary* ResolveObjectResourceDictionary(
    Base::Object& obj,
    const Schema* schema) noexcept {
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
    if (schema != nullptr) {
        return XamlVM::ResolveResourceScope(*schema, obj.RuntimeType(), obj);
    }
    return nullptr;
}

Meta::TypeId ExtractImplicitResourceKeyType(const Meta::Value& itemVal) noexcept {
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

Base::Result<void> SynthesizeContentSourceBindings(
    Controls::ControlTemplate& controlTemplate,
    XamlBytecodeProgram& subProg,
    Meta::TypeId controlTargetType,
    Meta::DependencyPropertyRegistry& properties,
    Meta::Registry& runtime) noexcept {
    if (controlTargetType == Meta::InvalidTypeId) return {};

    const DependencyProperty* contentProperty =
        properties.Find(Controls::ContentPresenter::ContentProperty.Handle());
    const DependencyProperty* contentTemplateProperty =
        properties.Find(Controls::ContentPresenter::ContentTemplateProperty.Handle());
    if (contentProperty == nullptr) return {};

    for (std::uint32_t slot = 0U; slot < subProg.slotTypes.Size(); ++slot) {
        const Meta::TypeId slotType = subProg.slotTypes[slot];
        if (slotType == Meta::InvalidTypeId ||
            !runtime.Types().IsDerivedFrom(slotType, Controls::ContentPresenter::StaticTypeId())) {
            continue;
        }

        Base::StringView nodeName = (slot < subProg.slotNames.Size()) ? subProg.slotNames[slot].View() : Base::StringView{};
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

                const std::uint32_t nameIdx = subProg.stringPool.Size();
                Base::String poolStr;
                static_cast<void>(poolStr.Assign(nodeName));
                subProg.stringPool.PushBack(std::move(poolStr));
                for (auto& inst : subProg.instructions) {
                    if (inst.op == XamlOpCode::SetRootVisual && inst.slot == slot) {
                        inst.operandIdx = nameIdx;
                    } else if (inst.op == XamlOpCode::AddVisualChild && inst.auxSlot == slot) {
                        inst.operandIdx = nameIdx;
                    }
                }
            }
        }

        bool hasContentBinding = false;
        for (const auto& existing : ::Aero::Controls::FrameworkTemplateState::Bindings(controlTemplate)) {
            if (existing.targetName.View() == nodeName && existing.targetProperty == contentProperty->Handle()) {
                hasContentBinding = true;
                break;
            }
        }
        if (hasContentBinding) continue;

        bool explicitlySetsContent = false;
        for (const auto& inst : subProg.instructions) {
            if ((inst.op == XamlOpCode::SetPropertyValue ||
                 inst.op == XamlOpCode::SetPropertyObject ||
                 inst.op == XamlOpCode::SetPropertyContent) &&
                inst.slot == slot &&
                inst.typeOrPropIdx < subProg.propertyPool.Size() &&
                subProg.propertyPool[inst.typeOrPropIdx] == contentProperty->Handle().value) {
                explicitlySetsContent = true;
                break;
            }
        }
        if (explicitlySetsContent) continue;

        Base::String contentSourceStr;
        static_cast<void>(contentSourceStr.Assign(Base::StringView("Content")));
        const DependencyProperty* contentSourceProp =
            properties.Find(Controls::ContentPresenter::ContentSourceProperty.Handle());
        if (contentSourceProp != nullptr) {
            for (const auto& inst : subProg.instructions) {
                if (inst.op == XamlOpCode::SetPropertyValue &&
                    inst.slot == slot &&
                    inst.typeOrPropIdx < subProg.propertyPool.Size() &&
                    subProg.propertyPool[inst.typeOrPropIdx] == contentSourceProp->Handle().value &&
                    inst.operandIdx < subProg.valuePool.Size()) {
                    const Value& v = subProg.valuePool[inst.operandIdx];
                    if (v.Kind() == Meta::ValueKind::String) {
                        static_cast<void>(contentSourceStr.Assign(v.AsString()));
                    }
                    break;
                }
            }
        }

        const DependencyProperty* source = properties.Find(controlTargetType, contentSourceStr.View());

        if (source != nullptr) {
            const std::int32_t partIndex = subProg.FindPartIndexByName(nodeName);
            Base::Result<void> configured = ::Aero::Controls::FrameworkTemplateState::AddTemplateBinding(
                controlTemplate,
                nodeName,
                source->Handle(),
                contentProperty->Handle(),
                partIndex);
            if (!configured) return configured.GetStatus();

            if (contentTemplateProperty != nullptr) {
                Base::String templatePropName;
                if (templatePropName.Assign(contentSourceStr.View()) &&
                    templatePropName.Append(Base::StringView("Template"))) {
                    const DependencyProperty* sourceTemplate = properties.Find(
                        controlTargetType, templatePropName.View());
                    if (sourceTemplate != nullptr) {
                        configured = ::Aero::Controls::FrameworkTemplateState::AddTemplateBinding(
                            controlTemplate,
                            nodeName,
                            sourceTemplate->Handle(),
                            contentTemplateProperty->Handle(),
                            partIndex);
                        if (!configured) return configured.GetStatus();
                    }
                }
            }
        }
    }
    return {};
}

Base::Result<void> XamlVM::Execute(
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
        key = TrimAscii(key);
        if (key.SizeBytes() >= 11U && key.Substr(0U, 11U) == "ResourceKey") {
            Base::StringView rest = TrimAscii(key.Substr(11U, key.SizeBytes() - 11U));
            if (!rest.Empty() && rest[0] == '=') {
                key = TrimAscii(rest.Substr(1U, rest.SizeBytes() - 1U));
            }
        }
        if (key.SizeBytes() >= 2U &&
            ((key[0] == '\'' && key[key.SizeBytes() - 1U] == '\'') ||
             (key[0] == '"' && key[key.SizeBytes() - 1U] == '"'))) {
            key = key.Substr(1U, key.SizeBytes() - 2U);
        }
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

        for (std::uint32_t i = ambientDictStack.Size(); i > 0U; --i) {
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
        if (context.templateBuilder != nullptr) {
            ::Aero::Controls::Control& parent = context.templateBuilder->TemplatedParent();
            if (resolvedTypeId != Meta::InvalidTypeId) {
                Base::Result<Aero::ResourceValue> rv = parent.FindResource(ResourceKey::FromType(resolvedTypeId));
                if (rv && !rv.Value().IsUnset()) return rv.Value();
            }
            Base::Result<Aero::ResourceValue> rv = parent.FindResource(key);
            if (rv && !rv.Value().IsUnset()) return rv.Value();
        }
        if (context.deferredContentOwner != nullptr) {
            if (auto* fe = ::Aero::TryCast<FrameworkElement>(context.deferredContentOwner)) {
                if (resolvedTypeId != Meta::InvalidTypeId) {
                    Base::Result<Aero::ResourceValue> rv = fe->FindResource(ResourceKey::FromType(resolvedTypeId));
                    if (rv && !rv.Value().IsUnset()) return rv.Value();
                }
                Base::Result<Aero::ResourceValue> rv = fe->FindResource(key);
                if (rv && !rv.Value().IsUnset()) return rv.Value();
            }
        }
        if (context.existingRoot) {
            if (auto* fe = ::Aero::TryCast<FrameworkElement>(context.existingRoot.Get())) {
                if (resolvedTypeId != Meta::InvalidTypeId) {
                    Base::Result<Aero::ResourceValue> rv = fe->FindResource(ResourceKey::FromType(resolvedTypeId));
                    if (rv && !rv.Value().IsUnset()) return rv.Value();
                }
                Base::Result<Aero::ResourceValue> rv = fe->FindResource(key);
                if (rv && !rv.Value().IsUnset()) {
                    return rv.Value();
                }
            }
        }
        if (context.loadContext != nullptr && context.loadContext->existingRoot) {
            if (auto* fe = ::Aero::TryCast<FrameworkElement>(context.loadContext->existingRoot.Get())) {
                if (resolvedTypeId != Meta::InvalidTypeId) {
                    Base::Result<Aero::ResourceValue> rv = fe->FindResource(ResourceKey::FromType(resolvedTypeId));
                    if (rv && !rv.Value().IsUnset()) return rv.Value();
                }
                Base::Result<Aero::ResourceValue> rv = fe->FindResource(key);
                if (rv && !rv.Value().IsUnset()) {
                    return rv.Value();
                }
            }
        }
        for (const auto& obj : outObjects) {
            if (obj) {
                if (auto* dict = ResolveObjectResourceDictionary(*obj, context.schema)) {
                    Aero::Value val = lookupDict(dict);
                    if (val.Kind() != Meta::ValueKind::Unset) return val;
                }
            }
        }
        for (std::uint32_t i = program.ambientResourceOwners.Size(); i > 0U; --i) {
            if (Base::Ref<Base::Object> owner = program.ambientResourceOwners[i - 1U].Lock()) {
                if (auto* dict = ResolveObjectResourceDictionary(*owner, context.schema)) {
                    Aero::Value val = lookupDict(dict);
                    if (val.Kind() != Meta::ValueKind::Unset) return val;
                }
            }
        }
        if (program.fallbackResources.Size() > 0U ||
            program.fallbackResources.MergedDictionaryCount() > 0U) {
            Aero::Value val = lookupDict(&program.fallbackResources);
            if (val.Kind() != Meta::ValueKind::Unset) return val;
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
    struct DeferredStaticExtension {
        std::uint16_t slot = 0U;
        std::uint64_t propHandle = 0U;
        Base::String resourceKey;
        const Aero::ResourceDictionary* localScope = nullptr;
    };
    Base::Vector<DeferredStaticResourceWrite> deferredWrites;
    Base::Vector<DeferredStaticExtension> deferredStaticExtensions;
    Base::Vector<std::uint16_t> deferredSealSlots;

    auto resolveObjectIfStaticResource = [&](
        std::uint16_t slot,
        const Aero::ResourceDictionary* localScope = nullptr,
        bool allowDefer = false) -> Base::Result<bool> {
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
                    if (allowDefer || (context.loadContext != nullptr && context.loadContext->deferUnresolvedStaticResources)) {
                        if (context.loadContext != nullptr && context.loadContext->deferUnresolvedStaticResources &&
                            context.hasDeferredStaticResources != nullptr) {
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
    Aero::NameScope localNames;
    Aero::NameScope* activeNames = context.documentNames != nullptr ? context.documentNames : &localNames;
    Meta::EffectiveValueEngine* activeEffectiveValues =
        context.loadContext != nullptr ? context.loadContext->effectiveValues : nullptr;
    if (activeEffectiveValues == nullptr && context.templateBuilder != nullptr) {
        activeEffectiveValues = ElementTree::ValuesOf(context.templateBuilder->TemplatedParent());
    }

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
    if (runtime == nullptr) {
        runtime = Meta::CurrentObjectFactory().metadata;
    }

    auto sealTemplateOrStyle = [&](Base::Object* obj) -> Base::Result<void> {
        auto* propRegistry = Meta::CurrentObjectFactory().dependencyProperties;
        if (propRegistry == nullptr) return {};
        if (auto* style = ::Aero::TryCast<Aero::Style>(obj)) {
            return Aero::SealStyle(*style, *propRegistry);
        } else if (auto* ct = ::Aero::TryCast<Controls::ControlTemplate>(obj)) {
            if (!ct->GetIsSealed()) {
                const Controls::ControlTemplate* ancestor = ::Aero::TryCast<const Controls::ControlTemplate>(ct->GetBasedOn());
                while (ancestor != nullptr) {
                    if (ancestor == ct) {
                        return Base::Status::Failure(
                            Base::ErrorCode::CycleDetected,
                            "ControlTemplate BasedOn graph contains a cycle");
                    }
                    if (!ancestor->GetIsSealed()) {
                        return Base::Status::Failure(
                            Base::ErrorCode::InvalidState,
                            "BasedOn template must be sealed before its derived template");
                    }
                    ancestor = ::Aero::TryCast<const Controls::ControlTemplate>(ancestor->GetBasedOn());
                }
                if (auto* templateBase = ::Aero::TryCast<const Controls::ControlTemplate>(ct->GetBasedOn())) {
                    if (::Aero::Controls::FrameworkTemplateState::Factory(*ct) == nullptr) {
                        if (ct->GetTargetType() == Meta::InvalidTypeId && templateBase->GetTargetType() != Meta::InvalidTypeId) {
                            static_cast<void>(::Aero::Controls::FrameworkTemplateState::SetTargetType(*ct, templateBase->GetTargetType()));
                        }
                        void* const baseFactoryContext = ::Aero::Controls::FrameworkTemplateState::FactoryContext(*templateBase);
                        if (baseFactoryContext != nullptr && runtime != nullptr) {
                            const auto* baseProgram = static_cast<const XamlBytecodeProgram*>(baseFactoryContext);
                            Base::Result<void> inherited = CompileInheritedControlTemplate(*ct, *baseProgram, *runtime, *propRegistry);
                            if (!inherited) return inherited.GetStatus();
                        }
                    }
                }
                if (::Aero::Controls::FrameworkTemplateState::Factory(*ct) == &BuildCompiledTemplate && runtime != nullptr) {
                    auto* subProg = static_cast<XamlBytecodeProgram*>(
                        ::Aero::Controls::FrameworkTemplateState::FactoryContext(*ct));
                    if (subProg != nullptr) {
                        if (ct->GetTargetType() != Meta::InvalidTypeId) {
                            Base::Result<void> synthRes = SynthesizeContentSourceBindings(
                                *ct, *subProg, ct->GetTargetType(), *propRegistry, *runtime);
                            if (!synthRes) return synthRes.GetStatus();
                        }
                        if (auto* ctState = ::Aero::Controls::FrameworkTemplateState::State(*ct)) {
                            for (auto& binding : ctState->program.bindings) {
                                if (binding.targetPartIndex < 0) {
                                    binding.targetPartIndex = subProg->FindPartIndexByName(binding.targetName.View());
                                }
                            }
                            for (auto& binding : ctState->program.metadataBindings) {
                                if (binding.targetPartIndex < 0) {
                                    binding.targetPartIndex = subProg->FindPartIndexByName(binding.targetName.View());
                                }
                            }
                            for (auto& dynamicResource : ctState->program.dynamicResources) {
                                if (dynamicResource.targetPartIndex < 0) {
                                    dynamicResource.targetPartIndex = subProg->FindPartIndexByName(dynamicResource.targetName.View());
                                }
                            }
                        }
                        if (::Aero::Controls::FrameworkTemplateState::Triggers(*ct).Empty() &&
                            !::Aero::Controls::FrameworkTemplateState::AuthoredTriggers(*ct).Empty()) {
                            auto trigRes = CompilePropertyTriggers(*ct, *subProg, *runtime, *propRegistry);
                            if (!trigRes) return trigRes.GetStatus();
                            for (auto& trigger : trigRes.Value()) {
                                Base::Result<void> configured = ::Aero::Controls::FrameworkTemplateState::AddPropertyTrigger(
                                    *ct, std::move(trigger));
                                if (!configured) return configured.GetStatus();
                            }
                        }
                        if (::Aero::Controls::FrameworkTemplateState::VisualStateGroups(*ct).Empty() &&
                            ::Aero::Controls::FrameworkTemplateState::AuthoredVisualStateGroups(*ct).Empty() &&
                            !subProg->vsmPrograms.Empty()) {
                            if (!MaterializeCompiledVsmPrograms(
                                    *ct, *subProg, *runtime, context.schema, /*ignoreUnresolvedResources=*/false)) {
                                static_cast<void>(MaterializeCompiledVsmPrograms(
                                    *ct, *subProg, *runtime, context.schema, /*ignoreUnresolvedResources=*/true));
                            }
                        }
                        if (::Aero::Controls::FrameworkTemplateState::VisualStateGroups(*ct).Empty() &&
                            (!::Aero::Controls::FrameworkTemplateState::AuthoredVisualStateGroups(*ct).Empty() ||
                             ::Aero::Controls::FrameworkTemplateState::AuthoredVisualTree(*ct))) {
                            auto vsRes = CompileVisualStates(*ct, *subProg, *runtime, *propRegistry);
                            if (!vsRes) return vsRes.GetStatus();
                            for (auto& group : vsRes.Value()) {
                                Base::Result<void> configured = ::Aero::Controls::FrameworkTemplateState::AddVisualStateGroup(
                                    *ct, std::move(group));
                                if (!configured) return configured.GetStatus();
                            }
                            ::Aero::Controls::FrameworkTemplateState::ClearAuthoredVisualTree(*ct);
                            ::Aero::Controls::FrameworkTemplateState::ClearAuthoredVisualStateGroups(*ct);
                        }
                        if (subProg->controlTemplateDataTriggers.Empty() &&
                            subProg->controlTemplateEventTriggers.Empty() &&
                            !::Aero::Controls::FrameworkTemplateState::AuthoredTriggers(*ct).Empty()) {
                            for (const Base::Ref<Base::Object>& authored :
                                 ::Aero::Controls::FrameworkTemplateState::AuthoredTriggers(*ct)) {
                                if (!authored) continue;
                                if (authored->RuntimeType() == DataTrigger::StaticTypeId() ||
                                    authored->RuntimeType() == MultiDataTrigger::StaticTypeId() ||
                                    (authored->RuntimeType() == Trigger::StaticTypeId() &&
                                     (!static_cast<const Trigger&>(*authored).GetEnterActions().Empty() ||
                                      !static_cast<const Trigger&>(*authored).GetExitActions().Empty()))) {
                                    Base::Ref<TriggerBase> retained =
                                        Base::Ref<TriggerBase>::TryFromBorrowed(
                                            static_cast<TriggerBase&>(*authored));
                                    if (!retained) continue;
                                    Base::Result<CompiledTriggerPlan> plan = CompileTriggerPlan(
                                        std::move(retained), *subProg, *runtime, *propRegistry, ct->GetTargetType());
                                    if (!plan) return plan.GetStatus();
                                    subProg->controlTemplateDataTriggers.PushBack(std::move(plan).Value());
                                } else if (runtime->Types().IsDerivedFrom(
                                               authored->RuntimeType(),
                                               Media::Animation::EventTrigger::StaticTypeId())) {
                                    Base::Ref<Media::Animation::EventTrigger> retained =
                                        Base::Ref<Media::Animation::EventTrigger>::TryFromBorrowed(
                                            static_cast<Media::Animation::EventTrigger&>(*authored));
                                    if (!retained) continue;
                                    subProg->controlTemplateEventTriggers.PushBack(std::move(retained));
                                }
                            }
                        }
                    }
                }
                return ::Aero::Controls::FrameworkTemplateState::Seal(*ct, *propRegistry);
            }
        } else if (auto* ft = ::Aero::TryCast<Aero::FrameworkTemplate>(obj)) {
            if (!ft->GetIsSealed()) {
                if (auto* ftState = ::Aero::Controls::FrameworkTemplateState::State(*ft)) {
                    if (ftState->deferredProgram.factory == &BuildCompiledDeferredTemplate &&
                        ftState->deferredProgram.context != nullptr && runtime != nullptr) {
                        auto* subProg = static_cast<XamlBytecodeProgram*>(ftState->deferredProgram.context);
                        if (subProg->dataTemplateTriggers.Empty() &&
                            !::Aero::Controls::FrameworkTemplateState::AuthoredTriggers(*ft).Empty()) {
                            for (const Base::Ref<Base::Object>& authored :
                                 ::Aero::Controls::FrameworkTemplateState::AuthoredTriggers(*ft)) {
                                if (!authored) continue;
                                if (authored->RuntimeType() == DataTrigger::StaticTypeId() ||
                                    authored->RuntimeType() == MultiDataTrigger::StaticTypeId() ||
                                    authored->RuntimeType() == Trigger::StaticTypeId()) {
                                    Base::Ref<TriggerBase> retained =
                                        Base::Ref<TriggerBase>::TryFromBorrowed(
                                            static_cast<TriggerBase&>(*authored));
                                    if (!retained) continue;
                                    Base::Result<CompiledTriggerPlan> plan = CompileTriggerPlan(
                                        std::move(retained), *subProg, *runtime, *propRegistry);
                                    if (!plan) return plan.GetStatus();
                                    subProg->dataTemplateTriggers.PushBack(std::move(plan).Value());
                                }
                            }
                        }
                    }
                }
                return ::Aero::Controls::FrameworkTemplateState::Seal(*ft);
            }
        }
        return {};
    };

    const auto isVisualEdge = [&](Base::Object* parentObj, Meta::MemberId member) noexcept -> bool {
        if (parentObj == nullptr || runtime == nullptr) return false;
        if (runtime->Types().IsDerivedFrom(parentObj->RuntimeType(), Controls::ItemsControl::StaticTypeId())) {
            return false;
        }
        if (member == Meta::InvalidMemberId) {
            member = runtime->FindContentMember(parentObj->RuntimeType());
        }
        if (member == Meta::InvalidMemberId) return false;
        Base::Result<Meta::ContentInfo> content = runtime->GetContentInfo(member);
        if (content && content.Value().writable && content.Value().IsVisual()) {
            return true;
        }
        const Meta::PropertyInfo* prop = runtime->Types().FindProperty(member);
        if (prop != nullptr &&
            (static_cast<std::uint32_t>(prop->Flags()) & static_cast<std::uint32_t>(Meta::PropertyFlags::Structural)) != 0U &&
            (static_cast<std::uint32_t>(prop->Flags()) & static_cast<std::uint32_t>(Meta::PropertyFlags::Collection)) == 0U &&
            runtime->CanWriteProperty(member)) {
            return true;
        }
        return false;
    };

    auto setPropertyObject = [&](std::uint16_t slot, std::uint16_t srcSlot, std::uint64_t propHandle, std::uint64_t valueType) -> Base::Result<void> {
        if (slot >= outObjects.Size() || srcSlot >= outObjects.Size() || !outObjects[slot]) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in SetPropertyObject");
        }
        const std::uint64_t actualValueType = (outObjects[srcSlot] &&
            (valueType == Markup::StaticResourceObject::StaticTypeId() ||
             valueType == Meta::InvalidTypeId ||
             valueType == Meta::TypeOf<Meta::Value>() ||
             valueType == Base::Object::StaticTypeId()))
            ? outObjects[srcSlot]->RuntimeType() : valueType;
        Meta::Value val = outObjects[srcSlot]
            ? Meta::Value::FromObject(actualValueType, outObjects[srcSlot])
            : (srcSlot < outValues.Size() ? outValues[srcSlot] : Meta::Value{});
        static_cast<void>(ApplyPropertyToObject(
            *outObjects[slot], propHandle,
            val,
            runtime, context.templateBuilder));
        if (context.visualContent != nullptr && context.templateBuilder == nullptr && isVisualEdge(outObjects[slot].Get(), propHandle)) {
            if (auto* parentUi = ::Aero::TryCast<UIElement>(outObjects[slot].Get())) {
                if (outObjects[srcSlot]) {
                    if (auto* childUi = ::Aero::TryCast<UIElement>(outObjects[srcSlot].Get())) {
                        bool alreadyExists = false;
                        for (const auto& edge : context.visualContent->mountEdges) {
                            if (edge.parent == parentUi && edge.child == childUi) {
                                alreadyExists = true;
                                break;
                            }
                        }
                        if (!alreadyExists) {
                            static_cast<void>(context.visualContent->AddNode(*parentUi));
                            static_cast<void>(context.visualContent->AddNode(*childUi));
                            context.visualContent->mountEdges.PushBack({ parentUi, childUi, {} });
                            context.visualContent->contentEdges.PushBack({
                                outObjects[slot],
                                outObjects[srcSlot],
                                runtime,
                                propHandle,
                                true
                            });
                        }
                    }
                }
            }
        }
        return {};
    };

    auto setPropertyContent = [&](std::uint16_t parentSlot, std::uint16_t childSlot, std::uint64_t memberId, std::uint64_t valueType, bool isPropertyEdge) -> Base::Result<void> {
        if (parentSlot >= outObjects.Size() || childSlot >= outObjects.Size() || !outObjects[parentSlot]) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Invalid slot in SetPropertyContent");
        }
        Base::Object* parent = outObjects[parentSlot].Get();
        Base::Ref<Base::Object> child = outObjects[childSlot];
        if (child) {
            const std::uint64_t actualValueType = (child &&
                (valueType == Markup::StaticResourceObject::StaticTypeId() ||
                 valueType == Meta::InvalidTypeId ||
                 valueType == Meta::TypeOf<Meta::Value>() ||
                 valueType == Base::Object::StaticTypeId()))
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
                    if (auto* uiChild = ::Aero::TryCast<UIElement>(child.Get())) {
                        contentCtrl->SetOwnedContent(child, *uiChild);
                    } else {
                        contentCtrl->SetContent(child);
                    }
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
            if (context.visualContent != nullptr && context.templateBuilder == nullptr && isVisualEdge(parent, memberId)) {
                if (auto* parentUi = ::Aero::TryCast<UIElement>(parent)) {
                    if (auto* childUi = ::Aero::TryCast<UIElement>(child.Get())) {
                        bool alreadyExists = false;
                        for (const auto& edge : context.visualContent->mountEdges) {
                            if (edge.parent == parentUi && edge.child == childUi) {
                                alreadyExists = true;
                                break;
                            }
                        }
                        if (!alreadyExists) {
                            static_cast<void>(context.visualContent->AddNode(*parentUi));
                            static_cast<void>(context.visualContent->AddNode(*childUi));
                            context.visualContent->mountEdges.PushBack({ parentUi, childUi, {} });
                            context.visualContent->contentEdges.PushBack({
                                Base::Ref<Base::Object>::FromBorrowed(*parent),
                                child,
                                runtime,
                                memberId,
                                isPropertyEdge
                            });
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
            if (context.templateBuilder != nullptr && slot == 0U &&
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

            Base::Result<bool> resStatus = resolveObjectIfStaticResource(srcSlot, nullptr, true);
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

            Base::Result<bool> resStatus = resolveObjectIfStaticResource(childSlot, nullptr, true);
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
            Base::StringView name = (nameIdx < program.stringPool.Size())
                ? program.stringPool[nameIdx].View()
                : Base::StringView{};
            if (!name.Empty()) {
                activeNames->Register(name, *outObjects[slot]);
            }
            if (context.templateBuilder != nullptr && visuals != nullptr && slot < visuals->Size() && (*visuals)[slot] != nullptr) {
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
            Base::StringView name = (nameIdx < program.stringPool.Size())
                ? program.stringPool[nameIdx].View()
                : Base::StringView{};
            if (!name.Empty()) {
                activeNames->Register(name, *outObjects[childSlot]);
            }
            if (context.templateBuilder != nullptr && visuals != nullptr) {
                if (parentSlot >= visuals->Size() || (*visuals)[parentSlot] == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        "Visual template child has a non-Visual parent");
                }
                if (childSlot < visuals->Size() && (*visuals)[childSlot] != nullptr) {
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
            Base::StringView name = (nameIdx < program.stringPool.Size())
                ? program.stringPool[nameIdx].View()
                : Base::StringView{};
            if (!name.Empty()) {
                activeNames->Register(name, *outObjects[slot]);
                if (context.templateBuilder != nullptr) {
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
                activeNames->Register(name, *outObjects[slot]);
                if (!outObjects.Empty() && outObjects[0]) {
                    if (auto* rootFe = ::Aero::TryCast<FrameworkElement>(outObjects[0].Get())) {
                        rootFe->RegisterName(name, *outObjects[slot]);
                    }
                    ::Aero::Meta::ApplyBoundFields(*outObjects[0], name, outObjects[slot].Get());
                }
                if (auto* vsg = ::Aero::TryCast<VisualStateGroup>(outObjects[slot].Get())) {
                    vsg->SetName(name);
                } else if (auto* vs = ::Aero::TryCast<VisualState>(outObjects[slot].Get())) {
                    vs->SetName(name);
                }
                if (runtime != nullptr) {
                    const Meta::PropertyInfo* prop = runtime->Types().FindProperty(
                        outObjects[slot]->RuntimeType(), "Name", false);
                    if (prop != nullptr && runtime->CanWriteProperty(prop->Id())) {
                        static_cast<void>(runtime->SetProperty(*outObjects[slot], prop->Id(), Meta::Value(name)));
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
                            Base::WeakRef<Base::Object>(Base::Ref<Base::Object>::TryFromBorrowed(*codeBehind)),
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
            services.baseUri = (context.loadContext != nullptr && context.loadContext->baseUri != nullptr)
                ? context.loadContext->baseUri
                : (!program.baseUri.Empty() ? &program.baseUri : nullptr);
            if (runtime != nullptr) {
                if (const auto* propInfo = runtime->Types().FindProperty(propHandle)) {
                    services.targetValueType = propInfo->ValueType();
                }
            }
            if (services.targetValueType == Meta::InvalidTypeId) {
                if (auto* depObj = ::Aero::TryCast<DependencyObject>(outObjects[slot].Get())) {
                    if (DependencyObjectAccess::HasPropertyRegistry(*depObj)) {
                        if (const auto* dp = DependencyObjectAccess::PropertyRegistry(*depObj).Find(Meta::DependencyPropertyHandle{propHandle})) {
                            services.targetValueType = dp->ValueType();
                        }
                    }
                }
            }
            services.namespaces = NamespaceScope(
                +[](void* ctx, Base::StringView prefix) noexcept -> Base::Result<Base::StringView> {
                    const auto* prog = static_cast<const XamlBytecodeProgram*>(ctx);
                    for (std::uint32_t i = prog->namespaces.Size(); i > 0U; --i) {
                        if (prog->namespaces[i - 1U].prefix.View() == prefix) {
                            return prog->namespaces[i - 1U].uri.View();
                        }
                    }
                    if (prefix.Empty()) return Meta::AeroNamespaceUri();
                    if (prefix == "x") return Base::StringView("http://schemas.microsoft.com/winfx/2006/xaml");
                    return Base::Status::Failure(Base::ErrorCode::NotFound, "XAML namespace prefix not found");
                },
                const_cast<XamlBytecodeProgram*>(&program));
            services.nameScope = activeNames;
            services.effectiveValues = activeEffectiveValues;
            services.bindings = context.bindings != nullptr ? context.bindings : (context.loadContext != nullptr ? context.loadContext->bindings : nullptr);
            services.fallbackResources = context.documentResources != nullptr
                ? context.documentResources
                : (context.loadContext != nullptr ? context.loadContext->fallbackResources : nullptr);

            Base::Vector<const ResourceDictionary*> ambientDicts;
            for (std::uint32_t i = ambientDictStack.Size(); i > 0U; --i) {
                if (ambientDictStack[i - 1U] != nullptr) {
                    ambientDicts.PushBack(ambientDictStack[i - 1U]);
                }
            }
            if (context.documentResources != nullptr) {
                ambientDicts.PushBack(context.documentResources);
            }
            if (context.loadContext != nullptr && context.loadContext->resources != nullptr) {
                ambientDicts.PushBack(context.loadContext->resources);
            }
            services.ambientResourceChain = Base::Span<const ResourceDictionary* const>(ambientDicts.Data(), ambientDicts.Size());

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

            const Meta::TypeInfo* extInfo = (context.schema != nullptr && context.schema->Metadata() != nullptr)
                ? context.schema->Types().FindType(extTypeId) : nullptr;
            const bool isStaticResourceExt = extInfo != nullptr &&
                (extInfo->Name() == "StaticResourceExtension" || extInfo->Name() == "StaticResource");
            if (isStaticResourceExt) {
                Base::Result<Aero::Value> res = resolveResource(arguments);
                if (!res) {
                    DeferredStaticExtension dse;
                    dse.slot = slot;
                    dse.propHandle = propHandle;
                    static_cast<void>(dse.resourceKey.Assign(arguments));
                    dse.localScope = !ambientDictStack.Empty() ? ambientDictStack.Back() : nullptr;
                    deferredStaticExtensions.PushBack(std::move(dse));
                    break;
                }
            }

            Base::Result<ProvidedValue> provided = context.schema->ProvideMarkupExtensionValue(
                extTypeId, arguments, services);
            if (!provided) {
                if (context.ignoreUnresolvedResources) break;
                return provided.GetStatus();
            }

            if (provided.Value().kind == ProvidedValueKind::Value) {
                Base::Result<void> appStatus = ApplyPropertyToObject(
                    *outObjects[slot], propHandle, provided.Value().value,
                    runtime, context.templateBuilder);
                if (!appStatus) return appStatus.GetStatus();
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
            auto* dict = ResolveObjectResourceDictionary(*outObjects[dictSlot], context.schema);
            if (dict == nullptr) {
                return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "Target object is not a ResourceDictionary");
            }
            if (valSlot < outObjects.Size() && outObjects[valSlot]) {
                if (auto* childDict = ::Aero::TryCast<Aero::ResourceDictionary>(outObjects[valSlot].Get())) {
                    dict->AddMerged(*childDict);
                    break;
                }
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
            const Aero::ResourceDictionary* dict = nullptr;
            if (slot < outObjects.Size() && outObjects[slot]) {
                dict = ResolveObjectResourceDictionary(*outObjects[slot], context.schema);
            }
            ambientDictStack.PushBack(dict);
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
            if (auto* subContainer = ::Aero::TryCast<XamlBytecodeContainer>(program.subPrograms[subIdx].Get())) {
                subContainer->program.ambientResourceOwners.Clear();
                for (const auto& owner : program.ambientResourceOwners) {
                    subContainer->program.ambientResourceOwners.PushBack(owner);
                }
                for (const auto& obj : outObjects) {
                    if (obj && ResolveObjectResourceDictionary(*obj, context.schema) != nullptr) {
                        subContainer->program.ambientResourceOwners.PushBack(Base::WeakRef<Base::Object>(obj));
                    }
                }
                if (subContainer->program.fallbackResources.Size() == 0U &&
                    subContainer->program.fallbackResources.MergedDictionaryCount() == 0U) {
                    if (context.loadContext != nullptr && context.loadContext->fallbackResources != nullptr) {
                        subContainer->program.fallbackResources = context.loadContext->fallbackResources->Share();
                    } else if (context.loadContext != nullptr && context.loadContext->resources != nullptr) {
                        subContainer->program.fallbackResources = context.loadContext->resources->Share();
                    } else if (program.fallbackResources.Size() > 0U ||
                               program.fallbackResources.MergedDictionaryCount() > 0U) {
                        subContainer->program.fallbackResources = program.fallbackResources.Share();
                    }
                }
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
            bool hasPendingWrites = !deferredWrites.Empty() || !deferredStaticExtensions.Empty();
            if (!hasPendingWrites) {
                if (auto* style = ::Aero::TryCast<Aero::Style>(outObjects[slot].Get())) {
                    if (style->GetBasedOn() != nullptr && !style->GetBasedOn()->GetIsSealed()) {
                        hasPendingWrites = true;
                    }
                } else if (auto* ct = ::Aero::TryCast<Controls::ControlTemplate>(outObjects[slot].Get())) {
                    if (auto* baseCt = ::Aero::TryCast<const Controls::ControlTemplate>(ct->GetBasedOn())) {
                        if (!baseCt->GetIsSealed()) {
                            hasPendingWrites = true;
                        }
                    }
                    if (!hasPendingWrites &&
                        ::Aero::Controls::FrameworkTemplateState::Factory(*ct) == &BuildCompiledTemplate) {
                        auto* subProg = static_cast<XamlBytecodeProgram*>(
                            ::Aero::Controls::FrameworkTemplateState::FactoryContext(*ct));
                        if (subProg != nullptr &&
                            !subProg->vsmPrograms.Empty() &&
                            ::Aero::Controls::FrameworkTemplateState::AuthoredVisualStateGroups(*ct).Empty() &&
                            ::Aero::Controls::FrameworkTemplateState::VisualStateGroups(*ct).Empty()) {
                            hasPendingWrites = true;
                        }
                    }
                }
            }
            if (hasPendingWrites) {
                deferredSealSlots.PushBack(slot);
                break;
            }
            if (auto sealStatus = sealTemplateOrStyle(outObjects[slot].Get()); !sealStatus) {
                return sealStatus.GetStatus();
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
                extServices.nameScope = activeNames;
                extServices.effectiveValues = activeEffectiveValues;
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
                    Base::Result<void> extAppStatus = ApplyPropertyToObject(
                        *targetObj, propHandle, prov.Value(), runtime, context.templateBuilder);
                    if (!extAppStatus) return extAppStatus.GetStatus();
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
    if (context.loadContext != nullptr &&
        context.loadContext->finalize != nullptr &&
        context.templateBuilder == nullptr &&
        !outObjects.Empty() && outObjects[0]) {
        LoaderResult tempResult;
        tempResult.root = outObjects[0];
        tempResult.metadata = runtime;
        if (context.documentResources != nullptr &&
            ::Aero::TryCast<Aero::ResourceDictionary>(outObjects[0].Get()) == nullptr) {
            tempResult.resources = context.documentResources->Share();
        }
        if (context.visualContent != nullptr) {
            tempResult.visualContent.nodes = context.visualContent->nodes;
        }
        Base::Result<void> finalized = context.loadContext->finalize(
            tempResult, context.loadContext->finalizeContext);
        if (!finalized) {
            return finalized.GetStatus();
        }
        if (context.documentResources != nullptr) {
            if (auto* dict = ::Aero::TryCast<Aero::ResourceDictionary>(outObjects[0].Get())) {
                *context.documentResources = dict->Share();
            } else {
                *context.documentResources = tempResult.resources.Share();
            }
        }
        if (context.finalizedCanonicalUri != nullptr) {
            *context.finalizedCanonicalUri = std::move(tempResult.canonicalUri);
        }
        if (context.finalizedDependencies != nullptr) {
            *context.finalizedDependencies = std::move(tempResult.dependencies);
        }
        if (context.finalizedByVM != nullptr) {
            *context.finalizedByVM = true;
        }
    }
    for (const auto& dw : deferredWrites) {
        Base::Result<bool> resStatus = resolveObjectIfStaticResource(dw.childSlot, dw.localScope);
        if (!resStatus) {
            if (context.ignoreUnresolvedResources) continue;
            return resStatus.GetStatus();
        }
        if (!resStatus.Value()) {
            if (context.ignoreUnresolvedResources) continue;
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
    for (const auto& dse : deferredStaticExtensions) {
        if (dse.slot < outObjects.Size() && outObjects[dse.slot]) {
            Base::Result<Aero::Value> res = resolveResource(dse.resourceKey.View(), dse.localScope);
            if (!res) {
                if (context.ignoreUnresolvedResources) continue;
                return res.GetStatus();
            }
            Base::Result<void> dseAppStatus = ApplyPropertyToObject(
                *outObjects[dse.slot], dse.propHandle, res.Value(),
                runtime, context.templateBuilder);
            if (!dseAppStatus) return dseAppStatus.GetStatus();
        }
    }
    {
        Base::Vector<std::uint16_t> remainingSealSlots = std::move(deferredSealSlots);
        while (!remainingSealSlots.Empty()) {
            Base::Vector<std::uint16_t> nextRound;
            Base::Status lastError = Base::Status::Ok();
            bool progressed = false;
            for (std::uint16_t sealSlot : remainingSealSlots) {
                if (sealSlot < outObjects.Size() && outObjects[sealSlot]) {
                    Base::Result<void> sealRes = sealTemplateOrStyle(outObjects[sealSlot].Get());
                    if (sealRes) {
                        progressed = true;
                    } else if (sealRes.GetStatus().code == Base::ErrorCode::CycleDetected) {
                        return sealRes.GetStatus();
                    } else {
                        lastError = sealRes.GetStatus();
                        nextRound.PushBack(sealSlot);
                    }
                }
            }
            if (!nextRound.Empty() && !progressed) {
                return lastError;
            }
            remainingSealSlots = std::move(nextRound);
        }
    }
    if (!context.ignoreUnresolvedResources) {
        for (PendingDeferredExtension& item : deferredExtensions) {
            EffectServices effectServices;
            effectServices.effectiveValues = activeEffectiveValues;
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
                    if (!bound) {
                        if (context.loadContext == nullptr && effectServices.effectiveValues == nullptr) {
                            continue;
                        }
                        return bound.GetStatus();
                    }
                }
                if (effect.prepare != nullptr && activeNames != nullptr) {
                    Base::Result<void> prepared = effect.prepare(effect.context, *activeNames);
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
    }
    return {};
}

Base::Result<void> XamlVM::Execute(
    const XamlBytecodeProgram& program,
    Meta::Registry& runtime,
    Controls::TemplateBuilder* context,
    Base::Vector<Base::Ref<Base::Object>>& outObjects,
    Base::Vector<Media::Visual*>* outVisuals) noexcept {
    XamlVMContext vmContext;
    vmContext.runtime = &runtime;
    vmContext.templateBuilder = context;
    vmContext.schema = program.schema;
    if (context != nullptr) {
        vmContext.bindings = &context->Bindings();
    }
    Base::Vector<Media::Visual*> localVisuals;
    if (outVisuals == nullptr) {
        localVisuals.Resize(program.slotCount, nullptr);
        outVisuals = &localVisuals;
    }
    return Execute(program, vmContext, outObjects, outVisuals);
}

Base::Result<void> XamlVM::ExecuteDocument(
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
    vmContext.finalizedCanonicalUri = &outResult.canonicalUri;
    vmContext.finalizedDependencies = &outResult.dependencies;
    vmContext.finalizedByVM = &outResult.finalizedByVM;

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

Base::Result<void> MaterializeCompiledVsmPrograms(
    Controls::ControlTemplate& controlTemplate,
    XamlBytecodeProgram& subProg,
    Meta::Registry& runtime,
    const Schema* schema,
    bool ignoreUnresolvedResources) noexcept {
    Base::Vector<Base::Ref<Base::Object>> materializedGroups;
    for (const auto& vsmProgObj : subProg.vsmPrograms) {
        auto* vsmContainer = ::Aero::TryCast<XamlBytecodeContainer>(vsmProgObj.Get());
        if (vsmContainer == nullptr) continue;
        vsmContainer->program.runtime = &runtime;
        if (schema != nullptr) {
            vsmContainer->program.schema = schema;
        }
        if (subProg.properties != nullptr) {
            vsmContainer->program.properties = subProg.properties;
        }
        vsmContainer->program.ambientResourceOwners.Clear();
        for (const auto& owner : subProg.ambientResourceOwners) {
            vsmContainer->program.ambientResourceOwners.PushBack(owner);
        }
        if (subProg.fallbackResources.Size() > 0U ||
            subProg.fallbackResources.MergedDictionaryCount() > 0U) {
            vsmContainer->program.fallbackResources = subProg.fallbackResources.Share();
        }
        Base::Vector<Base::Ref<Base::Object>> vsmObjects;
        XamlVMContext vmCtx;
        vmCtx.runtime = &runtime;
        vmCtx.schema = schema;
        vmCtx.documentResources = &controlTemplate.GetResources();
        vmCtx.ignoreUnresolvedResources = ignoreUnresolvedResources;
        Base::Result<void> executed = XamlVM::Execute(vsmContainer->program, vmCtx, vsmObjects, nullptr);
        if (!executed) {
            return executed.GetStatus();
        }
        if (!vsmObjects.Empty() && vsmObjects[0]) {
            if (::Aero::TryCast<VisualStateGroup>(vsmObjects[0].Get()) != nullptr) {
                materializedGroups.PushBack(vsmObjects[0]);
            } else if (auto* vsgColl = ::Aero::TryCast<VisualStateGroupCollection>(vsmObjects[0].Get())) {
                for (const auto& item : vsgColl->GetItems()) {
                    if (item) {
                        materializedGroups.PushBack(Base::Ref<Base::Object>(item));
                    }
                }
            }
        }
    }
    for (const auto& group : materializedGroups) {
        ::Aero::Controls::FrameworkTemplateState::AddAuthoredVisualStateGroup(
            controlTemplate, group);
    }
    return {};
}

Base::Result<void> BindCompiledSubProgramToTemplate(
    Base::Object& target,
    const Base::Ref<Base::Object>& subProgramRef,
    Meta::Registry* runtime,
    const Schema* schema) noexcept {
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
    if (schema != nullptr) {
        subProg.schema = schema;
    }
    if (runtime == nullptr && schema != nullptr) {
        runtime = schema->Metadata();
    }
    if (runtime == nullptr) {
        runtime = Meta::CurrentObjectFactory().metadata;
    }
    if (runtime != nullptr) {
        subProg.runtime = runtime;
    }
    if (auto* depObj = ::Aero::TryCast<DependencyObject>(&target)) {
        if (DependencyObjectAccess::HasPropertyRegistry(*depObj)) {
            subProg.properties = &DependencyObjectAccess::PropertyRegistry(*depObj);
        }
    }
    if (subProg.properties == nullptr) {
        subProg.properties = Meta::CurrentObjectFactory().dependencyProperties;
    }
    Meta::DependencyPropertyRegistry* properties = subProg.properties;

    if (auto* controlTemplate = ::Aero::TryCast<Controls::ControlTemplate>(&target)) {
        if (controlTemplate->GetTargetType() == Meta::InvalidTypeId) {
            Meta::TypeId tt = (subProg.ambientTargetType != Meta::InvalidTypeId)
                ? subProg.ambientTargetType
                : Controls::Control::StaticTypeId();
            static_cast<void>(::Aero::Controls::FrameworkTemplateState::SetTargetType(*controlTemplate, tt));
        }

        if (!subProg.vsmPrograms.Empty() && runtime != nullptr) {
            static_cast<void>(MaterializeCompiledVsmPrograms(
                *controlTemplate, subProg, *runtime, schema, /*ignoreUnresolvedResources=*/false));
        }

        bool hasOwnTree = false;
        for (const auto& ins : subProg.instructions) {
            if (ins.op == XamlOpCode::CreateObject || ins.op == XamlOpCode::AdoptOrCreateRoot) {
                hasOwnTree = true;
                break;
            }
        }
        if (!hasOwnTree && controlTemplate->GetBasedOn() != nullptr) {
            auto* templateBase = ::Aero::TryCast<const Controls::ControlTemplate>(controlTemplate->GetBasedOn());
            if (templateBase != nullptr) {
                const auto baseFactory = ::Aero::Controls::FrameworkTemplateState::Factory(*templateBase);
                void* const baseFactoryContext = ::Aero::Controls::FrameworkTemplateState::FactoryContext(*templateBase);
                if (baseFactoryContext != nullptr && runtime != nullptr && properties != nullptr) {
                    const auto* baseProgram = static_cast<const XamlBytecodeProgram*>(baseFactoryContext);
                    static_cast<void>(CompileInheritedControlTemplate(*controlTemplate, *baseProgram, *runtime, *properties));
                }
                Base::Ref<Base::Object> baseOwner = ::Aero::Controls::FrameworkTemplateState::FactoryOwner(*templateBase);
                static_cast<void>(::Aero::Controls::FrameworkTemplateState::ConfigureFactory(
                    *controlTemplate, baseFactory, baseFactoryContext, std::move(baseOwner)));
                return {};
            }
        }

        Base::Result<void> configured = ::Aero::Controls::FrameworkTemplateState::ConfigureFactory(
            *controlTemplate,
            &BuildCompiledTemplate,
            &subProg,
            subProgramRef);
        if (!configured) return configured.GetStatus();

        return {};
    }

    if (auto* frameworkTemplate = ::Aero::TryCast<Aero::FrameworkTemplate>(&target)) {
        Base::Result<void> configured = ::Aero::Controls::FrameworkTemplateState::Configure(
            *frameworkTemplate,
            &BuildCompiledDeferredTemplate,
            &subProg,
            subProgramRef);
        if (!configured) return configured.GetStatus();

        if (runtime != nullptr && properties != nullptr) {
            if (subProg.dataTemplateTriggers.Empty() &&
                !::Aero::Controls::FrameworkTemplateState::AuthoredTriggers(*frameworkTemplate).Empty()) {
                for (const Base::Ref<Base::Object>& authored :
                     ::Aero::Controls::FrameworkTemplateState::AuthoredTriggers(*frameworkTemplate)) {
                    if (!authored) continue;
                    if (authored->RuntimeType() == DataTrigger::StaticTypeId() ||
                        authored->RuntimeType() == MultiDataTrigger::StaticTypeId() ||
                        authored->RuntimeType() == Trigger::StaticTypeId()) {
                        Base::Ref<TriggerBase> retained =
                            Base::Ref<TriggerBase>::TryFromBorrowed(
                                static_cast<TriggerBase&>(*authored));
                        if (!retained) continue;
                        Base::Result<CompiledTriggerPlan> plan = CompileTriggerPlan(
                            std::move(retained), subProg, *runtime, *properties);
                        if (!plan) return plan.GetStatus();
                        subProg.dataTemplateTriggers.PushBack(std::move(plan).Value());
                    }
                }
            }
        }

        return {};
    }

    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        "Target object is not a template in BindCompiledSubProgramToTemplate");
}

Base::Result<void> CompileNodesToBytecode(
    Base::Span<const Node> nodes,
    std::size_t& nodeIndex,
    const Schema& schema,
    const Base::ResourceUri* baseUri,
    Meta::TypeId ambientTargetType,
    bool isSubProgram,
    XamlBytecodeProgram& outProgram,
    bool stopOnRootEnd = false) noexcept {
    outProgram.ambientTargetType = ambientTargetType;
    outProgram.schema = &schema;
    if (baseUri != nullptr) {
        outProgram.baseUri = *baseUri;
    }
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
    std::uint16_t nextSlot = 0U;

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

            if (isSubProgram && !objectStack.Empty() && !memberStack.Empty() &&
                memberStack.Back().parentSlot == objectStack.Back().slot &&
                memberStack.Back().memberId == VisualStateManager::VisualStateGroupsProperty.Handle().value) {
                auto vsmContainer = Base::MakeRef<XamlBytecodeContainer>();
                if (!vsmContainer) {
                    return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM creating VSM bytecode container");
                }
                for (const auto& nsEntry : outProgram.namespaces) {
                    XamlBytecodeProgram::NamespaceEntry copied;
                    static_cast<void>(copied.prefix.Assign(nsEntry.prefix.View()));
                    static_cast<void>(copied.uri.Assign(nsEntry.uri.View()));
                    vsmContainer->program.namespaces.PushBack(std::move(copied));
                }
                Base::Result<void> vsmRes = CompileNodesToBytecode(
                    nodes, nodeIndex, schema, baseUri, ambientTargetType, false, vsmContainer->program, true);
                if (!vsmRes) return vsmRes.GetStatus();
                outProgram.vsmPrograms.PushBack(std::move(vsmContainer));
                break;
            }

            if (!objectStack.Empty() && objectStack.Back().isTemplate &&
                (memberStack.Empty() || memberStack.Back().parentSlot != objectStack.Back().slot ||
                 (!memberStack.Back().isTriggers && !memberStack.Back().isResources))) {
                auto subContainer = Base::MakeRef<XamlBytecodeContainer>();
                for (const auto& nsEntry : outProgram.namespaces) {
                    XamlBytecodeProgram::NamespaceEntry copied;
                    static_cast<void>(copied.prefix.Assign(nsEntry.prefix.View()));
                    static_cast<void>(copied.uri.Assign(nsEntry.uri.View()));
                    subContainer->program.namespaces.PushBack(std::move(copied));
                }
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
            } else if (memName == "Resources" || (memName.SizeBytes() >= 10U && memName.Substr(memName.SizeBytes() - 10U, 10U) == ".Resources")) {
                mf.isResources = true;
                if (!objectStack.Empty() && !objectStack.Back().hasResourceScope) {
                    objectStack.Back().hasResourceScope = true;
                    builder.EmitPushAmbientResources(parentSlot);
                }
                mf.memberId = node.CompiledMemberId();
                if (mf.memberId == Meta::InvalidMemberId && !objectStack.Empty()) {
                    Base::Result<ResolvedMember> resolved = schema.ResolveMember(
                        objectStack.Back().typeId, node.Name(), memberSyntax);
                    if (resolved) {
                        mf.memberId = resolved.Value().id;
                        mf.valueType = resolved.Value().valueType;
                        mf.kind = resolved.Value().kind;
                    }
                }
            } else if (memName == "Triggers" || (memName.SizeBytes() >= 9U && memName.Substr(memName.SizeBytes() - 9U, 9U) == ".Triggers") ||
                       memName == "VisualStateGroups" || (memName.SizeBytes() >= 18U && memName.Substr(memName.SizeBytes() - 18U, 18U) == ".VisualStateGroups")) {
                mf.isTriggers = true;
                mf.memberId = node.CompiledMemberId();
                if (mf.memberId == Meta::InvalidMemberId && !objectStack.Empty()) {
                    Base::Result<ResolvedMember> resolved = schema.ResolveMember(
                        objectStack.Back().typeId, node.Name(), memberSyntax);
                    if (resolved) {
                        mf.memberId = resolved.Value().id;
                        mf.valueType = resolved.Value().valueType;
                        mf.kind = resolved.Value().kind;
                    }
                }
            } else if (memName == "TargetType") {
                mf.isTargetType = true;
                mf.memberId = node.CompiledMemberId();
                if (mf.memberId == Meta::InvalidMemberId && !objectStack.Empty()) {
                    Base::Result<ResolvedMember> resolved = schema.ResolveMember(
                        objectStack.Back().typeId, node.Name(), memberSyntax);
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
                        objectStack.Back().typeId, node.Name(), memberSyntax);
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

            auto makeConvServices = [&](Meta::TypeId targetObjType, Meta::MemberId targetMem) noexcept -> ExtensionServices {
                ExtensionServices convServices;
                convServices.schema = &schema;
                convServices.baseUri = baseUri;
                convServices.targetObjectType = targetObjType;
                convServices.targetMember = targetMem;
                convServices.namespaces = NamespaceScope(
                    +[](void* ctx, Base::StringView prefix) noexcept -> Base::Result<Base::StringView> {
                        const auto* prog = static_cast<const XamlBytecodeProgram*>(ctx);
                        for (std::uint32_t i = prog->namespaces.Size(); i > 0U; --i) {
                            if (prog->namespaces[i - 1U].prefix.View() == prefix) {
                                return prog->namespaces[i - 1U].uri.View();
                            }
                        }
                        if (prefix.Empty()) return Meta::AeroNamespaceUri();
                        if (prefix == "x") return Base::StringView("http://schemas.microsoft.com/winfx/2006/xaml");
                        return Base::Status::Failure(Base::ErrorCode::NotFound, "XAML namespace prefix not found");
                    },
                    &outProgram);
                return convServices;
            };

            if (!hasMemberForCurrent) {
                if (!node.IsFromAttribute() && !TrimAscii(node.Value()).Empty()) {
                    const ObjectFrame& currentObj = objectStack.Back();
                    if (currentObj.isValueNode) {
                        ExtensionServices convServices = makeConvServices(currentObj.typeId, Meta::InvalidMemberId);
                        Base::Result<Meta::Value> converted = schema.ConvertText(
                            currentObj.typeId, node.Value(), &convServices);
                        if (converted) {
                            builder.EmitSetSlotValue(currentObj.slot, converted.Value());
                            break;
                        }
                    }
                    Base::Result<ResolvedMember> contentMember = schema.ResolveContentMember(currentObj.typeId);
                    if (contentMember) {
                        const ResolvedMember& mem = contentMember.Value();
                        Meta::TypeId targetType = (mem.valueType != Meta::InvalidTypeId) ? mem.valueType : Meta::TypeOf<Base::String>();
                        ExtensionServices convServices = makeConvServices(currentObj.typeId, mem.id);
                        Base::Result<Meta::Value> converted = schema.ConvertText(targetType, node.Value(), &convServices);
                        if (converted) {
                            builder.EmitSetPropertyValue(currentObj.slot, Meta::DependencyPropertyHandle{mem.id}, converted.Value());
                            break;
                        }
                    }
                    const Meta::PropertyInfo* textProp = schema.Types().FindProperty(currentObj.typeId, Base::StringView("Text"), true);
                    if (textProp != nullptr) {
                        Meta::TypeId targetType = (textProp->ValueType() != Meta::InvalidTypeId) ? textProp->ValueType() : Meta::TypeOf<Base::String>();
                        ExtensionServices convServices = makeConvServices(currentObj.typeId, textProp->Id());
                        Base::Result<Meta::Value> converted = schema.ConvertText(targetType, node.Value(), &convServices);
                        if (converted) {
                            builder.EmitSetPropertyValue(currentObj.slot, Meta::DependencyPropertyHandle{textProp->Id()}, converted.Value());
                            break;
                        }
                    }
                    const Meta::PropertyInfo* contentProp = schema.Types().FindProperty(currentObj.typeId, Base::StringView("Content"), true);
                    if (contentProp != nullptr) {
                        Meta::TypeId targetType = (contentProp->ValueType() != Meta::InvalidTypeId) ? contentProp->ValueType() : Meta::TypeOf<Base::String>();
                        ExtensionServices convServices = makeConvServices(currentObj.typeId, contentProp->Id());
                        Base::Result<Meta::Value> converted = schema.ConvertText(targetType, node.Value(), &convServices);
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
                        Base::StringView extPrefix{};
                        Base::StringView extLocalName = extName;
                        for (std::uint32_t i = 0U; i < extName.SizeBytes(); ++i) {
                            if (extName[i] == ':') {
                                extPrefix = TrimAscii(extName.Substr(0U, i));
                                extLocalName = TrimAscii(extName.Substr(i + 1U, extName.SizeBytes() - i - 1U));
                                break;
                            }
                        }
                        char extWithSuffix[64];
                        const int extLen = std::snprintf(extWithSuffix, sizeof(extWithSuffix), "%.*sExtension",
                            static_cast<int>(extLocalName.SizeBytes()), extLocalName.Data());
                        const Base::StringView extWithSuffixView = (extLen > 0 && extLen < static_cast<int>(sizeof(extWithSuffix)))
                            ? Base::StringView(extWithSuffix, static_cast<std::uint32_t>(extLen))
                            : Base::StringView{};
                        Base::Result<const Meta::TypeInfo*> resolvedExt = Base::Status::Failure(Base::ErrorCode::NotFound, "ext not found");
                        if (!extPrefix.Empty()) {
                            for (std::uint32_t nsIdx = outProgram.namespaces.Size(); nsIdx > 0U; --nsIdx) {
                                if (outProgram.namespaces[nsIdx - 1U].prefix.View() == extPrefix) {
                                    resolvedExt = schema.ResolveType(outProgram.namespaces[nsIdx - 1U].uri.View(), extWithSuffixView);
                                    if (!resolvedExt) {
                                        resolvedExt = schema.ResolveType(outProgram.namespaces[nsIdx - 1U].uri.View(), extLocalName);
                                    }
                                    break;
                                }
                            }
                        }
                        if (!resolvedExt) {
                            resolvedExt = schema.ResolveType(
                                Meta::AeroNamespaceUri(), extWithSuffixView);
                        }
                        if (!resolvedExt) {
                            resolvedExt = schema.ResolveType(
                                Base::StringView("http://schemas.microsoft.com/winfx/2006/xaml"), extWithSuffixView);
                        }
                        if (!resolvedExt) {
                            resolvedExt = schema.ResolveType(
                                Meta::AeroNamespaceUri(), extLocalName);
                        }
                        if (!resolvedExt) {
                            resolvedExt = schema.ResolveType(
                                Base::StringView("http://schemas.microsoft.com/winfx/2006/xaml"), extLocalName);
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
                Base::StringView prefix{};
                Base::StringView localName = typeName;
                for (std::uint32_t i = 0U; i < typeName.SizeBytes(); ++i) {
                    if (typeName[i] == ':') {
                        prefix = TrimAscii(typeName.Substr(0U, i));
                        localName = TrimAscii(typeName.Substr(i + 1U, typeName.SizeBytes() - i - 1U));
                        break;
                    }
                }
                Base::Result<const Meta::TypeInfo*> resolved = Base::Status::Failure(Base::ErrorCode::NotFound, "type not found");
                if (!prefix.Empty()) {
                    for (std::uint32_t nsIdx = namespaces.Size(); nsIdx > 0U; --nsIdx) {
                        if (namespaces[nsIdx - 1U].prefix.View() == prefix) {
                            resolved = schema.ResolveType(namespaces[nsIdx - 1U].uri.View(), localName);
                            break;
                        }
                    }
                }
                if (!resolved) {
                    resolved = schema.ResolveType(Meta::AeroNamespaceUri(), localName);
                }
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
                    if (objectStack.Back().isTemplate ||
                        objectStack.Back().typeId == Style::StaticTypeId()) {
                        objectStack.Back().templateTargetType = resolved.Value()->Id();
                    }
                    break;
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
        Base::Result<Value> strVal =
            Value::TryFromString(Meta::TypeOf<Base::String>(), text);
        if (!strVal) return strVal.GetStatus();
        Base::Result<Base::Ref<Controls::BoxedItemValue>> boxed =
            Base::MakeRef<Controls::BoxedItemValue>(std::move(strVal).Value());
        if (!boxed) return boxed.GetStatus();
        return Value::FromObject(
            property.ValueType(),
            Base::Ref<Base::Object>(std::move(boxed).Value()));
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
    if (value.Kind() == ValueKind::Object &&
        !value.IsNullObject() &&
        value.Type() == Binding::StaticTypeId()) {
        return value;
    }
    if (value.IsDynamicResource()) {
        return value;
    }
    if (property.AcceptsAnyValue()) {
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
    thread_local char message[384];
    const TypeInfo* target = types.FindType(targetType);
    const Base::StringView typeName =
        target != nullptr ? target->Name() : Base::StringView("<unknown>");
    std::snprintf(
        message, sizeof(message),
        "ControlTemplate %s '%.*s' was not found on TargetType '%.*s'",
        role,
        static_cast<int>(property.SizeBytes()), property.Data(),
        static_cast<int>(typeName.SizeBytes()), typeName.Data());
    return InvalidTemplateCompiler(message);
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
            if (!plan.conditions.Empty() && plan.conditions[0U].property.IsValid()) {
                const CompiledTriggerCondition& cc = plan.conditions[0U];
                Base::Object* source = (cc.sourceSlot >= 0 && static_cast<std::uint32_t>(cc.sourceSlot) < objects.Size())
                    ? objects[static_cast<std::uint32_t>(cc.sourceSlot)].Get()
                    : (templatedParent != nullptr ? static_cast<Base::Object*>(templatedParent) : static_cast<Base::Object*>(root));
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
                condition.property = cc.property;
                condition.value = cc.value;
                runtimeTrigger.conditions.PushBack(std::move(condition));
            } else {
                const auto& property = static_cast<const Trigger&>(*authored);
                Base::Object* source = nullptr;
                if (!property.GetSourceName().Empty()) {
                    source = triggerContext->FindName(property.GetSourceName());
                } else if (templatedParent != nullptr) {
                    source = templatedParent;
                } else {
                    source = root;
                }
                if (source == nullptr ||
                    !program.runtime->Types().IsDerivedFrom(
                        source->RuntimeType(),
                        DependencyObject::StaticTypeId())) {
                    return InvalidTemplateCompiler(
                        "Template action trigger source was not found");
                }
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
                DataTemplateTriggerCondition condition;
                condition.source = Base::WeakRef<Base::Object>(
                    Base::Ref<Base::Object>::FromBorrowed(*source));
                condition.dependencySource =
                    Base::WeakRef<DependencyObject>(
                        Base::Ref<DependencyObject>::FromBorrowed(
                            *static_cast<DependencyObject*>(source)));
                condition.property = sourceProperty->Handle();
                Base::Result<Value> converted = ConvertTriggerValue(
                    property.GetAuthoredValue(), sourceType, *sourceProperty,
                    *program.runtime, *program.properties);
                if (!converted) return converted.GetStatus();
                condition.value = std::move(converted).Value();
                runtimeTrigger.conditions.PushBack(std::move(condition));
            }
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
        for (const Base::Ref<Setter>& setterObject : setters) {
            if (!setterObject || setterObject->GetPropertyName().Empty()) {
                return InvalidTemplateCompiler(
                    "ControlTemplate Trigger Setter requires Property");
            }
            const Base::StringView targetName = setterObject->GetTargetName();
            TypeId targetType = controlTemplate.GetTargetType();
            if (!targetName.Empty()) {
                const std::uint32_t slot = program.FindSlotByName(targetName);
                if (slot == UINT32_MAX) {
                    if (FrameworkTemplateState::AuthoredNames(controlTemplate).Find(
                            targetName) != nullptr) {
                        continue;
                    }
                    return InvalidTemplateCompiler(
                        "ControlTemplate Trigger Setter target was not found");
                }
                targetType = program.GetSlotType(slot);
            }
            const DependencyProperty* property = ResolveTemplateProperty(
                properties, targetType, setterObject->GetPropertyName());
            if (property == nullptr) {
                return InvalidTemplateCompiler(
                    "ControlTemplate Trigger Setter property was not found");
            }
            Base::Result<Value> value = ConvertSetterValue(
                *setterObject, targetType, *property, runtime, properties);
            if (!value) return value.GetStatus();
            TemplateTriggerSetter setter;
            Base::Result<void> assigned = setter.targetName.Assign(targetName);
            if (!assigned) return assigned.GetStatus();
            setter.targetPartIndex = program.FindPartIndexByName(targetName);
            setter.property = property->Handle();
            setter.value = std::move(value).Value();
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
                const auto& sourceSetter = static_cast<const Setter&>(*setterObject);
                if (sourceSetter.GetTargetName().Empty() ||
                    sourceSetter.GetPropertyName().Empty()) {
                    return InvalidTemplateCompiler(
                        "Visual state Setter requires TargetName and Property");
                }
                const std::uint32_t slot = program.FindSlotByName(sourceSetter.GetTargetName());
                if (slot == UINT32_MAX) {
                    return InvalidTemplateCompiler(
                        "Visual state Setter target was not found");
                }
                const TypeId targetType = program.GetSlotType(slot);
                const DependencyProperty* property = ResolveTemplateProperty(
                    properties, targetType, sourceSetter.GetPropertyName());
                if (property == nullptr) {
                    return InvalidTemplateCompiler(
                        "Visual state Setter property was not found");
                }
                Base::Result<Value> value = ConvertSetterValue(
                    sourceSetter, targetType, *property, runtime, properties);
                if (!value) return value.GetStatus();

                VisualStateSetterPlan setter;
                assigned = setter.targetName.Assign(sourceSetter.GetTargetName());
                if (!assigned) return assigned.GetStatus();
                setter.targetPartIndex = program.FindPartIndexByName(sourceSetter.GetTargetName());
                setter.property = property->Handle();
                setter.value = std::move(value).Value();
                state.setters.PushBack(std::move(setter));
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
    } else if (trigger->RuntimeType() == DataTrigger::StaticTypeId()) {
        const auto& data = static_cast<const DataTrigger&>(*trigger);
        setters = data.GetAuthoredSetters();
        if (!data.GetBinding()) {
            return InvalidTemplateCompiler(
                "Template DataTrigger requires Binding");
        }
        CompiledTriggerCondition compiledCondition;
        if (!data.GetBinding()->GetElementName().Empty()) {
            const std::uint32_t slot = program.FindSlotByName(data.GetBinding()->GetElementName());
            if (slot < program.slotNames.Size()) {
                compiledCondition.sourceSlot = static_cast<std::int32_t>(slot);
            }
        }
        compiledCondition.usesDataContext =
            data.GetBinding()->GetElementName().Empty() &&
            !data.GetBinding()->GetRelativeSource();
        compiledCondition.binding = data.GetBinding();
        compiledCondition.value = data.GetAuthoredValue();
        plan.conditions.PushBack(std::move(compiledCondition));
    } else if (trigger->RuntimeType() == MultiDataTrigger::StaticTypeId()) {
        const auto& multi = static_cast<const MultiDataTrigger&>(*trigger);
        setters = multi.GetAuthoredSetters();
        for (const Base::Ref<Condition>& authoredCondition : multi.GetConditions()) {
            if (!authoredCondition || !authoredCondition->GetBinding()) {
                return InvalidTemplateCompiler(
                    "Template MultiDataTrigger requires complete Conditions");
            }
            CompiledTriggerCondition compiledCondition;
            if (!authoredCondition->GetBinding()->GetElementName().Empty()) {
                const std::uint32_t slot = program.FindSlotByName(authoredCondition->GetBinding()->GetElementName());
                if (slot < program.slotNames.Size()) {
                    compiledCondition.sourceSlot = static_cast<std::int32_t>(slot);
                }
            }
            compiledCondition.usesDataContext =
                authoredCondition->GetBinding()->GetElementName().Empty() &&
                !authoredCondition->GetBinding()->GetRelativeSource();
            compiledCondition.binding = authoredCondition->GetBinding();
            compiledCondition.value = authoredCondition->GetAuthoredValue();
            plan.conditions.PushBack(std::move(compiledCondition));
        }
    }
    for (const Base::Ref<Setter>& setter : setters) {
        if (!setter) continue;
        const std::uint32_t target = setter->GetTargetName().Empty()
            ? 0U : program.FindSlotByName(setter->GetTargetName());
        if (target >= program.slotNames.Size()) {
            return InvalidTemplateCompiler(
                "Template trigger Setter target was not found");
        }
        const TypeId targetType = program.GetSlotType(target);
        const DependencyProperty* property = ResolveTemplateProperty(
            properties, targetType, setter->GetPropertyName());
        if (property == nullptr) {
            return InvalidTemplateCompiler(
                "Template trigger Setter property was not found");
        }
        Base::Result<Value> value = ConvertSetterValue(
            *setter, targetType, *property, runtime, properties);
        if (!value) return value.GetStatus();
        CompiledTriggerSetter compiledSetter;
        compiledSetter.target = target;
        compiledSetter.property = property->Handle();
        compiledSetter.value = std::move(value).Value();
        plan.setters.PushBack(std::move(compiledSetter));
    }
    return plan;
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

Base::Result<void> BuildCompiledTemplate(
    TemplateBuilder& context,
    void* factoryContext) noexcept {
    const auto* program = static_cast<const XamlBytecodeProgram*>(factoryContext);
    if (program == nullptr || !program->IsValid()) {
        return InvalidTemplateCompiler("Compiled template bytecode is invalid");
    }

    Base::Vector<Base::Ref<Base::Object>> objects;
    Base::Result<void> executed = XamlVM::Execute(
        *program, *program->runtime, &context, objects, nullptr);
    if (!executed) return executed.GetStatus();

    return AttachCompiledTemplateTriggers(
        *program, objects,
        program->controlTemplateDataTriggers,
        program->controlTemplateEventTriggers,
        &context.TemplatedParent(), {}, false);
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

    XamlVMContext vmContext;
    vmContext.runtime = program->runtime;
    vmContext.schema = program->schema;
    vmContext.dataItemPayload = payload;
    vmContext.bindings = bindings;

    Base::Vector<Base::Ref<Base::Object>> objects;
    Base::Result<void> executed = XamlVM::Execute(
        *program, vmContext, objects, nullptr);
    if (!executed) return executed.GetStatus();

    Base::Ref<Base::Object> root = objects[0U];

    static const Base::Vector<Base::Ref<EventTrigger>> noEventTriggers;
    Base::Result<void> triggered = AttachCompiledTemplateTriggers(
        *program, objects,
        program->dataTemplateTriggers,
        noEventTriggers, nullptr, payload, true);
    if (!triggered) return triggered.GetStatus();

    return root;
}

} // namespace Aero::Markup

