/* packet-dis-pdus.h
 * Declarations for DIS PDU parsing.
 * Copyright 2005, Scientific Research Corporation
 * Initial implementation by Jeremy Ouellette <jouellet@scires.com>
 *
 * $Id$
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
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

#ifndef __PACKET_DIS_PDUPARSERS_H__
#define __PACKET_DIS_PDUPARSERS_H__

#include "packet-dis-fields.h"

extern DIS_ParserNode DIS_PARSER_ENTITY_STATE_PDU[];

extern DIS_ParserNode DIS_PARSER_FIRE_PDU[];

extern DIS_ParserNode DIS_PARSER_DETONATION_PDU[];

extern DIS_ParserNode DIS_PARSER_DATA_PDU[];

extern DIS_ParserNode DIS_PARSER_DATA_QUERY_PDU[];

/* Limits of integral types. */
#ifndef INT8_MIN
#define INT8_MIN               (-128)
#endif
#ifndef INT16_MIN
#define INT16_MIN              (-32767-1)
#endif
#ifndef INT32_MIN
#define INT32_MIN              (-2147483647-1)
#endif
#ifndef INT8_MAX
#define INT8_MAX               (127)
#endif
#ifndef INT16_MAX
#define INT16_MAX              (32767)
#endif
#ifndef INT32_MAX
#define INT32_MAX              (2147483647)
#endif
#ifndef UINT8_MAX
#define UINT8_MAX              (255U)
#endif
#ifndef UINT16_MAX
#define UINT16_MAX             (65535U)
#endif
#ifndef UINT32_MAX
#define UINT32_MAX             (4294967295U)
#endif

DIS_ParserNode *createSubtree(DIS_ParserNode parserNodes[], gint *ettVar);

void initializeParser(DIS_ParserNode parserNodes[]);

void initializeParsers(void);

gint parseFields(tvbuff_t *tvb, proto_tree *tree, gint offset, DIS_ParserNode parserNodes[]);

extern guint32 numArticulations;
extern int ettArticulations[];

#define DIS_PDU_MAX_ARTICULATIONS 16

#endif /* packet-dis-pduparsers.h */
