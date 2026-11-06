/*
 * ScebXCe's XML tokenizer, which the SAX parser and the document builder share: attribute names
 * and values read from the input chunk, entity references in attribute values decoded, and the
 * small scanners the token readers are built on.
 */
#include "bxce_internal.h"
#include "byte_type.h"

typedef char BxceParserSizeCheck[sizeof(BxceParser) == 0xCC ? 1 : -1];

/* the declarations "<!" introduces, and the five predefined entities */
const char *g_bxceDoctype = "DOCTYPE";
const char *g_bxceCdata = "[CDATA[";
const char *g_bxceEntityLt = "lt";
const char *g_bxceEntityGt = "gt";
const char *g_bxceEntityAmp = "amp";
const char *g_bxceEntityApos = "apos";
const char *g_bxceEntityQuot = "quot";

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

/*
 * After a '<' the chunk's end split from what follows: which token the markup starts, the start
 * of a "<!" kept in pending until enough of it is here. 1 when the token is known, 0 when the
 * chunk ended first.
 */
int bxceParserReadMarkup(BxceParser *parser)
{
	int position = parser->position;
	int size = parser->size;
	int pending;
	int left;

	if (position >= size)
		return 0;

	pending = parser->pendingLength;
	if (pending == 0) {
		const char *input = parser->input;

		parser->tokenStart = input + position;
		switch (input[position]) {
		case '/':
			parser->token = BXCE_TOKEN_END_TAG;
			parser->position = position + 1;
			break;
		case '?':
			parser->token = BXCE_TOKEN_PI;
			parser->position = position + 1;
			break;
		case '!':
			parser->position = ++position;
			if (position >= size) {
				parser->pending[0] = '!';
				parser->pendingLength = 1;
				return 0;
			}
			parser->tokenStart = input + position;
			left = size - position;
			if (left > 1) {
				int differs = bxceMemcmp("--", input + position, 2);

				input = parser->input;
				position = parser->position;
				if (differs == 0) {
					parser->token = BXCE_TOKEN_COMMENT;
					parser->position = position + 2;
					break;
				}
			}
			if (left <= 6) {
				parser->pending[0] = '!';
				bxceMemcpy(parser->pending + 1, input + position, left);
				parser->pendingLength = left + 1;
				return 0;
			}
			if (bxceMemcmp(g_bxceDoctype, input + position, 7) == 0) {
				parser->token = BXCE_TOKEN_DTD;
				parser->position = parser->position + 7;
			} else if (bxceMemcmp(g_bxceCdata, parser->input + parser->position, 7) == 0) {
				parser->token = BXCE_TOKEN_CDATA;
				parser->position = parser->position + 7;
			} else {
				parser->token = BXCE_TOKEN_UNEXPECTED;
			}
			break;
		default:
			parser->token = BXCE_TOKEN_START_TAG;
			break;
		}
		goto done;
	}

	if (parser->pending[0] == '!') {
		const char *input;
		int total;

		if (position >= size)
			return 0;
		input = parser->input;
		parser->tokenStart = input + position;
		total = parser->size - position + pending - 1;
		if (total > 1) {
			if (pending < 3) {
				if (pending == 2) {
					if (parser->pending[1] == '-' && input[position] == '-') {
						parser->token = BXCE_TOKEN_COMMENT;
						parser->position = position + 1;
						goto done;
					}
				} else if (bxceMemcmp(input + position, "--", 2) == 0) {
					parser->token = BXCE_TOKEN_COMMENT;
					parser->position = parser->position + 2;
					goto done;
				}
			} else if (bxceMemcmp("--", parser->pending, 2) == 0) {
				/* Sony's code compares from the '!' */
				parser->token = BXCE_TOKEN_COMMENT;
				goto done;
			}
		}

		pending = parser->pendingLength;
		if (total <= 6) {
			bxceMemcpy(parser->pending + pending, parser->input + parser->position,
				   parser->size - parser->position);
			parser->pendingLength = parser->pendingLength + parser->size - parser->position;
			return 0;
		}
		if (pending < 8) {
			if (pending > 0) {
				if (bxceMemcmp(g_bxceDoctype, parser->pending + 1, pending - 1) == 0 &&
				    bxceMemcmp(g_bxceDoctype + parser->pendingLength - 1, parser->tokenStart,
					       8 - parser->pendingLength) == 0) {
					parser->position = parser->position - parser->pendingLength + 8;
					parser->token = BXCE_TOKEN_DTD;
				} else if (bxceMemcmp(g_bxceCdata, parser->pending + 1,
						      parser->pendingLength - 1) != 0 ||
					   bxceMemcmp(g_bxceCdata + parser->pendingLength - 1,
						      parser->tokenStart, 8 - parser->pendingLength) != 0) {
					parser->token = BXCE_TOKEN_UNEXPECTED;
				} else {
					parser->token = BXCE_TOKEN_CDATA;
					parser->position = parser->position - parser->pendingLength + 8;
				}
				goto done;
			}
		} else {
			if (bxceMemcmp(g_bxceDoctype, parser->pending, 7) == 0) {
				parser->token = BXCE_TOKEN_DTD;
				goto done;
			}
			if (bxceMemcmp(g_bxceCdata, parser->pending, 7) == 0) {
				parser->token = BXCE_TOKEN_CDATA;
				goto done;
			}
		}

		if (bxceMemcmp(parser->tokenStart, g_bxceDoctype, 7) == 0) {
			parser->token = BXCE_TOKEN_DTD;
			parser->position = parser->position + 7;
		} else if (bxceMemcmp(parser->tokenStart, g_bxceCdata, 7) == 0) {
			/* Sony's code makes a CDATA section here a DTD token */
			parser->token = BXCE_TOKEN_DTD;
			parser->position = parser->position + 7;
		} else {
			parser->token = BXCE_TOKEN_UNEXPECTED;
		}
	}

done:
	parser->pendingLength = 0;
	return 1;
}

/*
 * The next token in content: markup after '<', a reference after '&' (when references are
 * resolved) or character data; white space between the top-level items is skipped.
 */
int bxceParserReadToken(BxceParser *parser)
{
	const char *input;
	int position, size, left;
	char c;

	if (parser->skipIgnorableWhiteSpace) {
		int last = parser->lastToken;

		if (last != BXCE_TOKEN_CHAR_DATA && last != BXCE_TOKEN_ENTITY)
			bxceParserSkipSpace(parser);
	}

	position = parser->position;
	size = parser->size;
	if (position >= size)
		return 0;

	input = parser->input;
	if (parser->state != BXCE_STATE_CONTENT)
		parser->state = BXCE_STATE_CONTENT;

	c = input[position];
	if (c == '<') {
		parser->token = BXCE_TOKEN_START_TAG;
		parser->position = position + 1;
		if (position + 1 >= size) {
			parser->pendingLength = 0;
			return 0;
		}
		switch (input[position + 1]) {
		case '/':
			parser->token = BXCE_TOKEN_END_TAG;
			parser->position = position + 2;
			break;
		case '!':
			position += 2;
			parser->position = position;
			if (position >= size) {
				parser->pendingLength = 1;
				parser->pending[0] = '!';
				return 0;
			}
			left = size - position;
			if (left > 1) {
				int differs = bxceMemcmp("--", input + position, 2);

				input = parser->input;
				position = parser->position;
				if (differs == 0) {
					parser->token = BXCE_TOKEN_COMMENT;
					parser->position = position + 2;
					break;
				}
			}
			if (left <= 6) {
				parser->pending[0] = '!';
				bxceMemcpy(parser->pending + 1, input + position, left);
				parser->pendingLength = left + 1;
				return 0;
			}
			if (bxceMemcmp(g_bxceDoctype, input + position, 7) == 0) {
				parser->token = BXCE_TOKEN_DTD;
				parser->position = parser->position + 7;
			} else if (bxceMemcmp(g_bxceCdata, parser->input + parser->position, 7) == 0) {
				parser->token = BXCE_TOKEN_CDATA;
				parser->position = parser->position + 7;
			} else {
				parser->token = BXCE_TOKEN_UNEXPECTED;
			}
			break;
		case '?':
			parser->token = BXCE_TOKEN_PI;
			parser->position = position + 2;
			break;
		default:
			parser->token = BXCE_TOKEN_START_TAG;
			break;
		}
		return 1;
	}

	if (c == '&' && parser->resolveEntity) {
		parser->token = BXCE_TOKEN_ENTITY;
		parser->position = position + 1;
		return 1;
	}

	parser->token = BXCE_TOKEN_CHAR_DATA;
	if (bxceNameStackIsEmpty(parser->nameStack) && bxceParserSkipSpace(parser))
		return bxceParserReadToken(parser);
	return 1;
}

/* an empty function nothing calls */
void bxceParserNop(void)
{
}

/* the tokens content may hold */
static inline int bxceIsContentToken(int token)
{
	switch (token) {
	case BXCE_TOKEN_START_TAG:
	case BXCE_TOKEN_CHAR_DATA:
	case BXCE_TOKEN_PI:
	case BXCE_TOKEN_CDATA:
	case BXCE_TOKEN_COMMENT:
	case BXCE_TOKEN_ENTITY:
		return 1;
	default:
		return token == BXCE_TOKEN_END_TAG;
	}
}

/* whether the token may follow the last one: the prolog before the document element, content in it */
int bxceParserTokenAllowed(BxceParser *parser)
{
	int last = parser->lastToken;
	int token;

	if (parser->documentElementSeen) {
		if (last == BXCE_TOKEN_START_TAG)
			return bxceIsContentToken(parser->token);
		if (last == BXCE_TOKEN_END_TAG) {
			if (bxceNameStackIsEmpty(parser->nameStack)) {
				if (parser->state != BXCE_STATE_CONTENT)
					return 1;
				token = parser->token;
				return token == BXCE_TOKEN_PI || token == BXCE_TOKEN_COMMENT;
			}
			return bxceIsContentToken(parser->token);
		}
		if (bxceIsContentToken(last))
			return bxceIsContentToken(parser->token);
		return 0;
	}

	switch (last) {
	case 0:
		switch (parser->token) {
		case BXCE_TOKEN_START_TAG:
		case BXCE_TOKEN_DTD:
		case BXCE_TOKEN_PI:
		case BXCE_TOKEN_COMMENT:
			return 1;
		default:
			return 0;
		}
	case BXCE_TOKEN_DTD:
		token = parser->token;
		return token == BXCE_TOKEN_COMMENT || token == BXCE_TOKEN_PI ||
		       token == BXCE_TOKEN_START_TAG;
	case BXCE_TOKEN_PI:
	case BXCE_TOKEN_COMMENT:
		switch (parser->token) {
		case BXCE_TOKEN_START_TAG:
		case BXCE_TOKEN_DTD:
		case BXCE_TOKEN_PI:
		case BXCE_TOKEN_COMMENT:
			return 1;
		default:
			return 0;
		}
	default:
		return 0;
	}
}

/* a DOCTYPE's text up to its '>', an internal subset's "[...]" passed over: 1 when it ended, 0 at the chunk's end */
int bxceParserScanDoctype(BxceParser *parser, const char **text, int *length, char *terminator)
{
	int start = parser->position;

	*text = parser->input + start;
	while (parser->position < parser->size) {
		if (parser->input[parser->position] == '[')
			*terminator = ']';
		else if (parser->input[parser->position] == ']')
			*terminator = '>';
		if (parser->input[parser->position] == *terminator) {
			parser->position++;
			*length = parser->position - start;
			return 1;
		}
		parser->position++;
	}

	*length = parser->position - start;
	return 0;
}

/*
 * Text up to and with a terminator ("-->", "?>", "]]>"), matched across chunks: matched counts
 * its bytes seen, the terminator window keeps the bytes that may start it. 1 when it ended.
 */
int bxceParserScanUntil(BxceParser *parser, const char **text, int *length, const char *terminator,
			int *matched, int terminatorLength)
{
	int position = parser->position;
	int end = parser->size;
	int start = position;

	*text = parser->input + position;
	if (position < end) {
		do {
			const char *input = parser->input;
			char c = input[position];

			if (c == terminator[*matched]) {
				(*matched)++;
				if (*matched == terminatorLength) {
					position = parser->position + 1;
					parser->position = position;
					*length = position - start;
					parser->terminatorWindow.length = 0;
					return 1;
				}
				parser->terminatorWindow.data[parser->terminatorWindow.length] =
					input[parser->position];
				parser->terminatorWindow.length++;
			} else {
				int windowLength;

				parser->terminatorWindow.data[parser->terminatorWindow.length] = c;
				windowLength = ++parser->terminatorWindow.length;
				if (*matched <= windowLength) {
					char *tail = parser->terminatorWindow.data + windowLength - *matched;

					if (bxceMemcmp(tail, terminator, *matched) == 0) {
						/* Sony's code moves the whole window's length */
						if (*matched != 0)
							bxceMemmove(parser->terminatorWindow.data, tail,
								    parser->terminatorWindow.length);
						parser->terminatorWindow.length = *matched;
					} else {
						*matched = 0;
					}
				}
			}
			position = parser->position + 1;
			parser->position = position;
		} while (position < parser->size);
	}

	*length = position - start;
	return 0;
}

/* an entity reference's name up to ';' into the buffer: 1 when it ended or out of memory, 0 at the chunk's end */
int bxceParserReadReference(BxceParser *parser, BxceBuffer *name)
{
	int position = parser->position;

	if (position >= parser->size)
		return 0;

	for (;;) {
		const char *input = parser->input;
		int length;

		if (input[position] == ';') {
			parser->position = position + 1;
			name->data[name->length] = 0;
			return 1;
		}
		length = name->length;
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

/* moves the tokenizer's state into a SAX parser's slot (its buffers, stack and lists go with it) */
int bxceParserSaveState(BxceParser *parser, int slot)
{
	BxceParserState *state = parser->saved[slot];

	state->skipIgnorableWhiteSpace = parser->skipIgnorableWhiteSpace;
	state->resolveEntity = parser->resolveEntity;
	state->resolveAttrEntity = parser->resolveAttrEntity;
	state->state = parser->state;
	state->token = parser->token;
	state->lastToken = parser->lastToken;
	state->pendingLength = parser->pendingLength;
	state->terminator = parser->terminator;
	state->terminatorMatched = parser->terminatorMatched;
	state->input = parser->input;
	state->position = parser->position;
	state->size = parser->size;
	state->isFinal = parser->isFinal;
	state->elementName = parser->elementName;
	state->characters = parser->characters;
	state->terminatorWindow = parser->terminatorWindow;
	state->emptyElement = parser->emptyElement;
	parser->elementName.data = NULL;
	parser->elementName.length = 0;
	parser->elementName.capacity = 0;
	parser->characters.data = NULL;
	parser->characters.length = 0;
	parser->characters.capacity = 0;
	parser->terminatorWindow.data = NULL;
	parser->terminatorWindow.length = 0;
	parser->terminatorWindow.capacity = 0;

	parser->saved[slot]->nameStack = parser->nameStack;
	parser->nameStack = NULL;
	parser->saved[slot]->attrList = parser->attrList;
	parser->attrList = NULL;
	parser->saved[slot]->attr = parser->attr;
	parser->attr = NULL;

	state = parser->saved[slot];
	state->status = parser->status;
	state->text = parser->text;
	state->textLength = parser->textLength;
	state->skippedText = parser->skippedText;
	state->skippedTextLength = parser->skippedTextLength;
	state->tokenStart = parser->tokenStart;
	state->documentElementSeen = parser->documentElementSeen;
	state->quote = parser->quote;
	return 0;
}

/* gives the tokenizer the state in the slot back (-1 when the slot is empty); the slot keeps its copy */
int bxceParserRestoreState(BxceParser *parser, SceUInt8 slot)
{
	BxceParserState *state;

	bxceParserReleaseState(parser);
	state = parser->saved[slot];
	if (state == NULL)
		return -1;

	parser->skipIgnorableWhiteSpace = state->skipIgnorableWhiteSpace;
	parser->resolveEntity = state->resolveEntity;
	parser->state = state->state;
	parser->token = state->token;
	parser->resolveAttrEntity = state->resolveAttrEntity;
	parser->lastToken = state->lastToken;
	parser->pendingLength = state->pendingLength;
	parser->terminator = state->terminator;
	parser->terminatorMatched = state->terminatorMatched;
	parser->input = state->input;
	parser->position = state->position;
	parser->size = state->size;
	parser->isFinal = state->isFinal;
	if (parser->terminatorWindow.data != NULL) {
		ScebXCe_EC560647(parser->terminatorWindow.data);
		parser->terminatorWindow.data = NULL;
		parser->terminatorWindow.length = 0;
		parser->terminatorWindow.capacity = 0;
	}

	parser->elementName = parser->saved[slot]->elementName;
	parser->characters = parser->saved[slot]->characters;
	parser->terminatorWindow = parser->saved[slot]->terminatorWindow;

	state = parser->saved[slot];
	parser->nameStack = state->nameStack;
	parser->attrList = state->attrList;
	parser->attr = state->attr;
	parser->emptyElement = state->emptyElement;
	parser->status = state->status;
	parser->text = state->text;
	parser->textLength = state->textLength;
	parser->skippedText = state->skippedText;
	parser->skippedTextLength = state->skippedTextLength;
	parser->tokenStart = state->tokenStart;
	parser->documentElementSeen = state->documentElementSeen;
	parser->quote = state->quote;
	return 0;
}

/* the tokenizer made ready for a new document: its buffers, an attribute list and a name stack */
int bxceParserReset(BxceParser *parser)
{
	bxceParserReleaseState(parser);
	parser->documentElementSeen = 0;

	if (parser->terminatorWindow.data == NULL) {
		parser->terminatorWindow.data = (char *)ScebXCe_B8DDE741(BXCE_BUFFER_INITIAL_SIZE);
		if (parser->terminatorWindow.data == NULL)
			return SCE_BXCE_ERROR_NO_MEMORY;
		parser->terminatorWindow.capacity = BXCE_BUFFER_INITIAL_SIZE;
	}
	parser->terminatorWindow.length = 0;

	if (parser->characters.data == NULL) {
		parser->characters.data = (char *)ScebXCe_B8DDE741(BXCE_BUFFER_INITIAL_SIZE);
		if (parser->characters.data == NULL)
			return SCE_BXCE_ERROR_NO_MEMORY;
		parser->characters.capacity = BXCE_BUFFER_INITIAL_SIZE;
	}
	parser->characters.length = 0;

	if (parser->elementName.data == NULL) {
		parser->elementName.data = (char *)ScebXCe_B8DDE741(BXCE_BUFFER_INITIAL_SIZE);
		if (parser->elementName.data == NULL)
			return SCE_BXCE_ERROR_NO_MEMORY;
		parser->elementName.capacity = BXCE_BUFFER_INITIAL_SIZE;
	}
	parser->elementName.length = 0;

	parser->terminator = '>';
	parser->terminatorMatched = 0;
	parser->tokenStart = NULL;
	parser->status = 0;
	parser->state = 0;
	parser->token = 0;
	parser->lastToken = 0;

	parser->attrList = (BxceAttributeList *)ScebXCe_B8DDE741(sizeof(BxceAttributeList));
	if (parser->attrList == NULL || ScebXCe_E7C8B20D(parser->attrList) != 0)
		return SCE_BXCE_ERROR_NO_MEMORY;
	parser->attr = NULL;
	parser->emptyElement = 0;
	parser->pendingLength = 0;

	parser->nameStack = (BxceNameStack *)ScebXCe_B8DDE741(sizeof(BxceNameStack));
	if (parser->nameStack == NULL)
		return SCE_BXCE_ERROR_NO_MEMORY;
	bxceNameStackInitialize(parser->nameStack);

	parser->consumed = 0;
	return 0;
}

int bxceParserInitialize(BxceParser *parser)
{
	int i;

	parser->resolveAttrEntity = 0;
	parser->resolveEntity = 0;
	parser->nameStack = NULL;
	parser->attrList = NULL;
	parser->skipIgnorableWhiteSpace = 1;
	parser->elementName.data = NULL;
	parser->elementName.length = 0;
	parser->elementName.capacity = 0;
	parser->characters.data = NULL;
	parser->characters.capacity = 0;
	parser->terminatorWindow.data = NULL;
	parser->terminatorWindow.length = 0;
	parser->terminatorWindow.capacity = 0;
	parser->characters.length = 0;
	parser->attr = NULL;
	for (i = 0; i < BXCE_PARSER_SAVED_MAX; i++)
		parser->saved[i] = NULL;

	return bxceParserReset(parser);
}

int bxceParserTerminate(BxceParser *parser)
{
	int i;

	bxceParserReleaseState(parser);
	if (parser->terminatorWindow.data != NULL) {
		ScebXCe_EC560647(parser->terminatorWindow.data);
		parser->terminatorWindow.data = NULL;
		parser->terminatorWindow.length = 0;
		parser->terminatorWindow.capacity = 0;
	}

	for (i = 0; i < BXCE_PARSER_SAVED_MAX; i++) {
		if (parser->saved[i] != NULL) {
			bxceParserStateRelease(parser->saved[i]);
			if (parser->saved[i] != NULL)
				ScebXCe_EC560647(parser->saved[i]);
			parser->saved[i] = NULL;
		}
	}

	parser->reserved = 0;
	return 0;
}

void ScebXCe_A40944DD(BxceParser *parser, SceUInt8 skip)
{
	parser->skipIgnorableWhiteSpace = skip;
}

void ScebXCe_FF7CB737(BxceParser *parser, SceUInt8 resolve)
{
	parser->resolveEntity = resolve;
}

void bxceParserSetResolveAttrEntity(BxceParser *parser, SceUInt8 resolve)
{
	parser->resolveAttrEntity = resolve;
}

/* grows a buffer by extra bytes (by its capacity when 0), its bytes copied over */
__attribute__((noinline)) int bxceBufferGrow(char **data, int *capacity, int extra)
{
	char *grown;
	int size;

	if (extra == 0)
		extra = *capacity;
	size = *capacity + extra;
	grown = (char *)ScebXCe_B8DDE741(size);
	if (grown == NULL)
		return SCE_BXCE_ERROR_NO_MEMORY;

	bxceMemcpy(grown, *data, *capacity);
	if (*data != NULL)
		ScebXCe_EC560647(*data);
	*data = grown;
	*capacity = size;
	return 0;
}

/* frees what a saved state holds */
int bxceParserStateRelease(BxceParserState *state)
{
	if (state->nameStack != NULL) {
		bxceNameStackTerminate(state->nameStack);
		if (state->nameStack != NULL)
			ScebXCe_EC560647(state->nameStack);
		state->nameStack = NULL;
	}
	if (state->attrList != NULL) {
		ScebXCe_9C2F9897(state->attrList);
		if (state->attrList != NULL)
			ScebXCe_EC560647(state->attrList);
		state->attrList = NULL;
	}
	if (state->attr != NULL) {
		bxceAttrTerminate(state->attr);
		if (state->attr != NULL)
			ScebXCe_EC560647(state->attr);
		state->attr = NULL;
	}
	if (state->characters.data != NULL) {
		ScebXCe_EC560647(state->characters.data);
		state->characters.data = NULL;
	}
	if (state->elementName.data != NULL) {
		ScebXCe_EC560647(state->elementName.data);
		state->elementName.data = NULL;
	}
	if (state->terminatorWindow.data != NULL) {
		ScebXCe_EC560647(state->terminatorWindow.data);
		state->terminatorWindow.data = NULL;
	}
	return 0;
}
