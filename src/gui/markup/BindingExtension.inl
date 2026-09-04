// ===== BindingExtension =====



// Binding markup-extension implementation.



#include <cstdio>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>

#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Markup/MarkupExtension.hpp>
#include <Aero/HierarchicalDataTemplate.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Controls/ButtonBase.hpp>
#include <Aero/Controls/ToggleButton.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Documents/Span.hpp>
#include <Aero/Documents/Run.hpp>
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/EventTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Data/BindingBase.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Style.hpp>
#include <Aero/VisualStateManager.hpp>

#include <cmath>
#include <fstream>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace Aero::Markup {
namespace WriterBindingDetail {

constexpr Base::StringView ElementNameKey("ElementName");
constexpr Base::StringView SourceKey("Source");
constexpr Base::StringView PathKey("Path");
constexpr Base::StringView ModeKey("Mode");
constexpr Base::StringView RelativeSourceKey("RelativeSource");
constexpr Base::StringView StringFormatKey("StringFormat");
constexpr Base::StringView FallbackValueKey("FallbackValue");
constexpr Base::StringView ConverterKey("Converter");
constexpr Base::StringView ConverterParameterKey("ConverterParameter");
constexpr Base::StringView UpdateSourceTriggerKey("UpdateSourceTrigger");
constexpr Base::StringView OneTimeMode("OneTime");
constexpr Base::StringView OneWayMode("OneWay");
constexpr Base::StringView TwoWayMode("TwoWay");
constexpr Base::StringView OneWayToSourceMode("OneWayToSource");
constexpr Base::StringView PropertyChangedTrigger("PropertyChanged");
constexpr Base::StringView LostFocusTrigger("LostFocus");
constexpr Base::StringView ExplicitTrigger("Explicit");
constexpr Base::StringView DefaultTrigger("Default");
constexpr Base::StringView SelfValue("Self");
constexpr Base::StringView TemplatedParentValue("TemplatedParent");
constexpr Base::StringView RelativeSourcePrefix("{RelativeSource");
constexpr Base::StringView StaticResourcePrefix("{StaticResource");

enum class RelativeSourceKind : std::uint8_t {
    None = 0U,
    Self,
    TemplatedParent,
    Ancestor
};


Base::Result<long double> ReadConstantBindingNumberImpl(
    const Meta::Value& value) noexcept {
    switch (value.Kind()) {
    case Meta::ValueKind::SignedInteger:
        return static_cast<long double>(
            value.AsSignedInteger());
    case Meta::ValueKind::UnsignedInteger:
        return static_cast<long double>(
            value.AsUnsignedInteger());
    case Meta::ValueKind::Double:
        return static_cast<long double>(
            value.AsDouble());
    default:
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Binding constant is not numeric");
    }
}

Base::Result<Meta::Value> ConvertConstantBindingValueImpl(
    const Meta::Value& value,
    Meta::TypeId targetType) noexcept {
    if (targetType == Meta::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Binding constant target type is invalid");
    }
    if (value.Type() == targetType) return value;

    Base::Result<long double> number =
        ReadConstantBindingNumberImpl(value);
    if (!number) return number.GetStatus();
    const double converted =
        static_cast<double>(number.Value());
    if (!std::isfinite(converted)) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Binding constant is outside the target range");
    }
    if (targetType ==
            Meta::TypeOf<::Aero::GridLength>()) {
        return Meta::ValueCodec<::Aero::GridLength>::Encode(
            ::Aero::GridLength::Pixel(converted));
    }
    if (targetType == Meta::TypeOf<Aero::Length>()) {
        return Meta::ValueCodec<Aero::Length>::Encode(
            Aero::Length::Pixels(converted));
    }
    if (targetType == Meta::TypeOf<double>()) {
        return Meta::Value::FromDouble(
            targetType, converted);
    }
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        "Binding constant cannot be converted to the target property");
}


Base::Result<void> ParseArguments(
    Base::StringView arguments,
    Base::StringView& elementName,
    Base::StringView& sourceResource,
    Base::StringView& path,
    Base::StringView& stringFormat,
    Base::StringView& fallbackValue,
    Base::StringView& converterResource,
    Base::StringView& converterParameter,
    Base::StringView& ancestorType,
    std::uint32_t& ancestorLevel,
    RelativeSourceKind& relativeSource,
    Data::BindingMode& mode,
    Meta::UpdateSourceTrigger& updateSourceTrigger) noexcept {
    elementName = {};
    sourceResource = {};
    path = {};
    stringFormat = {};
    fallbackValue = {};
    converterResource = {};
    converterParameter = {};
    ancestorType = {};
    ancestorLevel = 1U;
    relativeSource = RelativeSourceKind::None;
    mode = Data::BindingMode::Default;
    updateSourceTrigger = Meta::UpdateSourceTrigger::Default;

    std::uint32_t begin = 0U;
    while (begin < arguments.SizeBytes()) {
        std::uint32_t end = begin;
        std::uint32_t depth = 0U;
        char quote = '\0';
        while (end < arguments.SizeBytes()) {
            const char character = arguments[end];
            if (quote != '\0') {
                if (character == quote) quote = '\0';
            } else if (character == '\'' || character == '"') {
                quote = character;
            } else if (character == '{') {
                ++depth;
            } else if (character == '}') {
                if (depth == 0U) {
                    return Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        "Binding contains an unmatched closing brace");
                }
                --depth;
            } else if (character == ',' && depth == 0U) {
                break;
            }
            ++end;
        }
        if (depth != 0U || quote != '\0') {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Binding contains an incomplete nested markup extension");
        }
        const Base::StringView item = TrimAscii(arguments.Substr(begin, end - begin));
        const std::uint32_t equals = [&item]() noexcept {
            std::uint32_t depth = 0U;
            char quote = '\0';
            for (std::uint32_t index = 0U; index < item.SizeBytes(); ++index) {
                const char character = item[index];
                if (quote != '\0') {
                    if (character == quote) quote = '\0';
                } else if (character == '\'' || character == '"') {
                    quote = character;
                } else if (character == '{') {
                    ++depth;
                } else if (character == '}') {
                    if (depth > 0U) --depth;
                } else if (character == '=' && depth == 0U) {
                    return index;
                }
            }
            return item.SizeBytes();
        }();
        if (item.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Binding argument is empty");
        }
        if (equals == item.SizeBytes()) {
            if (!path.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding positional Path is specified more than once");
            }
            path = item;
            begin = end + 1U;
            continue;
        }
        const Base::StringView key = TrimAscii(item.Substr(0U, equals));
        const Base::StringView value = TrimAscii(item.Substr(
            equals + 1U,
            item.SizeBytes() - equals - 1U));
        if (value.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Binding argument value is empty");
        }
        if (key == ElementNameKey) {
            if (!elementName.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding ElementName is specified more than once");
            }
            elementName = value;
        } else if (key == SourceKey) {
            if (!sourceResource.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding Source is specified more than once");
            }
            if (value.SizeBytes() <=
                    StaticResourcePrefix.SizeBytes() + 1U ||
                value.Substr(0U, StaticResourcePrefix.SizeBytes()) !=
                    StaticResourcePrefix ||
                value[value.SizeBytes() - 1U] != '}') {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Binding Source currently requires a StaticResource");
            }
            sourceResource = TrimAscii(value.Substr(
                StaticResourcePrefix.SizeBytes(),
                value.SizeBytes() - StaticResourcePrefix.SizeBytes() - 1U));
            if (sourceResource.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding StaticResource key is empty");
            }
        } else if (key == RelativeSourceKey) {
            if (relativeSource != RelativeSourceKind::None) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding RelativeSource is specified more than once");
            }
            Base::StringView relative = TrimAscii(value);
            if (relative == TemplatedParentValue) {
                relativeSource =
                    RelativeSourceKind::TemplatedParent;
            } else if (
                relative.SizeBytes() >
                    RelativeSourcePrefix.SizeBytes() + 1U &&
                relative.Substr(
                    0U,
                    RelativeSourcePrefix.SizeBytes()) ==
                    RelativeSourcePrefix &&
                relative[relative.SizeBytes() - 1U] == '}') {
                Base::StringView relativeMode = TrimAscii(
                    relative.Substr(
                        RelativeSourcePrefix.SizeBytes(),
                        relative.SizeBytes() -
                            RelativeSourcePrefix.SizeBytes() - 1U));
                // RelativeSource is itself a markup extension, so its
                // arguments are separated by commas outside nested x:Type
                // braces. Preserve the primary mode/AncestorType token and
                // parse AncestorLevel independently.
                std::uint32_t relativeEnd = 0U;
                std::uint32_t relativeDepth = 0U;
                while (relativeEnd < relativeMode.SizeBytes()) {
                    const char character = relativeMode[relativeEnd];
                    if (character == '{') {
                        ++relativeDepth;
                    } else if (character == '}') {
                        if (relativeDepth > 0U) --relativeDepth;
                    } else if (character == ',' && relativeDepth == 0U) {
                        break;
                    }
                    ++relativeEnd;
                }
                Base::StringView relativeTail;
                if (relativeEnd < relativeMode.SizeBytes()) {
                    relativeTail = TrimAscii(relativeMode.Substr(
                        relativeEnd + 1U,
                        relativeMode.SizeBytes() - relativeEnd - 1U));
                    relativeMode = TrimAscii(relativeMode.Substr(
                        0U, relativeEnd));
                }
                constexpr Base::StringView AncestorLevelPrefix(
                    "AncestorLevel=");
                if (!relativeTail.Empty()) {
                    if (relativeTail.SizeBytes() <=
                            AncestorLevelPrefix.SizeBytes() ||
                        relativeTail.Substr(
                            0U, AncestorLevelPrefix.SizeBytes()) !=
                            AncestorLevelPrefix) {
                        return Base::Status::Failure(
                            Base::ErrorCode::Unsupported,
                            "Binding RelativeSource argument is not supported");
                    }
                    const Base::StringView levelText = TrimAscii(
                        relativeTail.Substr(
                            AncestorLevelPrefix.SizeBytes(),
                            relativeTail.SizeBytes() -
                                AncestorLevelPrefix.SizeBytes()));
                    std::uint64_t parsedLevel = 0U;
                    if (levelText.Empty()) {
                        return Base::Status::Failure(
                            Base::ErrorCode::ValidationFailed,
                            "Binding RelativeSource AncestorLevel is empty");
                    }
                    for (std::uint32_t index = 0U;
                         index < levelText.SizeBytes(); ++index) {
                        const char digit = levelText[index];
                        if (digit < '0' || digit > '9') {
                            return Base::Status::Failure(
                                Base::ErrorCode::ValidationFailed,
                                "Binding RelativeSource AncestorLevel must be an unsigned integer");
                        }
                        parsedLevel = parsedLevel * 10U +
                            static_cast<std::uint64_t>(digit - '0');
                        if (parsedLevel > UINT32_MAX) {
                            return Base::Status::Failure(
                                Base::ErrorCode::OutOfRange,
                                "Binding RelativeSource AncestorLevel is out of range");
                        }
                    }
                    if (parsedLevel == 0U) {
                        return Base::Status::Failure(
                            Base::ErrorCode::OutOfRange,
                            "Binding RelativeSource AncestorLevel must be at least one");
                    }
                    ancestorLevel =
                        static_cast<std::uint32_t>(parsedLevel);
                }
                constexpr Base::StringView ModePrefix("Mode=");
                if (relativeMode.SizeBytes() >= ModePrefix.SizeBytes() &&
                    relativeMode.Substr(0U, ModePrefix.SizeBytes()) ==
                        ModePrefix) {
                    relativeMode = TrimAscii(relativeMode.Substr(
                        ModePrefix.SizeBytes(),
                        relativeMode.SizeBytes() - ModePrefix.SizeBytes()));
                }
                constexpr Base::StringView AncestorPrefix("AncestorType=");
                if (relativeMode == SelfValue) {
                    relativeSource = RelativeSourceKind::Self;
                } else if (relativeMode == TemplatedParentValue) {
                    relativeSource = RelativeSourceKind::TemplatedParent;
                } else if (relativeMode.SizeBytes() >=
                           AncestorPrefix.SizeBytes() &&
                    relativeMode.Substr(0U, AncestorPrefix.SizeBytes()) ==
                        AncestorPrefix) {
                    Base::StringView typeName = TrimAscii(relativeMode.Substr(
                        AncestorPrefix.SizeBytes(),
                        relativeMode.SizeBytes() - AncestorPrefix.SizeBytes()));
                    constexpr Base::StringView TypePrefix("{x:Type");
                    if (typeName.SizeBytes() > TypePrefix.SizeBytes() + 1U &&
                        typeName.Substr(0U, TypePrefix.SizeBytes()) == TypePrefix &&
                        typeName[typeName.SizeBytes() - 1U] == '}') {
                        typeName = TrimAscii(typeName.Substr(
                            TypePrefix.SizeBytes(),
                            typeName.SizeBytes() - TypePrefix.SizeBytes() - 1U));
                    }
                    if (typeName.Empty()) {
                        return Base::Status::Failure(
                            Base::ErrorCode::ValidationFailed,
                            "Binding RelativeSource AncestorType is empty");
                    }
                    ancestorType = typeName;
                    relativeSource = RelativeSourceKind::Ancestor;
                } else {
                    return Base::Status::Failure(
                        Base::ErrorCode::Unsupported,
                        "Binding RelativeSource mode is not supported");
                }
            } else {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Binding RelativeSource mode is not supported");
            }
        } else if (key == StringFormatKey) {
            if (!stringFormat.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding StringFormat is specified more than once");
            }
            stringFormat = value;
        } else if (key == FallbackValueKey) {
            if (!fallbackValue.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding FallbackValue is specified more than once");
            }
            // The metadata binding engine does not yet need the fallback for
            // a resolvable path, but WPF permits it on every Binding. Parse
            // it here so the declaration remains valid while the fallback is
            // carried by the higher-level binding semantics incrementally.
            fallbackValue = value;
        } else if (key == ConverterKey) {
            // Preserve WPF's nested StaticResource spelling. Converter
            // execution is attached by the binding runtime when present; the
            // parser must not reject an otherwise valid binding declaration.
            if (!converterResource.Empty() ||
                value.SizeBytes() <= StaticResourcePrefix.SizeBytes() + 1U ||
                value.Substr(0U, StaticResourcePrefix.SizeBytes()) !=
                    StaticResourcePrefix ||
                value[value.SizeBytes() - 1U] != '}') {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Binding Converter currently requires a StaticResource");
            }
            converterResource = TrimAscii(value.Substr(
                StaticResourcePrefix.SizeBytes(),
                value.SizeBytes() - StaticResourcePrefix.SizeBytes() - 1U));
            if (converterResource.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding Converter StaticResource key is empty");
            }
        } else if (key == ConverterParameterKey) {
            if (!converterParameter.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding ConverterParameter is specified more than once");
            }
            converterParameter = value;
        } else if (key == PathKey) {
            if (!path.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding Path is specified more than once");
            }
            path = value;
        } else if (key == ModeKey) {
            if (value == OneTimeMode) {
                mode = Data::BindingMode::OneTime;
            } else if (value == OneWayMode) {
                mode = Data::BindingMode::OneWay;
            } else if (value == TwoWayMode) {
                mode = Data::BindingMode::TwoWay;
            } else if (value == OneWayToSourceMode) {
                mode = Data::BindingMode::OneWayToSource;
            } else {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Binding mode is not supported");
            }
        } else if (key == UpdateSourceTriggerKey) {
            if (value == PropertyChangedTrigger) {
                updateSourceTrigger = Meta::UpdateSourceTrigger::PropertyChanged;
            } else if (value == LostFocusTrigger) {
                updateSourceTrigger = Meta::UpdateSourceTrigger::LostFocus;
            } else if (value == ExplicitTrigger) {
                updateSourceTrigger = Meta::UpdateSourceTrigger::Explicit;
            } else if (value == DefaultTrigger) {
                updateSourceTrigger = Meta::UpdateSourceTrigger::Default;
            } else {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Binding update trigger is not supported");
            }
        } else {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Binding argument is not supported");
        }
        begin = end + 1U;
    }
    // An empty Binding is WPF's canonical "bind to the current source
    // object" spelling. With no explicit source that means the inherited
    // DataContext object; with ElementName/Source/RelativeSource it means the
    // selected object itself. The runtime records this as bindsToSource.
    return {};
}

struct DeferredBindingState {
    Aero::BindingEngine* manager = nullptr;
    ::Aero::Meta::Registry* metadata = nullptr;
    Base::Object* source = nullptr;
    Base::Ref<::Aero::DependencyObject> targetOwner;
    ::Aero::DependencyObject* target = nullptr;
    Meta::DependencyPropertyHandle targetProperty;
    Meta::DependencyPropertyHandle dataContextProperty;
    ::Aero::DependencyObject* dataContextOwner = nullptr;
    Base::String elementName;
    Base::String path;
    Base::String stringFormat;
    bool bindsToSource = false;
    Data::BindingMode mode = Data::BindingMode::Default;
    Meta::UpdateSourceTrigger updateSourceTrigger =
        Meta::UpdateSourceTrigger::PropertyChanged;
    Base::Ref<Data::IValueConverter> converter;
    Meta::PropertyValue converterParameter;
    Base::IAllocator* allocator = nullptr;
};

Base::Result<void> PrepareBinding(
    void* context,
    const Aero::NameScope& names) noexcept {
    auto* state = static_cast<DeferredBindingState*>(context);
    if (state == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Deferred Binding state is invalid");
    }
    if (state->source != nullptr || state->elementName.Empty()) {
        return {};
    }
    state->source = names.Find(state->elementName.View());
    return state->source != nullptr
        ? Base::Result<void>()
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::NotFound,
              "Binding ElementName was not found in the completed NameScope"));
}

Base::Result<std::uint64_t> CommitBinding(void* context) noexcept {
    auto* state = static_cast<DeferredBindingState*>(context);
    if (state == nullptr || state->manager == nullptr ||
        state->metadata == nullptr || state->target == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Deferred Binding state is invalid");
    }
    Data::MetadataBindingDescriptor descriptor;
    descriptor.metadata = state->metadata;
    descriptor.source = state->source;
    descriptor.target = state->target;
    descriptor.targetProperty = state->targetProperty;
    descriptor.dataContextProperty = state->dataContextProperty;
    descriptor.dataContextOwner = state->dataContextOwner;
    descriptor.path = state->path.View();
    descriptor.stringFormat =
        state->stringFormat.View();
    descriptor.bindsToSource = state->bindsToSource;
    descriptor.mode = state->mode;
    descriptor.updateSourceTrigger = state->updateSourceTrigger;
    descriptor.converterResource = state->converter;
    descriptor.converterParameter = state->converterParameter;
    Base::Result<Data::BindingHandle> attached =
        state->manager->Attach(descriptor);
    return attached
        ? Base::Result<std::uint64_t>(attached.Value().value)
        : Base::Result<std::uint64_t>(attached.GetStatus());
}


Base::Result<void> BindBindingRuntime(
    void* context, const EffectRuntimeServices& services) noexcept {
    auto* state = static_cast<DeferredBindingState*>(context);
    if (state == nullptr || services.bindings == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Binding requires a mounted View BindingEngine");
    }
    state->manager = services.bindings;
    return {};
}

void RollbackBinding(
    void* context,
    std::uint64_t token) noexcept {
    auto* state = static_cast<DeferredBindingState*>(context);
    if (state != nullptr && state->manager != nullptr && token != 0U) {
        static_cast<void>(
            state->manager->Detach(Data::BindingHandle(token)));
    }
}

void CleanupBinding(void* context) noexcept {
    auto* state = static_cast<DeferredBindingState*>(context);
    if (state == nullptr) return;
    Base::IAllocator* allocator = state->allocator;
    state->~DeferredBindingState();
    allocator->Deallocate(
        state, sizeof(DeferredBindingState),
        alignof(DeferredBindingState), Base::MemoryTag::Markup);
}

struct DeferredMultiBindingState {
    explicit DeferredMultiBindingState(
        Base::IAllocator& value) noexcept
        : sources(&value),
          inputs(&value),
          ready(&value),
          handles(&value),
          changed(
              this,
              &DeferredMultiBindingState::OnInputChanged),
          allocator(&value) {}

    Aero::BindingEngine* manager = nullptr;
    ::Aero::Meta::Registry* metadata = nullptr;
    Base::Ref<::Aero::DependencyObject> targetOwner;
    ::Aero::DependencyObject* target = nullptr;
    Meta::DependencyPropertyHandle targetProperty;
    Meta::DependencyPropertyHandle dataContextProperty;
    ::Aero::DependencyObject* dataContextOwner = nullptr;
    Base::Ref<Data::MultiBinding> binding;
    Base::Vector<Base::Object*> sources;
    Base::Vector<Base::Ref<Data::MultiBindingProxy>> inputs;
    Base::Vector<std::uint8_t> ready;
    Base::Ref<Data::MultiBindingProxy> result;
    Base::Vector<Data::BindingHandle> handles;
    DependencyPropertyChangedEventHandler changed;
    Base::IAllocator* allocator = nullptr;

    bool AllInputsReady() const noexcept {
        if (ready.Size() != inputs.Size() || ready.Empty()) {
            return false;
        }
        for (std::uint8_t value : ready) {
            if (value == 0U) return false;
        }
        return true;
    }

    Base::Result<void> Recompute() noexcept {
        if (!AllInputsReady()) return {};
        if (!binding || !binding->GetConverter() || !result ||
            target == nullptr || metadata == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "MultiBinding runtime is incomplete");
        }

        Base::Vector<Meta::Value> values(allocator);
        Base::Result<void> reserved =
            values.Reserve(inputs.Size());
        if (!reserved) return reserved.GetStatus();
        for (const Base::Ref<Data::MultiBindingProxy>& input :
             inputs) {
            if (!input) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "MultiBinding input proxy is unavailable");
            }
            const Meta::Value current = input->GetValue(
                Data::MultiBindingProxy::ValueProperty.Handle());
            Base::Result<void> added =
                values.PushBack(current);
            if (!added) return added.GetStatus();
        }

        const Meta::DependencyProperty* targetInfo =
            target->PropertyRegistry().Find(targetProperty);
        if (targetInfo == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "MultiBinding target property was not found");
        }
        Base::Result<Meta::Value> converted =
            binding->GetConverter()->Convert(
                values.AsSpan(),
                targetInfo->ValueType(),
                binding->GetConverterParameter());
        if (!converted) return converted.GetStatus();
        Base::Result<Meta::Value> coerced =
            Aero::NormalizeValueForProperty(
                metadata,
                *targetInfo,
                std::move(converted).Value());
        if (!coerced) return coerced.GetStatus();
        return result->SetValueChecked(
            Data::MultiBindingProxy::ValueProperty.Handle(),
            std::move(coerced).Value());
    }

    void OnInputChanged(
        DependencyObject& input,
        const DependencyPropertyChangedEventArgs&) noexcept {
        for (std::uint32_t index = 0U;
             index < inputs.Size(); ++index) {
            if (inputs[index].Get() == &input) {
                ready[index] = 1U;
                break;
            }
        }
        Base::Result<void> recomputed = Recompute();
        if (!recomputed && manager != nullptr) {
            manager->RecordError(recomputed.GetStatus());
        }
    }

    void Detach() noexcept {
        if (manager != nullptr) {
            for (Data::BindingHandle handle : handles) {
                if (handle.IsValid()) {
                    static_cast<void>(
                        manager->Detach(handle));
                }
            }
        }
        handles.Clear();
        for (const Base::Ref<Data::MultiBindingProxy>& input :
             inputs) {
            if (input) {
                static_cast<void>(
                    input->RemoveValueChangedHandler(
                        Data::MultiBindingProxy::
                            ValueProperty.Handle(),
                        changed));
            }
        }
    }
};


Base::Result<void> BindMultiBindingRuntime(
    void* context, const EffectRuntimeServices& services) noexcept {
    auto* state = static_cast<DeferredMultiBindingState*>(context);
    if (state == nullptr || services.bindings == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MultiBinding requires a mounted View BindingEngine");
    }
    state->manager = services.bindings;
    return {};
}

Base::Result<void> PrepareMultiBinding(
    void* context,
    const Aero::NameScope& names) noexcept {
    auto* state =
        static_cast<DeferredMultiBindingState*>(context);
    if (state == nullptr || !state->binding) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Deferred MultiBinding state is invalid");
    }
    state->sources.Clear();
    Base::Result<void> reserved =
        state->sources.Reserve(
            state->binding->GetBindings().Size());
    if (!reserved) return reserved.GetStatus();
    for (const Base::Ref<Data::Binding>& child :
         state->binding->GetBindings()) {
        if (!child) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "MultiBinding contains a null Binding");
        }
        Base::Object* source = child->GetSource().Get();
        if (source == nullptr &&
            !child->GetElementName().Empty()) {
            source = names.Find(child->GetElementName());
            if (source == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "MultiBinding ElementName was not found");
            }
        }
        if (source == nullptr && child->GetRelativeSource()) {
            if (child->GetRelativeSource()->GetMode() ==
                    Data::RelativeSourceMode::Self) {
                source = state->target;
            } else {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "MultiBinding child RelativeSource mode is unsupported");
            }
        }
        Base::Result<void> added =
            state->sources.PushBack(source);
        if (!added) return added.GetStatus();
    }
    return {};
}

Base::Result<std::uint64_t> CommitMultiBinding(
    void* context) noexcept {
    auto* state =
        static_cast<DeferredMultiBindingState*>(context);
    if (state == nullptr || state->manager == nullptr ||
        state->metadata == nullptr || state->target == nullptr ||
        !state->binding || !state->binding->GetConverter()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Deferred MultiBinding state is incomplete");
    }
    const auto children = state->binding->GetBindings();
    if (children.Empty() ||
        state->sources.Size() != children.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MultiBinding has no prepared child bindings");
    }

    Base::Result<Base::Ref<Data::MultiBindingProxy>>
        resultProxy = Base::MakeRef<Data::MultiBindingProxy>();
    if (!resultProxy) return resultProxy.GetStatus();
    state->result = std::move(resultProxy).Value();

    const Meta::Value initial =
        state->target->GetValue(state->targetProperty);
    Base::Result<void> initialized =
        state->result->SetValueChecked(
            Data::MultiBindingProxy::ValueProperty.Handle(),
            initial);
    if (!initialized) return initialized.GetStatus();

    Base::Result<void> reservedInputs =
        state->inputs.Reserve(children.Size());
    if (reservedInputs) {
        reservedInputs =
            state->ready.Reserve(children.Size());
    }
    if (!reservedInputs) {
        state->Detach();
        return reservedInputs.GetStatus();
    }

    for (std::uint32_t index = 0U;
         index < children.Size(); ++index) {
        Base::Result<Base::Ref<Data::MultiBindingProxy>>
            input = Base::MakeRef<Data::MultiBindingProxy>();
        if (!input) {
            state->Detach();
            return input.GetStatus();
        }
        Base::Result<void> retained =
            state->inputs.PushBack(input.Value());
        if (retained) {
            retained = state->ready.PushBack(0U);
        }
        if (!retained) {
            state->Detach();
            return retained.GetStatus();
        }
        Base::Result<void> subscribed =
            input.Value()->AddValueChangedHandlerChecked(
                Data::MultiBindingProxy::ValueProperty.Handle(),
                state->changed);
        if (!subscribed) {
            state->Detach();
            return subscribed.GetStatus();
        }

        const Base::Ref<Data::Binding>& child =
            children[index];
        Data::MetadataBindingDescriptor descriptor;
        descriptor.metadata = state->metadata;
        descriptor.source = state->sources[index];
        descriptor.target = input.Value().Get();
        descriptor.targetProperty =
            Data::MultiBindingProxy::ValueProperty.Handle();
        descriptor.dataContextProperty =
            state->dataContextProperty;
        descriptor.dataContextOwner =
            state->dataContextOwner;
        descriptor.path = child->GetPath().GetPath();
        descriptor.stringFormat =
            child->GetStringFormat();
        descriptor.bindsToSource =
            child->GetPath().GetIsEmpty();
        descriptor.mode = Data::BindingMode::OneWay;
        descriptor.updateSourceTrigger =
            Meta::UpdateSourceTrigger::PropertyChanged;
        descriptor.converterResource =
            child->GetConverter();
        descriptor.converterParameter =
            child->GetConverterParameter();
        Base::Result<Data::BindingHandle> attached =
            state->manager->Attach(descriptor);
        if (!attached) {
            state->Detach();
            return attached.GetStatus();
        }
        Base::Result<void> retainedHandle =
            state->handles.PushBack(attached.Value());
        if (!retainedHandle) {
            static_cast<void>(
                state->manager->Detach(attached.Value()));
            state->Detach();
            return retainedHandle.GetStatus();
        }
    }

    // Attach the aggregate expression after its child expressions. During a
    // DataBind flush, child values then update the result proxy before the
    // final target expression is evaluated in the same pass.
    Data::BindingDescriptor output;
    output.source = state->result.Get();
    output.sourceProperty =
        Data::MultiBindingProxy::ValueProperty.Handle();
    output.target = state->target;
    output.targetProperty = state->targetProperty;
    output.mode = Data::BindingMode::OneWay;
    Base::Result<Data::BindingHandle> outputHandle =
        state->manager->Attach(output);
    if (!outputHandle) {
        state->Detach();
        return outputHandle.GetStatus();
    }
    Base::Result<void> retainedOutput =
        state->handles.PushBack(outputHandle.Value());
    if (!retainedOutput) {
        static_cast<void>(
            state->manager->Detach(outputHandle.Value()));
        state->Detach();
        return retainedOutput.GetStatus();
    }
    state->manager->RegisterMultiBinding(
        *state->target,
        state->targetProperty,
        state->handles.AsSpan());
    return UINT64_C(1);
}

void RollbackMultiBinding(
    void* context,
    std::uint64_t) noexcept {
    auto* state =
        static_cast<DeferredMultiBindingState*>(context);
    if (state != nullptr) state->Detach();
}

void CleanupMultiBinding(void* context) noexcept {
    auto* state =
        static_cast<DeferredMultiBindingState*>(context);
    if (state == nullptr) return;
    state->Detach();
    Base::IAllocator* allocator = state->allocator;
    state->~DeferredMultiBindingState();
    allocator->Deallocate(
        state,
        sizeof(DeferredMultiBindingState),
        alignof(DeferredMultiBindingState),
        Base::MemoryTag::Markup);
}

Base::Result<ProvidedValue> CreateMultiBindingValueImpl(
    Data::MultiBinding& binding,
    const ExtensionServices& services) noexcept {
    if (services.schema == nullptr ||
        services.targetObject == nullptr ||
        services.targetMember == Meta::InvalidMemberId ||
        services.nameScope == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MultiBinding has no target service context");
    }
    Base::Result<DependencyObject*> target =
        SchemaPrivate::ResolvePropertyTarget(
            *services.schema,
            *services.targetObject);
    if (!target) return target.GetStatus();
    ::Aero::Meta::Registry* metadata =
        SchemaPrivate::Metadata(
            *services.schema);
    if (metadata == nullptr ||
        target.Value()->PropertyRegistry().Find(
            Meta::DependencyPropertyHandle{
                services.targetMember}) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "MultiBinding target dependency property was not found");
    }

    Base::IAllocator& allocator =
        Base::GetDefaultAllocator();
    void* memory = allocator.Allocate({
        sizeof(DeferredMultiBindingState),
        alignof(DeferredMultiBindingState),
        Base::MemoryTag::Markup});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "MultiBinding state allocation failed");
    }
    auto* state = new (memory)
        DeferredMultiBindingState(allocator);
    state->manager = services.bindings;
    state->metadata = metadata;
    state->targetOwner =
        Base::Ref<DependencyObject>::TryFromBorrowed(
            *target.Value());
    state->target = state->targetOwner.Get();
    state->targetProperty = {
        services.targetMember};
    state->dataContextProperty =
        FrameworkElement::DataContextProperty.Handle();
    state->dataContextOwner =
        target.Value();
    if (!metadata->Types().IsDerivedFrom(
            target.Value()->RuntimeType(),
            FrameworkElement::StaticTypeId()) &&
        services.rootObject != nullptr &&
        metadata->Types().IsDerivedFrom(
            services.rootObject->RuntimeType(),
            DependencyObject::StaticTypeId())) {
        state->dataContextOwner =
            static_cast<DependencyObject*>(
                services.rootObject);
    }
    state->binding =
        Base::Ref<Data::MultiBinding>::TryFromBorrowed(
            binding);
    if (!state->targetOwner || !state->binding) {
        CleanupMultiBinding(state);
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MultiBinding objects are not reference-counted");
    }
    return ProvidedValue::Deferred(
        state,
        &CommitMultiBinding,
        &RollbackMultiBinding,
        &CleanupMultiBinding,
        &PrepareMultiBinding,
        &BindMultiBindingRuntime);
}

} // namespace WriterBindingDetail

Base::Result<long double> ReadConstantBindingNumber(
    const Meta::Value& value) noexcept {
    return WriterBindingDetail::ReadConstantBindingNumberImpl(value);
}

Base::Result<Meta::Value> ConvertConstantBindingValue(
    const Meta::Value& value,
    Meta::TypeId targetType) noexcept {
    return WriterBindingDetail::ConvertConstantBindingValueImpl(value, targetType);
}

Base::Result<ProvidedValue> CreateMultiBindingValue(
    Data::MultiBinding& binding,
    const ExtensionServices& services) noexcept {
    return WriterBindingDetail::CreateMultiBindingValueImpl(binding, services);
}

using namespace WriterBindingDetail;

Base::Result<void> CaptureControlTemplateChildName(
    Controls::ControlTemplate& controlTemplate,
    const Aero::NameScope* nameScope,
    Base::Object& target,
    Base::String& storage) noexcept {
    Base::StringView authoredName;
    if (nameScope != nullptr) {
        authoredName = nameScope->NameOf(target);
    }
    if (authoredName.Empty()) {
        authoredName =
            ::Aero::Controls::TemplatePrivate::AuthoredNames(
                controlTemplate).NameOf(target);
    }
    if (authoredName.Empty()) {
        Base::Result<Base::String> generated =
            ::Aero::Controls::TemplatePrivate::EnsureAuthoredName(
                controlTemplate, target);
        if (!generated) return generated.GetStatus();
        storage = std::move(generated).Value();
        authoredName = storage.View();
    } else {
        Base::Result<void> assigned = storage.Assign(authoredName);
        if (!assigned) return assigned.GetStatus();
        authoredName = storage.View();
        if (::Aero::Controls::TemplatePrivate::AuthoredNames(
                controlTemplate).Find(authoredName) == nullptr) {
            Base::Result<void> registered =
                ::Aero::Controls::TemplatePrivate::RegisterAuthoredName(
                    controlTemplate, authoredName, target);
            if (!registered) return registered.GetStatus();
        }
    }
    if (nameScope != nullptr &&
        nameScope->Find(authoredName) == nullptr) {
        // ExtensionServices exposes the active writer NameScope as const.
        // Generated TemplatedParent Binding names must still round-trip into
        // that same table so later ElementName lookups and CompileBlueprint
        // NameOf stay consistent with AuthoredNames.
        Base::Result<void> registered =
            const_cast<Aero::NameScope*>(nameScope)->Register(
                authoredName, target);
        if (!registered &&
            registered.GetStatus().code !=
                Base::ErrorCode::AlreadyExists) {
            return registered.GetStatus();
        }
    }
    return {};
}

BindingExtension::BindingExtension(
    const BindingExtensionOptions& options) noexcept
    : options_(options) {}

Base::Result<void> BindingExtension::Register(
    Schema& schema,
    Meta::TypeId bindingExtensionType) noexcept {
    return SchemaPrivate::AddMarkupExtension(schema, {
        bindingExtensionType,
        &BindingExtension::ProvideValue,
        this});
}

Base::Result<ProvidedValue> BindingExtension::ProvideValue(
    Base::StringView arguments,
    const ExtensionServices& services,
    void* context) noexcept {
    BindingExtension* extension =
        static_cast<BindingExtension*>(context);
    if (extension == nullptr ||
        services.schema == nullptr || services.targetObject == nullptr ||
        services.nameScope == nullptr ||
        services.targetMember == Meta::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Binding markup extension has no target service context");
    }

    Base::StringView elementName;
    Base::StringView sourceResource;
    Base::StringView path;
    Base::StringView stringFormat;
    Base::StringView fallbackValue;
    Base::StringView converterResource;
    Base::StringView converterParameter;
    Base::StringView ancestorType;
    std::uint32_t ancestorLevel = 1U;
    RelativeSourceKind relativeSource =
        RelativeSourceKind::None;
    Data::BindingMode mode = Data::BindingMode::Default;
    Meta::UpdateSourceTrigger updateSourceTrigger =
        Meta::UpdateSourceTrigger::PropertyChanged;
    Base::Result<void> parsed = ParseArguments(
        arguments,
        elementName,
        sourceResource,
        path,
        stringFormat,
        fallbackValue,
        converterResource,
        converterParameter,
        ancestorType,
        ancestorLevel,
        relativeSource,
        mode,
        updateSourceTrigger);
    if (!parsed) {
        return parsed.GetStatus();
    }
    (void)fallbackValue;
    if ((!elementName.Empty() &&
         relativeSource != RelativeSourceKind::None) ||
        (!sourceResource.Empty() &&
         (!elementName.Empty() ||
          relativeSource != RelativeSourceKind::None))) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Binding Source, ElementName, and RelativeSource are mutually exclusive");
    }

    ::Aero::Meta::Registry* metadata =
        SchemaPrivate::Metadata(
            *services.schema);
    const Meta::PropertyInfo* targetMember =
        metadata != nullptr
        ? metadata->Types().FindProperty(
            services.targetMember)
        : nullptr;

    Base::Ref<Data::IValueConverter> converter;
    if (!converterResource.Empty()) {
        if (!services.resources.IsAvailable()) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Binding Converter requires an active resource scope");
        }
        Base::Result<Aero::ResourceValue> resource =
            services.resources.Lookup(converterResource);
        if (!resource) return resource.GetStatus();
        if (resource.Value().Kind() != Meta::ValueKind::Object ||
            resource.Value().IsNullObject() ||
            !resource.Value().AsObject() ||
            metadata == nullptr ||
            !metadata->Types().IsDerivedFrom(
                resource.Value().AsObject()->RuntimeType(),
                Data::IValueConverter::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Binding Converter StaticResource is not an IValueConverter");
        }
        converter = Base::Ref<Data::IValueConverter>::FromBorrowed(
            *static_cast<Data::IValueConverter*>(
                resource.Value().AsObject().Get()));
    }
    if (!sourceResource.Empty() &&
        path.Empty() &&
        stringFormat.Empty() &&
        converterResource.Empty() &&
        elementName.Empty() &&
        relativeSource == RelativeSourceKind::None &&
        services.resources.IsAvailable()) {
        Base::Result<Aero::ResourceValue> constant =
            services.resources.Lookup(sourceResource);
        if (!constant) return constant.GetStatus();
        if (constant.Value().Kind() !=
                Meta::ValueKind::Object) {
            if (mode == Data::BindingMode::TwoWay ||
                mode == Data::BindingMode::OneWayToSource) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "A scalar StaticResource Binding cannot write back");
            }
            Base::Result<Meta::Value> converted =
                ConvertConstantBindingValue(
                    constant.Value(),
                    services.targetValueType);
            return converted
                ? Base::Result<ProvidedValue>(
                      ProvidedValue::FromValue(
                          std::move(converted).Value()))
                : Base::Result<ProvidedValue>(
                      converted.GetStatus());
        }
    }
    // Setter is only an authored declaration. Its target object is created
    // when the Style is applied, so preserve the binding specification here.
    const bool authoredSetterValue =
        targetMember != nullptr &&
        targetMember->OwnerType() == Aero::Setter::StaticTypeId() &&
        targetMember->Name() == Base::StringView("Value");
    const bool authoredHierarchicalItemsSource =
        targetMember != nullptr &&
        targetMember->OwnerType() == DataTemplate::StaticTypeId() &&
        targetMember->Name() == Base::StringView("ItemsSource");
    const bool authoredLaunchPath =
        targetMember != nullptr &&
        services.targetObject->RuntimeType() ==
            Aero::Interactivity::LaunchUriOrFileAction::StaticTypeId() &&
        targetMember->Name() == Base::StringView("Path");
    const bool authoredChangePropertyValue =
        targetMember != nullptr &&
        services.targetObject->RuntimeType() ==
            Aero::Interactivity::ChangePropertyAction::StaticTypeId() &&
        targetMember->Name() == Base::StringView("Value");
    const bool authoredTimerInterval =
        targetMember != nullptr &&
        services.targetObject->RuntimeType() ==
            Media::Animation::TimerTrigger::StaticTypeId() &&
        targetMember->Name() ==
            Base::StringView("MillisecondsPerTick");
    const bool authoredInvokeCommand =
        targetMember != nullptr &&
        services.targetObject->RuntimeType() ==
            Aero::Interactivity::InvokeCommandAction::StaticTypeId() &&
        targetMember->Name() == Base::StringView("Command");
    const bool authoredInvokeParameter =
        targetMember != nullptr &&
        services.targetObject->RuntimeType() ==
            Aero::Interactivity::InvokeCommandAction::StaticTypeId() &&
        targetMember->Name() == Base::StringView("CommandParameter");
    const bool authoredBehaviorBinding =
        targetMember != nullptr && metadata != nullptr &&
        metadata->Types().IsDerivedFrom(
            services.targetObject->RuntimeType(),
            ::Aero::Interactivity::Behavior::StaticTypeId());
    // Blend DataTrigger/Condition/ComparisonCondition store BindingBase as a
    // CLR authoring plan. They are not DependencyObjects, so a live expression
    // would fail ResolvePropertyTarget ("XAML target does not support
    // dependency properties") and abort ResourceDictionary Source loads.
    const bool targetIsDependencyObject =
        metadata != nullptr &&
        metadata->Types().IsDerivedFrom(
            services.targetObject->RuntimeType(),
            Meta::TypeOf<::Aero::DependencyObject>());
    const bool authoredBindingProperty =
        targetMember != nullptr &&
        (targetMember->ValueType() == Data::Binding::StaticTypeId() ||
         targetMember->ValueType() == Data::BindingBase::StaticTypeId());
    if (authoredBindingProperty || !targetIsDependencyObject ||
        authoredSetterValue || authoredHierarchicalItemsSource ||
        authoredLaunchPath || authoredChangePropertyValue ||
        authoredTimerInterval || authoredInvokeCommand ||
        authoredInvokeParameter || authoredBehaviorBinding) {
        if (!sourceResource.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Data::Binding does not support an explicit Source");
        }
        Base::Result<Base::Ref<
            Data::Binding>> binding =
                Base::MakeRef<
                    Data::Binding>();
        if (!binding) {
            return binding.GetStatus();
        }
        binding.Value()->SetPath(path);
        binding.Value()->SetElementName(elementName);
        binding.Value()->SetStringFormat(stringFormat);
        binding.Value()->SetMode(mode);
        binding.Value()->SetUpdateSourceTrigger(updateSourceTrigger);
        if (converter) binding.Value()->SetConverter(converter);
        if (!converterParameter.Empty()) {
            Base::Result<Meta::PropertyValue> parameter =
                Meta::PropertyValue::TryFromString(
                    Meta::TypeOf<Base::String>(),
                    converterParameter);
            if (!parameter) return parameter.GetStatus();
            binding.Value()->SetConverterParameter(
                std::move(parameter).Value());
        }
        if (relativeSource != RelativeSourceKind::None) {
            const Data::RelativeSourceMode sourceMode =
                relativeSource == RelativeSourceKind::Self
                    ? Data::RelativeSourceMode::Self
                    : relativeSource == RelativeSourceKind::TemplatedParent
                        ? Data::RelativeSourceMode::TemplatedParent
                        : Data::RelativeSourceMode::FindAncestor;
            Base::Result<Base::Ref<Data::RelativeSource>> source =
                Base::MakeRef<Data::RelativeSource>(sourceMode);
            if (!source) return source.GetStatus();
            if (sourceMode == Data::RelativeSourceMode::FindAncestor) {
                source.Value()->SetAncestorType(ancestorType);
                source.Value()->SetAncestorLevel(ancestorLevel);
            }
            binding.Value()->SetRelativeSource(std::move(source).Value());
        }
        if (authoredLaunchPath) {
            static_cast<Aero::Interactivity::LaunchUriOrFileAction*>(
                services.targetObject)->SetPathBinding(
                    std::move(binding).Value());
            return ProvidedValue::Handled();
        }
        if (authoredChangePropertyValue) {
            static_cast<Aero::Interactivity::ChangePropertyAction*>(
                services.targetObject)->SetValueBinding(
                    std::move(binding).Value());
            return ProvidedValue::Handled();
        }
        if (authoredTimerInterval) {
            static_cast<Media::Animation::TimerTrigger*>(
                services.targetObject)->SetMillisecondsPerTickBinding(
                    std::move(binding).Value());
            return ProvidedValue::Handled();
        }
        if (authoredInvokeCommand) {
            static_cast<Aero::Interactivity::InvokeCommandAction*>(
                services.targetObject)->SetCommandBinding(
                    std::move(binding).Value());
            return ProvidedValue::Handled();
        }
        if (authoredInvokeParameter) {
            static_cast<Aero::Interactivity::InvokeCommandAction*>(
                services.targetObject)->SetCommandParameterBinding(
                    std::move(binding).Value());
            return ProvidedValue::Handled();
        }
        if (authoredBehaviorBinding) {
            Base::Result<void> retained =
                static_cast<::Aero::Interactivity::Behavior*>(
                    services.targetObject)->AddAuthoredBinding(
                        Meta::DependencyPropertyHandle{
                            targetMember->Id()},
                        std::move(binding).Value());
            return retained
                ? Base::Result<ProvidedValue>(ProvidedValue::Handled())
                : Base::Result<ProvidedValue>(retained.GetStatus());
        }
        Base::Result<Meta::Value> value =
            Meta::Value::FromObject(
                authoredHierarchicalItemsSource
                    ? targetMember->ValueType()
                    : Data::Binding::StaticTypeId(),
                Base::Ref<Base::Object>(
                    std::move(binding).Value()));
        if (!value) return value.GetStatus();
        return ProvidedValue::FromValue(
            std::move(value).Value());
    }

    Base::Result<::Aero::DependencyObject*> targetResult =
        SchemaPrivate::ResolvePropertyTarget(
            *services.schema,
            *services.targetObject);
    if (!targetResult) {
        return targetResult.GetStatus();
    }
    ::Aero::DependencyObject* target = targetResult.Value();

    const Meta::DependencyPropertyHandle targetHandle{
        services.targetMember};
    const Meta::DependencyProperty* targetProperty =
        target->PropertyRegistry().Find(targetHandle);
    if (targetProperty == nullptr ||
        SchemaPrivate::Metadata(
            *services.schema) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Binding target property or metadata program was not found");
    }

    if (relativeSource ==
            RelativeSourceKind::TemplatedParent &&
        services.deferredContentOwner != nullptr &&
        services.deferredContentOwner->RuntimeType() ==
            Controls::ControlTemplate::StaticTypeId()) {
        auto& controlTemplate =
            static_cast<Controls::ControlTemplate&>(
                *services.deferredContentOwner);
        Base::String targetName;
        Base::Result<void> captured = CaptureControlTemplateChildName(
            controlTemplate,
            services.nameScope,
            *services.targetObject,
            targetName);
        if (!captured) {
            return captured.GetStatus();
        }
        Meta::PropertyValue stagedParameter;
        if (!converterParameter.Empty()) {
            Base::Result<Meta::PropertyValue> parameter =
                Meta::PropertyValue::TryFromString(
                    Meta::TypeOf<Base::String>(),
                    converterParameter);
            if (!parameter) return parameter.GetStatus();
            stagedParameter = std::move(parameter).Value();
        }
        Base::Result<void> added =
            ::Aero::Controls::TemplatePrivate::AddTemplatedParentBinding(controlTemplate,
                targetName.View(),
                path,
                stringFormat,
                targetHandle,
                mode,
                updateSourceTrigger,
                converter,
                stagedParameter);
        return added
            ? Base::Result<ProvidedValue>(
                  ProvidedValue::Handled())
            : Base::Result<ProvidedValue>(
                  added.GetStatus());
    }

    Base::Object* source = nullptr;
    if (!sourceResource.Empty()) {
        if (!services.resources.IsAvailable()) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Binding Source requires an active resource scope");
        }
        Base::Result<Aero::ResourceValue> resource =
            services.resources.Lookup(sourceResource);
        if (!resource) return resource.GetStatus();
        if (resource.Value().Kind() != Meta::ValueKind::Object ||
            resource.Value().IsNullObject() ||
            !resource.Value().AsObject()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Binding Source StaticResource must be an object");
        }
        source = resource.Value().AsObject().Get();
    } else if (!elementName.Empty()) {
        // ElementName may legally refer forward in the same NameScope. Resolve
        // what is already available now and prepare unresolved references once
        // the object writer has completed the document scope.
        source = services.nameScope->Find(elementName);
    } else if (relativeSource == RelativeSourceKind::Self) {
        source = services.targetObject;
    } else if (relativeSource ==
               RelativeSourceKind::TemplatedParent) {
        source = services.templatedParent;
        if (source == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Binding TemplatedParent is unavailable");
        }
    } else {
        if (!extension->options_.dataContextProperty.IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Binding without ElementName requires a DataContext property");
        }
    }

    if (services.deferredContentOwner != nullptr &&
        services.deferredContent != nullptr) {
        Meta::PropertyValue stagedParameter;
        if (!converterParameter.Empty()) {
            Base::Result<Meta::PropertyValue> parameter =
                Meta::PropertyValue::TryFromString(
                    Meta::TypeOf<Base::String>(),
                    converterParameter);
            if (!parameter) return parameter.GetStatus();
            stagedParameter = std::move(parameter).Value();
        }
        Base::Result<void> staged =
            services.deferredContent->StageBinding(
                *services.deferredContentOwner,
                source,
                elementName,
                relativeSource == RelativeSourceKind::Ancestor
                    ? ancestorType
                    : Base::StringView{},
                relativeSource == RelativeSourceKind::Ancestor
                    ? ancestorLevel
                    : 0U,
                *target,
                *SchemaPrivate::Metadata(
                    *services.schema),
                targetHandle,
                extension->options_.dataContextProperty,
                path,
                stringFormat,
                mode,
                updateSourceTrigger,
                path.Empty(),
                converter,
                stagedParameter);
        return staged
            ? Base::Result<ProvidedValue>(
                  ProvidedValue::Handled())
            : Base::Result<ProvidedValue>(
                  staged.GetStatus());
    }

    Base::IAllocator& allocator = Base::GetDefaultAllocator();
    void* memory = allocator.Allocate({
        sizeof(DeferredBindingState),
        alignof(DeferredBindingState),
        Base::MemoryTag::Markup});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Deferred Binding allocation failed");
    }
    auto* state = new (memory) DeferredBindingState();
    state->metadata = SchemaPrivate::Metadata(
        *services.schema);
    state->source = source;
    state->targetOwner =
        Base::Ref<::Aero::DependencyObject>::TryFromBorrowed(*target);
    if (!state->targetOwner) {
        CleanupBinding(state);
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Binding target is not reference-counted");
    }
    state->target = state->targetOwner.Get();
    state->targetProperty = targetHandle;
    state->dataContextProperty = extension->options_.dataContextProperty;
    state->dataContextOwner = target;
    if (source == nullptr &&
        services.rootObject != nullptr &&
        SchemaPrivate::Metadata(*services.schema)->Types().IsDerivedFrom(
            services.rootObject->RuntimeType(),
            ::Aero::DependencyObject::StaticTypeId())) {
        auto* root = static_cast<::Aero::DependencyObject*>(
            services.rootObject);
        const bool targetCanInheritDataContext =
            SchemaPrivate::Metadata(*services.schema)->Types().IsDerivedFrom(
                target->RuntimeType(), FrameworkElement::StaticTypeId());
        if (!targetCanInheritDataContext &&
            root->PropertyRegistry().Find(
                extension->options_.dataContextProperty) != nullptr) {
            state->dataContextOwner = root;
        }
    }
    state->mode = mode;
    state->bindsToSource = path.Empty();
    state->updateSourceTrigger = updateSourceTrigger;
    state->converter = std::move(converter);
    state->allocator = &allocator;
    if (!converterParameter.Empty()) {
        Base::Result<Meta::PropertyValue> parameter =
            Meta::PropertyValue::TryFromString(
                Meta::TypeOf<Base::String>(),
                converterParameter);
        if (!parameter) {
            CleanupBinding(state);
            return parameter.GetStatus();
        }
        state->converterParameter = std::move(parameter).Value();
    }
    Base::Result<void> assigned =
        state->elementName.Assign(elementName);
    if (!assigned) {
        CleanupBinding(state);
        return assigned.GetStatus();
    }
    assigned = state->path.Assign(path);
    if (!assigned) {
        CleanupBinding(state);
        return assigned.GetStatus();
    }
    assigned = state->stringFormat.Assign(
        stringFormat);
    if (!assigned) {
        CleanupBinding(state);
        return assigned.GetStatus();
    }
    return ProvidedValue::Deferred(
        state, &CommitBinding, &RollbackBinding, &CleanupBinding,
        &PrepareBinding, &BindBindingRuntime);
}

} // namespace Aero::Markup
