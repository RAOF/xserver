#include <stdint.h>
#include "dixstruct.h"
#include "windowstr.h"
#include "scrnintstr.h"

#include <mir_client_library.h>

_X_EXPORT int
xmir_get_drm_fd(xmir_screen *xmir);

_X_EXPORT void
xmir_mode_pre_init(ScrnInfoPtr scrn);

_X_EXPORT Bool
xmir_init(ScrnInfoPtr scrn);
