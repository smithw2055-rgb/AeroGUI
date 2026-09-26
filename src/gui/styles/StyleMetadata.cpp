// Ordered metadata installer. Describe bodies live next to each type.

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
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/Animatable.hpp>
#include <Aero/TextProperties.hpp>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace Aero {

Base::Result<void> PopulateUiStyling(
    ::Aero::Meta::Registration& context) noexcept {
    using namespace Data;
    ::Aero::Meta::DescribeHook<::Aero::Element>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::TextProperties>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::RichText>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::SetterBase>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Setter>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::EventSetter>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Data::IValueConverter>::Run(context);
    ::Aero::Meta::DescribeHook<::Aero::Data::IMultiValueConverter>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Data::BooleanToVisibilityConverter>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Data::BindingBase>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Data::RelativeSource>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Data::Binding>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Data::MultiBinding>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Data::MultiBindingProxy>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::TriggerBase>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Trigger>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::DataTrigger>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Condition>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::MultiDataTrigger>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::MultiTrigger>::Run(context);

    ::Aero::Meta::DescribeHook<::Aero::Style>::Run(context);
    return {};
}

} // namespace Aero
