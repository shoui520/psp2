/*
 * bxce_ext.cpp: R/W DOM and utility extensions of the bXCe core linked into
 * SceLibXml (vs0:sys/external/libSceXml.suprx).
 */
#include "libSceXml_internal.h"
#include <xml/xml_node_types.h>

static inline int bxceExtLengthSize(unsigned char lead)
{
	if (lead <= 0xEFu)
		return 1;
	switch (lead) {
	case 0xF0u:
		return 3;
	case 0xF1u:
	case 0xF4u:
		return 4;
	default:
		return 1;
	}
}

__attribute__((noinline)) int bxceStrncpySafe(void *dst, SceSize maxLen, const char *src, SceSize len)
{
	if (dst == NULL || src == NULL || maxLen == 0)
		return -1;
	SceSize copyLen = len;
	if (len == (SceSize)-2)
		copyLen = sceClibStrnlen(src, maxLen);
	if (copyLen >= maxLen)
		copyLen = maxLen - 1;
	sceClibMemcpy_safe(dst, src, copyLen);
	((char *)dst)[copyLen] = '\0';
	return 0;
}

int bxceAttrClone(BxceAttr *dst, const BxceAttr *src)
{
	dst->nameLength = 0;
	dst->valueLength = 0;
	dst->next = NULL;
	dst->reserved = 0;

	dst->name = (char *)ScebXCe_B8DDE741(BXCE_ATTR_INITIAL_SIZE);
	if (dst->name == NULL)
		return SCE_BXCE_ERROR_NO_MEMORY;
	bxceStrncpySafe(dst->name, BXCE_ATTR_INITIAL_SIZE, src->name, (SceSize)-2);
	dst->nameCapacity = src->nameCapacity;
	dst->nameLength = src->nameLength;

	dst->value = (char *)ScebXCe_B8DDE741(BXCE_ATTR_INITIAL_SIZE);
	if (dst->value == NULL) {
		if (dst->name != NULL)
			ScebXCe_EC560647(dst->name);
		return SCE_BXCE_ERROR_NO_MEMORY;
	}
	bxceStrncpySafe(dst->value, BXCE_ATTR_INITIAL_SIZE, src->value, (SceSize)-2);
	dst->valueCapacity = src->valueCapacity;
	dst->valueLength = src->valueLength;
	dst->next = NULL;
	return 0;
}

int bxceNodeListInitInvalid(BxceNodeList *list)
{
	list->doc = NULL;
	list->head = NULL;
	list->length = 0;
	list->tail = NULL;
	return SCE_BXCE_ERROR_NOT_SUPPORTED;
}

int bxceNodeListCloneInvalid(BxceNodeList *list)
{
	list->doc = NULL;
	list->head = NULL;
	list->length = 0;
	list->tail = NULL;
	return SCE_BXCE_ERROR_NOT_SUPPORTED;
}

int bxceDocumentCollectByTagNameLen(const BxceDocument *doc, BxceNodeId node,
				    const char *name, SceSize nameLen, BxceNodeList *list)
{
	int ret = 0;
	int type = ScebXCe_FA18A039(doc, node);
	if (type == SCE_XML_TOKEN_DOM_ELEMENT || type == SCE_XML_TOKEN_DOM_COMPACT_ELEMENT ||
	    type == SCE_XML_TOKEN_DOM_PRED_ELEMENT) {
		const char *nodeName = ScebXCe_1189A2B1(doc, node);
		if ((nodeName != NULL && bxceStrncmp(nodeName, name, nameLen) == 0) ||
		    bxceStrncmp(name, "*", 2) == 0) {
			if (bxceNodeListInsertLast(list, node) == 0)
				ret = SCE_BXCE_ERROR_NO_MEMORY;
		}
	}

	for (BxceNodeId child = ScebXCe_FEA6E4A1(doc, node); child != 0;
	     child = ScebXCe_6FC9DC3F(doc, child)) {
		int childRet = bxceDocumentCollectByTagNameLen(doc, child, name, nameLen, list);
		if (childRet != 0) {
			ScebXCe_1F59F0C4(list);
			return childRet;
		}
	}
	return ret;
}

int bxceDocumentGetElementsByTagNameStatus(const BxceDocument *doc, BxceNodeId node,
					   const char *name, SceSize nameLen,
					   BxceNodeList **outList)
{
	if (node == 0 || name == NULL || nameLen == 0)
		return SCE_BXCE_ERROR_INVALID_ARGUMENT;

	BxceNodeList *list = (BxceNodeList *)ScebXCe_B8DDE741(sizeof(BxceNodeList));
	*outList = list;
	if (list == NULL)
		return SCE_BXCE_ERROR_NO_MEMORY;

	bxceNodeListInitialize(list, doc);
	bxceDocumentCollectByTagNameLen(doc, node, name, nameLen, *outList);
	return 0;
}

void bxceDocumentConvertToDom(BxceDocument *doc, BxceNodeId bxNode, BxceDomNode *parentDom)
{
	while (bxNode != 0) {
		BxceDomNode *domNode = (BxceDomNode *)ScebXCe_B8DDE741(sizeof(BxceDomNode));
		bxceMemset(domNode, 0, sizeof(BxceDomNode));
		if (domNode == NULL)
			return;
		domNode->child = NULL;
		domNode->sibling = NULL;

		const SceUInt8 *raw = (const SceUInt8 *)doc->nodes + bxNode;
		SceUInt32 nameIdx = ((SceUInt32)raw[1] << 8) | raw[2];
		domNode->info = nameIdx;

		BxceNodeId firstAttr = ScebXCe_B034C595(doc, bxNode);
		if (firstAttr != 0) {
			BxceNodeId attrEnd = ScebXCe_FEA6E4A1(doc, bxNode);
			if (attrEnd == 0)
				attrEnd = ScebXCe_6FC9DC3F(doc, bxNode);
			if (attrEnd == 0) {
				BxceNodeId lastAttr = firstAttr;
				for (BxceNodeId it = firstAttr; it != 0;
				     it = ScebXCe_134F491C(doc, it))
					lastAttr = it;
				const SceUInt8 *ap = (const SceUInt8 *)doc->nodes + lastAttr;
				int off = ((ap[0] & SCE_XML_TOKEN_DOM_HAS_SIBLING) != 0) ? 1 : 0;
				off += 1 + bxceExtLengthSize(ap[off + 1]);
				off += bxceExtLengthSize(ap[off]);
				attrEnd = lastAttr + (BxceNodeId)off;
			}

			SceUInt32 attrLen = attrEnd - firstAttr;
			char *attrBlock = (char *)ScebXCe_B8DDE741(attrLen + 2);
			domNode->text = (SceUInt32)(uintptr_t)attrBlock;
			if (attrBlock == NULL)
				return;
			attrBlock[0] = (char)(attrLen + 1);
			bxceMemcpy(attrBlock + 1, doc->nodes + firstAttr, attrLen);
			attrBlock[attrLen + 1] = '\0';
		} else {
			domNode->text = 0;
		}

		SceUInt8 tok = ((const SceUInt8 *)doc->nodes)[bxNode];
		if (tok == SCE_XML_TOKEN_DOM_TEXT || tok == SCE_XML_TOKEN_DOM_HAS_TEXT_SIBLING) {
			domNode->info = tok;
			SceUInt32 textOff = (SceUInt32)bxceReadLength(
				(const SceUInt8 *)doc->nodes + bxNode + 1);
			SceUInt32 encoded = 0;
			SceUInt32 pos = 0;
			bxceWriteLength(doc, textOff, (char *)&encoded, &pos);
			domNode->text = encoded;
		} else {
			domNode->info = (domNode->info << 8) | tok;
		}

		if (parentDom->child != NULL) {
			BxceDomNode *tail = parentDom->child;
			while (tail->sibling != NULL)
				tail = tail->sibling;
			tail->sibling = domNode;
		} else {
			parentDom->child = domNode;
		}

		bxceDocumentConvertToDom(doc, ScebXCe_FEA6E4A1(doc, bxNode), domNode);
		bxNode = ScebXCe_6FC9DC3F(doc, bxNode);
	}
}

void bxceDocumentConvertToBx(BxceDocument *doc, BxceDomNode *domNode, BxceDocument *dstDoc,
			     SceUInt32 elemNameBase, SceUInt32 textBase,
			     SceUInt32 attrNameBase, SceUInt32 *childLenPos, SceUInt32 *writePos)
{
	while (domNode != NULL) {
		if (doc->status != 0)
			return;

		if (dstDoc->nodesCapacity <= *writePos + 12) {
			dstDoc->nodes = bxceGrowTable(dstDoc->nodes, dstDoc->nodesSize + 24,
						      &dstDoc->nodesCapacity);
			if (dstDoc->nodes == NULL) {
				dstDoc->status = SCE_BXCE_ERROR_NO_MEMORY;
				return;
			}
		}

		SceUInt32 nodeStart = *writePos;
		SceUInt32 sibPatchPos = nodeStart + 3;
		doc->writeCursor = dstDoc->nodes + nodeStart;
		dstDoc->nodes[nodeStart] = (char)(domNode->info & 0xFFu);
		*writePos += 1;

		int tokLow = dstDoc->nodes[nodeStart] & 0x0F;
		int isElem = 0;
		if (tokLow == SCE_XML_TOKEN_DOM_COMPACT_ELEMENT ||
		    tokLow == SCE_XML_TOKEN_DOM_ELEMENT) {
			SceUInt32 nameOff = elemNameBase + (domNode->info >> 8);
			dstDoc->nodes[nodeStart + 2] = (char)nameOff;
			dstDoc->nodes[nodeStart + 1] = (char)(nameOff >> 8);
			isElem = 1;
			*writePos += 6;
			tokLow = dstDoc->nodes[nodeStart] & 0x0F;
		} else if (domNode->sibling != NULL) {
			*writePos = nodeStart + 2;
			tokLow = dstDoc->nodes[nodeStart] & 0x0F;
		}

		if (tokLow == SCE_XML_TOKEN_DOM_TEXT || tokLow == SCE_XML_TOKEN_DOM_TEXT_SKIPPED) {
			SceUInt32 textOff = (SceUInt32)bxceReadLength(
				(const SceUInt8 *)&domNode->text) + textBase;
			SceUInt32 pos = *writePos;
			if (textOff <= 0xEF) {
				dstDoc->nodes[pos] = (char)textOff;
				pos += 1;
			} else if (textOff <= 0xFFFF) {
				dstDoc->nodes[pos] = (char)0xF0;
				dstDoc->nodes[pos + 1] = (char)(textOff >> 8);
				dstDoc->nodes[pos + 2] = (char)textOff;
				pos += 3;
			} else {
				dstDoc->nodes[pos] = (char)0xF1;
				dstDoc->nodes[pos + 1] = (char)(textOff >> 16);
				dstDoc->nodes[pos + 2] = (char)(textOff >> 8);
				dstDoc->nodes[pos + 3] = (char)textOff;
				pos += 4;
			}
			isElem = 0;
			*writePos = pos;
		}

		*childLenPos = 0;
		if (domNode->child != NULL) {
			*writePos += 1;
			*childLenPos = *writePos;
		}

		const SceUInt8 *attrData = (const SceUInt8 *)(uintptr_t)domNode->text;
		if (attrData != NULL && isElem != 0) {
			int totalLen = attrData[0];
			if (dstDoc->nodesCapacity <= *writePos + (SceUInt32)totalLen + 12) {
				dstDoc->nodes = bxceGrowTable(dstDoc->nodes,
							      dstDoc->nodesSize + 24,
							      &dstDoc->nodesCapacity);
				if (dstDoc->nodes == NULL) {
					dstDoc->status = SCE_BXCE_ERROR_NO_MEMORY;
					return;
				}
			}

			int idx = 1;
			doc->writeCursor = (char *)(attrData + 1);
			while (idx < totalLen) {
				dstDoc->nodes[*writePos] = *doc->writeCursor;
				const SceUInt8 *cur = (const SceUInt8 *)doc->writeCursor;
				int hasCount = (cur[0] & SCE_XML_TOKEN_DOM_HAS_SIBLING) != 0 ? 1 : 0;
				*writePos += 1;
				int nextIdx = idx + 2;
				if (hasCount) {
					dstDoc->nodes[*writePos] = (char)cur[1];
					*writePos += 1;
					nextIdx = idx + 3;
					cur = (const SceUInt8 *)doc->writeCursor;
				}
				bxceWriteLength(doc, (SceUInt32)cur[hasCount + 1] + attrNameBase,
						dstDoc->nodes, writePos);
				SceUInt32 valOff = (SceUInt32)bxceReadLength(
					(const SceUInt8 *)doc->writeCursor + hasCount + 2) + textBase;
				SceUInt32 wp = *writePos;
				dstDoc->nodes[wp + 3] = (char)valOff;
				dstDoc->nodes[wp + 2] = (char)(valOff >> 8);
				dstDoc->nodes[wp + 1] = (char)(valOff >> 16);
				dstDoc->nodes[wp] = (char)(valOff >> 24);
				idx = nextIdx + 1;
				doc->writeCursor = (char *)(attrData + idx);
			}
		}

		if (domNode->child != NULL)
			dstDoc->nodes[*childLenPos - 1] = (char)(*writePos - (*childLenPos - 1));

		bxceDocumentConvertToBx(doc, domNode->child, dstDoc,
					elemNameBase, textBase, attrNameBase,
					childLenPos, writePos);

		domNode = domNode->sibling;
		if (domNode == NULL)
			break;

		SceUInt32 dist = *writePos - sibPatchPos;
		dstDoc->nodes[sibPatchPos + 3] = (char)dist;
		dstDoc->nodes[sibPatchPos + 2] = (char)(dist >> 8);
		dstDoc->nodes[sibPatchPos + 1] = (char)(dist >> 16);
		dstDoc->nodes[nodeStart + 3] = (char)(dist >> 24);
	}
}

BxceNodeId bxceDocumentGetLastChild(const BxceDocument *doc, BxceNodeId node)
{
	BxceNodeId last = 0;
	for (BxceNodeId it = ScebXCe_FEA6E4A1(doc, node); it != 0;
	     it = ScebXCe_6FC9DC3F(doc, it))
		last = it;
	return last;
}

int bxceDocumentWriteDomAttrs(BxceDocument *doc, BxceDomNode *node,
			      const BxceAttributeList *attrs)
{
	char buffer[BXCE_DOM_ATTR_BUFFER_SIZE];

	if (attrs->length == 0) {
		doc->status = 0;
		return 0;
	}
	if (attrs->length > BXCE_DOM_ATTR_MAX) {
		doc->status = SCE_BXCE_ERROR_DOM_MAX_NUM_OF_ATTR_ERROR;
		return SCE_BXCE_ERROR_DOM_MAX_NUM_OF_ATTR_ERROR;
	}

	for (int i = 0; i < attrs->length; ++i) {
		const BxceAttr *attr = ScebXCe_01DC4E22(attrs, (SceSize)i);
		SceUInt32 needed = doc->attrNamesSize + (SceUInt32)attr->nameLength + 5;
		if (doc->attrNamesCapacity <= needed) {
			doc->attrNames = bxceGrowTable(doc->attrNames, needed,
						       &doc->attrNamesCapacity);
			if (doc->attrNames == NULL) {
				doc->status = SCE_BXCE_ERROR_NO_MEMORY;
				return SCE_BXCE_ERROR_NO_MEMORY;
			}
		}

		if (attr->nameLength > BXCE_DOM_ATTR_NAME_MAX)
			doc->status = SCE_BXCE_ERROR_EXCEED_LIMIT;

		int nameOff = bxceNameHashIntern(doc->attrNameHash, attr->name, attr->nameLength,
						 doc->attrNames, &doc->attrNamesSize);
		if (nameOff >= BXCE_DOM_UNIQUE_NAME_MAX) {
			doc->status = SCE_BXCE_ERROR_DOM_MAX_UNIQUE_ATTR_ERROR;
			return SCE_BXCE_ERROR_DOM_MAX_UNIQUE_ATTR_ERROR;
		}
		if (nameOff == -1) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return SCE_BXCE_ERROR_NO_MEMORY;
		}

		doc->pendingAttr[2 * i] = (SceUInt32)nameOff;
		doc->pendingAttr[2 * i + 1] = doc->textsSize;
		if (bxceDocumentAddText(doc, attr->value, (SceUInt32)attr->valueLength) != 0) {
			if (node != NULL)
				ScebXCe_EC560647(node);
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return SCE_BXCE_ERROR_NO_MEMORY;
		}
	}

	SceUInt32 written = 0;
	doc->nodesCapacity = BXCE_DOM_ATTR_BUFFER_SIZE;
	if (bxceDocumentWriteAttributes(doc, doc->pendingAttr, (SceUInt32)attrs->length,
					buffer, &written, 0) != 0) {
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return SCE_BXCE_ERROR_NO_MEMORY;
	}

	char *block = (char *)ScebXCe_B8DDE741(written + 2);
	node->text = (SceUInt32)(uintptr_t)block;
	if (block == NULL) {
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return SCE_BXCE_ERROR_NO_MEMORY;
	}

	bxceMemcpy(block + 1, buffer, written);
	block[0] = (char)(written + 1);
	block[written + 1] = (char)written;
	node->info |= SCE_XML_TOKEN_DOM_HAS_ATTR;
	doc->status = 0;
	return 0;
}

BxceNodeId bxceDocumentGetXmlMeta(const BxceDocument *doc)
{
	if (doc->nodes == NULL || !doc->isBinary)
		return 0;

	const char *p = doc->nodes + 2;
	if ((*p & 0x0F) != SCE_XML_TOKEN_DOM_TEXT_SKIPPED &&
	    (*p & 0x07) != SCE_XML_TOKEN_DOM_SKIP_TEXT_CDATA)
		return 0;
	return (BxceNodeId)(uintptr_t)p;
}

int bxceDocumentMergeTables(BxceDocument *dst, const BxceDocument *src)
{
	dst->status = 0;

	SceUInt32 elemTotal = dst->elementNamesSize + src->elementNamesSize;
	if (elemTotal != 0 && (src->elementNames != NULL || dst->elementNames != NULL)) {
		char *oldElem = dst->elementNames;
		char *newElem = (char *)ScebXCe_B8DDE741(elemTotal);
		dst->elementNames = newElem;
		if (newElem == NULL) {
			dst->status = SCE_BXCE_ERROR_NO_MEMORY;
			return SCE_BXCE_ERROR_NO_MEMORY;
		}
		bxceMemcpy(newElem, oldElem, dst->elementNamesSize);
		if (!dst->isExternal && oldElem != NULL)
			ScebXCe_EC560647(oldElem);
		bxceMemcpy(dst->elementNames + dst->elementNamesSize,
			   src->elementNames, src->elementNamesSize);
	}

	SceUInt32 attrTotal = dst->attrNamesSize + src->attrNamesSize;
	if (attrTotal != 0 && (src->attrNames != NULL || dst->attrNames != NULL)) {
		char *oldAttr = dst->attrNames;
		char *newAttr = (char *)ScebXCe_B8DDE741(attrTotal);
		dst->attrNames = newAttr;
		if (newAttr == NULL) {
			dst->status = SCE_BXCE_ERROR_NO_MEMORY;
			return SCE_BXCE_ERROR_NO_MEMORY;
		}
		bxceMemcpy(newAttr, oldAttr, dst->attrNamesSize);
		if (!dst->isExternal && oldAttr != NULL)
			ScebXCe_EC560647(oldAttr);
		bxceMemcpy(dst->attrNames + dst->attrNamesSize,
			   src->attrNames, src->attrNamesSize);
	}

	SceUInt32 textTotal = dst->textsSize + src->textsSize;
	if (textTotal != 0 && (src->texts != NULL || dst->texts != NULL)) {
		char *oldTexts = dst->texts;
		char *newTexts = (char *)ScebXCe_B8DDE741(textTotal);
		dst->texts = newTexts;
		if (newTexts == NULL) {
			dst->status = SCE_BXCE_ERROR_NO_MEMORY;
			return SCE_BXCE_ERROR_NO_MEMORY;
		}
		bxceMemcpy(newTexts, oldTexts, dst->textsSize);
		if (!dst->isExternal && oldTexts != NULL)
			ScebXCe_EC560647(oldTexts);
		bxceMemcpy(dst->texts + dst->textsSize, src->texts, src->textsSize);
	}

	return 0;
}

int bxceDocumentImportParent(BxceDocument *doc, BxceDocument *parentDoc, BxceNodeId parent)
{
	if (parentDoc == NULL || parent == 0) {
		doc->status = SCE_BXCE_ERROR_INVALID_ARGUMENT;
		return SCE_BXCE_ERROR_INVALID_ARGUMENT;
	}

	doc->status = 0;
	if (parentDoc->isBinary) {
		doc->status = SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
		return SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
	}

	if (!doc->isBinary) {
		BxceNodeId oldRoot = ScebXCe_CED6BECF(doc);
		BxceDomNode *docRoot = doc->domRoot;
		BxceNodeId parentRoot = ScebXCe_CED6BECF(parentDoc);
		if (doc->isBinary || docRoot == NULL || parentRoot == 0) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return SCE_BXCE_ERROR_NO_MEMORY;
		}
		if (docRoot->child == NULL)
			docRoot->info |= SCE_XML_TOKEN_DOM_HAS_CHILD;
		bxceDocumentCopyNodes(doc, parentDoc, parentRoot, 0, (BxceNodeId)docRoot, parentRoot);
		if (doc->status != 0) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return SCE_BXCE_ERROR_NO_MEMORY;
		}

		BxceNodeId newRoot = ScebXCe_CED6BECF(doc);
		BxceNodeId child = ScebXCe_FEA6E4A1(doc, newRoot);
		if (child != 0) {
			BxceNodeId lastChild = child;
			while ((child = ScebXCe_6FC9DC3F(doc, child)) != 0)
				lastChild = child;
			BxceNodeId grandChild = ScebXCe_FEA6E4A1(doc, lastChild);
			if (grandChild != 0) {
				BxceNodeId lastGrand = grandChild;
				while ((grandChild = ScebXCe_6FC9DC3F(doc, grandChild)) != 0)
					lastGrand = grandChild;
				ScebXCe_3BF2F11B(doc, lastGrand, 0, oldRoot);
			} else {
				ScebXCe_3BF2F11B(doc, lastChild, 0, oldRoot);
			}
		} else {
			ScebXCe_3BF2F11B(doc, newRoot, 0, oldRoot);
		}
		((BxceDomNode *)newRoot)->sibling = NULL;
		return doc->status;
	}

	if (bxceDocumentMergeTables(doc, parentDoc) != 0) {
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return SCE_BXCE_ERROR_NO_MEMORY;
	}

	parentDoc->nodes = (char *)ScebXCe_B8DDE741(BXCE_TABLE_INITIAL_SIZE);
	if (parentDoc->nodes == NULL) {
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return SCE_BXCE_ERROR_NO_MEMORY;
	}

	bxceMemset(parentDoc->nodes, 0, BXCE_TABLE_INITIAL_SIZE);
	parentDoc->nodes[0] = 0;
	parentDoc->nodes[1] = SCE_XML_TOKEN_DOM_ROOT_DOCUMENT;
	parentDoc->nodesSize = 2;
	parentDoc->nodesCapacity = BXCE_TABLE_INITIAL_SIZE;

	SceUInt32 writePos = 2;
	SceUInt32 childLenPos = 0;
	BxceDomNode *parentFirst = (BxceDomNode *)ScebXCe_FEA6E4A1(parentDoc,
								   (BxceNodeId)parentDoc->domRoot);
	bxceDocumentConvertToBx(doc, parentFirst, parentDoc,
				doc->elementNamesSize, doc->textsSize, doc->attrNamesSize,
				&childLenPos, &writePos);

	if (doc->status != 0) {
		doc->status = 0;
		return SCE_BXCE_ERROR_NO_MEMORY;
	}

	parentDoc->nodesSize = writePos;
	int lastTok = *doc->writeCursor & 0x0F;
	if (lastTok == SCE_XML_TOKEN_DOM_ELEMENT || lastTok == SCE_XML_TOKEN_DOM_COMPACT_ELEMENT) {
		parentDoc->nodes[parentDoc->nodesSize] = 1;
		parentDoc->nodesSize += 1;
		*doc->writeCursor |= SCE_XML_TOKEN_DOM_HAS_CHILD;
	} else {
		*doc->writeCursor |= SCE_XML_TOKEN_DOM_HAS_SIBLING;
		parentDoc->nodes[parentDoc->nodesSize] = 1;
		parentDoc->nodesSize += 1;
	}

	char *combined = (char *)ScebXCe_B8DDE741(doc->nodesSize + parentDoc->nodesSize);
	if (combined == NULL) {
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return SCE_BXCE_ERROR_NO_MEMORY;
	}

	bxceMemcpy(combined, parentDoc->nodes, parentDoc->nodesSize);
	bxceMemcpy(combined + parentDoc->nodesSize, doc->nodes + 2, doc->nodesSize - 2);
	doc->elementNamesSize += parentDoc->elementNamesSize;
	doc->attrNamesSize += parentDoc->attrNamesSize;
	doc->textsSize += parentDoc->textsSize;
	doc->nodesSize += parentDoc->nodesSize;
	parentDoc->nodesSize = 0;

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
		if (doc->nodes != NULL)
			ScebXCe_EC560647(doc->nodes);
	}

	doc->nodes = combined;
	doc->isExternal = 0;
	if (parentDoc->nodes != NULL)
		ScebXCe_EC560647(parentDoc->nodes);
	parentDoc->nodes = NULL;
	return 0;
}

BxceNodeId bxceDocumentSetWritable(BxceDocument *doc)
{
	doc->status = 0;
	if (!doc->isBinary)
		return 0;

	BxceDomNode *root = (BxceDomNode *)ScebXCe_B8DDE741(sizeof(BxceDomNode));
	if (root == NULL) {
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return 0;
	}
	root->child = NULL;
	root->sibling = NULL;
	root->text = 0;
	root->info = SCE_XML_TOKEN_DOM_ROOT_DOCUMENT;
	doc->domRoot = root;

	int shouldFreeNodes = 1;
	if (doc->isExternal) {
		SceUInt32 attrCap = doc->attrNamesSize > BXCE_NAMES_INITIAL_SIZE
					    ? doc->attrNamesSize
					    : BXCE_NAMES_INITIAL_SIZE;
		char *oldAttr = doc->attrNames;
		doc->attrNames = (char *)ScebXCe_B8DDE741(attrCap);
		if (doc->attrNames == NULL) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			ScebXCe_EC560647(root);
			return 0;
		}
		bxceMemset(doc->attrNames, 0, attrCap);
		bxceMemcpy(doc->attrNames, oldAttr, doc->attrNamesSize);

		SceUInt32 textCap = doc->textsSize > BXCE_TABLE_INITIAL_SIZE
					    ? doc->textsSize
					    : BXCE_TABLE_INITIAL_SIZE;
		char *oldTexts = doc->texts;
		doc->texts = (char *)ScebXCe_B8DDE741(textCap);
		if (doc->texts == NULL) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			ScebXCe_EC560647(root);
			return 0;
		}
		bxceMemset(doc->texts, 0, textCap);
		bxceMemcpy(doc->texts, oldTexts, doc->textsSize);

		SceUInt32 elemCap = doc->elementNamesSize > BXCE_TABLE_INITIAL_SIZE
					    ? doc->elementNamesSize
					    : BXCE_TABLE_INITIAL_SIZE;
		char *oldElem = doc->elementNames;
		doc->elementNames = (char *)ScebXCe_B8DDE741(elemCap);
		if (doc->elementNames == NULL) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			ScebXCe_EC560647(root);
			return 0;
		}
		bxceMemset(doc->elementNames, 0, elemCap);
		bxceMemcpy(doc->elementNames, oldElem, doc->elementNamesSize);

		doc->elementNameHash = (BxceNameHash *)ScebXCe_B8DDE741(sizeof(BxceNameHash));
		if (doc->elementNameHash == NULL) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			ScebXCe_EC560647(root);
			return 0;
		}
		bxceNameHashInitialize(doc->elementNameHash);

		doc->attrNameHash = (BxceNameHash *)ScebXCe_B8DDE741(sizeof(BxceNameHash));
		if (doc->attrNameHash == NULL) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			ScebXCe_EC560647(root);
			return 0;
		}
		bxceNameHashInitialize(doc->attrNameHash);

		shouldFreeNodes = 0;
		doc->isExternal = 0;
	}

	bxceDocumentConvertToDom(doc, ScebXCe_CED6BECF(doc), root);
	if (shouldFreeNodes && doc->nodes != NULL)
		ScebXCe_EC560647(doc->nodes);
	doc->nodes = NULL;
	doc->nodesSize = 0;
	doc->isBinary = 0;
	return (BxceNodeId)root;
}

int bxceDocumentSetText(BxceDocument *doc, BxceNodeId node, const char *text, SceUInt32 length)
{
	if (doc->isBinary) {
		doc->status = SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
		return SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
	}
	if (node == 0 || text == NULL || (int)length <= 0) {
		doc->status = SCE_BXCE_ERROR_INVALID_ARGUMENT;
		return SCE_BXCE_ERROR_INVALID_ARGUMENT;
	}

	doc->status = 0;
	int type = ScebXCe_FA18A039(doc, node);
	BxceDomNode *dn = (BxceDomNode *)node;
	char *texts = doc->texts;
	SceUInt32 offset = dn->text;
	if ((type & 8) == 0 && (type & 7) == 0)
		doc->status = SCE_BXCE_ERROR_DOM_NOT_ALLOWED_NODE_TYPE;

	if (offset > 0xEF)
		offset >>= 8;

	char *slot = texts + offset;
	SceUInt32 oldLen = (SceUInt32)bxceReadLength((const SceUInt8 *)slot);
	bxceMemset(slot, 0, oldLen);

	if ((int)oldLen >= (int)length) {
		char *dest;
		SceUInt32 nulPos;
		if ((int)length <= 0xEF) {
			slot[0] = (char)length;
			dest = slot + 1;
			nulPos = length + 1;
		} else if ((int)length <= 0xFFFF) {
			slot[0] = (char)0xF0;
			slot[1] = (char)(length >> 8);
			slot[2] = (char)length;
			dest = slot + 3;
			nulPos = length + 3;
		} else {
			slot[0] = (char)0xF1;
			slot[1] = (char)(length >> 16);
			slot[2] = (char)(length >> 8);
			slot[3] = (char)length;
			dest = slot + 4;
			nulPos = length + 4;
		}
		bxceMemcpy(dest, text, length);
		slot[nulPos] = '\0';
		return doc->status;
	}

	SceUInt32 newOff = doc->textsSize;
	if (newOff > 0xEF) {
		SceUInt32 encoded = 0;
		SceUInt32 pos = 0;
		bxceWriteLength(doc, newOff, (char *)&encoded, &pos);
		newOff = encoded;
	}
	dn->text = newOff;
	if (bxceDocumentAddText(doc, text, length) != 0) {
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return SCE_BXCE_ERROR_NO_MEMORY;
	}
	doc->status = 0;
	return 0;
}

int bxceDocumentSetAttribute(BxceDocument *doc, BxceNodeId node, const char *name,
			     const char *value, SceUInt32 valueLen)
{
	if (doc->isBinary) {
		doc->status = SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
		return SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
	}
	if (node == 0 || name == NULL || value == NULL) {
		doc->status = SCE_BXCE_ERROR_INVALID_ARGUMENT;
		return SCE_BXCE_ERROR_INVALID_ARGUMENT;
	}

	doc->status = 0;
	int type = ScebXCe_FA18A039(doc, node);
	if ((type & 4) == 0 && (type & 5) == 0) {
		doc->status = SCE_BXCE_ERROR_DOM_NOT_ALLOWED_NODE_TYPE;
		return SCE_BXCE_ERROR_DOM_NOT_ALLOWED_NODE_TYPE;
	}

	BxceDomNode *dn = (BxceDomNode *)node;
	SceUInt8 *existing = (SceUInt8 *)(uintptr_t)dn->text;
	if (existing == NULL) {
		SceSize nameLen = bxceStrnlen(name, BXCE_ATTR_STRING_MAX);
		if (doc->attrNamesCapacity <= doc->attrNamesSize + nameLen + 5) {
			doc->attrNames = bxceGrowTable(doc->attrNames,
						       doc->attrNamesSize + nameLen + 5,
						       &doc->attrNamesCapacity);
			if (doc->attrNames == NULL) {
				doc->status = SCE_BXCE_ERROR_NO_MEMORY;
				return SCE_BXCE_ERROR_NO_MEMORY;
			}
		}

		int nameOff = bxceNameHashIntern(doc->attrNameHash, name,
						 (int)bxceStrnlen(name, BXCE_ATTR_STRING_MAX),
						 doc->attrNames, &doc->attrNamesSize);
		char *block = (char *)ScebXCe_B8DDE741(9);
		if (nameOff == -1) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return SCE_BXCE_ERROR_NO_MEMORY;
		}
		SceUInt32 pos = 1;
		block[pos++] = SCE_XML_TOKEN_DOM_ATTR;
		bxceWriteLength(doc, (SceUInt32)nameOff, block, &pos);
		bxceWriteLength(doc, doc->textsSize, block, &pos);
		dn->text = (SceUInt32)(uintptr_t)block;
		block[0] = (char)pos;
		block[pos] = '\0';
		if (bxceDocumentAddText(doc, value, valueLen) != 0) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return SCE_BXCE_ERROR_NO_MEMORY;
		}
		dn->info |= SCE_XML_TOKEN_DOM_HAS_ATTR;
		return doc->status;
	}

	int curSize = existing[0];
	if ((SceUInt32)(curSize + 8) >= 0xFFu)
		return SCE_BXCE_ERROR_EXCEED_LIMIT;

	char *newBlock = (char *)ScebXCe_B8DDE741((SceSize)(curSize + 9));
	bxceMemcpy(newBlock, existing, (SceSize)(curSize + 1));

	BxceNodeId attrId = ScebXCe_B034C595(doc, node);
	while (attrId != 0) {
		const char *attrName = ScebXCe_B4989A58(doc, attrId);
		if (bxceStrncmpTag(attrName, name, BXCE_ATTR_STRING_MAX) == 0)
			break;
		attrId = ScebXCe_134F491C(doc, attrId);
	}

	if (attrId != 0) {
		SceUInt8 *ap = (SceUInt8 *)(uintptr_t)attrId;
		int off = ((ap[0] & SCE_XML_TOKEN_DOM_HAS_SIBLING) != 0) ? 1 : 0;
		off += 1 + bxceExtLengthSize(ap[off + 1]);
		SceUInt32 valOff = (SceUInt32)bxceReadLength(ap + off);
		char *slot = doc->texts + valOff;
		SceUInt32 oldLen = (SceUInt32)bxceReadLength((const SceUInt8 *)slot);
		bxceMemset(slot, 0, oldLen);

		if ((int)oldLen >= (int)valueLen) {
			char *dest;
			SceUInt32 nulPos;
			if ((int)valueLen <= 0xEF) {
				slot[0] = (char)valueLen;
				dest = slot + 1;
				nulPos = valueLen + 1;
			} else if ((int)valueLen <= 0xFFFF) {
				slot[0] = (char)0xF0;
				slot[1] = (char)(valueLen >> 8);
				slot[2] = (char)valueLen;
				dest = slot + 3;
				nulPos = valueLen + 3;
			} else {
				slot[0] = (char)0xF1;
				slot[1] = (char)(valueLen >> 16);
				slot[2] = (char)(valueLen >> 8);
				slot[3] = (char)valueLen;
				dest = slot + 4;
				nulPos = valueLen + 4;
			}
			bxceMemcpy(dest, value, valueLen);
			slot[nulPos] = '\0';
			if (newBlock != NULL)
				ScebXCe_EC560647(newBlock);
			return doc->status;
		}

		SceUInt8 *nameField = ap + (((ap[0] & SCE_XML_TOKEN_DOM_HAS_SIBLING) != 0) ? 1 : 0) + 1;
		if (doc->textsSize > 0xEF) {
			char encoded[4] = { 0, 0, 0, 0 };
			SceUInt32 encLen = 0;
			bxceWriteLength(doc, doc->textsSize, encoded, &encLen);
			SceSize relOff = (SceSize)(nameField - existing);
			bxceMemmove(newBlock + 1 + encLen + 1 + relOff,
				    newBlock + 1 + relOff,
				    (SceSize)(curSize - (int)relOff));
			bxceMemcpy(newBlock + relOff + 2, encoded, encLen);
			ScebXCe_EC560647(existing);
			dn->text = (SceUInt32)(uintptr_t)newBlock;
			newBlock[0] = (char)(curSize + (int)encLen - 1);
			newBlock[curSize + encLen] = '\0';
		} else {
			SceUInt32 pos = 1;
			bxceWriteLength(doc, doc->textsSize, (char *)nameField, &pos);
			if (newBlock != NULL)
				ScebXCe_EC560647(newBlock);
		}
	} else {
		SceSize nameLen = bxceStrnlen(name, BXCE_ATTR_STRING_MAX);
		if (doc->attrNamesCapacity <= doc->attrNamesSize + nameLen + 5) {
			doc->attrNames = bxceGrowTable(doc->attrNames,
						       doc->attrNamesSize + nameLen + 5,
						       &doc->attrNamesCapacity);
			if (doc->attrNames == NULL) {
				doc->status = SCE_BXCE_ERROR_NO_MEMORY;
				return SCE_BXCE_ERROR_NO_MEMORY;
			}
		}

		int nameOff = bxceNameHashIntern(doc->attrNameHash, name,
						 (int)bxceStrnlen(name, BXCE_ATTR_STRING_MAX),
						 doc->attrNames, &doc->attrNamesSize);
		if (nameOff == -1) {
			doc->status = SCE_BXCE_ERROR_NO_MEMORY;
			return SCE_BXCE_ERROR_NO_MEMORY;
		}

		SceUInt32 pos = (SceUInt32)(curSize - 1);
		char *body = newBlock + 1;
		body[pos++] = SCE_XML_TOKEN_DOM_ATTR;
		bxceWriteLength(doc, (SceUInt32)nameOff, body, &pos);
		bxceWriteLength(doc, doc->textsSize, body, &pos);
		ScebXCe_EC560647(existing);
		dn->text = (SceUInt32)(uintptr_t)newBlock;
		newBlock[0] = (char)(pos + 1);
		newBlock[pos + 1] = '\0';
	}

	if (bxceDocumentAddText(doc, value, valueLen) != 0) {
		doc->status = SCE_BXCE_ERROR_NO_MEMORY;
		return SCE_BXCE_ERROR_NO_MEMORY;
	}
	return doc->status;
}

int bxceDocumentSetAttrValue(BxceDocument *doc, BxceNodeId node, const char *name,
			     const char *value, SceUInt32 valueLen)
{
	return bxceDocumentSetAttribute(doc, node, name, value, valueLen);
}

int bxceDocumentSetAttributeList(BxceDocument *doc, BxceNodeId node,
				 const BxceAttributeList *attrs)
{
	doc->status = 0;
	if (doc->isBinary) {
		doc->status = SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
		return SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
	}
	if (node == 0 || attrs == NULL) {
		doc->status = SCE_BXCE_ERROR_INVALID_ARGUMENT;
		return SCE_BXCE_ERROR_INVALID_ARGUMENT;
	}

	BxceDomNode *dn = (BxceDomNode *)node;
	if (dn->text != 0)
		ScebXCe_EC560647((void *)(uintptr_t)dn->text);
	if (bxceDocumentWriteDomAttrs(doc, dn, attrs) == 0) {
		doc->status = 0;
		return 0;
	}
	return doc->status;
}

int bxceDocumentRemoveAttribute(BxceDocument *doc, BxceNodeId node, const char *name)
{
	doc->status = 0;
	if (doc->isBinary) {
		doc->status = SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
		return SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
	}
	if (node == 0) {
		doc->status = SCE_BXCE_ERROR_INVALID_ARGUMENT;
		return SCE_BXCE_ERROR_INVALID_ARGUMENT;
	}
	if (name == NULL)
		return 0;

	if (ScebXCe_FA18A039(doc, node) == SCE_XML_TOKEN_DOM_COMPACT_ELEMENT ||
	    ScebXCe_FA18A039(doc, node) == SCE_XML_TOKEN_DOM_ELEMENT) {
		BxceDomNode *dn = (BxceDomNode *)node;
		SceUInt32 oldBlock = dn->text;
		dn->text = 0;
		for (BxceNodeId attrId = oldBlock + 1; attrId != 0;
		     attrId = ScebXCe_134F491C(doc, attrId)) {
			const char *attrName = ScebXCe_B4989A58(doc, attrId);
			if (bxceStrncmpTag(attrName, name, BXCE_ATTR_STRING_MAX) != 0) {
				const char *attrVal = ScebXCe_84383D8E(doc, attrId);
				SceSize valLen = bxceStrnlen(attrVal, BXCE_ATTR_STRING_MAX);
				bxceDocumentSetAttribute(doc, node, attrName, attrVal, valLen);
			}
		}
		doc->status = 0;
		return 0;
	}
	return doc->status;
}

int bxceDocumentRemoveAttributes(BxceDocument *doc, BxceNodeId node)
{
	if (doc->isBinary) {
		doc->status = SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
		return SCE_BXCE_ERROR_DOM_READ_ONLY_ERROR;
	}
	if (node == 0) {
		doc->status = SCE_BXCE_ERROR_INVALID_ARGUMENT;
		return SCE_BXCE_ERROR_INVALID_ARGUMENT;
	}

	doc->status = 0;
	if (ScebXCe_FA18A039(doc, node) == SCE_XML_TOKEN_DOM_COMPACT_ELEMENT ||
	    ScebXCe_FA18A039(doc, node) == SCE_XML_TOKEN_DOM_ELEMENT) {
		BxceDomNode *dn = (BxceDomNode *)node;
		if (dn->text != 0)
			ScebXCe_EC560647((void *)(uintptr_t)dn->text);
		dn->text = 0;
		doc->status = 0;
		return 0;
	}
	return doc->status;
}

int bxceAttributeListCloneInit(BxceAttributeList *list)
{
	BxceAttrPool *pool = (BxceAttrPool *)ScebXCe_B8DDE741(sizeof(BxceAttrPool));
	list->pool = pool;
	if (pool == NULL)
		return SCE_BXCE_ERROR_NO_MEMORY;
	bxceAttrPoolInitialize(pool);
	list->head = NULL;
	list->tail = NULL;
	list->length = 0;
	return 0;
}

bool bxceDocumentHasChildNodes(const BxceDocument *doc, BxceNodeId node)
{
	return ScebXCe_FEA6E4A1(doc, node) != 0;
}

bool bxceDocumentHasAttributes(const BxceDocument *doc, BxceNodeId node)
{
	return ScebXCe_B034C595(doc, node) != 0;
}

BxceNodeList *bxceDocumentGetAttributes(const BxceDocument *doc, BxceNodeId node)
{
	if (node == 0)
		return NULL;

	BxceNodeList *list = (BxceNodeList *)ScebXCe_B8DDE741(sizeof(BxceNodeList));
	if (list == NULL)
		return NULL;

	bxceNodeListInitialize(list, doc);
	for (BxceNodeId attrId = ScebXCe_B034C595(doc, node); attrId != 0;
	     attrId = ScebXCe_134F491C(doc, attrId))
		bxceNodeListInsertLast(list, attrId);
	return list;
}
