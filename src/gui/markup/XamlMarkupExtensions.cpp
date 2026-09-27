#include "gui/markup/MarkupExtensionHost.hpp"

#include "gui/core/ValueConversion.hpp"

#include "gui/data/BindingEngine.hpp"

#include "gui/core/ElementTree.hpp"

#include "gui/core/LayoutEngine.hpp"

#include "gui/core/EffectiveValueEngine.hpp"

#include "gui/core/RoutedEvents.hpp"

#include "gui/core/EventRouter.hpp"

#include "gui/templates/TemplateInstance.hpp"

#include "gui/markup/MarkupCommon.hpp"

#include "gui/markup/XamlObjectWriterCommon.hpp"



#include <cstdio>

#include <cmath>

#include <fstream>

#include <new>

#include <string>

#include <utility>

#include <vector>



#include <Aero/Base/String.hpp>

#include <Aero/Base/StringView.hpp>

#include <Aero/Controls/ControlTemplate.hpp>

#include <Aero/Controls.hpp>

#include <Aero/Markup/MarkupExtension.hpp>

#include <Aero/DataTemplate.hpp>

#include <Aero/TryCast.hpp>

#include <Aero/Controls/Primitives/ButtonBase.hpp>

#include <Aero/Controls/Buttons.hpp>

#include <Aero/Controls/TextBlock.hpp>

#include <Aero/Documents/Inlines.hpp>

#include <Aero/FrameworkContentElement.hpp>

#include <Aero/FrameworkElement.hpp>

#include <Aero/Freezable.hpp>

#include <Aero/UIElement.hpp>

#include <Aero/Media/Animation.hpp>

#include <Aero/EventTrigger.hpp>

#include <Aero/Media/Animation/TimerTrigger.hpp>

#include <Aero/Media/Brushes.hpp>

#include <Aero/Media/Images.hpp>

#include <Aero/Interactivity/Behavior.hpp>

#include <Aero/Data/BindingBase.hpp>

#include <Aero/Resources.hpp>

#include <Aero/Style.hpp>

#include <Aero/VisualStateManager.hpp>

#include <Aero/Value.hpp>
#include "gui/core/DependencyObjectAccess.hpp"



// Markup-extension implementations. Kept as one TU because the historic

// .inl files share helper symbols (constant conversion, dynamic-resource

// lookup, CaptureControlTemplateChildName) that were written for a single

// amalgamated translation unit.



// ===== BindingExtension.inl =====

// ===== BindingExtension =====







// Binding markup-extension implementation.













namespace Aero::Markup {

namespace WriterBindingSupport {



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

    void* context, const EffectServices& services) noexcept {

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

        values.Reserve(inputs.Size());

        for (const Base::Ref<Data::MultiBindingProxy>& input :

             inputs) {

            if (!input) {

                return Base::Status::Failure(

                    Base::ErrorCode::InvalidState,

                    "MultiBinding input proxy is unavailable");

            }

            const Meta::Value current = input->GetValue(

                Data::MultiBindingProxy::ValueProperty.Handle());

            values.PushBack(current);

        }



        const Meta::DependencyProperty* targetInfo =

            DependencyObjectAccess::PropertyRegistry(target).Find(targetProperty);

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

        result->SetValue(

            Data::MultiBindingProxy::ValueProperty.Handle(),

            std::move(coerced).Value());

        return {};

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

    void* context, const EffectServices& services) noexcept {

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

    state->sources.Reserve(

            state->binding->GetBindings().Size());

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

        state->sources.PushBack(source);

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

    state->result->SetValue(

        Data::MultiBindingProxy::ValueProperty.Handle(),

        initial);



    state->inputs.Reserve(children.Size());

    state->ready.Reserve(children.Size());



    for (std::uint32_t index = 0U;

         index < children.Size(); ++index) {

        Base::Result<Base::Ref<Data::MultiBindingProxy>>

            input = Base::MakeRef<Data::MultiBindingProxy>();

        if (!input) {

            state->Detach();

            return input.GetStatus();

        }

        state->inputs.PushBack(input.Value());

        state->ready.PushBack(0U);

        input.Value()->AddValueChangedHandler(

            Data::MultiBindingProxy::ValueProperty.Handle(),

            state->changed);



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

        state->handles.PushBack(attached.Value());

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

    state->handles.PushBack(outputHandle.Value());

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

        services.schema->ResolvePropertyTarget(

            *services.targetObject);

    if (!target) return target.GetStatus();

    ::Aero::Meta::Registry* metadata =

        services.schema->Metadata();

    if (metadata == nullptr ||

        DependencyObjectAccess::PropertyRegistry(target.Value()).Find(

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



} // namespace WriterBindingSupport



Base::Result<long double> ReadConstantBindingNumber(

    const Meta::Value& value) noexcept {

    return WriterBindingSupport::ReadConstantBindingNumberImpl(value);

}



Base::Result<Meta::Value> ConvertConstantBindingValue(

    const Meta::Value& value,

    Meta::TypeId targetType) noexcept {

    return WriterBindingSupport::ConvertConstantBindingValueImpl(value, targetType);

}



Base::Result<ProvidedValue> CreateMultiBindingValue(

    Data::MultiBinding& binding,

    const ExtensionServices& services) noexcept {

    return WriterBindingSupport::CreateMultiBindingValueImpl(binding, services);

}



using namespace WriterBindingSupport;



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

            ::Aero::Controls::FrameworkTemplateState::AuthoredNames(

                controlTemplate).NameOf(target);

    }

    if (authoredName.Empty()) {

        Base::Result<Base::String> generated =

            ::Aero::Controls::FrameworkTemplateState::EnsureAuthoredName(

                controlTemplate, target);

        if (!generated) return generated.GetStatus();

        storage = std::move(generated).Value();

        authoredName = storage.View();

    } else {

        Base::Result<void> assigned = storage.Assign(authoredName);

        if (!assigned) return assigned.GetStatus();

        authoredName = storage.View();

        if (::Aero::Controls::FrameworkTemplateState::AuthoredNames(

                controlTemplate).Find(authoredName) == nullptr) {

            Base::Result<void> registered =

                ::Aero::Controls::FrameworkTemplateState::RegisterAuthoredName(

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

    return schema.AddMarkupExtension({

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

        services.schema->Metadata();

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

            static_cast<::Aero::Interactivity::Behavior*>(

                services.targetObject)->AddAuthoredBinding(

                    Meta::DependencyPropertyHandle{

                        targetMember->Id()},

                    std::move(binding).Value());

            return ProvidedValue::Handled();

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

        services.schema->ResolvePropertyTarget(

            *services.targetObject);

    if (!targetResult) {

        return targetResult.GetStatus();

    }

    ::Aero::DependencyObject* target = targetResult.Value();



    const Meta::DependencyPropertyHandle targetHandle{

        services.targetMember};

    const Meta::DependencyProperty* targetProperty =

        DependencyObjectAccess::PropertyRegistry(target).Find(targetHandle);

    if (targetProperty == nullptr ||

        services.schema->Metadata() == nullptr) {

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

            ::Aero::Controls::FrameworkTemplateState::AddTemplatedParentBinding(controlTemplate,

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

                *services.schema->Metadata(),

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

    state->metadata = services.schema->Metadata();

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

        services.schema->Metadata()->Types().IsDerivedFrom(

            services.rootObject->RuntimeType(),

            ::Aero::DependencyObject::StaticTypeId())) {

        auto* root = static_cast<::Aero::DependencyObject*>(

            services.rootObject);

        const bool targetCanInheritDataContext =

            services.schema->Metadata()->Types().IsDerivedFrom(

                target->RuntimeType(), FrameworkElement::StaticTypeId());

        if (!targetCanInheritDataContext &&

            DependencyObjectAccess::PropertyRegistry(root).Find(

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





// ===== DynamicResourceExtension.inl =====

// ===== DynamicResourceExtension =====









// Dynamic-resource markup-extension implementation.









namespace Aero::Markup {

using Aero::ResourceChangeSubscription;

using Aero::ResourceDictionary;



namespace {



struct DynamicResourceState {

    DynamicResourceState(

        Meta::EffectiveValueEngine& effectiveValues,

        ::Aero::DependencyObject& dependencyObject,

        Meta::DependencyPropertyHandle dependencyProperty) noexcept

        : engine(&effectiveValues),

          target(&dependencyObject),

          property(dependencyProperty),

          key(),

          sources(),

          allocator(&Base::GetDefaultAllocator()) {

        const Meta::DependencyProperty* descriptor =

            DependencyObjectAccess::PropertyRegistry((dependencyObject)).Find(dependencyProperty);

        if (descriptor != nullptr) property = descriptor->Handle();

    }



    struct Source {

        const ResourceDictionary* identity = nullptr;

        ResourceDictionary resources;

        ResourceChangeSubscription subscription;

    };



    Meta::EffectiveValueEngine* engine = nullptr;

    ::Aero::DependencyObject* target = nullptr;

    Meta::DependencyPropertyHandle property;

    Base::String key;

    Base::Vector<Source> sources;

    Base::IAllocator* allocator = nullptr;

};



Base::Result<Meta::PropertyValue> ConvertDynamicResourceValue(

    const Meta::PropertyValue& value,

    const Meta::DependencyProperty& property) noexcept {

    if (property.AcceptsAnyValue() ||

        value.Type() == property.ValueType() ||

        value.IsNullObject()) {

        return value;

    }

    if (property.ValueType() == Meta::TypeOf<Aero::Length>()) {

        Base::Result<long double> number = ReadConstantBindingNumber(value);

        if (number) {

            return Meta::ValueCodec<Aero::Length>::Encode(

                Aero::Length::Pixels(

                    static_cast<double>(number.Value())));

        }

    }

    if (property.ValueType() == Meta::TypeOf<Aero::GridLength>()) {

        Base::Result<long double> number = ReadConstantBindingNumber(value);

        if (number) {

            return Meta::ValueCodec<Aero::GridLength>::Encode(

                Aero::GridLength::Pixel(

                    static_cast<double>(number.Value())));

        }

    }

    if (property.ValueType() == Meta::TypeOf<Base::Thickness>()) {

        Base::Result<long double> number = ReadConstantBindingNumber(value);

        if (number) {

            const double size = static_cast<double>(number.Value());

            return Meta::ValueCodec<Base::Thickness>::Encode(

                Base::Thickness{size, size, size, size});

        }

    }

    if (value.Kind() == Meta::ValueKind::String) {

        return Meta::PropertyValue::TryFromString(

            property.ValueType(), value.AsString());

    }

    return value;

}



Base::Result<Meta::PropertyValue> MissingDynamicResourceValue(

    ::Aero::DependencyObject& object,

    const Meta::DependencyProperty* descriptor) noexcept {

    if (descriptor != nullptr) {

        const Meta::PropertyMetadata* metadata =

            descriptor->MetadataFor(object.RuntimeType());

        if (metadata == nullptr) {

            metadata = descriptor->MetadataFor(

                descriptor->RegisteredOwnerType());

        }

        if (metadata != nullptr && !metadata->defaultValue.IsUnset()) {

            return metadata->defaultValue;

        }

        if (descriptor->ValueType() == Meta::TypeOf<Base::String>()) {

            Base::Result<Meta::PropertyValue> empty =

                Meta::PropertyValue::TryFromString(

                    descriptor->ValueType(), Base::StringView{});

            if (empty) {

                return empty;

            }

        }

        return Meta::PropertyValue::NullObject(descriptor->ValueType());

    }

    return Meta::PropertyValue::NullObject(

        Meta::TypeOf<Base::Object>());

}



Base::Result<Meta::PropertyValue> ConvertLookedUpDynamicResource(

    const Aero::ResourceValue& resource,

    const Meta::DependencyProperty* descriptor) noexcept {

    return descriptor != nullptr

        ? ConvertDynamicResourceValue(resource, *descriptor)

        : Base::Result<Meta::PropertyValue>(resource);

}



Base::Result<Meta::PropertyValue> EvaluateDynamicResource(

    void* context,

    ::Aero::DependencyObject& object,

    Meta::DependencyPropertyHandle property) noexcept {

    DynamicResourceState* state = static_cast<DynamicResourceState*>(context);

    const Meta::DependencyProperty* descriptor =

        DependencyObjectAccess::PropertyRegistry((object)).Find(property);

    if (descriptor != nullptr) property = descriptor->Handle();

    if (state == nullptr ||

        state->target != &object || state->property != property) {

        return Base::Status::Failure(

            Base::ErrorCode::InvalidState,

            "DynamicResource expression state is invalid");

    }

    FrameworkElement* element =

        ::Aero::TryCast<FrameworkElement>(&object);

    if (element != nullptr) {

        Base::Result<Aero::ResourceValue> resource =

            element->FindResource(state->key.View());

        if (resource) {

            return ConvertLookedUpDynamicResource(

                resource.Value(), descriptor);

        }

        if (resource.GetStatus().code !=

            Base::ErrorCode::NotFound) {

            return resource.GetStatus();

        }

    }

    for (const DynamicResourceState::Source& source :

         state->sources) {

        if (source.identity == nullptr) {

            continue;

        }

        Base::Result<Aero::ResourceValue> resource =

            source.resources.Lookup(state->key.View());

        if (resource) {

            return ConvertLookedUpDynamicResource(

                resource.Value(), descriptor);

        }

        if (resource.GetStatus().code !=

            Base::ErrorCode::NotFound) {

            return resource.GetStatus();

        }

    }

    // WPF DynamicResource does not fail the tree when the key is still

    // absent; the target keeps its default until a later resource change.

    return MissingDynamicResourceValue(object, descriptor);

}



void ResourceChanged(

    void* context,

    Base::StringView key,

    Aero::ResourceChangeKind,

    std::uint64_t) noexcept {

    DynamicResourceState* state = static_cast<DynamicResourceState*>(context);

    if (state == nullptr || state->engine == nullptr || state->target == nullptr) {

        return;

    }

    if (!key.Empty() && key != state->key.View()) {

        return;

    }

    static_cast<void>(state->engine->Invalidate(*state->target, state->property));

}



Base::Vector<DynamicResourceState*>& LiveDynamicResources() noexcept {
    static Base::Vector<DynamicResourceState*> states;
    return states;
}

void TrackDynamicResource(DynamicResourceState* state) noexcept {
    if (state == nullptr) return;
    for (DynamicResourceState* existing : LiveDynamicResources()) {
        if (existing == state) return;
    }
    LiveDynamicResources().PushBack(state);
}

void UntrackDynamicResource(DynamicResourceState* state) noexcept {
    Base::Vector<DynamicResourceState*>& states = LiveDynamicResources();
    for (std::uint32_t index = 0U; index < states.Size(); ++index) {
        if (states[index] != state) continue;
        states[index] = states[states.Size() - 1U];
        states.PopBack();
        return;
    }
}

void CleanupDynamicResource(void* context) noexcept {

    DynamicResourceState* state = static_cast<DynamicResourceState*>(context);

    UntrackDynamicResource(state);

    if (state == nullptr) {

        return;

    }

    Base::IAllocator* allocator = state->allocator;

    for (DynamicResourceState::Source& source :

         state->sources) {

        if (source.identity != nullptr) {

            static_cast<void>(

                source.resources.Unsubscribe(

                    source.subscription));

        }

    }

    state->~DynamicResourceState();

    allocator->Deallocate(

        state,

        sizeof(DynamicResourceState),

        alignof(DynamicResourceState),

        Base::MemoryTag::Markup);

}





struct DeferredDynamicResourceState {

    Meta::EffectiveValueEngine* engine = nullptr;

    Base::Ref<::Aero::DependencyObject> targetOwner;

    ::Aero::DependencyObject* target = nullptr;

    Meta::DependencyPropertyHandle property;

    Base::Vector<ResourceDictionary> resources;

    ResourceDictionary fallbackResources;

    bool hasFallbackResources = false;

    Base::String key;

    Base::IAllocator* allocator = nullptr;

};





Base::Result<void> BindDynamicResourceRuntime(

    void* context, const EffectServices& services) noexcept {

    auto* state = static_cast<DeferredDynamicResourceState*>(context);

    if (state == nullptr || services.effectiveValues == nullptr) {

        return Base::Status::Failure(

            Base::ErrorCode::InvalidState,

            "DynamicResource requires mounted View value services");

    }

    state->engine = services.effectiveValues;

    if (!state->hasFallbackResources && services.fallbackResources != nullptr) {

        Base::Result<ResourceDictionary> shared =

            services.fallbackResources->Share();

        if (!shared) return shared.GetStatus();

        state->fallbackResources = std::move(shared).Value();

        state->hasFallbackResources = true;

    }

    return {};

}



Base::Result<std::uint64_t> CommitDynamicResource(void* context) noexcept {

    auto* state = static_cast<DeferredDynamicResourceState*>(context);

    if (state == nullptr || state->engine == nullptr ||

        state->target == nullptr) {

        return Base::Status::Failure(

            Base::ErrorCode::InvalidState,

            "Deferred DynamicResource state is invalid");

    }

    Base::Vector<const ResourceDictionary*> chain;

    chain.Reserve(

        state->resources.Size());

    for (const ResourceDictionary& resources : state->resources) {

        chain.PushBack(&resources);

    }

    Base::Result<void> attached = DynamicResource::Attach(

        *state->engine,

        {chain.Data(), chain.Size()},

        state->hasFallbackResources

            ? &state->fallbackResources

            : nullptr,

        *state->target, state->property, state->key.View());

    return attached

        ? Base::Result<std::uint64_t>(1U)

        : Base::Result<std::uint64_t>(attached.GetStatus());

}



void RollbackDynamicResource(

    void* context, std::uint64_t token) noexcept {

    auto* state = static_cast<DeferredDynamicResourceState*>(context);

    if (state != nullptr && token != 0U && state->engine != nullptr &&

        state->target != nullptr) {

        // Deferred resource effects are transaction-scoped. Teardown must

        // remove every queued effective-value record before targetOwner is

        // released; ClearLocalExpression would enqueue a final refresh and

        // leave a dangling object pointer in the engine.

        static_cast<void>(state->engine->DetachObject(*state->target));

    }

}



void CleanupDeferredDynamicResource(void* context) noexcept {

    auto* state = static_cast<DeferredDynamicResourceState*>(context);

    if (state == nullptr) return;

    Base::IAllocator* allocator = state->allocator;

    state->~DeferredDynamicResourceState();

    allocator->Deallocate(

        state, sizeof(DeferredDynamicResourceState),

        alignof(DeferredDynamicResourceState), Base::MemoryTag::Markup);

}



} // namespace



Base::Result<void> DynamicResource::Attach(

    Meta::EffectiveValueEngine& effectiveValues,

    ResourceDictionary& resources,

    ::Aero::DependencyObject& target,

    Meta::DependencyPropertyHandle property,

    Base::StringView key) noexcept {

    const ResourceDictionary* chain[] = {&resources};

    return Attach(

        effectiveValues,

        {chain, 1U},

        nullptr,

        target,

        property,

        key);

}



Base::Result<Meta::PropertyExpression> DynamicResource::CreateExpression(

    Meta::EffectiveValueEngine& effectiveValues,

    Base::Span<const ResourceDictionary* const> resourceChain,

    ResourceDictionary* fallbackResources,

    ::Aero::DependencyObject& target,

    Meta::DependencyPropertyHandle property,

    Base::StringView key) noexcept {

    const Base::StringView normalizedKey = TrimAscii(key);

    if (!property.IsValid() || normalizedKey.Empty()) {

        return Base::Status::Failure(

            Base::ErrorCode::InvalidArgument,

            "DynamicResource requires a target property and a non-empty key");

    }

    for (const ResourceDictionary* resources :

         resourceChain) {

        if (resources == nullptr) continue;

        Base::Result<Aero::ResourceValue> existing =

            resources->Lookup(normalizedKey);

        if (!existing &&

            existing.GetStatus().code !=

                Base::ErrorCode::NotFound) {

            return existing.GetStatus();

        }

    }

    if (fallbackResources != nullptr) {

        Base::Result<Aero::ResourceValue> existing =

            fallbackResources->Lookup(normalizedKey);

        if (!existing &&

            existing.GetStatus().code !=

                Base::ErrorCode::NotFound) {

            return existing.GetStatus();

        }

    }



    Base::IAllocator* stateAllocator = &Base::GetDefaultAllocator();

    void* memory = stateAllocator->Allocate({

        sizeof(DynamicResourceState),

        alignof(DynamicResourceState),

        Base::MemoryTag::Markup});

    if (memory == nullptr) {

        return Base::Status::Failure(

            Base::ErrorCode::OutOfMemory,

            "DynamicResource expression allocation failed");

    }

    DynamicResourceState* state = new (memory) DynamicResourceState(

        effectiveValues, target, property);

    Base::Result<void> assigned = state->key.Assign(normalizedKey);

    if (!assigned) {

        state->~DynamicResourceState();

        stateAllocator->Deallocate(

            memory, sizeof(DynamicResourceState), alignof(DynamicResourceState),

            Base::MemoryTag::Markup);

        return assigned.GetStatus();

    }

    auto subscribe =

        [state](ResourceDictionary* resources) noexcept

            -> Base::Result<void> {

        if (resources == nullptr) return {};

        for (const DynamicResourceState::Source& source :

             state->sources) {

            if (source.identity == resources) return {};

        }

        Base::Result<ResourceChangeSubscription> subscription =

            resources->SubscribeChanged(

                &ResourceChanged, state);

        if (!subscription) {

            return subscription.GetStatus();

        }

        Base::Result<ResourceDictionary> shared =

            resources->Share();

        if (!shared) {

            static_cast<void>(resources->Unsubscribe(

                subscription.Value()));

            return shared.GetStatus();

        }

        DynamicResourceState::Source source;

        source.identity = resources;

        source.resources = std::move(shared).Value();

        source.subscription = subscription.Value();

        state->sources.PushBack(std::move(source));

        return {};

    };

    FrameworkElement* current =

        ::Aero::TryCast<FrameworkElement>(&target);

    while (current != nullptr) {

        assigned = subscribe(&current->GetResources());

        if (!assigned) {

            CleanupDynamicResource(state);

            return assigned.GetStatus();

        }

        FrameworkElement* next = ::Aero::TryCast<FrameworkElement>(

            current->GetLogicalParent());

        if (next == nullptr) {

            next = ::Aero::TryCast<FrameworkElement>(current->GetVisualParent());

        }

        if (next == nullptr) {

            next = ::Aero::TryCast<FrameworkElement>(current->GetTemplatedParent());

        }

        current = next;

    }

    for (const ResourceDictionary* resources :

         resourceChain) {

        assigned = subscribe(

            const_cast<ResourceDictionary*>(resources));

        if (!assigned) {

            CleanupDynamicResource(state);

            return assigned.GetStatus();

        }

    }

    assigned = subscribe(fallbackResources);

    if (!assigned) {

        CleanupDynamicResource(state);

        return assigned.GetStatus();

    }



    TrackDynamicResource(state);

    return Meta::PropertyExpression{

        state,

        &EvaluateDynamicResource,

        &CleanupDynamicResource,

        Meta::PropertyExpressionKind::DynamicResource};

}



Base::Result<void> DynamicResource::Attach(

    Meta::EffectiveValueEngine& effectiveValues,

    Base::Span<const ResourceDictionary* const> resourceChain,

    ResourceDictionary* fallbackResources,

    ::Aero::DependencyObject& target,

    Meta::DependencyPropertyHandle property,

    Base::StringView key) noexcept {

    Base::Result<Meta::PropertyExpression> expression = CreateExpression(

        effectiveValues,

        resourceChain,

        fallbackResources,

        target,

        property,

        key);

    if (!expression) return expression.GetStatus();

    Base::Result<void> installed = effectiveValues.SetLocalExpression(

        target, property, expression.Value());

    if (!installed && expression.Value().cleanup != nullptr) {

        expression.Value().cleanup(expression.Value().context);

    }

    return installed;

}

Base::Result<void> AttachDeferredStyleDynamicResource(
    Meta::EffectiveValueEngine& engine,
    DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    Base::StringView key) noexcept {
    return DynamicResource::Attach(
        engine,
        Base::Span<const ResourceDictionary* const>{},
        nullptr,
        target,
        property,
        key);
}

void NotifyDynamicResourceScopeChanged(
    DependencyObject& object) noexcept {
    for (DynamicResourceState* state : LiveDynamicResources()) {
        if (state == nullptr || state->engine == nullptr ||
            state->target != &object) {
            continue;
        }
        static_cast<void>(
            state->engine->Invalidate(*state->target, state->property));
    }
}

DynamicResourceExtension::DynamicResourceExtension(

    const DynamicResourceExtensionOptions& options) noexcept

    : options_(options) {}



Base::Result<void> DynamicResourceExtension::Register(

    Schema& schema,

    Meta::TypeId dynamicResourceExtensionType) noexcept {

    return schema.AddMarkupExtension({

        dynamicResourceExtensionType,

        &DynamicResourceExtension::ProvideValue,

        this});

}



Base::Result<ProvidedValue> DynamicResourceExtension::ProvideValue(

    Base::StringView arguments,

    const ExtensionServices& services,

    void* context) noexcept {

    DynamicResourceExtension* extension =

        static_cast<DynamicResourceExtension*>(context);

    if (extension == nullptr ||

        services.schema == nullptr || services.targetObject == nullptr ||

        services.targetMember == Meta::InvalidMemberId) {

        return Base::Status::Failure(

            Base::ErrorCode::InvalidState,

            "DynamicResource markup extension has no target service context");

    }

    Base::StringView key = TrimAscii(arguments);

    constexpr Base::StringView ResourceKeyPrefix("ResourceKey=");

    if (key.SizeBytes() > ResourceKeyPrefix.SizeBytes() &&

        key.Substr(0U, ResourceKeyPrefix.SizeBytes()) ==

            ResourceKeyPrefix) {

        key = TrimAscii(key.Substr(

            ResourceKeyPrefix.SizeBytes(),

            key.SizeBytes() - ResourceKeyPrefix.SizeBytes()));

    }

    if (key.Empty() || key.Data() == nullptr) {

        return Base::Status::Failure(

            Base::ErrorCode::ValidationFailed,

            "DynamicResource requires a resource key");

    }

    // A Setter is an authored style record rather than a DependencyObject.

    // Resolve its current resource value while the style dictionary is being

    // built; the style finalizer subsequently converts that value for the

    // target dependency property.

    if (services.targetObject->RuntimeType() ==

        Aero::Setter::StaticTypeId()) {

        // Keep the key live. A resolved brush would stay tied to the dictionary
        // that defined the style, so Gallery's selectable Light palette could
        // not override shell brushes such as Brush.Window.Background.
        Base::Result<Base::Ref<Data::Binding>> marker =
            Base::MakeRef<Data::Binding>();
        if (!marker) return marker.GetStatus();
        Base::String markerPath;
        Base::Result<void> encodedKey =
            markerPath.Assign("\x01DynamicResource:");
        if (encodedKey) encodedKey = markerPath.Append(key);
        if (!encodedKey) return encodedKey.GetStatus();
        marker.Value()->SetPath(markerPath.View());
        Base::Result<Meta::Value> markerValue = Meta::Value::FromObject(
            Data::Binding::StaticTypeId(),
            Base::Ref<Base::Object>(std::move(marker).Value()));
        if (!markerValue) return markerValue.GetStatus();
        return ProvidedValue::FromValue(std::move(markerValue).Value());

        // Template/style setters are authored before their eventual target

        // exists. Resolve from the complete parse-time resource scope, not

        // only the immediate fallback dictionary: a theme's brushes commonly

        // live in an earlier merged sibling dictionary.

        for (const ResourceDictionary* resources :

             services.ambientResourceChain) {

            if (resources == nullptr) continue;

            Base::Result<Aero::ResourceValue> resource =

                resources->Lookup(key);

            if (resource) {

                return ProvidedValue::FromValue(

                    std::move(resource).Value());

            }

            if (resource.GetStatus().code !=

                Base::ErrorCode::NotFound) {

                return resource.GetStatus();

            }

        }

        if (services.fallbackResources != nullptr) {

            Base::Result<Aero::ResourceValue> resource =

                services.fallbackResources->Lookup(key);

            if (resource) {

                return ProvidedValue::FromValue(

                    std::move(resource).Value());

            }

            if (resource.GetStatus().code !=

                Base::ErrorCode::NotFound) {

                return resource.GetStatus();

            }

        }

        // WPF does not fail dictionary construction for a DynamicResource

        // whose key is currently absent. Preserve an unset object value until

        // the eventual style-instance resource expression can evaluate it.

        return ProvidedValue::FromValue(

            Meta::Value::NullObject(

                Meta::TypeOf<Base::Object>()));

    }

    Base::Result<::Aero::DependencyObject*> targetResult =

        services.schema->ResolvePropertyTarget(

            *services.targetObject);

    if (!targetResult) {

        return targetResult.GetStatus();

    }

    ::Aero::DependencyObject* target = targetResult.Value();

    const Meta::DependencyPropertyHandle property{services.targetMember};

    if (services.deferredContentOwner != nullptr &&

        services.deferredContentOwner->RuntimeType() ==

            Controls::ControlTemplate::StaticTypeId()) {

        auto& controlTemplate =

            static_cast<Controls::ControlTemplate&>(

                *services.deferredContentOwner);

        Base::String targetName;

        Base::Result<void> captured = CaptureControlTemplateChildName(

            controlTemplate,

            services.nameScope,

            *target,

            targetName);

        if (!captured) return captured.GetStatus();

        Base::Result<void> retained =

            ::Aero::Controls::FrameworkTemplateState::AddDynamicResource(

                controlTemplate,

                targetName.View(),

                key,

                property);

        return retained

            ? Base::Result<ProvidedValue>(ProvidedValue::Handled())

            : Base::Result<ProvidedValue>(retained.GetStatus());

    }

    Meta::EffectiveValueEngine* effectiveValues =

        services.effectiveValues != nullptr

        ? services.effectiveValues

        : extension->options_.effectiveValues;

    ResourceDictionary* fallbackResources =

        services.fallbackResources != nullptr

        ? services.fallbackResources

        : extension->options_.resources;



    Base::Result<Aero::ResourceValue> initialResource =

        Base::Status::Failure(Base::ErrorCode::NotFound, "Resource not found");

    for (const ResourceDictionary* resDict : services.ambientResourceChain) {

        if (resDict == nullptr) continue;

        Base::Result<Aero::ResourceValue> found = resDict->Lookup(key);

        if (found) {

            initialResource = std::move(found);

            break;

        }

    }

    if (!initialResource && fallbackResources != nullptr) {

        initialResource = fallbackResources->Lookup(key);

    }

    if (initialResource) {

        const Meta::DependencyProperty* descriptor =

            DependencyObjectAccess::PropertyRegistry((*target)).Find(property);

        Base::Result<Meta::PropertyValue> converted =

            ConvertLookedUpDynamicResource(initialResource.Value(), descriptor);

        if (converted) {

            static_cast<void>(target->SetValue(property, std::move(converted).Value()));

        }

    }

    Base::IAllocator& allocator = Base::GetDefaultAllocator();

    void* memory = allocator.Allocate({

        sizeof(DeferredDynamicResourceState),

        alignof(DeferredDynamicResourceState),

        Base::MemoryTag::Markup});

    if (memory == nullptr) {

        return Base::Status::Failure(

            Base::ErrorCode::OutOfMemory,

            "Deferred DynamicResource allocation failed");

    }

    auto* state = new (memory) DeferredDynamicResourceState();

    state->engine = effectiveValues;

    state->targetOwner =

        Base::Ref<::Aero::DependencyObject>::TryFromBorrowed(*target);

    if (!state->targetOwner) {

        CleanupDeferredDynamicResource(state);

        return Base::Status::Failure(

            Base::ErrorCode::InvalidState,

            "DynamicResource target is not reference-counted");

    }

    state->target = state->targetOwner.Get();

    const Meta::DependencyProperty* descriptor =

        DependencyObjectAccess::PropertyRegistry(target).Find(property);

    state->property = descriptor != nullptr

        ? descriptor->Handle()

        : property;

    state->allocator = &allocator;

    state->resources.Reserve(

        services.ambientResourceChain.Size());

    Base::Result<void> reserved{};

    for (const ResourceDictionary* resource :

         services.ambientResourceChain) {

        if (resource == nullptr) continue;

        Base::Result<ResourceDictionary> shared =

            resource->Share();

        if (!shared) {

            reserved = shared.GetStatus();

            break;

        }

        state->resources.PushBack(

            std::move(shared).Value());

    }

    if (reserved && fallbackResources != nullptr) {

        Base::Result<ResourceDictionary> shared =

            fallbackResources->Share();

        if (!shared) {

            reserved = shared.GetStatus();

        } else {

            state->fallbackResources =

                std::move(shared).Value();

            state->hasFallbackResources = true;

        }

    }

    if (reserved) reserved = state->key.Assign(key);

    if (!reserved) {

        CleanupDeferredDynamicResource(state);

        return reserved.GetStatus();

    }

    return ProvidedValue::Deferred(

        state,

        &CommitDynamicResource,

        &RollbackDynamicResource,

        &CleanupDeferredDynamicResource,

        nullptr,

        &BindDynamicResourceRuntime);

}



} // namespace Aero::Markup





// ===== StaticResourceExtension.inl =====

// ===== StaticResourceExtension =====



namespace Aero::Markup {



Base::Result<void> StaticResourceExtension::Register(

    Schema& schema,

    Meta::TypeId staticResourceExtensionType) noexcept {

    return schema.AddMarkupExtension({

        staticResourceExtensionType,

        &StaticResourceExtension::ProvideValue,

        this});

}



Base::Result<ProvidedValue> StaticResourceExtension::ProvideValue(

    Base::StringView arguments,

    const ExtensionServices& services,

    void* context) noexcept {

    StaticResourceExtension* extension =

        static_cast<StaticResourceExtension*>(context);

    if (extension == nullptr ||

        services.schema == nullptr || services.targetObject == nullptr ||

        services.targetMember == Meta::InvalidMemberId) {

        return Base::Status::Failure(

            Base::ErrorCode::InvalidState,

            "StaticResource markup extension has no target service context");

    }

    Base::StringView key = TrimAscii(arguments);

    constexpr Base::StringView ResourceKeyPrefix("ResourceKey=");

    if (key.SizeBytes() > ResourceKeyPrefix.SizeBytes() &&

        key.Substr(0U, ResourceKeyPrefix.SizeBytes()) ==

            ResourceKeyPrefix) {

        key = TrimAscii(key.Substr(

            ResourceKeyPrefix.SizeBytes(),

            key.SizeBytes() - ResourceKeyPrefix.SizeBytes()));

    }

    if (key.Empty() || key.Data() == nullptr) {

        return Base::Status::Failure(

            Base::ErrorCode::ValidationFailed,

            "StaticResource requires a resource key");

    }

    // A Setter is an authored style record rather than a DependencyObject.

    // Resolve its current resource value while the style dictionary is being

    // built; the style finalizer subsequently converts that value for the

    // target dependency property.

    if (services.targetObject->RuntimeType() ==

        Aero::Setter::StaticTypeId()) {

        for (const ResourceDictionary* resources :

             services.ambientResourceChain) {

            if (resources == nullptr) continue;

            Base::Result<Aero::ResourceValue> resource =

                resources->Lookup(key);

            if (resource) {

                return ProvidedValue::FromValue(

                    std::move(resource).Value());

            }

            if (resource.GetStatus().code !=

                Base::ErrorCode::NotFound) {

                return resource.GetStatus();

            }

        }

        if (services.fallbackResources != nullptr) {

            Base::Result<Aero::ResourceValue> resource =

                services.fallbackResources->Lookup(key);

            if (resource) {

                return ProvidedValue::FromValue(

                    std::move(resource).Value());

            }

            if (resource.GetStatus().code !=

                Base::ErrorCode::NotFound) {

                return resource.GetStatus();

            }

        }

        return ProvidedValue::FromValue(

            Meta::Value::NullObject(

                Meta::TypeOf<Base::Object>()));

    }

    Base::Result<::Aero::DependencyObject*> targetResult =

        services.schema->ResolvePropertyTarget(

            *services.targetObject);

    if (!targetResult) {

        return targetResult.GetStatus();

    }

    ::Aero::DependencyObject* target = targetResult.Value();

    const Meta::DependencyPropertyHandle property{services.targetMember};

    if (services.deferredContentOwner != nullptr &&

        services.deferredContentOwner->RuntimeType() ==

            Controls::ControlTemplate::StaticTypeId()) {

        Base::String targetName;

        Base::Result<void> captured = CaptureControlTemplateChildName(

            static_cast<Controls::ControlTemplate&>(

                *services.deferredContentOwner),

            services.nameScope,

            *target,

            targetName);

        if (!captured) return captured.GetStatus();

        Base::Result<void> retained =

            ::Aero::Controls::FrameworkTemplateState::AddDynamicResource(

                static_cast<Controls::ControlTemplate&>(

                    *services.deferredContentOwner),

                targetName.View(),

                key,

                property);

        return retained

            ? Base::Result<ProvidedValue>(ProvidedValue::Handled())

            : Base::Result<ProvidedValue>(retained.GetStatus());

    }

    const Meta::DependencyProperty* descriptor =

        DependencyObjectAccess::PropertyRegistry(target).Find(property);

    auto resolveFrom =

        [&](const ResourceDictionary* resources)

            -> Base::Result<Meta::PropertyValue> {

        if (resources == nullptr) {

            return Base::Status::Failure(

                Base::ErrorCode::NotFound, "StaticResource scope is absent");

        }

        Base::Result<Aero::ResourceValue> resource =

            resources->Lookup(key);

        if (resource) {

            return ConvertLookedUpDynamicResource(

                resource.Value(), descriptor);

        }

        return resource.GetStatus();

    };

    for (const ResourceDictionary* resources :

         services.ambientResourceChain) {

        Base::Result<Meta::PropertyValue> value = resolveFrom(resources);

        if (value) {

            return ProvidedValue::FromValue(

                std::move(value).Value());

        }

        if (value.GetStatus().code != Base::ErrorCode::NotFound) {

            return value.GetStatus();

        }

    }

    if (services.fallbackResources != nullptr) {

        Base::Result<Meta::PropertyValue> value =

            resolveFrom(services.fallbackResources);

        if (value) {

            return ProvidedValue::FromValue(

                std::move(value).Value());

        }

        if (value.GetStatus().code != Base::ErrorCode::NotFound) {

            return value.GetStatus();

        }

    }

    if (services.deferredContentOwner != nullptr) {

        Aero::ResourceDictionary* templateResources = nullptr;

        const Meta::TypeId ownerType =

            services.deferredContentOwner->RuntimeType();

        if (ownerType == ::Aero::DataTemplate::StaticTypeId() ||

            ownerType == ::Aero::HierarchicalDataTemplate::StaticTypeId()) {

            templateResources =

                &static_cast<::Aero::DataTemplate&>(

                    *services.deferredContentOwner).GetResources();

        } else if (

            ownerType == Controls::ItemsPanelTemplate::StaticTypeId()) {

            templateResources =

                &static_cast<Controls::ItemsPanelTemplate&>(

                    *services.deferredContentOwner).GetResources();

        }

        if (templateResources != nullptr) {

            Base::Result<Meta::PropertyValue> value =

                resolveFrom(templateResources);

            if (value) {

                return ProvidedValue::FromValue(

                    std::move(value).Value());

            }

            if (value.GetStatus().code !=

                Base::ErrorCode::NotFound) {

                return value.GetStatus();

            }

        }

    }

    // WPF StaticResource fails when the key is absent, but AeroGUI keeps the

    // tree alive by falling back to the property default so the gallery and

    // sample resources never crash the load.

    return ProvidedValue::FromValue(

        MissingDynamicResourceValue(*target, descriptor).Value());

}



} // namespace Aero::Markup





// ===== LocExtension.inl =====

// ===== LocExtension =====



namespace Aero::Markup {



namespace {



// The original AeroGUI sample uses Loc as a markup extension backed by a

// ResourceDictionary whose Source is data-bound at runtime.  A ResourceUri is

// intentionally only a URI value in the core property system, so this small

// bridge performs the missing dictionary swap while preserving the original

// XAML files as the sole source of translations and flag assets.

struct LocTarget {

    // Loc is a dynamic expression-like subscriber.  It must never own a

    // visual: the view owns that lifetime and may tear its property system

    // down before this process-wide compatibility registry is destroyed.

    Base::WeakRef<::Aero::DependencyObject> object;

    Meta::DependencyPropertyHandle property;

    Meta::TypeId targetType = Meta::InvalidTypeId;

    Base::String key;

    Base::ResourceUri baseUri;

};



Base::Vector<LocTarget>& LocTargets() {

    static Base::Vector<LocTarget> targets;

    return targets;

}



std::string ToNativeString(Base::StringView value) {

    return value.Data() == nullptr

        ? std::string{}

        : std::string(value.Data(), value.SizeBytes());

}



bool ReadLocDictionary(

    const Base::ResourceUri& uri,

    std::string& document) noexcept {

    std::ifstream input(ToNativeString(uri.Path()), std::ios::binary);

    if (!input) {

        // The desktop sample is packaged beside its executable.  This

        // fallback also handles a provider URI whose native Path is empty.

        input.open(ToNativeString(uri.Canonical()), std::ios::binary);

    }

    if (!input) return false;

    document.assign(

        std::istreambuf_iterator<char>(input),

        std::istreambuf_iterator<char>());

    return !document.empty();

}



bool ReadLocString(

    const std::string& document,

    Base::StringView key,

    Base::String& value) noexcept {

    const std::string keyText = ToNativeString(key);

    const std::string marker = "x:Key=\"" + keyText + "\"";

    const std::size_t markerAt = document.find(marker);

    if (markerAt == std::string::npos) return false;

    const std::size_t valueBegin = document.find('>', markerAt);

    if (valueBegin == std::string::npos) return false;

    const std::size_t valueEnd = document.find("</", valueBegin + 1U);

    if (valueEnd == std::string::npos || valueEnd < valueBegin) return false;

    return value.Assign(Base::StringView(

        document.data() + valueBegin + 1U,

        static_cast<std::uint32_t>(valueEnd - valueBegin - 1U))).HasValue();

}



bool ReadLocFlag(

    const std::string& document,

    Base::String& imageSource) noexcept {

    const std::size_t flagAt = document.find("x:Key=\"Flag\"");

    if (flagAt == std::string::npos) return false;

    const std::string marker = "ImageSource=\"";

    const std::size_t sourceAt = document.find(marker, flagAt);

    if (sourceAt == std::string::npos) return false;

    const std::size_t sourceBegin = sourceAt + marker.size();

    const std::size_t sourceEnd = document.find('"', sourceBegin);

    if (sourceEnd == std::string::npos) return false;

    return imageSource.Assign(Base::StringView(

        document.data() + sourceBegin,

        static_cast<std::uint32_t>(sourceEnd - sourceBegin))).HasValue();

}



Base::Result<Meta::Value> ReadLocValue(

    const Base::ResourceUri& dictionaryUri,

    Base::StringView key,

    Meta::TypeId targetType) noexcept {

    std::string document;

    if (!ReadLocDictionary(dictionaryUri, document)) {

        return Base::Status::Failure(

            Base::ErrorCode::NotFound,

            "Localization dictionary could not be opened");

    }

    if (key == Base::StringView("Flag")) {

        Base::String imageFile;

        if (!ReadLocFlag(document, imageFile)) {

            return Base::Status::Failure(

                Base::ErrorCode::NotFound,

                "Localization flag resource was not found");

        }

        Base::Result<Base::ResourceUri> imageUri =

            Base::ResourceUri::Resolve(dictionaryUri, imageFile.View());

        if (!imageUri) return imageUri.GetStatus();

        Base::Result<Base::Ref<Media::BitmapImage>> bitmap =

            Base::MakeRef<Media::BitmapImage>();

        if (!bitmap) return bitmap.GetStatus();

        bitmap.Value()->SetUriSource(imageUri.Value());

        Base::Result<Base::Ref<Media::ImageBrush>> brush =

            Base::MakeRef<Media::ImageBrush>();

        if (!brush) return brush.GetStatus();

        brush.Value()->SetSource(Base::Ref<Media::ImageSource>(

            std::move(bitmap).Value()));

        return Meta::Value::FromObject(

            targetType,

            Base::Ref<Base::Object>(std::move(brush).Value()));

    }

    Base::String text;

    if (!ReadLocString(document, key, text)) {

        return Base::Status::Failure(

            Base::ErrorCode::NotFound,

            "Localization string resource was not found");

    }

    return Meta::Value::TryFromString(

        Meta::TypeOf<Base::String>(), text.View());

}



Base::Result<Base::ResourceUri> ResolveLocDictionary(

    const Base::ResourceUri* baseUri,

    Base::StringView source) noexcept {

    if (baseUri != nullptr && !baseUri->Empty()) {

        return Base::ResourceUri::Resolve(*baseUri, source);

    }

    return Base::ResourceUri::Parse(source);

}



void RefreshLocTargets(const Base::ResourceUri& dictionaryUri) noexcept {

    for (LocTarget& target : LocTargets()) {

        Base::Ref<::Aero::DependencyObject> object = target.object.Lock();

        if (!object || !target.property.IsValid()) continue;

        Base::Result<Meta::Value> value = ReadLocValue(

            dictionaryUri, target.key.View(), target.targetType);

        if (value) {

            object->SetValue(target.property, value.Value());

        }

    }

}



} // namespace



Base::Result<void> LocExtension::Register(

    Schema& schema,

    Meta::TypeId markupExtensionType) noexcept {

    if (schema.IsFrozen() || markupExtensionType == Meta::InvalidTypeId) {

        return Base::Status::Failure(

            Base::ErrorCode::InvalidState,

            "Loc extension registration is invalid");

    }

    return schema.AddMarkupExtension({markupExtensionType, &ProvideValue, nullptr});

}



Base::Result<ProvidedValue> LocExtension::ProvideValue(

    Base::StringView arguments,

    const ExtensionServices& services,

    void* context) noexcept {

    if (context != nullptr || arguments.Empty() ||

        services.targetMember == Meta::InvalidMemberId) {

        return Base::Status::Failure(

            Base::ErrorCode::InvalidArgument,

            "Loc requires a non-empty resource key and target property");

    }



    Base::Result<::Aero::DependencyObject*> targetResult =

        services.schema->ResolvePropertyTarget( *services.targetObject);

    if (!targetResult) return targetResult.GetStatus();

    ::Aero::DependencyObject* target = targetResult.Value();

    const Meta::DependencyPropertyHandle property{services.targetMember};

    if (!property.IsValid()) {

        return Base::Status::Failure(

            Base::ErrorCode::NotFound,

            "Loc target property was not found");

    }



    // MainWindow's bound Source is not available until OnStartup supplies the

    // view model.  Load English for construction, then OnSourceChanged swaps

    // every registered value as soon as that binding produces a URI.

    Base::Result<Base::ResourceUri> english = ResolveLocDictionary(

        services.baseUri, "Language-en.xaml");

    if (!english) return english.GetStatus();

    Base::Result<Meta::Value> initial = ReadLocValue(

        english.Value(), arguments, services.targetValueType);

    if (!initial) return initial.GetStatus();



    LocTarget subscription;

    subscription.object = Base::WeakRef<::Aero::DependencyObject>(

        Base::Ref<::Aero::DependencyObject>::FromBorrowed(*target));

    subscription.property = property;

    subscription.targetType = services.targetValueType;

    Base::Result<void> key = subscription.key.Assign(arguments);

    if (!key) return key.GetStatus();

    subscription.baseUri = english.Value();

    LocTargets().PushBack(std::move(subscription));

    return ProvidedValue::FromValue(std::move(initial).Value());

}



void LocExtension::OnSourceChanged(

    ::Aero::DependencyObject&,

    const Meta::DependencyPropertyChangedEventArgs& args) noexcept {

    const Meta::Value& source = args.GetNewValue();

    if (source.Type() != Meta::TypeOf<Base::ResourceUri>()) return;

    Base::Result<Base::ResourceUri> dictionary =

        Meta::ValueCodec<Base::ResourceUri>::Decode(source);

    if (!dictionary || dictionary.Value().Empty()) return;

    // A Binding writes the URI exactly as supplied by the view model.  Those

    // values are often relative ("Language-ja.xaml"), while Loc's initial

    // dictionary was resolved against the loaded XAML document.  Re-resolve

    // relative updates from that same document before opening the dictionary,

    // otherwise the desktop process working directory is used and both the

    // language strings and the flag ImageBrush silently stay unavailable.

    Base::ResourceUri resolved = dictionary.Value();

    if (!resolved.IsAbsolute()) {

        for (const LocTarget& target : LocTargets()) {

            if (target.baseUri.Empty()) continue;

            Base::Result<Base::ResourceUri> relative =

                ResolveLocDictionary(

                    &target.baseUri, resolved.Canonical());

            if (relative) {

                resolved = std::move(relative).Value();

            }

            break;

        }

    }

    RefreshLocTargets(resolved);

}



} // namespace Aero::Markup





// ===== TemplateBindingExtension.inl =====

// ===== TemplateBindingExtension =====

















namespace Aero::Markup {

namespace {



Base::StringView PropertyLocalName(

    Base::StringView value) noexcept {

    value = ::Aero::Base::ValueConversion::Trim(value);

    std::uint32_t separator = UINT32_MAX;

    for (std::uint32_t index = 0U;

         index < value.SizeBytes();

         ++index) {

        if (value[index] == '.') {

            separator = index;

        }

    }

    return separator == UINT32_MAX

        ? value

        : value.Substr(

              separator + 1U,

              value.SizeBytes() - separator - 1U);

}



const char* MissingPropertyMessage(

    Base::StringView propertyName,

    bool source) noexcept {

    if (propertyName == Base::StringView("Content")) {

        return source

            ? "TemplateBinding source property Content was not found"

            : "TemplateBinding target property for Content was not found";

    }

    if (propertyName == Base::StringView("ContentTemplate")) {

        return source

            ? "TemplateBinding source property ContentTemplate was not found"

            : "TemplateBinding target property for ContentTemplate was not found";

    }

    if (propertyName == Base::StringView("ContentTemplateSelector")) {

        return source

            ? "TemplateBinding source property ContentTemplateSelector was not found"

            : "TemplateBinding target property for ContentTemplateSelector was not found";

    }

    if (propertyName == Base::StringView("CanContentScroll")) {

        return source

            ? "TemplateBinding source property CanContentScroll was not found"

            : "TemplateBinding target property for CanContentScroll was not found";

    }

    if (propertyName == Base::StringView("Padding")) {

        return source

            ? "TemplateBinding source property Padding was not found"

            : "TemplateBinding target property for Padding was not found";

    }

    return source

        ? "TemplateBinding source property was not found"

        : "TemplateBinding target property was not found";

}



} // namespace



Base::Result<void> TemplateBindingExtension::Register(

    Schema& schema,

    Meta::TypeId markupExtensionType) noexcept {

    if (schema.IsFrozen() ||

        markupExtensionType == Meta::InvalidTypeId) {

        return Base::Status::Failure(

            Base::ErrorCode::InvalidState,

            "TemplateBinding extension registration is invalid");

    }

    return schema.AddMarkupExtension(

        {markupExtensionType, &ProvideValue, nullptr});

}



Base::Result<ProvidedValue>

TemplateBindingExtension::ProvideValue(

    Base::StringView arguments,

    const ExtensionServices& services,

    void* context) noexcept {

    if (context != nullptr ||

        services.schema == nullptr ||

        services.targetObject == nullptr ||

        services.deferredContentOwner == nullptr ||

        services.nameScope == nullptr ||

        services.targetMember ==

            Meta::InvalidMemberId ||

        services.deferredContentOwner->RuntimeType() !=

            Controls::ControlTemplate::StaticTypeId()) {

        return Base::Status::Failure(

            Base::ErrorCode::InvalidState,

            "TemplateBinding requires an active ControlTemplate target");

    }



    const Base::StringView propertyName =

        PropertyLocalName(arguments);

    if (propertyName.Empty()) {

        return Base::Status::Failure(

            Base::ErrorCode::ValidationFailed,

            "TemplateBinding requires a source property");

    }



    auto& controlTemplate =

        static_cast<Controls::ControlTemplate&>(

            *services.deferredContentOwner);

    Base::Result<::Aero::DependencyObject*> target =

        services.schema->ResolvePropertyTarget(

            *services.targetObject);

    if (!target) return target.GetStatus();



    const Meta::DependencyProperty* source =

        DependencyObjectAccess::PropertyRegistry(target.Value()).Find(

            controlTemplate.GetTargetType(),

            propertyName);

    const Meta::DependencyProperty* destination =

        DependencyObjectAccess::PropertyRegistry(target.Value()).Find(

            Meta::DependencyPropertyHandle{

                services.targetMember});

    if (source == nullptr) {

        return Base::Status::Failure(

            Base::ErrorCode::NotFound,

            MissingPropertyMessage(propertyName, true));

    }

    if (destination == nullptr ||

        destination->MetadataFor(

            target.Value()->RuntimeType()) == nullptr) {

        return Base::Status::Failure(

            Base::ErrorCode::NotFound,

            MissingPropertyMessage(propertyName, false));

    }

    Base::String targetName;

    Base::Result<void> captured = CaptureControlTemplateChildName(

        controlTemplate,

        services.nameScope,

        *services.targetObject,

        targetName);

    if (!captured) {

        return captured.GetStatus();

    }

    Base::Result<void> added =

        ::Aero::Controls::FrameworkTemplateState::AddTemplateBinding(controlTemplate,

            targetName.View(),

            source->Handle(),

            destination->Handle());

    return added

        ? Base::Result<ProvidedValue>(

              ProvidedValue::Handled())

        : Base::Result<ProvidedValue>(

              added.GetStatus());

}



} // namespace Aero::Markup





// ===== TypeExtension.inl =====

// ===== TypeExtension =====













namespace Aero::Markup {

Base::Result<void> TypeExtension::Register(

    Schema& schema,

    Meta::TypeId markupExtensionType) noexcept {

    if (schema.IsFrozen() ||

        markupExtensionType == Meta::InvalidTypeId) {

        return Base::Status::Failure(

            Base::ErrorCode::InvalidState,

            "x:Type extension registration is invalid");

    }

    const Meta::TypeInfo* token =

        schema.Types().FindType(

            Meta::TypeOf<Meta::TypeReference>());

    if (token == nullptr ||

        (static_cast<std::uint32_t>(token->Flags()) &

            static_cast<std::uint32_t>(Meta::TypeFlags::ValueType)) == 0U) {

        return Base::Status::Failure(

            Base::ErrorCode::InvalidArgument,

            "x:Type reference token must be a value type");

    }

    return schema.AddMarkupExtension({

        markupExtensionType, &ProvideValue, nullptr});

}



Base::Result<ProvidedValue> TypeExtension::ProvideValue(

    Base::StringView arguments,

    const ExtensionServices& services,

    void* context) noexcept {

    if (context != nullptr || services.schema == nullptr) {

        return Base::Status::Failure(

            Base::ErrorCode::InvalidState,

            "x:Type extension context is invalid");

    }

    Base::Result<Meta::Value> value =

        services.schema->ConvertText(

            Meta::TypeOf<Meta::TypeReference>(),

            arguments,

            &services);

    return value

        ? ProvidedValue::FromValue(

              std::move(value).Value())

        : Base::Result<ProvidedValue>(

              value.GetStatus());

}



} // namespace Aero::Markup





// ===== StaticExtension.inl =====

// ===== StaticExtension =====













namespace Aero::Markup {

namespace {







} // namespace



Base::Result<void> StaticExtension::Register(

    Schema& schema,

    Meta::TypeId markupExtensionType) noexcept {

    if (schema.IsFrozen() ||

        markupExtensionType == Meta::InvalidTypeId) {

        return Base::Status::Failure(

            Base::ErrorCode::InvalidState,

            "x:Static extension registration is invalid");

    }

    return schema.AddMarkupExtension({

        markupExtensionType, &ProvideValue, nullptr});

}



Base::Result<ProvidedValue> StaticExtension::ProvideValue(

    Base::StringView arguments,

    const ExtensionServices& services,

    void* context) noexcept {

    if (context != nullptr ||

        services.schema == nullptr ||

        !services.namespaces.IsAvailable()) {

        return Base::Status::Failure(

            Base::ErrorCode::InvalidState,

            "x:Static extension context is invalid");

    }



    const Base::StringView expression = TrimAscii(arguments);

    std::uint32_t memberSeparator = expression.SizeBytes();

    for (std::uint32_t index = 0U;

         index < expression.SizeBytes(); ++index) {

        if (expression[index] == '.') memberSeparator = index;

    }

    if (memberSeparator == 0U ||

        memberSeparator + 1U >= expression.SizeBytes()) {

        return Base::Status::Failure(

            Base::ErrorCode::ValidationFailed,

            "x:Static requires Type.Member");

    }



    const Base::StringView qualifiedType =

        expression.Substr(0U, memberSeparator);

    const Base::StringView memberName =

        expression.Substr(

            memberSeparator + 1U,

            expression.SizeBytes() - memberSeparator - 1U);

    std::uint32_t prefixSeparator = qualifiedType.SizeBytes();

    for (std::uint32_t index = 0U;

         index < qualifiedType.SizeBytes(); ++index) {

        if (qualifiedType[index] == ':') {

            if (prefixSeparator != qualifiedType.SizeBytes()) {

                return Base::Status::Failure(

                    Base::ErrorCode::ValidationFailed,

                    "x:Static type has multiple namespace separators");

            }

            prefixSeparator = index;

        }

    }

    Base::StringView prefix;

    Base::StringView typeName = qualifiedType;

    if (prefixSeparator != qualifiedType.SizeBytes()) {

        if (prefixSeparator == 0U ||

            prefixSeparator + 1U >= qualifiedType.SizeBytes()) {

            return Base::Status::Failure(

                Base::ErrorCode::ValidationFailed,

                "x:Static type name is invalid");

        }

        prefix = qualifiedType.Substr(0U, prefixSeparator);

        typeName = qualifiedType.Substr(

            prefixSeparator + 1U,

            qualifiedType.SizeBytes() - prefixSeparator - 1U);

    }



    Base::Result<Base::StringView> xamlNamespace =

        services.namespaces.Lookup(prefix);

    if (!xamlNamespace) return xamlNamespace.GetStatus();

    Base::Result<const Meta::TypeInfo*> type =

        services.schema->ResolveType(

            xamlNamespace.Value(),

            typeName);

    if (!type) return type.GetStatus();

    if (type.Value()->Kind() == Meta::MetadataTypeKind::Enum) {

        const Meta::EnumValueInfo* value =

            services.schema->Types().FindEnumValue(

                type.Value()->Id(), memberName);

        if (value == nullptr) {

            return Base::Status::Failure(

                Base::ErrorCode::NotFound,

                "x:Static enum member was not found");

        }



        const bool signedEnum =

            (static_cast<std::uint32_t>(type.Value()->Flags()) &

             static_cast<std::uint32_t>(Meta::TypeFlags::SignedEnum)) != 0U;

        Meta::Value result = signedEnum

            ? Meta::Value::FromSignedInteger(

                  type.Value()->Id(),

                  static_cast<std::int64_t>(value->RawValue()))

            : Meta::Value::FromUnsignedInteger(

                  type.Value()->Id(),

                  value->RawValue());

        return ProvidedValue::FromValue(std::move(result));

    }



    if (services.targetValueType != Meta::InvalidTypeId &&

        services.schema->Types().IsAssignableFrom(

            services.targetValueType,

            Input::RoutedCommand::StaticTypeId())) {

        Base::Result<Base::Ref<Input::RoutedCommand>> command =

            Input::RoutedCommand::ResolveStatic(

                type.Value()->Id(), memberName);

        if (!command) return command.GetStatus();

        Meta::Value result = Meta::Value::FromObject(

            services.targetValueType,

            Base::Ref<Base::Object>(command.Value()));

        return ProvidedValue::FromValue(std::move(result));

    }



    return Base::Status::Failure(

        Base::ErrorCode::Unsupported,

        "x:Static member is not a registered enum or routed command");

}



} // namespace Aero::Markup



