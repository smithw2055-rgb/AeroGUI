// Ordered metadata installer. Describe bodies live next to each type.

#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/TextProperties.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/IValueConverter.hpp>

namespace Aero {

Base::Result<void> PopulateUiStyling(
    ::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    DescribeHook<::Aero::Element>::Run(context);
    DescribeHook<::Aero::TextProperties>::Run(context);
    DescribeHook<::Aero::RichText>::Run(context);
    DescribeHook<::Aero::SetterBase>::Run(context);
    DescribeHook<::Aero::Setter>::Run(context);
    DescribeHook<::Aero::EventSetter>::Run(context);
    DescribeHook<::Aero::Data::IValueConverter>::Run(context);
    DescribeHook<::Aero::Data::IMultiValueConverter>::Run(context);
    DescribeHook<::Aero::Data::BooleanToVisibilityConverter>::Run(context);
    DescribeHook<::Aero::Data::BindingBase>::Run(context);
    DescribeHook<::Aero::Data::RelativeSource>::Run(context);
    DescribeHook<::Aero::Data::Binding>::Run(context);
    DescribeHook<::Aero::Data::MultiBinding>::Run(context);
    DescribeHook<::Aero::Data::MultiBindingProxy>::Run(context);
    DescribeHook<::Aero::TriggerBase>::Run(context);
    DescribeHook<::Aero::Trigger>::Run(context);
    DescribeHook<::Aero::DataTrigger>::Run(context);
    DescribeHook<::Aero::Condition>::Run(context);
    DescribeHook<::Aero::MultiDataTrigger>::Run(context);
    DescribeHook<::Aero::MultiTrigger>::Run(context);
    DescribeHook<::Aero::Style>::Run(context);
    return {};
}

} // namespace Aero
