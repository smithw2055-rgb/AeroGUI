#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Data/NotifyPropertyChanged.hpp>
#include <Aero/RelayCommand.hpp>
#include <Aero/Value.hpp>

namespace Aero {

class AERO_GUI_API ViewModelBase : public Base::Object, public Data::NotifyPropertyChanged<ViewModelBase> {
    AERO_DECLARE_TYPE(ViewModelBase, Base::Object)

public:
    ViewModelBase() noexcept : ViewModelBase(StaticTypeId()) {}
    ~ViewModelBase() override = default;

protected:
    explicit ViewModelBase(Base::MetaTypeId runtimeType) noexcept : runtimeType_(runtimeType) {}

public:
    Base::MetaTypeId RuntimeType() const noexcept override { return runtimeType_; }

private:
    Base::MetaTypeId runtimeType_ = StaticTypeId();
};

using RelayCommand = Input::RelayCommand;

} // namespace Aero

#define AERO_VIEWMODEL(TClass) \
    AERO_DECLARE_TYPE(TClass, ::Aero::ViewModelBase)

#define AERO_OBSERVABLE_PROPERTY(type, name, defaultValue) \
private: \
    type name##_ = (defaultValue); \
public: \
    const type& Get##name() const noexcept { return name##_; } \
    bool Set##name(const type& value) noexcept { \
        return this->SetProperty(name##_, value, #name); \
    }

#define AERO_RELAY_COMMAND(name, ...) \
    ::Aero::Base::Ref<::Aero::Input::RelayCommand> name##Command = \
        ::Aero::Base::MakeRef<::Aero::Input::RelayCommand>(__VA_ARGS__)
