/*
 * ScebXCe's document: its initial state and the node accessors, for both of its forms. In the bX
 * form a node is its token byte in the node table, then its operands; a string in a table is
 * its encoded length, then its bytes and a NUL.
 */
#include <xml/xml_node_types.h>
#include "bxce_internal.h"

/* PLACEHOLDER: what the object ScebXCe_578CC576 returns is not known yet */
static const int s_bxceObject = 0;

/* the size of an encoded length from its first byte */
static inline int bxceLengthSize(int lead)
{
	if (lead <= 0xEF)
		return 1;

	switch (lead) {
	case 0xF0:
		return 3;
	case 0xF1:
	case 0xF4:
		return 4;
	default:
		return 1;
	}
}

/* a value stored most significant byte first, put together byte by byte */
typedef union BxceBytes {
	SceUInt32 value;
	SceUInt8 byte[4];
} BxceBytes;

/* a string of a table, past its encoded length */
static inline const char *bxceString(const char *table, int offset)
{
	const SceUInt8 *s = (const SceUInt8 *)table + offset;

	return (const char *)s + bxceLengthSize(*s);
}

const int *ScebXCe_578CC576(void)
{
	return &s_bxceObject;
}

BxceDocument *ScebXCe_63593407(BxceDocument *doc)
{
	doc->isExternal = 0;
	doc->nodes = NULL;
	doc->texts = NULL;
	doc->elementNames = NULL;
	doc->attrNames = NULL;
	doc->field20 = 0;
	doc->attrNameHash = NULL;
	doc->elementNameHash = NULL;
	doc->field32 = 0;
	doc->field36 = 0;
	doc->field40 = 0;
	doc->field44 = 0;
	doc->isBinary = 1;
	doc->field48 = 0;
	doc->field52 = 1;
	doc->elementNamesSize = 0;
	doc->attrNamesSize = 0;
	doc->field1092 = 0;
	doc->field1112 = 0;
	doc->field1120 = 0;
	doc->domRoot = NULL;
	return doc;
}

void ScebXCe_6E809E76(void)
{
}

BxceNodeId ScebXCe_6FC9DC3F(const BxceDocument *doc, BxceNodeId node)
{
	const SceUInt8 *u;
	const char *p;
	BxceBytes distance;
	int token;

	if (node == 0)
		return 0;
	if (!doc->isBinary)
		return (BxceNodeId)((const BxceDomNode *)node)->sibling;

	p = doc->nodes + node;
	token = *p;
	if ((token & SCE_XML_TOKEN_DOM_HAS_SIBLING) == 0)
		return 0;

	switch (token & 0x2F) {
	case SCE_XML_TOKEN_DOM_ENT_SIB:
	case SCE_XML_TOKEN_DOM_SKIP_TEXT_SIB_CDATA:
	case SCE_XML_TOKEN_DOM_HAS_TEXT_SIBLING:
	case SCE_XML_TOKEN_DOM_SKIP_TEXT_SIB_DTD:
	case SCE_XML_TOKEN_DOM_SKIP_TEXT_SIB_PI:
		/* the sibling follows the node */
		return node + 2 + bxceLengthSize(((const SceUInt8 *)p)[1]);
	case SCE_XML_TOKEN_DOM_ELEMENT_SIB:
	case SCE_XML_TOKEN_DOM_ELEM_SIBL:
	case SCE_XML_TOKEN_DOM_ELEMENT_SIB_CHILD:
	case SCE_XML_TOKEN_DOM_ELEM_SIBL_CHILD:
	case SCE_XML_TOKEN_DOM_ELEMENT_ATTR_SIB:
	case SCE_XML_TOKEN_DOM_ELEMENT_ATTR_SIB_CHILD:
	case SCE_XML_TOKEN_DOM_RWELEMENT_ATTR_SIB_CHILD:
		/* an element gives its sibling's distance */
		u = (const SceUInt8 *)p;
		distance.byte[0] = u[6];
		distance.byte[1] = u[5];
		distance.byte[2] = u[4];
		distance.byte[3] = u[3];
		return node + 3 + distance.value;
	default:
		return 0;
	}
}

/* the document element: the first node after the leading skipped texts */
BxceNodeId ScebXCe_CED6BECF(const BxceDocument *doc)
{
	const char *p;

	if (!doc->isBinary) {
		if (doc->domRoot != NULL)
			return (BxceNodeId)doc->domRoot->child;
		return 0;
	}

	if (doc->nodes == NULL)
		return 0;

	p = doc->nodes + 2;
	while ((*p & 0xF) == SCE_XML_TOKEN_DOM_TEXT_SKIPPED ||
	       (*p & 7) == SCE_XML_TOKEN_DOM_SKIP_TEXT_CDATA)
		p += 1 + bxceLengthSize((SceUInt8)p[1]);

	return p - doc->nodes;
}

BxceNodeId ScebXCe_FEA6E4A1(const BxceDocument *doc, BxceNodeId node)
{
	const char *p;
	int token;

	if (node == 0)
		return 0;
	if (!doc->isBinary)
		return (BxceNodeId)((const BxceDomNode *)node)->child;

	p = doc->nodes + node;
	token = *p;
	if ((token & SCE_XML_TOKEN_DOM_HAS_CHILD) == 0)
		return 0;

	switch (token & 0xF) {
	case SCE_XML_TOKEN_DOM_ELEMENT:
	case SCE_XML_TOKEN_DOM_COMPACT_ELEMENT:
	case SCE_XML_TOKEN_DOM_PRED_ELEMENT:
		/* the first child follows the element's attributes */
		return node + 7 + (SceUInt8)p[7];
	}

	if (token == SCE_XML_TOKEN_DOM_ROOT_DOCUMENT)
		return ScebXCe_CED6BECF(doc);
	return 0;
}

int bxceReadLength(const SceUInt8 *p)
{
	BxceBytes length;

	if (p[0] <= 0xEF)
		return p[0];

	switch (p[0]) {
	case 0xF0:
		length.value = p[2];
		length.byte[1] = p[1];
		break;
	case 0xF1:
	case 0xF4:
		length.byte[0] = p[3];
		length.byte[1] = p[2];
		length.byte[2] = p[1];
		length.byte[3] = 0;
		break;
	case 0xFF:
		length.value = 0;
		break;
	default:
		length.value = 1;
		break;
	}

	return length.value;
}

const char *ScebXCe_B4989A58(const BxceDocument *doc, BxceNodeId node)
{
	const char *p;

	if (node == 0)
		return NULL;

	if (doc->isBinary) {
		p = doc->nodes + node;
		if ((*p & 0xF) != SCE_XML_TOKEN_DOM_ATTR)
			return NULL;
		if ((*p & SCE_XML_TOKEN_DOM_CONTINUE) == 0)
			return doc->attrNames + bxceReadLength((const SceUInt8 *)p + 1);
		return doc->attrNames + bxceReadLength((const SceUInt8 *)p + 2);
	}

	p = (const char *)node;
	if ((*p & 0xF) != SCE_XML_TOKEN_DOM_ATTR)
		return NULL;
	if ((*p & SCE_XML_TOKEN_DOM_CONTINUE) != 0)
		return doc->attrNames + bxceReadLength((const SceUInt8 *)p + 2);
	return doc->attrNames + bxceReadLength((const SceUInt8 *)p + 1);
}

const char *ScebXCe_2FAD3DFA(const BxceDocument *doc, BxceNodeId node)
{
	const char *p;
	BxceBytes offset;
	SceUInt32 text;

	if (node == 0)
		return NULL;

	if (!doc->isBinary) {
		/* the length's size is taken from the offset itself, as in Sony's code */
		text = ((const BxceDomNode *)node)->text;
		return doc->texts + text + bxceLengthSize((SceUInt8)text);
	}

	p = doc->nodes + node;
	if ((*p & 0xF) != SCE_XML_TOKEN_DOM_TEXT_SKIPPED &&
	    (*p & 7) != SCE_XML_TOKEN_DOM_SKIP_TEXT_CDATA)
		return NULL;

	offset.value = bxceReadLength((const SceUInt8 *)p + 1);
	return bxceString(doc->texts, offset.value);
}

const char *ScebXCe_33825EFF(const BxceDocument *doc, BxceNodeId node)
{
	const BxceDomNode *dn;
	const char *p;
	BxceBytes offset;
	SceUInt32 text;

	if (node == 0)
		return NULL;

	if (!doc->isBinary) {
		dn = (const BxceDomNode *)node;
		if ((dn->info & 0xF) != SCE_XML_TOKEN_DOM_TEXT)
			return NULL;

		/* the offset, small or in the bytes of an encoded length */
		text = dn->text;
		if (text > 0xEF) {
			if ((SceUInt8)text == 0xF0) {
				offset.value = (SceUInt8)(text >> 16);
				offset.byte[1] = (SceUInt8)(text >> 8);
				return bxceString(doc->texts, offset.value);
			}
			offset.byte[0] = (SceUInt8)(text >> 24);
			offset.byte[1] = (SceUInt8)(text >> 16);
			offset.byte[2] = (SceUInt8)(text >> 8);
			offset.byte[3] = 0;
			return bxceString(doc->texts, offset.value);
		}
		return bxceString(doc->texts, text);
	}

	p = doc->nodes + node;
	if ((*p & 0xDF) != SCE_XML_TOKEN_DOM_TEXT)	/* a text, with or without a sibling */
		return NULL;
	offset.value = bxceReadLength((const SceUInt8 *)p + 1);
	return bxceString(doc->texts, offset.value);
}

const char *ScebXCe_1189A2B1(const BxceDocument *doc, BxceNodeId node)
{
	const char *p;
	const char *name;
	SceUInt32 info;

	if (node == 0)
		return NULL;

	if (!doc->isBinary) {
		info = ((const BxceDomNode *)node)->info;
		switch (info & 0xF) {
		case SCE_XML_TOKEN_DOM_PRED_ELEMENT:
		case SCE_XML_TOKEN_DOM_TEXT_BOOL:
		case SCE_XML_TOKEN_DOM_TEXT_SHORT:
		case SCE_XML_TOKEN_DOM_TEXT_INT:
		case SCE_XML_TOKEN_DOM_TEXT_LONG:
		case SCE_XML_TOKEN_DOM_TEXT_FLOAT:
		case SCE_XML_TOKEN_DOM_TEXT_USER_DATA:
			return NULL;
		case SCE_XML_TOKEN_DOM_ELEMENT:
		case SCE_XML_TOKEN_DOM_COMPACT_ELEMENT:
			name = doc->elementNames + (info >> 8);
			break;
		case SCE_XML_TOKEN_DOM_ELEMENT_ATTR:
			/* never taken: the type is masked to four bits (so in Sony's code) */
			return ScebXCe_B4989A58(doc, node);
		case SCE_XML_TOKEN_DOM_SKIP_TEXT_CDATA:
			return ScebXCe_2FAD3DFA(doc, node);
		case SCE_XML_TOKEN_DOM_TEXT:
			return ScebXCe_33825EFF(doc, node);
		case SCE_XML_TOKEN_DOM_TEXT_SKIPPED:
			name = NULL;		/* the DOM form keeps no skipped text's name */
			break;
		default:
			return NULL;
		}
		return name;
	}

	p = doc->nodes + node;
	switch (*p & 0xF) {
	case SCE_XML_TOKEN_DOM_PRED_ELEMENT:
	case SCE_XML_TOKEN_DOM_TEXT_BOOL:
	case SCE_XML_TOKEN_DOM_TEXT_SHORT:
	case SCE_XML_TOKEN_DOM_TEXT_INT:
	case SCE_XML_TOKEN_DOM_TEXT_LONG:
	case SCE_XML_TOKEN_DOM_TEXT_FLOAT:
	case SCE_XML_TOKEN_DOM_TEXT_USER_DATA:
		return NULL;
	case SCE_XML_TOKEN_DOM_ELEMENT_ATTR:
		return ScebXCe_B4989A58(doc, node);	/* never taken, as above */
	case SCE_XML_TOKEN_DOM_ELEMENT:
	case SCE_XML_TOKEN_DOM_COMPACT_ELEMENT:
		return doc->elementNames + (((const SceUInt8 *)p)[2] | ((const SceUInt8 *)p)[1] << 8);
	case SCE_XML_TOKEN_DOM_SKIP_TEXT_CDATA:
		return ScebXCe_2FAD3DFA(doc, node);
	case SCE_XML_TOKEN_DOM_TEXT:
		return ScebXCe_33825EFF(doc, node);
	case SCE_XML_TOKEN_DOM_TEXT_SKIPPED:
		return doc->texts + bxceReadLength((const SceUInt8 *)p + 1) + 1;
	}
	return NULL;
}

int ScebXCe_FA18A039(const BxceDocument *doc, BxceNodeId node)
{
	SceUInt32 token;

	if (doc->isBinary)
		token = doc->nodes[node];
	else
		token = ((const BxceDomNode *)node)->info;

	switch (token & 0xF) {
	case SCE_XML_TOKEN_DOM_SKIP_TEXT_CDATA:
		return (token & SCE_XML_TOKEN_DOM_HAS_CHILD) ? SCE_XML_TOKEN_DOM_SKIP_TEXT_PI :
			SCE_XML_TOKEN_DOM_SKIP_TEXT_CDATA;
	case SCE_XML_TOKEN_DOM_TEXT_SKIPPED:
		return (token & SCE_XML_TOKEN_DOM_HAS_CHILD) ? SCE_XML_TOKEN_DOM_SKIP_TEXT_COMMENT :
			SCE_XML_TOKEN_DOM_TEXT_SKIPPED;
	default:
		return token & 0xF;
	}
}
