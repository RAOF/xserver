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
#include <stdio.h>

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
xmir_output_dpms(xf86OutputPtr output, int mode)
{
    return;
}

static xf86OutputStatus
xmir_output_detect(xf86OutputPtr output)
{
    MirDisplayOutput *mir_output = output->driver_private;
    return mir_output->connected ? XF86OutputStatusConnected : XF86OutputStatusDisconnected;
}

static Bool
xmir_output_mode_valid(xf86OutputPtr output, DisplayModePtr pModes)
{
    return MODE_OK;
}

static DisplayModePtr
xmir_output_get_modes(xf86OutputPtr xf86output)
{
    MirDisplayOutput *mir_output = xf86output->driver_private;
    DisplayModePtr modes = NULL, mode;

    for (int i = 0; i < mir_output->num_modes; i++) {
        char *mode_name = malloc(32);
        mode = xf86CVTMode(mir_output->modes[i].horizontal_resolution,
                           mir_output->modes[i].vertical_resolution,
                           mir_output->modes[i].refresh_rate,
                           FALSE, FALSE);
        /* And now, because the CVT standard doesn't support such common resolutions as 1366x768... */
        mode->VDisplay = mir_output->modes[i].vertical_resolution;
        mode->HDisplay = mir_output->modes[i].horizontal_resolution;

        mode->type = M_T_DRIVER;
        /* TODO: Get preferred mode from Mir, or get a guarantee that the first mode is preferred */
        if (i == 0)
            mode->type |= M_T_PREFERRED;

        snprintf(mode_name, 32, "%dx%d", mode->HDisplay, mode->VDisplay);
        mode->name = mode_name;
        modes = xf86ModesAdd(modes, mode);
    }
    /* TODO: Get Mir to send us the EDID blob and add that */

    return modes;
}

static void
xmir_output_destroy(xf86OutputPtr xf86output)
{
    /* The MirDisplayOutput* in driver_private gets cleaned up by 
       mir_display_config_destroy() */
}

static const xf86OutputFuncsRec xmir_output_funcs = {
    .dpms       = xmir_output_dpms,
    .detect     = xmir_output_detect,
    .mode_valid = xmir_output_mode_valid,
    .get_modes  = xmir_output_get_modes,
    .destroy    = xmir_output_destroy
};

static Bool
xmir_resize(ScrnInfoPtr scrn, int width, int height)
{
    if (scrn->virtualX == width && scrn->virtualY == height)
        return TRUE;
    /* We don't handle resize at all, we must match the compositor size */
    return FALSE;
}

static const xf86CrtcConfigFuncsRec config_funcs = {
    xmir_resize
};

static void
xmir_output_init(ScrnInfoPtr scrn, MirDisplayOutput *output, int num)
{
    xf86OutputPtr xf86output;
    char name[32];

    snprintf(name, sizeof name, "XMIR-%d", num);

    xf86output = xf86OutputCreate(scrn, &xmir_output_funcs, name);
    xf86output->possible_crtcs = 0xffffffff;
    xf86output->possible_clones = 0xffffffff;
    xf86output->driver_private = output;

    xf86output->interlaceAllowed = FALSE;
    xf86output->doubleScanAllowed = FALSE;
    xf86output->mm_width = output->physical_width_mm;
    xf86output->mm_height = output->physical_height_mm;
    /* TODO: Subpixel order from Mir */
    xf86output->subpixel_order = SubPixelUnknown;
}

Bool
xmir_mode_pre_init(ScrnInfoPtr scrn, xmir_screen *xmir)
{
    int i;
    MirDisplayConfiguration *display_config;
    xf86CrtcPtr xf86crtc;

    /* Set up CRTC config functions */
    xf86CrtcConfigInit(scrn, &config_funcs);

    /* What range of sizes can we actually support? */
    xf86CrtcSetSizeRange(scrn,
                         320, 320,
                         8192, 8192);


    display_config =
        mir_connection_create_display_config(xmir_connection_get());

    for (i = 0; i < display_config->num_displays; i++)
        xmir_output_init(scrn, display_config->displays + i, i);

    /* TODO: get the number of CRTCs from Mir */
    for (i = 0; i < display_config->num_displays; i++) {
        xf86crtc = xf86CrtcCreate(scrn, &crtc_funcs);
        xf86crtc->driver_private = NULL;
    }

    xf86InitialConfiguration(scrn, TRUE);
  
    return TRUE;
}
