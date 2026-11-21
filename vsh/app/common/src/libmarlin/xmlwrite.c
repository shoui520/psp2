/*
 * The XML writer: a node of the parser's tree as XML text. Each element declares the
 * namespaces it needs that the enclosing elements have not declared with the same value,
 * sorted by prefix, then its attributes, sorted by name, then its content. The file's
 * name is ours.
 */
#include "mln_internal.h"

static const char MlnXmlHexDigits[] = "0123456789ABCDEF";

/* text content, its markup characters as references */
int MlnXmlWriteText(int heap, const MlnBuffer *text, MlnTextBuffer *out)
{
	const unsigned char *p = text->data, *end;
	int size = text->size;
	char reference[5];
	int ret;

	reference[0] = '&';
	reference[1] = '#';
	reference[2] = 'x';
	reference[3] = '0';
	reference[4] = ';';

	if (size <= 0)
		return 0;
	end = p + size;
	do {
		if (*p == '\r') {
			reference[3] = MlnXmlHexDigits['\r'];
			ret = MlnTextBufferAppend(heap, out, 5, reference);
		} else if (*p == '<') {
			ret = MlnTextBufferAppend(heap, out, 4, "&lt;");
		} else if (*p == '>') {
			ret = MlnTextBufferAppend(heap, out, 4, "&gt;");
		} else if (*p == '&') {
			ret = MlnTextBufferAppend(heap, out, 5, "&amp;");
		} else {
			ret = MlnTextBufferAppend(heap, out, 1, p);
		}
		if (ret != 0)
			return ret;
		p++;
	} while (p != end);
	return 0;
}

/* the length of a name's prefix, the bytes before its first ':' (0 without one) */
static inline unsigned int MlnXmlPrefixLength(const MlnBuffer *name)
{
	unsigned int i;

	for (i = 0; i < name->size; i++) {
		if (name->data[i] == ':')
			return i;
	}
	return 0;
}

/* an element, its namespace declarations, attributes and content */
int MlnXmlWriteElement(int heap, MlnXmlParser *writer, const MlnXmlElement *element,
		       const MlnXmlElement *except, MlnTextBuffer *out)
{
	const MlnXmlAttributeArray *namespaces, *attributes;
	const MlnXmlContentArray *content;
	const MlnXmlContent *item;
	MlnXmlAttribute *declaration, *declared, *attribute;
	const MlnBuffer *prefix, *value, *name, *least;
	const unsigned char *p, *end;
	unsigned int elementPrefix, count, i, j, k, leastIndex, swap;
	unsigned int *order = NULL;
	MlnXmlLink *link;
	int depth = writer->namespaces.depth;
	int number, size, used;
	int ret;

	if (element == except)
		return 0;
	ret = MlnTextBufferAppend(heap, out, 1, "<");
	if (ret != 0)
		goto out;
	ret = MlnTextBufferAppend(heap, out, element->name->size, element->name->data);
	if (ret != 0)
		goto out;
	elementPrefix = MlnXmlPrefixLength(element->name);
	namespaces = element->namespaces;
	attributes = element->attributes;

	/* the declarations this element needs, in prefix order */
	count = namespaces->count;
	if (count != 0) {
		order = MlnAllocate(heap, count * sizeof(*order));
		if (order == NULL) {
			ret = 2;
			goto fail;
		}
		for (i = 0; i < count; i++)
			order[i] = i;
		for (i = 0; i < count - 1; i++) {
			leastIndex = i;
			least = namespaces->items[order[i]]->name;
			for (j = i + 1; j < count; j++) {
				name = namespaces->items[order[j]]->name;
				if (MlnBufferCompare(heap, least, name) > 0) {
					leastIndex = j;
					least = name;
				}
			}
			if (i != leastIndex) {
				swap = order[leastIndex];
				order[leastIndex] = order[i];
				order[i] = swap;
			}
		}
		for (k = 0; k < count; k++) {
			declaration = namespaces->items[order[k]];
			prefix = declaration->name;
			used = 0;
			if (prefix->size == elementPrefix &&
			    (elementPrefix == 0 ||
			     MlnMemcmp(prefix->data, element->name->data, elementPrefix) == 0)) {
				used = 1;
			} else if (prefix->size != 0) {
				for (j = 0; j < (unsigned int)attributes->count; j++) {
					name = attributes->items[j]->name;
					if (name->size == 0)
						continue;
					if (prefix->size == MlnXmlPrefixLength(name) &&
					    MlnMemcmp(prefix->data, name->data, prefix->size) == 0) {
						used = 1;
						break;
					}
				}
			}
			if (!used)
				continue;
			value = declaration->value;
			/* the innermost declaration of the prefix in scope, when it has this value */
			for (link = writer->namespaces.top; link != NULL; link = link->next) {
				declared = link->item;
				if (prefix->size == 0 ? declared->name->size == 0 :
				    declared->name->size == prefix->size &&
				    MlnMemcmp(prefix->data, declared->name->data, prefix->size) == 0)
					break;
			}
			if (link != NULL && MlnBufferCompare(heap, value, declared->value) == 0)
				continue;
			ret = MlnTextBufferAppend(heap, out, 6, " xmlns");
			if (ret != 0)
				goto fail;
			if (prefix->size != 0) {
				ret = MlnTextBufferAppend(heap, out, 1, ":");
				if (ret != 0)
					goto fail;
				ret = MlnTextBufferAppend(heap, out, prefix->size, prefix->data);
				if (ret != 0)
					goto fail;
			}
			ret = MlnTextBufferAppend(heap, out, 2, "=\"");
			if (ret != 0)
				goto fail;
			ret = MlnTextBufferAppend(heap, out, value->size, value->data);
			if (ret != 0)
				goto fail;
			ret = MlnTextBufferAppend(heap, out, 1, "\"");
			if (ret != 0)
				goto fail;
			declaration->refCount++;
			ret = MlnXmlStackPush(heap, &writer->namespaces, declaration);
			if (ret != 0) {
				MlnXmlAttributeRelease(heap, &declaration);
				goto fail;
			}
		}
	}
	MlnFree(heap, order);

	/* the attributes, in name order, their values with references */
	order = NULL;
	number = attributes->count;
	if (number > 0) {
		order = MlnAllocate(heap, number * sizeof(*order));
		if (order == NULL) {
			ret = 2;
			goto fail;
		}
		for (i = 0; (int)i < number; i++)
			order[i] = i;
		for (i = 0; i != (unsigned int)number - 1; i++) {
			leastIndex = i;
			least = attributes->items[order[i]]->name;
			for (j = i + 1; j != (unsigned int)number; j++) {
				name = attributes->items[order[j]]->name;
				if (MlnXmlCompareAttributeNames(heap, element->namespaces, element->name,
								least, name) > 0) {
					leastIndex = j;
					least = name;
				}
			}
			if (i != leastIndex) {
				swap = order[leastIndex];
				order[leastIndex] = order[i];
				order[i] = swap;
			}
		}
		for (k = 0; k != (unsigned int)number; k++) {
			attribute = attributes->items[order[k]];
			ret = MlnTextBufferAppend(heap, out, 1, " ");
			if (ret != 0)
				goto fail;
			ret = MlnTextBufferAppend(heap, out, attribute->name->size, attribute->name->data);
			if (ret != 0)
				goto fail;
			ret = MlnTextBufferAppend(heap, out, 2, "=\"");
			if (ret != 0)
				goto fail;
			{
				char reference[5];

				reference[0] = '&';
				reference[1] = '#';
				reference[2] = 'x';
				reference[3] = '0';
				reference[4] = ';';
				p = attribute->value->data;
				size = attribute->value->size;
				if (size > 0) {
					end = p + size;
					do {
						if (*p == '\t' || *p == '\n' || *p == '\r') {
							reference[3] = MlnXmlHexDigits[*p];
							ret = MlnTextBufferAppend(heap, out, 5, reference);
						} else if (*p == '&') {
							ret = MlnTextBufferAppend(heap, out, 5, "&amp;");
						} else if (*p == '<') {
							ret = MlnTextBufferAppend(heap, out, 4, "&lt;");
						} else if (*p == '"') {
							ret = MlnTextBufferAppend(heap, out, 6, "&quot;");
						} else {
							ret = MlnTextBufferAppend(heap, out, 1, p);
						}
						if (ret != 0)
							goto fail;
						p++;
					} while (p != end);
				}
			}
			ret = MlnTextBufferAppend(heap, out, 1, "\"");
			if (ret != 0)
				goto fail;
		}
	}
	MlnFree(heap, order);

	/* the content, or none */
	content = element->content;
	if ((writer->flags & MLN_XML_WRITE_EMPTY_TAGS) && content->count == 0) {
		ret = MlnTextBufferAppend(heap, out, 2, "/>");
		goto out;
	}
	ret = MlnTextBufferAppend(heap, out, 1, ">");
	if (ret != 0)
		goto out;
	number = content->count;
	for (i = 0; (int)i < number; i++) {
		item = content->items[i];
		if (item->kind != MLN_XML_CONTENT_TEXT)
			ret = MlnXmlWriteElement(heap, writer, item->data.element, except, out);
		else
			ret = MlnXmlWriteText(heap, item->data.text, out);
		if (ret != 0)
			goto out;
	}
	ret = MlnTextBufferAppend(heap, out, 2, "</");
	if (ret != 0)
		goto out;
	ret = MlnTextBufferAppend(heap, out, element->name->size, element->name->data);
	if (ret != 0)
		goto out;
	ret = MlnTextBufferAppend(heap, out, 1, ">");
	goto out;

fail:
	MlnFree(heap, order);
out:
	while (writer->namespaces.depth > depth)
		MlnXmlStackPop(heap, &writer->namespaces);
	return ret;
}

/* a document's root element, an element or a text as XML text */
__attribute__((noinline)) int MlnXmlWriteNode(int heap, MlnXmlParser *writer,
					      const MlnXmlNode *node,
					      const MlnXmlElement *except, MlnBuffer **out)
{
	MlnTextBuffer *text;
	MlnBuffer *buffer;
	int ret;

	text = MlnTextBufferCreate(heap, 0, NULL);
	if (text == NULL) {
		ret = 2;
		goto out;
	}
	switch (node->type) {
	case MLN_XML_NODE_DOCUMENT:
		ret = MlnXmlWriteElement(heap, writer, node->object.document->root, except, text);
		break;
	case MLN_XML_NODE_ELEMENT:
		ret = MlnXmlWriteElement(heap, writer, node->object.element, except, text);
		break;
	case MLN_XML_NODE_TEXT:
		ret = MlnXmlWriteText(heap, node->object.text, text);
		break;
	default:
		ret = MLN_XML_ERROR_NODE;
		goto out;
	}
	if (ret != 0)
		goto out;
	buffer = MlnBufferCreate(heap, text->size, text->data);
	if (buffer == NULL) {
		ret = 2;
		goto out;
	}
	*out = buffer;
out:
	MlnTextBufferDestroy(heap, text);
	return ret;
}

int MlnXmlNodeToXml(int heap, const MlnXmlNode *node, MlnBuffer **out)
{
	MlnXmlParser writer;
	int ret;

	MlnXmlParserInit(heap, &writer);
	ret = MlnXmlWriteNode(heap, &writer, node, NULL, out);
	MlnXmlParserTerm(heap, &writer);
	return ret;
}

int MlnXmlNodeToText(int heap, const MlnXmlNode *node, MlnBuffer **text)
{
	MlnXmlParser writer;
	int ret;

	MlnXmlParserInit(heap, &writer);
	writer.flags |= MLN_XML_WRITE_EMPTY_TAGS;
	ret = MlnXmlWriteNode(heap, &writer, node, NULL, text);
	MlnXmlParserTerm(heap, &writer);
	return ret;
}
