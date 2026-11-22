/*
 * XML Encryption: an EncryptedData or EncryptedKey element's method, key and cipher value.
 */
#include "mln_internal.h"

#define MLN_NS_XMLENC		"http://www.w3.org/2001/04/xmlenc#"

/*
 * an encrypted element: its EncryptionMethod's Algorithm and OAEP parameters (an empty buffer
 * when it has none), and the key its KeyInfo gives: a KeyName, or an EncryptedKey, whose own
 * method becomes the key's and whose cipher value is the encrypted key
 */
int MlnImpl_81082230(MlnXmlNode *node, MlnBuffer **encryptionMethod, MlnBuffer **oaepParams,
		     MlnBuffer **keyName, MlnBuffer **keyEncryptionMethod, MlnBuffer **encryptedKey,
		     MlnBuffer **cipherValue)
{
	MlnXmlNodeList *list = NULL;
	MlnXmlNodeList *sub = NULL;
	MlnBuffer *method = NULL;
	MlnBuffer *oaep = NULL;
	MlnBuffer *oaepText = NULL;
	MlnBuffer *name = NULL;
	MlnBuffer *keyMethod = NULL;
	MlnBuffer *key = NULL;
	MlnBuffer *cipherText = NULL;
	MlnBuffer *cipher = NULL;
	MlnBuffer *innerKeyMethod = NULL;
	MlnBuffer *innerKey = NULL;
	MlnXmlNode *info;
	int ret;

	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "EncryptionMethod", MLN_NS_XMLENC, &list);
	if (ret != 0)
		goto out;
	if (list->count != 1)
		goto format;
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, list->nodes[0], NULL, "Algorithm", &method);
	if (ret != 0)
		goto out;
	if (MlnXmlFindNodes(MLN_HEAP_GENERAL, list->nodes[0], "OAEPparams", MLN_NS_XMLENC,
			    &sub) == 0) {
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, sub->nodes[0], &oaepText);
		if (ret != 0)
			goto out;
		ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, oaepText, &oaep);
		if (ret != 0)
			goto out;
		MlnXmlListFree(MLN_HEAP_GENERAL, sub);
		sub = NULL;
	}
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	list = NULL;

	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "KeyInfo", MLN_XMLDSIG_NAMESPACE, &list);
	if (ret != 0)
		goto out;
	if (list->count != 1)
		goto format;
	info = list->nodes[0];
	if (MlnXmlFindNodes(MLN_HEAP_GENERAL, info, "KeyName", MLN_XMLDSIG_NAMESPACE, &sub) == 0) {
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, sub->nodes[0], &name);
		if (ret != 0)
			goto out;
		MlnXmlListFree(MLN_HEAP_GENERAL, sub);
		sub = NULL;
	} else {
		if (MlnXmlFindNodes(MLN_HEAP_GENERAL, info, "EncryptedKey", MLN_NS_XMLENC,
				    &sub) != 0)
			goto format;
		keyMethod = method;
		method = NULL;
		ret = MlnImpl_81082230(sub->nodes[0], &method, &oaep, &name, &innerKeyMethod,
				       &innerKey, &key);
		if (ret != 0)
			goto out;
		if (innerKeyMethod != NULL || innerKey != NULL)
			goto format;
		MlnXmlListFree(MLN_HEAP_GENERAL, sub);
		sub = NULL;
	}
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	list = NULL;

	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "CipherData/CipherValue", MLN_NS_XMLENC,
			      &list);
	if (ret != 0)
		goto out;
	if (list->count != 1)
		goto format;
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, list->nodes[0], &cipherText);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, cipherText, &cipher);
	if (ret != 0)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	list = NULL;
	if (oaep == NULL)
		oaep = MlnBufferCreate(MLN_HEAP_GENERAL, 0, NULL);
	*encryptionMethod = method;
	method = NULL;
	*oaepParams = oaep;
	oaep = NULL;
	*keyName = name;
	name = NULL;
	*keyEncryptionMethod = keyMethod;
	keyMethod = NULL;
	*encryptedKey = key;
	key = NULL;
	*cipherValue = cipher;
	cipher = NULL;
	goto out;
format:
	ret = MLN_OCTOPUS_ERROR_FORMAT;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, method);
	MlnBufferDestroy(MLN_HEAP_GENERAL, oaep);
	MlnBufferDestroy(MLN_HEAP_GENERAL, oaepText);
	MlnBufferDestroy(MLN_HEAP_GENERAL, name);
	MlnBufferDestroy(MLN_HEAP_GENERAL, keyMethod);
	MlnBufferDestroy(MLN_HEAP_GENERAL, key);
	MlnBufferDestroy(MLN_HEAP_GENERAL, cipherText);
	MlnBufferDestroy(MLN_HEAP_GENERAL, cipher);
	MlnBufferDestroy(MLN_HEAP_GENERAL, innerKeyMethod);
	MlnBufferDestroy(MLN_HEAP_GENERAL, innerKey);
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	MlnXmlListFree(MLN_HEAP_GENERAL, sub);
	return ret;
}
