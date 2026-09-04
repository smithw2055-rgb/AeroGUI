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
    return SchemaPrivate::AddMarkupExtension(
        schema, {markupExtensionType, &ProvideValue, nullptr});
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
        SchemaPrivate::ResolvePropertyTarget(
            *services.schema, *services.targetObject);
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
    Base::Result<void> retained =
        LocTargets().PushBack(std::move(subscription));
    if (!retained) return retained.GetStatus();
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
