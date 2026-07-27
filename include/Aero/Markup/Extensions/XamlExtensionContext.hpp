#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Core/Metadata/Value.hpp>
#include <Aero/Markup/Resources/XamlNamesResources.hpp>

namespace Aero::Core {
class EffectiveValueEngine;
}

namespace Aero::Presentation {
class BindingManager;
}

namespace Aero::Markup {

class XamlDeferredContentPlan;
class XamlSchemaContext;
struct XamlVisualContentPlan;

using XamlValueKind = Core::ValueKind;
using XamlValue = Core::Value;

// Callback-scoped, data-only context shared by member facets and markup
// extensions. It is assembled by one XamlLoadSession, contains no RTTI service
// lookup, and never survives the callback that receives it.
struct XamlExtensionContext final {
    const XamlSchemaContext* schema = nullptr;
    Base::Object* targetObject = nullptr;
    Core::TypeId targetObjectType = Core::InvalidTypeId;
    Core::MemberId targetMember = Core::InvalidMemberId;
    Core::TypeId targetValueType = Core::InvalidTypeId;
    Base::Object* rootObject = nullptr;
    Base::Object* templatedParent = nullptr;
    const Base::ResourceUri* baseUri = nullptr;
    Core::SourceSpan source;
    const NameScope* nameScope = nullptr;
    XamlNamespaceScope namespaces;
    XamlResourceResolver resources;
    Core::EffectiveValueEngine* effectiveValues = nullptr;
    Presentation::BindingManager* bindings = nullptr;
    ResourceDictionary* fallbackResources = nullptr;
    // Innermost-to-outermost live dictionaries. The span is valid only for
    // the duration of the current member/markup-extension callback.
    Base::Span<const ResourceDictionary* const> ambientResourceChain;
    XamlVisualContentPlan* visualContent = nullptr;
    Base::Object* deferredContentOwner = nullptr;
    XamlDeferredContentPlan* deferredContent = nullptr;
};

// Source-compatible migration alias. New extension and member-facet APIs should
// use XamlExtensionContext directly.
using XamlServiceProvider = XamlExtensionContext;

} // namespace Aero::Markup
