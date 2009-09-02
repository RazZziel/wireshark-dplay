/* packet-x411.h
 * Routines forX.411 (X.400 Message Transfer) packet dissection
 * Graeme Lunt 2005
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

#ifndef PACKET_X411_H
#define PACKET_X411_H

char* x411_get_last_oraddress(void);
void dissect_x411_mts_apdu (tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree);
#include "packet-x411-exp.h"

#endif  /* PACKET_X411_H */
