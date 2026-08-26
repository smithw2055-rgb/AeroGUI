#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/MetadataId.hpp>
#include <Aero/Base/Object.hpp>

#include <type_traits>

namespace Aero {

// TypeId-chain downcast (WPF `as` / Noesis DynamicCast). Walks the registered
// metadata parent chain; no RTTI, no exceptions. Everyday control headers may
// include this file: it does not pull Meta.hpp, Animation.hpp, or
// DrawingContext.hpp. The TypeRegistry lives in the library, not this header.

AERO_GUI_API bool IsRuntimeTypeDerivedFrom(
    Base::MetaTypeId runtimeType,
    Base::MetaTypeId baseType) noexcept;
AERO_GUI_API bool IsRuntimeTypeDerivedFrom(
    const Object* object,
    Base::MetaTypeId baseType) noexcept;

template<class T>
T* TryCast(Object* object) noexcept {
    static_assert(
        std::is_base_of<Object, T>::value,
        "Aero::TryCast requires an Aero object type");
    if (!IsRuntimeTypeDerivedFrom(object, T::StaticTypeId())) {
        return nullptr;
    }
    return static_cast<T*>(object);
}

template<class T>
const T* TryCast(const Object* object) noexcept {
    return TryCast<T>(const_cast<Object*>(object));
}

AERO_GUI_API void* TryCastToInterface(
    Object* object,
    Base::MetaTypeId interfaceType) noexcept;
AERO_GUI_API const void* TryCastToInterface(
    const Object* object,
    Base::MetaTypeId interfaceType) noexcept;

template<class TInterface>
TInterface* TryCastToInterface(Object* object) noexcept {
    return static_cast<TInterface*>(
        TryCastToInterface(object, TInterface::StaticTypeId()));
}

template<class TInterface>
const TInterface* TryCastToInterface(const Object* object) noexcept {
    return TryCastToInterface<TInterface>(const_cast<Object*>(object));
}

} // namespace Aero
