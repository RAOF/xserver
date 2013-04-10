/*
 * Copyright © 2012 Canonical, Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Soft-
 * ware"), to deal in the Software without restriction, including without
 * limitation the rights to use, copy, modify, merge, publish, distribute,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, provided that the above copyright
 * notice(s) and this permission notice appear in all copies of the Soft-
 * ware and that both the above copyright notice(s) and this permission
 * notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
 * ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY
 * RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN
 * THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSE-
 * QUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFOR-
 * MANCE OF THIS SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder shall
 * not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization of
 * the copyright holder.
 *
 * Authors:
 *   Christopher James Halse Rogers (christopher.halse.rogers@canonical.com)
 */

#include <stdlib.h>

#include "xmir-private.h"
#include "xf86Crtc.h"

static void
crtc_dpms(xf86CrtcPtr drmmode_crtc, int mode)
{
}

static Bool
crtc_set_mode_major(xf86CrtcPtr crtc, DisplayModePtr mode,
		    Rotation rotation, int x, int y)  
{
	return TRUE;
}

static void
crtc_set_cursor_colors (xf86CrtcPtr crtc, int bg, int fg)
{
}

static void
crtc_set_cursor_position (xf86CrtcPtr crtc, int x, int y)
{
}

static void
crtc_show_cursor (xf86CrtcPtr crtc)
{
}

static void
crtc_hide_cursor (xf86CrtcPtr crtc)
{
}

static void
crtc_load_cursor_argb (xf86CrtcPtr crtc, CARD32 *image)
{
}

static PixmapPtr
crtc_shadow_create(xf86CrtcPtr crtc, void *data, int width, int height)
{
	return NULL;
}

static void *
crtc_shadow_allocate(xf86CrtcPtr crtc, int width, int height)
{
	return NULL;
}

static void
crtc_shadow_destroy(xf86CrtcPtr crtc, PixmapPtr rotate_pixmap, void *data)
{
}

static const xf86CrtcFuncsRec crtc_funcs = {
    .dpms                = crtc_dpms,
    .set_mode_major      = crtc_set_mode_major,
    .set_cursor_colors   = crtc_set_cursor_colors,
    .set_cursor_position = crtc_set_cursor_position,
    .show_cursor         = crtc_show_cursor,
    .hide_cursor         = crtc_hide_cursor,
    .load_cursor_argb    = crtc_load_cursor_argb,
    .shadow_create       = crtc_shadow_create,
    .shadow_allocate     = crtc_shadow_allocate,
    .shadow_destroy      = crtc_shadow_destroy,
    .destroy             = NULL, /* XXX */
};

static void
output_dpms(xf86OutputPtr output, int mode)
{
	return;
}

static xf86OutputStatus
output_detect(xf86OutputPtr output)
{
	return XF86OutputStatusConnected;
}

static Bool
output_mode_valid(xf86OutputPtr output, DisplayModePtr pModes)
{
	return MODE_OK;
}

struct mir_output {
    int width;
    int height;
    xf86Monitor monitor;
};

static DisplayModePtr
output_get_modes(xf86OutputPtr xf86output)
{
    struct mir_output *output = xf86output->driver_private;
    struct monitor_ranges *ranges;
    DisplayModePtr modes;

    modes = xf86CVTMode(output->width, output->height, 60, TRUE, FALSE);
    /* And now, because CVT the CVT standard doesn't support such common resolutions as 1366x768... */
    /* TODO: We should really get Mir to just send us an EDID or at least enough info to generate one */
    modes->VDisplay = output->height;
    modes->HDisplay = output->width;

    output->monitor.det_mon[0].type = DS_RANGES;
    ranges = &output->monitor.det_mon[0].section.ranges;
    ranges->min_h = modes->HSync - 10;
    ranges->max_h = modes->HSync + 10;
    ranges->min_v = modes->VRefresh - 10;
    ranges->max_v = modes->VRefresh + 10;
    ranges->max_clock = modes->Clock + 100;
    output->monitor.det_mon[1].type = DT;
    output->monitor.det_mon[2].type = DT;
    output->monitor.det_mon[3].type = DT;
    output->monitor.no_sections = 0;
    modes->type = M_T_PREFERRED | M_T_DRIVER;

    modes->name = strdup("XMIR mode of death");

    xf86output->MonInfo = &output->monitor;

    return modes;
}

static void
output_destroy(xf86OutputPtr xf86output)
{
    struct mir_output *output = xf86output->driver_private;
    
    free(output);
}

static const xf86OutputFuncsRec output_funcs = {
    .dpms	    = output_dpms,
    .detect	    = output_detect,
    .mode_valid	= output_mode_valid,
    .get_modes	= output_get_modes,
    .destroy	= output_destroy
};

static Bool
resize(ScrnInfoPtr scrn, int width, int height)
{
    if (scrn->virtualX == width && scrn->virtualY == height)
        return TRUE;
    /* We don't handle resize at all, we must match the compositor size */
    return FALSE;
}

static const xf86CrtcConfigFuncsRec config_funcs = {
    resize
};

Bool
xmir_mode_pre_init(ScrnInfoPtr scrn, xmir_screen *xmir)
{
    MirDisplayInfo display_info;
    xf86OutputPtr xf86output;
    xf86CrtcPtr xf86crtc;
    struct mir_output *output;

    mir_connection_get_display_info(xmir->conn, &display_info);
    
    /* Set up CRTC config functions */
    xf86CrtcConfigInit(scrn, &config_funcs);

    /* We don't support resizing whatsoever */
    xf86CrtcSetSizeRange(scrn,
                         320, 320,
                         8192, 8192);

    output = malloc(sizeof *output);
    output->width = display_info.width;
    output->height = display_info.height;
    
    xf86output = xf86OutputCreate(scrn, &output_funcs, "XMIR-1");
    xf86output->possible_crtcs = 1;
    xf86output->possible_clones = 1;
    xf86output->driver_private = output;

    xf86crtc = xf86CrtcCreate(scrn, &crtc_funcs);
    xf86crtc->driver_private = NULL;

    xf86InitialConfiguration(scrn, TRUE);
  
    return TRUE;
}
