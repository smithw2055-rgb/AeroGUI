#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/DependencyProperty.hpp>

namespace Aero::Core {

AERO_NODISCARD AERO_API Base::StringView
AeroPresentationNamespaceUri() noexcept;

struct CorePresentationMetadata final {
    TypeId objectType = InvalidTypeId;
    TypeId dependencyObjectType = InvalidTypeId;
    TypeId treeNodeType = InvalidTypeId;
    TypeId layoutElementType = InvalidTypeId;
    TypeId renderElementType = InvalidTypeId;
    TypeId stackPanelType = InvalidTypeId;
    TypeId canvasType = InvalidTypeId;
    TypeId gridType = InvalidTypeId;
    TypeId borderType = InvalidTypeId;
    TypeId textBlockType = InvalidTypeId;
    TypeId contentPresenterType = InvalidTypeId;
    TypeId booleanType = InvalidTypeId;
    TypeId unsignedIntegerType = InvalidTypeId;
    TypeId doubleType = InvalidTypeId;
    TypeId stringType = InvalidTypeId;
    TypeId lengthType = InvalidTypeId;
    TypeId thicknessType = InvalidTypeId;
    TypeId colorType = InvalidTypeId;
    TypeId horizontalAlignmentType = InvalidTypeId;
    TypeId verticalAlignmentType = InvalidTypeId;
    TypeId orientationType = InvalidTypeId;
};

// Registers built-in type/value/property metadata without freezing either
// registry. Applications register custom controls and properties afterwards.
AERO_NODISCARD AERO_API Base::Result<CorePresentationMetadata>
TryRegisterCorePresentationMetadata(
    TypeRegistry& types,
    DependencyPropertyRegistry& properties) noexcept;

} // namespace Aero::Core
