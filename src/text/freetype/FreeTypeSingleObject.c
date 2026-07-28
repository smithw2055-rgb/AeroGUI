/***************************************************************************
 *
 * FreeTypeSingleObject.c
 *
 * Single-object FreeType compilation unit for AeroGUI's built-in text
 * runtime. The included files are part of an official FreeType source tree
 * supplied through AERO_THIRD_PARTY_ROOT.
 *
 ***************************************************************************/

#define FT2_BUILD_LIBRARY 1
#define FT_CONFIG_MODULES_H <FreeTypeModules.h>

#include "src/base/ftsystem.c"
#include "src/base/ftbase.c"
#include "src/base/ftinit.c"
#include "src/base/ftdebug.c"
#include "src/base/ftbitmap.c"
#include "src/base/ftglyph.c"
#include "src/base/ftsynth.c"
#include "src/base/ftmm.c"

#include "src/sfnt/sfnt.c"
#include "src/truetype/truetype.c"
#include "src/cff/cff.c"
#include "src/psaux/psaux.c"
#include "src/psnames/psnames.c"
#include "src/pshinter/pshinter.c"

#include "src/smooth/smooth.c"
#include "src/raster/raster.c"
#include "src/autofit/autofit.c"
#include "src/gzip/ftgzip.c"
