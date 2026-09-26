#include <Aero/DataObject.hpp>

#include <Aero/Meta.hpp>

#include <cstdint>
#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryDetail.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Resources.hpp>
#include <Aero/EventTrigger.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/MediaActions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/ApplicationCommands.hpp>
#include <Aero/InputGesture.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/Animatable.hpp>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace Aero {

namespace {

constexpr char FileDropListSeparator = '\n';

Base::Result<Base::String> JoinFileDropList(const Base::Vector<Base::String>& files) noexcept {
    Base::String joined;
    for (std::uint32_t index = 0; index < files.Size(); ++index) {
        if (index != 0) {
            const Base::Result<void> separator =
                joined.Append(Base::StringView(&FileDropListSeparator, 1U));
            if (!separator) {
                return separator.GetStatus();
            }
        }
        const Base::StringView view = files[index].View();
        const Base::Result<void> appended = joined.Append(view);
        if (!appended) {
            return appended.GetStatus();
        }
    }
    return joined;
}

Base::Vector<Base::String> SplitFileDropList(Base::StringView value) noexcept {
    Base::Vector<Base::String> files;
    const char* data = value.Data();
    const std::uint32_t size = value.SizeBytes();
    std::uint32_t start = 0U;
    while (start <= size) {
        std::uint32_t index = start;
        while (index < size && data[index] != FileDropListSeparator) {
            ++index;
        }
        const std::uint32_t length = index - start;
        if (length > 0U) {
            Base::String entry;
            const Base::Result<void> assigned =
                entry.Assign(Base::StringView(data + start, length));
            if (assigned) {
                files.EmplaceBack(std::move(entry));
            }
        }
        if (index >= size) {
            break;
        }
        start = index + 1U;
    }
    return files;
}

} // namespace

void DataObject::SetData(Base::StringView format, const Meta::Value& data) noexcept {
    for (std::uint32_t index = 0; index < entries_.Size(); ++index) {
        if (entries_[index].format == format) {
            entries_[index].value = data;
            return;
        }
    }
    Base::String key;
    const Base::Result<void> assigned = key.Assign(format);
    if (!assigned) {
        return;
    }
    Entry entry;
    entry.format = std::move(key);
    entry.value = data;
    entries_.EmplaceBack(std::move(entry));
}

void DataObject::SetData(Base::StringView format, const Base::Ref<Base::Object>& object) noexcept {
    const Meta::Value payload = !object
        ? Meta::Value{}
        : Meta::Value::FromObject(Meta::TypeOf<Base::Object>(), object);
    SetData(format, payload);
}

bool DataObject::ContainsData(Base::StringView format) const noexcept {
    for (std::uint32_t index = 0; index < entries_.Size(); ++index) {
        if (entries_[index].format == format) {
            return true;
        }
    }
    return false;
}

Meta::Value DataObject::GetData(Base::StringView format) const noexcept {
    for (std::uint32_t index = 0; index < entries_.Size(); ++index) {
        if (entries_[index].format == format) {
            return entries_[index].value;
        }
    }
    return Meta::Value{};
}

Base::Vector<Base::String> DataObject::GetFormats() const noexcept {
    Base::Vector<Base::String> formats;
    for (std::uint32_t index = 0; index < entries_.Size(); ++index) {
        Base::String copy;
        const Base::Result<void> assigned = copy.Assign(entries_[index].format.View());
        if (assigned) {
            formats.EmplaceBack(std::move(copy));
        }
    }
    return formats;
}

bool DataObject::ContainsFileDropList() const noexcept {
    return ContainsData(DataFormats::FileDropList());
}

Base::Vector<Base::String> DataObject::GetFileDropList() const noexcept {
    const Meta::Value value = GetData(DataFormats::FileDropList());
    if (value.IsUnset()) {
        return Base::Vector<Base::String>{};
    }
    return SplitFileDropList(value.AsString());
}

void DataObject::SetFileDropList(const Base::Vector<Base::String>& files) noexcept {
    const Base::Result<Base::String> joined = JoinFileDropList(files);
    if (!joined) {
        return;
    }
    const Base::Result<Meta::Value> encoded = Meta::ValueCodec<Base::String>::Encode(joined.Value());
    if (!encoded) {
        return;
    }
    SetData(DataFormats::FileDropList(), encoded.Value());
}

} // namespace Aero

// Metadata registration for the types implemented in this file.
AERO_DESCRIBE(::Aero::DataObject) {
    using namespace ::Aero;
    using namespace ::Aero::Meta;
    using namespace ::Aero::Input;
    using namespace Aero::Meta;
    using namespace Aero::Input;
    Register<DataObject>(context);
}
