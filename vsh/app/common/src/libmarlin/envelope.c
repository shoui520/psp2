/*
 * XML-DSIG's enveloped-signature transform: a document written without the Signature
 * element that holds the transform; and a certificate chain split into its certificates
 * (0x81058E54-). The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_ENVELOPE_ERROR_FORMAT	0x220A0004

/* what a search below an element finds */
#define MLN_ENVELOPE_NOT_FOUND		0
#define MLN_ENVELOPE_TARGET		1	/* the target, not yet inside a Signature */
#define MLN_ENVELOPE_SIGNATURE		2	/* the Signature element enclosing the target */

/* the certificates of a chain while they are parsed */
typedef struct MlnEnvelopeChain {
	int count;
	MlnBuffer **items;
} MlnEnvelopeChain;

/*
 * Search an element's tree for the target: the innermost Signature element (of any
 * namespace prefix) that encloses it is returned in signature.
 */
int MlnImpl_81058E54(const MlnXmlElement *element, const MlnXmlElement *target,
		     const MlnXmlElement **signature)
{
	const MlnBuffer *name;
	const MlnXmlContentArray *content;
	const MlnXmlContent *item;
	unsigned int start;
	int isSignature;
	int ret;
	int i;

	if (element == target)
		return MLN_ENVELOPE_TARGET;

	/* the local part of the element's name */
	name = element->name;
	for (start = 0; start < name->size; start++) {
		if (name->data[start] == ':') {
			start++;
			break;
		}
	}
	if (start == name->size)
		start = 0;
	isSignature = MlnMemcmp(name->data + start, "Signature", name->size - start) == 0;

	content = element->content;
	for (i = 0; i < content->count; i++) {
		item = content->items[i];
		if (item->kind != MLN_XML_CONTENT_ELEMENT)
			continue;
		ret = MlnImpl_81058E54(item->data.element, target, signature);
		if (ret == MLN_ENVELOPE_TARGET && isSignature) {
			*signature = element;
			return MLN_ENVELOPE_SIGNATURE;
		}
		if (ret != MLN_ENVELOPE_NOT_FOUND)
			return ret;
	}
	return MLN_ENVELOPE_NOT_FOUND;
}

/*
 * Apply the enveloped-signature transform: the document or element written as XML without
 * the Signature element that holds the transform.
 */
int MlnImpl_81059558(MlnXmlNode *node, MlnXmlNode *transform, MlnBuffer **out)
{
	const MlnXmlElement *root;
	MlnXmlParser writer;
	const MlnXmlElement *signature;
	int ret;

	switch (node->type) {
	case MLN_XML_NODE_DOCUMENT:
		root = node->object.document->root;
		break;
	case MLN_XML_NODE_ELEMENT:
		root = node->object.element;
		break;
	default:
		return MLN_ENVELOPE_ERROR_FORMAT;
	}
	if (transform->type != MLN_XML_NODE_ELEMENT)
		return MLN_ENVELOPE_ERROR_FORMAT;
	signature = NULL;
	if (MlnImpl_81058E54(root, transform->object.element, &signature) != MLN_ENVELOPE_SIGNATURE)
		return MLN_ENVELOPE_ERROR_FORMAT;

	MlnXmlParserInit(MLN_HEAP_GENERAL, &writer);
	ret = MlnXmlWriteNode(MLN_HEAP_GENERAL, &writer, node, signature, out);
	MlnXmlParserTerm(MLN_HEAP_GENERAL, &writer);
	return ret;
}

/* free a chain and its first count certificates */
static inline void MlnEnvelopeChainFree(MlnEnvelopeChain *chain, unsigned int count)
{
	unsigned int i;

	if (chain->items != NULL) {
		for (i = 0; i < count; i++)
			MlnBufferDestroy(MLN_HEAP_GENERAL, chain->items[i]);
		MlnFree(MLN_HEAP_GENERAL, chain->items);
	}
	MlnFree(MLN_HEAP_GENERAL, chain);
}

/* the certificates of a DER SEQUENCE, in their order */
static inline unsigned int MlnEnvelopeChainParse(const MlnBuffer *der, MlnEnvelopeChain **out)
{
	MlnEnvelopeChain *list;
	const unsigned char *contents;
	const unsigned char *start;
	const unsigned char *p;
	int length;
	int total;
	int remaining;
	unsigned int encodingSize;
	unsigned int n;
	unsigned int ret;
	int i;

	ret = MlnImpl_81037660(MLN_HEAP_GENERAL, der->data, der->size, &contents, &length,
			       &encodingSize);
	if (ret != 0)
		return ret;

	/* count the certificates */
	total = length;
	start = contents;
	n = 0;
	for (remaining = total, p = start; remaining > 0; remaining -= encodingSize,
	     p += encodingSize) {
		ret = MlnImpl_81037660(MLN_HEAP_GENERAL, p, remaining, &contents, &length,
				       &encodingSize);
		n++;
		if (ret != 0)
			return ret;
	}

	list = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*list));
	if (list == NULL)
		return 2;
	list->count = 0;
	list->items = MlnAllocate(MLN_HEAP_GENERAL, n * sizeof(MlnBuffer *));
	if (list->items == NULL) {
		MlnEnvelopeChainFree(list, 0);
		return 2;
	}
	list->count = n;
	if (n != 0) {
		remaining = total;
		p = start;
		i = 0;
		do {
			ret = MlnImpl_81037660(MLN_HEAP_GENERAL, p, remaining, &contents, &length,
					       &encodingSize);
			if (ret != 0) {
				MlnEnvelopeChainFree(list, i);
				return ret;
			}
			if ((list->items[i] = MlnBufferCreate(MLN_HEAP_GENERAL, encodingSize,
							      p)) == NULL) {
				MlnEnvelopeChainFree(list, i);
				return 2;
			}
			i++;
			remaining -= encodingSize;
			p += encodingSize;
		} while (i < list->count);
	}
	*out = list;
	return 0;
}

/*
 * Split a certificate chain, a base64 DER SEQUENCE of certificates, into its certificates,
 * in the reverse order (the root first).
 */
unsigned int MlnImpl_81059600(const MlnBuffer *chain, MlnBuffer ***certificates, int *count)
{
	MlnBuffer *decoded = NULL;
	MlnEnvelopeChain *list = NULL;
	MlnBuffer **result;
	unsigned int ret;
	int size;
	int i;

	if (certificates == NULL || count == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, chain, &decoded);
	if (ret != 0)
		goto out;
	ret = MlnEnvelopeChainParse(decoded, &list);
	if (ret != 0)
		goto out;

	size = list->count;
	result = MlnAllocate(MLN_HEAP_GENERAL, size * sizeof(MlnBuffer *));
	if (result == NULL) {
		ret = 2;
	} else {
		for (i = 0; i < size; i++) {
			result[size - 1 - i] = list->items[i];
			list->items[i] = NULL;
		}
		list->count = 0;
		*certificates = result;
		*count = size;
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, decoded);
	if (list->items != NULL) {
		for (i = 0; i < list->count; i++)
			MlnBufferDestroy(MLN_HEAP_GENERAL, list->items[i]);
		MlnFree(MLN_HEAP_GENERAL, list->items);
	}
	MlnFree(MLN_HEAP_GENERAL, list);
	return ret;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, decoded);
	return ret;
}
