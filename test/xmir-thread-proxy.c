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

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "xmir-private.h"

struct test_content {
	int *variable;
	int value;
};

static void
_test_callback(void *msg_content)
{
	struct test_content *content = msg_content;
	*content->variable = content->value;
}

static void
xmir_test_marshall_to_eventloop(void)
{
	xmir_marshall_handler *test_marshaller;
	struct test_content msg;
	int check = 0;

	xmir_init_thread_to_eventloop();

	test_marshaller = xmir_register_handler(&_test_callback, sizeof msg);

	msg.variable = &check;
	msg.value = 1;

	xmir_post_to_eventloop(test_marshaller, &msg);
	xmir_process_from_eventloop();

	assert(check == 1);
}

static void
_racy_test_callback(void *msg_content)
{
	struct test_content *content = msg_content;
	int new_value = *content->variable + 1;
	/* Ensure the other threads get to run and see the old value of content->variable */
	usleep(100);
	*content->variable = new_value;
}

struct thread_context {
	xmir_marshall_handler *marshaller;
	struct test_content *msg;
};

static void *
_post_racy_msg(void *thread_ctx)
{
	struct thread_context *ctx = thread_ctx;

	xmir_post_to_eventloop(ctx->marshaller, ctx->msg);

	return NULL;
}

#define NUM_THREADS 10

static void
xmir_test_many_threads_to_eventloop(void)
{
	pthread_t threads[NUM_THREADS];
	pthread_attr_t attr;
	xmir_marshall_handler *test_marshaller;
	struct thread_context ctx;
	struct test_content msg;
	int check = 0, i;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	xmir_init_thread_to_eventloop();

	test_marshaller = xmir_register_handler(&_racy_test_callback, sizeof msg);

	msg.variable = &check;

	ctx.marshaller = test_marshaller;
	ctx.msg = &msg;

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_create(&threads[i], &attr, _post_racy_msg, (void *)&ctx);
	}

	pthread_attr_destroy(&attr);

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	xmir_process_from_eventloop();

	assert(check == NUM_THREADS);	
}

int
main(int argc, char **argv)
{
	xmir_test_marshall_to_eventloop();
	xmir_test_many_threads_to_eventloop();
}