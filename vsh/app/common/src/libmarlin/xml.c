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

/* optional white space */
static inline __attribute__((always_inline)) void MlnXmlSkipOptionalSpaces(MlnBuffer *cursor)
{
	while (cursor->size != 0 && XML_IS(*cursor->data, XML_SPACE))
		MlnXmlConsume(cursor, 1);
}

/*
 * A qualified name's namespace: the value of the declaration in scope whose prefix is the
 * name's text before its ':', and the rest of the name from the ':' on. A name without a
 * ':' (or with an undeclared prefix) has the empty namespace.
 */
static inline __attribute__((always_inline)) void MlnXmlResolveName(
	const MlnXmlAttributeArray *namespaces, const unsigned char **name, unsigned int *size,
	const unsigned char **uri, unsigned int *uriSize)
{
	const MlnXmlAttribute *declaration;
	unsigned int prefix;
	int i;

	*uri = (const unsigned char *)"";
	*uriSize = 0;
	for (prefix = 0; prefix != *size; prefix++) {
		if ((*name)[prefix] == ':')
			break;
	}
	if (prefix == *size)
		return;
	for (i = 0; i < namespaces->count; i++) {
		declaration = namespaces->items[i];
		if (declaration->name->size == prefix &&
		    MlnMemcmp(*name, declaration->name->data, prefix) == 0) {
			*uri = declaration->value->data;
			*uriSize = declaration->value->size;
			break;
		}
	}
	if (prefix == *size) {
		*uri = (const unsigned char *)"";
		*uriSize = 0;
	} else {
		*name += prefix;
		*size -= prefix;
	}
}

/* Orders two attribute names of an element: by namespace, then by name. */
__attribute__((noinline)) int MlnXmlCompareAttributeNames(int heap,
	const MlnXmlAttributeArray *namespaces, const MlnBuffer *elementName,
	const MlnBuffer *a, const MlnBuffer *b)
{
	const unsigned char *nameA = a->data, *nameB = b->data, *uriA, *uriB;
	unsigned int sizeA = a->size, sizeB = b->size, uriSizeA, uriSizeB;
	int cmp;

	(void)heap;
	(void)elementName;
	MlnXmlResolveName(namespaces, &nameA, &sizeA, &uriA, &uriSizeA);
	MlnXmlResolveName(namespaces, &nameB, &sizeB, &uriB, &uriSizeB);
	if (uriSizeA != 0) {
		if (uriSizeB == 0)
			return 1;
		cmp = MlnMemcmp(uriA, uriB, uriSizeB >= uriSizeA ? uriSizeA : uriSizeB);
		if (cmp != 0)
			return cmp;
	} else if (uriSizeB != 0) {
		return -1;
	}
	if (sizeA == 0)
		return sizeB == 0 ? 0 : -1;
	if (sizeB == 0)
		return 1;
	cmp = MlnMemcmp(nameA, nameB, sizeB >= sizeA ? sizeA : sizeB);
	if (cmp == 0 && sizeA != sizeB)
		return sizeA < sizeB ? -1 : 1;
	return cmp;
}

/* a name: a name-start character, then name characters */
static inline __attribute__((always_inline)) int MlnXmlReadName(int heap, MlnBuffer *cursor,
								MlnBuffer **name)
{
	const unsigned char *start;

	if (cursor->size == 0 || !XML_IS(*cursor->data, XML_NAME_START))
		return MLN_XML_ERROR_SYNTAX;
	start = cursor->data;
	MlnXmlConsume(cursor, 1);
	while (cursor->size != 0 && XML_IS(*cursor->data, XML_NAME))
		MlnXmlConsume(cursor, 1);
	*name = MlnBufferCreate(heap, cursor->data - start, start);
	if (*name == NULL)
		return MLN_ERROR_NO_MEMORY;
	return MLN_OK;
}

/*
 * One attribute: Name S? '=' S? quoted value. A syntax error rewinds to the attribute's
 * start. An "xmlns" or "xmlns:prefix" attribute is a namespace declaration: its name
 * becomes the prefix and it goes onto the parser's namespace stack; any other goes onto
 * the element's list.
 */
static inline __attribute__((always_inline)) int MlnXmlParseAttribute(int heap,
	MlnXmlParser *parser, MlnBuffer *cursor, MlnXmlLink **attributes,
	MlnXmlAttribute **pending)
{
	unsigned int mark = cursor->size;
	MlnXmlAttribute *attribute = NULL;
	MlnBuffer *value, *name;
	int quote, ret;

	ret = MlnXmlAttributeCreate(heap, &attribute);
	if (ret != MLN_OK)
		goto fail;
	ret = MlnXmlReadName(heap, cursor, &attribute->name);
	if (ret != MLN_OK)
		goto fail;
	MlnXmlSkipOptionalSpaces(cursor);
	if (cursor->size == 0 || *cursor->data != '=')
		goto syntax;
	MlnXmlConsume(cursor, 1);
	if (cursor->size == 0)
		goto syntax;
	MlnXmlSkipOptionalSpaces(cursor);
	if (cursor->size == 0)
		goto syntax;
	quote = *cursor->data;
	if (quote != '"' && quote != '\'')
		goto syntax;
	MlnXmlConsume(cursor, 1);
	ret = MlnXmlReadText(heap, &value, cursor, quote == '"' ? '"' : '\'');
	if (ret != MLN_OK)
		goto fail;
	MlnXmlConsume(cursor, 1);

	*pending = attribute;
	attribute->value = value;
	name = attribute->name;
	if (name->size == 5) {
		if (MlnMemcmp(name->data, "xmlns", 5) != 0)
			goto attribute;
		MlnXmlBufferTruncate(heap, &name, 0);
	} else {
		if (name->size <= 6 || MlnMemcmp(name->data, "xmlns:", 6) != 0)
			goto attribute;
		MlnXmlBufferSet(heap, &name, name->data + 6, name->size - 6);
	}
	(*pending)->name = name;
	ret = MlnXmlStackPush(heap, &parser->namespaces, *pending);
	if (ret != MLN_OK)
		return ret;
	*pending = NULL;
	return MLN_OK;

attribute:
	ret = MlnXmlLinkPush(heap, attributes, *pending);
	if (ret != MLN_OK)
		return ret;
	*pending = NULL;
	return MLN_OK;

syntax:
	ret = MLN_XML_ERROR_SYNTAX;
fail:
	MlnXmlRewind(cursor, mark);
	MlnXmlAttributeRelease(heap, &attribute);
	return ret;
}

/*
 * An element: '<' Name (S Attribute)* S? ('/>' | '>' content '</' Name S? '>'), the
 * attributes sorted by namespace and name, adjacent text joined. The namespaces it
 * declares are in scope for its content and popped when it ends. Any error rewinds the
 * cursor to the element's start.
 */
__attribute__((noinline)) int MlnXmlParseElement(int heap, MlnXmlParser *parser,
						 MlnXmlElement **out, MlnBuffer *cursor)
{
	int depth = parser->namespaces.depth;
	unsigned int size = cursor->size;
	MlnBuffer *closeName = NULL;
	MlnXmlElement *element = NULL;
	MlnXmlAttribute *pending = NULL;
	MlnXmlContent *content = NULL;
	MlnXmlContentArray *contents = NULL;
	MlnXmlLink *attributes = NULL, *children = NULL;
	MlnXmlAttributeArray *sorted = NULL, *namespaces = NULL;
	MlnXmlContent *last;
	MlnXmlAttribute *best, *candidate, *swap;
	MlnBuffer *elementName, *bestName, *candidateName, *text, *joined;
	int count, i, j, chosen, ret;

	ret = MlnXmlElementCreate(heap, &element);
	if (ret != MLN_OK)
		goto fail;
	if (cursor->size == 0 || *cursor->data != '<')
		goto syntax;
	MlnXmlConsume(cursor, 1);
	ret = MlnXmlReadName(heap, cursor, &element->name);
	if (ret != MLN_OK)
		goto fail;

	MlnXmlSkipOptionalSpaces(cursor);
	attributes = NULL;
	for (;;) {
		ret = MlnXmlParseAttribute(heap, parser, cursor, &attributes, &pending);
		if (ret == MLN_XML_ERROR_SYNTAX)
			break;
		if (ret != MLN_OK)
			goto fail;
		MlnXmlSkipOptionalSpaces(cursor);
	}

	ret = MlnXmlLinksToAttributeArray(heap, attributes, &sorted);
	if (ret != MLN_OK)
		goto fail;
	MlnXmlLinksFree(heap, &attributes, MLN_XML_LINK_ATTRIBUTE);
	attributes = NULL;
	element->attributes = sorted;
	ret = MlnXmlStackGetNamespaces(heap, &parser->namespaces, &namespaces);
	if (ret != MLN_OK)
		goto fail;
	element->namespaces = namespaces;

	/* a selection sort, as Sony's */
	count = sorted->count;
	elementName = element->name;
	for (i = 0; i < count - 1; i++) {
		chosen = i;
		best = sorted->items[i];
		bestName = best->name;
		for (j = i + 1; j < count; j++) {
			candidate = sorted->items[j];
			candidateName = candidate->name;
			if (MlnXmlCompareAttributeNames(heap, namespaces, elementName, bestName,
							candidateName) > 0) {
				chosen = j;
				best = candidate;
				bestName = candidateName;
			}
		}
		if (i != chosen) {
			swap = sorted->items[i];
			sorted->items[i] = best;
			sorted->items[chosen] = swap;
		}
	}

	children = NULL;
	if (cursor->size > 1 && cursor->data[0] == '/') {
		if (cursor->data[1] != '>')
			goto syntax;
		MlnXmlConsume(cursor, 2);
		goto done;
	}
	if (cursor->size == 0 || *cursor->data != '>')
		goto syntax;
	MlnXmlConsume(cursor, 1);

	for (;;) {
		ret = MlnXmlParseContent(heap, parser, &content, cursor);
		if (ret != MLN_OK)
			break;
		if (content == NULL)
			continue;
		if (content->kind != MLN_XML_CONTENT_TEXT || children == NULL ||
		    (last = children->item)->kind != MLN_XML_CONTENT_TEXT) {
			ret = MlnXmlLinkPush(heap, &children, content);
			if (ret != MLN_OK)
				goto fail;
			content = NULL;
			continue;
		}
		text = last->data.text;
		joined = MlnBufferConcat(heap, text, content->data.text);
		if (joined == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto fail;
		}
		last->data.text = joined;
		MlnBufferDestroy(heap, text);
		MlnXmlContentRelease(heap, &content);
	}
	if (ret != MLN_XML_ERROR_SYNTAX)
		goto fail;
	if (cursor->size <= 1 || cursor->data[0] != '<' || cursor->data[1] != '/')
		goto fail;
	MlnXmlConsume(cursor, 2);
	ret = MlnXmlReadName(heap, cursor, &closeName);
	if (ret != MLN_OK)
		goto fail;
	if (MlnBufferCompare(heap, element->name, closeName) != 0) {
		ret = MLN_XML_ERROR_END_TAG;
		goto fail;
	}
	MlnBufferDestroy(heap, closeName);
	closeName = NULL;
	if (cursor->size == 0 || *cursor->data != '>')
		goto syntax;
	MlnXmlConsume(cursor, 1);

done:
	ret = MlnXmlLinksToContentArray(heap, children, &contents);
	if (ret != MLN_OK)
		goto fail;
	element->content = contents;
	MlnXmlLinksFree(heap, &children, MLN_XML_LINK_CONTENT);
	children = NULL;
	*out = element;
	goto out;

syntax:
	ret = MLN_XML_ERROR_SYNTAX;
fail:
	MlnXmlRewind(cursor, size);
out:
	while (depth < parser->namespaces.depth)
		MlnXmlStackPop(heap, &parser->namespaces);
	if (ret != MLN_OK) {
		MlnXmlLinksFree(heap, &attributes, MLN_XML_LINK_ATTRIBUTE);
		MlnXmlLinksFree(heap, &children, MLN_XML_LINK_CONTENT);
		MlnXmlContentRelease(heap, &content);
		MlnXmlAttributeRelease(heap, &pending);
		MlnXmlElementRelease(heap, &element);
		MlnBufferDestroy(heap, closeName);
	}
	return ret;
}

/*
 * One item of an element's content: a CDATA section or character data (text), a comment
 * (no item), or an element. "</" ends the content: a syntax error the element parser
 * reads as its end tag.
 */
__attribute__((noinline)) int MlnXmlParseContent(int heap, MlnXmlParser *parser,
						 MlnXmlContent **out, MlnBuffer *cursor)
{
	unsigned int size = cursor->size;
	MlnXmlContent *content = NULL;
	MlnXmlElement *element = NULL;
	MlnBuffer *text = NULL;
	unsigned int mark;
	unsigned char *start;
	int ret;

	ret = MlnXmlContentCreate(heap, &content);
	if (ret != MLN_OK)
		goto fail;
	if (cursor->size == 0 || *cursor->data != '<') {
		ret = MlnXmlReadText(heap, &text, cursor, '<');
		if (ret != MLN_OK)
			goto fail;
		content->kind = MLN_XML_CONTENT_TEXT;
		content->data.text = text;
		*out = content;
		return MLN_OK;
	}
	if (cursor->size <= 1 || cursor->data[1] == '/') {
		ret = MLN_XML_ERROR_SYNTAX;
		goto fail;
	}
	if (cursor->data[1] != '!') {
		ret = MlnXmlParseElement(heap, parser, &element, cursor);
		if (ret != MLN_OK)
			goto fail;
		content->kind = MLN_XML_CONTENT_ELEMENT;
		content->data.element = element;
		*out = content;
		return MLN_OK;
	}

	/* '<![CDATA[' text ']]>'; one that does not end is read again as a comment */
	mark = cursor->size;
	if (cursor->size > 8 && MlnMemcmp(cursor->data, "<![CDATA[", 9) == 0) {
		MlnXmlConsume(cursor, 9);
		start = cursor->data;
		while (cursor->size != 0) {
			if (cursor->size > 2 && cursor->data[0] == ']' && cursor->data[1] == ']' &&
			    cursor->data[2] == '>') {
				text = MlnBufferCreate(heap, cursor->data - start, start);
				if (text == NULL) {
					ret = MLN_ERROR_NO_MEMORY;
					goto fail;
				}
				MlnXmlConsume(cursor, 3);
				content->kind = MLN_XML_CONTENT_TEXT;
				content->data.text = text;
				*out = content;
				return MLN_OK;
			}
			MlnXmlConsume(cursor, 1);
		}
	}
	MlnXmlRewind(cursor, mark);
	ret = MlnXmlSkipComment(cursor);
	if (ret != MLN_OK)
		goto fail;
	MlnXmlContentRelease(heap, &content);
	*out = content;
	return MLN_OK;

fail:
	MlnXmlRewind(cursor, size);
	MlnXmlContentRelease(heap, &content);
	return ret;
}

/* A document: the prolog and the root element, as a document node. */
int MlnXmlParse(int heap, unsigned int size, const void *xml, MlnXmlNode **document)
{
	MlnXmlNode *node = NULL;
	MlnXmlDocument *doc = NULL;
	MlnXmlProlog *prolog = NULL;
	MlnXmlElement *root;
	MlnXmlParser parser;
	MlnBuffer cursor;
	int ret;

	MlnXmlParserInit(heap, &parser);
	cursor.size = size;
	cursor.data = (unsigned char *)xml;
	ret = MlnXmlNodeCreate(heap, &node);
	if (ret != MLN_OK)
		goto fail;
	ret = MlnXmlDocumentCreate(heap, &doc);
	if (ret != MLN_OK)
		goto fail;
	ret = MlnXmlParseProlog(heap, &parser, &prolog, &cursor);
	if (ret != MLN_OK)
		goto fail;
	doc->prolog = prolog;
	ret = MlnXmlParseElement(heap, &parser, &root, &cursor);
	if (ret != MLN_OK)
		goto fail;
	doc->root = root;
	node->type = MLN_XML_NODE_DOCUMENT;
	node->object.document = doc;
	*document = node;
	goto out;

fail:
	MlnXmlDocumentRelease(heap, &doc);
	MlnXmlNodeRelease(heap, &node);
out:
	MlnXmlParserTerm(heap, &parser);
	return ret;
}

/* the namespaces in scope at an element, pushed onto a parser's stack */
static inline __attribute__((always_inline)) int MlnXmlParserPushNamespaces(int heap,
	MlnXmlParser *parser, const MlnXmlAttributeArray *namespaces)
{
	MlnXmlAttribute *declaration;
	int count = namespaces->count, i, ret;

	for (i = 0; i < count; i++) {
		declaration = namespaces->items[i];
		declaration->refCount++;
		ret = MlnXmlStackPush(heap, &parser->namespaces, declaration);
		if (ret != MLN_OK) {
			MlnXmlAttributeRelease(heap, &declaration);
			return ret;
		}
	}
	return MLN_OK;
}

/*
 * Parses XML content and appends its items to an element node, with the element's
 * namespaces in scope; the content ends at the input's end or at the first item that
 * does not parse. Sony's terminates the parser also when the node is not an element,
 * before initialising it; ours does the same.
 */
int MlnXmlNodeAppendXml(int heap, unsigned int size, const void *xml, MlnXmlNode *node)
{
	MlnXmlContent *content = NULL;
	MlnXmlElement *element;
	MlnXmlParser parser;
	MlnBuffer cursor;
	int ret;

	if (node == NULL || node->type != MLN_XML_NODE_ELEMENT) {
		ret = MLN_XML_ERROR_NOT_ELEMENT;
		goto out;
	}
	element = node->object.element;
	MlnXmlParserInit(heap, &parser);
	cursor.size = size;
	cursor.data = (unsigned char *)xml;
	ret = MlnXmlParserPushNamespaces(heap, &parser, element->namespaces);
	if (ret != MLN_OK)
		goto release;
	if (cursor.size == 0) {
		ret = MLN_OK;
		goto release;
	}
	for (;;) {
		ret = MlnXmlParseContent(heap, &parser, &content, &cursor);
		if (ret != MLN_OK)
			break;
		MlnXmlElementAppendContents(heap, element, 1, &content);
		content = NULL;
		if (cursor.size == 0)
			goto out;
	}
	if (ret == MLN_XML_ERROR_SYNTAX)
		ret = MLN_OK;
release:
	if (content != NULL)
		MlnXmlContentRelease(heap, &content);
out:
	MlnXmlParserTerm(heap, &parser);
	return ret;
}

/*
 * Parses one item of XML (after an optional prolog) and puts it in place of a child
 * element of an element node, with the parent's namespaces in scope. Not an element:
 * as MlnXmlNodeAppendXml.
 */
int MlnXmlNodeReplaceChildXml(int heap, unsigned int size, const void *xml, MlnXmlNode *child,
			      MlnXmlNode *parent)
{
	MlnXmlContent *content = NULL;
	MlnXmlElement *element;
	MlnXmlParser parser;
	MlnBuffer cursor;
	int ret;

	if (parent == NULL || parent->type != MLN_XML_NODE_ELEMENT || child == NULL ||
	    child->type != MLN_XML_NODE_ELEMENT) {
		ret = MLN_XML_ERROR_NOT_ELEMENT;
		goto out;
	}
	element = parent->object.element;
	MlnXmlParserInit(heap, &parser);
	cursor.size = size;
	cursor.data = (unsigned char *)xml;
	ret = MlnXmlParserPushNamespaces(heap, &parser, element->namespaces);
	if (ret != MLN_OK)
		goto release;
	ret = MlnXmlParseProlog(heap, &parser, NULL, &cursor);
	if (ret != MLN_OK)
		goto release;
	ret = MlnXmlParseContent(heap, &parser, &content, &cursor);
	if (ret != MLN_OK)
		goto release;
	MlnXmlElementReplaceChild(heap, element, child->object.element, content);
	content = NULL;
	goto out;

release:
	if (content != NULL)
		MlnXmlContentRelease(heap, &content);
out:
	MlnXmlParserTerm(heap, &parser);
	return ret;
}

/* A decimal integer with an optional '-'; an empty number is 0. */
int MlnXmlParseInt(int heap, unsigned int size, const char *text, int *value)
{
	const char *end;
	int sign, result, digit;

	(void)heap;
	if (size == 0 || text == NULL || value == NULL)
		return MLN_ERROR_INVALID_ARGUMENT;
	end = text + size;
	if (*text == '-') {
		text++;
		sign = -1;
	} else {
		sign = 1;
	}
	if (end <= text) {
		*value = 0;
		return MLN_OK;
	}
	digit = (unsigned char)*text - '0';
	if ((unsigned char)digit > 9)
		return MLN_XML_ERROR_SYNTAX;
	result = 0;
	for (;;) {
		result = digit + result * 10;
		if (text == end - 1)
			break;
		digit = (unsigned char)*++text - '0';
		if ((unsigned char)digit > 9)
			return MLN_XML_ERROR_SYNTAX;
	}
	*value = result * sign;
	return MLN_OK;
}

/*
 * An xsd:dateTime: [-]YYYY-MM-DDThh:mm:ss[.fff] then 'Z' or an offset (+|-)hh:mm. The
 * fraction's first three digits fill the microsecond field as thousandths, as Sony's do;
 * the offset is stored negated, in seconds. Text that ends without a 'Z' or an offset is
 * a syntax error.
 */
int MlnXmlParseDateTime(int heap, unsigned int size, const char *text, MlnDateTime *time)
{
	const unsigned char *p, *end, *mark;
	MlnDateTime parsed;
	unsigned int state = 0;
	int sign, scale = 100, digit, minutes;
	unsigned char c;

	(void)heap;
	if (size == 0 || text == NULL || time == NULL)
		return MLN_ERROR_INVALID_ARGUMENT;
	MlnMemset(&parsed, 0, sizeof(parsed));
	p = (const unsigned char *)text;
	end = p + size;
	if (*p == '-') {
		p++;
		sign = -1;
	} else {
		sign = 1;
	}
	if (p >= end)
		return MLN_XML_ERROR_SYNTAX;
	for (;;) {
		mark = p;
		c = *p++;
		digit = c - '0';
		if ((unsigned int)digit <= 9) {
			switch (state) {
			case 0:
				parsed.year = sign * digit + parsed.year * 10;
				break;
			case 1:
				parsed.month = digit + parsed.month * 10;
				break;
			case 2:
				parsed.day = digit + parsed.day * 10;
				break;
			case 3:
				parsed.hour = digit + parsed.hour * 10;
				break;
			case 4:
				parsed.minute = digit + parsed.minute * 10;
				break;
			case 5:
				parsed.second = digit + parsed.second * 10;
				break;
			case 6:
				parsed.microsecond += scale * digit;
				scale /= 10;
				break;
			default:
				return MLN_XML_ERROR_SYNTAX;
			}
		} else if (c == '-') {
			/* a '-' between the time's fields is skipped */
			if (state <= 1)
				state++;
			else if (state - 5 <= 1)
				break;
		} else if (c == 'T') {
			if (state != 2)
				return MLN_XML_ERROR_SYNTAX;
			state = 3;
		} else if (c == ':') {
			if (state - 3 > 1)
				return MLN_XML_ERROR_SYNTAX;
			state++;
		} else if (c == '.') {
			if (state != 5)
				return MLN_XML_ERROR_SYNTAX;
			state = 6;
		} else if (c == '+' || c == 'Z') {
			break;
		} else {
			return MLN_XML_ERROR_SYNTAX;
		}
		if (p == end)
			return MLN_XML_ERROR_SYNTAX;
	}
	if (c != 'Z') {
		if (end <= mark + 5)
			return MLN_XML_ERROR_SYNTAX;
		minutes = (mark[1] - '0') * 600 + (mark[2] - '0') * 60 + (mark[4] - '0') * 10 +
			  (mark[5] - '0');
		parsed.utcOffset = c == '+' ? -60 * minutes : 60 * minutes;
	}
	MlnMemcpy(time, &parsed, sizeof(parsed));
	return MLN_OK;
}

/*
 * A path step's predicate, the text between '[' and ']': empty (always true), a position
 * (the step's position among its matches), "@name" (the element has the attribute) or
 * "@name=value" with the value quoted (equal values). Sony's checks the closing quote only
 * when the value starts with one, and always drops the value's first and last character.
 */
int MlnXmlElementMatchPredicate(const MlnXmlElement *element, const char *predicate,
				int length, int position)
{
	const MlnXmlAttributeArray *attributes;
	const MlnXmlAttribute *attribute;
	const MlnBuffer *value;
	const char *wanted = NULL;
	const signed char *s = (const signed char *)predicate;
	int name, wantedLength = -1, number, i;

	if (length == 0)
		return 1;
	if (length <= 0)
		return 0;
	for (name = 0; name != length; name++) {
		if (s[name] == '=')
			break;
	}
	if (name == length) {
		if (s[0] >= '0' && s[0] <= '9') {
			number = 0;
			for (i = 0; i != length && s[i] >= '0' && s[i] <= '9'; i++)
				number = number * 10 + s[i] - '0';
			if (i == length)
				return number == position;
		}
	} else {
		if (length <= name + 2)
			return 0;
		if (s[name + 1] == '\'') {
			if (s[length - 1] != '\'')
				return 0;
		} else if (s[name + 1] == '"' && s[length - 1] != '"') {
			return 0;
		}
		wanted = predicate + name + 2;
		wantedLength = length - name - 3;
	}

	if (name == 0 || s[0] != '@')
		return 0;
	attributes = element->attributes;
	for (i = 0; i < attributes->count; i++) {
		attribute = attributes->items[i];
		if (attribute->name->size == (unsigned int)(name - 1) &&
		    (name - 1 == 0 || MlnMemcmp(predicate + 1, attribute->name->data, name - 1) == 0))
			goto found;
	}
	return 0;

found:
	value = attribute->value;
	if (wantedLength < 0)
		return 1;
	if ((unsigned int)wantedLength != value->size)
		return 0;
	if (wantedLength == 0)
		return 1;
	return MlnMemcmp(wanted, value->data, wantedLength) == 0;
}

/*
 * Whether an element is in a namespace: the declaration in scope for its name's prefix
 * (the default declaration when the name has none) has that value.
 */
int MlnXmlElementInNamespace(const MlnXmlElement *element, const void *uri, unsigned int size)
{
	const MlnBuffer *name = element->name;
	unsigned int length = name->size;
	const unsigned char *data = name->data;
	const MlnXmlAttributeArray *namespaces;
	const MlnXmlAttribute *declaration;
	unsigned int prefix;
	int count, i;

	for (prefix = 0; prefix != length; prefix++) {
		if (data[prefix] == ':')
			break;
	}
	namespaces = element->namespaces;
	count = namespaces->count;
	if (count == 0)
		return 0;
	if (prefix != length && prefix != 0) {
		for (i = 0; i < count; i++) {
			declaration = namespaces->items[i];
			if (declaration->name->size == prefix &&
			    MlnMemcmp(data, declaration->name->data, prefix) == 0 &&
			    declaration->value->size == size &&
			    MlnMemcmp(uri, declaration->value->data, size) == 0)
				return 1;
		}
		return 0;
	}
	for (i = 0; i < count; i++) {
		declaration = namespaces->items[i];
		if (declaration->name->size == 0 && declaration->value->size == size &&
		    MlnMemcmp(uri, declaration->value->data, size) == 0)
			return 1;
	}
	return 0;
}
