#pragma once

#include "runtime/RuntimeFwd.hpp"

namespace Aero {
class ObjectTree;
namespace Core {
class EffectiveValueEngine;
class MetadataRuntime;
class ObjectServicesScope;
}
namespace Render { class RenderTree; }
}

namespace Aero::Detail {

class ImageRuntime;
class TextRuntime;

// View-owned WPF presentation services. This is a private state aggregate, not
// an extension interface: it centralizes service ownership without adding a
// second public runtime model.
struct PresentationRuntime {
    Core::MetadataRuntime* metadataRuntime = nullptr;
    Core::ObjectServicesScope* objectServices = nullptr;
    Core::EffectiveValueEngine* values = nullptr;
    AnimationManager* animations = nullptr;
    Aero::ObjectTree* tree = nullptr;
    LayoutManager* layout = nullptr;
    Render::RenderTree* renderer = nullptr;
    ImageRuntime* imageRuntime = nullptr;
    TextRuntime* textRuntime = nullptr;
    BindingManager* bindings = nullptr;
    EventRouter* events = nullptr;
    InputService* input = nullptr;

};

} // namespace Aero::Detail
