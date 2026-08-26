#include <Aero/TryCast.hpp>

#include <Aero/DependencyObject.hpp>
#include "gui/core/State.hpp"
#include "gui/meta/MetadataState.hpp"

namespace Aero {

bool IsRuntimeTypeDerivedFrom(
    Base::MetaTypeId runtimeType,
    Base::MetaTypeId baseType) noexcept {
    if (runtimeType == Base::InvalidMetaTypeId ||
        baseType == Base::InvalidMetaTypeId) {
        return false;
    }
    if (runtimeType == baseType) {
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
    if (object == nullptr) {
        return false;
    }
    const Base::MetaTypeId runtimeType = object->RuntimeType();
    if (runtimeType == Base::InvalidMetaTypeId ||
        baseType == Base::InvalidMetaTypeId) {
        return false;
    }
    if (runtimeType == baseType) {
        return true;
    }

    const Meta::TypeRegistry* types = nullptr;
    const Meta::ObjectFactoryState factory = Meta::CurrentObjectFactory();
    if (factory.dependencyProperties != nullptr) {
        types = &factory.dependencyProperties->Types();
    }
    if (types != nullptr &&
        types->IsDerivedFrom(
            runtimeType, DependencyObject::StaticTypeId())) {
        return static_cast<const DependencyObject*>(object)
            ->PropertyRegistry()
            .Types()
            .IsDerivedFrom(runtimeType, baseType);
    }
    if (types != nullptr) {
        return types->IsDerivedFrom(runtimeType, baseType);
    }
    return false;
}

} // namespace Aero
