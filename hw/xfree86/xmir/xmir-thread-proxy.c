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

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "xmir-private.h"

struct xmir_marshall_handler {
	void (*msg_handler)(void *msg);
	size_t msg_size;
	char msg[];
};

static int pipefds[2];

static void
xmir_wakeup_handler(pointer data, int err, pointer read_mask)
{
    if (err >= 0 && FD_ISSET(pipefds[0], (fd_set *)read_mask))
        xmir_process_from_eventloop();
}

void
xmir_init_thread_to_eventloop(void)
{
	pipe(pipefds);

	/* Set the read end to not block; we'll pull from this in the event loop
	 * We don't need to care about the write end, as that'll be written to
	 * from its own thread
	 */
	fcntl(pipefds[0], F_SETFL, O_NONBLOCK);

	AddGeneralSocket(pipefds[0]);
	RegisterBlockAndWakeupHandlers(NULL, xmir_wakeup_handler, NULL);
}

xmir_marshall_handler *
xmir_register_handler(void (*msg_handler)(void *msg), size_t msg_size)
{
	xmir_marshall_handler *handler;

	if (msg_size + sizeof *handler > PIPE_BUF)
		return NULL;

	handler = malloc(sizeof *handler + msg_size);
	if (!handler)
		return NULL;

	handler->msg_handler = msg_handler;
	handler->msg_size = msg_size;
	return handler;
}

void
xmir_post_to_eventloop(xmir_marshall_handler *handler, void *msg) 
{
	const int total_size = sizeof *handler + handler->msg_size;
	/* We require the total size to be less than PIPE_BUF to ensure an atomic write */
	assert(total_size < PIPE_BUF);

	memcpy(handler->msg, msg, handler->msg_size);
	write(pipefds[1], handler, total_size);
}

void
xmir_process_from_eventloop(void)
{
	xmir_marshall_handler handler;
	void *msg;

	for (;;) {
		if (read(pipefds[0], &handler, sizeof handler) < 0) {
			return;
		}

		msg = malloc(handler.msg_size);
		if(read(pipefds[0], msg, handler.msg_size) == handler.msg_size)
			(*handler.msg_handler)(msg);
		free(msg);
	}
}

