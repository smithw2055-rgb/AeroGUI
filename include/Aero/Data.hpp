#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Meta/TypeRegistry.hpp>
#include <Aero/Meta/Value.hpp>
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

class AERO_API PropertyPath final {
public:
    PropertyPath() noexcept = default;
    explicit PropertyPath(Base::StringView path) noexcept { static_cast<void>(path_.TryAssign(path)); }

    Base::StringView GetPath() const noexcept { return path_.View(); }
    bool IsEmpty() const noexcept { return path_.Empty(); }
    Base::Result<void> SetPath(Base::StringView value) noexcept { return path_.TryAssign(value); }

private:
    Base::String path_;
};

class AERO_API RelativeSource final : public Base::Object {
    AERO_DECLARE_TYPE(RelativeSource, Base::Object)
public:
    RelativeSource() noexcept = default;
    explicit RelativeSource(RelativeSourceMode mode) noexcept : mode_(mode) {}

    Core::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    RelativeSourceMode GetMode() const noexcept { return mode_; }
    void SetMode(RelativeSourceMode value) noexcept { mode_ = value; }
    Base::StringView GetAncestorType() const noexcept { return ancestorType_.View(); }
    Base::Result<void> SetAncestorType(Base::StringView value) noexcept { return ancestorType_.TryAssign(value); }
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

    virtual Base::Result<Core::Value> Convert(const Core::Value& value, const Core::Value& parameter) noexcept = 0;
    virtual Base::Result<Core::Value> ConvertBack(const Core::Value& value, const Core::Value& parameter) noexcept = 0;

protected:
    IValueConverter() noexcept = default;
};

class AERO_API BindingBase : public Base::Object {
    AERO_DECLARE_TYPE(BindingBase, Base::Object)
public:
    ~BindingBase() override = default;

    Core::TypeId RuntimeType() const noexcept override { return runtimeType_; }
    Base::StringView GetStringFormat() const noexcept { return stringFormat_.View(); }
    Base::Result<void> SetStringFormat(Base::StringView value) noexcept { return stringFormat_.TryAssign(value); }
    const Core::Value& GetFallbackValue() const noexcept { return fallbackValue_; }
    void SetFallbackValue(Core::Value value) noexcept { fallbackValue_ = std::move(value); }
    const Core::Value& GetTargetNullValue() const noexcept { return targetNullValue_; }
    void SetTargetNullValue(Core::Value value) noexcept { targetNullValue_ = std::move(value); }

protected:
    explicit BindingBase(Core::TypeId runtimeType) noexcept : runtimeType_(runtimeType) {}

private:
    Core::TypeId runtimeType_ = StaticTypeId();
    Base::String stringFormat_;
    Core::Value fallbackValue_;
    Core::Value targetNullValue_;
};

class AERO_API Binding final : public BindingBase {
    AERO_DECLARE_TYPE(Binding, BindingBase)
public:
    Binding() noexcept : BindingBase(StaticTypeId()) {}
    explicit Binding(Base::StringView path) noexcept : BindingBase(StaticTypeId()), path_(path) {}

    const PropertyPath& GetPath() const noexcept { return path_; }
    Base::Result<void> SetPath(PropertyPath value) noexcept { path_ = std::move(value); return {}; }
    Base::Result<void> SetPath(Base::StringView value) noexcept { return path_.SetPath(value); }
    Base::StringView GetElementName() const noexcept { return elementName_.View(); }
    Base::Result<void> SetElementName(Base::StringView value) noexcept { return elementName_.TryAssign(value); }
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
    const Core::Value& GetConverterParameter() const noexcept { return converterParameter_; }
    void SetConverterParameter(Core::Value value) noexcept { converterParameter_ = std::move(value); }

private:
    PropertyPath path_;
    Base::String elementName_;
    BindingMode mode_ = BindingMode::Default;
    UpdateSourceTrigger updateSourceTrigger_ = UpdateSourceTrigger::Default;
    Base::Ref<Base::Object> source_;
    Base::Ref<RelativeSource> relativeSource_;
    Base::Ref<IValueConverter> converter_;
    Core::Value converterParameter_;
};

} // namespace Aero::Data
