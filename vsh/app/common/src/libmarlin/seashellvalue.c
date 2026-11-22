/*
 * SceLibMarlin's SeaShell object values: an object's value replaced by a word, a string or
 * bytes, an ASN.1 node tagged [2], [3] or [4] in its second place (replacing the value there,
 * or appended after the object's name).
 */
#include "mln_internal.h"

/* an object's value set to a word, big-endian, tagged [2] */
int MlnImpl_81086B18(unsigned int value, MlnAsn1Node *object)
{
	unsigned char word[4];
	MlnAsn1Node *node = NULL;
	int ret;

	if (object == NULL) {
		ret = MLN_ERROR_INVALID;
		goto out;
	}
	word[1] = value >> 16;
	word[0] = value >> 24;
	word[2] = value >> 8;
	word[3] = value;
	ret = MlnImpl_81037D68(MLN_HEAP_GENERAL, 4, word, &node);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81038470(MLN_HEAP_GENERAL, 2, node);
	if (ret != 0)
		goto out;
	if (object->length > 1) {
		ret = MlnImpl_81037A80(MLN_HEAP_GENERAL, object, 1, node, NULL);
		if (ret == 0)
			node = NULL;
	} else {
		ret = MlnImpl_81037948(MLN_HEAP_GENERAL, object, node);
		if (ret == 0)
			node = NULL;
	}
out:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, node);
	return ret;
}

/* an object's value set to a string, tagged [3] */
int MlnImpl_81086BE4(const unsigned char *data, unsigned int size, MlnAsn1Node *object)
{
	MlnAsn1Node *node = NULL;
	int ret;

	if (data == NULL || object == NULL) {
		ret = MLN_ERROR_INVALID;
		goto out;
	}
	ret = MlnImpl_81037D68(MLN_HEAP_GENERAL, size, data, &node);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81038470(MLN_HEAP_GENERAL, 3, node);
	if (ret != 0)
		goto out;
	if (object->length > 1) {
		ret = MlnImpl_81037A80(MLN_HEAP_GENERAL, object, 1, node, NULL);
		if (ret == 0)
			node = NULL;
	} else {
		ret = MlnImpl_81037948(MLN_HEAP_GENERAL, object, node);
		if (ret == 0)
			node = NULL;
	}
out:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, node);
	return ret;
}

/* an object's value set to bytes, tagged [4] */
int MlnImpl_81086C9C(const unsigned char *data, unsigned int size, MlnAsn1Node *object)
{
	MlnAsn1Node *node = NULL;
	int ret;

	if (data == NULL || object == NULL) {
		ret = MLN_ERROR_INVALID;
		goto out;
	}
	ret = MlnImpl_81037D68(MLN_HEAP_GENERAL, size, data, &node);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81038470(MLN_HEAP_GENERAL, 4, node);
	if (ret != 0)
		goto out;
	if (object->length > 1) {
		ret = MlnImpl_81037A80(MLN_HEAP_GENERAL, object, 1, node, NULL);
		if (ret == 0)
			node = NULL;
	} else {
		ret = MlnImpl_81037948(MLN_HEAP_GENERAL, object, node);
		if (ret == 0)
			node = NULL;
	}
out:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, node);
	return ret;
}
