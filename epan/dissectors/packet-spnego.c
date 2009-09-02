/* Do not modify this file.                                                   */
/* It is created automatically by the ASN.1 to Wireshark dissector compiler   */
/* packet-spnego.c                                                            */
/* ../../tools/asn2wrs.py -b -p spnego -c spnego.cnf -s packet-spnego-template spnego.asn */

/* Input file: packet-spnego-template.c */

#line 1 "packet-spnego-template.c"
/* packet-spnego.c
 * Routines for the simple and protected GSS-API negotiation mechanism
 * as described in RFC 2478.
 * Copyright 2002, Tim Potter <tpot@samba.org>
 * Copyright 2002, Richard Sharpe <rsharpe@ns.aus.com>
 * Copyright 2003, Richard Sharpe <rsharpe@richardsharpe.com>
 * Copyright 2005, Ronnie Sahlberg (krb decryption)
 * Copyright 2005, Anders Broman (converted to asn2wrs generated dissector)
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
/* The heimdal code for decryption of GSSAPI wrappers using heimdal comes from
   Heimdal 1.6 and has been modified for wireshark's requirements.
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib.h>
#include <epan/packet.h>
#include <epan/asn1.h>
#include "packet-dcerpc.h"
#include "packet-gssapi.h"
#include "packet-kerberos.h"
#include <epan/crypt/crypt-rc4.h>
#include <epan/conversation.h>
#include <epan/emem.h>
#include <epan/asn1.h>

#include <stdio.h>
#include <string.h>

#include "packet-ber.h"


#define PNAME  "Simple Protected Negotiation"
#define PSNAME "SPNEGO"
#define PFNAME "spnego"

/* Initialize the protocol and registered fields */
static int proto_spnego = -1;
static int proto_spnego_krb5 = -1;


static int hf_spnego = -1;
static int hf_spnego_wraptoken = -1;
static int hf_spnego_krb5_oid;
static int hf_spnego_krb5 = -1;
static int hf_spnego_krb5_tok_id = -1;
static int hf_spnego_krb5_sgn_alg = -1;
static int hf_spnego_krb5_seal_alg = -1;
static int hf_spnego_krb5_snd_seq = -1;
static int hf_spnego_krb5_sgn_cksum = -1;
static int hf_spnego_krb5_confounder = -1;
static int hf_spnego_krb5_filler = -1;
static int hf_spnego_krb5_cfx_flags = -1;
static int hf_spnego_krb5_cfx_flags_01 = -1;
static int hf_spnego_krb5_cfx_flags_02 = -1;
static int hf_spnego_krb5_cfx_flags_04 = -1;
static int hf_spnego_krb5_cfx_ec = -1;
static int hf_spnego_krb5_cfx_rrc = -1;
static int hf_spnego_krb5_cfx_seq = -1;


/*--- Included file: packet-spnego-hf.c ---*/
#line 1 "packet-spnego-hf.c"
static int hf_spnego_negTokenInit = -1;           /* NegTokenInit */
static int hf_spnego_negTokenTarg = -1;           /* NegTokenTarg */
static int hf_spnego_MechTypeList_item = -1;      /* MechType */
static int hf_spnego_principal = -1;              /* GeneralString */
static int hf_spnego_mechTypes = -1;              /* MechTypeList */
static int hf_spnego_reqFlags = -1;               /* ContextFlags */
static int hf_spnego_mechToken = -1;              /* T_mechToken */
static int hf_spnego_negTokenInit_mechListMIC = -1;  /* T_NegTokenInit_mechListMIC */
static int hf_spnego_negResult = -1;              /* T_negResult */
static int hf_spnego_supportedMech = -1;          /* T_supportedMech */
static int hf_spnego_responseToken = -1;          /* T_responseToken */
static int hf_spnego_mechListMIC = -1;            /* T_mechListMIC */
static int hf_spnego_thisMech = -1;               /* MechType */
static int hf_spnego_innerContextToken = -1;      /* InnerContextToken */
/* named bits */
static int hf_spnego_ContextFlags_delegFlag = -1;
static int hf_spnego_ContextFlags_mutualFlag = -1;
static int hf_spnego_ContextFlags_replayFlag = -1;
static int hf_spnego_ContextFlags_sequenceFlag = -1;
static int hf_spnego_ContextFlags_anonFlag = -1;
static int hf_spnego_ContextFlags_confFlag = -1;
static int hf_spnego_ContextFlags_integFlag = -1;

/*--- End of included file: packet-spnego-hf.c ---*/
#line 84 "packet-spnego-template.c"

/* Global variables */
static const char *MechType_oid;
gssapi_oid_value *next_level_value;
gboolean saw_mechanism = FALSE;


/* Initialize the subtree pointers */
static gint ett_spnego = -1;
static gint ett_spnego_wraptoken = -1;
static gint ett_spnego_krb5 = -1;
static gint ett_spnego_krb5_cfx_flags = -1;


/*--- Included file: packet-spnego-ett.c ---*/
#line 1 "packet-spnego-ett.c"
static gint ett_spnego_NegotiationToken = -1;
static gint ett_spnego_MechTypeList = -1;
static gint ett_spnego_PrincipalSeq = -1;
static gint ett_spnego_NegTokenInit = -1;
static gint ett_spnego_ContextFlags = -1;
static gint ett_spnego_NegTokenTarg = -1;
static gint ett_spnego_InitialContextToken_U = -1;

/*--- End of included file: packet-spnego-ett.c ---*/
#line 98 "packet-spnego-template.c"

/*
 * Unfortunately, we have to have a forward declaration of this,
 * as the code generated by asn2wrs includes a call before the
 * definition.
 */
static int dissect_spnego_PrincipalSeq(gboolean implicit_tag, tvbuff_t *tvb,
                                       int offset, asn1_ctx_t *actx _U_,
                                       proto_tree *tree, int hf_index);


/*--- Included file: packet-spnego-fn.c ---*/
#line 1 "packet-spnego-fn.c"


static int
dissect_spnego_MechType(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
#line 23 "spnego.cnf"

  gssapi_oid_value *value;

  offset = dissect_ber_object_identifier_str(implicit_tag, actx, tree, tvb, offset, hf_index, &MechType_oid);


  value = gssapi_lookup_oid_str(MechType_oid);

  /*
   * Tell our caller the first mechanism we see, so that if
   * this is a negTokenInit with a mechToken, it can interpret
   * the mechToken according to the first mechType.  (There
   * might not have been any indication of the mechType
   * in prior frames, so we can't necessarily use the
   * mechanism from the conversation; i.e., a negTokenInit
   * can contain the initial security token for the desired
   * mechanism of the initiator - that's the first mechanism
   * in the list.)
   */
  if (!saw_mechanism) {
    if (value)
      next_level_value = value;
    saw_mechanism = TRUE;
  }



  return offset;
}


static const ber_sequence_t MechTypeList_sequence_of[1] = {
  { &hf_spnego_MechTypeList_item, BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_spnego_MechType },
};

static int
dissect_spnego_MechTypeList(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
#line 91 "spnego.cnf"

  conversation_t *conversation;

  saw_mechanism = FALSE;

  offset = dissect_ber_sequence_of(implicit_tag, actx, tree, tvb, offset,
                                      MechTypeList_sequence_of, hf_index, ett_spnego_MechTypeList);


  /* 
   * If we saw a mechType we need to store it in case the negTokenTarg
   * does not provide a supportedMech.
   */
  if(saw_mechanism){
    conversation = find_conversation(actx->pinfo->fd->num, 
                                        &actx->pinfo->src, &actx->pinfo->dst,
					actx->pinfo->ptype, 
					actx->pinfo->srcport, actx->pinfo->destport, 0);
    if(!conversation){
      conversation = conversation_new(actx->pinfo->fd->num, 
                                        &actx->pinfo->src, &actx->pinfo->dst,
    	                                actx->pinfo->ptype,
                                        actx->pinfo->srcport, actx->pinfo->destport, 0);
    }
    conversation_add_proto_data(conversation, proto_spnego, next_level_value);
  }



  return offset;
}


static const asn_namedbit ContextFlags_bits[] = {
  {  0, &hf_spnego_ContextFlags_delegFlag, -1, -1, "delegFlag", NULL },
  {  1, &hf_spnego_ContextFlags_mutualFlag, -1, -1, "mutualFlag", NULL },
  {  2, &hf_spnego_ContextFlags_replayFlag, -1, -1, "replayFlag", NULL },
  {  3, &hf_spnego_ContextFlags_sequenceFlag, -1, -1, "sequenceFlag", NULL },
  {  4, &hf_spnego_ContextFlags_anonFlag, -1, -1, "anonFlag", NULL },
  {  5, &hf_spnego_ContextFlags_confFlag, -1, -1, "confFlag", NULL },
  {  6, &hf_spnego_ContextFlags_integFlag, -1, -1, "integFlag", NULL },
  { 0, NULL, 0, 0, NULL, NULL }
};

static int
dissect_spnego_ContextFlags(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_bitstring(implicit_tag, actx, tree, tvb, offset,
                                    ContextFlags_bits, hf_index, ett_spnego_ContextFlags,
                                    NULL);

  return offset;
}



static int
dissect_spnego_T_mechToken(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
#line 121 "spnego.cnf"

  tvbuff_t *mechToken_tvb = NULL;

  offset = dissect_ber_octet_string(implicit_tag, actx, tree, tvb, offset, hf_index,
                                       &mechToken_tvb);


  /*
   * Now, we should be able to dispatch, if we've gotten a tvbuff for
   * the token and we have information on how to dissect its contents.
   */
  if (mechToken_tvb && next_level_value)
     call_dissector(next_level_value->handle, mechToken_tvb, actx->pinfo, tree);




  return offset;
}



static int
dissect_spnego_T_NegTokenInit_mechListMIC(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
#line 135 "spnego.cnf"

  gint8 class;
  gboolean pc;
  gint32 tag;
  tvbuff_t *mechListMIC_tvb;

  /*
   * There seems to be two different forms this can take,
   * one as an octet string, and one as a general string in a 
   * sequence.
   *
   * Peek at the header, and then decide which it is we're seeing.
   */
  get_ber_identifier(tvb, offset, &class, &pc, &tag);
  if (class == BER_CLASS_UNI && pc && tag == BER_UNI_TAG_SEQUENCE) {
    /*
     * It's a sequence.
     */
    return dissect_spnego_PrincipalSeq(FALSE, tvb, offset, actx, tree,
                                       hf_spnego_mechListMIC);
  } else {
    /*
     * It's not a sequence, so dissect it as an octet string,
     * which is what it's supposed to be; that'll cause the
     * right error report if it's not an octet string, either.
     */
    offset = dissect_ber_octet_string(FALSE, actx, tree, tvb, offset,
                                      hf_spnego_mechListMIC, &mechListMIC_tvb);

    /*
     * Now, we should be able to dispatch with that tvbuff.
     */
    if (mechListMIC_tvb && next_level_value)
      call_dissector(next_level_value->handle, mechListMIC_tvb, actx->pinfo, tree);
    return offset;
  }



  return offset;
}


static const ber_sequence_t NegTokenInit_sequence[] = {
  { &hf_spnego_mechTypes    , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_spnego_MechTypeList },
  { &hf_spnego_reqFlags     , BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_spnego_ContextFlags },
  { &hf_spnego_mechToken    , BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_spnego_T_mechToken },
  { &hf_spnego_negTokenInit_mechListMIC, BER_CLASS_CON, 3, BER_FLAGS_OPTIONAL, dissect_spnego_T_NegTokenInit_mechListMIC },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_spnego_NegTokenInit(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   NegTokenInit_sequence, hf_index, ett_spnego_NegTokenInit);

  return offset;
}


static const value_string spnego_T_negResult_vals[] = {
  {   0, "accept-completed" },
  {   1, "accept-incomplete" },
  {   2, "reject" },
  { 0, NULL }
};


static int
dissect_spnego_T_negResult(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_integer(implicit_tag, actx, tree, tvb, offset, hf_index,
                                  NULL);

  return offset;
}



static int
dissect_spnego_T_supportedMech(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
#line 173 "spnego.cnf"

  conversation_t *conversation;

  saw_mechanism = FALSE;

  offset = dissect_spnego_MechType(implicit_tag, tvb, offset, actx, tree, hf_index);


  /*
   * If we saw an explicit mechType we store this in the conversation so that
   * it will override any mechType we might have picked up from the 
   * negTokenInit.
   */
  if(saw_mechanism){
    conversation = find_conversation(actx->pinfo->fd->num, 
                                        &actx->pinfo->src, &actx->pinfo->dst,
					actx->pinfo->ptype, 
					actx->pinfo->srcport, actx->pinfo->destport, 0);
    if(!conversation){
      conversation = conversation_new(actx->pinfo->fd->num, 
                                        &actx->pinfo->src, &actx->pinfo->dst,
    	                                actx->pinfo->ptype,
                                        actx->pinfo->srcport, actx->pinfo->destport, 0);
    }
    conversation_add_proto_data(conversation, proto_spnego, next_level_value);
  }




  return offset;
}



static int
dissect_spnego_T_responseToken(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
#line 205 "spnego.cnf"

  tvbuff_t *responseToken_tvb;


  offset = dissect_ber_octet_string(implicit_tag, actx, tree, tvb, offset, hf_index,
                                       &responseToken_tvb);



  /*
   * Now, we should be able to dispatch, if we've gotten a tvbuff for
   * the token and we have information on how to dissect its contents.
   * However, we should make sure that there is something in the 
   * response token ...
   */
  if (responseToken_tvb && (tvb_reported_length(responseToken_tvb) > 0) ){
    gssapi_oid_value *value=next_level_value;

    if(value){
      call_dissector(value->handle, responseToken_tvb, actx->pinfo, tree);
    }
  }




  return offset;
}



static int
dissect_spnego_T_mechListMIC(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
#line 232 "spnego.cnf"

  tvbuff_t *mechListMIC_tvb;


  offset = dissect_ber_octet_string(implicit_tag, actx, tree, tvb, offset, hf_index,
                                       &mechListMIC_tvb);



  /*
   * Now, we should be able to dispatch, if we've gotten a tvbuff for
   * the MIC and we have information on how to dissect its contents.
   */
  if (mechListMIC_tvb && (tvb_reported_length(mechListMIC_tvb) > 0) ){
    gssapi_oid_value *value=next_level_value;

    if(value){
      call_dissector(value->handle, mechListMIC_tvb, actx->pinfo, tree);
    }
  }




  return offset;
}


static const ber_sequence_t NegTokenTarg_sequence[] = {
  { &hf_spnego_negResult    , BER_CLASS_CON, 0, BER_FLAGS_OPTIONAL, dissect_spnego_T_negResult },
  { &hf_spnego_supportedMech, BER_CLASS_CON, 1, BER_FLAGS_OPTIONAL, dissect_spnego_T_supportedMech },
  { &hf_spnego_responseToken, BER_CLASS_CON, 2, BER_FLAGS_OPTIONAL, dissect_spnego_T_responseToken },
  { &hf_spnego_mechListMIC  , BER_CLASS_CON, 3, BER_FLAGS_OPTIONAL, dissect_spnego_T_mechListMIC },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_spnego_NegTokenTarg(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   NegTokenTarg_sequence, hf_index, ett_spnego_NegTokenTarg);

  return offset;
}


static const value_string spnego_NegotiationToken_vals[] = {
  {   0, "negTokenInit" },
  {   1, "negTokenTarg" },
  { 0, NULL }
};

static const ber_choice_t NegotiationToken_choice[] = {
  {   0, &hf_spnego_negTokenInit , BER_CLASS_CON, 0, 0, dissect_spnego_NegTokenInit },
  {   1, &hf_spnego_negTokenTarg , BER_CLASS_CON, 1, 0, dissect_spnego_NegTokenTarg },
  { 0, NULL, 0, 0, 0, NULL }
};

static int
dissect_spnego_NegotiationToken(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_choice(actx, tree, tvb, offset,
                                 NegotiationToken_choice, hf_index, ett_spnego_NegotiationToken,
                                 NULL);

  return offset;
}



static int
dissect_spnego_GeneralString(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_restricted_string(implicit_tag, BER_UNI_TAG_GeneralString,
                                            actx, tree, tvb, offset, hf_index,
                                            NULL);

  return offset;
}


static const ber_sequence_t PrincipalSeq_sequence[] = {
  { &hf_spnego_principal    , BER_CLASS_CON, 0, 0, dissect_spnego_GeneralString },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_spnego_PrincipalSeq(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   PrincipalSeq_sequence, hf_index, ett_spnego_PrincipalSeq);

  return offset;
}



static int
dissect_spnego_InnerContextToken(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
#line 48 "spnego.cnf"

  gssapi_oid_value *next_level_value;
  proto_item *item;
  proto_tree *subtree;
  tvbuff_t *token_tvb;
  int len;

  /*
   * XXX - what should we do if this OID doesn't match the value
   * attached to the frame or conversation?  (That would be
   * bogus, but that's not impossible - some broken implementation
   * might negotiate some security mechanism but put the OID
   * for some other security mechanism in GSS_Wrap tokens.)
   * Does it matter?
   */
  next_level_value = gssapi_lookup_oid_str(MechType_oid);

  /*
   * Now dissect the GSS_Wrap token; it's assumed to be in the
   * rest of the tvbuff.
   */
  item = proto_tree_add_item(tree, hf_spnego_wraptoken, tvb, offset, -1, FALSE); 

  subtree = proto_item_add_subtree(item, ett_spnego_wraptoken);

  /*
   * Now, we should be able to dispatch after creating a new TVB.
   * The subdissector must return the length of the part of the
   * token it dissected, so we can return the length of the part
   * we (and it) dissected.
   */
  token_tvb = tvb_new_subset_remaining(tvb, offset);
  if (next_level_value && next_level_value->wrap_handle) {
    len = call_dissector(next_level_value->wrap_handle, token_tvb, actx->pinfo,
                         subtree);
    if (len == 0)
      offset = tvb_length(tvb);
    else
      offset = offset + len;
  } else
    offset = tvb_length(tvb);



  return offset;
}


static const ber_sequence_t InitialContextToken_U_sequence[] = {
  { &hf_spnego_thisMech     , BER_CLASS_UNI, BER_UNI_TAG_OID, BER_FLAGS_NOOWNTAG, dissect_spnego_MechType },
  { &hf_spnego_innerContextToken, BER_CLASS_ANY, 0, BER_FLAGS_NOOWNTAG, dissect_spnego_InnerContextToken },
  { NULL, 0, 0, 0, NULL }
};

static int
dissect_spnego_InitialContextToken_U(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_sequence(implicit_tag, actx, tree, tvb, offset,
                                   InitialContextToken_U_sequence, hf_index, ett_spnego_InitialContextToken_U);

  return offset;
}



static int
dissect_spnego_InitialContextToken(gboolean implicit_tag _U_, tvbuff_t *tvb _U_, int offset _U_, asn1_ctx_t *actx _U_, proto_tree *tree _U_, int hf_index _U_) {
  offset = dissect_ber_tagged_type(implicit_tag, actx, tree, tvb, offset,
                                      hf_index, BER_CLASS_APP, 0, TRUE, dissect_spnego_InitialContextToken_U);

  return offset;
}


/*--- End of included file: packet-spnego-fn.c ---*/
#line 109 "packet-spnego-template.c"
/*
 * This is the SPNEGO KRB5 dissector. It is not true KRB5, but some ASN.1
 * wrapped blob with an OID, USHORT token ID, and a Ticket, that is also
 * ASN.1 wrapped by the looks of it. It conforms to RFC1964.
 */

#define KRB_TOKEN_AP_REQ		0x0001
#define KRB_TOKEN_AP_REP		0x0002
#define KRB_TOKEN_AP_ERR		0x0003
#define KRB_TOKEN_GETMIC		0x0101
#define KRB_TOKEN_WRAP			0x0102
#define KRB_TOKEN_DELETE_SEC_CONTEXT	0x0201
#define KRB_TOKEN_CFX_GETMIC		0x0404
#define KRB_TOKEN_CFX_WRAP		0x0405

static const value_string spnego_krb5_tok_id_vals[] = {
  { KRB_TOKEN_AP_REQ,             "KRB5_AP_REQ"},
  { KRB_TOKEN_AP_REP,             "KRB5_AP_REP"},
  { KRB_TOKEN_AP_ERR,             "KRB5_ERROR"},
  { KRB_TOKEN_GETMIC,             "KRB5_GSS_GetMIC" },
  { KRB_TOKEN_WRAP,               "KRB5_GSS_Wrap" },
  { KRB_TOKEN_DELETE_SEC_CONTEXT, "KRB5_GSS_Delete_sec_context" },
  { KRB_TOKEN_CFX_GETMIC,         "KRB_TOKEN_CFX_GetMic" },
  { KRB_TOKEN_CFX_WRAP,	          "KRB_TOKEN_CFX_WRAP" },
  { 0, NULL}
};

#define KRB_SGN_ALG_DES_MAC_MD5	0x0000
#define KRB_SGN_ALG_MD2_5	0x0001
#define KRB_SGN_ALG_DES_MAC	0x0002
#define KRB_SGN_ALG_HMAC	0x0011

static const value_string spnego_krb5_sgn_alg_vals[] = {
  { KRB_SGN_ALG_DES_MAC_MD5, "DES MAC MD5"},
  { KRB_SGN_ALG_MD2_5,       "MD2.5"},
  { KRB_SGN_ALG_DES_MAC,     "DES MAC"},
  { KRB_SGN_ALG_HMAC,        "HMAC"},
  { 0, NULL}
};

#define KRB_SEAL_ALG_DES_CBC	0x0000
#define KRB_SEAL_ALG_RC4	0x0010
#define KRB_SEAL_ALG_NONE	0xffff

static const value_string spnego_krb5_seal_alg_vals[] = {
  { KRB_SEAL_ALG_DES_CBC, "DES CBC"},
  { KRB_SEAL_ALG_RC4,     "RC4"},
  { KRB_SEAL_ALG_NONE,    "None"},
  { 0, NULL}
};

/*
 * XXX - is this for SPNEGO or just GSS-API?
 * RFC 1964 is "The Kerberos Version 5 GSS-API Mechanism"; presumably one
 * can directly designate Kerberos V5 as a mechanism in GSS-API, rather
 * than designating SPNEGO as the mechanism, offering Kerberos V5, and
 * getting it accepted.
 */
static int
dissect_spnego_krb5_getmic_base(tvbuff_t *tvb, int offset, packet_info *pinfo, proto_tree *tree);
static int
dissect_spnego_krb5_wrap_base(tvbuff_t *tvb, int offset, packet_info *pinfo, proto_tree *tree, guint16 token_id);
static int
dissect_spnego_krb5_cfx_getmic_base(tvbuff_t *tvb, int offset, packet_info *pinfo, proto_tree *tree);
static int
dissect_spnego_krb5_cfx_wrap_base(tvbuff_t *tvb, int offset, packet_info *pinfo, proto_tree *tree, guint16 token_id);

static void
dissect_spnego_krb5(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
	proto_item *item;
	proto_tree *subtree;
	int offset = 0;
	guint16 token_id;
	const char *oid;
	gssapi_oid_value *value;
	tvbuff_t *krb5_tvb;
	gint8 class;
	gboolean pc, ind = 0;
	gint32 tag;
	guint32 len;
	asn1_ctx_t asn1_ctx;
	asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, TRUE, pinfo);

	item = proto_tree_add_item(tree, hf_spnego_krb5, tvb, offset,
				   -1, FALSE);

	subtree = proto_item_add_subtree(item, ett_spnego_krb5);

	/*
	 * The KRB5 blob conforms to RFC1964:
	 * [APPLICATION 0] {
	 *   OID,
	 *   USHORT (0x0001 == AP-REQ, 0x0002 == AP-REP, 0x0003 == ERROR),
	 *   OCTET STRING }
         *
         * However, for some protocols, the KRB5 blob starts at the SHORT
	 * and has no DER encoded header etc.
	 *
	 * It appears that for some other protocols the KRB5 blob is just
	 * a Kerberos message, with no [APPLICATION 0] header, no OID,
	 * and no USHORT.
	 *
	 * So:
	 *
	 *	If we see an [APPLICATION 0] HEADER, we show the OID and
	 *	the USHORT, and then dissect the rest as a Kerberos message.
	 *
	 *	If we see an [APPLICATION 14] or [APPLICATION 15] header,
	 *	we assume it's an AP-REQ or AP-REP message, and dissect
	 *	it all as a Kerberos message.
	 *
	 *	Otherwise, we show the USHORT, and then dissect the rest
	 *	as a Kerberos message.
	 */

	/*
	 * Get the first header ...
	 */
	get_ber_identifier(tvb, offset, &class, &pc, &tag);
	if (class == BER_CLASS_APP && pc) {
	    /*
	     * [APPLICATION <tag>]
	     */
	    offset = dissect_ber_identifier(pinfo, subtree, tvb, offset, &class, &pc, &tag);
	    offset = dissect_ber_length(pinfo, subtree, tvb, offset, &len, &ind);

	    switch (tag) {

	    case 0:
		/*
		 * [APPLICATION 0]
		 */

		/* Next, the OID */
		offset=dissect_ber_object_identifier_str(FALSE, &asn1_ctx, subtree, tvb, offset, hf_spnego_krb5_oid, &oid);

		value = gssapi_lookup_oid_str(oid);

		token_id = tvb_get_letohs(tvb, offset);
		proto_tree_add_uint(subtree, hf_spnego_krb5_tok_id, tvb, offset, 2,
				    token_id);

		offset += 2;

		break;

	    case 14:	/* [APPLICATION 14] */
	    case 15:	/* [APPLICATION 15] */
		/*
		 * No token ID - just dissect as a Kerberos message and
		 * return.
		 */
		offset = dissect_kerberos_main(tvb, pinfo, subtree, FALSE, NULL);
		return;

	    default:
		proto_tree_add_text(subtree, tvb, offset, 0,
			"Unknown header (class=%d, pc=%d, tag=%d)",
			class, pc, tag);
		goto done;
	    }
	} else {
	    /* Next, the token ID ... */

	    token_id = tvb_get_letohs(tvb, offset);
	    proto_tree_add_uint(subtree, hf_spnego_krb5_tok_id, tvb, offset, 2,
				token_id);

	    offset += 2;
	}

	switch (token_id) {

	case KRB_TOKEN_AP_REQ:
	case KRB_TOKEN_AP_REP:
	case KRB_TOKEN_AP_ERR:
	  krb5_tvb = tvb_new_subset_remaining(tvb, offset);
	  offset = dissect_kerberos_main(krb5_tvb, pinfo, subtree, FALSE, NULL);
	  break;

	case KRB_TOKEN_GETMIC:
	  offset = dissect_spnego_krb5_getmic_base(tvb, offset, pinfo, subtree);
	  break;

	case KRB_TOKEN_WRAP:
          offset = dissect_spnego_krb5_wrap_base(tvb, offset, pinfo, subtree, token_id);
	  break;

	case KRB_TOKEN_DELETE_SEC_CONTEXT:

	  break;

	case KRB_TOKEN_CFX_GETMIC:
	  offset = dissect_spnego_krb5_cfx_getmic_base(tvb, offset, pinfo, subtree);
	  break;

	case KRB_TOKEN_CFX_WRAP:
          offset = dissect_spnego_krb5_cfx_wrap_base(tvb, offset, pinfo, subtree, token_id);
	  break;

	default:

	  break;
	}

 done:
	proto_item_set_len(item, offset);
	return;
}

#ifdef HAVE_KERBEROS
#include <epan/crypt/crypt-md5.h>

#ifndef KEYTYPE_ARCFOUR_56
# define KEYTYPE_ARCFOUR_56 24
#endif
/* XXX - We should probably do a configure-time check for this instead */
#ifndef KRB5_KU_USAGE_SEAL
# define KRB5_KU_USAGE_SEAL 22
#endif

static int
arcfour_mic_key(void *key_data, size_t key_size, int key_type,
		void *cksum_data, size_t cksum_size,
		void *key6_data)
{
    guint8 k5_data[16];
    guint8 T[4];

    memset(T, 0, 4);

    if (key_type == KEYTYPE_ARCFOUR_56) {
	guint8 L40[14] = "fortybits";

	memcpy(L40 + 10, T, sizeof(T));
	md5_hmac(
                L40, 14,
                key_data,
                key_size,
	    	k5_data);
	memset(&k5_data[7], 0xAB, 9);
    } else {
	md5_hmac(
                T, 4,
                key_data,
                key_size,
	    	k5_data);
    }

    md5_hmac(
	cksum_data, cksum_size,
	k5_data,
	16,
	key6_data);

    return 0;
}

static int
usage2arcfour(int usage)
{
    switch (usage) {
    case 3: /*KRB5_KU_AS_REP_ENC_PART 3 */
    case 9: /*KRB5_KU_TGS_REP_ENC_PART_SUB_KEY 9 */
	return 8;
    case 22: /*KRB5_KU_USAGE_SEAL 22 */
	return 13;
    case 23: /*KRB5_KU_USAGE_SIGN 23 */
        return 15;
    case 24: /*KRB5_KU_USAGE_SEQ 24 */
	return 0;
    default :
	return 0;
    }
}

static int
arcfour_mic_cksum(guint8 *key_data, int key_length,
		  unsigned usage,
		  guint8 sgn_cksum[8],
		  const void *v1, size_t l1,
		  const void *v2, size_t l2,
		  const void *v3, size_t l3)
{
    const guint8 signature[] = "signaturekey";
    guint8 ksign_c[16];
    unsigned char t[4];
    md5_state_t ms;
    unsigned char digest[16];
    int rc4_usage;
    guint8 cksum[16];

    rc4_usage=usage2arcfour(usage);
    md5_hmac(signature, sizeof(signature),
		key_data, key_length,
		ksign_c);
    md5_init(&ms);
    t[0] = (rc4_usage >>  0) & 0xFF;
    t[1] = (rc4_usage >>  8) & 0xFF;
    t[2] = (rc4_usage >> 16) & 0xFF;
    t[3] = (rc4_usage >> 24) & 0xFF;
    md5_append(&ms, t, 4);
    md5_append(&ms, v1, l1);
    md5_append(&ms, v2, l2);
    md5_append(&ms, v3, l3);
    md5_finish(&ms, digest);
    md5_hmac(digest, 16, ksign_c, 16, cksum);

    memcpy(sgn_cksum, cksum, 8);

    return 0;
}

/*
 * Verify padding of a gss wrapped message and return its length.
 */
static int
gssapi_verify_pad(unsigned char *wrapped_data, int wrapped_length,
		   size_t datalen,
		   size_t *padlen)
{
    unsigned char *pad;
    size_t padlength;
    int i;

    pad = wrapped_data + wrapped_length - 1;
    padlength = *pad;

    if (padlength > datalen)
	return 1;

    for (i = padlength; i > 0 && *pad == padlength; i--, pad--)
	;
    if (i != 0)
	return 2;

    *padlen = padlength;

    return 0;
}

static int
decrypt_arcfour(packet_info *pinfo,
	 guint8 *input_message_buffer,
	 guint8 *output_message_buffer,
	 guint8 *key_value, int key_size, int key_type)
{
    guint8 Klocaldata[16];
    int ret;
    gint32 seq_number;
    size_t datalen;
    guint8 k6_data[16];
    guint32 SND_SEQ[2];
    guint8 Confounder[8];
    guint8 cksum_data[8];
    int cmp;
    int conf_flag;
    size_t padlen = 0;

    datalen = tvb_length(pinfo->gssapi_encrypted_tvb);

    if(tvb_get_ntohs(pinfo->gssapi_wrap_tvb, 4)==0x1000){
	conf_flag=1;
    } else if (tvb_get_ntohs(pinfo->gssapi_wrap_tvb, 4)==0xffff){
	conf_flag=0;
    } else {
	return -3;
    }

    if(tvb_get_ntohs(pinfo->gssapi_wrap_tvb, 6)!=0xffff){
	return -4;
    }

    ret = arcfour_mic_key(key_value, key_size, key_type,
			  (void *)tvb_get_ptr(pinfo->gssapi_wrap_tvb, 16, 8),
			  8, /* SGN_CKSUM */
			  k6_data);
    if (ret) {
	return -5;
    }

    {
	rc4_state_struct rc4_state;

	crypt_rc4_init(&rc4_state, k6_data, sizeof(k6_data));
	memcpy(SND_SEQ, (unsigned char *)tvb_get_ptr(pinfo->gssapi_wrap_tvb, 8, 8), 8);
	crypt_rc4(&rc4_state, (unsigned char *)SND_SEQ, 8);

	memset(k6_data, 0, sizeof(k6_data));
    }

    seq_number=g_ntohl(SND_SEQ[0]);

    if (SND_SEQ[1] != 0xFFFFFFFF && SND_SEQ[1] != 0x00000000) {
	return -6;
    }

    {
	int i;

	for (i = 0; i < 16; i++)
	    Klocaldata[i] = ((guint8 *)key_value)[i] ^ 0xF0;
    }
    ret = arcfour_mic_key(Klocaldata,sizeof(Klocaldata),key_type,
			  (unsigned char *)SND_SEQ, 4,
			  k6_data);
    memset(Klocaldata, 0, sizeof(Klocaldata));
    if (ret) {
	return -7;
    }

    if(conf_flag) {
	rc4_state_struct rc4_state;

	crypt_rc4_init(&rc4_state, k6_data, sizeof(k6_data));
	memcpy(Confounder, (unsigned char *)tvb_get_ptr(pinfo->gssapi_wrap_tvb, 24, 8), 8);
	crypt_rc4(&rc4_state, Confounder, 8);
	memcpy(output_message_buffer, input_message_buffer, datalen);
	crypt_rc4(&rc4_state, output_message_buffer, datalen);
    } else {
	memcpy(Confounder,
		tvb_get_ptr(pinfo->gssapi_wrap_tvb, 24, 8),
		8); /* Confounder */
	memcpy(output_message_buffer,
		input_message_buffer,
	        datalen);
    }
    memset(k6_data, 0, sizeof(k6_data));

    /* only normal (i.e. non DCE style  wrapping use padding ? */
    if(pinfo->decrypt_gssapi_tvb==DECRYPT_GSSAPI_NORMAL){
	ret = gssapi_verify_pad(output_message_buffer,datalen,datalen, &padlen);
    	if (ret) {
	    return -9;
	}
	datalen -= padlen;
    }

    /* dont know what the checksum looks like for dce style gssapi */
    if(pinfo->decrypt_gssapi_tvb==DECRYPT_GSSAPI_NORMAL){
	ret = arcfour_mic_cksum(key_value, key_size,
			    KRB5_KU_USAGE_SEAL,
			    cksum_data,
			    tvb_get_ptr(pinfo->gssapi_wrap_tvb, 0, 8), 8,
			    Confounder, sizeof(Confounder),
			    output_message_buffer,
			    datalen + padlen);
	if (ret) {
	    return -10;
	}

	cmp = memcmp(cksum_data,
	    tvb_get_ptr(pinfo->gssapi_wrap_tvb, 16, 8),
	    8); /* SGN_CKSUM */
	if (cmp) {
	    return -11;
	}
    }

    return datalen;
}



#if defined(HAVE_HEIMDAL_KERBEROS) || defined(HAVE_MIT_KERBEROS)

static void
decrypt_gssapi_krb_arcfour_wrap(proto_tree *tree, packet_info *pinfo, tvbuff_t *tvb, int keytype)
{
	int ret;
	enc_key_t *ek;
	int length;
	const guint8 *original_data;

	static int omb_index=0;
	static guint8 *omb_arr[4]={NULL,NULL,NULL,NULL};
	static guint8 *cryptocopy=NULL; /* workaround for pre-0.6.1 heimdal bug */
	guint8 *output_message_buffer;

	omb_index++;
	if(omb_index>=4){
		omb_index=0;
	}
	output_message_buffer=omb_arr[omb_index];


	length=tvb_length(pinfo->gssapi_encrypted_tvb);
	original_data=tvb_get_ptr(pinfo->gssapi_encrypted_tvb, 0, length);

	/* dont do anything if we are not attempting to decrypt data */
/*
	if(!krb_decrypt){
		return;
	}
*/
	/* XXX we should only do this for first time, then store somewhere */
	/* XXX We also need to re-read the keytab when the preference changes */

	cryptocopy=ep_alloc(length);
	if(output_message_buffer){
		g_free(output_message_buffer);
		output_message_buffer=NULL;
	}
	output_message_buffer=g_malloc(length);

	for(ek=enc_key_list;ek;ek=ek->next){
		/* shortcircuit and bail out if enctypes are not matching */
		if(ek->keytype!=keytype){
			continue;
		}

		/* pre-0.6.1 versions of Heimdal would sometimes change
		  the cryptotext data even when the decryption failed.
		  This would obviously not work since we iterate over the
		  keys. So just give it a copy of the crypto data instead.
		  This has been seen for RC4-HMAC blobs.
		*/
		memcpy(cryptocopy, original_data, length);
		ret=decrypt_arcfour(pinfo,
				cryptocopy,
				output_message_buffer,
				ek->keyvalue,
				ek->keylength,
				ek->keytype
					    );
		if (ret >= 0) {
			proto_tree_add_text(tree, NULL, 0, 0, "[Decrypted using: %s]", ek->key_origin);
			pinfo->gssapi_decrypted_tvb=tvb_new_child_real_data(tvb, 
				output_message_buffer,
				ret, ret);
			add_new_data_source(pinfo, pinfo->gssapi_decrypted_tvb, "Decrypted GSS-Krb5");
			return;
		}
	}
	return;
}

/* borrowed from heimdal */
static int
rrc_rotate(void *data, int len, guint16 rrc, int unrotate)
{
	unsigned char *tmp, buf[256];
	size_t left;

	if (len == 0)
		return 0;

	rrc %= len;

	if (rrc == 0)
		return 0;

	left = len - rrc;

	if (rrc <= sizeof(buf)) {
		tmp = buf;
	} else {
		tmp = g_malloc(rrc);
		if (tmp == NULL)
			return -1;
	}

	if (unrotate) {
		memcpy(tmp, data, rrc);
		memmove(data, (unsigned char *)data + rrc, left);
		memcpy((unsigned char *)data + left, tmp, rrc);
	} else {
		memcpy(tmp, (unsigned char *)data + left, rrc);
		memmove((unsigned char *)data + rrc, data, left);
		memcpy(data, tmp, rrc);
	}

	if (rrc > sizeof(buf))
		g_free(tmp);

	return 0;
}


#define KRB5_KU_USAGE_ACCEPTOR_SEAL	22
#define KRB5_KU_USAGE_ACCEPTOR_SIGN	23
#define KRB5_KU_USAGE_INITIATOR_SEAL	24
#define KRB5_KU_USAGE_INITIATOR_SIGN	25

static void
decrypt_gssapi_krb_cfx_wrap(proto_tree *tree _U_, packet_info *pinfo _U_, tvbuff_t *tvb _U_, guint16 ec _U_, guint16 rrc _U_, int keytype, unsigned int usage)
{
	int res;
	char *rotated;
	char *output;
	int datalen;
	tvbuff_t *next_tvb;

	/* dont do anything if we are not attempting to decrypt data */
	if(!krb_decrypt){
		return;
	}

	rotated = ep_alloc(tvb_length(tvb));

	tvb_memcpy(tvb, rotated, 0, tvb_length(tvb));
	res = rrc_rotate(rotated, tvb_length(tvb), rrc, TRUE);

	next_tvb=tvb_new_child_real_data(tvb, rotated, tvb_length(tvb), tvb_reported_length(tvb));
	add_new_data_source(pinfo, next_tvb, "GSSAPI CFX");

	output = decrypt_krb5_data(tree, pinfo, usage, next_tvb,
		  keytype, &datalen);

	if (output) {
		char *outdata;

		outdata = ep_alloc(tvb_length(tvb));
		memcpy(outdata, output, tvb_length(tvb));
		g_free(output);

		pinfo->gssapi_decrypted_tvb=tvb_new_child_real_data(tvb,
			outdata,
			datalen-16,
			datalen-16);
		add_new_data_source(pinfo, pinfo->gssapi_decrypted_tvb, "Decrypted GSS-Krb5");
		return;
	}
	return;
}

#endif /* HAVE_HEIMDAL_KERBEROS || HAVE_MIT_KERBEROS */


#endif

/*
 * This is for GSSAPI Wrap tokens ...
 */
static int
dissect_spnego_krb5_wrap_base(tvbuff_t *tvb, int offset, packet_info *pinfo
#ifndef HAVE_KERBEROS
	_U_
#endif
    , proto_tree *tree, guint16 token_id
#ifndef HAVE_KERBEROS
	_U_
#endif
    )
{
	guint16 sgn_alg, seal_alg;
#ifdef HAVE_KERBEROS
	int start_offset=offset;
#endif

	/*
	 * The KRB5 blob conforms to RFC1964:
	 *   USHORT (0x0102 == GSS_Wrap)
	 *   and so on }
	 */

	/* Now, the sign and seal algorithms ... */

	sgn_alg = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_spnego_krb5_sgn_alg, tvb, offset, 2,
			    sgn_alg);

	offset += 2;

	seal_alg = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_spnego_krb5_seal_alg, tvb, offset, 2,
			    seal_alg);

	offset += 2;

	/* Skip the filler */

	offset += 2;

	/* Encrypted sequence number */

	proto_tree_add_item(tree, hf_spnego_krb5_snd_seq, tvb, offset, 8,
			    TRUE);

	offset += 8;

	/* Checksum of plaintext padded data */

	proto_tree_add_item(tree, hf_spnego_krb5_sgn_cksum, tvb, offset, 8,
			    TRUE);

	offset += 8;

	/*
	 * At least according to draft-brezak-win2k-krb-rc4-hmac-04,
	 * if the signing algorithm is KRB_SGN_ALG_HMAC, there's an
	 * extra 8 bytes of "Random confounder" after the checksum.
	 * It certainly confounds code expecting all Kerberos 5
	 * GSS_Wrap() tokens to look the same....
	 */
	if ((sgn_alg == KRB_SGN_ALG_HMAC) ||
	    /* there also seems to be a confounder for DES MAC MD5 - certainly seen when using with
	       SASL with LDAP between a Java client and Active Directory. If this breaks other things
	       we may need to make this an option. gal 17/2/06 */
	    (sgn_alg == KRB_SGN_ALG_DES_MAC_MD5)) {
	  proto_tree_add_item(tree, hf_spnego_krb5_confounder, tvb, offset, 8,
			      TRUE);
	  offset += 8;
	}

	/* Is the data encrypted? */
	pinfo->gssapi_data_encrypted=(seal_alg!=KRB_SEAL_ALG_NONE);

#ifdef HAVE_KERBEROS
#define GSS_ARCFOUR_WRAP_TOKEN_SIZE 32
	if(pinfo->decrypt_gssapi_tvb){
		/* if the caller did not provide a tvb, then we just use
		   whatever is left of our current tvb.
		*/
		if(!pinfo->gssapi_encrypted_tvb){
			int len;
			len=tvb_reported_length_remaining(tvb,offset);
			if(len>tvb_length_remaining(tvb, offset)){
				/* no point in trying to decrypt,
				   we dont have the full pdu.
				*/
				return offset;
			}
			pinfo->gssapi_encrypted_tvb = tvb_new_subset(
					tvb, offset, len, len);
		}

		/* if this is KRB5 wrapped rc4-hmac */
		if((token_id==KRB_TOKEN_WRAP)
		 &&(sgn_alg==KRB_SGN_ALG_HMAC)
		 &&(seal_alg==KRB_SEAL_ALG_RC4)){
			/* do we need to create a tvb for the wrapper
			   as well ?
			*/
			if(!pinfo->gssapi_wrap_tvb){
				pinfo->gssapi_wrap_tvb = tvb_new_subset(
					tvb, start_offset-2,
					GSS_ARCFOUR_WRAP_TOKEN_SIZE,
					GSS_ARCFOUR_WRAP_TOKEN_SIZE);
			}
#if defined(HAVE_HEIMDAL_KERBEROS) || defined(HAVE_MIT_KERBEROS)
			decrypt_gssapi_krb_arcfour_wrap(tree,
				pinfo,
				tvb,
				23 /* rc4-hmac */);
#endif /* HAVE_HEIMDAL_KERBEROS || HAVE_MIT_KERBEROS */
		}
	}
#endif
	/*
	 * Return the offset past the checksum, so that we know where
	 * the data we're wrapped around starts.  Also, set the length
	 * of our top-level item to that offset, so it doesn't cover
	 * the data we're wrapped around.
	 *
	 * Note that for DCERPC the GSSAPI blobs comes after the data it wraps,
	 * not before.
	 */
	return offset;
}

/*
 * XXX - This is for GSSAPI GetMIC tokens ...
 */
static int
dissect_spnego_krb5_getmic_base(tvbuff_t *tvb, int offset, packet_info *pinfo _U_, proto_tree *tree)
{
	guint16 sgn_alg;

	/*
	 * The KRB5 blob conforms to RFC1964:
	 *   USHORT (0x0101 == GSS_GetMIC)
	 *   and so on }
	 */

	/* Now, the sign algorithm ... */

	sgn_alg = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_spnego_krb5_sgn_alg, tvb, offset, 2,
			    sgn_alg);

	offset += 2;

	/* Skip the filler */

	offset += 4;

	/* Encrypted sequence number */

	proto_tree_add_item(tree, hf_spnego_krb5_snd_seq, tvb, offset, 8,
			    TRUE);

	offset += 8;

	/* Checksum of plaintext padded data */

	proto_tree_add_item(tree, hf_spnego_krb5_sgn_cksum, tvb, offset, 8,
			    TRUE);

	offset += 8;

	/*
	 * At least according to draft-brezak-win2k-krb-rc4-hmac-04,
	 * if the signing algorithm is KRB_SGN_ALG_HMAC, there's an
	 * extra 8 bytes of "Random confounder" after the checksum.
	 * It certainly confounds code expecting all Kerberos 5
	 * GSS_Wrap() tokens to look the same....
	 *
	 * The exception is DNS/TSIG where there is no such confounder
	 * so we need to test here if there are more bytes in our tvb or not.
	 *  -- ronnie
	 */
	if (tvb_length_remaining(tvb, offset)) {
	  if (sgn_alg == KRB_SGN_ALG_HMAC) {
	    proto_tree_add_item(tree, hf_spnego_krb5_confounder, tvb, offset, 8,
			      TRUE);

	    offset += 8;
	  }
        }

	/*
	 * Return the offset past the checksum, so that we know where
	 * the data we're wrapped around starts.  Also, set the length
	 * of our top-level item to that offset, so it doesn't cover
	 * the data we're wrapped around.
	 */

	return offset;
}

static int
dissect_spnego_krb5_cfx_flags(tvbuff_t *tvb, int offset,
			      proto_tree *spnego_krb5_tree,
			      guint8 cfx_flags)
{
	proto_tree *cfx_flags_tree = NULL;
	proto_item *tf = NULL;

	if (spnego_krb5_tree) {
		tf = proto_tree_add_uint(spnego_krb5_tree,
					 hf_spnego_krb5_cfx_flags,
					 tvb, offset, 1, cfx_flags);
		cfx_flags_tree = proto_item_add_subtree(tf, ett_spnego_krb5_cfx_flags);
	}

	proto_tree_add_boolean(cfx_flags_tree,
			       hf_spnego_krb5_cfx_flags_04,
			       tvb, offset, 1, cfx_flags);
	proto_tree_add_boolean(cfx_flags_tree,
			       hf_spnego_krb5_cfx_flags_02,
			       tvb, offset, 1, cfx_flags);
	proto_tree_add_boolean(cfx_flags_tree,
			       hf_spnego_krb5_cfx_flags_01,
			       tvb, offset, 1, cfx_flags);

	return (offset + 1);
}

/*
 * This is for GSSAPI CFX Wrap tokens ...
 */
static int
dissect_spnego_krb5_cfx_wrap_base(tvbuff_t *tvb, int offset, packet_info *pinfo
#ifndef HAVE_KERBEROS
	_U_
#endif
    , proto_tree *tree, guint16 token_id _U_
    )
{
	guint8 flags;
	guint16 ec;
	guint16 rrc;
	int checksum_size;
	int start_offset=offset;

	/*
	 * The KRB5 blob conforms to RFC4121:
	 *   USHORT (0x0504)
	 *   and so on }
	 */

	/* Now, the sign and seal algorithms ... */

	flags = tvb_get_guint8(tvb, offset);
	offset = dissect_spnego_krb5_cfx_flags(tvb, offset, tree, flags);

	pinfo->gssapi_data_encrypted=(flags & 2);

	/* Skip the filler */

	proto_tree_add_item(tree, hf_spnego_krb5_filler, tvb, offset, 1,
			    FALSE);
	offset += 1;

	/* EC */
	ec = tvb_get_ntohs(tvb, offset);
	proto_tree_add_item(tree, hf_spnego_krb5_cfx_ec, tvb, offset, 2,
			    FALSE);
	offset += 2;

	/* RRC */
	rrc = tvb_get_ntohs(tvb, offset);
	proto_tree_add_item(tree, hf_spnego_krb5_cfx_rrc, tvb, offset, 2,
			    FALSE);
	offset += 2;

	/* sequence number */

	proto_tree_add_item(tree, hf_spnego_krb5_cfx_seq, tvb, offset, 8,
			    FALSE);
	offset += 8;

	/* Checksum of plaintext padded data */

	if (pinfo->gssapi_data_encrypted) {
		checksum_size = 44 + ec;
	} else {
		checksum_size = 12;
	}

	proto_tree_add_item(tree, hf_spnego_krb5_sgn_cksum, tvb, offset,
			    checksum_size, FALSE);
	offset += checksum_size;

	if(pinfo->decrypt_gssapi_tvb){
		/* if the caller did not provide a tvb, then we just use
		   whatever is left of our current tvb.
		*/
		if(!pinfo->gssapi_encrypted_tvb){
			int len;
			len=tvb_reported_length_remaining(tvb,offset);
			if(len>tvb_length_remaining(tvb, offset)){
				/* no point in trying to decrypt,
				   we dont have the full pdu.
				*/
				return offset;
			}
			pinfo->gssapi_encrypted_tvb = tvb_new_subset(
					tvb, offset, len, len);
		}

		if (pinfo->gssapi_data_encrypted) {
			/* do we need to create a tvb for the wrapper
			   as well ?
			*/
			if(!pinfo->gssapi_wrap_tvb){
				pinfo->gssapi_wrap_tvb = tvb_new_subset(
					tvb, start_offset-2,
					offset - (start_offset-2),
					offset - (start_offset-2));
			}
		}
	}

#if defined(HAVE_HEIMDAL_KERBEROS) || defined(HAVE_MIT_KERBEROS)
	pinfo->gssapi_encrypted_tvb = tvb_new_subset_remaining(tvb, 16);

	if (flags & 0x0002) {
		if(pinfo->gssapi_encrypted_tvb){
			decrypt_gssapi_krb_cfx_wrap(tree,
				pinfo,
				pinfo->gssapi_encrypted_tvb,
				ec,
				rrc,
				-1,
				(flags & 0x0001)?
				KRB5_KU_USAGE_ACCEPTOR_SEAL:
				KRB5_KU_USAGE_INITIATOR_SEAL);
		}
	}
#endif /* HAVE_HEIMDAL_KERBEROS || HAVE_MIT_KERBEROS */

	/*
	 * Return the offset past the checksum, so that we know where
	 * the data we're wrapped around starts.  Also, set the length
	 * of our top-level item to that offset, so it doesn't cover
	 * the data we're wrapped around.
	 *
	 * Note that for DCERPC the GSSAPI blobs comes after the data it wraps,
	 * not before.
	 */
	return offset;
}

/*
 * XXX - This is for GSSAPI CFX GetMIC tokens ...
 */
static int
dissect_spnego_krb5_cfx_getmic_base(tvbuff_t *tvb, int offset, packet_info *pinfo _U_, proto_tree *tree)
{
	guint8 flags;
	int checksum_size;

	/*
	 * The KRB5 blob conforms to RFC4121:
	 *   USHORT (0x0404 == GSS_GetMIC)
	 *   and so on }
	 */

	flags = tvb_get_guint8(tvb, offset);
	offset = dissect_spnego_krb5_cfx_flags(tvb, offset, tree, flags);

	/* Skip the filler */

	proto_tree_add_item(tree, hf_spnego_krb5_filler, tvb, offset, 5,
			    FALSE);
	offset += 5;

	/* sequence number */

	proto_tree_add_item(tree, hf_spnego_krb5_cfx_seq, tvb, offset, 8,
			    FALSE);
	offset += 8;

	/* Checksum of plaintext padded data */

	checksum_size = tvb_length_remaining(tvb, offset);

	proto_tree_add_item(tree, hf_spnego_krb5_sgn_cksum, tvb, offset,
			    checksum_size, FALSE);
	offset += checksum_size;

	/*
	 * Return the offset past the checksum, so that we know where
	 * the data we're wrapped around starts.  Also, set the length
	 * of our top-level item to that offset, so it doesn't cover
	 * the data we're wrapped around.
	 */

	return offset;
}

/*
 * XXX - is this for SPNEGO or just GSS-API?
 * RFC 1964 is "The Kerberos Version 5 GSS-API Mechanism"; presumably one
 * can directly designate Kerberos V5 as a mechanism in GSS-API, rather
 * than designating SPNEGO as the mechanism, offering Kerberos V5, and
 * getting it accepted.
 */
static int
dissect_spnego_krb5_wrap(tvbuff_t *tvb, packet_info *pinfo _U_, proto_tree *tree)
{
	proto_item *item;
	proto_tree *subtree;
	int offset = 0;
	guint16 token_id;

	item = proto_tree_add_item(tree, hf_spnego_krb5, tvb, 0, -1, FALSE);

	subtree = proto_item_add_subtree(item, ett_spnego_krb5);

	/*
	 * The KRB5 blob conforms to RFC1964:
	 *   USHORT (0x0102 == GSS_Wrap)
	 *   and so on }
	 */

	/* First, the token ID ... */

	token_id = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(subtree, hf_spnego_krb5_tok_id, tvb, offset, 2,
			    token_id);

	offset += 2;

	switch (token_id) {
	case KRB_TOKEN_GETMIC:
	  offset = dissect_spnego_krb5_getmic_base(tvb, offset, pinfo, subtree);
	  break;

	case KRB_TOKEN_WRAP:
          offset = dissect_spnego_krb5_wrap_base(tvb, offset, pinfo, subtree, token_id);
	  break;

	case KRB_TOKEN_CFX_GETMIC:
	  offset = dissect_spnego_krb5_cfx_getmic_base(tvb, offset, pinfo, subtree);
	  break;

	case KRB_TOKEN_CFX_WRAP:
          offset = dissect_spnego_krb5_cfx_wrap_base(tvb, offset, pinfo, subtree, token_id);
	  break;

	default:

	  break;
	}

        /*
	 * Return the offset past the checksum, so that we know where
	 * the data we're wrapped around starts.  Also, set the length
	 * of our top-level item to that offset, so it doesn't cover
	 * the data we're wrapped around.
	 */
	proto_item_set_len(item, offset);
	return offset;
}

/* Spnego stuff from here */

static int
dissect_spnego_wrap(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
	proto_item *item;
	proto_tree *subtree;
	int offset = 0;
	asn1_ctx_t asn1_ctx;
	asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, TRUE, pinfo);



	/*
	 * We need this later, so lets get it now ...
	 * It has to be per-frame as there can be more than one GSS-API
	 * negotiation in a conversation.
	 */


	item = proto_tree_add_item(tree, hf_spnego, tvb, offset,
				   -1, FALSE);

	subtree = proto_item_add_subtree(item, ett_spnego);
	/*
	 * The TVB contains a [0] header and a sequence that consists of an
	 * object ID and a blob containing the data ...
	 * XXX - is this RFC 2743's "Mechanism-Independent Token Format",
	 * with the "optional" "use in non-initial tokens" being chosen.
	 * ASN1 code addet to spnego.asn to handle this.
	 */

	offset = dissect_spnego_InitialContextToken(FALSE, tvb, offset, &asn1_ctx , subtree, -1);

	return offset;
}


static void
dissect_spnego(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree)
{
	proto_item *item;
	proto_tree *subtree;
	int offset = 0;
	conversation_t *conversation;
	asn1_ctx_t asn1_ctx;
	asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, TRUE, pinfo);

	/*
	 * We need this later, so lets get it now ...
	 * It has to be per-frame as there can be more than one GSS-API
	 * negotiation in a conversation.
	 */
	next_level_value = p_get_proto_data(pinfo->fd, proto_spnego);
	if (!next_level_value && !pinfo->fd->flags.visited) {
	    /*
	     * No handle attached to this frame, but it's the first
	     * pass, so it'd be attached to the conversation.
	     * If we have a conversation, try to get the handle,
	     * and if we get one, attach it to the frame.
	     */
	    conversation = find_conversation(pinfo->fd->num, &pinfo->src, &pinfo->dst,
					     pinfo->ptype, pinfo->srcport,
					     pinfo->destport, 0);

	    if (conversation) {
		next_level_value = conversation_get_proto_data(conversation,
							       proto_spnego);
		if (next_level_value)
		    p_add_proto_data(pinfo->fd, proto_spnego, next_level_value);
	    }
	}

	item = proto_tree_add_item(parent_tree, hf_spnego, tvb, offset,
				   -1, FALSE);

	subtree = proto_item_add_subtree(item, ett_spnego);

	/*
	 * The TVB contains a [0] header and a sequence that consists of an
	 * object ID and a blob containing the data ...
	 * Actually, it contains, according to RFC2478:
	 * NegotiationToken ::= CHOICE {
	 *          negTokenInit [0] NegTokenInit,
	 *          negTokenTarg [1] NegTokenTarg }
	 * NegTokenInit ::= SEQUENCE {
	 *          mechTypes [0] MechTypeList OPTIONAL,
	 *          reqFlags [1] ContextFlags OPTIONAL,
	 *          mechToken [2] OCTET STRING OPTIONAL,
	 *          mechListMIC [3] OCTET STRING OPTIONAL }
	 * NegTokenTarg ::= SEQUENCE {
	 *          negResult [0] ENUMERATED {
	 *              accept_completed (0),
	 *              accept_incomplete (1),
	 *              reject (2) } OPTIONAL,
	 *          supportedMech [1] MechType OPTIONAL,
	 *          responseToken [2] OCTET STRING OPTIONAL,
	 *          mechListMIC [3] OCTET STRING OPTIONAL }
	 *
	 * Windows typically includes mechTypes and mechListMic ('NONE'
	 * in the case of NTLMSSP only).
	 * It seems to duplicate the responseToken into the mechListMic field
	 * as well. Naughty, naughty.
	 *
	 */
	offset = dissect_spnego_NegotiationToken(FALSE, tvb, offset, &asn1_ctx, subtree, -1);

}

/*--- proto_register_spnego -------------------------------------------*/
void proto_register_spnego(void) {

	/* List of fields */
	static hf_register_info hf[] = {
		{ &hf_spnego,
		  { "SPNEGO", "spnego", FT_NONE, BASE_NONE, NULL, 0x0,
		    NULL, HFILL }},
		{ &hf_spnego_wraptoken,
		  { "wrapToken", "spnego.wraptoken",
		    FT_NONE, BASE_NONE, NULL, 0x0, "SPNEGO wrapToken",
		    HFILL}},
		{ &hf_spnego_krb5,
		  { "krb5_blob", "spnego.krb5.blob", FT_BYTES,
		    BASE_NONE, NULL, 0, NULL, HFILL }},
		{ &hf_spnego_krb5_oid,
		  { "KRB5 OID", "spnego.krb5_oid", FT_STRING,
		    BASE_NONE, NULL, 0, NULL, HFILL }},
		{ &hf_spnego_krb5_tok_id,
		  { "krb5_tok_id", "spnego.krb5.tok_id", FT_UINT16, BASE_HEX,
		    VALS(spnego_krb5_tok_id_vals), 0, "KRB5 Token Id", HFILL}},
		{ &hf_spnego_krb5_sgn_alg,
		  { "krb5_sgn_alg", "spnego.krb5.sgn_alg", FT_UINT16, BASE_HEX,
		    VALS(spnego_krb5_sgn_alg_vals), 0, "KRB5 Signing Algorithm", HFILL}},
		{ &hf_spnego_krb5_seal_alg,
		  { "krb5_seal_alg", "spnego.krb5.seal_alg", FT_UINT16, BASE_HEX,
		    VALS(spnego_krb5_seal_alg_vals), 0, "KRB5 Sealing Algorithm", HFILL}},
		{ &hf_spnego_krb5_snd_seq,
		  { "krb5_snd_seq", "spnego.krb5.snd_seq", FT_BYTES, BASE_NONE,
		    NULL, 0, "KRB5 Encrypted Sequence Number", HFILL}},
		{ &hf_spnego_krb5_sgn_cksum,
		  { "krb5_sgn_cksum", "spnego.krb5.sgn_cksum", FT_BYTES, BASE_NONE,
		    NULL, 0, "KRB5 Data Checksum", HFILL}},
		{ &hf_spnego_krb5_confounder,
		  { "krb5_confounder", "spnego.krb5.confounder", FT_BYTES, BASE_NONE,
		    NULL, 0, "KRB5 Confounder", HFILL}},
		{ &hf_spnego_krb5_filler,
		  { "krb5_filler", "spnego.krb5.filler", FT_BYTES, BASE_NONE,
		    NULL, 0, "KRB5 Filler", HFILL}},
		{ &hf_spnego_krb5_cfx_flags,
		  { "krb5_cfx_flags", "spnego.krb5.cfx_flags", FT_UINT8, BASE_HEX,
		    NULL, 0, "KRB5 CFX Flags", HFILL}},
		{ &hf_spnego_krb5_cfx_flags_01,
		  { "SendByAcceptor", "spnego.krb5.send_by_acceptor", FT_BOOLEAN, 8,
		    TFS (&tfs_set_notset), 0x01, NULL, HFILL}},
		{ &hf_spnego_krb5_cfx_flags_02,
		  { "Sealed", "spnego.krb5.sealed", FT_BOOLEAN, 8,
		    TFS (&tfs_set_notset), 0x02, NULL, HFILL}},
		{ &hf_spnego_krb5_cfx_flags_04,
		  { "AcceptorSubkey", "spnego.krb5.acceptor_subkey", FT_BOOLEAN, 8,
		    TFS (&tfs_set_notset), 0x04, NULL, HFILL}},
		{ &hf_spnego_krb5_cfx_ec,
		  { "krb5_cfx_ec", "spnego.krb5.cfx_ec", FT_UINT16, BASE_DEC,
		    NULL, 0, "KRB5 CFX Extra Count", HFILL}},
		{ &hf_spnego_krb5_cfx_rrc,
		  { "krb5_cfx_rrc", "spnego.krb5.cfx_rrc", FT_UINT16, BASE_DEC,
		    NULL, 0, "KRB5 CFX Right Rotation Count", HFILL}},
		{ &hf_spnego_krb5_cfx_seq,
		  { "krb5_cfx_seq", "spnego.krb5.cfx_seq", FT_UINT64, BASE_DEC,
		    NULL, 0, "KRB5 Sequence Number", HFILL}},


/*--- Included file: packet-spnego-hfarr.c ---*/
#line 1 "packet-spnego-hfarr.c"
    { &hf_spnego_negTokenInit,
      { "negTokenInit", "spnego.negTokenInit",
        FT_NONE, BASE_NONE, NULL, 0,
        "spnego.NegTokenInit", HFILL }},
    { &hf_spnego_negTokenTarg,
      { "negTokenTarg", "spnego.negTokenTarg",
        FT_NONE, BASE_NONE, NULL, 0,
        "spnego.NegTokenTarg", HFILL }},
    { &hf_spnego_MechTypeList_item,
      { "MechType", "spnego.MechType",
        FT_OID, BASE_NONE, NULL, 0,
        "spnego.MechType", HFILL }},
    { &hf_spnego_principal,
      { "principal", "spnego.principal",
        FT_STRING, BASE_NONE, NULL, 0,
        "spnego.GeneralString", HFILL }},
    { &hf_spnego_mechTypes,
      { "mechTypes", "spnego.mechTypes",
        FT_UINT32, BASE_DEC, NULL, 0,
        "spnego.MechTypeList", HFILL }},
    { &hf_spnego_reqFlags,
      { "reqFlags", "spnego.reqFlags",
        FT_BYTES, BASE_NONE, NULL, 0,
        "spnego.ContextFlags", HFILL }},
    { &hf_spnego_mechToken,
      { "mechToken", "spnego.mechToken",
        FT_BYTES, BASE_NONE, NULL, 0,
        "spnego.T_mechToken", HFILL }},
    { &hf_spnego_negTokenInit_mechListMIC,
      { "mechListMIC", "spnego.mechListMIC",
        FT_BYTES, BASE_NONE, NULL, 0,
        "spnego.T_NegTokenInit_mechListMIC", HFILL }},
    { &hf_spnego_negResult,
      { "negResult", "spnego.negResult",
        FT_UINT32, BASE_DEC, VALS(spnego_T_negResult_vals), 0,
        "spnego.T_negResult", HFILL }},
    { &hf_spnego_supportedMech,
      { "supportedMech", "spnego.supportedMech",
        FT_OID, BASE_NONE, NULL, 0,
        "spnego.T_supportedMech", HFILL }},
    { &hf_spnego_responseToken,
      { "responseToken", "spnego.responseToken",
        FT_BYTES, BASE_NONE, NULL, 0,
        "spnego.T_responseToken", HFILL }},
    { &hf_spnego_mechListMIC,
      { "mechListMIC", "spnego.mechListMIC",
        FT_BYTES, BASE_NONE, NULL, 0,
        "spnego.T_mechListMIC", HFILL }},
    { &hf_spnego_thisMech,
      { "thisMech", "spnego.thisMech",
        FT_OID, BASE_NONE, NULL, 0,
        "spnego.MechType", HFILL }},
    { &hf_spnego_innerContextToken,
      { "innerContextToken", "spnego.innerContextToken",
        FT_NONE, BASE_NONE, NULL, 0,
        "spnego.InnerContextToken", HFILL }},
    { &hf_spnego_ContextFlags_delegFlag,
      { "delegFlag", "spnego.delegFlag",
        FT_BOOLEAN, 8, NULL, 0x80,
        NULL, HFILL }},
    { &hf_spnego_ContextFlags_mutualFlag,
      { "mutualFlag", "spnego.mutualFlag",
        FT_BOOLEAN, 8, NULL, 0x40,
        NULL, HFILL }},
    { &hf_spnego_ContextFlags_replayFlag,
      { "replayFlag", "spnego.replayFlag",
        FT_BOOLEAN, 8, NULL, 0x20,
        NULL, HFILL }},
    { &hf_spnego_ContextFlags_sequenceFlag,
      { "sequenceFlag", "spnego.sequenceFlag",
        FT_BOOLEAN, 8, NULL, 0x10,
        NULL, HFILL }},
    { &hf_spnego_ContextFlags_anonFlag,
      { "anonFlag", "spnego.anonFlag",
        FT_BOOLEAN, 8, NULL, 0x08,
        NULL, HFILL }},
    { &hf_spnego_ContextFlags_confFlag,
      { "confFlag", "spnego.confFlag",
        FT_BOOLEAN, 8, NULL, 0x04,
        NULL, HFILL }},
    { &hf_spnego_ContextFlags_integFlag,
      { "integFlag", "spnego.integFlag",
        FT_BOOLEAN, 8, NULL, 0x02,
        NULL, HFILL }},

/*--- End of included file: packet-spnego-hfarr.c ---*/
#line 1379 "packet-spnego-template.c"
	};

	/* List of subtrees */
	static gint *ett[] = {
		&ett_spnego,
		&ett_spnego_wraptoken,
		&ett_spnego_krb5,
		&ett_spnego_krb5_cfx_flags,


/*--- Included file: packet-spnego-ettarr.c ---*/
#line 1 "packet-spnego-ettarr.c"
    &ett_spnego_NegotiationToken,
    &ett_spnego_MechTypeList,
    &ett_spnego_PrincipalSeq,
    &ett_spnego_NegTokenInit,
    &ett_spnego_ContextFlags,
    &ett_spnego_NegTokenTarg,
    &ett_spnego_InitialContextToken_U,

/*--- End of included file: packet-spnego-ettarr.c ---*/
#line 1389 "packet-spnego-template.c"
	};

	/* Register protocol */
	proto_spnego = proto_register_protocol(PNAME, PSNAME, PFNAME);

	register_dissector("spnego", dissect_spnego, proto_spnego);

	proto_spnego_krb5 = proto_register_protocol("SPNEGO-KRB5",
						    "SPNEGO-KRB5",
						    "spnego-krb5");

	register_dissector("spnego-krb5", dissect_spnego_krb5, proto_spnego_krb5);
	new_register_dissector("spnego-krb5-wrap", dissect_spnego_krb5_wrap, proto_spnego_krb5);

	/* Register fields and subtrees */
	proto_register_field_array(proto_spnego, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));
}


/*--- proto_reg_handoff_spnego ---------------------------------------*/
void proto_reg_handoff_spnego(void) {

	dissector_handle_t spnego_handle, spnego_wrap_handle;
	dissector_handle_t spnego_krb5_handle, spnego_krb5_wrap_handle;

	/* Register protocol with GSS-API module */

        spnego_handle = find_dissector("spnego");
	spnego_wrap_handle = new_create_dissector_handle(dissect_spnego_wrap,  proto_spnego);
	gssapi_init_oid("1.3.6.1.5.5.2", proto_spnego, ett_spnego,
	    spnego_handle, spnego_wrap_handle,
	    "SPNEGO - Simple Protected Negotiation");

	/* Register both the one MS created and the real one */
	/*
	 * Thanks to Jean-Baptiste Marchand and Richard B Ward, the
	 * mystery of the MS KRB5 OID is cleared up. It was due to a library
	 * that did not handle OID components greater than 16 bits, and was
	 * fixed in Win2K SP2 as well as WinXP.
	 * See the archive of <ietf-krb-wg@anl.gov> for the thread topic
	 * SPNEGO implementation issues. 3-Dec-2002.
	 */
        spnego_krb5_handle = find_dissector("spnego-krb5");
	spnego_krb5_wrap_handle = find_dissector("spnego-krb5-wrap");
	gssapi_init_oid("1.2.840.48018.1.2.2", proto_spnego_krb5, ett_spnego_krb5,
			spnego_krb5_handle, spnego_krb5_wrap_handle,
			"MS KRB5 - Microsoft Kerberos 5");
	gssapi_init_oid("1.2.840.113554.1.2.2", proto_spnego_krb5, ett_spnego_krb5,
			spnego_krb5_handle, spnego_krb5_wrap_handle,
			"KRB5 - Kerberos 5");
	gssapi_init_oid("1.2.840.113554.1.2.2.3", proto_spnego_krb5, ett_spnego_krb5,
			spnego_krb5_handle, spnego_krb5_wrap_handle,
			"KRB5 - Kerberos 5 - User to User");

}