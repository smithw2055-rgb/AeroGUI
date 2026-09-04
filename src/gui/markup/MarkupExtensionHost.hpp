#pragma once

// Shared include set formerly provided transitively when all markup-extension
// .inl files were amalgamated into XamlObjectWriter.cpp after BindingExtension.inl.

#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/controls/State.hpp"
#include "gui/templates/TemplateState.hpp"
#include "gui/markup/MarkupState.hpp"
#include "gui/markup/MarkupWriterState.hpp"
#include "gui/markup/MarkupCommon.hpp"

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
#include <Aero/Value.hpp>



namespace Aero::Markup {

// Shared helpers defined in BindingExtension.cpp (formerly private to the
// amalgamated Writer TU). Visible to ObjectBuilder property-apply and other
// extension TUs.
Base::Result<long double> ReadConstantBindingNumber(
    const Meta::Value& value) noexcept;
Base::Result<Meta::Value> ConvertConstantBindingValue(
    const Meta::Value& value,
    Meta::TypeId targetType) noexcept;
Base::Result<ProvidedValue> CreateMultiBindingValue(
    Data::MultiBinding& binding,
    const ExtensionServices& services) noexcept;
Base::Result<void> CaptureControlTemplateChildName(
    Controls::ControlTemplate& controlTemplate,
    const Aero::NameScope* nameScope,
    Base::Object& target,
    Base::String& storage) noexcept;

} // namespace Aero::Markup
