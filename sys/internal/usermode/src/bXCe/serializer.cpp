/*
 * ScebXCe's XML writer: a document's nodes written back as XML text, in a buffer that grows as
 * it fills.
 */
#include "bxce_internal.h"
#include <xml/xml_node_types.h>

/* what an entity node stands for, by its entity type */
const char *g_bxceSerializerEntityText[5] = { "&", "\"", "<", ">", "'" };

const char *g_bxceXmlDeclaration = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";

BxceSerializer *ScebXCe_DC8483D4(BxceSerializer *serializer)
{
	serializer->tagOpen = 0;
	return serializer;
}

BxceSerializer *ScebXCe_8EA5A077(BxceSerializer *serializer)
{
	serializer->tagOpen = 0;
	return serializer;
}

void bxceSerializerNop(void)
{
}

void ScebXCe_BE1AD985(void)
{
}

void ScebXCe_D24E80F2(void)
{
}

void bxceSerializerNop2(void)
{
}

/* room for extra more bytes: the buffer doubles up to 4 KiB, then grows by 512 bytes */
static inline int bxceSerializerReserve(char **buffer, SceUInt32 *length, SceUInt32 *capacity,
					SceUInt32 extra)
{
	SceUInt32 used = *length;
	SceUInt32 size;
	char *grown;

	if (*length + extra <= *capacity)
		return 0;

	size = *capacity + 512;
	if (*capacity <= 0x800)
		size = *capacity * 2;
	if (size <= *length + extra)
		size = *length + extra;
	grown = (char *)ScebXCe_B8DDE741(size);
	if (grown != NULL) {
		bxceMemcpy(grown, *buffer, used);
		*capacity = size;
		if (*buffer != NULL)
			ScebXCe_EC560647(*buffer);
	} else {
		grown = NULL;
	}
	*buffer = grown;
	if (grown == NULL)
		return SCE_BXCE_ERROR_NO_MEMORY;
	return 0;
}

/* the start tag that is still open gets its '>' before the content */
static inline void bxceSerializerCloseTag(BxceSerializer *serializer, char **buffer, SceUInt32 *length)
{
	if (serializer->tagOpen) {
		*length += bxceSprintf(*buffer + *length, ">");
		serializer->tagOpen = 0;
	}
}

/* writes the node, its siblings after it and their children */
int bxceSerializerWriteNodes(BxceSerializer *serializer, const BxceDocument *doc, BxceNodeId node,
			     char **buffer, SceUInt32 *length, SceUInt32 *capacity)
{
	const char *name = NULL;
	BxceNodeId sibling;
	int ret;

	if (node == 0)
		return 0;

	for (;;) {
		switch ((SceUInt32)ScebXCe_FA18A039(doc, node)) {
		case SCE_XML_TOKEN_DOM_ENTITY: {
			int type = ScebXCe_35CDE77C(doc, node);

			if (bxceSerializerReserve(buffer, length, capacity, 15) != 0)
				return SCE_BXCE_ERROR_NO_MEMORY;
			if (type >= 5)
				*length += bxceSprintf(*buffer + *length, "%s", bxceDocumentGetEntityText(doc, node));
			else
				*length += bxceSprintf(*buffer + *length, "%s", g_bxceSerializerEntityText[type]);
			break;
		}

		case SCE_XML_TOKEN_DOM_ELEMENT:
		case SCE_XML_TOKEN_DOM_COMPACT_ELEMENT: {
			BxceNodeId attr;

			name = ScebXCe_1189A2B1(doc, node);
			if (bxceSerializerReserve(buffer, length, capacity,
						  2 * bxceStrnlen(name, 0x100000) + 8) != 0)
				return SCE_BXCE_ERROR_NO_MEMORY;
			bxceSerializerCloseTag(serializer, buffer, length);
			*length += bxceSprintf(*buffer + *length, "<%s", name);

			for (attr = ScebXCe_B034C595(doc, node); attr != 0; attr = ScebXCe_134F491C(doc, attr)) {
				const char *attrName = ScebXCe_B4989A58(doc, attr);
				const char *attrValue = ScebXCe_84383D8E(doc, attr);
				SceUInt32 nameLength = 0;
				SceUInt32 valueLength = 0;

				if (attrName != NULL)
					nameLength = bxceStrnlen(attrName, 0x100000);
				if (attrValue != NULL)
					valueLength = bxceStrnlen(attrValue, 0x100000);
				if (bxceSerializerReserve(buffer, length, capacity, nameLength + valueLength + 5) != 0)
					return SCE_BXCE_ERROR_NO_MEMORY;
				*length += bxceSprintf(*buffer + *length, " %s=\"", attrName);
				*length += bxceSprintf(*buffer + *length, "%s\"", attrValue);
			}
			serializer->tagOpen = 1;
			break;
		}

		case SCE_XML_TOKEN_DOM_TEXT:
		case SCE_XML_TOKEN_DOM_TEXT | 0x20: {
			const char *text = ScebXCe_33825EFF(doc, node);
			SceUInt32 textLength;

			if (text == NULL)
				break;
			textLength = bxceStrnlen(text, 0x100000);
			if (bxceSerializerReserve(buffer, length, capacity, textLength + 2) != 0)
				return SCE_BXCE_ERROR_NO_MEMORY;
			bxceSerializerCloseTag(serializer, buffer, length);
			bxceMemcpy(*buffer + *length, text, textLength);
			*length += textLength;
			break;
		}

		case SCE_XML_TOKEN_DOM_SKIP_TEXT_COMMENT: {
			const char *text = ScebXCe_2FAD3DFA(doc, node);

			if (text == NULL)
				break;
			if (bxceSerializerReserve(buffer, length, capacity, bxceStrnlen(text, 0x100000) + 7) != 0)
				return SCE_BXCE_ERROR_NO_MEMORY;
			bxceSerializerCloseTag(serializer, buffer, length);
			*length += bxceSprintf(*buffer + *length, "<!--%s", text);
			break;
		}

		default: {
			/* skipped text keeps its markup */
			const char *text = ScebXCe_2FAD3DFA(doc, node);
			SceUInt32 textLength;

			if (text == NULL)
				break;
			textLength = bxceStrnlen(text, 0x100000);
			if (bxceSerializerReserve(buffer, length, capacity, textLength + 2) != 0)
				return SCE_BXCE_ERROR_NO_MEMORY;
			bxceSerializerCloseTag(serializer, buffer, length);
			bxceMemcpy(*buffer + *length, text, textLength);
			*length += textLength;
			break;
		}
		}

		ret = bxceSerializerWriteNodes(serializer, doc, ScebXCe_FEA6E4A1(doc, node), buffer, length, capacity);
		if (ret != 0)
			return ret;
		sibling = ScebXCe_6FC9DC3F(doc, node);

		/* the element's end: "/>" while its start tag is open, else its end tag */
		if (name != NULL) {
			int written;

			if (bxceSerializerReserve(buffer, length, capacity, 2 * bxceStrnlen(name, 0x100000) + 6) != 0)
				return SCE_BXCE_ERROR_NO_MEMORY;
			if (serializer->tagOpen) {
				written = bxceSprintf(*buffer + *length, "/>");
				serializer->tagOpen = 0;
			} else {
				written = bxceSprintf(*buffer + *length, "</%s>", name);
				name = NULL;
			}
			*length += written;
		}

		node = sibling;
		if (node == 0)
			return 0;
	}
}

/*
 * The document as XML text in a new buffer, after the XML declaration when asked; outSize gets
 * what is left of the buffer. A failed write frees the buffer, still reported as success.
 */
int ScebXCe_D7FE96A2(BxceSerializer *serializer, const BxceDocument *doc, char **out, SceUInt32 *outSize,
		     SceUInt8 writeDeclaration)
{
	char *buffer;
	SceUInt32 length;
	SceUInt32 capacity;
	SceUInt32 size;
	SceUInt32 total;

	if (doc == NULL)
		return SCE_BXCE_ERROR_INVALID_ARGUMENT;

	total = doc->nodesSize + doc->textsSize + doc->elementNamesSize + doc->attrNamesSize;
	size = 512;
	if (total + 32 >= 400)
		size = total + 288;
	buffer = (char *)ScebXCe_B8DDE741(size);
	if (buffer == NULL)
		return SCE_BXCE_ERROR_NO_MEMORY;

	length = 0;
	if (writeDeclaration) {
		bxceStrlcpy(buffer, g_bxceXmlDeclaration, 0x100000);
		length += bxceStrnlen(buffer, 0x100000);
	}
	serializer->tagOpen = 0;
	capacity = size - length;
	if (bxceSerializerWriteNodes(serializer, doc, ScebXCe_CED6BECF(doc), &buffer, &length, &capacity) != 0 &&
	    buffer != NULL)
		ScebXCe_EC560647(buffer);
	*out = buffer;
	if (outSize != NULL)
		*outSize = capacity;
	return 0;
}
