#pragma once

// Canonical source-only GUI contract.  Individual files under private/ hold
// the implementation seams, while consumers use this single domain entry.
#include "private/Metadata.hpp"
#include "private/Property.hpp"
#include "private/Element.hpp"
#include "private/RoutedEvent.hpp"
#include "private/Input.hpp"
#include "private/Layout.hpp"
#include "private/Binding.hpp"
#include "private/Animation.hpp"
#include "private/Style.hpp"
