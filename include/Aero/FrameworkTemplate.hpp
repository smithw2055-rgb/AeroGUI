#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Resources.hpp>

namespace Aero {

namespace Meta { class Registration; }
namespace Controls { struct FrameworkTemplateState; }
class DependencyObject;

// WPF-shaped template object. XAML compilation, factory callbacks, bindings,
// triggers, namescopes and the immutable runtime program are implementation
// details owned by the markup and controls runtime.
class AERO_GUI_API FrameworkTemplate : public Base::Object {
    AERO_DECLARE_TYPE(FrameworkTemplate, Base::Object)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    FrameworkTemplate() noexcept;
    ~FrameworkTemplate() noexcept override;

    FrameworkTemplate(const FrameworkTemplate&) = delete;
    FrameworkTemplate& operator=(const FrameworkTemplate&) = delete;

    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Meta::TypeId GetTargetType() const noexcept;
    bool GetIsSealed() const noexcept;
    ResourceDictionary& GetResources() noexcept;
    const ResourceDictionary& GetResources() const noexcept;
    void SetResources(Ref<ResourceDictionary> value) noexcept;
    // WPF FrameworkTemplate.LoadContent extension point. Default returns
    // null; ControlTemplate/DataTemplate override to materialize content
    // without exposing state_/Program to public headers.
    virtual Ref<DependencyObject> LoadContent() const noexcept { return {}; }

private:
    friend struct Controls::FrameworkTemplateState;
    void* state_ = nullptr;
};

} // namespace Aero
