/*
 * The XML writer's side of the tree: values turned into text (integers, big-endian
 * integers of any length, dateTimes), elements built from a name, attributes and content,
 * and attributes, text and children added to or replaced in an element. The file's name
 * is ours.
 */
#include "mln_internal.h"

/* the decimal text of an integer */
__attribute__((noinline)) int MlnXmlIntToText(int heap, int value, MlnBuffer **text)
{
	MlnBuffer *buffer = NULL;
	unsigned int magnitude = value < 0 ? -value : value;
	char digits[11];
	int start = 11;
	int ret;

	do {
		digits[--start] = '0' + magnitude % 10;
		magnitude /= 10;
	} while (magnitude != 0);
	if (value < 0)
		digits[--start] = '-';
	buffer = MlnBufferCreate(heap, sizeof(digits) - start, &digits[start]);
	if (buffer != NULL) {
		*text = buffer;
		buffer = NULL;
		ret = MLN_OK;
	} else {
		ret = MLN_ERROR_NO_MEMORY;
	}
	MlnBufferDestroy(heap, buffer);
	return ret;
}

/* the decimal text of an unsigned big-endian integer of any length */
__attribute__((noinline)) int MlnXmlBytesToDecimal(int heap, const MlnBuffer *number,
						   MlnBuffer **text)
{
	MlnBuffer *buffer = NULL;
	int size = number->size;
	unsigned char *work;
	char *digits;
	int length, position, remainder, value, i;
	int ret;

	work = MlnAllocate(heap, size);
	if (work == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	for (i = 0; i < size; i++)
		work[i] = number->data[i];

	/* room for 32 / 13 (more than log10(256)) digits a byte */
	length = 4 * (8 * size / 13 + 1);
	digits = MlnAllocate(heap, length + 1);
	if (digits == NULL) {
		MlnFree(heap, work);
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	digits[length] = '\0';
	position = length;
	if (size > 0) {
		for (;;) {
			for (i = 0; i != size; i++) {
				if (work[i] != 0)
					break;
			}
			if (i == size)
				break;
			remainder = 0;
			for (i = 0; i != size; i++) {
				value = work[i] + (remainder << 8);
				work[i] = value / 10;
				remainder = value % 10;
			}
			digits[--position] = '0' + remainder;
		}
	}
	if (position == length)
		digits[--position] = '0';
	buffer = MlnBufferCreate(heap, length - position, &digits[position]);
	if (buffer != NULL) {
		*text = buffer;
		buffer = NULL;
		ret = MLN_OK;
	} else {
		ret = MLN_ERROR_NO_MEMORY;
	}
	MlnFree(heap, work);
	MlnFree(heap, digits);
out:
	MlnBufferDestroy(heap, buffer);
	return ret;
}

/* "YYYY-MM-DDThh:mm:ss.fff" and "Z" or the UTC offset "+hh:mm" */
__attribute__((noinline)) int MlnXmlDateTimeToText(int heap, const MlnDateTime *time,
						   MlnBuffer **text)
{
	MlnBuffer *buffer = NULL;
	char digits[29];
	unsigned int offset, length;
	int ret;

	digits[0] = '0' + time->year / 1000 % 10;
	digits[1] = '0' + time->year / 100 % 10;
	digits[2] = '0' + time->year / 10 % 10;
	digits[3] = '0' + time->year % 10;
	digits[4] = '-';
	digits[5] = '0' + time->month / 10 % 10;
	digits[6] = '0' + time->month % 10;
	digits[7] = '-';
	digits[8] = '0' + time->day / 10 % 10;
	digits[9] = '0' + time->day % 10;
	digits[10] = 'T';
	digits[11] = '0' + time->hour / 10 % 10;
	digits[12] = '0' + time->hour % 10;
	digits[13] = ':';
	digits[14] = '0' + time->minute / 10 % 10;
	digits[15] = '0' + time->minute % 10;
	digits[16] = ':';
	digits[17] = '0' + time->second / 10 % 10;
	digits[18] = '0' + time->second % 10;
	digits[19] = '.';
	digits[20] = '0' + time->microsecond / 100 % 10;
	digits[21] = '0' + time->microsecond / 10 % 10;
	digits[22] = '0' + time->microsecond % 10;
	if (time->utcOffset == 0) {
		digits[23] = 'Z';
		length = 24;
	} else {
		if (time->utcOffset < 0) {
			digits[23] = '-';
			offset = -time->utcOffset;
		} else {
			digits[23] = '+';
			offset = time->utcOffset;
		}
		digits[24] = '0' + offset / 3600 / 10 % 10;
		digits[25] = '0' + offset / 3600 % 10;
		digits[26] = ':';
		digits[27] = '0' + offset / 60 / 10 % 10;
		digits[28] = '0' + offset / 60 % 10;
		length = 29;
	}
	buffer = MlnBufferCreate(heap, length, digits);
	if (buffer != NULL) {
		ret = MLN_OK;
		*text = buffer;
		buffer = NULL;
	} else {
		ret = MLN_ERROR_NO_MEMORY;
	}
	MlnBufferDestroy(heap, buffer);
	return ret;
}

/*
 * An element and its descendants given the declarations in scope at their new place:
 * each declaration of a prefix the element does not declare itself is added to its own.
 */
__attribute__((noinline)) int MlnXmlElementInheritNamespaces(int heap, MlnXmlElement *element,
							     MlnXmlAttributeArray *namespaces)
{
	MlnXmlAttributeArray *merged = NULL, *own;
	MlnXmlAttribute *declaration;
	MlnXmlContentArray *content;
	MlnXmlContent *item;
	int missing[32];
	int count = namespaces->count;
	int ownCount, added, children, i, k;
	int ret;

	if (count > 0) {
		own = element->namespaces;
		ownCount = own->count;
		if (ownCount != 0) {
			added = 0;
			for (i = 0; i != count; i++) {
				declaration = namespaces->items[i];
				for (k = 0; k < ownCount; k++) {
					if (MlnBufferCompare(heap, declaration->name,
							     own->items[k]->name) == 0)
						break;
				}
				if (k < ownCount)
					continue;
				missing[added++] = i;
			}
			if (added == 0)
				goto children;
			ret = MlnXmlAttributeArrayCreate(heap, &merged, ownCount + added);
			if (ret != MLN_OK)
				return ret;
			for (k = 0; k < ownCount; k++) {
				declaration = own->items[k];
				declaration->refCount++;
				merged->items[k] = declaration;
			}
		} else {
			for (i = 0; i < count; i++)
				missing[i] = i;
			ret = MlnXmlAttributeArrayCreate(heap, &merged, count);
			if (ret != MLN_OK)
				return ret;
			added = count;
		}
		for (i = 0; i < added; i++) {
			declaration = namespaces->items[missing[i]];
			declaration->refCount++;
			merged->items[ownCount + i] = declaration;
		}
		element->namespaces = merged;
		merged = NULL;
		MlnXmlAttributeArrayRelease(heap, &own);
	}

children:
	content = element->content;
	children = content->count;
	own = element->namespaces;
	for (i = 0; i < children; i++) {
		item = content->items[i];
		if (item->kind != MLN_XML_CONTENT_ELEMENT)
			continue;
		ret = MlnXmlElementInheritNamespaces(heap, item->data.element, own);
		if (ret != MLN_OK)
			return ret;
	}
	return MLN_OK;
}

/* content items added after an element's content */
__attribute__((noinline)) int MlnXmlElementAppendContents(int heap, MlnXmlElement *element,
							  int count, MlnXmlContent **items)
{
	MlnXmlContentArray *array = NULL;
	MlnXmlContentArray *old = element->content;
	int total = count, kept;
	int ret;

	if (old != NULL)
		total = old->count + count;
	ret = MlnXmlContentArrayCreate(heap, &array, total);
	if (ret != MLN_OK) {
		if (array != NULL)
			MlnXmlContentArrayRelease(heap, &array);
		return ret;
	}
	if (old != NULL) {
		kept = old->count;
		MlnMemcpy(array->items, old->items, kept * sizeof(array->items[0]));
		MlnMemset(old->items, 0, kept * sizeof(old->items[0]));
		MlnXmlContentArrayRelease(heap, &old);
	} else {
		kept = 0;
	}
	MlnMemcpy(&array->items[kept], items, count * sizeof(array->items[0]));
	element->content = array;
	return ret;
}

/* a child element taken out of an element's content */
__attribute__((noinline)) int MlnXmlElementRemoveChild(int heap, MlnXmlElement *element,
						       MlnXmlElement *child)
{
	MlnXmlContentArray *content = element->content;
	MlnXmlContent *removed;
	int count = content->count;
	int i;

	for (i = 0; i < count; i++) {
		removed = content->items[i];
		if (removed->kind == MLN_XML_CONTENT_ELEMENT && removed->data.element == child)
			break;
	}
	if (i >= count)
		return MLN_OK;
	for (; i < count - 1; i++)
		content->items[i] = content->items[i + 1];
	content->count = count - 1;
	MlnXmlContentRelease(heap, &removed);
	return MLN_OK;
}

/* a child element of an element replaced by a content item */
__attribute__((noinline)) int MlnXmlElementReplaceChild(int heap, MlnXmlElement *element,
							MlnXmlElement *child, MlnXmlContent *content)
{
	MlnXmlContentArray *array = element->content;
	MlnXmlContent *replaced;
	int count = array->count;
	int i;

	for (i = 0; i < count; i++) {
		replaced = array->items[i];
		if (replaced->kind == MLN_XML_CONTENT_ELEMENT && replaced->data.element == child) {
			array->items[i] = content;
			MlnXmlContentRelease(heap, &replaced);
			break;
		}
	}
	return MLN_OK;
}

/* whether an attribute name declares a namespace: "xmlns" or "xmlns:prefix" */
#define MLN_XML_IS_DECLARATION(compare, name) \
	((compare)("xmlns", (name), 5) == 0 && ((name)[5] == '\0' || (name)[5] == ':'))

/*
 * An element node built from its name (with a prefix: "prefix:name"), the namespace that
 * prefix is declared as, attributes ("xmlns" and "xmlns:p" ones declare namespaces) and
 * content. Element content takes on the new element's declarations. With release set, the
 * content's buffers and nodes are released once the element holds what it needs of them.
 */
__attribute__((noinline)) int MlnXmlElementBuild(int heap, const char *name, const char *prefix,
						 const char *uri, int attributeCount,
						 const MlnXmlBuildAttribute *attributeList,
						 int contentCount, MlnXmlBuildContent *contentList,
						 int release, MlnXmlNode **out)
{
	MlnXmlElement *element = NULL;
	MlnXmlNode *node = NULL, *child;
	MlnXmlAttribute *attribute = NULL;
	MlnXmlAttributeArray *namespaces = NULL, *attributes = NULL;
	MlnXmlContent *content = NULL;
	MlnXmlContentArray *contents = NULL;
	const MlnBuffer *source;
	const char *attributeName;
	unsigned char *data;
	unsigned int length, prefixLength;
	int declarations, others, declared, other, i;
	int ret;

	ret = MlnXmlElementCreate(heap, &element);
	if (ret != MLN_OK)
		goto out;
	length = MlnStrlen(name);
	if (prefix == NULL || *prefix == '\0') {
		element->name = MlnBufferCreate(heap, length, name);
		if (element->name == NULL)
			goto no_memory;
	} else {
		prefixLength = MlnStrlen(prefix);
		element->name = MlnBufferCreate(heap, length + 1 + prefixLength, NULL);
		if (element->name == NULL)
			goto no_memory;
		data = element->name->data;
		MlnMemcpy(data, prefix, prefixLength);
		data[prefixLength] = ':';
		MlnMemcpy(data + prefixLength + 1, name, length);
	}

	declared = uri != NULL;
	declarations = declared;
	others = 0;
	for (i = 0; i < attributeCount; i++) {
		if (MLN_XML_IS_DECLARATION(MlnMemcmp, attributeList[i].name))
			declarations++;
		else
			others++;
	}
	ret = MlnXmlAttributeArrayCreate(heap, &namespaces, declarations);
	if (ret != MLN_OK)
		goto out;
	ret = MlnXmlAttributeArrayCreate(heap, &attributes, others);
	if (ret != MLN_OK)
		goto out;

	/* the element's own prefix first */
	if (declared) {
		ret = MlnXmlAttributeCreate(heap, &attribute);
		if (ret != MLN_OK)
			goto out;
		attribute->name = MlnBufferCreate(heap, MlnStrlen(prefix), prefix);
		if (attribute->name == NULL)
			goto no_memory;
		attribute->value = MlnBufferCreate(heap, MlnStrlen(uri), uri);
		if (attribute->value == NULL)
			goto no_memory;
		namespaces->items[0] = attribute;
		attribute = NULL;
	}
	other = 0;
	for (i = 0; i < attributeCount; i++) {
		ret = MlnXmlAttributeCreate(heap, &attribute);
		if (ret != MLN_OK)
			goto out;
		attribute->value = MlnBufferCreate(heap, MlnStrlen(attributeList[i].value),
						   attributeList[i].value);
		if (attribute->value == NULL)
			goto no_memory;
		attributeName = attributeList[i].name;
		if (MLN_XML_IS_DECLARATION(MlnMemcmp, attributeName)) {
			if (attributeName[5] == '\0')
				attribute->name = MlnBufferCreate(heap, 0, NULL);
			else
				attribute->name = MlnBufferCreate(heap, MlnStrlen(attributeName) - 6,
								  attributeName + 6);
			if (attribute->name == NULL)
				goto no_memory;
			namespaces->items[declared++] = attribute;
		} else {
			attribute->name = MlnBufferCreate(heap, MlnStrlen(attributeName),
							  attributeName);
			if (attribute->name == NULL)
				goto no_memory;
			attributes->items[other++] = attribute;
		}
		attribute = NULL;
	}
	element->namespaces = namespaces;
	namespaces = NULL;
	element->attributes = attributes;
	attributes = NULL;

	for (i = 0; i < contentCount; i++) {
		if (contentList[i].kind != MLN_XML_BUILD_NODE)
			continue;
		child = contentList[i].value.node;
		ret = MlnXmlElementInheritNamespaces(heap, child->object.element,
						     element->namespaces);
		if (ret != MLN_OK)
			goto out;
	}
	ret = MlnXmlContentArrayCreate(heap, &contents, contentCount);
	if (ret != MLN_OK)
		goto out;
	for (i = 0; i < contentCount; i++) {
		ret = MlnXmlContentCreate(heap, &content);
		if (ret != MLN_OK)
			goto out;
		switch (contentList[i].kind) {
		case MLN_XML_BUILD_STRING:
			content->kind = MLN_XML_CONTENT_TEXT;
			content->data.text = MlnBufferCreate(heap, MlnStrlen(contentList[i].value.string),
							     contentList[i].value.string);
			if (content->data.text == NULL)
				goto no_memory;
			break;
		case MLN_XML_BUILD_BUFFER:
			content->kind = MLN_XML_CONTENT_TEXT;
			source = contentList[i].value.buffer;
			content->data.text = MlnBufferCreate(heap, source->size, source->data);
			if (content->data.text == NULL)
				goto no_memory;
			break;
		case MLN_XML_BUILD_INT:
			content->kind = MLN_XML_CONTENT_TEXT;
			ret = MlnXmlIntToText(heap, contentList[i].value.integer, &content->data.text);
			if (ret != MLN_OK)
				goto out;
			break;
		case MLN_XML_BUILD_DATETIME:
			content->kind = MLN_XML_CONTENT_TEXT;
			ret = MlnXmlDateTimeToText(heap, contentList[i].value.time,
						   &content->data.text);
			if (ret != MLN_OK)
				goto out;
			break;
		case MLN_XML_BUILD_NUMBER:
			content->kind = MLN_XML_CONTENT_TEXT;
			ret = MlnXmlBytesToDecimal(heap, contentList[i].value.buffer,
						   &content->data.text);
			if (ret != MLN_OK)
				goto out;
			break;
		default:
			content->kind = MLN_XML_CONTENT_ELEMENT;
			child = contentList[i].value.node;
			child->object.element->refCount++;
			content->data.element = child->object.element;
			break;
		}
		contents->items[i] = content;
		content = NULL;
	}
	element->content = contents;
	contents = NULL;

	ret = MlnXmlNodeCreate(heap, &node);
	if (ret != MLN_OK)
		goto out;
	node->type = MLN_XML_NODE_ELEMENT;
	node->object.element = element;
	element = NULL;
	*out = node;
	node = NULL;
	if (release) {
		for (i = 0; i < contentCount; i++) {
			switch (contentList[i].kind) {
			case MLN_XML_BUILD_BUFFER:
			case MLN_XML_BUILD_NUMBER:
				MlnBufferDestroy(heap, contentList[i].value.buffer);
				contentList[i].value.buffer = NULL;
				break;
			case MLN_XML_BUILD_NODE:
				child = contentList[i].value.node;
				MlnXmlNodeRelease(heap, &child);
				contentList[i].value.node = NULL;
				break;
			}
		}
	}
	goto out;

no_memory:
	ret = MLN_ERROR_NO_MEMORY;
out:
	if (attribute != NULL)
		MlnXmlAttributeRelease(heap, &attribute);
	if (namespaces != NULL)
		MlnXmlAttributeArrayRelease(heap, &namespaces);
	if (attributes != NULL)
		MlnXmlAttributeArrayRelease(heap, &attributes);
	if (content != NULL)
		MlnXmlContentRelease(heap, &content);
	if (contents != NULL)
		MlnXmlContentArrayRelease(heap, &contents);
	if (element != NULL)
		MlnXmlElementRelease(heap, &element);
	if (node != NULL)
		MlnXmlNodeRelease(heap, &node);
	return ret;
}

/* an attribute ("xmlns" and "xmlns:p": a namespace declaration) added to an element node */
__attribute__((noinline)) int MlnXmlNodeAddAttribute(int heap, MlnXmlNode *node, const char *name,
						     const char *value)
{
	MlnXmlAttributeArray *merged = NULL, *old;
	MlnXmlAttributeArray **slot;
	MlnXmlAttribute *attribute = NULL;
	MlnXmlElement *element;
	int count, ret;

	if (node->type != MLN_XML_NODE_ELEMENT)
		return MLN_XML_ERROR_NOT_ELEMENT;
	ret = MlnXmlAttributeCreate(heap, &attribute);
	if (ret != MLN_OK)
		goto out;
	attribute->value = MlnBufferCreate(heap, MlnStrlen(value), value);
	if (attribute->value == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	element = node->object.element;
	if (MLN_XML_IS_DECLARATION(MlnStrncmp, name)) {
		slot = &element->namespaces;
		if (name[5] == '\0')
			attribute->name = MlnBufferCreate(heap, 0, NULL);
		else
			attribute->name = MlnBufferCreate(heap, MlnStrlen(name) - 6, name + 6);
	} else {
		attribute->name = MlnBufferCreate(heap, MlnStrlen(name), name);
		slot = &element->attributes;
	}
	if (attribute->name == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	old = *slot;
	count = old != NULL ? old->count + 1 : 1;
	ret = MlnXmlAttributeArrayCreate(heap, &merged, count);
	if (ret == MLN_OK) {
		if (old != NULL) {
			MlnMemcpy(merged->items, old->items, (count - 1) * sizeof(merged->items[0]));
			MlnMemset(old->items, 0, (count - 1) * sizeof(old->items[0]));
			MlnXmlAttributeArrayRelease(heap, &old);
		}
		merged->items[count - 1] = attribute;
		attribute = NULL;
		*slot = merged;
		merged = NULL;
	}
out:
	if (attribute != NULL)
		MlnXmlAttributeRelease(heap, &attribute);
	return ret;
}

/* text added after an element node's content */
__attribute__((noinline)) int MlnXmlNodeAddText(int heap, MlnXmlNode *node, const char *text)
{
	MlnXmlContentArray *array = NULL, *old;
	MlnXmlContent *content = NULL;
	MlnXmlElement *element;
	int count, ret;

	if (node->type != MLN_XML_NODE_ELEMENT)
		return MLN_XML_ERROR_NOT_ELEMENT;
	element = node->object.element;
	ret = MlnXmlContentCreate(heap, &content);
	if (ret != MLN_OK)
		goto out;
	content->kind = MLN_XML_CONTENT_TEXT;
	content->data.text = MlnBufferCreate(heap, MlnStrlen(text), text);
	if (content->data.text == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	old = element->content;
	count = old != NULL ? old->count + 1 : 1;
	ret = MlnXmlContentArrayCreate(heap, &array, count);
	if (ret == MLN_OK) {
		if (old != NULL) {
			MlnMemcpy(array->items, old->items, (count - 1) * sizeof(array->items[0]));
			MlnMemset(old->items, 0, (count - 1) * sizeof(old->items[0]));
			MlnXmlContentArrayRelease(heap, &old);
		}
		array->items[count - 1] = content;
		content = NULL;
		element->content = array;
		array = NULL;
	}
out:
	if (content != NULL)
		MlnXmlContentRelease(heap, &content);
	if (array != NULL)
		MlnXmlContentArrayRelease(heap, &array);
	return ret;
}
