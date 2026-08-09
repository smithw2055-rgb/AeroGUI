#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
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

class AERO_GUI_API IValueConverter : public Base::Object {
    AERO_DECLARE_TYPE(IValueConverter, Base::Object)
public:
    ~IValueConverter() override = default;

    virtual Result<Value> Convert(const Value& value, const Value& parameter) noexcept = 0;
    virtual Result<Value> ConvertBack(const Value& value, const Value& parameter) noexcept = 0;

protected:
    IValueConverter() noexcept = default;
};

// Composite converter used by MultiBinding. Values preserve their concrete
// metadata types, so application converters can combine numbers, colors,
// strings and object references without an additional boxing layer.
class AERO_GUI_API IMultiValueConverter : public Base::Object {
    AERO_DECLARE_TYPE(IMultiValueConverter, Base::Object)
public:
    ~IMultiValueConverter() override = default;

    virtual Result<Value> Convert(
        Span<const Value> values,
        Meta::TypeId targetType,
        const Value& parameter) noexcept = 0;

protected:
    IMultiValueConverter() noexcept = default;
};


// WPF-compatible BooleanToVisibilityConverter. Keeping this converter in the
// data layer lets resource dictionaries use it without pulling in Controls.
class AERO_GUI_API BooleanToVisibilityConverter final
    : public IValueConverter {
    AERO_DECLARE_TYPE(BooleanToVisibilityConverter, IValueConverter)
public:
    BooleanToVisibilityConverter() noexcept = default;

    Result<Value> Convert(
        const Value& value,
        const Value& parameter) noexcept override;
    Result<Value> ConvertBack(
        const Value& value,
        const Value& parameter) noexcept override;
};

class AERO_GUI_API BindingBase : public Base::Object {
    AERO_DECLARE_TYPE(BindingBase, Base::Object)
public:
    ~BindingBase() override = default;

    Meta::TypeId RuntimeType() const noexcept override { return runtimeType_; }
    StringView GetStringFormat() const noexcept { return stringFormat_.View(); }
    void SetStringFormat(StringView value) noexcept { (void)stringFormat_.Assign(value); }
    const Value& GetFallbackValue() const noexcept { return fallbackValue_; }
    void SetFallbackValue(Value value) noexcept { fallbackValue_ = std::move(value); }
    const Value& GetTargetNullValue() const noexcept { return targetNullValue_; }
    void SetTargetNullValue(Value value) noexcept { targetNullValue_ = std::move(value); }

protected:
    explicit BindingBase(Meta::TypeId runtimeType) noexcept : runtimeType_(runtimeType) {}

private:
    Meta::TypeId runtimeType_ = StaticTypeId();
    String stringFormat_;
    Value fallbackValue_;
    Value targetNullValue_;
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

// WPF-shaped composite binding declaration. The live expression is created by
// the XAML writer from ordinary BindingEngine expressions, keeping source
// resolution and notification behavior in one runtime.
class AERO_GUI_API MultiBinding final : public BindingBase {
    AERO_DECLARE_TYPE(MultiBinding, BindingBase)
public:
    MultiBinding() noexcept
        : BindingBase(StaticTypeId()),
          bindings_(&Base::GetDefaultAllocator()) {}

    Ref<IMultiValueConverter> GetConverter() const noexcept {
        return converter_;
    }
    void SetConverter(Ref<IMultiValueConverter> value) noexcept {
        converter_ = std::move(value);
    }
    const Value& GetConverterParameter() const noexcept {
        return converterParameter_;
    }
    void SetConverterParameter(Value value) noexcept {
        converterParameter_ = std::move(value);
    }
    Span<const Ref<Binding>> GetBindings() const noexcept {
        return bindings_.AsSpan();
    }
    Result<void> AddBinding(Ref<Binding> value) noexcept {
        return value
            ? bindings_.PushBack(std::move(value))
            : Result<void>(Base::Status::Failure(
                  Base::ErrorCode::InvalidArgument,
                  "MultiBinding child Binding is null"));
    }
    void ClearBindings() noexcept { bindings_.Clear(); }

private:
    Base::Vector<Ref<Binding>> bindings_;
    Ref<IMultiValueConverter> converter_;
    Value converterParameter_;
};

} // namespace Aero::Data
