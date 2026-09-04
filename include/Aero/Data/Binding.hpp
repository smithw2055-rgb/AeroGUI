#pragma once

#include <Aero/Data/BindingBase.hpp>
#include <Aero/Data/PropertyPath.hpp>
#include <Aero/Data/RelativeSource.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/DependencyObject.hpp>

namespace Aero::Data {

enum class BindingMode : std::uint8_t {
    OneTime = 0U,
    OneWay,
    TwoWay,
    OneWayToSource,
    Default
};

class AERO_GUI_API Binding : public BindingBase {
    AERO_DECLARE_TYPE(Binding, BindingBase)
public:
    Binding() noexcept : BindingBase(StaticTypeId()) {}
    explicit Binding(StringView path) noexcept : BindingBase(StaticTypeId()), path_(path) {}

    const PropertyPath& GetPath() const noexcept { return path_; }
    StringView GetPathText() const noexcept { return path_.GetPath(); }
    void SetPath(PropertyPath value) noexcept { path_ = std::move(value); return; }
    void SetPath(StringView value) noexcept { path_.SetPath(value); }
    StringView GetElementName() const noexcept { return elementName_.View(); }
    void SetElementName(StringView value) noexcept { (void)elementName_.Assign(value); }
    BindingMode GetMode() const noexcept { return mode_; }
    void SetMode(BindingMode value) noexcept { mode_ = value; }
    UpdateSourceTrigger GetUpdateSourceTrigger() const noexcept { return updateSourceTrigger_; }
    void SetUpdateSourceTrigger(UpdateSourceTrigger value) noexcept { updateSourceTrigger_ = value; }
    Ref<Base::Object> GetSource() const noexcept { return source_; }
    void SetSource(Ref<Base::Object> value) noexcept { source_ = std::move(value); }
    Ref<RelativeSource> GetRelativeSource() const noexcept { return relativeSource_; }
    void SetRelativeSource(Ref<RelativeSource> value) noexcept { relativeSource_ = std::move(value); }
    Ref<IValueConverter> GetConverter() const noexcept { return converter_; }
    void SetConverter(Ref<IValueConverter> value) noexcept { converter_ = std::move(value); }
    const Value& GetConverterParameter() const noexcept { return converterParameter_; }
    void SetConverterParameter(Value value) noexcept { converterParameter_ = std::move(value); }

private:
    PropertyPath path_;
    String elementName_;
    BindingMode mode_ = BindingMode::Default;
    UpdateSourceTrigger updateSourceTrigger_ = UpdateSourceTrigger::Default;
    Ref<Base::Object> source_;
    Ref<RelativeSource> relativeSource_;
    Ref<IValueConverter> converter_;
    Value converterParameter_;
};
} // namespace Aero::Data
