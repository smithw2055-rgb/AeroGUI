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

class DependencyObject;
class UIElement;
class FrameworkElement;
class ContentElement;
class Freezable;
namespace Media { class Visual; }
namespace Controls { class Control; class Panel; }
namespace Shapes { class Shape; }

template<class T> struct CoreTypeBit { static constexpr std::uint16_t kValue = 0U; };
template<> struct CoreTypeBit<DependencyObject> { static constexpr std::uint16_t kValue = Base::kCoreTypeDependencyObject; };
template<> struct CoreTypeBit<Media::Visual> { static constexpr std::uint16_t kValue = Base::kCoreTypeVisual; };
template<> struct CoreTypeBit<UIElement> { static constexpr std::uint16_t kValue = Base::kCoreTypeUIElement; };
template<> struct CoreTypeBit<FrameworkElement> { static constexpr std::uint16_t kValue = Base::kCoreTypeFrameworkElement; };
template<> struct CoreTypeBit<Controls::Control> { static constexpr std::uint16_t kValue = Base::kCoreTypeControl; };
template<> struct CoreTypeBit<Controls::Panel> { static constexpr std::uint16_t kValue = Base::kCoreTypePanel; };
template<> struct CoreTypeBit<Shapes::Shape> { static constexpr std::uint16_t kValue = Base::kCoreTypeShape; };
template<> struct CoreTypeBit<ContentElement> { static constexpr std::uint16_t kValue = Base::kCoreTypeContentElement; };
template<> struct CoreTypeBit<Freezable> { static constexpr std::uint16_t kValue = Base::kCoreTypeFreezable; };

AERO_GUI_API bool IsRuntimeTypeDerivedFrom(Base::MetaTypeId runtimeType, Base::MetaTypeId baseType) noexcept;
AERO_GUI_API bool IsRuntimeTypeDerivedFrom(const Object* object, Base::MetaTypeId baseType) noexcept;

template<class T> T* TryCast(Object* object) noexcept {
    static_assert(std::is_base_of<Object, T>::value, "Aero::TryCast requires an Aero object type");
    if (object == nullptr) { return nullptr; }
    using CleanT = std::remove_cv_t<T>;
    if constexpr (CoreTypeBit<CleanT>::kValue != 0U) {
        return (object->CoreTypeMask() & CoreTypeBit<CleanT>::kValue) != 0U
            ? static_cast<T*>(object)
            : nullptr;
    } else {
        const Base::MetaTypeId runtimeType = object->RuntimeType();
        if (runtimeType == T::StaticTypeId()) { return static_cast<T*>(object); }
        if (!IsRuntimeTypeDerivedFrom(runtimeType, T::StaticTypeId())) { return nullptr; }
        return static_cast<T*>(object);
    }
}

template<class T> const T* TryCast(const Object* object) noexcept { return TryCast<T>(const_cast<Object*>(object)); }

AERO_GUI_API void* TryCastToInterface(Object* object, Base::MetaTypeId interfaceType) noexcept;
AERO_GUI_API const void* TryCastToInterface(const Object* object, Base::MetaTypeId interfaceType) noexcept;

template<class TInterface> TInterface* TryCastToInterface(Object* object) noexcept {
    if (object == nullptr) { return nullptr; }
    return static_cast<TInterface*>(TryCastToInterface(object, TInterface::StaticTypeId()));
}

template<class TInterface> const TInterface* TryCastToInterface(const Object* object) noexcept {
    return TryCastToInterface<TInterface>(const_cast<Object*>(object));
}

} // namespace Aero
