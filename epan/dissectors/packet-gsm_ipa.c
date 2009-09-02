/* packet-gsm_ipa.c
 * Routines for packet dissection of ip.access GSM over IP
 * Copyright 2009 by Harald Welte <laforge@gnumonks.org>
 * Copyright 2009 by Holger Hans Peter Freyther <zecke@selfish.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib.h>

#include <epan/packet.h>

/* Initialize the protocol and registered fields */
static int proto_ipa = -1;
static int proto_ipaccess = -1;

static int hf_ipa_data_len = -1;
static int hf_ipa_protocol = -1;

static int hf_ipaccess_msgtype = -1;
static int hf_ipaccess_attr_tag = -1;
static int hf_ipaccess_attr_string = -1;

/* Initialize the subtree pointers */
static gint ett_ipa = -1;
static gint ett_ipaccess = -1;

enum {
	SUB_OML,
	SUB_RSL,
	SUB_SCCP,
/*	SUB_IPACCESS, */

	SUB_MAX
};

static dissector_handle_t sub_handles[SUB_MAX];

#define TCP_PORT_ABISIP_PRIM	 3002
#define TCP_PORT_ABISIP_SEC	 3003
#define TCP_PORT_ABISIP_INST	 3006
#define TCP_PORT_AIP_PRIM	 5000

#define ABISIP_RSL	0x00
#define AIP_SCCP	0xfd
#define ABISIP_IPACCESS	0xfe
#define ABISIP_OML	0xff

static const value_string ipa_protocol_vals[] = {
	{ 0x00,		"RSL" },
	{ 0xfd,		"SCCP" },
	{ 0xfe,		"IPA" },
	{ 0xff,		"OML" },
	{ 0, 		NULL }
};

static const value_string ipaccess_msgtype_vals[] = {
	{ 0x00,		"PING?" },
	{ 0x01, 	"PONG!" },
	{ 0x04, 	"IDENTITY REQUEST" },
	{ 0x05, 	"IDENTITY RESPONSE" },
	{ 0x06, 	"IDENTITY ACK" },
	{ 0x07, 	"IDENTITY NACK" },
	{ 0x08,		"PROXY REQUEST" },
	{ 0x09,		"PROXY ACK" },
	{ 0x0a,		"PROXY NACK" },
	{ 0,		NULL }
};

static const value_string ipaccess_idtag_vals[] = {
	{ 0x00,		"Serial Number" },
	{ 0x01,		"Unit Name" },
	{ 0x02,		"Location" },
	{ 0x03,		"Unit Type" },
	{ 0x04,		"Equipment Version" },
	{ 0x05,		"Software Version" },
	{ 0x06,		"IP Address" },
	{ 0x07,		"MAC Address" },
	{ 0x08,		"Unit ID" },
	{ 0,		NULL }
};

static gint
dissect_ipa_attr(tvbuff_t *tvb, int base_offs, proto_tree *tree)
{
	guint8 len, attr_type;

	int offset = base_offs;

	while (tvb_reported_length_remaining(tvb, offset) > 0) {
		attr_type = tvb_get_guint8(tvb, offset);

		switch (attr_type) {
		case 0x00:	/* a string prefixed by its length */
			len = tvb_get_guint8(tvb, offset+1);
			proto_tree_add_item(tree, hf_ipaccess_attr_tag,
					    tvb, offset+2, 1, FALSE);
			proto_tree_add_item(tree, hf_ipaccess_attr_string,
					    tvb, offset+3, len-1, FALSE);
			break;
		case 0x01:	/* a single-byte reqest for a certain attr */
			len = 0;
			proto_tree_add_item(tree, hf_ipaccess_attr_tag,
					    tvb, offset+1, 1, FALSE);
			break;
		default:
			len = 0;
			proto_tree_add_text(tree, tvb, offset+1, 1,
					    "unknown attribute type 0x%02x",
					    attr_type);
			break;
		};
		offset += len + 2;
	};
	return offset;
}

/* Dissect an ip.access specific message */
static gint
dissect_ipaccess(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
	proto_item *ti;
	proto_tree *ipaccess_tree;
	guint8 msg_type;

	msg_type = tvb_get_guint8(tvb, 0);

	col_append_fstr(pinfo->cinfo, COL_INFO, "%s ",
	                val_to_str(msg_type, ipaccess_msgtype_vals,
	                           "unknown 0x%02x"));
	if (tree) {
		ti = proto_tree_add_item(tree, proto_ipaccess, tvb, 0, -1, FALSE);
		ipaccess_tree = proto_item_add_subtree(ti, ett_ipaccess);
		proto_tree_add_item(ipaccess_tree, hf_ipaccess_msgtype,
				    tvb, 0, 1, FALSE);
		switch (msg_type) {
		case 4:
		case 5:
			dissect_ipa_attr(tvb, 1, ipaccess_tree);
			break;
		}
	}

	return 1;
}


/* Code to actually dissect the packets */
static void
dissect_ipa(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{

	int offset = 0;

	col_set_str(pinfo->cinfo, COL_PROTOCOL, "IPA");
	col_clear(pinfo->cinfo, COL_INFO);

	while (tvb_reported_length_remaining(tvb, offset) > 0) {
		proto_item *ti;
		proto_tree *ipa_tree;
		guint16 len, msg_type;
		tvbuff_t *next_tvb;

		len = tvb_get_ntohs(tvb, offset);
		msg_type = tvb_get_guint8(tvb, offset+2);

		col_append_fstr(pinfo->cinfo, COL_INFO, "%s ",
		                val_to_str(msg_type, ipa_protocol_vals,
		                           "unknown 0x%02x"));

		if (tree) {
			ti = proto_tree_add_protocol_format(tree, proto_ipa,
					tvb, offset, len+3,
					"IPA protocol ip.access, type: %s",
					val_to_str(msg_type, ipa_protocol_vals,
						   "unknown 0x%02x"));
			ipa_tree = proto_item_add_subtree(ti, ett_ipa);
			proto_tree_add_item(ipa_tree, hf_ipa_data_len,
					    tvb, offset+1, 1, FALSE);
			proto_tree_add_item(ipa_tree, hf_ipa_protocol,
					    tvb, offset+2, 1, FALSE);
		}

		next_tvb = tvb_new_subset(tvb, offset+3, len, len);

		switch (msg_type) {
		case ABISIP_RSL:
			/* hand this off to the standard A-bis RSL dissector */
			call_dissector(sub_handles[SUB_RSL], next_tvb, pinfo, tree);
			break;
		case ABISIP_OML:
			/* hand this off to the standard A-bis OML dissector */
			if (sub_handles[SUB_OML])
				call_dissector(sub_handles[SUB_OML], next_tvb,
						 pinfo, tree);
			break;
		case ABISIP_IPACCESS:
			dissect_ipaccess(next_tvb, pinfo, tree);
			break;
		case AIP_SCCP:
			/* hand this off to the standard SCCP dissector */
			call_dissector(sub_handles[SUB_SCCP], next_tvb, pinfo, tree);
			break;
		}
		offset += len + 3;
	}
}

void proto_register_ipa(void)
{
	static hf_register_info hf[] = {
		{&hf_ipa_data_len,
		 {"DataLen", "ipa.data_len",
		  FT_UINT8, BASE_DEC, NULL, 0x0,
		  "The length of the data (in bytes)", HFILL}
		 },
		{&hf_ipa_protocol,
		 {"Protocol", "ipa.protocol",
		  FT_UINT8, BASE_HEX, VALS(ipa_protocol_vals), 0x0,
		  "The IPA Sub-Protocol", HFILL}
		 },
	};
	static hf_register_info hf_ipa[] = {
		{&hf_ipaccess_msgtype,
		 {"MessageType", "ipaccess.msg_type",
		  FT_UINT8, BASE_HEX, VALS(ipaccess_msgtype_vals), 0x0,
		  "Type of ip.access messsage", HFILL}
		 },
		{&hf_ipaccess_attr_tag,
		 {"Tag", "ipaccess.attr_tag",
		  FT_UINT8, BASE_HEX, VALS(ipaccess_idtag_vals), 0x0,
		  "Attribute Tag", HFILL}
		 },
		{&hf_ipaccess_attr_string,
		 {"String", "ipaccess.attr_string",
		  FT_STRING, BASE_NONE, NULL, 0x0,
		  "String attribute", HFILL}
		 },
	};

	static gint *ett[] = {
		&ett_ipa,
		&ett_ipaccess,
	};

	proto_ipa =
	    proto_register_protocol("GSM over IP protocol as used by ip.access",
				    "GSM over IP", "gsm_ipa");
	proto_ipaccess =
	    proto_register_protocol("GSM over IP ip.access CCM sub-protocol",
				    "IPA", "ipaccess");

	proto_register_field_array(proto_ipa, hf, array_length(hf));
	proto_register_field_array(proto_ipaccess, hf_ipa, array_length(hf_ipa));
	proto_register_subtree_array(ett, array_length(ett));

	register_dissector("gsm_ipa", dissect_ipa, proto_ipa);
}

void proto_reg_handoff_gsm_ipa(void)
{
	dissector_handle_t ipa_handle;

	sub_handles[SUB_RSL] = find_dissector("gsm_abis_rsl");
	sub_handles[SUB_OML] = find_dissector("gsm_abis_oml");
	sub_handles[SUB_SCCP] = find_dissector("sccp");

	ipa_handle = create_dissector_handle(dissect_ipa, proto_ipa);
	dissector_add("tcp.port", TCP_PORT_ABISIP_PRIM, ipa_handle);
	dissector_add("tcp.port", TCP_PORT_ABISIP_SEC, ipa_handle);
	dissector_add("tcp.port", TCP_PORT_ABISIP_INST, ipa_handle);
	dissector_add("tcp.port", TCP_PORT_AIP_PRIM, ipa_handle);
	dissector_add("udp.port", TCP_PORT_ABISIP_INST, ipa_handle);
}
