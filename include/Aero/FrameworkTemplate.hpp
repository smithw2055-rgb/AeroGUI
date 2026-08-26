#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Resources.hpp>

namespace Aero {

struct FrameworkTemplateRuntime;

// WPF-shaped template object. XAML compilation, factory callbacks, bindings,
// triggers, namescopes and the immutable runtime program are implementation
// details owned by the markup and controls runtime.
class AERO_GUI_API FrameworkTemplate : public Base::Object {
    AERO_DECLARE_TYPE(FrameworkTemplate, Base::Object)
public:

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

private:
    friend struct FrameworkTemplateRuntime;
    void* state_ = nullptr;
};

} // namespace Aero
