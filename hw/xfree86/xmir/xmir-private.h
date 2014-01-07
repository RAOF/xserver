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

#ifndef _XMIR_PRIVATE_H
#define _XMIR_PRIVATE_H

#include <mir_toolkit/mir_client_library.h>
#include "xmir.h"
#include "xf86str.h"
#include "list.h"
#include "scrnintstr.h"
#include "regionstr.h"

#define MIR_MAX_BUFFER_AGE 3

typedef struct xmir_marshall_handler xmir_marshall_handler;

struct xmir_screen {
    ScrnInfoPtr            scrn;
    CreateWindowProcPtr    CreateWindow;
    DestroyWindowProcPtr   DestroyWindow;
    xmir_driver *          driver;
    xmir_marshall_handler *submit_rendering_handler;
    xmir_marshall_handler *hotplug_event_handler;
    xmir_marshall_handler *focus_event_handler;
    struct xorg_list       damage_list;
    struct xmir_window   **root_window_fragments; /* NULL terminated array of xmir_window * */
    unsigned int           dpms_on:1;             /* Until Mir is less stupid about DPMS */
};

struct xmir_window {
    WindowPtr           win;
    MirSurface         *surface;
    RegionRec           region;
    RegionRec           past_damage[MIR_MAX_BUFFER_AGE];
    DamagePtr           damage;
    int                 damage_index;
    struct xorg_list    link_damage;
    unsigned int        has_free_buffer:1;
    unsigned int        damaged:1;
};

MirConnection *
xmir_connection_get(void);

xmir_screen *
xmir_screen_get(ScreenPtr screen);

xmir_window *
xmir_window_get(WindowPtr win);

void
xmir_window_enable_damage_tracking(xmir_window *xmir_win);

void
xmir_window_disable_damage_tracking(xmir_window *xmir_win);


Bool
xmir_screen_init_window(ScreenPtr screen, xmir_screen *xmir);

Bool
xmir_mode_pre_init(ScrnInfoPtr scrn, xmir_screen *xmir);

void
xmir_init_thread_to_eventloop(void);

xmir_marshall_handler *
xmir_register_handler(void (*msg_handler)(void *msg), size_t msg_size);

void
xmir_post_to_eventloop(xmir_marshall_handler *handler, void *msg);

void
xmir_process_from_eventloop(void);

 #endif /* _MIR_PRIVATE_H */
