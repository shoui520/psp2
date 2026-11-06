/*
 * ScebXCe's XML tokenizer, which the SAX parser and the document builder share: attribute names
 * and values read from the input chunk, entity references in attribute values decoded, and the
 * small scanners the token readers are built on.
 */
#include "bxce_internal.h"

typedef char BxceParserSizeCheck[sizeof(BxceParser) == 0xCC ? 1 : -1];

/* the class of each byte value */
static const SceUInt8 s_bxceByteType[256] = {
	/* 0x00 */
	BXCE_BT_NONXML, BXCE_BT_NONXML, BXCE_BT_NONXML, BXCE_BT_NONXML,
	BXCE_BT_NONXML, BXCE_BT_NONXML, BXCE_BT_NONXML, BXCE_BT_NONXML,
	BXCE_BT_NONXML, BXCE_BT_S, BXCE_BT_S, BXCE_BT_NONXML,
	BXCE_BT_NONXML, BXCE_BT_S, BXCE_BT_NONXML, BXCE_BT_NONXML,
	/* 0x10 */
	BXCE_BT_NONXML, BXCE_BT_NONXML, BXCE_BT_NONXML, BXCE_BT_NONXML,
	BXCE_BT_NONXML, BXCE_BT_NONXML, BXCE_BT_NONXML, BXCE_BT_NONXML,
	BXCE_BT_NONXML, BXCE_BT_NONXML, BXCE_BT_NONXML, BXCE_BT_NONXML,
	BXCE_BT_NONXML, BXCE_BT_NONXML, BXCE_BT_NONXML, BXCE_BT_NONXML,
	/* 0x20: ' ' to '/' */
	BXCE_BT_S, BXCE_BT_EXCL, BXCE_BT_QUOT, BXCE_BT_NUM,
	BXCE_BT_OTHER, BXCE_BT_PERCNT, BXCE_BT_AMP, BXCE_BT_APOS,
	BXCE_BT_LPAR, BXCE_BT_RPAR, BXCE_BT_AST, BXCE_BT_PLUS,
	BXCE_BT_COMMA, BXCE_BT_MINUS, BXCE_BT_PERIOD, BXCE_BT_SOL,
	/* 0x30: '0' to '?' */
	BXCE_BT_DIGIT, BXCE_BT_DIGIT, BXCE_BT_DIGIT, BXCE_BT_DIGIT,
	BXCE_BT_DIGIT, BXCE_BT_DIGIT, BXCE_BT_DIGIT, BXCE_BT_DIGIT,
	BXCE_BT_DIGIT, BXCE_BT_DIGIT, BXCE_BT_COLON, BXCE_BT_SEMI,
	BXCE_BT_LT, BXCE_BT_EQUALS, BXCE_BT_GT, BXCE_BT_QUEST,
	/* 0x40: '@' to 'O' */
	BXCE_BT_OTHER, BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER,
	BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER,
	BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER,
	BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER,
	/* 0x50: 'P' to '_' */
	BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER,
	BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER,
	BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LSQB,
	BXCE_BT_OTHER, BXCE_BT_RSQB, BXCE_BT_OTHER, BXCE_BT_NMSTRT,
	/* 0x60: '`' to 'o' */
	BXCE_BT_OTHER, BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER,
	BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER,
	BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER,
	BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER,
	/* 0x70: 'p' to DEL */
	BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER,
	BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER,
	BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_LETTER, BXCE_BT_OTHER,
	BXCE_BT_VERBAR, BXCE_BT_OTHER, BXCE_BT_OTHER, BXCE_BT_OTHER,
	/* 0x80 to 0xBF: UTF-8 continuation bytes */
	BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL,
	BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL,
	BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL,
	BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL,
	BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL,
	BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL,
	BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL,
	BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL,
	BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL,
	BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL,
	BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL,
	BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL,
	BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL,
	BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL,
	BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL,
	BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL, BXCE_BT_TRAIL,
	/* 0xC0 to 0xDF: leads of two bytes */
	BXCE_BT_LEAD2, BXCE_BT_LEAD2, BXCE_BT_LEAD2, BXCE_BT_LEAD2,
	BXCE_BT_LEAD2, BXCE_BT_LEAD2, BXCE_BT_LEAD2, BXCE_BT_LEAD2,
	BXCE_BT_LEAD2, BXCE_BT_LEAD2, BXCE_BT_LEAD2, BXCE_BT_LEAD2,
	BXCE_BT_LEAD2, BXCE_BT_LEAD2, BXCE_BT_LEAD2, BXCE_BT_LEAD2,
	BXCE_BT_LEAD2, BXCE_BT_LEAD2, BXCE_BT_LEAD2, BXCE_BT_LEAD2,
	BXCE_BT_LEAD2, BXCE_BT_LEAD2, BXCE_BT_LEAD2, BXCE_BT_LEAD2,
	BXCE_BT_LEAD2, BXCE_BT_LEAD2, BXCE_BT_LEAD2, BXCE_BT_LEAD2,
	BXCE_BT_LEAD2, BXCE_BT_LEAD2, BXCE_BT_LEAD2, BXCE_BT_LEAD2,
	/* 0xE0 to 0xEF: three bytes */
	BXCE_BT_LEAD3, BXCE_BT_LEAD3, BXCE_BT_LEAD3, BXCE_BT_LEAD3,
	BXCE_BT_LEAD3, BXCE_BT_LEAD3, BXCE_BT_LEAD3, BXCE_BT_LEAD3,
	BXCE_BT_LEAD3, BXCE_BT_LEAD3, BXCE_BT_LEAD3, BXCE_BT_LEAD3,
	BXCE_BT_LEAD3, BXCE_BT_LEAD3, BXCE_BT_LEAD3, BXCE_BT_LEAD3,
	/* 0xF0 to 0xFF: four, five and six bytes */
	BXCE_BT_LEAD4, BXCE_BT_LEAD4, BXCE_BT_LEAD4, BXCE_BT_LEAD4,
	BXCE_BT_LEAD4, BXCE_BT_LEAD4, BXCE_BT_LEAD4, BXCE_BT_LEAD4,
	BXCE_BT_LEAD5, BXCE_BT_LEAD5, BXCE_BT_LEAD5, BXCE_BT_LEAD5,
	BXCE_BT_LEAD6, BXCE_BT_LEAD6, BXCE_BT_LEAD6, BXCE_BT_LEAD6,
};

/* the declarations "<!" introduces, and the five predefined entities */
const char *g_bxceDoctype = "DOCTYPE";
const char *g_bxceCdata = "[CDATA[";
const char *g_bxceEntityLt = "lt";
const char *g_bxceEntityGt = "gt";
const char *g_bxceEntityAmp = "amp";
const char *g_bxceEntityApos = "apos";
const char *g_bxceEntityQuot = "quot";

/* the text of each predefined entity, by BXCE_REF_* */
const char *g_bxceEntityText[5] = { "&", "\"", "<", ">", "'" };

/* the NUL an attribute's name and value end with */
static char s_bxceTerminator;

/* a buffer of a function's own: made empty, given back when it goes */
class BxceStringBuffer : public BxceBuffer {
public:
	BxceStringBuffer()
	{
		capacity = BXCE_BUFFER_INITIAL_SIZE;
		data = (char *)ScebXCe_B8DDE741(BXCE_BUFFER_INITIAL_SIZE);
		if (data != NULL) {
			data[0] = 0;
			length = 0;
		}
	}

	~BxceStringBuffer()
	{
		if (data != NULL) {
			data[0] = 0;
			if (data != NULL)
				ScebXCe_EC560647(data);
			data = NULL;
			length = 0;
			capacity = 0;
		}
	}

	void clear()
	{
		if (data != NULL) {
			data[0] = 0;
			length = 0;
		}
	}

	int append(char c)
	{
		int ret = 0;

		if (capacity <= length) {
			ret = bxceBufferGrow(&data, &capacity, BXCE_BUFFER_INITIAL_SIZE);
			if (ret != 0)
				return ret;
		}
		data[length++] = c;
		data[length] = 0;
		return ret;
	}

	/* appends the other buffer; Sony's code sizes and copies it by its capacity */
	int append(const BxceStringBuffer &src)
	{
		const char *s;
		char *d;
		int ret = 0;
		int i;

		if (capacity + src.capacity <= length) {
			int extra = BXCE_BUFFER_INITIAL_SIZE;

			if (src.capacity > BXCE_BUFFER_INITIAL_SIZE)
				extra = src.capacity;
			ret = bxceBufferGrow(&data, &capacity, extra);
			if (ret != 0)
				return ret;
		}

		d = data + length;
		s = src.data;
		for (i = 0; i < src.capacity; i++)
			*d++ = *s++;
		length += src.length;
		data[length] = 0;
		return ret;
	}
};

int bxceAttrPoolInitialize(BxceAttrPool *pool)
{
	pool->count = 0;
	return 0;
}

BxceAttr *bxceAttrPoolPop(BxceAttrPool *pool)
{
	if (pool->count <= 0)
		return NULL;

	return pool->attr[--pool->count];
}

/* 1 when the pool keeps the attribute, 0 when it is full */
int bxceAttrPoolPush(BxceAttrPool *pool, BxceAttr *attr)
{
	int count = pool->count;

	if (count >= BXCE_ATTR_POOL_SIZE)
		return 0;

	pool->attr[count] = attr;
	pool->count = count + 1;
	return 1;
}

void bxceParserResetBuffer(BxceParser *parser, BxceBuffer *buffer)
{
	buffer->data = NULL;
	buffer->length = 0;
	buffer->capacity = 0;
}

/* frees the attribute list, the name stack, the element name, the characters and the attribute */
void bxceParserReleaseState(BxceParser *parser)
{
	if (parser->attrList != NULL) {
		ScebXCe_9C2F9897(parser->attrList);
		if (parser->attrList != NULL)
			ScebXCe_EC560647(parser->attrList);
		parser->attrList = NULL;
	}

	if (parser->nameStack != NULL) {
		bxceNameStackTerminate(parser->nameStack);
		if (parser->nameStack != NULL)
			ScebXCe_EC560647(parser->nameStack);
		parser->nameStack = NULL;
	}

	if (parser->characters.data != NULL) {
		ScebXCe_EC560647(parser->characters.data);
		parser->characters.data = NULL;
		parser->characters.length = 0;
		parser->characters.capacity = 0;
	}

	if (parser->elementName.data != NULL) {
		ScebXCe_EC560647(parser->elementName.data);
		parser->elementName.data = NULL;
		parser->elementName.length = 0;
		parser->elementName.capacity = 0;
	}

	if (parser->attr != NULL) {
		bxceAttrTerminate(parser->attr);
		if (parser->attr != NULL)
			ScebXCe_EC560647(parser->attr);
		parser->attr = NULL;
	}
}

/*
 * Reads an attribute's name up to '=' or white space: 1 when it ended (the name gets its NUL,
 * outside its length), 1 with the status set when out of memory, 0 when the chunk ended first.
 */
int bxceParserReadAttrName(BxceParser *parser, BxceAttr *attr)
{
	int position = parser->position;
	int start = position;
	int end = parser->size;
	const char *name = parser->input + position;
	const char *s;

	for (s = name; position < end; s++) {
		char c = *s;

		if (c == '=' || s_bxceByteType[(SceUInt8)c] == BXCE_BT_S) {
			if (position - start != 0 &&
			    bxceAttrAppendName(attr, name, position - start) != 0) {
				parser->status = SCE_BXCE_ERROR_NO_MEMORY;
				return 1;
			}
			if (bxceAttrAppendName(attr, &s_bxceTerminator, 1) != 0)
				parser->status = SCE_BXCE_ERROR_NO_MEMORY;
			attr->nameLength--;
			return 1;
		}
		parser->position = ++position;
	}

	if (position - start != 0 && bxceAttrAppendName(attr, name, position - start) != 0) {
		parser->status = SCE_BXCE_ERROR_NO_MEMORY;
		return 1;
	}
	return 0;
}

/* reads an attribute's value up to its closing quote, as bxceParserReadAttrName its name */
int bxceParserReadAttrValue(BxceParser *parser, BxceAttr *attr)
{
	int position = parser->position;
	int start = position;
	int end = parser->size;
	const char *value = parser->input + position;
	const char *s;

	if (position < end) {
		char quote = parser->quote;

		for (s = value; position < end; s++) {
			if (*s == quote) {
				if (position - start != 0 &&
				    bxceAttrAppendValue(attr, value, position - start) != 0) {
					parser->status = SCE_BXCE_ERROR_NO_MEMORY;
					return 1;
				}
				if (bxceAttrAppendValue(attr, &s_bxceTerminator, 1) != 0)
					parser->status = SCE_BXCE_ERROR_NO_MEMORY;
				attr->valueLength--;
				return 1;
			}
			parser->position = ++position;
		}
	}

	if (position - start != 0 && bxceAttrAppendValue(attr, value, position - start) != 0) {
		parser->status = SCE_BXCE_ERROR_NO_MEMORY;
		return 1;
	}
	return 0;
}

/* what the entity reference's name (between '&' and ';') is; a character reference becomes its UTF-8 */
int bxceParserLookupReference(BxceParser *parser, BxceBuffer *reference)
{
	SceUInt32 value;
	char *out;

	switch (reference->length) {
	case 2:
		if (bxceMemcmp(reference->data, g_bxceEntityLt, 2) == 0)
			return BXCE_REF_LT;
		if (bxceMemcmp(reference->data, g_bxceEntityGt, 2) == 0)
			return BXCE_REF_GT;
		break;
	case 3:
		if (bxceMemcmp(reference->data, g_bxceEntityAmp, 3) == 0)
			return BXCE_REF_AMP;
		break;
	case 4:
		if (bxceMemcmp(reference->data, g_bxceEntityApos, 4) == 0)
			return BXCE_REF_APOS;
		if (bxceMemcmp(reference->data, g_bxceEntityQuot, 4) == 0)
			return BXCE_REF_QUOT;
		break;
	}

	if (reference->data[0] != '#')
		return BXCE_REF_UNKNOWN;

	if (reference->data[1] == 'x') {
		if (ScebXCe_DFB92627(&value, reference->data + 2, reference->length - 2) != 0)
			return BXCE_REF_ERROR;
	} else {
		if (ScebXCe_CC190FA8(&value, reference->data + 1, reference->length - 1) != 0)
			return BXCE_REF_ERROR;
	}

	out = reference->data;
	if (ScebXCe_BD9D727B(value, &out, &reference->length) != 0)
		return BXCE_REF_ERROR;
	return BXCE_REF_CHAR;
}

/* replaces the references in an attribute's value by their text; an unknown one stays as it was */
int bxceParserDecodeAttrValue(BxceParser *parser, BxceAttr *attr)
{
	const char *s = attr->value;
	int valueLength = attr->valueLength;
	BxceStringBuffer value;
	BxceStringBuffer reference;
	int inReference = 0;
	int ret = 0;
	char c;

	if (valueLength != 0) {
		for (c = *s; c != 0; c = *++s) {
			if (inReference == 0 && c == '&') {
				inReference = 1;
			} else if (inReference == 1) {
				if (c == ';') {
					int type = bxceParserLookupReference(parser, &reference);

					if (type == BXCE_REF_ERROR) {
						ret = SCE_BXCE_ERROR_XML_SYNTAX_ERROR;
						goto fail;
					}
					if (type >= BXCE_REF_AMP && type <= BXCE_REF_APOS) {
						ret = value.append(*g_bxceEntityText[type]);
					} else if (type == BXCE_REF_CHAR) {
						ret = value.append(reference);
					} else {
						value.append('&');
						value.append(reference);
						ret = value.append(';');
					}
					if (ret != 0)
						break;
					reference.clear();
					inReference = 0;
				} else {
					reference.append(c);
				}
			} else {
				value.append(c);
			}
		}
		if (ret != 0)
			goto fail;
		if (inReference == 1) {
			ret = SCE_BXCE_ERROR_XML_EXPECTED_SEMI_COLON;
			goto fail;
		}

		if (value.length != 0) {
			ret = bxceAttrSetValue(attr, value.data, value.length);
			if (ret != 0) {
fail:
				parser->status = ret;
			}
		}
	}
	return ret;
}

/* 1 and past it when the next byte is c */
int bxceParserSkipChar(BxceParser *parser, char c)
{
	int position = parser->position;

	if (parser->input[position] != c)
		return 0;

	parser->position = position + 1;
	return 1;
}

/* 1 when it skipped white space */
int bxceParserSkipSpace(BxceParser *parser)
{
	int position = parser->position;
	int skipped = 0;
	int end = parser->size;

	if (position < end) {
		const char *s = parser->input + position;

		do {
			if (s_bxceByteType[(SceUInt8)*s] != BXCE_BT_S)
				break;
			parser->position = ++position;
			s++;
			skipped = 1;
		} while (position < end);
	}
	return skipped;
}

/*
 * Goes on with the attribute a chunk's end split, from the state it left: 1 when the attribute
 * was appended or an error set the status, 0 when the chunk ended again.
 */
int bxceParserResumeAttribute(BxceParser *parser, BxceAttributeList *list)
{
	int state = parser->state;
	int ret;

	if (state == BXCE_STATE_ATTR_NAME) {
		if (bxceParserReadAttrName(parser, parser->attr) == 0)
			return 0;
		if (parser->status != 0)
			return 1;
		state = BXCE_STATE_ATTR_EQUALS;
		parser->state = BXCE_STATE_ATTR_EQUALS;
	} else if (state >= BXCE_STATE_ATTR_QUOTE) {
		goto value;
	}

	bxceParserSkipSpace(parser);
	if (parser->position >= parser->size)
		return 0;
	if (parser->input[parser->position] != '=') {
		parser->status = SCE_BXCE_ERROR_XML_EXPECTED_EQUALITY_OPERATOR;
		return 1;
	}
	parser->position++;

value:
	if (state < BXCE_STATE_ATTR_QUOTE + 1) {
		bxceParserSkipSpace(parser);
		if (parser->position >= parser->size) {
			parser->state = BXCE_STATE_ATTR_QUOTE;
			return 0;
		}
		parser->quote = parser->input[parser->position];
		if (parser->input[parser->position] != '"' &&
		    parser->input[parser->position] != '\'') {
			parser->status = SCE_BXCE_ERROR_XML_EXPECTED_QUOTE;
			return 1;
		}
		parser->state = BXCE_STATE_ATTR_VALUE;
		parser->position++;
	} else if (state != BXCE_STATE_ATTR_VALUE) {
		goto close;
	}

	if (bxceParserReadAttrValue(parser, parser->attr) == 0)
		return 0;
	if (parser->status != 0)
		return 1;
	parser->state = BXCE_STATE_ATTR_CLOSE;

close:
	if (parser->position >= parser->size)
		return 0;
	if (parser->input[parser->position] != parser->quote) {
		parser->status = SCE_BXCE_ERROR_XML_EXPECTED_QUOTE;
		return 1;
	}
	parser->position++;

	if (parser->resolveAttrEntity &&
	    (ret = bxceParserDecodeAttrValue(parser, parser->attr)) != 0) {
		parser->status = ret;
		return 0;
	}
	ret = bxceAttributeListAppend(list, parser->attr);
	parser->attr = NULL;
	if (ret != 0) {
		parser->status = SCE_BXCE_ERROR_DUPLICATE_ATTRIBUTE_NAME;
		return 0;
	}
	return 1;
}

/* reads a start tag's attributes up to its '>' or "/": 1 when there, or on an error, 0 when the chunk ended */
int bxceParserReadAttributes(BxceParser *parser, BxceAttributeList *list)
{
	char c = parser->input[parser->position];
	BxceAttr *attr;
	int ret;

	if (c == '/')
		return 1;

	while (c != '>') {
		attr = bxceAttributeListNewAttr(list);
		parser->attr = attr;
		if (attr == NULL) {
			parser->status = SCE_BXCE_ERROR_NO_MEMORY;
			return 1;
		}
		if (bxceParserReadAttrName(parser, attr) == 0) {
			parser->state = BXCE_STATE_ATTR_NAME;
			return 0;
		}
		if (parser->status != 0)
			return 1;

		bxceParserSkipSpace(parser);
		if (parser->position >= parser->size) {
			parser->state = BXCE_STATE_ATTR_EQUALS;
			return 0;
		}
		if (parser->input[parser->position] != '=') {
			parser->status = SCE_BXCE_ERROR_XML_EXPECTED_EQUALITY_OPERATOR;
			return 1;
		}
		parser->position++;

		bxceParserSkipSpace(parser);
		if (parser->position >= parser->size) {
			parser->state = BXCE_STATE_ATTR_QUOTE;
			return 0;
		}
		parser->quote = parser->input[parser->position];
		if (parser->input[parser->position] != '"' &&
		    parser->input[parser->position] != '\'') {
			parser->status = SCE_BXCE_ERROR_XML_EXPECTED_QUOTE;
			return 1;
		}
		parser->state = BXCE_STATE_ATTR_VALUE;
		parser->position++;

		ret = bxceParserReadAttrValue(parser, parser->attr);
		attr = parser->attr;
		if (ret == 0)
			return 0;
		if (parser->status != 0)
			return 1;
		if (parser->position >= parser->size) {
			parser->state = BXCE_STATE_ATTR_CLOSE;
			return 0;
		}
		if (parser->input[parser->position] != parser->quote) {
			parser->status = SCE_BXCE_ERROR_XML_EXPECTED_QUOTE;
			return 1;
		}
		parser->position++;

		if (parser->resolveAttrEntity) {
			ret = bxceParserDecodeAttrValue(parser, attr);
			attr = parser->attr;
			if (ret != 0) {
				parser->status = ret;
				return 0;
			}
		}
		ret = bxceAttributeListAppend(list, attr);
		parser->attr = NULL;
		if (ret != 0) {
			parser->status = SCE_BXCE_ERROR_DUPLICATE_ATTRIBUTE_NAME;
			return 0;
		}

		if (parser->position >= parser->size)
			return 0;
		if (!bxceParserSkipSpace(parser))
			return 1;
		if (parser->position >= parser->size) {
			parser->state = BXCE_STATE_ATTR_NAME;
			return 0;
		}
		c = parser->input[parser->position];
		if (c == '/')
			return 1;
	}
	return 1;
}

/*
 * Reads a tag's name up to '>', '/' or white space: 1 when it ended (NUL-terminated, its capacity
 * set to its length) or out of memory, 0 when the chunk ended first.
 */
int bxceParserReadName(BxceParser *parser, BxceBuffer *name)
{
	int position = parser->position;

	if (position >= parser->size)
		return 0;

	for (;;) {
		const char *input = parser->input;
		int length = name->length;
		char c = input[position];

		if (c == '>' || c == '/' || s_bxceByteType[(SceUInt8)input[position]] == BXCE_BT_S) {
			name->data[length] = 0;
			name->capacity = name->length;
			return 1;
		}
		if (name->capacity <= length) {
			int ret = bxceBufferGrow(&name->data, &name->capacity, 0);

			input = parser->input;
			position = parser->position;
			length = name->length;
			if (ret != 0)
				break;
		}
		name->data[length] = input[position];
		name->length++;
		position = parser->position + 1;
		parser->position = position;
		if (position >= parser->size)
			return 0;
	}

	parser->status = SCE_BXCE_ERROR_NO_MEMORY;
	return 1;
}

/* reads a start tag's name and attributes: 1 when the tag's end is next or on an error, 0 when the chunk ended */
int bxceParserReadStartTag(BxceParser *parser, BxceBuffer *name, BxceAttributeList *list)
{
	int state = parser->state;

	if (state == BXCE_STATE_TAG_NAME) {
		if (name->data == NULL) {
			name->data = (char *)ScebXCe_B8DDE741(0x80);
			if (name->data == NULL) {
				parser->status = SCE_BXCE_ERROR_NO_MEMORY;
				return 1;
			}
			name->capacity = 0x80;
		}
		if (bxceParserReadName(parser, name) == 0)
			return 0;
		if (parser->status != 0)
			return 1;
		state = BXCE_STATE_ATTRIBUTES;
		parser->state = BXCE_STATE_ATTRIBUTES;
	}

	if (parser->position >= parser->size)
		return 0;
	if (parser->attr != NULL) {
		if (bxceParserResumeAttribute(parser, list) == 0)
			return 0;
		if (parser->status != 0)
			return 1;
		state = parser->state;
		if (parser->position >= parser->size)
			return 0;
	}

	if (!bxceParserSkipSpace(parser) && state != BXCE_STATE_ATTR_NAME)
		return 1;
	if (parser->position >= parser->size) {
		parser->state = BXCE_STATE_ATTR_NAME;
		return 0;
	}
	return bxceParserReadAttributes(parser, list) != 0;
}

/* character data up to '<' (or '&' when references are resolved): 1 when it ended there, 0 at the chunk's end */
int bxceParserScanText(BxceParser *parser, const char **text, int *length)
{
	int position = parser->position;
	int end = parser->size;
	int start = position;

	*text = parser->input + position;
	if (position < end) {
		const char *s = parser->input + position;

		do {
			char c = *s;

			if (c == '<' || (parser->resolveEntity && c == '&')) {
				*length = position - start;
				return 1;
			}
			parser->position = ++position;
			s++;
		} while (position < end);
	}

	*length = position - start;
	return 0;
}

/*
 * Matches an end tag's name against the innermost open element, a chunk's end at any place, then
 * its '>': 1 when done or on an error, 0 when the chunk ended.
 */
int bxceParserReadEndTag(BxceParser *parser)
{
	if (parser->state == BXCE_STATE_END_TAG_NAME) {
		int available, matched, remaining;
		const char *input;

		if (parser->elementName.data == NULL) {
			BxceNameStackEntry *entry = bxceNameStackPop(parser->nameStack);

			if (entry == NULL) {
				parser->status = SCE_BXCE_ERROR_XML_END_TAG_MISMATCH;
				return 1;
			}
			parser->elementName.capacity = entry->length;
			parser->elementName.data = entry->name;
			parser->elementName.length = 0;
			ScebXCe_EC560647(entry);
		}

		available = parser->size - parser->position;
		matched = parser->elementName.length;
		remaining = parser->elementName.capacity - matched;
		input = parser->input + parser->position;
		if (remaining > available) {
			if (bxceMemcmp(parser->elementName.data + matched, input, available) != 0) {
				parser->status = SCE_BXCE_ERROR_XML_END_TAG_MISMATCH;
				return 1;
			}
			parser->elementName.length += available;
			return 0;
		}
		if (bxceMemcmp(parser->elementName.data + matched, input, remaining) != 0) {
			parser->status = SCE_BXCE_ERROR_XML_END_TAG_MISMATCH;
			return 1;
		}
		matched = parser->elementName.length;
		parser->elementName.length = parser->elementName.capacity;
		parser->position = parser->position + parser->elementName.capacity - matched;
		parser->state = BXCE_STATE_END_TAG_CLOSE;
	}

	bxceParserSkipSpace(parser);
	parser->state = BXCE_STATE_END_TAG_CLOSE;
	if (parser->size == parser->position)
		return 0;
	if (parser->input[parser->position] == '>')
		parser->position++;
	else
		parser->status = SCE_BXCE_ERROR_XML_EXPECTED_CLOSING_ANGLE_BRACKET;
	return 1;
}
