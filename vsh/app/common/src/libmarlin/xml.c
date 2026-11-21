/*
 * The library's XML parser: a document read from a byte buffer into a tree of nodes, and
 * the node and list frees its callers use. The parser reads through a cursor, a buffer
 * whose size and data advance as bytes are taken; a step that fails puts the cursor back.
 * The file's name is ours.
 */
#include "mln_internal.h"

/* the classes of the parser's character table */
#define XML_NAME	0x01	/* a name character */
#define XML_NCNAME	0x02	/* a name character other than ':' */
#define XML_LETTER	0x04
#define XML_DIGIT	0x08
#define XML_NAME_START	0x10
#define XML_HEX_LETTER	0x20
#define XML_SPACE	0x80

#define XML_UPPER_HEX	(XML_NAME | XML_NCNAME | XML_LETTER | XML_NAME_START | XML_HEX_LETTER)
#define XML_UPPER	(XML_NAME | XML_NCNAME | XML_LETTER | XML_NAME_START)

static const unsigned char s_xmlCharClass[256] = {
	['\t'] = XML_SPACE, ['\n'] = XML_SPACE, ['\r'] = XML_SPACE, [' '] = XML_SPACE,
	['-'] = XML_NAME | XML_NCNAME, ['.'] = XML_NAME | XML_NCNAME,
	['0' ... '9'] = XML_NAME | XML_NCNAME | XML_DIGIT,
	[':'] = XML_NAME | XML_NAME_START,
	['A' ... 'F'] = XML_UPPER_HEX, ['G' ... 'Z'] = XML_UPPER,
	['_'] = XML_NAME | XML_NCNAME | XML_NAME_START,
	['a' ... 'f'] = XML_UPPER_HEX, ['g' ... 'z'] = XML_UPPER,
};

#define XML_IS(c, classes)	(s_xmlCharClass[(unsigned char)(c)] & (classes))

/* the cursor's size back to what it was at a step's start, its data moved back with it */
static inline __attribute__((always_inline)) void MlnXmlRewind(MlnBuffer *cursor,
							       unsigned int size)
{
	cursor->data += cursor->size - size;
	cursor->size = size;
}

int MlnXmlNodeFree(int heap, MlnXmlNode *node)
{
	return MlnXmlNodeRelease(heap, &node);
}

int MlnXmlListFree(int heap, MlnXmlNodeList *list)
{
	return MlnXmlNodeListRelease(heap, &list);
}

/* "<!-- ... -->" */
__attribute__((noinline)) int MlnXmlSkipComment(MlnBuffer *cursor)
{
	unsigned int size = cursor->size;
	unsigned char *data = cursor->data;
	unsigned int remaining;
	unsigned char *p;
	unsigned char c;

	if (size > 3 && data[0] == '<' && data[1] == '!' && data[2] == '-' && data[3] == '-') {
		remaining = size - 4;
		p = data + 4;
		cursor->size = remaining;
		cursor->data = p;
		while (remaining != 0) {
			c = *p++;
			remaining--;
			cursor->size = remaining;
			cursor->data = p;
			if (c != '-')
				continue;
			if (remaining == 0)
				break;
			if (*p != '-')
				continue;
			if (remaining == 1 || p[1] != '>')
				break;
			cursor->size = remaining - 2;
			cursor->data = p + 2;
			return MLN_OK;
		}
	}
	MlnXmlRewind(cursor, size);
	return MLN_XML_ERROR_SYNTAX;
}

/*
 * The characters up to the delimiter ('<' for text, the quote for an attribute's value,
 * ']' for the "]]>" that ends a CDATA section) as a new buffer, with the five entities and
 * character references replaced; a line end is '\n' in text. In an attribute's value each
 * run of white space is one space, without spaces at the start or the end.
 */
__attribute__((noinline)) int MlnXmlReadText(int heap, MlnBuffer **text, MlnBuffer *cursor, int delimiter)
{
	unsigned int size = cursor->size;
	MlnBuffer *buffer = NULL;
	unsigned int length, end, value;
	unsigned char *data, *name, *out, *start;
	unsigned char c;
	int attribute, spaces, ret;

	if (size == 0) {
		ret = MLN_XML_ERROR_SYNTAX;
		goto fail;
	}
	data = cursor->data;
	for (length = 0; length < size; length++) {
		c = data[length];
		if ((c == '<' && delimiter == '<') || (c == '\'' && delimiter == '\'') ||
		    (c == '"' && delimiter == '"'))
			break;
		if (c == ']' && delimiter == ']' && size > length + 2 &&
		    data[length + 1] == ']' && data[length + 2] == '>')
			break;
	}

	buffer = MlnBufferCreate(heap, length, NULL);
	if (buffer == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto fail;
	}
	start = out = buffer->data;
	attribute = delimiter == '\'' || delimiter == '"';
	spaces = 1;
	end = size - length;

	while (cursor->size > end) {
		data = cursor->data;
		c = *data;
		cursor->size--;
		cursor->data++;
		if (c == '\r') {
			if (cursor->size > end && data[1] == '\n') {
				cursor->data = data + 2;
				cursor->size--;
			}
			if (!attribute) {
				c = '\n';
				goto emit;
			}
			goto space;
		}
		if (attribute) {
			if (c == '\n' || c == '\t' || c == ' ')
				goto space;
			spaces = 0;
		}
		if (c != '&')
			goto emit;

		if (cursor->size <= end) {
			ret = MLN_XML_ERROR_SYNTAX;
			goto fail;
		}
		name = cursor->data;
		if (*name == '#') {
			cursor->data++;
			cursor->size--;
			if (cursor->size == 0) {
				ret = MLN_XML_ERROR_SYNTAX;
				goto fail;
			}
			value = 0;
			if (*cursor->data != 'x') {
				for (;;) {
					c = *cursor->data;
					cursor->size--;
					cursor->data++;
					if (c == ';')
						break;
					value = value * 10 + c - '0';
					if (!XML_IS(c, XML_DIGIT) || cursor->size == 0) {
						ret = MLN_XML_ERROR_SYNTAX;
						goto fail;
					}
				}
			} else {
				cursor->data++;
				cursor->size--;
				for (;;) {
					if (cursor->size == 0) {
						ret = MLN_XML_ERROR_SYNTAX;
						goto fail;
					}
					c = *cursor->data;
					cursor->size--;
					cursor->data++;
					if (c == ';')
						break;
					if (XML_IS(c, XML_DIGIT))
						value = value * 16 + c - '0';
					else if (XML_IS(c, XML_HEX_LETTER))
						value = value * 16 + ((c - 55) & 0xf);
					else {
						ret = MLN_XML_ERROR_SYNTAX;
						goto fail;
					}
				}
			}
			if (value > 0x7e) {
				ret = MLN_XML_ERROR_SYNTAX;
				goto fail;
			}
			*out++ = value;
			continue;
		}

		if (!XML_IS(*name, XML_NAME | XML_NAME_START)) {
			ret = MLN_XML_ERROR_SYNTAX;
			goto fail;
		}
		do {
			cursor->data++;
			cursor->size--;
		} while (cursor->size != end && XML_IS(*cursor->data, XML_NAME | XML_NAME_START));
		if (*cursor->data != ';') {
			ret = MLN_XML_ERROR_SYNTAX;
			goto fail;
		}
		switch (cursor->data - name) {
		case 3:
			if (MlnMemcmp(name, "amp", 3) != 0)
				goto bad_entity;
			*out++ = '&';
			break;
		case 4:
			if (MlnMemcmp(name, "quot", 4) == 0)
				*out++ = '"';
			else if (MlnMemcmp(name, "apos", 4) == 0)
				*out++ = '\'';
			else
				goto bad_entity;
			break;
		case 2:
			if (MlnMemcmp(name, "lt", 2) == 0)
				*out++ = '<';
			else if (MlnMemcmp(name, "gt", 2) == 0)
				*out++ = '>';
			else
				goto bad_entity;
			break;
		default:
			goto bad_entity;
		}
		cursor->data++;
		cursor->size--;
		continue;

space:
		spaces++;
		c = ' ';
emit:
		if (spaces <= 1)
			*out++ = c;
	}

	if (attribute && out > start && out[-1] == ' ') {
		do
			out--;
		while (out != start && out[-1] == ' ');
	}
	ret = MlnXmlBufferTruncate(heap, &buffer, out - start);
	if (ret != MLN_OK)
		goto fail;
	*text = buffer;
	buffer = NULL;
	goto out;

bad_entity:
	ret = MLN_XML_ERROR_SYNTAX;
fail:
	MlnXmlRewind(cursor, size);
out:
	MlnBufferDestroy(heap, buffer);
	return ret;
}

static inline __attribute__((always_inline)) void MlnXmlConsume(MlnBuffer *cursor,
								unsigned int count)
{
	cursor->size -= count;
	cursor->data += count;
}

/* white space that may be absent; reaching the input's end is an error */
static inline __attribute__((always_inline)) int MlnXmlSkipSpaces(MlnBuffer *cursor)
{
	if (cursor->size == 0)
		return MLN_XML_ERROR_SYNTAX;
	while (XML_IS(*cursor->data, XML_SPACE)) {
		MlnXmlConsume(cursor, 1);
		if (cursor->size == 0)
			return MLN_XML_ERROR_SYNTAX;
	}
	return MLN_OK;
}

/* S? '=' S? and the opening quote, which it returns (0 when there is none) */
static inline __attribute__((always_inline)) int MlnXmlReadEq(MlnBuffer *cursor)
{
	int quote;

	if (MlnXmlSkipSpaces(cursor) != MLN_OK || *cursor->data != '=')
		return 0;
	MlnXmlConsume(cursor, 1);
	if (MlnXmlSkipSpaces(cursor) != MLN_OK)
		return 0;
	quote = *cursor->data;
	if (quote != '"' && quote != '\'')
		return 0;
	MlnXmlConsume(cursor, 1);
	return quote;
}

/*
 * '<?xml' version [encoding] [standalone] S? '?>': the version and the encoding are kept,
 * the standalone declaration is read and dropped (one that does not parse is skipped).
 */
static inline __attribute__((always_inline)) int MlnXmlParseDeclaration(int heap,
	MlnBuffer *cursor, MlnBuffer **versionOut, MlnBuffer **encodingOut)
{
	MlnBuffer *version = NULL, *encoding = NULL, *standalone;
	unsigned int mark;
	unsigned char *value;
	int quote, ret;

	if (cursor->size <= 4 || MlnMemcmp(cursor->data, "<?xml", 5) != 0)
		goto syntax;
	MlnXmlConsume(cursor, 5);
	if (MlnXmlSkipSpaces(cursor) != MLN_OK)
		goto syntax;

	if (cursor->size <= 6 || MlnMemcmp(cursor->data, "version", 7) != 0)
		goto syntax;
	MlnXmlConsume(cursor, 7);
	quote = MlnXmlReadEq(cursor);
	if (quote == 0)
		goto syntax;
	value = cursor->data;
	while (cursor->size != 0 && XML_IS(*cursor->data, XML_NAME))
		MlnXmlConsume(cursor, 1);
	if (*cursor->data != quote)
		goto syntax;
	version = MlnBufferCreate(heap, cursor->data - value, value);
	if (version == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto fail;
	}
	MlnXmlConsume(cursor, 1);

	if (cursor->size == 0)
		goto syntax;
	while (XML_IS(*cursor->data, XML_SPACE)) {
		MlnXmlConsume(cursor, 1);
		if (cursor->size == 0)
			goto syntax;
	}
	if (cursor->size > 7 && MlnMemcmp(cursor->data, "encoding", 8) == 0) {
		MlnXmlConsume(cursor, 8);
		quote = MlnXmlReadEq(cursor);
		if (quote == 0 || cursor->size == 0 || !XML_IS(*cursor->data, XML_LETTER))
			goto syntax;
		value = cursor->data;
		MlnXmlConsume(cursor, 1);
		while (cursor->size != 0 && XML_IS(*cursor->data, XML_NCNAME))
			MlnXmlConsume(cursor, 1);
		if (*cursor->data != quote)
			goto syntax;
		encoding = MlnBufferCreate(heap, cursor->data - value, value);
		if (encoding == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto fail;
		}
		MlnXmlConsume(cursor, 1);
	}

	if (cursor->size == 0)
		goto syntax;
	mark = cursor->size;
	if (MlnXmlSkipSpaces(cursor) != MLN_OK ||
	    cursor->size <= 9 || MlnMemcmp(cursor->data, "standalone", 10) != 0)
		goto skip_standalone;
	MlnXmlConsume(cursor, 10);
	quote = MlnXmlReadEq(cursor);
	if (quote == 0)
		goto skip_standalone;
	standalone = NULL;
	ret = MlnXmlReadText(heap, &standalone, cursor, quote);
	if (ret != MLN_OK) {
		MlnXmlRewind(cursor, mark);
		if (ret != MLN_XML_ERROR_SYNTAX)
			goto fail;
		goto close;
	}
	MlnXmlConsume(cursor, 1);
	MlnBufferDestroy(heap, standalone);
	goto check_end;

skip_standalone:
	MlnXmlRewind(cursor, mark);
check_end:
	if (cursor->size == 0)
		goto syntax;
close:
	if (MlnXmlSkipSpaces(cursor) != MLN_OK || cursor->size <= 1 ||
	    cursor->data[0] != '?' || cursor->data[1] != '>')
		goto syntax;
	MlnXmlConsume(cursor, 2);
	*versionOut = version;
	*encodingOut = encoding;
	return MLN_OK;

syntax:
	ret = MLN_XML_ERROR_SYNTAX;
fail:
	if (encoding != NULL)
		MlnBufferDestroy(heap, encoding);
	if (version != NULL)
		MlnBufferDestroy(heap, version);
	return ret;
}

/* The prolog: the XML declaration if there is one, then white space and comments. */
int MlnXmlParseProlog(int heap, MlnXmlParser *parser, MlnXmlProlog **out, MlnBuffer *cursor)
{
	unsigned int size = cursor->size;
	MlnXmlProlog *prolog = NULL;
	unsigned int start;
	int ret;

	(void)parser;
	ret = MlnXmlPrologCreate(heap, &prolog);
	if (ret != MLN_OK)
		goto fail;

	start = cursor->size;
	ret = MlnXmlParseDeclaration(heap, cursor, &prolog->version, &prolog->encoding);
	if (ret != MLN_OK) {
		MlnXmlRewind(cursor, start);
		if (ret != MLN_XML_ERROR_SYNTAX)
			goto fail;
	}

	for (;;) {
		while (cursor->size != 0 && XML_IS(*cursor->data, XML_SPACE))
			MlnXmlConsume(cursor, 1);
		if (cursor->size <= 1 || cursor->data[0] != '<' || cursor->data[1] != '!')
			break;
		ret = MlnXmlSkipComment(cursor);
		if (ret == MLN_OK)
			continue;
		if (ret != MLN_XML_ERROR_SYNTAX)
			goto fail;
		break;
	}

	if (out != NULL) {
		*out = prolog;
		prolog = NULL;
	}
	ret = MLN_OK;
	goto out;

fail:
	MlnXmlRewind(cursor, size);
out:
	MlnXmlPrologRelease(heap, &prolog);
	return ret;
}
