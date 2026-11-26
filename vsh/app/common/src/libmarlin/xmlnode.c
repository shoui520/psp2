/*
 * The XML parser's tree: its reference-counted objects (attributes, elements, content,
 * the prolog and the document), the nodes and node lists a caller holds, and the linked
 * lists and the namespace stack the parser builds them with. The file's name is ours.
 */
#include "mln_internal.h"


/*
 * Sony's build inlines the releases into one another to different depths; these copies
 * are the inlined forms, and each out-of-line release is one of them.
 */
#define XML_INLINE	static inline __attribute__((always_inline))

XML_INLINE void MlnXmlAttributeReleaseInline(int heap, MlnXmlAttribute **attribute)
{
	MlnXmlAttribute *own = *attribute;

	if (own == NULL)
		return;
	if (own->refCount >= 0 && own->refCount-- == 0) {
		MlnBufferDestroy(heap, own->name);
		MlnBufferDestroy(heap, own->value);
		MlnFree(heap, own);
	}
	*attribute = NULL;
}

XML_INLINE void MlnXmlAttributeArrayReleaseInline(int heap, MlnXmlAttributeArray **array)
{
	MlnXmlAttributeArray *own = *array;
	int i;

	if (own == NULL)
		return;
	for (i = 0; i < own->count; i++) {
		if (own->items[i] != NULL)
			MlnXmlAttributeReleaseInline(heap, &own->items[i]);
	}
	MlnFree(heap, own);
	*array = NULL;
}

/* calls: whether the attribute arrays are released by calls or inline */
XML_INLINE void MlnXmlElementReleaseInline(int heap, MlnXmlElement **element, int calls)
{
	MlnXmlElement *own = *element;

	if (own == NULL)
		return;
	if (own->refCount >= 0 && own->refCount-- == 0) {
		MlnBufferDestroy(heap, own->name);
		if (calls) {
			MlnXmlAttributeArrayRelease(heap, &own->namespaces);
			MlnXmlAttributeArrayRelease(heap, &own->attributes);
		} else {
			MlnXmlAttributeArrayReleaseInline(heap, &own->namespaces);
			MlnXmlAttributeArrayReleaseInline(heap, &own->attributes);
		}
		MlnXmlContentArrayRelease(heap, &own->content);
		MlnFree(heap, own);
	}
	*element = NULL;
}

XML_INLINE void MlnXmlContentReleaseInline(int heap, MlnXmlContent **content)
{
	MlnXmlContent *own = *content;

	if (own == NULL)
		return;
	if (own->refCount >= 0 && own->refCount-- == 0) {
		if (own->kind == MLN_XML_CONTENT_TEXT)
			MlnBufferDestroy(heap, own->data.text);
		else if (own->kind == MLN_XML_CONTENT_ELEMENT)
			MlnXmlElementReleaseInline(heap, &own->data.element, 0);
		MlnFree(heap, own);
	}
	*content = NULL;
}

XML_INLINE void MlnXmlPrologReleaseInline(int heap, MlnXmlProlog **prolog)
{
	MlnXmlProlog *own = *prolog;

	if (own == NULL)
		return;
	if (own->refCount >= 0 && own->refCount-- == 0) {
		MlnBufferDestroy(heap, own->version);
		MlnBufferDestroy(heap, own->encoding);
		MlnFree(heap, own);
	}
	*prolog = NULL;
}

XML_INLINE void MlnXmlDocumentReleaseInline(int heap, MlnXmlDocument **document)
{
	MlnXmlDocument *own = *document;

	if (own == NULL)
		return;
	if (own->refCount >= 0 && own->refCount-- == 0) {
		MlnXmlPrologReleaseInline(heap, &own->prolog);
		MlnXmlElementReleaseInline(heap, &own->root, 1);
		MlnFree(heap, own);
	}
	*document = NULL;
}

XML_INLINE void MlnXmlLinksFreeInline(int heap, MlnXmlLink **list, int kind)
{
	MlnXmlLink *link = *list, *next;
	void *item;

	if (link == NULL)
		return;
	do {
		item = link->item;
		if (item != NULL) {
			switch (kind) {
			case MLN_XML_LINK_CONTENT:
				MlnXmlContentRelease(heap, (MlnXmlContent **)&item);
				break;
			case MLN_XML_LINK_NODE:
				MlnXmlNodeRelease(heap, (MlnXmlNode **)&item);
				break;
			case MLN_XML_LINK_ATTRIBUTE:
				MlnXmlAttributeRelease(heap, (MlnXmlAttribute **)&item);
				break;
			}
		}
		next = link->next;
		MlnFree(heap, link);
		link = next;
	} while (next != NULL);
	*list = NULL;
}

__attribute__((noinline)) int MlnXmlNodeGetElement(int heap, MlnXmlNode *node, MlnXmlNode **out)
{
	MlnXmlElement *element;
	MlnXmlNode *result;
	int ret;

	if (node->type != MLN_XML_NODE_DOCUMENT) {
		if (node->type != MLN_XML_NODE_ELEMENT)
			return MLN_XML_ERROR_NOT_ELEMENT;
		element = node->object.element;
	} else {
		element = node->object.document->root;
	}
	ret = MlnXmlNodeCreateElement(heap, &result, element);
	if (ret == MLN_OK)
		*out = result;
	return ret;
}

__attribute__((noinline)) int MlnXmlNodeGetName(int heap, MlnXmlNode *node, MlnBuffer **name)
{
	const MlnBuffer *own;
	MlnBuffer *copy;

	if (node->type != MLN_XML_NODE_ATTRIBUTE && node->type != MLN_XML_NODE_ELEMENT &&
	    node->type != MLN_XML_NODE_NAMESPACE)
		return MLN_XML_ERROR_NOT_FOUND;
	/* an attribute's and an element's name are both their object's second word */
	own = node->object.attribute->name;
	copy = MlnBufferCreate(heap, own->size, own->data);
	if (copy == NULL)
		return MLN_ERROR_NO_MEMORY;
	*name = copy;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlBufferSet(int heap, MlnBuffer **buffer, const void *data, unsigned int size)
{
	MlnBuffer *copy = MlnBufferCreate(heap, size, data);

	if (copy == NULL)
		return MLN_ERROR_NO_MEMORY;
	MlnBufferDestroy(heap, *buffer);
	*buffer = copy;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlBufferTruncate(int heap, MlnBuffer **buffer, unsigned int size)
{
	MlnBuffer *copy = MlnBufferCreate(heap, size, (*buffer)->data);

	if (copy == NULL)
		return MLN_ERROR_NO_MEMORY;
	MlnBufferDestroy(heap, *buffer);
	*buffer = copy;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlLinkCreateEmpty(int heap, MlnXmlLink **out)
{
	MlnXmlLink *link = MlnAllocate(heap, sizeof(*link));

	if (link == NULL)
		return MLN_ERROR_NO_MEMORY;
	link->item = NULL;
	link->next = NULL;
	*out = link;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlLinksFree(int heap, MlnXmlLink **list, int kind)
{
	MlnXmlLinksFreeInline(heap, list, kind);
	return MLN_OK;
}

static inline __attribute__((always_inline)) MlnXmlLink *MlnXmlLinkCreate(int heap, void *item)
{
	MlnXmlLink *link = MlnAllocate(heap, sizeof(*link));

	if (link != NULL) {
		link->item = item;
		link->next = NULL;
	}
	return link;
}

static inline __attribute__((always_inline)) int MlnXmlLinkPushInline(int heap, MlnXmlLink **list,
									void *item)
{
	MlnXmlLink *link = MlnXmlLinkCreate(heap, item);

	if (link == NULL)
		return MLN_ERROR_NO_MEMORY;
	link->next = *list;
	*list = link;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlLinkPush(int heap, MlnXmlLink **list, void *item)
{
	return MlnXmlLinkPushInline(heap, list, item);
}

__attribute__((noinline)) int MlnXmlLinkAppend(int heap, MlnXmlLink **list, void *item)
{
	MlnXmlLink **tail = list;
	MlnXmlLink *last = *list;
	MlnXmlLink *link;

	if (last != NULL) {
		while (last->next != NULL)
			last = last->next;
		tail = &last->next;
	}
	link = MlnXmlLinkCreate(heap, item);
	if (link == NULL)
		return MLN_ERROR_NO_MEMORY;
	link->next = *tail;
	*tail = link;
	return MLN_OK;
}

/*
 * Inserts the attribute into the list sorted by name, taking its reference; one whose name
 * is already there is released instead.
 */
static inline __attribute__((always_inline)) int MlnXmlAttributeInsertSortedInline(int heap,
	MlnXmlLink **list, MlnXmlAttribute *attribute)
{
	MlnXmlLink **position = list;
	MlnXmlLink *link = *list, *created;
	MlnBuffer *name = attribute->name;
	int cmp, ret;

	for (; link != NULL; link = link->next) {
		cmp = MlnBufferCompare(heap, name, ((MlnXmlAttribute *)link->item)->name);
		if (cmp == 0) {
			ret = MLN_OK;
			goto out;
		}
		if (cmp < 0)
			break;
		position = &link->next;
	}
	/* the new link is cleared, then the attribute moves into it */
	created = MlnAllocate(heap, sizeof(*created));
	if (created == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
	} else {
		created->item = NULL;
		ret = MLN_OK;
		created->item = attribute;
		created->next = NULL;
		attribute = NULL;
		created->next = *position;
		*position = created;
	}
out:
	if (attribute != NULL)
		MlnXmlAttributeRelease(heap, &attribute);
	return ret;
}

#ifndef MLN_PLAYBACK
__attribute__((noinline)) int MlnXmlAttributeInsertSorted(int heap, MlnXmlLink **list, MlnXmlAttribute *attribute)
{
	return MlnXmlAttributeInsertSortedInline(heap, list, attribute);
}
#endif

__attribute__((noinline)) int MlnXmlLinksPop(int heap, MlnXmlLink **list, int kind)
{
	MlnXmlLink *link = *list;

	if (link == NULL)
		return MLN_OK;
	*list = link->next;
	link->next = NULL;
	MlnXmlLinksFreeInline(heap, &link, kind);
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlLinksToAttributeArray(int heap, MlnXmlLink *list, MlnXmlAttributeArray **out)
{
	MlnXmlAttributeArray *array;
	MlnXmlAttribute *attribute;
	MlnXmlLink *link;
	int count = 0, i, ret;

	for (link = list; link != NULL; link = link->next)
		count++;
	ret = MlnXmlAttributeArrayCreate(heap, &array, count);
	if (ret != MLN_OK)
		return ret;
	for (i = 0, link = list; i != count; i++, link = link->next) {
		attribute = link->item;
		attribute->refCount++;
		array->items[i] = attribute;
	}
	*out = array;
	return MLN_OK;
}

/*
 * The list's attributes sorted by name, each name once, as an array. Each attribute the
 * array takes is counted twice, as Sony's does (nothing calls it).
 */
__attribute__((noinline)) int MlnXmlAttributesSortUnique(int heap, MlnXmlLink *list, MlnXmlAttributeArray **out)
{
	MlnXmlLink *sorted = NULL, *link, *next;
	MlnXmlAttributeArray *array;
	MlnXmlAttribute *attribute;
	int count, i, ret;

	for (link = list; link != NULL; link = link->next) {
		attribute = link->item;
		attribute->refCount++;
		ret = MlnXmlAttributeInsertSortedInline(heap, &sorted, attribute);
		if (ret != MLN_OK)
			goto out;
	}
	count = 0;
	for (link = sorted; link != NULL; link = link->next)
		count++;
	ret = MlnXmlAttributeArrayCreate(heap, &array, count);
	if (ret != MLN_OK)
		goto out;
	for (i = 0, link = sorted; i != count; i++, link = link->next) {
		attribute = link->item;
		attribute->refCount++;
		array->items[i] = attribute;
		link->item = NULL;
	}
	*out = array;
	ret = MLN_OK;
out:
	for (link = sorted; link != NULL; link = next) {
		attribute = link->item;
		if (attribute != NULL)
			MlnXmlAttributeRelease(heap, &attribute);
		next = link->next;
		MlnFree(heap, link);
	}
	return ret;
}

/* the list's content, last first, as an array */
__attribute__((noinline)) int MlnXmlLinksToContentArray(int heap, MlnXmlLink *list, MlnXmlContentArray **out)
{
	MlnXmlContentArray *array;
	MlnXmlContent *content;
	MlnXmlLink *link;
	int count = 0, i, ret;

	for (link = list; link != NULL; link = link->next)
		count++;
	ret = MlnXmlContentArrayCreate(heap, &array, count);
	if (ret != MLN_OK)
		return ret;
	for (i = count - 1, link = list; i != -1; i--, link = link->next) {
		content = link->item;
		content->refCount++;
		array->items[i] = content;
	}
	*out = array;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlLinksToNodeList(int heap, MlnXmlLink *list, MlnXmlNodeList **out)
{
	MlnXmlNodeList *nodes;
	MlnXmlNode *node;
	MlnXmlLink *link;
	int count = 0, i, ret;

	for (link = list; link != NULL; link = link->next)
		count++;
	ret = MlnXmlNodeListCreate(heap, &nodes, count);
	if (ret != MLN_OK)
		return ret;
	for (i = 0, link = list; i != count; i++, link = link->next) {
		node = link->item;
		node->refCount++;
		nodes->nodes[i] = node;
	}
	*out = nodes;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlParserTerm(int heap, MlnXmlParser *parser)
{
	MlnXmlLink *link = parser->namespaces.top, *next;
	MlnXmlAttribute *attribute;

	if (link == NULL)
		return MLN_OK;
	do {
		attribute = link->item;
		if (attribute != NULL)
			MlnXmlAttributeRelease(heap, &attribute);
		next = link->next;
		MlnFree(heap, link);
		link = next;
	} while (next != NULL);
	parser->namespaces.top = NULL;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlStackPush(int heap, MlnXmlStack *stack, MlnXmlAttribute *declaration)
{
	int ret = MlnXmlLinkPushInline(heap, &stack->top, declaration);

	if (ret != MLN_OK)
		return ret;
	stack->depth++;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlStackPop(int heap, MlnXmlStack *stack)
{
	MlnXmlLink *link = stack->top, *next;
	MlnXmlAttribute *attribute;

	if (link == NULL)
		return MLN_OK;
	stack->top = link->next;
	link->next = NULL;
	do {
		attribute = link->item;
		if (attribute != NULL)
			MlnXmlAttributeRelease(heap, &attribute);
		next = link->next;
		MlnFree(heap, link);
		link = next;
	} while (next != NULL);
	stack->depth--;
	return MLN_OK;
}

/* The namespace declarations in scope, the innermost of each prefix, sorted by prefix. */
__attribute__((noinline)) int MlnXmlStackGetNamespaces(int heap, MlnXmlStack *stack, MlnXmlAttributeArray **out)
{
	MlnXmlLink *sorted = NULL, *link, *inner, *next;
	MlnXmlAttributeArray *array;
	MlnXmlAttribute *declaration;
	int count, i, ret;

	if (stack->depth == 0)
		return MlnXmlAttributeArrayCreate(heap, out, 0);
	for (link = stack->top; link != NULL; link = link->next) {
		declaration = link->item;
		for (inner = stack->top; inner != link; inner = inner->next) {
			if (MlnBufferCompare(heap, declaration->name,
					     ((MlnXmlAttribute *)inner->item)->name) == 0)
				break;
		}
		if (inner != link)
			continue;
		declaration->refCount++;
		ret = MlnXmlAttributeInsertSortedInline(heap, &sorted, declaration);
		if (ret != MLN_OK)
			goto out;
	}
	count = 0;
	for (link = sorted; link != NULL; link = link->next)
		count++;
	ret = MlnXmlAttributeArrayCreate(heap, &array, count);
	if (ret != MLN_OK)
		goto out;
	for (i = 0, link = sorted; i != count; i++, link = link->next) {
		declaration = link->item;
		declaration->refCount++;
		array->items[i] = declaration;
	}
	*out = array;
out:
	for (link = sorted; link != NULL; link = next) {
		declaration = link->item;
		if (declaration != NULL)
			MlnXmlAttributeRelease(heap, &declaration);
		next = link->next;
		MlnFree(heap, link);
	}
	return ret;
}

__attribute__((noinline)) int MlnXmlContentCreate(int heap, MlnXmlContent **out)
{
	MlnXmlContent *content = MlnAllocate(heap, sizeof(*content));

	if (content == NULL)
		return MLN_ERROR_NO_MEMORY;
	content->refCount = 0;
	content->kind = MLN_XML_CONTENT_TEXT;
	content->data.text = NULL;
	*out = content;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlContentArrayCreate(int heap, MlnXmlContentArray **out, int count)
{
	MlnXmlContentArray *array;
	int i;

	if (count < 0)
		count = 0;
	array = MlnAllocate(heap, (count + 2) * sizeof(void *));
	if (array == NULL)
		return MLN_ERROR_NO_MEMORY;
	array->refCount = 0;
	array->count = count;
	for (i = 0; i < count; i++)
		array->items[i] = NULL;
	*out = array;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlContentArrayRelease(int heap, MlnXmlContentArray **array)
{
	MlnXmlContentArray *own = *array;
	int i;

	if (own == NULL)
		return MLN_OK;
	if (own->refCount >= 0 && own->refCount-- == 0) {
		for (i = 0; i < own->count; i++)
			MlnXmlContentReleaseInline(heap, &own->items[i]);
		MlnFree(heap, own);
	}
	*array = NULL;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlAttributeCreate(int heap, MlnXmlAttribute **out)
{
	MlnXmlAttribute *attribute = MlnAllocate(heap, sizeof(*attribute));

	if (attribute == NULL)
		return MLN_ERROR_NO_MEMORY;
	attribute->refCount = 0;
	attribute->name = NULL;
	attribute->value = NULL;
	*out = attribute;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlAttributeRelease(int heap, MlnXmlAttribute **attribute)
{
	MlnXmlAttributeReleaseInline(heap, attribute);
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlAttributeArrayCreate(int heap, MlnXmlAttributeArray **out, int count)
{
	MlnXmlAttributeArray *array;
	int i;

	if (count < 0)
		count = 0;
	array = MlnAllocate(heap, (count + 1) * sizeof(void *));
	if (array == NULL)
		return MLN_ERROR_NO_MEMORY;
	array->count = count;
	for (i = 0; i < count; i++)
		array->items[i] = NULL;
	*out = array;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlAttributeArrayRelease(int heap, MlnXmlAttributeArray **array)
{
	MlnXmlAttributeArrayReleaseInline(heap, array);
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlElementCreate(int heap, MlnXmlElement **out)
{
	MlnXmlElement *element = MlnAllocate(heap, sizeof(*element));

	if (element == NULL)
		return MLN_ERROR_NO_MEMORY;
	element->refCount = 0;
	element->name = NULL;
	element->namespaces = NULL;
	element->attributes = NULL;
	element->content = NULL;
	*out = element;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlElementRelease(int heap, MlnXmlElement **element)
{
	MlnXmlElementReleaseInline(heap, element, 0);
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlContentRelease(int heap, MlnXmlContent **content)
{
	MlnXmlContent *own = *content;

	if (own == NULL)
		return MLN_OK;
	if (own->refCount >= 0 && own->refCount-- == 0) {
		if (own->kind == MLN_XML_CONTENT_TEXT)
			MlnBufferDestroy(heap, own->data.text);
		else if (own->kind == MLN_XML_CONTENT_ELEMENT)
			MlnXmlElementRelease(heap, &own->data.element);
		MlnFree(heap, own);
	}
	*content = NULL;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlPrologCreate(int heap, MlnXmlProlog **out)
{
	MlnXmlProlog *prolog = MlnAllocate(heap, sizeof(*prolog));

	if (prolog == NULL)
		return MLN_ERROR_NO_MEMORY;
	prolog->refCount = 0;
	prolog->version = NULL;
	prolog->encoding = NULL;
	*out = prolog;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlPrologRelease(int heap, MlnXmlProlog **prolog)
{
	MlnXmlPrologReleaseInline(heap, prolog);
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlDocumentCreate(int heap, MlnXmlDocument **out)
{
	MlnXmlDocument *document = MlnAllocate(heap, sizeof(*document));

	if (document == NULL)
		return MLN_ERROR_NO_MEMORY;
	document->refCount = 0;
	document->prolog = NULL;
	document->root = NULL;
	*out = document;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlDocumentRelease(int heap, MlnXmlDocument **document)
{
	MlnXmlDocument *own = *document;

	if (own == NULL)
		return MLN_OK;
	if (own->refCount >= 0 && own->refCount-- == 0) {
		MlnXmlPrologReleaseInline(heap, &own->prolog);
		MlnXmlElementRelease(heap, &own->root);
		MlnFree(heap, own);
	}
	*document = NULL;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlNodeCreate(int heap, MlnXmlNode **out)
{
	MlnXmlNode *node = MlnAllocate(heap, sizeof(*node));

	if (node == NULL)
		return MLN_ERROR_NO_MEMORY;
	node->refCount = 0;
	node->type = MLN_XML_NODE_DOCUMENT;
	node->object.document = NULL;
	*out = node;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlNodeCreateDocument(int heap, MlnXmlNode **out, MlnXmlDocument *document)
{
	MlnXmlNode *node = MlnAllocate(heap, sizeof(*node));

	if (node == NULL)
		return MLN_ERROR_NO_MEMORY;
	node->object.document = document;
	node->refCount = 0;
	node->type = MLN_XML_NODE_DOCUMENT;
	document->refCount++;
	*out = node;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlNodeCreateElement(int heap, MlnXmlNode **out, MlnXmlElement *element)
{
	MlnXmlNode *node = MlnAllocate(heap, sizeof(*node));

	if (node == NULL)
		return MLN_ERROR_NO_MEMORY;
	node->refCount = 0;
	node->type = MLN_XML_NODE_ELEMENT;
	node->object.element = element;
	element->refCount++;
	*out = node;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlNodeCreateNamespace(int heap, MlnXmlNode **out, MlnXmlAttribute *declaration)
{
	MlnXmlNode *node = MlnAllocate(heap, sizeof(*node));

	if (node == NULL)
		return MLN_ERROR_NO_MEMORY;
	node->refCount = 0;
	node->type = MLN_XML_NODE_NAMESPACE;
	node->object.attribute = declaration;
	declaration->refCount++;
	*out = node;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlNodeCreateAttribute(int heap, MlnXmlNode **out, MlnXmlAttribute *attribute)
{
	MlnXmlNode *node = MlnAllocate(heap, sizeof(*node));

	if (node == NULL)
		return MLN_ERROR_NO_MEMORY;
	node->refCount = 0;
	node->type = MLN_XML_NODE_ATTRIBUTE;
	node->object.attribute = attribute;
	attribute->refCount++;
	*out = node;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlNodeCreateText(int heap, MlnXmlNode **out, const MlnBuffer *text)
{
	MlnXmlNode *node = MlnAllocate(heap, sizeof(*node));
	MlnBuffer *copy;
	int ret;

	if (node != NULL) {
		node->refCount = 0;
		node->type = MLN_XML_NODE_TEXT;
		copy = MlnBufferCreate(heap, text->size, text->data);
		if (copy != NULL) {
			node->object.text = copy;
			ret = MLN_OK;
			*out = node;
			node = NULL;
			goto out;
		}
	}
	ret = MLN_ERROR_NO_MEMORY;
out:
	MlnFree(heap, node);
	return ret;
}

__attribute__((noinline)) int MlnXmlNodeRelease(int heap, MlnXmlNode **node)
{
	MlnXmlNode *own = *node;

	if (own == NULL)
		return MLN_OK;
	if (own->refCount >= 0 && own->refCount-- == 0) {
		switch (own->type) {
		case MLN_XML_NODE_DOCUMENT:
			MlnXmlDocumentReleaseInline(heap, &own->object.document);
			break;
		case MLN_XML_NODE_NAMESPACE:
		case MLN_XML_NODE_ATTRIBUTE:
			MlnXmlAttributeReleaseInline(heap, &own->object.attribute);
			break;
		case MLN_XML_NODE_ELEMENT:
			MlnXmlElementReleaseInline(heap, &own->object.element, 0);
			break;
		case MLN_XML_NODE_TEXT:
			MlnBufferDestroy(heap, own->object.text);
			break;
		}
		MlnFree(heap, own);
	}
	*node = NULL;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlNodeListCreate(int heap, MlnXmlNodeList **out, int count)
{
	MlnXmlNodeList *list;
	int i;

	if (count < 0)
		count = 0;
	list = MlnAllocate(heap, (count + 2) * sizeof(void *));
	if (list == NULL)
		return MLN_ERROR_NO_MEMORY;
	list->refCount = 0;
	list->count = count;
	for (i = 0; i < count; i++)
		list->nodes[i] = NULL;
	*out = list;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlNodeListRelease(int heap, MlnXmlNodeList **list)
{
	MlnXmlNodeList *own = *list;
	int i;

	if (own == NULL)
		return MLN_OK;
	if (own->refCount >= 0 && own->refCount-- == 0) {
		for (i = 0; i < own->count; i++)
			MlnXmlNodeRelease(heap, &own->nodes[i]);
		MlnFree(heap, own);
	}
	*list = NULL;
	return MLN_OK;
}

__attribute__((noinline)) int MlnXmlParserInit(int heap, MlnXmlParser *parser)
{
	(void)heap;
	MlnMemset(parser, 0, sizeof(*parser));
	return MLN_OK;
}
