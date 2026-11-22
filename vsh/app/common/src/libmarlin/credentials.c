/*
 * The device's credentials in the sealed cache: a buffer and two certificate chains packed
 * under one key, the assertion under another, and the stored XML reformatted
 * (0x81064238-). The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_CREDENTIALS_ERROR_NONE		0x230A0002	/* no credentials are stored */
#define MLN_CREDENTIALS_ERROR_FORMAT		0x230A0004	/* the packed credentials are malformed */
#define MLN_CREDENTIALS_ERROR_NO_ASSERTION	0x230A0006	/* no assertion is stored */
#define MLN_CREDENTIALS_ERROR_ASSERTION		0x230A0007	/* the stored assertion is malformed */
#define MLN_SAML_ERROR_FORMAT			0x230F0002

/* the header of the packed credentials, big-endian sizes */
typedef struct MlnCredentialsHeader {
	unsigned char version;			/* 1 */
	unsigned char reserved[3];
	unsigned short firstOffset;		/* the buffer's, from the end of the header */
	unsigned short firstSize;
	unsigned short sizes[3];		/* the buffer and the two chains, in their order */
	unsigned char padding[2];
} MlnCredentialsHeader;

/* free a list of certificates */
static inline void MlnCredentialsListFree(MlnX509Certificates *list)
{
	int i;

	for (i = 0; i < list->count; i++)
		MlnBufferDestroy(MLN_HEAP_GENERAL, list->items[i]);
	MlnFree(MLN_HEAP_GENERAL, list->items);
	MlnFree(MLN_HEAP_GENERAL, list);
}

/* an XML document written again as the XML layer writes it */
static inline int MlnCredentialsReformat(const MlnBuffer *xml, MlnBuffer **out)
{
	MlnXmlNode *document = NULL;
	MlnBuffer *text = NULL;
	int ret;

	ret = MlnXmlParse(MLN_HEAP_GENERAL, xml->size, xml->data, &document);
	if (ret == 0)
		ret = MlnXmlNodeToXml(MLN_HEAP_GENERAL, document, &text);
	if (ret == 0) {
		*out = text;
		text = NULL;
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, document);
	return ret;
}

/* store the credentials: the buffer and the chains packed, then the assertion */
int MlnImpl_81064238(const MlnBufferGroups *groups)
{
	MlnBuffer *value = NULL;
	int ret;

	if (groups == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_81064A18(groups->first, groups->lists[0].count, groups->lists[0].items,
			       groups->lists[1].count, groups->lists[1].items, &value);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8105DC0C(15, 0, value);
	if (ret != 0)
		goto out;
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	value = NULL;
	ret = MlnCredentialsReformat(groups->last, &value);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8105DC0C(17, 0, value);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	return ret;
}

/* load the credentials stored by MlnImpl_81064238 */
int MlnImpl_81064340(MlnBufferGroups **out)
{
	MlnBuffer *packed = NULL;
	MlnBuffer *first = NULL;
	MlnBuffer *second = NULL;
	MlnBufferGroups *groups;
	int ret;

	groups = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*groups));
	if (groups == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(groups, 0, sizeof(*groups));
	ret = MlnImpl_8105DEDC(15, 0, &packed);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_81064838(packed, &groups->first, &first, &second);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_81064728(first, &groups->lists[0].count, &groups->lists[0].items);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_81064728(second, &groups->lists[1].count, &groups->lists[1].items);
	if (ret != 0)
		goto fail;
	MlnBufferDestroy(MLN_HEAP_GENERAL, packed);
	packed = NULL;
	ret = MlnImpl_8105DEDC(17, 0, &packed);
	if (ret != 0)
		goto fail;
	groups->last = packed;
	packed = NULL;
	*out = groups;
	groups = NULL;
	goto out;

fail:
	if (ret == 1)
		ret = MLN_CREDENTIALS_ERROR_NONE;
	else if (ret != MLN_CREDENTIALS_ERROR_NONE && ret != 6 &&
		 ret != MLN_CREDENTIALS_ERROR_FORMAT)
		goto out;
	MlnImpl_810485B4();
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, second);
	second = NULL;
	MlnBufferDestroy(MLN_HEAP_GENERAL, first);
	first = NULL;
	MlnBufferDestroy(MLN_HEAP_GENERAL, packed);
	MlnImpl_81042A88(groups);
	return ret;
}

/* the stored assertion */
unsigned int MlnImpl_810644A0(MlnOctopusSamlAssertion **out)
{
	MlnBuffer *token = NULL;
	unsigned int ret;

	ret = MlnImpl_8105DEDC(17, 0, &token);
	if (ret == 0) {
		ret = MlnImpl_81042B18(token, 0, out, 0);
		if (ret == 0)
			goto out;
		if (ret == MLN_SAML_ERROR_FORMAT) {
			ret = MLN_CREDENTIALS_ERROR_ASSERTION;
			goto reset;
		}
	} else if (ret == 1) {
		ret = MLN_CREDENTIALS_ERROR_NO_ASSERTION;
		goto reset;
	}
	if (ret == MLN_CREDENTIALS_ERROR_NO_ASSERTION || ret == 6 ||
	    ret == MLN_CREDENTIALS_ERROR_ASSERTION)
reset:
		MlnImpl_810485B4();
out:
	if (token != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, token);
	return ret;
}

/* store an XML document under cache kind 3, reformatted */
int MlnImpl_8106455C(const MlnBuffer *xml)
{
	MlnBuffer *value = NULL;
	int ret;

	if (xml == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnCredentialsReformat(xml, &value);
	if (ret == 0)
		ret = MlnImpl_8105DC0C(3, 0, value);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	return ret;
}

/* a list of copies of count buffers, in the reverse order */
__attribute__((noinline)) int MlnImpl_81064614(int count, MlnBuffer **items, MlnX509Certificates **out)
{
	MlnX509Certificates *list;
	int i;

	if (count == 0 || items == NULL)
		return 3;
	list = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*list));
	if (list == NULL)
		return 2;
	list->count = count;
	list->items = MlnAllocate(MLN_HEAP_GENERAL, count * sizeof(MlnBuffer *));
	if (list->items != NULL) {
		for (i = list->count - 1; i >= 0; i--) {
			const MlnBuffer *item = items[list->count - 1 - i];

			list->items[i] = MlnBufferCreate(MLN_HEAP_GENERAL, item->size, item->data);
			if (list->items[i] == NULL)
				goto fail;
		}
		*out = list;
		return 0;
	}
fail:
	for (i = 0; i < list->count; i++)
		MlnBufferDestroy(MLN_HEAP_GENERAL, list->items[i]);
	MlnFree(MLN_HEAP_GENERAL, list);
	return 2;
}

/* the certificates of a DER chain, copied in the reverse order */
int MlnImpl_81064728(const MlnBuffer *der, int *count, MlnBuffer ***items)
{
	MlnAsn1Node *chain = NULL;
	MlnBuffer **list;
	int ret;
	int n;
	int i;

	ret = MlnImpl_81037918(MLN_HEAP_GENERAL, der->data, der->size, NULL, &chain);
	if (ret != 0)
		goto out;
	n = chain->length;
	if (n <= 0) {
		list = NULL;
	} else {
		list = MlnAllocate(MLN_HEAP_GENERAL, n * sizeof(MlnBuffer *));
		if (list == NULL) {
			ret = 2;
			goto out;
		}
		MlnMemset(list, 0, n * sizeof(MlnBuffer *));
		for (i = 0; i < n; i++) {
			const MlnAsn1Node *certificate = chain->children[i];

			list[n - 1 - i] = MlnBufferCreate(MLN_HEAP_GENERAL, certificate->encodingLength,
							  certificate->encoding);
			if (list[n - 1 - i] == NULL) {
				for (i = 0; i < n; i++)
					MlnBufferDestroy(MLN_HEAP_GENERAL, list[i]);
				MlnFree(MLN_HEAP_GENERAL, list);
				ret = 2;
				goto out;
			}
		}
	}
	*items = list;
	*count = n;
out:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, chain);
	return ret;
}

/* the buffer and the two DER chains of packed credentials */
int MlnImpl_81064838(const MlnBuffer *packed, MlnBuffer **first, MlnBuffer **second,
		     MlnBuffer **third)
{
	const unsigned char *data;
	const unsigned char *body;
	MlnBuffer *a = NULL;
	MlnBuffer *b = NULL;
	MlnBuffer *c;
	unsigned int offset;
	unsigned int size;
	unsigned int sizes[3];
	int ret;

	if (packed == NULL || packed->size == 0 || packed->data == NULL || first == NULL ||
	    second == NULL || third == NULL) {
		ret = 3;
		goto out;
	}
	data = packed->data;
	if (packed->size < sizeof(MlnCredentialsHeader)) {
		ret = MLN_CREDENTIALS_ERROR_FORMAT;
		goto out;
	}
	offset = data[4] << 8 | data[5];
	size = data[6] << 8 | data[7];
	sizes[0] = data[8] << 8 | data[9];
	sizes[1] = data[10] << 8 | data[11];
	sizes[2] = data[12] << 8 | data[13];
	if (offset + size > packed->size - sizeof(MlnCredentialsHeader)) {
		ret = MLN_CREDENTIALS_ERROR_FORMAT;
		goto out;
	}
	body = data + sizeof(MlnCredentialsHeader);
	a = MlnBufferCreate(MLN_HEAP_GENERAL, size, body + offset);
	if (a == NULL) {
		ret = 2;
		goto out;
	}
	if (sizes[0] + sizes[1] + sizes[2] != packed->size - sizeof(MlnCredentialsHeader)) {
		ret = MLN_CREDENTIALS_ERROR_FORMAT;
		goto out;
	}
	b = MlnBufferCreate(MLN_HEAP_GENERAL, sizes[1], body + sizes[0]);
	if (b == NULL) {
		ret = 2;
		goto out;
	}
	c = MlnBufferCreate(MLN_HEAP_GENERAL, sizes[2], body + sizes[0] + sizes[1]);
	if (c == NULL) {
		ret = 2;
		goto out;
	}
	*first = a;
	*second = b;
	b = NULL;
	*third = c;
	a = NULL;
	ret = 0;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, NULL);
	MlnBufferDestroy(MLN_HEAP_GENERAL, b);
	MlnBufferDestroy(MLN_HEAP_GENERAL, a);
	return ret;
}

/*
 * Pack the credentials: a header, the buffer and the two certificate lists, each encoded
 * as a DER chain.
 */
__attribute__((noinline)) int MlnImpl_81064A18(const MlnBuffer *first, int count1, MlnBuffer **items1, int count2,
		     MlnBuffer **items2, MlnBuffer **out)
{
	MlnBuffer *chain1 = NULL;
	MlnBuffer *chain2 = NULL;
	MlnX509Certificates *list = NULL;
	MlnCredentialsHeader header;
	MlnBuffer *packed = NULL;
	unsigned int size0;
	unsigned int size1;
	unsigned int size2;
	unsigned char *p;
	int ret;

	if (count1 == 0 || items1 == NULL || count2 == 0 || items2 == NULL || out == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_81064614(count1, items1, &list);
	if (ret != 0)
		goto free;
	ret = MlnImpl_8103A888(list, &chain1);
	if (ret != 0)
		goto free;
	MlnCredentialsListFree(list);
	list = NULL;
	ret = MlnImpl_81064614(count2, items2, &list);
	if (ret != 0)
		goto free;
	ret = MlnImpl_8103A888(list, &chain2);
	if (ret != 0)
		goto free;

	size0 = (unsigned short)first->size;
	size1 = (unsigned short)chain1->size;
	size2 = (unsigned short)chain2->size;
	header.version = 1;
	header.firstOffset = 0;
	header.firstSize = MlnHtons(size0);
	header.sizes[0] = MlnHtons(size0);
	header.sizes[1] = MlnHtons(size1);
	header.sizes[2] = MlnHtons(size2);
	MlnMemset(header.reserved, 0, sizeof(header.reserved));
	MlnMemset(header.padding, 0, sizeof(header.padding));
	packed = MlnBufferCreate(MLN_HEAP_GENERAL,
				 (unsigned short)(size0 + size1 + size2) + sizeof(header), NULL);
	if (packed == NULL) {
		ret = 2;
		goto free;
	}
	p = packed->data;
	MlnMemcpy(p, &header, sizeof(header));
	p += sizeof(header);
	MlnMemcpy(p, first->data, first->size);
	p += first->size;
	MlnMemcpy(p, chain1->data, chain1->size);
	p += chain1->size;
	MlnBufferDestroy(MLN_HEAP_GENERAL, chain1);
	chain1 = NULL;
	MlnMemcpy(p, chain2->data, chain2->size);
	MlnBufferDestroy(MLN_HEAP_GENERAL, chain2);
	chain2 = NULL;
	*out = packed;
	packed = NULL;
free:
	if (list != NULL)
		MlnCredentialsListFree(list);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, packed);
	MlnBufferDestroy(MLN_HEAP_GENERAL, chain2);
	MlnBufferDestroy(MLN_HEAP_GENERAL, chain1);
	return ret;
}
