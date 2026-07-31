#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/Metadata/TypeRegistry.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>

#include <cstdint>

namespace Aero::Data {

using namespace Aero::Core;

enum class BindingMode : std::uint8_t {
    OneTime = 0U,
    OneWay,
    TwoWay,
    OneWayToSource
};

enum class RelativeSourceMode : std::uint8_t {
    None = 0U,
    Self,
    TemplatedParent,
    Ancestor
};

class AERO_API Binding final
    : public Base::Object {
    AERO_DECLARE_TYPE(Binding, Base::Object)
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::StringView Path() const noexcept {
        return path_.View();
    }
    Base::StringView ElementName() const noexcept {
        return elementName_.View();
    }
    Base::StringView StringFormat() const noexcept {
        return stringFormat_.View();
    }
    RelativeSourceMode RelativeSource() const noexcept {
        return relativeSource_;
    }
    Base::StringView AncestorType() const noexcept {
        return ancestorType_.View();
    }
    BindingMode Mode() const noexcept {
        return mode_;
    }
    UpdateSourceTrigger UpdateTrigger() const noexcept {
        return updateSourceTrigger_;
    }
    Base::Result<void> Configure(
        Base::StringView path,
        Base::StringView elementName,
        BindingMode mode,
        UpdateSourceTrigger updateSourceTrigger,
        Base::StringView stringFormat = {},
        RelativeSourceMode relativeSource =
            RelativeSourceMode::None,
        Base::StringView ancestorType = {}) noexcept {
        Base::Result<void> assigned =
            path_.TryAssign(path);
        if (!assigned) return assigned.GetStatus();
        assigned = elementName_.TryAssign(
            elementName);
        if (!assigned) return assigned.GetStatus();
        assigned = stringFormat_.TryAssign(
            stringFormat);
        if (!assigned) return assigned.GetStatus();
        assigned = ancestorType_.TryAssign(ancestorType);
        if (!assigned) return assigned.GetStatus();
        mode_ = mode;
        relativeSource_ = relativeSource;
        updateSourceTrigger_ =
            updateSourceTrigger;
        return {};
    }

private:
    Base::String path_;
    Base::String elementName_;
    Base::String stringFormat_;
    Base::String ancestorType_;
    BindingMode mode_ = BindingMode::OneWay;
    RelativeSourceMode relativeSource_ =
        RelativeSourceMode::None;
    UpdateSourceTrigger updateSourceTrigger_ =
        UpdateSourceTrigger::PropertyChanged;
};

} // namespace Aero::Data
