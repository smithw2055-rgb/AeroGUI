// Consolidated metadata implementation. Keep sections ordered by dependency.

#include <Aero/Value.hpp>
#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Meta.hpp>
#include <Aero/DependencyProperty.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include <utility>

#include "gui/meta/TypeRegistry.inl"
#include "gui/meta/MetadataAuthoring.inl"
#include "gui/meta/BehaviorTable.inl"
#include "gui/meta/MetaTable.inl"
#include "gui/meta/Registry.inl"
