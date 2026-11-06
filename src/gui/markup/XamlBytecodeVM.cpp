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

Base::Result<void> AppendCompiledCacheIdentity(
    Base::Vector<std::uint8_t>& output,
    const CompiledCacheIdentity& identity) noexcept {
    Base::Result<void> result = AppendU32(output, identity.cacheFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity.typeIdAlgorithmVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity.metadataSchemaFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity.metadataProgramFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity.schemaVersion);
    if (!result) return result.GetStatus();
    return AppendU64(output, identity.metadataSchemaHash);
}

Base::Result<CompiledCacheIdentity> ReadCompiledCacheIdentity(
    Decoder& decoder) noexcept {
    CompiledCacheIdentity identity;
    Base::Result<std::uint32_t> value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.cacheFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.typeIdAlgorithmVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.metadataSchemaFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.metadataProgramFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    identity.schemaVersion = value.Value();
    Base::Result<std::uint64_t> hash = decoder.ReadU64();
    if (!hash) return hash.GetStatus();
    identity.metadataSchemaHash = hash.Value();
    return identity;
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

NamespaceScope XamlBytecodeProgram::MakeNamespaceScope() const noexcept {
    return NamespaceScope(
        +[](void* ctx, Base::StringView prefix) noexcept -> Base::Result<Base::StringView> {
            const auto* prog = static_cast<const XamlBytecodeProgram*>(ctx);
            for (std::uint32_t i = prog->namespaces.Size(); i > 0U; --i) {
                if (prog->namespaces[i - 1U].prefix.View() == prefix) {
                    return prog->namespaces[i - 1U].uri.View();
                }
            }
            if (prefix.Empty()) return Meta::AeroNamespaceUri();
            if (prefix == "x") return LanguageNamespaceUri();
            return Base::Status::Failure(Base::ErrorCode::NotFound, "XAML namespace prefix not found");
        },
        const_cast<XamlBytecodeProgram*>(this));
}

void XamlBytecodeProgram::CopyNamespacesFrom(const XamlBytecodeProgram& parent) noexcept {
    for (const auto& nsEntry : parent.namespaces) {
        NamespaceEntry copied;
        static_cast<void>(copied.prefix.Assign(nsEntry.prefix.View()));
        static_cast<void>(copied.uri.Assign(nsEntry.uri.View()));
        namespaces.PushBack(std::move(copied));
    }
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
    ambientDictionaries.Clear();
    fallbackResources.Clear();
    templateOwner = nullptr;
    ambientTargetType = Meta::InvalidTypeId;
}

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

void XamlBytecodeProgram::PropagateAmbientResourcesFrom(
    const XamlBytecodeProgram& parent,
    Base::Span<const Aero::ResourceDictionary* const> ambientStack,
    Base::Span<const Base::Ref<Base::Object>> activeObjects,
    const Schema* activeSchema,
    const LoadState* loadContext) noexcept {
    ambientResourceOwners.Clear();
    ambientDictionaries.Clear();
    for (const auto& owner : parent.ambientResourceOwners) {
        ambientResourceOwners.PushBack(owner);
    }
    for (const auto& dict : parent.ambientDictionaries) {
        if (Base::Result<Aero::ResourceDictionary> shared = dict.Share()) {
            ambientDictionaries.PushBack(std::move(shared).Value());
        }
    }
    for (const Aero::ResourceDictionary* dict : ambientStack) {
        if (dict != nullptr) {
            if (Base::Result<Aero::ResourceDictionary> shared = dict->Share()) {
                ambientDictionaries.PushBack(std::move(shared).Value());
            }
        }
    }
    for (const auto& obj : activeObjects) {
        if (obj) {
            if (auto* dict = ResolveObjectResourceDictionary(*obj, activeSchema)) {
                ambientResourceOwners.PushBack(Base::WeakRef<Base::Object>(obj));
                if (Base::Result<Aero::ResourceDictionary> shared = dict->Share()) {
                    ambientDictionaries.PushBack(std::move(shared).Value());
                }
            }
        }
    }
    if (fallbackResources.Size() == 0U &&
        fallbackResources.MergedDictionaryCount() == 0U) {
        if (loadContext != nullptr && loadContext->fallbackResources != nullptr) {
            fallbackResources = loadContext->fallbackResources->Share();
        } else if (loadContext != nullptr && loadContext->resources != nullptr) {
            fallbackResources = loadContext->resources->Share();
        } else if (parent.fallbackResources.Size() > 0U ||
                   parent.fallbackResources.MergedDictionaryCount() > 0U) {
            fallbackResources = parent.fallbackResources.Share();
        }
    }
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
        auto resolveSchemaTypeByName = [&](Base::StringView candidateName) noexcept -> Meta::TypeId {
            if (context.schema == nullptr || candidateName.Empty()) {
                return Meta::InvalidTypeId;
            }
            Base::StringView prefix;
            Base::StringView localName;
            SplitRelaxedQualifiedXamlName(candidateName, prefix, localName);
            Base::Result<const Meta::TypeInfo*> resolved =
                context.schema->ResolveType(Meta::AeroNamespaceUri(), localName);
            if (!resolved) {
                resolved = context.schema->ResolveType(WpfPresentationNamespace, localName);
            }
            return (resolved && resolved.Value() != nullptr)
                ? resolved.Value()->Id()
                : Meta::InvalidTypeId;
        };

        Base::StringView typeName = key;
        const bool isTypeKey = TryUnwrapTypeExtensionLiteral(key, typeName);
        const Meta::TypeId resolvedTypeId = isTypeKey
            ? resolveSchemaTypeByName(typeName)
            : Meta::InvalidTypeId;

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
        auto lookupFrameworkElement = [&](Base::Object* owner) -> Aero::Value {
            if (auto* fe = ::Aero::TryCast<FrameworkElement>(owner)) {
                if (resolvedTypeId != Meta::InvalidTypeId) {
                    Base::Result<Aero::ResourceValue> rv = fe->FindResource(ResourceKey::FromType(resolvedTypeId));
                    if (rv && !rv.Value().IsUnset()) return rv.Value();
                }
                Base::Result<Aero::ResourceValue> rv = fe->FindResource(key);
                if (rv && !rv.Value().IsUnset()) return rv.Value();
            }
            return Aero::Value{};
        };

        if (context.templateBuilder != nullptr) {
            Aero::Value val = lookupFrameworkElement(&context.templateBuilder->TemplatedParent());
            if (!val.IsUnset()) return val;
        }
        if (context.deferredContentOwner != nullptr) {
            Aero::Value val = lookupFrameworkElement(context.deferredContentOwner);
            if (!val.IsUnset()) return val;
        }
        if (context.existingRoot) {
            Aero::Value val = lookupFrameworkElement(context.existingRoot.Get());
            if (!val.IsUnset()) return val;
        }
        if (context.loadContext != nullptr && context.loadContext->existingRoot) {
            Aero::Value val = lookupFrameworkElement(context.loadContext->existingRoot.Get());
            if (!val.IsUnset()) return val;
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
        for (std::uint32_t i = program.ambientDictionaries.Size(); i > 0U; --i) {
            Aero::Value val = lookupDict(&program.ambientDictionaries[i - 1U]);
            if (val.Kind() != Meta::ValueKind::Unset) return val;
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
                        Base::Result<void> ctTriggers = CompileAuthoredTemplateTriggers(
                            *ct, *subProg, *runtime, *propRegistry, true);
                        if (!ctTriggers) return ctTriggers.GetStatus();
                    }
                }
                return ::Aero::Controls::FrameworkTemplateState::Seal(*ct, *propRegistry);
            }
        } else if (auto* ft = ::Aero::TryCast<Aero::FrameworkTemplate>(obj)) {
            if (!ft->GetIsSealed()) {
                if (::Aero::Controls::FrameworkTemplateState::DeferredFactory(*ft) == &BuildCompiledDeferredTemplate &&
                    ::Aero::Controls::FrameworkTemplateState::FactoryContext(*ft) != nullptr &&
                    runtime != nullptr) {
                    auto* subProg = static_cast<XamlBytecodeProgram*>(
                        ::Aero::Controls::FrameworkTemplateState::FactoryContext(*ft));
                    Base::Result<void> dtTriggers = CompileAuthoredTemplateTriggers(
                        *ft, *subProg, *runtime, *propRegistry, false);
                    if (!dtTriggers) return dtTriggers.GetStatus();
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

    auto recordVisualContentEdge = [&](
        const Base::Ref<Base::Object>& parentRef,
        const Base::Ref<Base::Object>& childRef,
        std::uint64_t memberId,
        bool isPropertyEdge) noexcept {
        if (context.visualContent == nullptr || context.templateBuilder != nullptr ||
            !parentRef || !childRef || !isVisualEdge(parentRef.Get(), memberId)) {
            return;
        }
        auto* parentUi = ::Aero::TryCast<UIElement>(parentRef.Get());
        auto* childUi = ::Aero::TryCast<UIElement>(childRef.Get());
        if (parentUi == nullptr || childUi == nullptr) {
            return;
        }
        for (const auto& edge : context.visualContent->mountEdges) {
            if (edge.parent == parentUi && edge.child == childUi) {
                return;
            }
        }
        static_cast<void>(context.visualContent->AddNode(*parentUi));
        static_cast<void>(context.visualContent->AddNode(*childUi));
        context.visualContent->mountEdges.PushBack({ parentUi, childUi, {} });
        context.visualContent->contentEdges.PushBack({
            parentRef,
            childRef,
            runtime,
            memberId,
            isPropertyEdge
        });
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
        recordVisualContentEdge(outObjects[slot], outObjects[srcSlot], propHandle, true);
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
                    } else if (parent->RuntimeType() == DataTrigger::StaticTypeId()) {
                        setterTrigger->AddEnterAction(child);
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
            recordVisualContentEdge(outObjects[parentSlot], child, memberId, isPropertyEdge);
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
            Meta::Value propVal = program.valuePool[ip->operandIdx];
            if (propVal.Kind() == Meta::ValueKind::Object && !propVal.IsNullObject() && propVal.AsObject()) {
                if (auto* freezable = ::Aero::TryCast<Freezable>(propVal.AsObject().Get())) {
                    if (!freezable->IsFrozen()) {
                        if (Base::Ref<Freezable> cloned = freezable->Clone()) {
                            propVal = Meta::Value::FromObject(
                                propVal.Type(),
                                Base::Ref<Base::Object>(std::move(cloned)));
                        }
                    }
                }
            }
            static_cast<void>(ApplyPropertyToObject(
                *outObjects[slot], propHandle, propVal,
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
            services.namespaces = program.MakeNamespaceScope();
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
            for (std::uint32_t i = program.ambientDictionaries.Size(); i > 0U; --i) {
                ambientDicts.PushBack(&program.ambientDictionaries[i - 1U]);
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
            if (slot < outObjects.Size()) {
                if (outValues[slot].Kind() == Meta::ValueKind::Object && !outValues[slot].IsNullObject()) {
                    outObjects[slot] = outValues[slot].AsObject();
                } else {
                    outObjects[slot].Reset();
                }
            }
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
                subContainer->program.PropagateAmbientResourcesFrom(
                    program,
                    Base::Span<const Aero::ResourceDictionary* const>(
                        ambientDictStack.Data(), ambientDictStack.Size()),
                    Base::Span<const Base::Ref<Base::Object>>(
                        outObjects.Data(), outObjects.Size()),
                    context.schema,
                    context.loadContext);
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
        vsmContainer->program.PropagateAmbientResourcesFrom(subProg);
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
            Base::Result<void> dtTriggers = CompileAuthoredTemplateTriggers(
                *frameworkTemplate, subProg, *runtime, *properties, false);
            if (!dtTriggers) return dtTriggers.GetStatus();
        }

        return {};
    }

    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        "Target object is not a template in BindCompiledSubProgramToTemplate");
}

} // namespace Aero::Markup
