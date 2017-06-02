/*
 *
 *    Copyright (c) 2016 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 *    Description:
 *      Flavor of protothreads for handling asynchronous I/O, via select()
 *
 */

#ifndef wpantund_nlpt_glib_h
#define wpantund_nlpt_glib_h

#include <glib.h>
#include <glib-object.h>

struct nlpt {
	struct pt pt;
	char (*func)(struct nlpt* nlpt);
	int status;
	GSource*        source;
	GSource*        fd_source;
	GPollFD poll_fd;
};

static void
___nlpt_source_destroy_notify(struct nlpt* nlpt)
{
	syslog(LOG_INFO, "Destroying protothread %s.",
	       nlpt ? g_source_get_name((nlpt)->source) : "(NULL)");
	if (nlpt) {
		g_source_unref((nlpt)->source);
		(nlpt)->source = NULL;
	}
}

static gboolean
___nlpt_source_event_handler(struct nlpt* nlpt)
{
	gboolean ret = FALSE;

	syslog(LOG_INFO, "Running protothread %s.",
	       nlpt ? g_source_get_name((nlpt)->source) : "(NULL)");

	switch ((nlpt->status = nlpt->func(nlpt))) {
	default:
	case PT_YIELDED:
	case PT_WAITING:
		ret = TRUE;
		break;

	case PT_EXITED:
	case PT_ENDED:
		break;
	}
	ret = TRUE;
	return ret;
}

static gboolean
___nlpt_unix_fd_source_event_handler(
    void* ignore, void* ignore2, struct nlpt* nlpt
    )
{
	return ___nlpt_source_event_handler(nlpt);
}

#define NLPT_GLIB_POLL_READ_FLAGS \
	(GIOCondition)(G_IO_IN | G_IO_HUP | \
	               G_IO_ERR)
#define NLPT_GLIB_POLL_WRITE_FLAGS  (GIOCondition)(G_IO_OUT | G_IO_ERR)

static void _nlpt_cleanup_read_fd_source(
    struct nlpt* nlpt, int fd
    )
{
	g_source_remove_poll(nlpt->fd_source, &nlpt->poll_fd);
	g_source_destroy(nlpt->fd_source);
	g_source_unref(nlpt->fd_source);
	nlpt->fd_source = NULL;
}

static void _nlpt_cleanup_write_fd_source(
    struct nlpt* nlpt, int fd
    )
{
	g_source_remove_poll(nlpt->fd_source, &nlpt->poll_fd);
	g_source_destroy(nlpt->fd_source);
	g_source_unref(nlpt->fd_source);
	nlpt->fd_source = NULL;
}


static void
_nlpt_setup_fd_source(
    struct nlpt* nlpt, int fd, short flags
    )
{
	if (nlpt->fd_source)
		_nlpt_cleanup_fd_source(nlpt, fd);
	nlpt->fd_source = g_timeout_source_new(4000);
	assert(nlpt->fd_source != NULL);
	g_source_set_callback(
	    nlpt->fd_source,
	    (GSourceFunc)___nlpt_source_event_handler,
	    (gpointer)nlpt,
	    NULL     //(GDestroyNotify)___nlpt_source_destroy_notify
	    );
	nlpt->poll_fd.fd = fd;
	nlpt->poll_fd.events = (GIOCondition)flags;
	nlpt->poll_fd.revents = (GIOCondition)0;
	g_source_add_poll(nlpt->fd_source, &nlpt->poll_fd);
	g_source_attach((nlpt)->fd_source, NULL);
}

static void
_nlpt_setup_read_fd_source(
    struct nlpt* nlpt, int fd
    )
{
	_nlpt_setup_fd_source(nlpt, fd, NLPT_GLIB_POLL_READ_FLAGS);
}

static void
_nlpt_setup_write_fd_source(
    struct nlpt* nlpt, int fd
    )
{
	_nlpt_setup_fd_source(nlpt, fd, NLPT_GLIB_POLL_WRITE_FLAGS);
}

static bool
nlpt_hook_check_read_fd_source(
    struct nlpt* nlpt, int fd
    )
{
	return _nlpt_checkpoll(nlpt->poll_fd.fd, NLPT_GLIB_POLL_READ_FLAGS);
}

static bool
nlpt_hook_check_write_fd_source(
    struct nlpt* nlpt, int fd
    )
{
	return _nlpt_checkpoll(nlpt->poll_fd.fd, NLPT_GLIB_POLL_WRITE_FLAGS);
}

static void
_nlpt_init(
    struct nlpt* nlpt, char (*f)(struct nlpt* nlpt), const char* name
    )
{
	(nlpt)->func = (f);
	if ((nlpt)->source) {
		g_source_destroy((nlpt)->source);
		g_source_unref((nlpt)->source);
	}
	(nlpt)->source = g_timeout_source_new(4000);
	assert((nlpt)->source != NULL);
	g_source_set_name((nlpt)->source, name);
	g_source_set_callback(
	    (nlpt)->source,
	    (GSourceFunc)___nlpt_source_event_handler,
	    (gpointer)(nlpt),
	    (GDestroyNotify)___nlpt_source_destroy_notify
	    );
	g_source_attach((nlpt)->source, NULL);
	(nlpt)->status = (nlpt)->func(nlpt);
}

#endif
