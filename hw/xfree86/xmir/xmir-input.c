/*
 * Copyright © 2014 Canonical, Inc
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
 *   Robert Carr <robert.carr@canonical.com>
 */

#include <xorg-config.h>
#include <X11/extensions/XI2proto.h>

#include "xmir.h"
#include "xmir-private.h"
#include "xmir-input.h"

#include "list.h"
#include "xf86.h"
#include "windowstr.h"
#include "xf86Priv.h"
#include "exevents.h"

#include <input.h>
#include <xf86Xinput.h>
#include <inpututils.h>
#include <xkbsrv.h>
#include <xserver-properties.h>

#include <mir_toolkit/mir_client_library.h>

static void
xmir_pointer_control(DeviceIntPtr device, PtrCtrl *ctrl)
{
    /* The rumor is there is nothing to do here */
}

static int
xmir_pointer_proc(DeviceIntPtr device, int what)
{
#define num_buttons 3
#define num_axes 2

    BYTE map[num_buttons+1];
    Atom btn_labels[num_buttons] = { 0 };
    Atom axes_labels[num_axes] = { 0 };
    int i = 0;

    switch (what) {
    case DEVICE_INIT:
         device->public.on = FALSE;

        for (i = 1; i <= num_buttons; i++)
            map[i] = i;

        btn_labels[0] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_LEFT);
        btn_labels[1] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_MIDDLE);
        btn_labels[2] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_RIGHT);

        axes_labels[0] = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_X);
        axes_labels[1] = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_Y);

        if (!InitValuatorClassDeviceStruct(device, 2, btn_labels,
                                           GetMotionHistorySize(), Absolute))
            return BadValue;

        /* Valuators */
        InitValuatorAxisStruct(device, 0, axes_labels[0],
                               0, 0xFFFF, 10000, 0, 10000, Absolute);
        InitValuatorAxisStruct(device, 1, axes_labels[1],
                               0, 0xFFFF, 10000, 0, 10000, Absolute);

        if (!InitPtrFeedbackClassDeviceStruct(device, xmir_pointer_control))
            return BadValue;
        if (!InitButtonClassDeviceStruct(device, 3, btn_labels, map))
            return BadValue;

        return Success;
    case DEVICE_ON:
        device->public.on = TRUE;
        return Success;
    case DEVICE_OFF:
    case DEVICE_CLOSE:
        device->public.on = FALSE;
        return Success;
    }

    return BadMatch;
}

static void
xmir_keyboard_control(DeviceIntPtr device, KeybdCtrl *ctrl)
{
    /* FIXME: Rumor is we should deal with CAPSFLAG etc being set in
     * ctrl->leds . Set keyboard leds and stuff? */
}

static int
xmir_keyboard_proc(DeviceIntPtr device, int what)
{
    XkbRMLVOSet rmlvo;

    switch (what) {
    case DEVICE_INIT:
	device->public.on = FALSE;

    XkbInitRules(&rmlvo, "evdev", "pc104", "us", "dvorak", NULL);

        // TODO: Mir already does XKB mapping so it seems strange to replicate it here...
        // requires investigation
        if (!InitKeyboardDeviceStruct(device, &rmlvo, NULL, xmir_keyboard_control))
            return BadValue;

        return Success;
    case DEVICE_ON:
	device->public.on = TRUE;
        return Success;

    case DEVICE_OFF:
    case DEVICE_CLOSE:
	device->public.on = FALSE;
        return Success;
    }

    return BadMatch;
}

static DeviceIntPtr
xmir_create_input_device(xmir_screen *xmir, const char *driver, DeviceProc device_proc)
{
    DeviceIntPtr dev = NULL;
    static Atom type_atom;
    char name[32];

    dev = AddInputDevice(serverClient, device_proc, TRUE);
    if (dev == NULL)
        return NULL;

    if (type_atom == None)
        type_atom = MakeAtom(driver, strlen(driver), TRUE);

    snprintf(name, sizeof name, "%s:%d", driver, 0); // TODO: ID
    AssignTypeAndName(dev, type_atom, name);

    dev->public.devicePrivate = xmir;
    dev->type = SLAVE;
    dev->spriteInfo->spriteOwner = FALSE;

    LogMessage(X_INFO, "config/xmir: Adding input device %s\n", driver);

    ActivateDevice(dev, FALSE);
    EnableDevice(dev, FALSE);

    return dev;
}

static void 
xmir_window_handle_key_event(xmir_screen *xmir,
                             MirKeyEvent const* kev)
{
    // TODO: Ideally we would be using the keycode, already mapped by mir...
    int32_t scan_code = kev->scan_code + 8; // X scan codes differ by 8 from linux/input.h scancodes.

    ValuatorMask mask;
    valuator_mask_zero(&mask);

    QueueKeyboardEvents(xmir->keyboard,
                        kev->action == mir_key_action_up ? KeyRelease : KeyPress,
                        scan_code, &mask);
}

static void
xmir_window_handle_motion_event(xmir_screen *xmir,
                                MirMotionEvent const* mev)
{
    // FIXME: https://bugs.launchpad.net/mir/+bug/1197108
    switch (mev->action)
    {
    case mir_motion_action_move:
    case mir_motion_action_hover_move:
    {
        ValuatorMask mask;

        valuator_mask_zero(&mask);
        valuator_mask_set(&mask, 0, mev->pointer_coordinates[0].x);
        valuator_mask_set(&mask, 1, mev->pointer_coordinates[0].y);

        QueuePointerEvents(xmir->pointer, MotionNotify, 0,
                           POINTER_ABSOLUTE | POINTER_SCREEN, &mask);
        break;
    }
    case mir_motion_action_down:
    case mir_motion_action_up:
    {
            ValuatorMask mask;

            // TODO: Messed up...need to track past button state of get better API to mir to tell
            // which button was actually released in case of motion_action_up.
            int button_states[3] = { ButtonRelease, ButtonRelease, ButtonRelease };

            if (mev->button_state & mir_motion_button_primary)
                button_states[0] = ButtonPress;
            else if (mev->button_state & mir_motion_button_tertiary)
                button_states[1] = ButtonPress;
            else if (mev->button_state & mir_motion_button_secondary)
                button_states[2] = ButtonPress;

            valuator_mask_zero(&mask);
            for (int i = 0; i < 3; i++)
                QueuePointerEvents(xmir->pointer,
                    button_states[i], i+1,
                    0, &mask);
    }
    default:
        break;
    }
}


static void xmir_handle_input_in_main_thread(void *vctx)
{
    XMirEventContext *ctx = (XMirEventContext *)vctx;
    MirEvent *ev = &(ctx->ev);

    switch (ev->type)
    {
    case mir_event_type_key:
        xmir_window_handle_key_event(ctx->xmir, &ev->key);
        break;
    case mir_event_type_motion:
        xmir_window_handle_motion_event(ctx->xmir, &ev->motion);
        break;
    default:
        break;
    }
}

void xmir_surface_handle_event(MirSurface *surface, MirEvent const* ev,
                               void *context)
{
    xmir_screen *xmir = (void *)context;
    XMirEventContext ectx;

    // To prevent copying the whole MirEvent (three times in total)
    // we could pass only the information used by X
    memcpy(&ectx.ev, ev, sizeof(MirEvent));
    ectx.xmir = xmir;

    xmir_post_to_eventloop(xmir->input_handler, &ectx);
}

int
xmir_create_input_devices(xmir_screen *xmir)
{
    xmir->input_handler = xmir_register_handler(&xmir_handle_input_in_main_thread, sizeof (XMirEventContext));
    xmir->keyboard = xmir_create_input_device(xmir, "xmir-keyboard", xmir_keyboard_proc);
    xmir->pointer = xmir_create_input_device(xmir, "xmir-pointer", xmir_pointer_proc);

    // TODO: Error handling

    return 1;
}

