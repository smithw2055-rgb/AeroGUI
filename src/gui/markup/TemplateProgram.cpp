#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/controls/State.hpp" 
#include "gui/templates/TemplateState.hpp"
#include "gui/markup/MarkupState.hpp"
#include "gui/markup/MarkupWriterState.hpp"
#include <cstdio>
// Consolidated implementation. Keep sections ordered by dependency.


#include "gui/markup/StyleSupport.inl"
#include "gui/markup/TemplateSupport.inl"
#include "gui/markup/TemplateCompiler.inl"
