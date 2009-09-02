/* packet-nhrp.c
 * Routines for NBMA Next Hop Resolution Protocol
 * RFC 2332 plus Cisco extensions (documented where?)
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
 *
 * CIE decoding for extensions and Cisco 12.4T extensions
 * added by Timo Teras <timo.teras@iki.fi>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <glib.h>
#include <epan/packet.h>
#include <epan/addr_resolv.h>
#include <epan/expert.h>
#include <epan/etypes.h>
#include <epan/ipproto.h>
#include <epan/greproto.h>
#include <epan/nlpid.h>
#include <epan/oui.h>
#include <epan/sminmpec.h>
#include <epan/afn.h>
#include <epan/in_cksum.h>
#include <epan/iana_snap_pid.h>
#include <epan/dissectors/packet-llc.h>
#include "packet-nhrp.h"

/* forward reference */
void proto_register_nhrp(void);
void proto_reg_handoff_nhrp(void);
void dissect_nhrp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree);

static int proto_nhrp = -1;
static int hf_nhrp_hdr_afn = -1;
static int hf_nhrp_hdr_pro_type = -1;
static int hf_nhrp_hdr_pro_snap_oui = -1;
static int hf_nhrp_hdr_pro_snap_pid = -1;
static int hf_nhrp_hdr_hopcnt = -1;
static int hf_nhrp_hdr_pktsz = -1;
static int hf_nhrp_hdr_chksum = -1;
static int hf_nhrp_hdr_extoff = -1;
static int hf_nhrp_hdr_version = -1;
static int hf_nhrp_hdr_op_type = -1;
static int hf_nhrp_hdr_shtl = -1;
static int hf_nhrp_hdr_shtl_type = -1;
static int hf_nhrp_hdr_shtl_len = -1;
static int hf_nhrp_hdr_sstl = -1;
static int hf_nhrp_hdr_sstl_type = -1;
static int hf_nhrp_hdr_sstl_len = -1;

static int hf_nhrp_src_proto_len = -1;
static int hf_nhrp_dst_proto_len = -1;
static int hf_nhrp_flags = -1;
static int hf_nhrp_flag_Q = -1;
static int hf_nhrp_flag_N = -1;
static int hf_nhrp_flag_A = -1;
static int hf_nhrp_flag_D = -1;
static int hf_nhrp_flag_U1 = -1;
static int hf_nhrp_flag_U2 = -1;
static int hf_nhrp_flag_S = -1;
static int hf_nhrp_flag_NAT = -1;
static int hf_nhrp_src_nbma_addr = -1;
static int hf_nhrp_src_nbma_saddr = -1;
static int hf_nhrp_src_prot_addr = -1;
static int hf_nhrp_dst_prot_addr = -1;
static int hf_nhrp_request_id = -1;

static int hf_nhrp_code = -1;
static int hf_nhrp_prefix_len = -1;
static int hf_nhrp_unused = -1;
static int hf_nhrp_mtu = -1;
static int hf_nhrp_holding_time = -1;
static int hf_nhrp_cli_addr_tl = -1;
static int hf_nhrp_cli_addr_tl_type = -1;
static int hf_nhrp_cli_addr_tl_len = -1;
static int hf_nhrp_cli_saddr_tl = -1;
static int hf_nhrp_cli_saddr_tl_type = -1;
static int hf_nhrp_cli_saddr_tl_len = -1;
static int hf_nhrp_cli_prot_len = -1;
static int hf_nhrp_pref = -1;
static int hf_nhrp_client_nbma_addr = -1;
static int hf_nhrp_client_nbma_saddr = -1;
static int hf_nhrp_client_prot_addr = -1;
static int hf_nhrp_ext_C = -1;
static int hf_nhrp_ext_type = -1;
static int hf_nhrp_ext_len = -1;
static int hf_nhrp_ext_value = -1;
static int hf_nhrp_error_offset = -1;
static int hf_nhrp_error_packet = -1;

static gint ett_nhrp = -1;
static gint ett_nhrp_hdr = -1;
static gint ett_nhrp_hdr_shtl = -1;
static gint ett_nhrp_hdr_sstl = -1;
static gint ett_nhrp_mand = -1;
static gint ett_nhrp_ext = -1;
static gint ett_nhrp_mand_flag = -1;
static gint ett_nhrp_cie = -1;
static gint ett_nhrp_cie_cli_addr_tl = -1;
static gint ett_nhrp_cie_cli_saddr_tl = -1;
static gint ett_nhrp_indication = -1;

/* NHRP Packet Types */
#define NHRP_RESOLUTION_REQ		1
#define NHRP_RESOLUTION_REPLY	2
#define NHRP_REGISTRATION_REQ	3
#define NHRP_REGISTRATION_REPLY	4
#define NHRP_PURGE_REQ			5
#define NHRP_PURGE_REPLY		6
#define NHRP_ERROR_INDICATION	7
#define NHRP_TRAFFIC_INDICATION	8

/* NHRP Extension Types */
#define NHRP_EXT_NULL			0	/* End of Extension */
#define NHRP_EXT_RESP_ADDR		3	/* Responder Address Extension */
#define NHRP_EXT_FWD_RECORD		4	/* NHRP Forward Transit NHS Record Extension */
#define NHRP_EXT_REV_RECORD		5	/* NHRP Reverse Transit NHS Record Extension */
#define NHRP_EXT_AUTH			7	/* NHRP Authentication Extension */
#define NHRP_EXT_VENDOR_PRIV	8	/* NHRP Vendor Private Extension */
#define NHRP_EXT_NAT_ADDRESS	9	/* Cisco NAT Address Extension */

/* NHRP Error Codes */
#define NHRP_ERR_UNRECOGNIZED_EXT		0x0001
#define NHRP_ERR_NHRP_LOOP_DETECT		0x0003
#define NHRP_ERR_PROT_ADDR_UNREACHABLE	0x0006
#define NHRP_ERR_PROT_ERROR				0x0007
#define NHRP_ERR_SDU_SIZE_EXCEEDED		0x0008
#define NHRP_ERR_INV_EXT				0x0009
#define NHRP_ERR_INV_RESOLUTION_REPLY	0x000a
#define NHRP_ERR_AUTH_FAILURE			0x000b
#define NHRP_ERR_HOP_COUNT_EXCEEDED		0x000f

/* NHRP CIE codes */
#define NHRP_CODE_SUCCESS					0x00
#define NHRP_CODE_ADMIN_PROHIBITED       	0x04
#define NHRP_CODE_INSUFFICIENT_RESOURCES	0x05
#define NHRP_CODE_NO_BINDING_EXISTS 		0x0c
#define NHRP_CODE_NON_UNIQUE_BINDING      	0x0d
#define NHRP_CODE_ALREADY_REGISTERED		0x0e

/* NHRP Subnetwork layer address type/length */
#define NHRP_SHTL_TYPE_MASK	0x40
#define NHRP_SHTL_LEN_MASK	0x3F
#define NHRP_SHTL_TYPE(val)	(((val) & (NHRP_SHTL_TYPE_MASK)) >> 6)
#define NHRP_SHTL_LEN(val)	((val) & (NHRP_SHTL_LEN_MASK))

#define NHRP_SHTL_TYPE_NSAP	0
#define NHRP_SHTL_TYPE_E164	1

static const value_string nhrp_shtl_type_vals[] = {
	{ NHRP_SHTL_TYPE_NSAP, "NSAP format" },
	{ NHRP_SHTL_TYPE_E164, "Native E.164 format" },
	{ 0, NULL }
};

static const value_string nhrp_op_type_vals[] = {
	{ NHRP_RESOLUTION_REQ, 		"NHRP Resolution Request" },
	{ NHRP_RESOLUTION_REPLY, 	"NHRP Resolution Reply" },
	{ NHRP_REGISTRATION_REQ, 	"NHRP Registration Request" },
	{ NHRP_REGISTRATION_REPLY,	"NHRP Registration Reply" },
	{ NHRP_PURGE_REQ, 			"NHRP Purge Request" },
	{ NHRP_PURGE_REPLY, 		"NHRP Purge Reply" },
	{ NHRP_ERROR_INDICATION, 	"NHRP Error Indication" },
	{ NHRP_TRAFFIC_INDICATION,	"NHRP Traffic Indication" },
	{ 0, 						NULL }
};

static const value_string ext_type_vals[] = {
	{ NHRP_EXT_NULL,			"End of Extension" },
	{ NHRP_EXT_RESP_ADDR,		"Responder Address Extension" },
	{ NHRP_EXT_FWD_RECORD,		"Forward Transit NHS Record Extension" },
	{ NHRP_EXT_REV_RECORD,		"Reverse Transit NHS Record Extension" },
	{ NHRP_EXT_AUTH,			"NHRP Authentication Extension" },
	{ NHRP_EXT_VENDOR_PRIV,		"NHRP Vendor Private Extension" },
	{ NHRP_EXT_NAT_ADDRESS,		"Cisco NAT Address Extension" },
	{ 0,						NULL }
};

static const value_string nhrp_error_code_vals[] = {
	{ NHRP_ERR_UNRECOGNIZED_EXT, 		"Unrecognized Extension" },
	{ NHRP_ERR_NHRP_LOOP_DETECT, 		"NHRP Loop Detected" },
	{ NHRP_ERR_PROT_ADDR_UNREACHABLE, 	"Protocol Address Unreachable" },
	{ NHRP_ERR_PROT_ERROR, 				"Protocol Error" },
	{ NHRP_ERR_SDU_SIZE_EXCEEDED, 		"NHRP SDU Size Exceeded" },
	{ NHRP_ERR_INV_EXT, 				"Invalid Extension" },
	{ NHRP_ERR_INV_RESOLUTION_REPLY, 	"Invalid NHRP Resolution Reply Received" },
	{ NHRP_ERR_AUTH_FAILURE, 			"Authentication Failure" },
	{ NHRP_ERR_HOP_COUNT_EXCEEDED, 		"Hop Count Exceeded" },
	{ 0, 								NULL }
};

static const value_string nhrp_cie_code_vals[] = {
	{ NHRP_CODE_SUCCESS,				"Success" },
	{ NHRP_CODE_ADMIN_PROHIBITED, 		"Administratively Prohibited" },
	{ NHRP_CODE_INSUFFICIENT_RESOURCES, "Insufficient Resources" },
	{ NHRP_CODE_NO_BINDING_EXISTS, 		"No Interworking Layer Address to NBMA Address Binding Exists" },
	{ NHRP_CODE_NON_UNIQUE_BINDING, 	"Binding Exists But Is Not Unique" },
	{ NHRP_CODE_ALREADY_REGISTERED, 	"Unique Internetworking Layer Address Already Registered" },
	{ 0, 								NULL }
};

static dissector_table_t osinl_subdissector_table;
static dissector_table_t osinl_excl_subdissector_table;
static dissector_table_t ethertype_subdissector_table;

static dissector_handle_t data_handle;

typedef struct _e_nhrp {
	guint16	ar_afn;
	guint16	ar_pro_type;
	guint32 ar_pro_type_oui;
	guint16 ar_pro_type_pid;
	guint8	ar_hopCnt;
	guint16	ar_pktsz;
	guint16	ar_chksum;
	guint16	ar_extoff;
	guint8	ar_op_version;
	guint8	ar_op_type;
	guint8	ar_shtl;
	guint8	ar_sstl;
} e_nhrp_hdr;

static guint16 nhrp_checksum(const guint8 *ptr, int len)
{
	vec_t cksum_vec[1];

	cksum_vec[0].ptr = ptr;
	cksum_vec[0].len = len;
	return in_cksum(&cksum_vec[0], 1);
}

void dissect_nhrp_hdr(tvbuff_t *tvb,
					  packet_info *pinfo,
					  proto_tree *tree,
					  gint *pOffset,
					  gint *pMandLen,
					  gint *pExtLen,
					  oui_info_t **pOuiInfo,
					  e_nhrp_hdr *hdr)
{
	gint	offset = *pOffset;
	const gchar *pro_type_str;
	guint   total_len = tvb_reported_length(tvb);
	guint16	ipcsum, rx_chksum;
	
	proto_item *nhrp_tree_item = NULL;
	proto_tree *nhrp_tree = NULL;
	proto_item *shtl_tree_item = NULL;
	proto_tree *shtl_tree = NULL;
	proto_item *sstl_tree_item = NULL;
	proto_tree *sstl_tree = NULL;
	proto_item *ti;

	nhrp_tree_item = proto_tree_add_text(tree, tvb, offset, 20, "NHRP Fixed Header");
	nhrp_tree = proto_item_add_subtree(nhrp_tree_item, ett_nhrp_hdr);

	hdr->ar_pktsz = tvb_get_ntohs(tvb, 10);
	if (total_len > hdr->ar_pktsz) {
		total_len = hdr->ar_pktsz;
	}

	hdr->ar_afn = tvb_get_ntohs(tvb, offset);
	proto_tree_add_item(nhrp_tree, hf_nhrp_hdr_afn, tvb, offset, 2, FALSE);
	offset += 2;

	hdr->ar_pro_type = tvb_get_ntohs(tvb, offset);
	if (hdr->ar_pro_type <= 0xFF) {
		/* It's an NLPID */
		pro_type_str = val_to_str(hdr->ar_pro_type, nlpid_vals,
		    "Unknown NLPID");
	} else if (hdr->ar_pro_type <= 0x3FF) {
		/* Reserved for future use by the IETF */
		pro_type_str = "Reserved for future use by the IETF";
	} else if (hdr->ar_pro_type <= 0x04FF) {
		/* Allocated for use by the ATM Forum */
		pro_type_str = "Allocated for use by the ATM Forum";
	} else if (hdr->ar_pro_type <= 0x05FF) {
		/* Experimental/Local use */
		pro_type_str = "Experimental/Local use";
	} else {
		pro_type_str = val_to_str(hdr->ar_pro_type, etype_vals,
		    "Unknown Ethertype");
	}
	proto_tree_add_uint_format(nhrp_tree, hf_nhrp_hdr_pro_type, tvb, offset, 2,
	    hdr->ar_pro_type, "Protocol Type (short form): %s (0x%04x)",
	    pro_type_str, hdr->ar_pro_type);
	offset += 2;

	if (hdr->ar_pro_type == NLPID_SNAP) {
		/*
		 * The long form protocol type is a SNAP OUI and PID.
		 */
		hdr->ar_pro_type_oui = tvb_get_ntoh24(tvb, offset);
		proto_tree_add_uint(nhrp_tree, hf_nhrp_hdr_pro_snap_oui,
		    tvb, offset, 3, hdr->ar_pro_type_oui);
		offset += 3;

		hdr->ar_pro_type_pid = tvb_get_ntohs(tvb, offset);
		*pOuiInfo = get_snap_oui_info(hdr->ar_pro_type_oui);
		if (*pOuiInfo != NULL) {
			proto_tree_add_uint(nhrp_tree,
			    *(*pOuiInfo)->field_info->p_id,
			    tvb, offset, 2, hdr->ar_pro_type_pid);
		} else {
			proto_tree_add_uint(nhrp_tree, hf_nhrp_hdr_pro_snap_pid,
			    tvb, offset, 2, hdr->ar_pro_type_pid);
		}
	} else {
		/*
		 * XXX - we should check that this is zero, as RFC 2332
		 * says it should be zero.
		 */
		proto_tree_add_text(nhrp_tree, tvb, offset, 5,
						"Protocol Type (long form): %s",
						tvb_bytes_to_str(tvb, offset, 5));
		offset += 5;
	}

	proto_tree_add_item(nhrp_tree, hf_nhrp_hdr_hopcnt, tvb, offset, 1, FALSE);
	offset += 1;

	proto_tree_add_item(nhrp_tree, hf_nhrp_hdr_pktsz, tvb, offset, 2, FALSE);
	offset += 2;

	rx_chksum = tvb_get_ntohs(tvb, offset);
	if (tvb_bytes_exist(tvb, 0, total_len)) {
		ipcsum = nhrp_checksum(tvb_get_ptr(tvb, 0, total_len),
		    total_len);
		if (ipcsum == 0) {
			proto_tree_add_uint_format(nhrp_tree, hf_nhrp_hdr_chksum, tvb, offset, 2, rx_chksum,
			    "NHRP Packet checksum: 0x%04x [correct]", rx_chksum);
		} else {
			proto_tree_add_uint_format(nhrp_tree, hf_nhrp_hdr_chksum, tvb, offset, 2, rx_chksum,
			    "NHRP Packet checksum: 0x%04x [incorrect, should be 0x%04x]", rx_chksum,
			    in_cksum_shouldbe(rx_chksum, ipcsum));
		}
	} else {
		proto_tree_add_uint_format(nhrp_tree, hf_nhrp_hdr_chksum, tvb, offset, 2, rx_chksum,
		    "NHRP Packet checksum: 0x%04x [not all data available]", rx_chksum);
	}
	offset += 2;

	hdr->ar_extoff = tvb_get_ntohs(tvb, offset);
	ti = proto_tree_add_item(nhrp_tree, hf_nhrp_hdr_extoff, tvb, offset, 2, FALSE);
	if (hdr->ar_extoff != 0 && hdr->ar_extoff < 20) {
		expert_add_info_format(pinfo, ti, PI_MALFORMED, PI_ERROR,
		    "Extension offset is less than the fixed header length");
	}
	offset += 2;

	hdr->ar_op_version = tvb_get_guint8(tvb, offset);
	proto_tree_add_text(nhrp_tree, tvb, offset, 1, "Version : %u (%s)",
						hdr->ar_op_version,
						(hdr->ar_op_version == 1) ? "NHRP - rfc2332" : "Unknown");
	offset += 1;

	proto_tree_add_text(nhrp_tree, tvb, offset, 1, "NHRP Packet Type : (%s)", 
						val_to_str(hdr->ar_op_type, nhrp_op_type_vals, "Unknown (%u)"))	;
	offset += 1;

	hdr->ar_shtl = tvb_get_guint8(tvb, offset);
	shtl_tree_item = proto_tree_add_uint_format(nhrp_tree, hf_nhrp_hdr_shtl,
		tvb, offset, 1, hdr->ar_shtl, "Source Address Type/Len: %s/%u",
		val_to_str(NHRP_SHTL_TYPE(hdr->ar_shtl), nhrp_shtl_type_vals, "Unknown Type"),
		NHRP_SHTL_LEN(hdr->ar_shtl));
	shtl_tree = proto_item_add_subtree(shtl_tree_item, ett_nhrp_hdr_shtl);
	proto_tree_add_item(shtl_tree, hf_nhrp_hdr_shtl_type, tvb, offset, 1, FALSE);
	proto_tree_add_item(shtl_tree, hf_nhrp_hdr_shtl_len, tvb, offset, 1, FALSE);
	offset += 1;
	
	hdr->ar_sstl = tvb_get_guint8(tvb, offset);
	sstl_tree_item = proto_tree_add_uint_format(nhrp_tree, hf_nhrp_hdr_sstl,
		tvb, offset, 1, hdr->ar_sstl, "Source SubAddress Type/Len: %s/%u",
		val_to_str(NHRP_SHTL_TYPE(hdr->ar_sstl), nhrp_shtl_type_vals, "Unknown Type"),
		NHRP_SHTL_LEN(hdr->ar_sstl));
	sstl_tree = proto_item_add_subtree(sstl_tree_item, ett_nhrp_hdr_sstl);
	proto_tree_add_item(sstl_tree, hf_nhrp_hdr_sstl_type, tvb, offset, 1, FALSE);
	proto_tree_add_item(sstl_tree, hf_nhrp_hdr_sstl_len, tvb, offset, 1, FALSE);
	offset += 1;
	
	*pOffset = offset;
	if (hdr->ar_extoff != 0) {
		if (hdr->ar_extoff >= 20) {
			*pMandLen = hdr->ar_extoff - 20;
			*pExtLen = total_len - hdr->ar_extoff;
		} else {
			/* Error */
			*pMandLen = 0;
			*pExtLen = 0;
		}
	}
	else {
		if (total_len >= 20)
			*pMandLen = total_len - 20;
		else {
			/* "Can't happen" - we would have thrown an exception */
			*pMandLen = 0;
		}
		*pExtLen = 0;
	}
}

void dissect_cie_list(tvbuff_t *tvb,
					  proto_tree *tree,
					  gint offset,
					  gint cieEnd,
					  e_nhrp_hdr *hdr,
					  gint isReq)
{
	proto_item *cli_addr_tree_item = NULL;
	proto_tree *cli_addr_tree = NULL;
	proto_item *cli_saddr_tree_item = NULL;
	proto_tree *cli_saddr_tree = NULL;
	guint8 val;

	while ((offset + 12) <= cieEnd) {
		guint cli_addr_len = tvb_get_guint8(tvb, offset + 8);
		guint cli_saddr_len = tvb_get_guint8(tvb, offset + 9);
		guint cli_prot_len = tvb_get_guint8(tvb, offset + 10);
		guint cie_len = 12 + cli_addr_len + cli_saddr_len + cli_prot_len;
		proto_item *cie_tree_item = proto_tree_add_text(tree, tvb, offset, cie_len, "Client Information Entry");
		proto_tree *cie_tree = proto_item_add_subtree(cie_tree_item, ett_nhrp_cie);

		if (isReq) {
			proto_tree_add_item(cie_tree, hf_nhrp_code, tvb, offset, 1, FALSE);
		}
		else {
			guint8 code = tvb_get_guint8(tvb, offset);
			proto_tree_add_text(cie_tree, tvb, offset, 1, "Code: %s",
								val_to_str(code, nhrp_cie_code_vals, "Unknown (%u)"));
		}
		offset += 1;
		
		proto_tree_add_item(cie_tree, hf_nhrp_prefix_len, tvb, offset, 1, FALSE);
		offset += 1;

		proto_tree_add_item(cie_tree, hf_nhrp_unused, tvb, offset, 2, FALSE);
		offset += 2;

		proto_tree_add_item(cie_tree, hf_nhrp_mtu, tvb, offset, 2, FALSE);
		offset += 2;

		proto_tree_add_item(cie_tree, hf_nhrp_holding_time, tvb, offset, 2, FALSE);
		offset += 2;

		val = tvb_get_guint8(tvb, offset);
		cli_addr_tree_item = proto_tree_add_uint_format(cie_tree, 
			hf_nhrp_cli_addr_tl, tvb, offset, 1, val, 
			"Client Address Type/Len: %s/%u", 
			val_to_str(NHRP_SHTL_TYPE(val), nhrp_shtl_type_vals, "Unknown Type"),
			NHRP_SHTL_LEN(val));
		cli_addr_tree = proto_item_add_subtree(cli_addr_tree_item, ett_nhrp_cie_cli_addr_tl);
		proto_tree_add_item(cli_addr_tree, hf_nhrp_cli_addr_tl_type, tvb, offset, 1, FALSE);
		proto_tree_add_item(cli_addr_tree, hf_nhrp_cli_addr_tl_len, tvb, offset, 1, FALSE);
		offset += 1;

		val = tvb_get_guint8(tvb, offset);
		cli_saddr_tree_item = proto_tree_add_uint_format(cie_tree, 
			hf_nhrp_cli_saddr_tl, tvb, offset, 1, val, 
			"Client Sub Address Type/Len: %s/%u", 
			val_to_str(NHRP_SHTL_TYPE(val), nhrp_shtl_type_vals, "Unknown Type"),
			NHRP_SHTL_LEN(val));
		cli_saddr_tree = proto_item_add_subtree(cli_saddr_tree_item, ett_nhrp_cie_cli_saddr_tl);
		proto_tree_add_item(cli_saddr_tree, hf_nhrp_cli_saddr_tl_type, tvb, offset, 1, FALSE);
		proto_tree_add_item(cli_saddr_tree, hf_nhrp_cli_saddr_tl_len, tvb, offset, 1, FALSE);
		offset += 1;

		proto_tree_add_item(cie_tree, hf_nhrp_cli_prot_len, tvb, offset, 1, FALSE);
		offset += 1;

		proto_tree_add_item(cie_tree, hf_nhrp_pref, tvb, offset, 1, FALSE);
		offset += 1;

		if (cli_addr_len) {
			switch (hdr->ar_afn) {

			case AFNUM_INET:
				if (cli_addr_len == 4)
					proto_tree_add_item(cie_tree, hf_nhrp_client_nbma_addr, tvb, offset, 4, FALSE);
				else {
					proto_tree_add_text(cie_tree, tvb, offset, cli_addr_len,
					    "Client NBMA Address: %s",
					    tvb_bytes_to_str(tvb, offset, cli_addr_len));
				}
				break;

			default:
				proto_tree_add_text(cie_tree, tvb, offset, cli_addr_len,
				    "Client NBMA Address: %s",
				    tvb_bytes_to_str(tvb, offset, cli_addr_len));
				break;
			}
			offset += cli_addr_len;
		}
		
		if (cli_saddr_len) {
			proto_tree_add_text(cie_tree, tvb, offset, cli_saddr_len,
								"Client NBMA Sub Address: %s",
								tvb_bytes_to_str(tvb, offset, cli_saddr_len));
		}

		if (cli_prot_len) {
			if (cli_prot_len == 4)
				proto_tree_add_ipv4(cie_tree, hf_nhrp_client_prot_addr, tvb, offset, 4, FALSE);
			else {
				proto_tree_add_text(cie_tree, tvb, offset, cli_prot_len,
									"Client Protocol Address: %s",
									tvb_bytes_to_str(tvb, offset, cli_prot_len));
			}
			offset += cli_prot_len;
		}
	}
}

void dissect_nhrp_mand(tvbuff_t *tvb,
					   packet_info *pinfo,
					   proto_tree *tree,
					   gint *pOffset,
					   gint mandLen,
					   oui_info_t *oui_info,
					   e_nhrp_hdr *hdr)
{
	gint	offset = *pOffset;
	gint	mandEnd = offset + mandLen;
	guint8	ssl, shl;
	guint16	flags;
	guint	srcLen, dstLen;
	gboolean isReq = 0;
	gboolean isErr = 0;
	gboolean isInd = 0;
	
	proto_item *nhrp_tree_item = NULL;
	proto_item *flag_item = NULL;
	proto_tree *nhrp_tree = NULL;
	proto_tree *flag_tree = NULL;

	tvb_ensure_bytes_exist(tvb, offset, mandLen);

	switch (hdr->ar_op_type)
	{
	case NHRP_RESOLUTION_REQ:
	case NHRP_REGISTRATION_REQ:
	case NHRP_PURGE_REQ:
		isReq = 1;
		break;
	case NHRP_ERROR_INDICATION:	/* This needs special treatment */
		isErr = 1;
		isInd = 1;
		break;
	case NHRP_TRAFFIC_INDICATION:
		isInd = 1;
		break;
	}
	nhrp_tree_item = proto_tree_add_text(tree, tvb, offset, mandLen, "NHRP Mandatory Part");
	nhrp_tree = proto_item_add_subtree(nhrp_tree_item, ett_nhrp_mand);

	srcLen = tvb_get_guint8(tvb, offset);
	proto_tree_add_item(nhrp_tree, hf_nhrp_src_proto_len, tvb, offset, 1, FALSE);
	offset += 1;
	
	dstLen = tvb_get_guint8(tvb, offset);
	proto_tree_add_item(nhrp_tree, hf_nhrp_dst_proto_len, tvb, offset, 1, FALSE);
	offset += 1;

	if (!isInd) {
		flags = tvb_get_ntohs(tvb, offset);
		flag_item = proto_tree_add_uint(nhrp_tree, hf_nhrp_flags, tvb, offset, 2, flags);
		flag_tree = proto_item_add_subtree(flag_item, ett_nhrp_mand_flag);

		switch (hdr->ar_op_type)
		{
		case NHRP_RESOLUTION_REQ:
		case NHRP_RESOLUTION_REPLY:
			proto_tree_add_boolean(flag_tree, hf_nhrp_flag_Q, tvb, offset, 2, flags);
			proto_tree_add_boolean(flag_tree, hf_nhrp_flag_A, tvb, offset, 2, flags);
			proto_tree_add_boolean(flag_tree, hf_nhrp_flag_D, tvb, offset, 2, flags);
			proto_tree_add_boolean(flag_tree, hf_nhrp_flag_U1, tvb, offset, 2, flags);
			proto_tree_add_boolean(flag_tree, hf_nhrp_flag_S, tvb, offset, 2, flags);
			break;
		case NHRP_REGISTRATION_REQ:
		case NHRP_REGISTRATION_REPLY:
			proto_tree_add_boolean(flag_tree, hf_nhrp_flag_U2, tvb, offset, 2, flags);
			break;
			
		case NHRP_PURGE_REQ:
		case NHRP_PURGE_REPLY:
			proto_tree_add_boolean(flag_tree, hf_nhrp_flag_N, tvb, offset, 2, flags);
			break;
		}
		proto_tree_add_boolean(flag_tree, hf_nhrp_flag_NAT, tvb, offset, 2, flags);

		offset += 2;

		proto_tree_add_item(nhrp_tree, hf_nhrp_request_id, tvb, offset, 4, FALSE);
		offset += 4;
	}
	else if (isErr) {
		guint16 err_code;
		offset += 2;
		
		err_code = tvb_get_ntohs(tvb, offset);
		proto_tree_add_text(tree, tvb, offset, 2, "Error Code: %s",
							val_to_str(err_code, nhrp_error_code_vals, "Unknown (%u)"));
		offset += 2;
		
		proto_tree_add_item(nhrp_tree, hf_nhrp_error_offset, tvb, offset, 2, FALSE);
		offset += 2;
	}
	else {
		offset += 6;
	}

	shl = NHRP_SHTL_LEN(hdr->ar_shtl);
	if (shl) {
		switch (hdr->ar_afn) {

		case AFNUM_INET:
			if (shl == 4)
				proto_tree_add_item(nhrp_tree, hf_nhrp_src_nbma_addr, tvb, offset, 4, FALSE);
			else {
				proto_tree_add_text(nhrp_tree, tvb, offset, shl,
				    "Source NBMA Address: %s",
				    tvb_bytes_to_str(tvb, offset, shl));
			}
			break;

		default:
			proto_tree_add_text(nhrp_tree, tvb, offset, shl,
			    "Source NBMA Address: %s",
			    tvb_bytes_to_str(tvb, offset, shl));
			break;
		}
		offset += shl;
	}
	
	ssl = NHRP_SHTL_LEN(hdr->ar_sstl);
	if (ssl) {
		proto_tree_add_text(nhrp_tree, tvb, offset, ssl,
							"Source NBMA Sub Address: %s",
							tvb_bytes_to_str(tvb, offset, ssl));
		offset += ssl;
	}

	if (srcLen) {
		if (srcLen == 4)
			proto_tree_add_item(nhrp_tree, hf_nhrp_src_prot_addr, tvb, offset, 4, FALSE);
		else {
			proto_tree_add_text(nhrp_tree, tvb, offset, srcLen,
								"Source Protocol Address: %s",
								tvb_bytes_to_str(tvb, offset, srcLen));
		}
		offset += srcLen;
	}

	if (dstLen) {
		if (dstLen == 4)
			proto_tree_add_item(nhrp_tree, hf_nhrp_dst_prot_addr, tvb, offset, 4, FALSE);
		else {
			proto_tree_add_text(nhrp_tree, tvb, offset, dstLen,
								"Destination Protocol Address: %s",
								tvb_bytes_to_str(tvb, offset, dstLen));
		}
		offset += dstLen;
	}

	if (isInd) {
		gboolean save_in_error_pkt;
		gint pkt_len = mandEnd - offset;
		proto_item *ind_tree_item = proto_tree_add_text(tree, tvb, offset, pkt_len, "Packet Causing Indication");
		proto_tree *ind_tree = proto_item_add_subtree(ind_tree_item, ett_nhrp_indication);
		gboolean dissected;
		tvbuff_t *sub_tvb;

		save_in_error_pkt = pinfo->in_error_pkt;
		pinfo->in_error_pkt = TRUE;
		sub_tvb = tvb_new_subset_remaining(tvb, offset);
		if (isErr) {
			dissect_nhrp(sub_tvb, pinfo, ind_tree);
		}
		else {
			if (hdr->ar_pro_type <= 0xFF) {
				/* It's an NLPID */
				if (hdr->ar_pro_type == NLPID_SNAP) {
					/*
					 * Dissect based on the SNAP OUI
					 * and PID.
					 */
					if (hdr->ar_pro_type_oui == 0x000000) {
						/*
						 * "Should not happen", as
						 * the protocol type should
						 * be the Ethertype, but....
						 */
						dissected = dissector_try_port(
						    ethertype_subdissector_table,
						    hdr->ar_pro_type_pid,
						    sub_tvb, pinfo, ind_tree);
					} else {
						/*
						 * If we have a dissector
						 * table, use it, otherwise
						 * just dissect as data.
						 */
						if (oui_info != NULL) {
							dissected = dissector_try_port(
							    oui_info->table,
							    hdr->ar_pro_type_pid,
							    sub_tvb, pinfo,
							    ind_tree);
						} else
							dissected = FALSE;
					}
				} else {
					/*
					 * Dissect based on the NLPID.
					 */
					dissected = dissector_try_port(
					    osinl_subdissector_table,
					    hdr->ar_pro_type, sub_tvb, pinfo,
					    ind_tree) ||
					            dissector_try_port(
					    osinl_excl_subdissector_table,
					    hdr->ar_pro_type, sub_tvb, pinfo,
					    ind_tree);
				}
			} else if (hdr->ar_pro_type <= 0x3FF) {
				/* Reserved for future use by the IETF */
				dissected = FALSE;
			} else if (hdr->ar_pro_type <= 0x04FF) {
				/* Allocated for use by the ATM Forum */
				dissected = FALSE;
			} else if (hdr->ar_pro_type <= 0x05FF) {
				/* Experimental/Local use */
				dissected = FALSE;
			} else {
				dissected = dissector_try_port(
				    ethertype_subdissector_table,
				    hdr->ar_pro_type, sub_tvb, pinfo, ind_tree);
			}
			if (!dissected) {
				call_dissector(data_handle, sub_tvb, pinfo,
				    ind_tree);
			}
		}
		pinfo->in_error_pkt = save_in_error_pkt;
		offset = mandEnd;
	}

	dissect_cie_list(tvb, nhrp_tree, offset, mandEnd, hdr, isReq);

	*pOffset = mandEnd;
}

/* TBD : Decode Authentication Extension and Vendor Specific Extension */
void dissect_nhrp_ext(tvbuff_t *tvb,
					  proto_tree *tree,
					  gint *pOffset,
					  gint extLen,
					  e_nhrp_hdr *hdr)
{
	gint	offset = *pOffset;
	gint	extEnd = offset + extLen;
	
	proto_item *nhrp_tree_item = NULL;
	proto_tree *nhrp_tree = NULL;
	
	tvb_ensure_bytes_exist(tvb, offset, extLen);

	while ((offset + 4) <= extEnd)
	{
		gint extTypeC = tvb_get_ntohs(tvb, offset);
		gint extType = extTypeC & 0x3FFF;
		gint len  = tvb_get_ntohs(tvb, offset+2);
		nhrp_tree_item =  proto_tree_add_text(tree, tvb, offset,
		    len + 4, "%s",
		    val_to_str(extType, ext_type_vals, "Unknown (%u)"));
		nhrp_tree = proto_item_add_subtree(nhrp_tree_item, ett_nhrp_ext);
		proto_tree_add_boolean(nhrp_tree, hf_nhrp_ext_C, tvb, offset, 2, extTypeC);
		proto_tree_add_item(nhrp_tree, hf_nhrp_ext_type, tvb, offset, 2, FALSE);
		offset += 2;
		
		proto_tree_add_item(nhrp_tree, hf_nhrp_ext_len, tvb, offset, 2, FALSE);
		offset += 2;

		if (len) {
			tvb_ensure_bytes_exist(tvb, offset, len);
			switch (extType) {
			case NHRP_EXT_RESP_ADDR:
			case NHRP_EXT_FWD_RECORD:
			case NHRP_EXT_REV_RECORD:
			case NHRP_EXT_NAT_ADDRESS:
				dissect_cie_list(tvb, nhrp_tree,
				    offset, offset + len, hdr, 0);
				break;
			default:
				proto_tree_add_text(nhrp_tree, tvb, offset, len,
				    "Extension Value: %s",
				    tvb_bytes_to_str(tvb, offset, len));
				break;
			}
			offset += len;
		}
	}	

	*pOffset = extEnd;
}

void dissect_nhrp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
	e_nhrp_hdr hdr;
	gint mandLen = 0;
	gint extLen = 0;
	gint offset = 0;
	proto_item *ti = NULL;
	proto_tree *nhrp_tree = NULL;
	oui_info_t *oui_info;
		
	col_set_str(pinfo->cinfo, COL_PROTOCOL, "NHRP");
	col_clear(pinfo->cinfo, COL_INFO);
	
	memset(&hdr, 0, sizeof(e_nhrp_hdr));
	
	hdr.ar_op_type = tvb_get_guint8(tvb, 17);
	
	if (check_col(pinfo->cinfo, COL_INFO)) {
		col_add_str(pinfo->cinfo, COL_INFO,
		    val_to_str(hdr.ar_op_type, nhrp_op_type_vals,
			       "0x%02X - unknown"));
	}
	col_set_writable(pinfo->cinfo, FALSE);

	ti = proto_tree_add_protocol_format(tree, proto_nhrp, tvb, 0, -1,
	    "Next Hop Resolution Protocol (%s)",
	    val_to_str(hdr.ar_op_type, nhrp_op_type_vals, "0x%02X - unknown"));
	nhrp_tree = proto_item_add_subtree(ti, ett_nhrp);
		
	dissect_nhrp_hdr(tvb, pinfo, nhrp_tree, &offset, &mandLen, &extLen,
	    &oui_info, &hdr);
	if (mandLen) {
		dissect_nhrp_mand(tvb, pinfo, nhrp_tree, &offset, mandLen,
		    oui_info, &hdr);
	}

	if (extLen) {
		dissect_nhrp_ext(tvb, nhrp_tree, &offset, extLen, &hdr);
	}
}

void
proto_register_nhrp(void)
{
	static hf_register_info hf[] = {
		
		{ &hf_nhrp_hdr_afn,
		  { "Address Family Number", 		"nhrp.hdr.afn", 	FT_UINT16, BASE_HEX_DEC, VALS(afn_vals), 0x0, NULL, HFILL }},
		{ &hf_nhrp_hdr_pro_type,
		  { "Protocol Type (short form)",	"nhrp.hdr.pro.type",FT_UINT16, BASE_HEX_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_hdr_pro_snap_oui,
		  { "Protocol Type (long form) - OUI",	"nhrp.hdr.pro.snap.oui",FT_UINT24, BASE_HEX, VALS(oui_vals), 0x0, NULL, HFILL }},
		{ &hf_nhrp_hdr_pro_snap_pid,
		  { "Protocol Type (long form) - PID",	"nhrp.hdr.pro.snap.pid",FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_hdr_hopcnt,
		  { "Hop Count", 					"nhrp.hdr.hopcnt", 	FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_hdr_pktsz,
		  { "Packet Length", 				"nhrp.hdr.pktsz", 	FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_hdr_chksum,
		  { "Packet Checksum", 				"nhrp.hdr.chksum", 	FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_hdr_extoff,
		  { "Extension Offset", 			"nhrp.hdr.extoff", 	FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_hdr_version,
		  { "Version", 						"nhrp.hdr.version", FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_hdr_op_type,
		  { "NHRP Packet Type", 			"nhrp.hdr.op.type", FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_hdr_shtl,
		  { "Source Address Type/Len", 		"nhrp.hdr.shtl", 	FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_hdr_shtl_type,
		  { "Type", 						"nhrp.hdr.shtl.type", FT_UINT8, BASE_DEC, VALS(nhrp_shtl_type_vals), NHRP_SHTL_TYPE_MASK, NULL, HFILL }},
		{ &hf_nhrp_hdr_shtl_len,
		  { "Length", 						"nhrp.hdr.shtl.len", FT_UINT8, BASE_DEC, NULL, NHRP_SHTL_LEN_MASK, NULL, HFILL }},
		{ &hf_nhrp_hdr_sstl,
		  { "Source SubAddress Type/Len", 	"nhrp.hdr.sstl", 	FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_hdr_sstl_type,
		  { "Type", 						"nhrp.hdr.sstl.type", FT_UINT8, BASE_DEC, VALS(nhrp_shtl_type_vals), NHRP_SHTL_TYPE_MASK, NULL, HFILL }},
		{ &hf_nhrp_hdr_sstl_len,
		  { "Length", 						"nhrp.hdr.sstl.len", FT_UINT8, BASE_DEC, NULL, NHRP_SHTL_LEN_MASK, NULL, HFILL }},

		{ &hf_nhrp_src_proto_len,
		  { "Source Protocol Len", 			"nhrp.src.prot.len",FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_dst_proto_len,
		  { "Destination Protocol Len", 	"nhrp.dst.prot.len",FT_UINT16, BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_flags,
		  { "Flags",						"nhrp.flags", 		FT_UINT16, BASE_HEX, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_flag_Q,
		  { "Is Router", 					"nhrp.flag.q", 		FT_BOOLEAN, 16, NULL, 0x8000, NULL, HFILL }},
		{ &hf_nhrp_flag_N,
		  { "Expected Purge Reply",			"nhrp.flag.n", 		FT_BOOLEAN, 16, NULL, 0x8000, NULL, HFILL }},
		{ &hf_nhrp_flag_A,
		  { "Authoritative", 				"nhrp.flag.a", 		FT_BOOLEAN, 16, NULL, 0x4000, "A bit", HFILL }},
		{ &hf_nhrp_flag_D,
		  { "Stable Association", 			"nhrp.flag.d", 		FT_BOOLEAN, 16, NULL, 0x2000, "D bit", HFILL }},
		{ &hf_nhrp_flag_U1,
		  { "Uniqueness Bit", 				"nhrp.flag.u1", 	FT_BOOLEAN, 16, NULL, 0x1000, "U bit", HFILL }},
		{ &hf_nhrp_flag_U2,
		  { "Uniqueness Bit", 				"nhrp.flag.u1", 	FT_BOOLEAN, 16, NULL, 0x8000, "U bit", HFILL }},
		{ &hf_nhrp_flag_S,
		  { "Stable Binding", 				"nhrp.flag.s", 		FT_BOOLEAN, 16, NULL, 0x0800, "S bit", HFILL }},
		{ &hf_nhrp_flag_NAT,
		  { "Cisco NAT Supported",			"nhrp.flag.nat",	FT_BOOLEAN, 16, NULL, 0x0002, "NAT bit", HFILL }},
		{ &hf_nhrp_request_id,
		  { "Request ID", 					"nhrp.reqid",		FT_UINT32,	BASE_HEX_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_src_nbma_addr,
		  { "Source NBMA Address",			"nhrp.src.nbma.addr",FT_IPv4, 	BASE_NONE, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_src_nbma_saddr,
		  { "Source NBMA Sub Address",		"nhrp.src.nbma.saddr",FT_UINT_BYTES,BASE_NONE, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_src_prot_addr,
		  { "Source Protocol Address",		"nhrp.src.prot.addr",FT_IPv4, 	BASE_NONE, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_dst_prot_addr,
		  { "Destination Protocol Address",	"nhrp.dst.prot.addr",FT_IPv4, 	BASE_NONE, NULL, 0x0, NULL, HFILL }},

		{ &hf_nhrp_code,
		  { "Code", 					"nhrp.code", 			FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_prefix_len,
		  { "Prefix Length", 			"nhrp.prefix", 			FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_unused,
		  { "Unused", 					"nhrp.unused", 			FT_UINT16,BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_mtu,
		  { "Max Transmission Unit",	"nhrp.mtu", 			FT_UINT16,BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_holding_time,
		  { "Holding Time (s)",			"nhrp.htime", 			FT_UINT16,BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_cli_addr_tl,
		  { "Client Address Type/Len",	"nhrp.cli.addr_tl",		FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_cli_addr_tl_type,
		  { "Type", 				   	"nhrp.cli.addr_tl.type",	FT_UINT8, BASE_DEC, VALS(nhrp_shtl_type_vals), NHRP_SHTL_TYPE_MASK, NULL, HFILL }},
		{ &hf_nhrp_cli_addr_tl_len,
		  { "Length", 				   	"nhrp.cli.addr_tl.len",	FT_UINT8, BASE_DEC, NULL, NHRP_SHTL_LEN_MASK, NULL, HFILL }},
		{ &hf_nhrp_cli_saddr_tl,
		  { "Client Sub Address Type/Len","nhrp.cli.saddr_tl", 	FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_cli_saddr_tl_type,
		  { "Type", 				   	"nhrp.cli.saddr_tl.type",	FT_UINT8, BASE_DEC, VALS(nhrp_shtl_type_vals), NHRP_SHTL_TYPE_MASK, NULL, HFILL }},
		{ &hf_nhrp_cli_saddr_tl_len,
		  { "Length", 				   	"nhrp.cli.saddr_tl.len",	FT_UINT8, BASE_DEC, NULL, NHRP_SHTL_LEN_MASK, NULL, HFILL }},
		{ &hf_nhrp_cli_prot_len,
		  { "Client Protocol Length", 	"nhrp.prot.len", 		FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_pref,
		  { "CIE Preference Value",		"nhrp.pref", 			FT_UINT8, BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_client_nbma_addr,
		  { "Client NBMA Address",		"nhrp.client.nbma.addr", FT_IPv4, 		BASE_NONE, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_client_nbma_saddr,
		  { "Client NBMA Sub Address",	"nhrp.client.nbma.saddr",FT_UINT_BYTES,	BASE_NONE,  NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_client_prot_addr,
		  { "Client Protocol Address",	"nhrp.client.prot.addr", FT_IPv4, 		BASE_NONE, NULL, 0x0, NULL, HFILL }},

		{ &hf_nhrp_ext_C,
		  { "Compulsory Flag",			"nhrp.ext.c", 			FT_BOOLEAN, 16, NULL, 0x8000, NULL, HFILL }},
		{ &hf_nhrp_ext_type,
		  { "Extension Type",			"nhrp.ext.type", 		FT_UINT16,	BASE_HEX, NULL, 0x3FFF, NULL, HFILL }},
		{ &hf_nhrp_ext_len,
		  { "Extension length", 		"nhrp.ext.len",			FT_UINT16,	BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_ext_value,
		  { "Extension Value", 			"nhrp.ext.val",			FT_UINT_BYTES,BASE_NONE, NULL, 0x0, NULL, HFILL }},

		{ &hf_nhrp_error_offset,
		  { "Error Offset", 			"nhrp.err.offset",		FT_UINT16,	BASE_DEC, NULL, 0x0, NULL, HFILL }},
		{ &hf_nhrp_error_packet,
		  { "Errored Packet", 			"nhrp.err.pkt",			FT_UINT_BYTES,BASE_NONE, NULL, 0x0, NULL, HFILL }},
	};
	
	static gint *ett[] = {
		&ett_nhrp,
		&ett_nhrp_hdr,
		&ett_nhrp_hdr_shtl,
		&ett_nhrp_hdr_sstl,
		&ett_nhrp_mand,
		&ett_nhrp_ext,
		&ett_nhrp_mand_flag,
		&ett_nhrp_cie,
		&ett_nhrp_cie_cli_addr_tl,
		&ett_nhrp_cie_cli_saddr_tl,
		&ett_nhrp_indication
	};
	
	proto_nhrp = proto_register_protocol(
		"NBMA Next Hop Resolution Protocol",
		"NHRP",
		"nhrp");
	proto_register_field_array(proto_nhrp, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));
}

void
proto_reg_handoff_nhrp(void)
{
	dissector_handle_t nhrp_handle;

	data_handle = find_dissector("data");

	osinl_subdissector_table = find_dissector_table("osinl");
	osinl_excl_subdissector_table = find_dissector_table("osinl.excl");
	ethertype_subdissector_table = find_dissector_table("ethertype");

	nhrp_handle = create_dissector_handle(dissect_nhrp, proto_nhrp);
	dissector_add("ip.proto", IP_PROTO_NARP, nhrp_handle);
	dissector_add("gre.proto", GRE_NHRP, nhrp_handle);
	dissector_add("llc.iana_pid", IANA_PID_MARS_NHRP_CONTROL, nhrp_handle);
}
