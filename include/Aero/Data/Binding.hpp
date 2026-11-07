#pragma once

#include <Aero/Data/BindingBase.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/PropertyPath.hpp>
#include <cstdint>
#include <new>

namespace Aero::Data {

using PropertyPath = ::Aero::PropertyPath;

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
    static Ref<RelativeSource> ForSelf() noexcept;
    static Ref<RelativeSource> ForTemplatedParent() noexcept;

    RelativeSourceMode GetMode() const noexcept { return mode_; }
    void SetMode(RelativeSourceMode value) noexcept { mode_ = value; }
    StringView GetAncestorType() const noexcept { return ancestorType_.View(); }
    void SetAncestorType(StringView value) noexcept { (void)ancestorType_.Assign(value); }
    std::uint32_t GetAncestorLevel() const noexcept { return ancestorLevel_; }
    void SetAncestorLevel(std::uint32_t value) noexcept { ancestorLevel_ = value == 0U ? 1U : value; }

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
    ~Binding() override { delete options_; }

    const PropertyPath& GetPath() const noexcept { return path_; }
    void SetPath(PropertyPath value) noexcept { path_ = std::move(value); return; }
    void SetPath(StringView value) noexcept { path_.SetPath(value); }
    StringView GetPathText() const noexcept { return path_.GetPath(); }
    StringView GetElementName() const noexcept {
        return options_ != nullptr ? options_->elementName.View() : StringView{};
    }
    void SetElementName(StringView value) noexcept {
        if (value.Empty() && options_ == nullptr) return;
        if (BindingOptions* opt = EnsureOptions()) {
            (void)opt->elementName.Assign(value);
        }
    }
    BindingMode GetMode() const noexcept { return mode_; }
    void SetMode(BindingMode value) noexcept { mode_ = value; }
    UpdateSourceTrigger GetUpdateSourceTrigger() const noexcept { return updateSourceTrigger_; }
    void SetUpdateSourceTrigger(UpdateSourceTrigger value) noexcept { updateSourceTrigger_ = value; }
    Ref<Base::Object> GetSource() const noexcept {
        return options_ != nullptr ? options_->source : Ref<Base::Object>{};
    }
    void SetSource(Ref<Base::Object> value) noexcept {
        if (!value && options_ == nullptr) return;
        if (BindingOptions* opt = EnsureOptions()) {
            opt->source = std::move(value);
        }
    }
    Ref<RelativeSource> GetRelativeSource() const noexcept {
        return options_ != nullptr ? options_->relativeSource : Ref<RelativeSource>{};
    }
    void SetRelativeSource(Ref<RelativeSource> value) noexcept {
        if (!value && options_ == nullptr) return;
        if (BindingOptions* opt = EnsureOptions()) {
            opt->relativeSource = std::move(value);
        }
    }
    Ref<IValueConverter> GetConverter() const noexcept {
        return options_ != nullptr ? options_->converter : Ref<IValueConverter>{};
    }
    void SetConverter(Ref<IValueConverter> value) noexcept {
        if (!value && options_ == nullptr) return;
        if (BindingOptions* opt = EnsureOptions()) {
            opt->converter = std::move(value);
        }
    }
    const Value& GetConverterParameter() const noexcept {
        return rare_ != nullptr ? rare_->converterParameter : EmptyValue();
    }
    void SetConverterParameter(Value value) noexcept {
        if (value.IsUnset() && rare_ == nullptr) return;
        if (RareOptions* r = EnsureRare()) {
            r->converterParameter = std::move(value);
        }
    }

private:
    struct BindingOptions {
        String elementName{&Base::GetDefaultAllocator()};
        Ref<Base::Object> source;
        Ref<RelativeSource> relativeSource;
        Ref<IValueConverter> converter;
    };

    BindingOptions* EnsureOptions() noexcept {
        if (options_ == nullptr) {
            options_ = new (std::nothrow) BindingOptions();
        }
        return options_;
    }

    PropertyPath path_;
    BindingOptions* options_ = nullptr;
    BindingMode mode_ = BindingMode::Default;
    UpdateSourceTrigger updateSourceTrigger_ = UpdateSourceTrigger::Default;
};
} // namespace Aero::Data

AERO_DECLARE_TYPE_ENUM(Aero::Data::RelativeSourceMode)
AERO_DECLARE_TYPE_ENUM(Aero::Data::BindingMode)
AERO_DECLARE_TYPE_ENUM(Aero::Meta::UpdateSourceTrigger)
