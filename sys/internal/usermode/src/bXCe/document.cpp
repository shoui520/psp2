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
	doc->status = 0;
	doc->attrNameHash = NULL;
	doc->elementNameHash = NULL;
	doc->nodesCapacity = 0;
	doc->textsCapacity = 0;
	doc->elementNamesCapacity = 0;
	doc->attrNamesCapacity = 0;
	doc->isBinary = 1;
	doc->nodesSize = 0;
	doc->textsSize = 1;
	doc->elementNamesSize = 0;
	doc->attrNamesSize = 0;
	doc->pendingAttrCount = 0;
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

/*
 * The elements under the node (and the node) named so, or all of them for "*", in document
 * order, appended to the list (a new one when none is given). NULL when out of memory; a list
 * the recursion could not fill is terminated, a new one not freed.
 */
BxceNodeList *bxceGetElementsByTagName(const BxceDocument *doc, BxceNodeId node,
					const char *name, BxceNodeList *list)
{
	const char *nodeName;
	BxceNodeId child;
	int type;

	if (list == NULL) {
		list = (BxceNodeList *)ScebXCe_B8DDE741(sizeof(BxceNodeList));
		if (list == NULL)
			return NULL;
		bxceNodeListInitialize(list, doc);
	}

	type = ScebXCe_FA18A039(doc, node);
	if (type == SCE_XML_TOKEN_DOM_ELEMENT || type == SCE_XML_TOKEN_DOM_COMPACT_ELEMENT ||
	    type == SCE_XML_TOKEN_DOM_PRED_ELEMENT) {
		nodeName = ScebXCe_1189A2B1(doc, node);
		if ((nodeName != NULL && bxceStrncmpTag(nodeName, name, BXCE_ATTR_STRING_MAX) == 0) ||
		    bxceStrncmpTag(name, "*", BXCE_ATTR_STRING_MAX) == 0)
			bxceNodeListInsertLast(list, node);
	}

	for (child = ScebXCe_FEA6E4A1(doc, node); child != 0;
	     child = ScebXCe_6FC9DC3F(doc, child)) {
		if (bxceGetElementsByTagName(doc, child, name, list) == NULL) {
			ScebXCe_1F59F0C4(list);
			return NULL;
		}
	}

	return list;
}

/* the tables a document builds in, each empty and owned by it; NoMemory frees what was made */
int bxceDocumentAllocateTables(BxceDocument *doc)
{
	doc->field1112 = 0;

	doc->elementNames = (char *)ScebXCe_B8DDE741(BXCE_NAMES_INITIAL_SIZE);
	if (doc->elementNames == NULL) {
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return SCE_BXCE_ERROR_NO_MEMORY;
	}
	bxceMemset(doc->elementNames, 0, BXCE_NAMES_INITIAL_SIZE);
	doc->elementNamesCapacity = BXCE_NAMES_INITIAL_SIZE;

	doc->texts = (char *)ScebXCe_B8DDE741(BXCE_TABLE_INITIAL_SIZE);
	if (doc->texts == NULL) {
		if (doc->elementNames != NULL)
			ScebXCe_EC560647(doc->elementNames);
		doc->elementNames = NULL;
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return SCE_BXCE_ERROR_NO_MEMORY;
	}
	bxceMemset(doc->texts, 0, BXCE_TABLE_INITIAL_SIZE);
	doc->textsCapacity = BXCE_TABLE_INITIAL_SIZE;

	doc->attrNames = (char *)ScebXCe_B8DDE741(BXCE_NAMES_INITIAL_SIZE);
	if (doc->attrNames == NULL) {
		if (doc->texts != NULL)
			ScebXCe_EC560647(doc->texts);
		doc->texts = NULL;
		if (doc->elementNames != NULL)
			ScebXCe_EC560647(doc->elementNames);
		doc->elementNames = NULL;
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return SCE_BXCE_ERROR_NO_MEMORY;
	}
	bxceMemset(doc->attrNames, 0, BXCE_NAMES_INITIAL_SIZE);
	doc->attrNamesCapacity = BXCE_NAMES_INITIAL_SIZE;

	doc->elementNameHash = (BxceNameHash *)ScebXCe_B8DDE741(sizeof(BxceNameHash));
	if (doc->elementNameHash == NULL) {
		if (doc->texts != NULL)
			ScebXCe_EC560647(doc->texts);
		doc->texts = NULL;
		if (doc->elementNames != NULL)
			ScebXCe_EC560647(doc->elementNames);
		doc->elementNames = NULL;
		if (doc->attrNames != NULL)
			ScebXCe_EC560647(doc->attrNames);
		doc->attrNames = NULL;
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return SCE_BXCE_ERROR_NO_MEMORY;
	}
	bxceNameHashInitialize(doc->elementNameHash);

	doc->attrNameHash = (BxceNameHash *)ScebXCe_B8DDE741(sizeof(BxceNameHash));
	if (doc->attrNameHash == NULL) {
		bxceNameHashClear(doc->elementNameHash);
		if (doc->elementNameHash != NULL)
			ScebXCe_EC560647(doc->elementNameHash);
		doc->elementNameHash = NULL;
		if (doc->texts != NULL)
			ScebXCe_EC560647(doc->texts);
		doc->texts = NULL;
		if (doc->elementNames != NULL)
			ScebXCe_EC560647(doc->elementNames);
		doc->elementNames = NULL;
		if (doc->attrNames != NULL)
			ScebXCe_EC560647(doc->attrNames);
		doc->attrNames = NULL;
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return SCE_BXCE_ERROR_NO_MEMORY;
	}
	bxceNameHashInitialize(doc->attrNameHash);
	return 0;
}

/* an empty bX document to build: its node table holds the document node, at 1 */
int bxceDocumentInitializeWriter(BxceDocument *doc)
{
	doc->isExternal = 0;
	doc->nodes = NULL;
	doc->texts = NULL;
	doc->elementNames = NULL;
	doc->attrNames = NULL;
	doc->status = 0;
	doc->attrNameHash = NULL;
	doc->elementNameHash = NULL;
	doc->nodesCapacity = 0;
	doc->textsCapacity = 0;
	doc->elementNamesCapacity = 0;
	doc->attrNamesCapacity = 0;
	doc->isBinary = 1;
	doc->nodesSize = 0;
	doc->textsSize = 1;
	doc->elementNamesSize = 0;
	doc->attrNamesSize = 0;
	doc->pendingAttrCount = 0;
	doc->field1112 = 0;
	doc->domRoot = NULL;
	doc->field1120 = 0;

	doc->nodes = (char *)ScebXCe_B8DDE741(BXCE_TABLE_INITIAL_SIZE);
	if (doc->nodes == NULL) {
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return SCE_BXCE_ERROR_NO_MEMORY;
	}
	bxceMemset(doc->nodes, 0, BXCE_TABLE_INITIAL_SIZE);
	doc->nodesCapacity = BXCE_TABLE_INITIAL_SIZE;
	doc->nodes[doc->nodesSize + 1] = SCE_XML_TOKEN_DOM_ROOT_DOCUMENT;
	doc->nodesSize += 2;
	return bxceDocumentAllocateTables(doc);
}

/*
 * Sets *parent to the node under which target is (target itself when it is the node), searching
 * from node down; *found tells whether it was found. A sibling that is the target is taken even
 * after the search below found it.
 */
void bxceFindParent(const BxceDocument *doc, BxceNodeId node, BxceNodeId target, int *found,
		    BxceNodeId *parent)
{
	BxceNodeId child;

	if (*found != 0)
		return;

	if (node == target) {
		*parent = node;
		*found = 1;
		return;
	}

	child = ScebXCe_FEA6E4A1(doc, node);
	if (child == target) {
		*parent = node;
		*found = 1;
		return;
	}

	while (child != 0) {
		bxceFindParent(doc, child, target, found, parent);
		child = ScebXCe_6FC9DC3F(doc, child);
		if (child == target) {
			*parent = node;
			*found = 1;
			return;
		}
		if (*found != 0)
			return;
	}

	if (*found == 0)
		*parent = 0;
}

/* writes an encoded length at *pos of the table and moves *pos past it */
void bxceWriteLength(BxceDocument *doc, SceUInt32 value, char *table, SceUInt32 *pos)
{
	if (value <= 0xEF) {
		table[*pos] = value;
		*pos += 1;
	} else if (value <= 0xFFFF) {
		table[*pos] = 0xF0;
		*pos += 1;
		table[*pos + 1] = value;
		table[*pos] = value >> 8;
		*pos += 2;
	} else {
		table[*pos] = 0xF1;
		*pos += 1;
		table[*pos + 2] = value;
		table[*pos + 1] = value >> 8;
		table[*pos] = value >> 16;
		*pos += 3;
	}
}

/* appends a string to the text table */
int bxceDocumentAddText(BxceDocument *doc, const char *text, SceUInt32 length)
{
	SceUInt32 size;

	size = doc->textsSize + length;
	if (size + 5 >= BXCE_TABLE_LIMIT) {
		doc->status = SCE_BXCE_ERROR_EXCEED_LIMIT;
		return SCE_BXCE_ERROR_EXCEED_LIMIT;
	}

	if (doc->textsCapacity <= size + 10) {
		doc->texts = bxceGrowTable(doc->texts, size + 10, &doc->textsCapacity);
		if (doc->texts == NULL) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return SCE_BXCE_ERROR_NO_MEMORY;
		}
	}

	bxceWriteLength(doc, length, doc->texts, &doc->textsSize);
	bxceMemcpy(doc->texts + doc->textsSize, text, length);
	doc->texts[doc->textsSize + length] = 0;
	doc->textsSize += length + 1;
	return 0;
}

/*
 * Writes an attribute node at *pos of the node table: its name offset and value position pairs,
 * the count first when there are several. The table is grown for one pair only.
 */
int bxceDocumentWriteAttributes(BxceDocument *doc, const SceUInt32 *attrs, SceUInt32 count,
				char *nodes, SceUInt32 *pos, int reserved)
{
	SceUInt32 start, i;

	if (doc->isBinary && doc->nodesCapacity <= doc->nodesSize + count * 12) {
		nodes = doc->nodes = bxceGrowTable(doc->nodes, doc->nodesSize + 12,
						   &doc->nodesCapacity);
		if (nodes == NULL) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return SCE_BXCE_ERROR_NO_MEMORY;
		}
	}

	start = *pos;
	doc->attrNode = start;
	nodes[start] = 0;
	nodes[start] |= SCE_XML_TOKEN_DOM_ATTR;
	*pos += 1;

	if (count > 1) {
		nodes[start] |= SCE_XML_TOKEN_DOM_ATTR | SCE_XML_TOKEN_DOM_HAS_SIBLING;
		nodes[*pos] = count;
		*pos += 1;
		bxceWriteLength(doc, attrs[0], nodes, pos);
		bxceWriteLength(doc, attrs[1], nodes, pos);
	} else {
		nodes[*pos] = attrs[0];
		*pos += 1;
		bxceWriteLength(doc, attrs[1], nodes, pos);
	}

	for (i = 2; i < count * 2; i += 2) {
		nodes[*pos] = SCE_XML_TOKEN_DOM_ATTR;
		*pos += 1;
		bxceWriteLength(doc, attrs[i], nodes, pos);
		bxceWriteLength(doc, attrs[i + 1], nodes, pos);
	}

	return 0;
}

/*
 * Appends an element under parent: after its last child, or as its first child (writing the
 * attributes waiting for the parent first). Its own attributes wait for its first child. The
 * node, or 0 on an error.
 */
BxceNodeId bxceDocumentAppendElement(BxceDocument *doc, BxceNodeId parent, const char *name,
				     int length, const BxceAttributeList *attrs)
{
	const BxceAttr *attr;
	char *nodes;
	int nameOffset, offset, i, len;
	BxceNodeId prev, next;
	SceUInt32 start, off, pos;
	char token;

	if (doc->nodesCapacity <= doc->nodesSize + 12) {
		doc->nodes = bxceGrowTable(doc->nodes, doc->nodesSize + 12, &doc->nodesCapacity);
		if (doc->nodes == NULL) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return 0;
		}
	}

	if (doc->elementNamesCapacity <= doc->elementNamesSize + length + 5) {
		doc->elementNames = bxceGrowTable(doc->elementNames,
						  doc->elementNamesSize + length + 5,
						  &doc->elementNamesCapacity);
		if (doc->elementNames == NULL) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return 0;
		}
	}

	if (length > BXCE_NAME_MAX) {
		doc->status = SCE_BXCE_ERROR_EXCEED_LIMIT;
		length = BXCE_NAME_MAX;
	}

	nameOffset = bxceNameHashIntern(doc->elementNameHash, name, length, doc->elementNames,
					&doc->elementNamesSize);
	if (nameOffset == -1) {
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return 0;
	}

	token = doc->nodes[parent];
	if (token != SCE_XML_TOKEN_DOM_ROOT_DOCUMENT) {
		if (token & SCE_XML_TOKEN_DOM_HAS_CHILD) {
			prev = next = ScebXCe_FEA6E4A1(doc, parent);
			while (next != 0) {
				prev = next;
				next = ScebXCe_6FC9DC3F(doc, prev);
			}
			if (((doc->nodes[prev] |= SCE_XML_TOKEN_DOM_HAS_SIBLING) & 0xF) ==
			    SCE_XML_TOKEN_DOM_ELEMENT) {
				doc->nodes[prev + 6] = doc->nodesSize - prev - 3;
				doc->nodes[prev + 5] = ((doc->nodesSize - prev - 3) >> 8) & 0xFF;
				doc->nodes[prev + 4] = ((doc->nodesSize - prev - 3) >> 16) & 0xFF;
				doc->nodes[prev + 3] = (doc->nodesSize - prev - 3) >> 24;
			} else {
				doc->nodes[doc->nodesSize] = SCE_XML_TOKEN_DOM_ATTR;
				doc->nodesSize += 1;
			}
		} else {
			doc->nodes[parent] = token | SCE_XML_TOKEN_DOM_HAS_CHILD;
			start = doc->nodesSize;
			doc->nodesSize = start + 1;
			if (doc->pendingAttrCount > 0) {
				if (bxceDocumentWriteAttributes(doc, doc->pendingAttr,
								doc->pendingAttrCount, doc->nodes,
								&doc->nodesSize, 0) != 0)
					return 0;
				doc->pendingAttrCount = 0;
			}
			len = doc->nodesSize - start;
			off = start + 1;
			if (len <= 0xEF) {
				doc->nodes[start] = len;
			} else if (len <= 0xFFFF) {
				doc->nodes[start] = 0xF0;
				doc->nodes[off + 1] = len;
				doc->nodes[start + 1] = (len >> 8) & 0xFF;
			} else {
				doc->nodes[start] = 0xF1;
				doc->nodes[off + 2] = len;
				doc->nodes[off + 1] = (len >> 8) & 0xFF;
				doc->nodes[start + 1] = (len >> 16) & 0xFF;
			}
		}
	}

	nodes = doc->nodes;
	pos = doc->nodesSize;
	doc->nodesSize = pos + 1;
	if (nameOffset & 0xFFFF0000)
		nodes[pos] = SCE_XML_TOKEN_DOM_PRED_ELEMENT;
	else
		nodes[pos] = SCE_XML_TOKEN_DOM_ELEMENT;
	doc->nodes[doc->nodesSize + 1] = nameOffset;
	doc->nodes[doc->nodesSize] = (nameOffset >> 8) & 0xFF;
	doc->nodesSize += 6;

	doc->pendingAttrCount = attrs->length;
	if (doc->pendingAttrCount != 0)
		nodes[pos] |= SCE_XML_TOKEN_DOM_HAS_ATTR;

	for (i = 0; i < doc->pendingAttrCount; i++) {
		attr = ScebXCe_01DC4E22(attrs, i);
		if (doc->attrNamesCapacity <= doc->attrNamesSize + attr->nameLength + 5) {
			doc->attrNames = bxceGrowTable(doc->attrNames,
						       doc->attrNamesSize + attr->nameLength + 5,
						       &doc->attrNamesCapacity);
			if (doc->attrNames == NULL) {
				doc->status = SCE_BXCE_ERROR_NO_MEMORY;
				return 0;
			}
		}

		offset = bxceNameHashIntern(doc->attrNameHash, attr->name, attr->nameLength,
					    doc->attrNames, &doc->attrNamesSize);
		if (offset == -1) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return 0;
		}

		doc->pendingAttr[2 * i] = offset;
		doc->pendingAttr[2 * i + 1] = doc->textsSize;
		if (bxceDocumentAddText(doc, attr->value, attr->valueLength) != 0) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return 0;
		}
	}

	return pos;
}

/* appends a text under parent, as appendElement places a node; its text position, or 0 */
SceUInt32 bxceDocumentAppendText(BxceDocument *doc, BxceNodeId parent, const char *text,
				 SceUInt32 length)
{
	SceUInt32 pos, start, off, len;
	BxceNodeId prev, next;
	char *nodes;
	char token;

	pos = doc->textsSize;
	if (bxceDocumentAddText(doc, text, length) != 0)
		return 0;

	token = doc->nodes[parent];
	if (token & SCE_XML_TOKEN_DOM_HAS_CHILD) {
		prev = next = ScebXCe_FEA6E4A1(doc, parent);
		while (next != 0) {
			prev = next;
			next = ScebXCe_6FC9DC3F(doc, prev);
		}
		if (((doc->nodes[prev] |= SCE_XML_TOKEN_DOM_HAS_SIBLING) & 0xF) ==
		    SCE_XML_TOKEN_DOM_ELEMENT) {
			doc->nodes[prev + 6] = doc->nodesSize - prev - 3;
			doc->nodes[prev + 5] = ((doc->nodesSize - prev - 3) >> 8) & 0xFF;
			doc->nodes[prev + 4] = ((doc->nodesSize - prev - 3) >> 16) & 0xFF;
			doc->nodes[prev + 3] = (doc->nodesSize - prev - 3) >> 24;
		} else {
			doc->nodes[doc->nodesSize] = SCE_XML_TOKEN_DOM_ATTR;
			doc->nodesSize += 1;
		}
	} else {
		doc->nodes[parent] = token | SCE_XML_TOKEN_DOM_HAS_CHILD;
		start = doc->nodesSize;
		doc->nodesSize = start + 1;
		if (doc->pendingAttrCount > 0) {
			if (bxceDocumentWriteAttributes(doc, doc->pendingAttr, doc->pendingAttrCount,
							doc->nodes, &doc->nodesSize, 0) != 0)
				return 0;
			doc->pendingAttrCount = 0;
		}
		len = doc->nodesSize - start;
		off = start + 1;
		if (len <= 0xEF) {
			doc->nodes[start] = len;
		} else if (len <= 0xFFFF) {
			doc->nodes[start] = 0xF0;
			doc->nodes[off + 1] = doc->nodesSize - off;
			doc->nodes[start + 1] = (doc->nodesSize - off) >> 8;
		} else {
			doc->nodes[start] = 0xF1;
			doc->nodes[off + 2] = doc->nodesSize - off;
			doc->nodes[off + 1] = (doc->nodesSize - off) >> 8;
			doc->nodes[start + 1] = (doc->nodesSize - off) >> 16;
		}
	}

	nodes = doc->nodes;
	if (doc->nodesCapacity <= doc->nodesSize + 12) {
		nodes = doc->nodes = bxceGrowTable(nodes, doc->nodesSize + 12, &doc->nodesCapacity);
		if (nodes == NULL) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return 0;
		}
	}

	nodes[doc->nodesSize] = SCE_XML_TOKEN_DOM_TEXT;
	doc->nodesSize += 1;
	bxceWriteLength(doc, pos, nodes, &doc->nodesSize);
	return pos;
}

/* writes a token and its encoded operand at the end of the node table; a failed grow is noted only */
int bxceDocumentWriteSkipped(BxceDocument *doc, SceUInt32 value, int token)
{
	if (doc->nodesCapacity <= doc->nodesSize + 12) {
		doc->nodes = bxceGrowTable(doc->nodes, doc->nodesSize + 12, &doc->nodesCapacity);
		if (doc->nodes == NULL)
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
	}

	doc->nodes[doc->nodesSize] = token;
	doc->nodesSize += 1;
	if (value <= 0xEF) {
		doc->nodes[doc->nodesSize] = value;
		doc->nodesSize += 1;
	} else if (value <= 0xFFFF) {
		doc->nodes[doc->nodesSize] = 0xF0;
		doc->nodesSize += 1;
		doc->nodes[doc->nodesSize + 1] = value;
		doc->nodes[doc->nodesSize] = value >> 8;
		doc->nodesSize += 2;
	} else {
		doc->nodes[doc->nodesSize] = 0xF1;
		doc->nodesSize += 1;
		doc->nodes[doc->nodesSize + 2] = value;
		doc->nodes[doc->nodesSize + 1] = value >> 8;
		doc->nodes[doc->nodesSize] = value >> 16;
		doc->nodesSize += 3;
	}
	return 0;
}

/*
 * Appends a skipped text (kind is its token) under parent. An entity of kind 2 below 5 is written
 * as its number, without a text; 5 and above mark the parent. Its text position, or 0.
 */
SceUInt32 bxceDocumentAppendSkipped(BxceDocument *doc, BxceNodeId parent, const char *text,
				    SceUInt32 length, int kind, int entity)
{
	SceUInt32 pos, start, off, len;
	BxceNodeId prev, next;
	char *nodes;
	char token;

	pos = doc->textsSize;
	if (kind != 2 || entity > 4)
		bxceDocumentAddText(doc, text, length);

	nodes = doc->nodes;
	token = nodes[parent];
	if (token & SCE_XML_TOKEN_DOM_HAS_CHILD) {
		prev = next = ScebXCe_FEA6E4A1(doc, parent);
		while (next != 0) {
			prev = next;
			next = ScebXCe_6FC9DC3F(doc, prev);
		}
		if (((doc->nodes[prev] |= SCE_XML_TOKEN_DOM_HAS_SIBLING) & 0xF) ==
		    SCE_XML_TOKEN_DOM_ELEMENT) {
			doc->nodes[prev + 6] = doc->nodesSize - prev - 3;
			doc->nodes[prev + 5] = ((doc->nodesSize - prev - 3) >> 8) & 0xFF;
			doc->nodes[prev + 4] = ((doc->nodesSize - prev - 3) >> 16) & 0xFF;
			doc->nodes[prev + 3] = (doc->nodesSize - prev - 3) >> 24;
		} else {
			doc->nodes[doc->nodesSize] = SCE_XML_TOKEN_DOM_ATTR;
			doc->nodesSize += 1;
		}
	} else {
		nodes[parent] = token | SCE_XML_TOKEN_DOM_HAS_CHILD;
		start = doc->nodesSize;
		doc->nodesSize = start + 1;
		if (doc->pendingAttrCount > 0) {
			if (bxceDocumentWriteAttributes(doc, doc->pendingAttr, doc->pendingAttrCount,
							doc->nodes, &doc->nodesSize, 0) != 0)
				return 0;
			doc->pendingAttrCount = 0;
		}
		len = doc->nodesSize - start;
		off = start + 1;
		if (len <= 0xEF) {
			doc->nodes[start] = len;
		} else if (len <= 0xFFFF) {
			doc->nodes[start] = 0xF0;
			doc->nodes[off + 1] = doc->nodesSize - off;
			doc->nodes[start + 1] = (doc->nodesSize - off) >> 8;
		} else {
			doc->nodes[start] = 0xF1;
			doc->nodes[off + 2] = doc->nodesSize - off;
			doc->nodes[off + 1] = (doc->nodesSize - off) >> 8;
			doc->nodes[start + 1] = (doc->nodesSize - off) >> 16;
		}
	}

	if (kind == 2) {
		if (entity < 5) {
			bxceDocumentWriteSkipped(doc, entity, kind);
		} else if (entity == 5) {
			bxceDocumentWriteSkipped(doc, pos, kind);
			nodes[parent] |= 0x12;
		} else {
			bxceDocumentWriteSkipped(doc, pos, kind);
			nodes[parent] |= 0x42;
		}
	} else {
		bxceDocumentWriteSkipped(doc, pos, kind);
	}

	return pos;
}

/* a token for a text added after it: the text's position */
SceUInt32 bxceDocumentAddSkipped(BxceDocument *doc, const char *text, SceUInt32 length, int token)
{
	SceUInt32 pos;

	pos = doc->textsSize;
	bxceDocumentWriteSkipped(doc, pos, token);
	bxceDocumentAddText(doc, text, length);
	return pos;
}

/*
 * Appends to the text at position text of the text table, moving it when its encoded length
 * grows (by the sizes Sony's code adds, which for a one byte length becoming three is one more
 * than it moved).
 */
int bxceDocumentExtendText(BxceDocument *doc, SceUInt32 text, const char *more, SceUInt32 length)
{
	SceUInt32 oldLength, newLength;
	char *data;

	if (doc->textsCapacity <= doc->textsSize + length + 5) {
		doc->texts = bxceGrowTable(doc->texts, doc->textsSize + length + 5,
					   &doc->textsCapacity);
		if (doc->texts == NULL) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return SCE_BXCE_ERROR_NO_MEMORY;
		}
	}

	data = doc->texts + bxceLengthSize(((const SceUInt8 *)doc->texts)[text]) + text;
	oldLength = bxceReadLength((const SceUInt8 *)doc->texts + text);
	bxceMemcpy(data + oldLength, more, length);
	newLength = oldLength + length;
	data[newLength] = 0;
	doc->textsSize += length;

	if (newLength <= 0xEF) {
		doc->texts[text] = newLength;
	} else if (newLength <= 0xFFFF) {
		SceUInt32 pos;

		if (oldLength <= 0xEF) {
			bxceMemmove(doc->texts + text + 3, doc->texts + text + 1, newLength);
			doc->textsSize += 2;
		}
		pos = text;
		bxceWriteLength(doc, newLength, doc->texts, &pos);
	} else {
		SceUInt32 pos;

		if (oldLength <= 0xEF) {
			bxceMemmove(doc->texts + text + 4, doc->texts + text + 1, newLength);
			doc->textsSize += 4;
		} else if (oldLength <= 0xFFFF) {
			bxceMemmove(doc->texts + text + 4, doc->texts + text + 3, newLength);
			doc->textsSize += 1;
		}
		pos = text;
		bxceWriteLength(doc, newLength, doc->texts, &pos);
	}

	return 0;
}

/* writes the attributes waiting for a child, at the end of the node table */
int bxceDocumentFlushAttributes(BxceDocument *doc)
{
	int ret;

	if (doc->pendingAttrCount <= 0)
		return 0;

	ret = bxceDocumentWriteAttributes(doc, doc->pendingAttr, doc->pendingAttrCount, doc->nodes,
					  &doc->nodesSize, 0);
	doc->pendingAttrCount = 0;
	return ret;
}

/* the attribute after this one of its element, or 0 */
BxceNodeId ScebXCe_134F491C(const BxceDocument *doc, BxceNodeId node)
{
	const char *p;
	int i;

	if (node == 0)
		return 0;

	p = doc->nodes + node;
	if ((p[0] & 0xF) != SCE_XML_TOKEN_DOM_ATTR)
		return 0;

	/* past the count, the name offset and the value position */
	i = (p[0] & SCE_XML_TOKEN_DOM_HAS_SIBLING) ? 1 : 0;
	i += 1 + bxceLengthSize(((const SceUInt8 *)p)[i + 1]);
	i += bxceLengthSize(((const SceUInt8 *)p)[i]);
	if ((p[i] & 0xF) == SCE_XML_TOKEN_DOM_ATTR)
		return node + i;
	return 0;
}

/* the first attribute of an element, or 0 */
BxceNodeId ScebXCe_B034C595(const BxceDocument *doc, BxceNodeId node)
{
	const char *p;
	SceUInt32 attr;
	int type, size;

	if (node == 0)
		return 0;

	if (!doc->isBinary) {
		type = ScebXCe_FA18A039(doc, node);
		if ((type & 5) != 0 || (type & 4) != 0) {
			attr = ((const BxceDomNode *)node)->text;
			return attr != 0 ? attr + 1 : 0;
		}
		return 0;
	}

	p = doc->nodes + node;
	switch (p[0]) {
	case SCE_XML_TOKEN_DOM_ELEMENT_ATTR:
	case SCE_XML_TOKEN_DOM_RWELEMENT_ATTR:
	case SCE_XML_TOKEN_DOM_ELEMENT_ATTR_CHILD:
	case SCE_XML_TOKEN_DOM_RWELEMENT_ATTR_CHILD:
	case SCE_XML_TOKEN_DOM_ELEMENT_ATTR_SIB:
	case SCE_XML_TOKEN_DOM_RWELEMENT_ATTR_SIB:
	case SCE_XML_TOKEN_DOM_ELEMENT_ATTR_SIB_CHILD:
	case SCE_XML_TOKEN_DOM_RWELEMENT_ATTR_SIB_CHILD:
		/* past the token, the name, the sibling offset and a child's length */
		size = 0;
		if (p[0] & SCE_XML_TOKEN_DOM_HAS_CHILD)
			size = bxceLengthSize(((const SceUInt8 *)p)[7]);
		return node + size + 7;
	default:
		return 0;
	}
}

/* the document node */
BxceNodeId ScebXCe_75E1B095(const BxceDocument *doc)
{
	if (!doc->isBinary)
		return (BxceNodeId)doc->domRoot;
	return doc->nodes != NULL ? 1 : 0;
}

/* the parent of a node, searched from the root; 0 for the root itself */
BxceNodeId bxceDocumentGetParent(BxceDocument *doc, BxceNodeId node)
{
	BxceNodeId root, parent;
	int found;

	if (node == 0) {
		doc->status = SCE_BXCE_ERROR_INVALID_ARGUMENT;
		return 0;
	}

	doc->status = 0;
	root = ScebXCe_CED6BECF(doc);
	if (root == 0)
		return ScebXCe_75E1B095(doc);

	parent = root;
	found = 0;
	bxceFindParent(doc, root, node, &found, &parent);
	if (parent == node)
		return 0;
	if (parent == 0)
		doc->status = SCE_BXCE_ERROR_DOM_NOT_FOUND_ERROR;
	return parent;
}

/* links a DOM node under parent, after the child after (first when 0) */
BxceNodeId ScebXCe_3BF2F11B(BxceDocument *doc, BxceNodeId parent, BxceNodeId after,
			    BxceNodeId child)
{
	BxceDomNode *p = (BxceDomNode *)parent;
	BxceDomNode *c = (BxceDomNode *)child;
	BxceDomNode *n, *next;

	doc->status = 0;
	if (doc->isBinary) {
		doc->status = SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
		return 0;
	}

	if (child == 0 || parent == 0) {
		doc->status = SCE_BXCE_ERROR_INVALID_ARGUMENT;
		return 0;
	}

	if (p->child == NULL) {
		p->child = c;
		p->info |= SCE_XML_TOKEN_DOM_HAS_CHILD;
		return child;
	}

	if (after == 0) {
		n = p->child;
		p->child = c;
		c->sibling = n;
		c->info |= SCE_XML_TOKEN_DOM_HAS_SIBLING;
		return child;
	}

	for (n = p->child; n != NULL; n = next) {
		next = n->sibling;
		if (n == (BxceDomNode *)after) {
			n->sibling = c;
			n->info |= SCE_XML_TOKEN_DOM_HAS_SIBLING;
			c->sibling = next;
			if (next != NULL)
				c->info |= SCE_XML_TOKEN_DOM_HAS_SIBLING;
			return child;
		}
	}

	doc->status = SCE_BXCE_ERROR_DOM_NOT_FOUND_ERROR;
	return 0;
}

/* a new DOM text node, its text added to the text table */
BxceNodeId ScebXCe_AEB2CE13(BxceDocument *doc, const char *text, SceUInt32 length)
{
	BxceDomNode *node;
	SceUInt32 pos;

	if (doc->isBinary) {
		doc->status = SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
		return 0;
	}

	if (text == NULL || length == 0) {
		doc->status = SCE_BXCE_ERROR_INVALID_ARGUMENT;
		return 0;
	}

	doc->status = 0;
	node = (BxceDomNode *)ScebXCe_B8DDE741(sizeof(BxceDomNode));
	if (node == NULL) {
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return 0;
	}

	bxceMemset(node, 0, sizeof(BxceDomNode));
	node->info = SCE_XML_TOKEN_DOM_TEXT;
	node->child = NULL;
	node->sibling = NULL;
	if (doc->textsSize <= 0xEF) {
		node->text = doc->textsSize;
	} else {
		pos = 0;
		bxceWriteLength(doc, doc->textsSize, (char *)&node->text, &pos);
	}

	if (bxceDocumentAddText(doc, text, length) != 0) {
		ScebXCe_EC560647(node);
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return 0;
	}

	return (BxceNodeId)node;
}

/*
 * A new DOM element of this name, with its attributes written in bX form into a block of its own
 * and, when text is given, a text node as its child.
 */
BxceNodeId bxceDocumentCreateElement(BxceDocument *doc, const char *name,
				     const BxceAttributeList *attrs, const char *text,
				     SceUInt32 textLength)
{
	char buffer[BXCE_DOM_ATTR_BUFFER_SIZE];
	const BxceAttr *attr;
	BxceDomNode *node, *child;
	SceUInt32 size, pos;
	char *block;
	int length, nameOffset, offset, nameLength, i;

	if (doc->isBinary) {
		doc->status = SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
		return 0;
	}

	if (name == NULL) {
		doc->status = SCE_BXCE_ERROR_INVALID_ARGUMENT;
		return 0;
	}

	doc->status = 0;
	length = bxceStrnlen(name, BXCE_ATTR_STRING_MAX);
	if (length > BXCE_NAME_MAX) {
		doc->status = SCE_BXCE_ERROR_EXCEED_LIMIT;
		length = BXCE_NAME_MAX;
	}

	if (doc->elementNamesCapacity <= doc->elementNamesSize + length + 5) {
		doc->elementNames = bxceGrowTable(doc->elementNames,
						  doc->elementNamesSize + length + 5,
						  &doc->elementNamesCapacity);
		if (doc->elementNames == NULL) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return 0;
		}
	}

	nameOffset = bxceNameHashIntern(doc->elementNameHash, name, length, doc->elementNames,
					&doc->elementNamesSize);
	if (nameOffset >= BXCE_DOM_UNIQUE_NAME_MAX) {
		doc->status = SCE_BXCE_ERROR_DOM_MAX_UNIQUE_ELEMENT_ERROR;
		return 0;
	}
	if (nameOffset == -1) {
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return 0;
	}

	node = (BxceDomNode *)ScebXCe_B8DDE741(sizeof(BxceDomNode));
	if (node == NULL) {
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return 0;
	}
	node->text = 0;
	node->child = NULL;
	node->sibling = NULL;
	node->info = nameOffset << 8 | SCE_XML_TOKEN_DOM_COMPACT_ELEMENT;

	if (attrs != NULL && attrs->length != 0) {
		if (attrs->length > BXCE_DOM_ATTR_MAX) {
			ScebXCe_EC560647(node);
			doc->status = SCE_BXCE_ERROR_DOM_MAX_NUM_OF_ATTR_ERROR;
			return 0;
		}

		for (i = 0; i < attrs->length; i++) {
			attr = ScebXCe_01DC4E22(attrs, i);
			if (doc->attrNamesCapacity <= doc->attrNamesSize + attr->nameLength + 5) {
				doc->attrNames = bxceGrowTable(doc->attrNames,
							       doc->attrNamesSize + attr->nameLength + 5,
							       &doc->attrNamesCapacity);
				if (doc->attrNames == NULL) {
					if (node != NULL)
						ScebXCe_EC560647(node);
					doc->status = SCE_BXCE_ERROR_NO_MEMORY;
					return 0;
				}
			}

			nameLength = attr->nameLength;
			if (nameLength > BXCE_DOM_ATTR_NAME_MAX) {
				doc->status = SCE_BXCE_ERROR_EXCEED_LIMIT;
				nameLength = BXCE_DOM_ATTR_NAME_MAX;
			}

			offset = bxceNameHashIntern(doc->attrNameHash, attr->name, nameLength,
						    doc->attrNames, &doc->attrNamesSize);
			if (offset >= BXCE_DOM_UNIQUE_NAME_MAX) {
				if (node != NULL)
					ScebXCe_EC560647(node);
				doc->status = SCE_BXCE_ERROR_DOM_MAX_UNIQUE_ATTR_ERROR;
				return 0;
			}
			if (offset == -1) {
				if (node != NULL)
					ScebXCe_EC560647(node);
				doc->status = SCE_BXCE_ERROR_NO_MEMORY;
				return 0;
			}

			doc->pendingAttr[2 * i] = offset;
			doc->pendingAttr[2 * i + 1] = doc->textsSize;
			if (bxceDocumentAddText(doc, attr->value, attr->valueLength) != 0) {
				if (node != NULL)
					ScebXCe_EC560647(node);
				doc->status = SCE_BXCE_ERROR_NO_MEMORY;
				return 0;
			}
		}

		/* the attributes into the buffer, then into a block after its length byte */
		size = 0;
		doc->nodesCapacity = BXCE_DOM_ATTR_BUFFER_SIZE;
		if (bxceDocumentWriteAttributes(doc, doc->pendingAttr, attrs->length, buffer, &size,
						0) != 0) {
			if (node != NULL)
				ScebXCe_EC560647(node);
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return 0;
		}

		block = (char *)ScebXCe_B8DDE741(size + 2);
		node->text = (SceUInt32)block;
		if (block == NULL) {
			if (node != NULL)
				ScebXCe_EC560647(node);
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return 0;
		}
		bxceMemcpy(block + 1, buffer, size);
		((char *)node->text)[0] = size + 1;
		((char *)node->text)[size + 1] = 0;
		node->info |= SCE_XML_TOKEN_DOM_HAS_ATTR;
	}

	if (text != NULL) {
		child = (BxceDomNode *)ScebXCe_B8DDE741(sizeof(BxceDomNode));
		if (child == NULL) {
			if (node->text != 0)
				ScebXCe_EC560647((void *)node->text);
			if (node != NULL)
				ScebXCe_EC560647(node);
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return 0;
		}

		bxceMemset(child, 0, sizeof(BxceDomNode));
		child->child = NULL;
		child->sibling = NULL;
		child->info = SCE_XML_TOKEN_DOM_TEXT;
		if (doc->textsSize <= 0xEF) {
			child->text = doc->textsSize;
		} else {
			pos = 0;
			bxceWriteLength(doc, doc->textsSize, (char *)&child->text, &pos);
		}

		if (bxceDocumentAddText(doc, text, textLength) != 0) {
			if (node->text != 0)
				ScebXCe_EC560647((void *)node->text);
			if (node != NULL)
				ScebXCe_EC560647(node);
			ScebXCe_EC560647(child);
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return 0;
		}

		node->child = child;
		node->info |= SCE_XML_TOKEN_DOM_HAS_CHILD;
	}

	return (BxceNodeId)node;
}

/* an attribute's value: the text its value position names */
const char *ScebXCe_84383D8E(const BxceDocument *doc, BxceNodeId attr)
{
	const char *p;
	const char *texts;
	int i;

	if (attr == 0)
		return NULL;

	if (!doc->isBinary) {
		p = (const char *)attr;
		if ((p[0] & 0xF) != SCE_XML_TOKEN_DOM_ATTR)
			return NULL;
		texts = doc->texts;
		i = (p[0] & SCE_XML_TOKEN_DOM_HAS_SIBLING) ? 1 : 0;
		i += 1 + bxceLengthSize(((const SceUInt8 *)p)[i + 1]);
		return bxceString(texts, bxceReadLength((const SceUInt8 *)p + i));
	}

	p = doc->nodes + attr;
	if ((p[0] & 0xF) != SCE_XML_TOKEN_DOM_ATTR)
		return NULL;
	texts = doc->texts;
	i = (p[0] & SCE_XML_TOKEN_DOM_HAS_SIBLING) ? 1 : 0;
	i += 1 + bxceLengthSize(((const SceUInt8 *)p)[i + 1]);
	return bxceString(texts, bxceReadLength((const SceUInt8 *)p + i));
}

/*
 * Copies node and its following siblings (up to last) of the document src, with their elements'
 * attributes, texts and children, into the DOM document doc under parent, after the child after.
 * Stops at the first error doc notes.
 */
void bxceDocumentCopyNodes(BxceDocument *doc, const BxceDocument *src, BxceNodeId node,
			   BxceNodeId after, BxceNodeId parent, BxceNodeId last)
{
	BxceAttributeList *attrs;
	BxceNodeId attr, copy, child;
	const char *name;
	SceUInt32 type;

	for (; node != 0 && doc->status == 0; node = ScebXCe_6FC9DC3F(src, node)) {
		type = ScebXCe_FA18A039(src, node) & 0xF;
		if (type <= SCE_XML_TOKEN_DOM_COMPACT_ELEMENT) {
			copy = 0;
			if (type > SCE_XML_TOKEN_DOM_END_ELEMENT) {
				attrs = NULL;
				attr = ScebXCe_B034C595(src, node);
				if (attr != 0) {
					attrs = (BxceAttributeList *)ScebXCe_B8DDE741(sizeof(BxceAttributeList));
					if (attrs == NULL) {
						doc->status = SCE_BXCE_ERROR_NO_MEMORY;
						return;
					}
					ScebXCe_E7C8B20D(attrs);
					do {
						name = ScebXCe_B4989A58(src, attr);
						if (ScebXCe_3DD761CF(attrs, name,
								     ScebXCe_84383D8E(src, attr)) != 0) {
							doc->status = SCE_BXCE_ERROR_NO_MEMORY;
							ScebXCe_9C2F9897(attrs);
							if (attrs != NULL)
								ScebXCe_EC560647(attrs);
							return;
						}
						attr = ScebXCe_134F491C(src, attr);
					} while (attr != 0);
				}

				copy = bxceDocumentCreateElement(doc, ScebXCe_1189A2B1(src, node), attrs,
								 NULL, 0);
				if (copy == 0) {
					doc->status = SCE_BXCE_ERROR_NO_MEMORY;
					return;
				}
				if (after != 0)
					ScebXCe_3BF2F11B(doc, parent, after, copy);
				else
					ScebXCe_3BF2F11B(doc, parent, after, copy);
				if (attrs != NULL) {
					ScebXCe_9C2F9897(attrs);
					ScebXCe_EC560647(attrs);
				}
			}
		} else {
			copy = 0;
			if (type == SCE_XML_TOKEN_DOM_TEXT) {
				copy = ScebXCe_AEB2CE13(doc, ScebXCe_33825EFF(src, node),
							bxceStrnlen(ScebXCe_33825EFF(src, node),
								    BXCE_ATTR_STRING_MAX));
				if (copy == 0) {
					doc->status = SCE_BXCE_ERROR_NO_MEMORY;
					return;
				}
				if (after != 0)
					ScebXCe_3BF2F11B(doc, parent, after, copy);
				else
					ScebXCe_3BF2F11B(doc, parent, after, copy);
			}
		}

		/* the children, after the copy's last child */
		after = 0;
		for (child = ScebXCe_FEA6E4A1(doc, copy); child != 0;
		     child = ScebXCe_6FC9DC3F(doc, child))
			after = child;
		bxceDocumentCopyNodes(doc, src, ScebXCe_FEA6E4A1(src, node), after, copy, last);

		/* the next sibling, after the last child of the copy's parent */
		parent = bxceDocumentGetParent(doc, copy);
		after = 0;
		for (child = ScebXCe_FEA6E4A1(doc, parent); child != 0;
		     child = ScebXCe_6FC9DC3F(doc, child))
			after = child;

		if (node == last)
			return;
	}
}

/* an empty document: a bX one with no tables, or a DOM one with its document node and tables */
int ScebXCe_FAB50608(BxceDocument *doc, SceUInt8 isDom)
{
	if (!isDom) {
		doc->isExternal = 0;
		doc->nodes = NULL;
		doc->texts = NULL;
		doc->elementNames = NULL;
		doc->attrNames = NULL;
		doc->status = 0;
		doc->attrNameHash = NULL;
		doc->elementNameHash = NULL;
		doc->nodesCapacity = 0;
		doc->textsCapacity = 0;
		doc->elementNamesCapacity = 0;
		doc->attrNamesCapacity = 0;
		doc->nodesSize = 0;
		doc->textsSize = 1;
		doc->elementNamesSize = 0;
		doc->attrNamesSize = 0;
		doc->pendingAttrCount = 0;
		doc->field1112 = 0;
		doc->domRoot = NULL;
		doc->isBinary = 1;
		doc->field1120 = 0;
		return 0;
	}

	doc->isExternal = 0;
	doc->nodes = NULL;
	doc->texts = NULL;
	doc->elementNames = NULL;
	doc->attrNames = NULL;
	doc->status = 0;
	doc->attrNameHash = NULL;
	doc->elementNameHash = NULL;
	doc->nodesCapacity = 0;
	doc->textsCapacity = 0;
	doc->elementNamesCapacity = 0;
	doc->attrNamesCapacity = 0;
	doc->nodesSize = 0;
	doc->textsSize = 1;
	doc->elementNamesSize = 0;
	doc->attrNamesSize = 0;
	doc->pendingAttrCount = 0;
	doc->field1112 = 0;
	doc->domRoot = NULL;
	doc->isBinary = 1;
	doc->field1120 = 0;

	doc->domRoot = (BxceDomNode *)ScebXCe_B8DDE741(sizeof(BxceDomNode));
	if (doc->domRoot == NULL) {
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return SCE_BXCE_ERROR_NO_MEMORY;
	}
	doc->domRoot->child = NULL;
	doc->domRoot->sibling = NULL;
	doc->domRoot->info = SCE_XML_TOKEN_DOM_ROOT_DOCUMENT;
	doc->domRoot->text = 0;
	doc->isBinary = 0;
	return bxceDocumentAllocateTables(doc);
}

/* an empty function nothing calls */
void bxceDocumentNop(void)
{
}

/* the kind of a bX entity node: 5 or 6 when marked so, else the number it holds */
int ScebXCe_35CDE77C(const BxceDocument *doc, BxceNodeId node)
{
	const char *p;

	if (node == 0)
		return SCE_BXCE_ERROR_DOM_ENITITY_ERROR;
	if (!doc->isBinary)
		return SCE_BXCE_ERROR_DOM_ENITITY_ERROR;

	p = doc->nodes + node;
	if (p[0] & 0x10)
		return 5;
	if (p[0] & 0x40)
		return 6;
	return ((const SceUInt8 *)p)[1];
}

/* the text of a bX entity node marked 0x12 or 0x42, else NULL */
const char *bxceDocumentGetEntityText(const BxceDocument *doc, BxceNodeId node)
{
	const char *p;
	const char *texts;

	if (node == 0)
		return NULL;
	if (!doc->isBinary)
		return NULL;

	p = doc->nodes + node;
	if ((p[0] & 0x12) == 0x12 || (p[0] & 0x42) == 0x42) {
		texts = doc->texts;
		return texts + bxceReadLength((const SceUInt8 *)p + 1) + 1;
	}
	return NULL;
}

/* frees a DOM node, its following siblings and their children, with their attribute blocks */
void ScebXCe_2C2628BB(BxceDocument *doc, BxceNodeId node)
{
	BxceNodeId next;

	if (doc->isBinary || node == 0)
		return;

	do {
		ScebXCe_2C2628BB(doc, ScebXCe_FEA6E4A1(doc, node));
		next = ScebXCe_6FC9DC3F(doc, node);
		if (node != 0) {
			switch (ScebXCe_FA18A039(doc, node)) {
			case SCE_XML_TOKEN_DOM_ELEMENT:
			case SCE_XML_TOKEN_DOM_COMPACT_ELEMENT:
			case SCE_XML_TOKEN_DOM_PRED_ELEMENT:
				if (((BxceDomNode *)node)->text != 0)
					ScebXCe_EC560647((void *)((BxceDomNode *)node)->text);
				/* fall through */
			case SCE_XML_TOKEN_DOM_SKIP_TEXT_CDATA:
			case SCE_XML_TOKEN_DOM_TEXT:
			case SCE_XML_TOKEN_DOM_TEXT_SKIPPED:
			case SCE_XML_TOKEN_DOM_SKIP_TEXT_PI:
			case SCE_XML_TOKEN_DOM_SKIP_TEXT_COMMENT:
				ScebXCe_EC560647((void *)node);
				break;
			default:
				break;
			}
		}
		node = next;
	} while (node != 0);
}

/* frees what the document owns: its tables unless external, its DOM nodes */
int ScebXCe_B0891B13(BxceDocument *doc)
{
	if (!doc->isExternal) {
		if (doc->attrNameHash != NULL) {
			bxceNameHashClear(doc->attrNameHash);
			if (doc->attrNameHash != NULL)
				ScebXCe_EC560647(doc->attrNameHash);
			doc->attrNameHash = NULL;
		}
		if (doc->elementNameHash != NULL) {
			bxceNameHashClear(doc->elementNameHash);
			if (doc->elementNameHash != NULL)
				ScebXCe_EC560647(doc->elementNameHash);
			doc->elementNameHash = NULL;
		}
		if (doc->attrNames != NULL)
			ScebXCe_EC560647(doc->attrNames);
		doc->attrNames = NULL;
		if (doc->texts != NULL)
			ScebXCe_EC560647(doc->texts);
		doc->texts = NULL;
		if (doc->elementNames != NULL)
			ScebXCe_EC560647(doc->elementNames);
		doc->elementNames = NULL;
		if (doc->nodes != NULL)
			ScebXCe_EC560647(doc->nodes);
		doc->nodes = NULL;
	}

	if (!doc->isBinary) {
		ScebXCe_2C2628BB(doc, ScebXCe_CED6BECF(doc));
		if (doc->domRoot != NULL)
			ScebXCe_EC560647(doc->domRoot);
		doc->domRoot = NULL;
	}

	if (doc->field1112 != 0)
		doc->field1112 = 0;
	return 0;
}

BxceNodeList *ScebXCe_1FA24DC0(const BxceDocument *doc, BxceNodeId node, const char *name)
{
	if (node == 0 || name == NULL)
		return NULL;
	return bxceGetElementsByTagName(doc, node, name, NULL);
}

/*
 * createElement's work, the element then appended as the last child of parent. Unlike it, a
 * failed attribute text, attribute write or attribute block leaves the status as it was, and the
 * text node is cleared before its allocation is checked.
 */
BxceNodeId ScebXCe_AC0E4F67(BxceDocument *doc, BxceNodeId parent, const char *name,
			    const BxceAttributeList *attrs, const char *text, SceUInt32 textLength)
{
	char buffer[BXCE_DOM_ATTR_BUFFER_SIZE];
	const BxceAttr *attr;
	BxceDomNode *owner = (BxceDomNode *)parent;
	BxceDomNode *node, *child, *last, *next;
	SceUInt32 size, pos;
	char *block;
	int length, nameOffset, offset, nameLength, i;

	if (doc->isBinary) {
		doc->status = SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
		return 0;
	}

	if (name == NULL || parent == 0) {
		doc->status = SCE_BXCE_ERROR_INVALID_ARGUMENT;
		return 0;
	}

	doc->status = 0;
	length = bxceStrnlen(name, BXCE_ATTR_STRING_MAX);
	if (length > BXCE_NAME_MAX) {
		doc->status = SCE_BXCE_ERROR_EXCEED_LIMIT;
		length = BXCE_NAME_MAX;
	}

	if (doc->elementNamesCapacity <= doc->elementNamesSize + length + 5) {
		doc->elementNames = bxceGrowTable(doc->elementNames,
						  doc->elementNamesSize + length + 5,
						  &doc->elementNamesCapacity);
		if (doc->elementNames == NULL) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return 0;
		}
	}

	nameOffset = bxceNameHashIntern(doc->elementNameHash, name, length, doc->elementNames,
					&doc->elementNamesSize);
	if (nameOffset >= BXCE_DOM_UNIQUE_NAME_MAX) {
		doc->status = SCE_BXCE_ERROR_DOM_MAX_UNIQUE_ELEMENT_ERROR;
		return 0;
	}
	if (nameOffset == -1) {
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return 0;
	}

	node = (BxceDomNode *)ScebXCe_B8DDE741(sizeof(BxceDomNode));
	if (node == NULL) {
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return 0;
	}
	node->text = 0;
	node->child = NULL;
	node->sibling = NULL;
	node->info = nameOffset << 8 | SCE_XML_TOKEN_DOM_COMPACT_ELEMENT;

	if (attrs != NULL && attrs->length != 0) {
		if (attrs->length > BXCE_DOM_ATTR_MAX) {
			ScebXCe_EC560647(node);
			doc->status = SCE_BXCE_ERROR_DOM_MAX_NUM_OF_ATTR_ERROR;
			return 0;
		}

		for (i = 0; i < attrs->length; i++) {
			attr = ScebXCe_01DC4E22(attrs, i);
			if (doc->attrNamesCapacity <= doc->attrNamesSize + attr->nameLength + 5) {
				doc->attrNames = bxceGrowTable(doc->attrNames,
							       doc->attrNamesSize + attr->nameLength + 5,
							       &doc->attrNamesCapacity);
				if (doc->attrNames == NULL) {
					if (node != NULL)
						ScebXCe_EC560647(node);
					doc->status = SCE_BXCE_ERROR_NO_MEMORY;
					return 0;
				}
			}

			nameLength = attr->nameLength;
			if (nameLength > BXCE_DOM_ATTR_NAME_MAX) {
				doc->status = SCE_BXCE_ERROR_EXCEED_LIMIT;
				nameLength = BXCE_DOM_ATTR_NAME_MAX;
			}

			offset = bxceNameHashIntern(doc->attrNameHash, attr->name, nameLength,
						    doc->attrNames, &doc->attrNamesSize);
			if (offset >= BXCE_DOM_UNIQUE_NAME_MAX) {
				if (node != NULL)
					ScebXCe_EC560647(node);
				doc->status = SCE_BXCE_ERROR_DOM_MAX_UNIQUE_ATTR_ERROR;
				return 0;
			}
			if (offset == -1) {
				if (node != NULL)
					ScebXCe_EC560647(node);
				doc->status = SCE_BXCE_ERROR_NO_MEMORY;
				return 0;
			}

			doc->pendingAttr[2 * i] = offset;
			doc->pendingAttr[2 * i + 1] = doc->textsSize;
			if (bxceDocumentAddText(doc, attr->value, attr->valueLength) != 0) {
				if (node != NULL)
					ScebXCe_EC560647(node);
				return 0;
			}
		}

		/* the attributes into the buffer, then into a block after its length byte */
		size = 0;
		doc->nodesCapacity = BXCE_DOM_ATTR_BUFFER_SIZE;
		if (bxceDocumentWriteAttributes(doc, doc->pendingAttr, attrs->length, buffer, &size,
						0) != 0) {
			if (node != NULL)
				ScebXCe_EC560647(node);
			return 0;
		}

		block = (char *)ScebXCe_B8DDE741(size + 2);
		node->text = (SceUInt32)block;
		if (block == NULL) {
			if (node != NULL)
				ScebXCe_EC560647(node);
			return 0;
		}
		bxceMemcpy(block + 1, buffer, size);
		((char *)node->text)[0] = size + 1;
		((char *)node->text)[size + 1] = 0;
		node->info |= SCE_XML_TOKEN_DOM_HAS_ATTR;
	}

	if (text != NULL) {
		child = (BxceDomNode *)ScebXCe_B8DDE741(sizeof(BxceDomNode));
		bxceMemset(child, 0, sizeof(BxceDomNode));
		if (child == NULL) {
			if (node->text != 0)
				ScebXCe_EC560647((void *)node->text);
			if (node != NULL)
				ScebXCe_EC560647(node);
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return 0;
		}

		child->child = NULL;
		child->sibling = NULL;
		child->info = SCE_XML_TOKEN_DOM_TEXT;
		if (doc->textsSize <= 0xEF) {
			child->text = doc->textsSize;
		} else {
			pos = 0;
			bxceWriteLength(doc, doc->textsSize, (char *)&child->text, &pos);
		}

		if (bxceDocumentAddText(doc, text, textLength) != 0) {
			ScebXCe_EC560647(child);
			if (node->text != 0)
				ScebXCe_EC560647((void *)node->text);
			if (node != NULL)
				ScebXCe_EC560647(node);
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return 0;
		}

		node->child = child;
		node->info |= SCE_XML_TOKEN_DOM_HAS_CHILD;
	}

	next = owner->child;
	if (next == NULL) {
		owner->child = node;
		owner->info |= SCE_XML_TOKEN_DOM_HAS_CHILD;
	} else {
		do {
			last = next;
			next = last->sibling;
		} while (next != NULL);
		last->sibling = node;
		last->info |= SCE_XML_TOKEN_DOM_HAS_SIBLING;
	}

	return (BxceNodeId)node;
}

/* unlinks a DOM child from parent; the flags stay as they were */
BxceNodeId ScebXCe_568CE24D(BxceDocument *doc, BxceNodeId child, BxceNodeId parent)
{
	BxceDomNode *c = (BxceDomNode *)child;
	BxceDomNode *p = (BxceDomNode *)parent;
	BxceDomNode *first, *prev, *n;

	doc->status = 0;
	if (doc->isBinary) {
		doc->status = SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
		return 0;
	}

	if (child == 0 || parent == 0) {
		doc->status = SCE_BXCE_ERROR_INVALID_ARGUMENT;
		return 0;
	}

	first = p->child;
	if (first == NULL)
		return 0;

	if (first == c) {
		if (c->sibling == NULL) {
			p->child = NULL;
		} else {
			p->child = c->sibling;
			c->sibling = NULL;
		}
		return child;
	}

	/* never taken: the case above */
	if (first == c && c->sibling == NULL) {
		p->child = NULL;
		return child;
	}

	prev = first;
	n = prev->sibling;
	while (n != NULL && n != c) {
		prev = n;
		n = prev->sibling;
	}
	if (n == NULL) {
		doc->status = SCE_BXCE_ERROR_DOM_NOT_FOUND_ERROR;
		return 0;
	}

	prev->sibling = c->sibling;
	c->sibling = NULL;
	return child;
}

/* copies node of src and its subtree under parent, after the child after; parent, or 0 */
BxceNodeId ScebXCe_62C76804(BxceDocument *doc, BxceNodeId parent, BxceNodeId after,
			    const BxceDocument *src, BxceNodeId node)
{
	if (doc->isBinary) {
		doc->status = SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
		return 0;
	}

	if (parent == 0 || src == NULL || node == 0) {
		doc->status = SCE_BXCE_ERROR_INVALID_ARGUMENT;
		return 0;
	}

	if (((BxceDomNode *)parent)->child == NULL)
		((BxceDomNode *)parent)->info |= SCE_XML_TOKEN_DOM_HAS_CHILD;

	bxceDocumentCopyNodes(doc, src, node, after, parent, node);
	if (doc->status != 0) {
		doc->status = 0;
		return 0;
	}
	return parent;
}

/* a new list of the node's children */
BxceNodeList *ScebXCe_2B9B57AE(const BxceDocument *doc, BxceNodeId node)
{
	BxceNodeList *list;
	BxceNodeId child;

	list = (BxceNodeList *)ScebXCe_B8DDE741(sizeof(BxceNodeList));
	if (list == NULL)
		return NULL;

	bxceNodeListInitialize(list, doc);
	for (child = ScebXCe_FEA6E4A1(doc, node); child != 0; child = ScebXCe_6FC9DC3F(doc, child))
		bxceNodeListInsertLast(list, child);
	return list;
}

/* the value of the element's attribute of this name, or NULL */
const char *ScebXCe_753D9382(const BxceDocument *doc, BxceNodeId node, const char *name)
{
	BxceNodeId attr;

	if (name == NULL || node == 0)
		return NULL;

	for (attr = ScebXCe_B034C595(doc, node); attr != 0; attr = ScebXCe_134F491C(doc, attr)) {
		if (bxceStrncmpTag(ScebXCe_B4989A58(doc, attr), name, BXCE_ATTR_STRING_MAX) == 0)
			return ScebXCe_84383D8E(doc, attr);
	}
	return NULL;
}
