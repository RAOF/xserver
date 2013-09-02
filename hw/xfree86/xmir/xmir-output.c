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
#include <math.h>
#include <atomic_ops.h>

#include <xorg-config.h>
#include "xmir.h"
#include "xmir-private.h"
#include "xf86Crtc.h"
#include "xf86Priv.h"

struct xmir_crtc {
    xmir_screen             *xmir;
    xmir_window             *root_fragment;
    MirDisplayConfiguration *config;
};

static void
crtc_dpms(xf86CrtcPtr drmmode_crtc, int mode)
{
}

static void
xmir_surface_event_handler(MirSurface *surf, MirEvent const *event, void *ctx)
{
    xmir_screen *xmir = ctx;
    Bool new_focus;
    if (event->type == mir_event_type_surface) {
        MirSurfaceEvent const *surf_event = (MirSurfaceEvent const *)event;
        if (surf_event->attrib == mir_surface_attrib_focus) {
            switch(surf_event->value) {
            case mir_surface_unfocused:
            {
                AO_t val = AO_fetch_and_sub1(&xmir->focus_count);
                xf86Msg(X_INFO, "[XMir] Received unfocus event. Focus count %d\n", val);
                if (val == 0) {
                    new_focus = FALSE;
                    xmir_post_to_eventloop(xmir->focus_event_handler, &new_focus);
                }
                if (val < 0)
                    xf86Msg(X_ERROR, "[xmir] Focus count dropped below 0\n");

                break;
            }
            case mir_surface_focused:
            {
                AO_t val;
                if ((val = AO_fetch_and_add1(&xmir->focus_count)) == 1) {
                    new_focus = TRUE;
                    xmir_post_to_eventloop(xmir->focus_event_handler, &new_focus);
                }
                xf86Msg(X_INFO, "[XMir] Received focus event. Focus count %d\n", val);
                break;
            }
            default:
                xf86Msg(X_WARNING, "[xmir] Unknown focus event received\n");
            }
        }
    }
}

static void
xmir_output_populate(xf86OutputPtr xf86output, MirDisplayOutput *output)
{
    /* We can always arbitrarily clone and output */
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

static Bool
xmir_mir_mode_matches(MirDisplayMode *mir_mode, DisplayModePtr X_mode)
{
    return (mir_mode->vertical_resolution == X_mode->VDisplay &&
            mir_mode->horizontal_resolution == X_mode->HDisplay /*&&
            fabs(mir_mode->refresh_rate - X_mode->VRefresh) < 0.01*/);
}

static Bool
xmir_set_mode_for_output(MirDisplayOutput *output,
                         DisplayModePtr mode)
{
    for (int i = 0; i < output->num_modes; i++) {
        xf86Msg(X_INFO, "Checking against mode (%dx%d)\n",
                output->modes[i].horizontal_resolution,
                output->modes[i].vertical_resolution);
        if (xmir_mir_mode_matches(&output->modes[i], mode)) {
            output->current_mode = i;
            output->used = 1;
            xf86Msg(X_INFO, "Matched mode %d\n", i);
            return TRUE;
        }
    }
    return FALSE;
}

static uint32_t
xmir_update_outputs_for_crtc(xf86CrtcPtr crtc, DisplayModePtr mode, int x, int y)
{
    xf86CrtcConfigPtr crtc_cfg = XF86_CRTC_CONFIG_PTR(crtc->scrn);
    uint32_t representative_output_id = mir_display_output_id_invalid;

    for (int i = 0; i < crtc_cfg->num_output; i++) {
        /* If this output should be driven by our "CRTC", set its mode */
        if (crtc_cfg->output[i]->crtc == crtc) {
            MirDisplayOutput *output = crtc_cfg->output[i]->driver_private;
            xmir_set_mode_for_output(output, mode);
            output->position_x = x;
            output->position_y = y;
            representative_output_id = output->output_id;
        }
    }
    return representative_output_id;
}

static void
xmir_disable_unused_outputs(xf86CrtcPtr crtc)
{
    xf86CrtcConigPtr crtc_cfg = XF86_CRTC_CONFIG_PTR(crtc->scrn);

    for (int i = 0; i < crtc_cfg->num_output; i++)
        /* If any outputs are no longer associated with a CRTC, disable them */
        if (crtc_cfg->output[i]->crtc == NULL)
            ((MirDisplayOutput*)crtc_cfg->output[i]->driver_private)->used = 0;
}

static void
xmir_stupid_callback(MirSurface *surf, void *ctx)
{
}

static void
xmir_dump_config(MirDisplayConfiguration *config)
{
  for (int i = 0; i < config->num_outputs; i++)
    {
      xf86Msg(X_INFO, "Output %d (%s, %s) has mode %d (%d x %d @ %.2f), position (%d,%d)\n",
	      config->outputs[i].output_id,
	      config->outputs[i].connected ? "connected" : "disconnected",
	      config->outputs[i].used ? "enabled" : "disabled",
	      config->outputs[i].current_mode,
          config->outputs[i].used ? config->outputs[i].modes[config->outputs[i].current_mode].horizontal_resolution : 0,
          config->outputs[i].used ? config->outputs[i].modes[config->outputs[i].current_mode].vertical_resolution : 0,
          config->outputs[i].used ? config->outputs[i].modes[config->outputs[i].current_mode].refresh_rate : 0,
	      config->outputs[i].position_x,
	      config->outputs[i].position_y);
    }
}

static void
xmir_update_config(xf86CrtcConfigPtr crtc_cfg)
{
    MirDisplayConfiguration *new_config;
    struct xmir_crtc *xmir_crtc = crtc_cfg->crtc[0]->driver_private;

    mir_display_config_destroy(xmir_crtc->config);

    new_config = mir_connection_create_display_config(xmir_connection_get());
    for (int i = 0; i < crtc_cfg->num_crtc; i++) {
        xmir_crtc = crtc_cfg->crtc[i]->driver_private;
        xmir_crtc-> config = new_config;
    }

    if (crtc_cfg->num_output != new_config->num_outputs)
        FatalError("[xmir] New Mir config has different number of outputs?");

    for (int i = 0; i < crtc_cfg->num_output ; i++) {
        /* TODO: Ensure that the order actually matches up */
        xmir_output_populate(crtc_cfg->output[i], new_config->outputs + i);
    }
}

static Bool
xmir_crtc_set_mode_major(xf86CrtcPtr crtc, DisplayModePtr mode,
                         Rotation rotation, int x, int y)
{
    MirSurfaceParameters params = {
        .name = "Xorg",
        .width = mode->HDisplay,
        .height = mode->VDisplay,
        .pixel_format = mir_pixel_format_xrgb_8888,
        .buffer_usage = mir_buffer_usage_hardware,
        .output_id = mir_display_output_id_invalid
    };
    BoxRec output_bounds = {
        .x1 = x,
        .y1 = y,
        .x2 = x + mode->HDisplay,
        .y2 = y + mode->VDisplay
    };
    MirEventDelegate focus_delegate = {
        .callback = &xmir_surface_event_handler,
        .context = xmir_screen_get(xf86ScrnToScreen(crtc->scrn))
    };
    struct xmir_crtc *xmir_crtc = crtc->driver_private;
    uint32_t output_id = mir_display_output_id_invalid;
    MirSurface *surface;
    const char *error_msg;

    if (mode->HDisplay == 0 || mode->VDisplay == 0)
        return FALSE;    

    xf86Msg(X_INFO, "Initial configuration:\n");
    xmir_dump_config(xmir_crtc->config);

    xf86Msg(X_INFO, "Setting mode to %dx%d (%.2f)", mode->HDisplay, mode->VDisplay, mode->VRefresh);
    output_id = xmir_update_outputs_for_crtc(crtc, mode, x, y);
    xmir_disable_unused_outputs(crtc);

    xf86Msg(X_INFO, "Updated configuration:\n");

    xmir_dump_config(xmir_crtc->config);
    mir_wait_for(mir_connection_apply_display_config(xmir_connection_get(),
                                                     xmir_crtc->config));
    error_msg = mir_connection_get_error_message(xmir_connection_get());
    if (*error_msg != '\0') {
        xf86Msg(X_ERROR, "[xmir] Failed to set new display config: %s\n",
                error_msg);
        return FALSE;
        /* TODO: Restore correct config cache */
    }

    xmir_update_config(XF86_CRTC_CONFIG_PTR(crtc->scrn));

    xf86Msg(X_INFO, "Post-modeset config:\n");
    xmir_dump_config(xmir_crtc->config);

    if (xmir_crtc->root_fragment->surface != NULL)
        mir_wait_for(mir_surface_release(xmir_crtc->root_fragment->surface, xmir_stupid_callback, NULL));

    if (output_id == mir_display_output_id_invalid) {
        xmir_crtc->root_fragment->surface = NULL;
        return TRUE;
    }

    params.output_id = output_id;
    xf86Msg(X_INFO, "Putting surface on output %d\n", output_id);
    surface = mir_connection_create_surface_sync(xmir_connection_get(),
                                                 &params);
    if (!mir_surface_is_valid(surface)) {
        xf86Msg(X_ERROR,
                "[xmir] Failed to create surface for %dx%d mode: %s\n",
                mode->HDisplay, mode->VDisplay,
                mir_surface_get_error_message(surface));
        return FALSE;
    }
    mir_surface_set_event_handler(surface, &focus_delegate);

    xmir_crtc->root_fragment->surface = surface;

    /* During X server init this will be NULL.
       This is fixed up in xmir_window_create */
    xmir_crtc->root_fragment->win = xf86ScrnToScreen(crtc->scrn)->root;

    RegionInit(&xmir_crtc->root_fragment->region, &output_bounds, 0);
    xmir_crtc->root_fragment->has_free_buffer = TRUE;

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

static void
xmir_crtc_destroy(xf86CrtcPtr crtc)
{
    struct xmir_crtc *xmir_crtc = crtc->driver_private;

    if (xmir_crtc->root_fragment->surface != NULL)
        mir_surface_release(xmir_crtc->root_fragment->surface, NULL, NULL);

    free(xmir_crtc);
}

static const xf86CrtcFuncsRec crtc_funcs = {
    .dpms                = crtc_dpms,
    .set_mode_major      = xmir_crtc_set_mode_major,
    .set_cursor_colors   = crtc_set_cursor_colors,
    .set_cursor_position = crtc_set_cursor_position,
    .show_cursor         = crtc_show_cursor,
    .hide_cursor         = crtc_hide_cursor,
    .load_cursor_argb    = crtc_load_cursor_argb,
    .shadow_create       = crtc_shadow_create,
    .shadow_allocate     = crtc_shadow_allocate,
    .shadow_destroy      = crtc_shadow_destroy,
    .destroy             = xmir_crtc_destroy,
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
    DisplayModePtr modes = NULL, mode = NULL;

    for (int i = 0; i < mir_output->num_modes; i++) {
        /* Check if mode differs only by refresh rate from previous mode and reject */
        /* TODO: Remove this check and instead handle refresh rate correctly */
        if (mode != NULL)
            if (xmir_mir_mode_matches((mir_output->modes + i), mode))
                continue;

        char *mode_name = malloc(32);
        mode = xf86CVTMode(mir_output->modes[i].horizontal_resolution,
                           mir_output->modes[i].vertical_resolution,
                           60.0f,
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


struct xmir_visit_set_pixmap_window {
    PixmapPtr old, new;
};

static int
xmir_visit_set_window_pixmap(WindowPtr window, pointer data)
{
    struct xmir_visit_set_pixmap_window *visit = data;

    if (window->drawable.pScreen->GetWindowPixmap(window) == visit->old) {
        window->drawable.pScreen->SetWindowPixmap(window, visit->new);
        return WT_WALKCHILDREN;
    }

    return WT_DONTWALKCHILDREN;
}

static void
xmir_set_screen_pixmap(PixmapPtr old_front, PixmapPtr new_front)
{
    struct xmir_visit_set_pixmap_window visit = {
        .old = old_front,
        .new = new_front
    };
    (old_front->drawable.pScreen->SetScreenPixmap)(new_front);

    TraverseTree(old_front->drawable.pScreen->root, &xmir_visit_set_window_pixmap, &visit);
}

static Bool
xmir_resize(ScrnInfoPtr scrn, int width, int height)
{
    xf86CrtcConfigPtr crtc_cfg = XF86_CRTC_CONFIG_PTR(scrn);
    ScreenPtr screen = xf86ScrnToScreen(scrn);
    PixmapPtr old_screen_pixmap, new_screen_pixmap;

    if (scrn->virtualX == width && scrn->virtualY == height)
        return TRUE;

    old_screen_pixmap = screen->GetScreenPixmap(screen);
    new_screen_pixmap = screen->CreatePixmap(screen, width, height, scrn->depth,
                                             CREATE_PIXMAP_USAGE_BACKING_PIXMAP);

    if (!new_screen_pixmap)
        return FALSE;

    scrn->virtualX = width;
    scrn->virtualY = height;
    scrn->displayWidth = width;

    for (int i = 0; i < crtc_cfg->num_crtc; i++) {
        xf86CrtcPtr crtc = crtc_cfg->crtc[i];

        if (!crtc->enabled)
            continue;

        xmir_crtc_set_mode_major(crtc, &crtc->mode,
                                 crtc->rotation, crtc->x, crtc->y);
    }

    xmir_set_screen_pixmap(old_screen_pixmap, new_screen_pixmap);
    screen->DestroyPixmap(old_screen_pixmap);

    xf86_reload_cursors(screen);

    return TRUE;
}

static const xf86CrtcConfigFuncsRec config_funcs = {
    xmir_resize
};

static void
xmir_handle_hotplug(void *ctx)
{
    ScrnInfoPtr scrn = *(ScrnInfoPtr *)ctx;
    xf86CrtcConfigPtr crtc_config = XF86_CRTC_CONFIG_PTR(scrn);

    if (crtc_config->num_crtc == 0)
        FatalError("[xmir] Received hotplug event, but have no CRTCs?\n");

    xmir_update_config(crtc_config);

    /* Trigger RANDR refresh */
    RRGetInfo(xf86ScrnToScreen(scrn), TRUE);   
}

static void
xmir_display_config_callback(MirConnection *unused, void *ctx)
{
    xmir_screen *xmir = ctx;

    xmir_post_to_eventloop(xmir->hotplug_event_handler, &xmir->scrn);
}

static void xmir_handle_focus_event(void *ctx)
{
    Bool has_focus = *(Bool *)ctx;

    xf86Msg(X_INFO, "[XMir] Handling focus event, new_focus = %s\n", has_focus ? "TRUE" : "FALSE");

    if ((xf86VTOwner() && !has_focus) ||
        (!xf86VTOwner() && has_focus))
        xf86Info.vtRequestsPending = TRUE;
}

Bool
xmir_mode_pre_init(ScrnInfoPtr scrn, xmir_screen *xmir)
{
    int i;
    MirDisplayConfiguration *display_config;
    xf86CrtcPtr xf86crtc;

    /* Set up CRTC config functions */
    xf86CrtcConfigInit(scrn, &config_funcs);

    /* We don't scanout of a single surface, so we don't have a scanout limit */
    xf86CrtcSetSizeRange(scrn,
                         320, 320,
                         INT16_MAX, INT16_MAX);


    /* Hook up focus -> VT switch proxy */
    xmir->focus_count = 0;
    xmir->focus_event_handler = 
        xmir_register_handler(&xmir_handle_focus_event,
                              sizeof (Bool));
    if (xmir->focus_event_handler == NULL)
        return FALSE;

    /* Hook up hotplug notification */
    xmir->hotplug_event_handler =
        xmir_register_handler(&xmir_handle_hotplug,
                              sizeof (ScreenPtr));

    mir_connection_set_display_config_change_callback(
        xmir_connection_get(),
        &xmir_display_config_callback, xmir);

    display_config =
        mir_connection_create_display_config(xmir_connection_get());

    xmir->root_window_fragments = malloc((display_config->num_outputs + 1) *
                                         sizeof(xmir_window *));
    xmir->root_window_fragments[display_config->num_outputs] = NULL;

    if (xmir->root_window_fragments == NULL)
        return FALSE;

    for (i = 0; i < display_config->num_outputs; i++) {
        xf86OutputPtr xf86output;
        char name[32];

        snprintf(name, sizeof name, "XMIR-%d", i);

        xf86output = xf86OutputCreate(scrn, &xmir_output_funcs, name);

        xmir_output_populate(xf86output, display_config->outputs + i);
    }

    /* TODO: Get the number of CRTCs from Mir */
    for (i = 0; i < display_config->num_outputs; i++) {
        struct xmir_crtc *xmir_crtc = malloc(sizeof *xmir_crtc);
        if (xmir_crtc == NULL)
            return FALSE;

        xmir_crtc->xmir = xmir;
        xmir_crtc->root_fragment = calloc(1, sizeof *xmir_crtc->root_fragment);
        xmir_crtc->config = display_config;

        if (xmir_crtc->root_fragment == NULL)
            return FALSE;

        xmir->root_window_fragments[i] = xmir_crtc->root_fragment;
        RegionNull(&xmir_crtc->root_fragment->region);

        xf86crtc = xf86CrtcCreate(scrn, &crtc_funcs);
        xf86crtc->driver_private = xmir_crtc;
    }

    xf86SetScrnInfoModes(scrn);

    /* TODO: Use initial Mir state rather than setting up our own */
    xf86InitialConfiguration(scrn, TRUE);
  
    return TRUE;
}
