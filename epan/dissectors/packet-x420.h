/* Do not modify this file.                                                   */
/* It is created automatically by the ASN.1 to Wireshark dissector compiler   */
/* packet-x420.h                                                              */
/* ../../tools/asn2wrs.py -b -e -p x420 -c ./x420.cnf -s ./packet-x420-template -D . IPMSInformationObjects.asn IPMSHeadingExtensions.asn IPMSExtendedBodyPartTypes2.asn IPMSFileTransferBodyPartType.asn IPMSExtendedVoiceBodyPartType.asn IPMSForwardedContentBodyPartType.asn IPMSMessageStoreAttributes.asn IPMSSecurityExtensions.asn IPMSObjectIdentifiers.asn */

/* Input file: packet-x420-template.h */

#line 1 "packet-x420-template.h"
/* packet-x420.h
 * Routines for X.420 (X.400 Message Transfer) packet dissection
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

#ifndef PACKET_X420_H
#define PACKET_X420_H


/*--- Included file: packet-x420-exp.h ---*/
#line 1 "packet-x420-exp.h"
extern const value_string x420_InformationObject_vals[];
extern const value_string x420_NonReceiptReasonField_vals[];
extern const value_string x420_DiscardReasonField_vals[];
int dissect_x420_InformationObject(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_);
int dissect_x420_IPM(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_);
int dissect_x420_ORDescriptor(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_);
int dissect_x420_ExtensionsField(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_);
int dissect_x420_MessageParameters(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_);
int dissect_x420_IPN(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_);
int dissect_x420_NonReceiptReasonField(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_);
int dissect_x420_DiscardReasonField(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_);

/*--- End of included file: packet-x420-exp.h ---*/
#line 30 "packet-x420-template.h"

#endif  /* PACKET_X420_H */
