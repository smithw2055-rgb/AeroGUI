#include <Aero/Data/NotifyPropertyChanged.hpp>

#include "gui/core/State.hpp"
#include "gui/meta/MetadataState.hpp"

namespace Aero::Data {

Meta::MemberId FindNotifyPropertyMember(
    Base::Object& object,
    Base::StringView propertyName) noexcept {
    if (propertyName.Empty()) {
        return Meta::InvalidMemberId;
    }
    const Meta::TypeRegistry* types = nullptr;
    const Meta::ObjectFactoryState factory = Meta::CurrentObjectFactory();
    if (factory.dependencyProperties != nullptr) {
        types = &factory.dependencyProperties->Types();
    }
    if (types == nullptr) {
        return Meta::InvalidMemberId;
    }
    const Meta::PropertyInfo* property = types->FindProperty(
        object.RuntimeType(), propertyName, true);
    return property != nullptr ? property->Id() : Meta::InvalidMemberId;
}

} // namespace Aero::Data
