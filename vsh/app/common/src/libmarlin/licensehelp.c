/*
 * The license service's helpers: certificate paths in the order the verifier takes them,
 * the exchange clock, the stored keys, the XML of a request or a reply, the API's error
 * codes, and XPath expressions naming an object by its uid.
 */
#include "mln_internal.h"

/* a certificate chain, leaf first, verified as a path that starts at the anchor's child */
unsigned int MlnImpl_81082A60(MlnBuffer **certificates, int count, int kind)
{
	MlnX509Certificates path;
	unsigned short i;
	unsigned int ret;

	path.count = 0;
	path.items = NULL;
	if (certificates == NULL) {
		ret = 3;
		goto out;
	}
	path.count = count;
	path.items = MlnAllocate(MLN_HEAP_GENERAL, count * sizeof(*path.items));
	if (path.items == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	for (i = 0; i < path.count; i++)
		path.items[i] = certificates[path.count - i - 1];
	ret = MlnImpl_81057314(kind, &path);
out:
	MlnFree(MLN_HEAP_GENERAL, path.items);
	return ret;
}

/* sets the clock from the exchange's timestamp, when it gave one */
unsigned int MlnImpl_81082B04(void *exchange)
{
	int timestamp;
	unsigned int ret;

	ret = MlnImpl_810688CC(exchange, &timestamp);
	if (ret != 0)
		return ret == 0x23000008 ? 0 : ret;
	return MlnImpl_8105F334(timestamp - 1);
}

/* the stored key pair a reply's payload names */
unsigned int MlnImpl_81082B44(MlnXmlNode *document, MlnXmlNode **node)
{
	MlnArray *keys = NULL;
	MlnArray *pending = NULL;
	unsigned int ret;

	ret = MlnImpl_8104570C(&keys);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81068C90(document, keys, 5, node, &pending);
	if (ret != 0)
		goto out;
	ret = MlnImpl_810456B0((const MlnStoreKeyPair *)pending);
out:
	MlnArrayDestroy(MLN_HEAP_GENERAL, pending);
	MlnArrayDestroy(MLN_HEAP_GENERAL, keys);
	return ret;
}

/* the request header kept in the store, parsed */
unsigned int MlnImpl_81082BC4(MlnXmlNode **node)
{
	MlnXmlNode *document = NULL, *parsed;
	MlnBuffer *text = NULL;
	unsigned int ret;

	ret = MlnImpl_81043AE4(1, &text);
	if (ret != 0)
		goto fail;
	ret = MlnXmlParse(MLN_HEAP_GENERAL, text->size, text->data, &document);
	if (ret != 0)
		goto fail;
	parsed = document;
	document = NULL;
	*node = parsed;
	goto out;
fail:
	switch (ret) {
	case MLN_ERROR_NO_MEMORY:
		break;
	case 6:
		ret = 68;
		break;
	case 0x230A0002:
		ret = 16;
		break;
	case 0x230A0004:
		ret = 9;
		break;
	default:
		ret &= 0xffff0000;
		ret = ret == 0x10010000 || ret == 0x220A0000 ? 9 : 7;
		break;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, document);
	return ret;
}

/*
 * a request or reply given in the store's encrypted form, decrypted and parsed; when it does
 * not parse and the store cannot take it either, detail is the store's last error message
 */
unsigned int MlnImpl_81082CB4(const MlnBuffer *xml, MlnXmlNode **node, MlnBlob *detail)
{
	MlnXmlNode *document = NULL, *parsed;
	MlnBuffer *plain = NULL;
	MlnBuffer *message = NULL;
	unsigned int ret;

	ret = MlnImpl_810459F0(xml, &plain);
	switch (ret) {
	case 0:
		break;
	case MLN_ERROR_NO_MEMORY:
		goto out;
	case 6:
		ret = 68;
		goto out;
	case 0x230A0011:
		ret = 17;
		goto out;
	default:
		ret = 7;
		goto out;
	}
	ret = MlnXmlParse(MLN_HEAP_GENERAL, plain->size, plain->data, &document);
	if (ret == 0) {
		parsed = document;
		document = NULL;
		*node = parsed;
		goto out;
	}
	if (ret == MLN_ERROR_NO_MEMORY)
		goto out;
	if (MlnImpl_81045A3C(xml) != 0) {
		ret = 11;
		goto out;
	}
	MlnImpl_81043A60(1, &message);
	ret = 11;
	detail->buffer = message;
	detail->size = message->size;
	message = NULL;
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, document);
	MlnBufferDestroy(MLN_HEAP_GENERAL, plain);
	MlnBufferDestroy(MLN_HEAP_GENERAL, message);
	return ret;
}

/* the API's error code for one of the exchange or the store */
unsigned int MlnImpl_81082DC0(int ret)
{
	switch (ret) {
	case 0:
	case MLN_ERROR_NO_MEMORY:
		return ret;
	case 6:
		return 68;
	case 0x230A0002:
	case 0x23110005:
		return 15;
	case 0x230A0004:
	case 0x23110004:
		return 9;
	default:
		return 7;
	}
}

/* a certificate list in reverse order, as a path */
unsigned int MlnImpl_81082E44(int count, MlnBuffer **items, struct MlnX509Certificates *out)
{
	MlnBuffer **path, **end;
	int i;

	path = MlnAllocate(MLN_HEAP_GENERAL, count * sizeof(*path));
	if (path == NULL)
		return MLN_ERROR_NO_MEMORY;
	end = path + count;
	for (i = 0; i != count; i++)
		*--end = items[i];
	out->items = path;
	out->count = i;
	return 0;
}

/* prefix, the value and suffix as one string: an XPath that names an object by its uid */
int MlnImpl_81082EA4(const char *prefix, const MlnBuffer *value, const char *suffix, char **out)
{
	MlnTextBuffer *text = NULL;
	MlnBuffer *buffer = NULL;
	char *string = NULL, *made;
	int ret;

	if (prefix == NULL || suffix == NULL || value == NULL || value->data == NULL ||
	    value->size == 0) {
		ret = 3;
		goto out;
	}
	text = MlnTextBufferCreate(MLN_HEAP_GENERAL, 0, NULL);
	if (text == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	ret = MlnTextBufferAppend(MLN_HEAP_GENERAL, text, MlnStrlen(prefix), prefix);
	if (ret != 0)
		goto out;
	ret = MlnTextBufferAppend(MLN_HEAP_GENERAL, text, value->size, value->data);
	if (ret != 0)
		goto out;
	ret = MlnTextBufferAppend(MLN_HEAP_GENERAL, text, MlnStrlen(suffix), suffix);
	if (ret != 0)
		goto out;
	buffer = MlnBufferCreate(MLN_HEAP_GENERAL, text->size, text->data);
	if (buffer == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	ret = MlnBufferToString(MLN_HEAP_GENERAL, buffer, &string);
	if (ret == 0) {
		made = string;
		string = NULL;
		*out = made;
	}
out:
	MlnTextBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	MlnFree(MLN_HEAP_GENERAL, string);
	return ret;
}
