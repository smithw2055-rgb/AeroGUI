#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Value.hpp>
#include <Aero/DependencyObject.hpp>

#include <cstdint>
#include <utility>

namespace Aero::Data {

enum class BindingMode : std::uint8_t {
    OneTime = 0U,
    OneWay,
    TwoWay,
    OneWayToSource,
    Default
};

enum class RelativeSourceMode : std::uint8_t {
    PreviousData = 0U,
    TemplatedParent,
    Self,
    FindAncestor
};

class AERO_API PropertyPath {
public:
    PropertyPath() noexcept = default;
    explicit PropertyPath(Base::StringView path) noexcept { static_cast<void>(path_.Assign(path)); }

    Base::StringView GetPath() const noexcept { return path_.View(); }
    bool GetIsEmpty() const noexcept { return path_.Empty(); }
    void SetPath(Base::StringView value) noexcept { (void)path_.Assign(value); }

private:
    Base::String path_;
};

class AERO_API RelativeSource : public Base::Object {
    AERO_DECLARE_TYPE(RelativeSource, Base::Object)
public:
    RelativeSource() noexcept = default;
    explicit RelativeSource(RelativeSourceMode mode) noexcept : mode_(mode) {}

    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    RelativeSourceMode GetMode() const noexcept { return mode_; }
    void SetMode(RelativeSourceMode value) noexcept { mode_ = value; }
    Base::StringView GetAncestorType() const noexcept { return ancestorType_.View(); }
    void SetAncestorType(Base::StringView value) noexcept { (void)ancestorType_.Assign(value); }
    std::uint32_t GetAncestorLevel() const noexcept { return ancestorLevel_; }
    void SetAncestorLevel(std::uint32_t value) noexcept { ancestorLevel_ = value == 0U ? 1U : value; }

    static Base::Ref<RelativeSource> ForSelf() noexcept;
    static Base::Ref<RelativeSource> ForTemplatedParent() noexcept;

private:
    RelativeSourceMode mode_ = RelativeSourceMode::Self;
    Base::String ancestorType_;
    std::uint32_t ancestorLevel_ = 1U;
};

class AERO_API IValueConverter : public Base::Object {
    AERO_DECLARE_TYPE(IValueConverter, Base::Object)
public:
    ~IValueConverter() override = default;

    virtual Base::Result<Value> Convert(const Value& value, const Value& parameter) noexcept = 0;
    virtual Base::Result<Value> ConvertBack(const Value& value, const Value& parameter) noexcept = 0;

protected:
    IValueConverter() noexcept = default;
};

class AERO_API BindingBase : public Base::Object {
    AERO_DECLARE_TYPE(BindingBase, Base::Object)
public:
    ~BindingBase() override = default;

    Meta::TypeId RuntimeType() const noexcept override { return runtimeType_; }
    Base::StringView GetStringFormat() const noexcept { return stringFormat_.View(); }
    void SetStringFormat(Base::StringView value) noexcept { (void)stringFormat_.Assign(value); }
    const Value& GetFallbackValue() const noexcept { return fallbackValue_; }
    void SetFallbackValue(Value value) noexcept { fallbackValue_ = std::move(value); }
    const Value& GetTargetNullValue() const noexcept { return targetNullValue_; }
    void SetTargetNullValue(Value value) noexcept { targetNullValue_ = std::move(value); }

protected:
    explicit BindingBase(Meta::TypeId runtimeType) noexcept : runtimeType_(runtimeType) {}

private:
    Meta::TypeId runtimeType_ = StaticTypeId();
    Base::String stringFormat_;
    Value fallbackValue_;
    Value targetNullValue_;
};

class AERO_API Binding : public BindingBase {
    AERO_DECLARE_TYPE(Binding, BindingBase)
public:
    Binding() noexcept : BindingBase(StaticTypeId()) {}
    explicit Binding(Base::StringView path) noexcept : BindingBase(StaticTypeId()), path_(path) {}

    const PropertyPath& GetPath() const noexcept { return path_; }
    void SetPath(PropertyPath value) noexcept { path_ = std::move(value); return; }
    void SetPath(Base::StringView value) noexcept { path_.SetPath(value); }
    Base::StringView GetElementName() const noexcept { return elementName_.View(); }
    void SetElementName(Base::StringView value) noexcept { (void)elementName_.Assign(value); }
    BindingMode GetMode() const noexcept { return mode_; }
    void SetMode(BindingMode value) noexcept { mode_ = value; }
    UpdateSourceTrigger GetUpdateSourceTrigger() const noexcept { return updateSourceTrigger_; }
    void SetUpdateSourceTrigger(UpdateSourceTrigger value) noexcept { updateSourceTrigger_ = value; }
    Base::Ref<Base::Object> GetSource() const noexcept { return source_; }
    void SetSource(Base::Ref<Base::Object> value) noexcept { source_ = std::move(value); }
    Base::Ref<RelativeSource> GetRelativeSource() const noexcept { return relativeSource_; }
    void SetRelativeSource(Base::Ref<RelativeSource> value) noexcept { relativeSource_ = std::move(value); }
    Base::Ref<IValueConverter> GetConverter() const noexcept { return converter_; }
    void SetConverter(Base::Ref<IValueConverter> value) noexcept { converter_ = std::move(value); }
    const Value& GetConverterParameter() const noexcept { return converterParameter_; }
    void SetConverterParameter(Value value) noexcept { converterParameter_ = std::move(value); }

private:
    PropertyPath path_;
    Base::String elementName_;
    BindingMode mode_ = BindingMode::Default;
    UpdateSourceTrigger updateSourceTrigger_ = UpdateSourceTrigger::Default;
    Base::Ref<Base::Object> source_;
    Base::Ref<RelativeSource> relativeSource_;
    Base::Ref<IValueConverter> converter_;
    Value converterParameter_;
};

} // namespace Aero::Data
