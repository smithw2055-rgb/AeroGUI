#include <Aero/Data/NotifyPropertyChanged.hpp>

#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/meta/TypeRegistryDetail.hpp"

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
