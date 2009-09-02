/* epan.h
 *
 * $Id$
 *
 * Wireshark Protocol Analyzer Library
 *
 * Copyright (c) 2001 by Gerald Combs <gerald@wireshark.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef EPAN_H
#define EPAN_H

#include <glib.h>
#include "frame_data.h"
#include "column_info.h"
#include "register.h"

typedef struct _epan_dissect_t epan_dissect_t;

#include "dfilter/dfilter.h"

/* init the whole epan module, this is used to be called only once in a program */
void epan_init(void (*register_all_protocols_func)(register_cb cb, gpointer client_data),
	       void (*register_all_handoffs_func)(register_cb cb, gpointer client_data),
	       register_cb cb,
	       void *client_data,
	       void (*report_failure)(const char *, va_list),
	       void (*report_open_failure)(const char *, int, gboolean),
	       void (*report_read_failure)(const char *, int),
	       void (*report_write_failure)(const char *, int));

/* cleanup the whole epan module, this is used to be called only once in a program */
void epan_cleanup(void);

/*
 * Initialize the table of conversations.  Conversations are identified by
 * their endpoints; they are used for protocols such as IP, TCP, and UDP,
 * where packets contain endpoint information but don't contain a single
 * value indicating to which flow the packet belongs.
 */
void epan_conversation_init(void);

/*
 * Initialize the table of circuits.  Circuits are identified by a
 * circuit ID; they are used for protocols where packets *do* contain
 * a circuit ID value indicating to which flow the packet belongs.
 *
 * We might want to make a superclass for both endpoint-specified
 * conversations and circuit ID-specified circuits, so we can attach
 * information either to a circuit or a conversation with common
 * code.
 */
void epan_circuit_init(void);

/* A client will create one epan_t for an entire dissection session.
 * A single epan_t will be used to analyze the entire sequence of packets,
 * sequentially, in a single session. A session corresponds to a single
 * packet trace file. The reaons epan_t exists is that some packets in
 * some protocols cannot be decoded without knowledge of previous packets.
 * This inter-packet "state" is stored in the epan_t.
 */
/* XXX - NOTE: epan_t, epan_new and epan_free are currently unused! */
typedef struct epan_session epan_t;

epan_t*
epan_new(void);

void
epan_free(epan_t*);

extern gchar*
epan_get_version(void);

/* initialize an existing single packet dissection */
epan_dissect_t*
epan_dissect_init(epan_dissect_t	*edt, gboolean create_proto_tree, gboolean proto_tree_visible);

/* get a new single packet dissection */
/* should be freed using epan_dissect_free() after packet dissection completed */
epan_dissect_t*
epan_dissect_new(gboolean create_proto_tree, gboolean proto_tree_visible);

/* Indicate whether we should fake protocols or not */
void
epan_dissect_fake_protocols(epan_dissect_t *edt, gboolean fake_protocols);

/* run a single packet dissection */
void
epan_dissect_run(epan_dissect_t *edt, void* pseudo_header,
        const guint8* data, frame_data *fd, column_info *cinfo);

/* Prime a proto_tree using the fields/protocols used in a dfilter. */
void
epan_dissect_prime_dfilter(epan_dissect_t *edt, const dfilter_t *dfcode);

/* fill the dissect run output into the packet list columns */
void
epan_dissect_fill_in_columns(epan_dissect_t *edt, gboolean fill_fd_colums);

/* releases resources attached to the packet dissection. DOES NOT free the actual pointer */
void
epan_dissect_cleanup(epan_dissect_t* edt);

/* free a single packet dissection */
void
epan_dissect_free(epan_dissect_t* edt);

/* Sets custom column */
const gchar *
epan_custom_set(epan_dissect_t *edt, const gchar *field,
				gchar *result, gchar *expr, int size );

#endif /* EPAN_H */
