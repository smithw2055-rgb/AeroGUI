#pragma once

#include <Aero/Data/BindingBase.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/DependencyObject.hpp>
#include <cstdint>

namespace Aero::Data {

class AERO_GUI_API PropertyPath {
public:
    PropertyPath() noexcept = default;
    explicit PropertyPath(StringView path) noexcept { static_cast<void>(path_.Assign(path)); }

    StringView GetPath() const noexcept { return path_.View(); }
    bool GetIsEmpty() const noexcept { return path_.Empty(); }
    void SetPath(StringView value) noexcept { (void)path_.Assign(value); }

private:
    String path_;
};

enum class RelativeSourceMode : std::uint8_t {
    PreviousData = 0U,
    TemplatedParent,
    Self,
    FindAncestor
};

class AERO_GUI_API RelativeSource : public Base::Object {
    AERO_DECLARE_TYPE(RelativeSource, Base::Object)
public:
    RelativeSource() noexcept = default;
    explicit RelativeSource(RelativeSourceMode mode) noexcept : mode_(mode) {}

    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    RelativeSourceMode GetMode() const noexcept { return mode_; }
    void SetMode(RelativeSourceMode value) noexcept { mode_ = value; }
    StringView GetAncestorType() const noexcept { return ancestorType_.View(); }
    void SetAncestorType(StringView value) noexcept { (void)ancestorType_.Assign(value); }
    std::uint32_t GetAncestorLevel() const noexcept { return ancestorLevel_; }
    void SetAncestorLevel(std::uint32_t value) noexcept { ancestorLevel_ = value == 0U ? 1U : value; }

    static Ref<RelativeSource> ForSelf() noexcept;
    static Ref<RelativeSource> ForTemplatedParent() noexcept;

private:
    RelativeSourceMode mode_ = RelativeSourceMode::Self;
    String ancestorType_;
    std::uint32_t ancestorLevel_ = 1U;
};

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
