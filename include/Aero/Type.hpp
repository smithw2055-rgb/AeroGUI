#pragma once

#include <Aero/Metadata.hpp>

// Public OOP declaration helpers. These keep the existing stable type-id
// implementation while exposing the vocabulary expected by WPF/NoesisGUI
// developers: Self, BaseType, Property and RoutedEvent.
#define AERO_DECLARE_TYPE_NAMED( \
    typeName, baseTypeName, metadataNamespace, metadataName) \
    AERO_TYPED_META_NAMED( \
        typeName, baseTypeName, metadataNamespace, metadataName) \
public: \
    using Self = typeName; \
    using BaseType = baseTypeName; \
    struct Members final { \
        template<class TValue> \
        using Property = \
            Aero::Core::DependencyPropertyRef<Self, TValue>; \
        template<class TValue> \
        using ReadOnlyProperty = \
            Aero::Core::ReadOnlyDependencyPropertyRef<Self, TValue>; \
        template<class TValue> \
        using AttachedProperty = \
            Aero::Core::DependencyPropertyRef<Self, TValue>; \
        template<class TEventArgs> \
        using RoutedEvent = \
            Aero::Core::RoutedEventRef<Self, TEventArgs>; \
    };

#define AERO_DECLARE_TYPE(typeName, baseTypeName) \
    AERO_DECLARE_TYPE_NAMED( \
        typeName, baseTypeName, \
        Aero::Core::AeroNamespaceUri(), #typeName)
