#include <Aero/TryCast.hpp>

#include <Aero/DependencyObject.hpp>
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero {

bool IsRuntimeTypeDerivedFrom(
    Base::MetaTypeId runtimeType,
    Base::MetaTypeId baseType) noexcept {
    if (runtimeType == Base::InvalidMetaTypeId ||
        baseType == Base::InvalidMetaTypeId) {
        return false;
    }
    if (runtimeType == baseType || baseType == Object::StaticTypeId()) {
        return true;
    }
    const Meta::ObjectFactoryState factory = Meta::CurrentObjectFactory();
    if (factory.dependencyProperties == nullptr) {
        return false;
    }
    return factory.dependencyProperties->Types().IsDerivedFrom(
        runtimeType, baseType);
}

bool IsRuntimeTypeDerivedFrom(
    const Object* object,
    Base::MetaTypeId baseType) noexcept {
    if (object == nullptr || baseType == Base::InvalidMetaTypeId) {
        return false;
    }
    const Base::MetaTypeId runtimeType = object->RuntimeType();
    if (runtimeType == Base::InvalidMetaTypeId) {
        return false;
    }
    if (runtimeType == baseType || baseType == Object::StaticTypeId()) {
        return true;
    }
    return IsRuntimeTypeDerivedFrom(runtimeType, baseType);
}

void* TryCastToInterface(
    Object* object,
    Base::MetaTypeId interfaceType) noexcept {
    if (object == nullptr || interfaceType == Base::InvalidMetaTypeId) {
        return nullptr;
    }
    const Meta::ObjectFactoryState factory = Meta::CurrentObjectFactory();
    if (factory.dependencyProperties != nullptr) {
        return factory.dependencyProperties->Types().TryCastToInterface(
            *object, interfaceType);
    }
    return nullptr;
}

const void* TryCastToInterface(
    const Object* object,
    Base::MetaTypeId interfaceType) noexcept {
    return TryCastToInterface(const_cast<Object*>(object), interfaceType);
}

} // namespace Aero
