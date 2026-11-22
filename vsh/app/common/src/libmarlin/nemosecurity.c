/*
 * The security of a NEMO message read and written: its fault, the keys its Security header
 * gives, its encrypted parts, digests, certificates and the nodes of its header.
 */
#include "mln_internal.h"

/* the message's fault: its exceptions are kept when the fault is the client's */
unsigned int MlnImpl_81083F90(MlnXmlNode *body, MlnXmlNode *header, MlnNemoMessage *message)
{
	MlnXmlNode *fault = NULL;
	MlnXmlNode *step = NULL;
	MlnNemoFaults *faults = NULL;
	MlnBuffer *name = NULL;
	MlnBuffer *client = NULL;
	unsigned int prefix, i;
	unsigned int ret;

	if (MlnXmlFindNode(MLN_HEAP_GENERAL, body, "Fault", MLN_NEMO_NS_SOAP, &fault) != 0) {
		ret = 0;
		goto out;
	}
	if (header == NULL ||
	    MlnXmlFindNode(MLN_HEAP_GENERAL, header,
			   "Security/ProtocolDeclaration/Step[@Type='fault']", NULL, &step) != 0)
		ret = MlnImpl_81079594(fault, NULL, &faults);
	else
		ret = MlnImpl_81079594(fault, header, &faults);
	if (ret != 0) {
		if (ret == MLN_XML_ERROR_NOT_FOUND)
			ret = 0;
		goto out;
	}
	ret = MlnXmlNodeGetName(MLN_HEAP_GENERAL, body, &name);
	if (ret != 0)
		goto out;
	/* the fault code the client is given: "Client" with the body's prefix */
	i = 0;
	do {
		if (i == name->size) {
			i = 0;
			break;
		}
	} while (name->data[i++] != ':');
	prefix = i;
	client = MlnBufferCreate(MLN_HEAP_GENERAL, prefix + 6, NULL);
	MlnMemcpy(client->data, name->data, prefix);
	MlnMemcpy(client->data + prefix, "Client", 6);
	if (MlnBytesCompare(MLN_HEAP_GENERAL, faults->code->size, faults->code->data, prefix + 6,
			    client->data) == 0) {
		ret = MLN_NEMO_ERROR_FAULT;
		message->faults = faults;
		faults = NULL;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, name);
	MlnBufferDestroy(MLN_HEAP_GENERAL, client);
	MlnImpl_810794DC(faults);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, fault);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, step);
	return ret;
}

/* a same-document reference: "#" and the id */
__attribute__((noinline)) unsigned int MlnImpl_8108418C(const char *string, char **copy)
{
	char *uri = NULL;
	int length;
	unsigned int ret;

	if (string == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	length = MlnStrlen(string);
	if (length <= 0) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	uri = MlnAllocate(MLN_HEAP_GENERAL, length + 2);
	if (uri == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	uri[0] = '#';
	MlnMemcpy(uri + 1, string, length + 1);
	*copy = uri;
	uri = NULL;
	ret = 0;
out:
	MlnFree(MLN_HEAP_GENERAL, uri);
	return ret;
}

/*
 * an EncryptedKey's EncryptionMethod: RSA-OAEP with MGF1, its OAEPparams (empty when not
 * given) and, when given, a SHA-1 DigestMethod; *oaep stays NULL without a method
 */
static inline __attribute__((always_inline)) unsigned int
MlnNemoReadEncryptionMethod(MlnXmlNode *encryptedKey, MlnBuffer **oaep)
{
	MlnXmlNode *method = NULL;
	MlnXmlNode *params = NULL;
	MlnXmlNode *digest = NULL;
	MlnBuffer *algorithm = NULL;
	MlnBuffer *text = NULL;
	MlnBuffer *value = NULL;
	MlnBuffer *digestAlgorithm = NULL;
	unsigned int ret;

	if (encryptedKey == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, encryptedKey, "EncryptionMethod", MLN_NEMO_NS_XENC,
			     &method);
	if (ret != 0) {
		if (ret == MLN_XML_ERROR_NOT_FOUND)
			ret = 0;
		goto out;
	}
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, method, NULL, "Algorithm", &algorithm);
	if (ret != 0)
		goto out;
	if (MlnBytesCompare(MLN_HEAP_GENERAL,
			    MlnStrlen("http://www.w3.org/2001/04/xmlenc#rsa-oaep-mgf1p"),
			    "http://www.w3.org/2001/04/xmlenc#rsa-oaep-mgf1p", algorithm->size,
			    algorithm->data) != 0) {
		ret = MLN_NEMO_ERROR_FORMAT;
		goto out;
	}
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, method, "OAEPparams", MLN_NEMO_NS_XENC, &params);
	if (ret == 0) {
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, params, &text);
		if (ret != 0)
			goto out;
		ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &value);
		if (ret != 0)
			goto out;
	} else {
		if (ret != MLN_XML_ERROR_NOT_FOUND)
			goto out;
		value = MlnBufferCreate(MLN_HEAP_GENERAL, MlnStrlen(""), "");
		if (value == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto out;
		}
	}
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, method, "DigestMethod", MLN_NEMO_NS_DSIG, &digest);
	if (ret == 0) {
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, digest, NULL, "Algorithm",
					 &digestAlgorithm);
		if (ret != 0)
			goto out;
		if (MlnBytesCompare(MLN_HEAP_GENERAL,
				    MlnStrlen("http://www.w3.org/2000/09/xmldsig#sha1"),
				    "http://www.w3.org/2000/09/xmldsig#sha1", digestAlgorithm->size,
				    digestAlgorithm->data) != 0) {
			ret = MLN_NEMO_ERROR_FORMAT;
			goto out;
		}
	} else if (ret != MLN_XML_ERROR_NOT_FOUND) {
		goto out;
	}
	*oaep = value;
	value = NULL;
	ret = 0;
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, method);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, params);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, digest);
	MlnBufferDestroy(MLN_HEAP_GENERAL, algorithm);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	MlnBufferDestroy(MLN_HEAP_GENERAL, digestAlgorithm);
	return ret;
}

/*
 * the message key of the Security header's EncryptedKey, at most one: its key identifier
 * must be the one of the device's certificate, the key is unwrapped by the HCI, and its
 * DataReferences (one to three) name the parts it opens
 */
unsigned int MlnImpl_81084204(MlnXmlNode *security, MlnNemoMessage *message,
			      MlnNemoMessageKey **keys, int *count)
{
	MlnXmlNodeList *list = NULL;
	MlnXmlNodeList *references = NULL;
	MlnXmlNode *keyInfo = NULL;
	MlnXmlNode *identifier = NULL;
	MlnXmlNode *cipher = NULL;
	MlnBuffer *text = NULL;
	MlnBuffer *decoded = NULL;
	MlnBuffer *oaep = NULL;
	MlnX509 *x509 = NULL;
	MlnNemoMessageKey *made = NULL;
	MlnXmlNode *encryptedKey;
	void *wrapping;
	unsigned int ret, i;
	int n = 0, m;

	if (security == NULL || message == NULL || count == NULL ||
	    message->credentials == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, security, "EncryptedKey", MLN_NEMO_NS_XENC, &list);
	if (ret != 0) {
		if (ret != MLN_XML_ERROR_NOT_FOUND)
			goto out;
		goto done;
	}
	n = list->count;
	if (n > 1) {
		ret = MLN_NEMO_ERROR_FORMAT;
		goto out;
	}
	made = MlnAllocate(MLN_HEAP_GENERAL, n * sizeof(*made));
	if (made == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	if (n != 1)
		goto done;
	made[0].key = NULL;
	made[0].ids = NULL;
	made[0].idCount = 0;
	encryptedKey = list->nodes[0];
	ret = MlnNemoReadEncryptionMethod(encryptedKey, &oaep);
	if (ret != 0)
		goto out;
	if (oaep == NULL) {
		ret = MLN_NEMO_ERROR_FORMAT;
		goto out;
	}

	/* the key identifier of the certificate it is encrypted for */
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, encryptedKey, "KeyInfo", MLN_NEMO_NS_DSIG, &keyInfo);
	if (ret != 0)
		goto out;
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, keyInfo, "SecurityTokenReference/KeyIdentifier",
			     MLN_NEMO_NS_SECEXT, &identifier);
	if (ret != 0)
		goto out;
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, identifier, &text);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &decoded);
	if (ret != 0)
		goto out;
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	text = NULL;
	ret = MlnImpl_81038EF4(*message->credentials->lists[1].items, &x509);
	if (ret != 0)
		goto out;
	ret = MlnImpl_810390C0(x509, &text);
	if (ret != 0)
		goto out;
	if (MlnBufferCompare(MLN_HEAP_GENERAL, text, decoded) != 0) {
		ret = MLN_NEMO_ERROR_FORMAT;
		goto out;
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	text = NULL;
	MlnBufferDestroy(MLN_HEAP_GENERAL, decoded);
	decoded = NULL;

	/* the key, unwrapped */
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, encryptedKey, "CipherData/CipherValue",
			     MLN_NEMO_NS_XENC, &cipher);
	if (ret != 0)
		goto out;
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, cipher, &text);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &decoded);
	if (ret != 0)
		goto out;
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	text = NULL;
	wrapping = NULL;
	ret = MlnImpl_81080C3C(0, &wrapping);
	if (ret == 0)
		ret = MlnImpl_81080E80(wrapping, 1, oaep, decoded, 8, &made[0].key);
	MlnImpl_81080C80(wrapping);
	if (ret != 0)
		goto out;

	/* the parts it opens */
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, encryptedKey, "ReferenceList/DataReference",
			      MLN_NEMO_NS_XENC, &references);
	if (ret != 0)
		goto out;
	m = references->count;
	if ((unsigned int)(m - 1) > 2) {
		ret = MLN_NEMO_ERROR_FORMAT;
		goto out;
	}
	made[0].ids = MlnAllocate(MLN_HEAP_GENERAL, m * sizeof(*made[0].ids));
	if (made[0].ids == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemset(made[0].ids, 0, m * sizeof(*made[0].ids));
	made[0].idCount = m;
	for (i = 0; i < made[0].idCount; i++) {
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, references->nodes[i], NULL, "URI", &text);
		if (ret != 0)
			goto out;
		made[0].ids[i] = MlnBufferCreate(MLN_HEAP_GENERAL, text->size - 1, text->data + 1);
		if (made[0].ids[i] == NULL) {
			ret = MLN_ERROR_NO_MEMORY;
			goto out;
		}
		MlnBufferDestroy(MLN_HEAP_GENERAL, text);
		text = NULL;
	}
	MlnXmlNodeFree(MLN_HEAP_GENERAL, keyInfo);
	keyInfo = NULL;
	MlnXmlNodeFree(MLN_HEAP_GENERAL, identifier);
	identifier = NULL;
	MlnXmlNodeFree(MLN_HEAP_GENERAL, cipher);
	cipher = NULL;
	MlnXmlListFree(MLN_HEAP_GENERAL, references);
	references = NULL;
done:
	*count = n;
	*keys = made;
	made = NULL;
	ret = 0;
out:
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	MlnXmlListFree(MLN_HEAP_GENERAL, references);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, keyInfo);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, identifier);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, cipher);
	MlnNemoMessageKeysFree(made, n);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnBufferDestroy(MLN_HEAP_GENERAL, decoded);
	MlnBufferDestroy(MLN_HEAP_GENERAL, oaep);
	MlnImpl_81038E90(x509);
	return ret;
}

/*
 * the encrypted parts of a message read, each replaced by its AES-128-CBC plaintext under
 * the message key whose DataReferences name its Id
 */
unsigned int MlnImpl_810849E0(MlnXmlNodeList *encrypted, MlnXmlNode *parent,
			      MlnNemoMessageKey *keys, int count)
{
	MlnBuffer *text = NULL;
	MlnBuffer *cipher = NULL;
	MlnBuffer *algorithm = NULL;
	MlnXmlNodeList *values = NULL;
	MlnXmlNodeList *methods = NULL;
	MlnXmlNode *node;
	int found = 0, total, i, k;
	unsigned int ret, j, length;

	total = encrypted->count;
	for (i = 0; i < total; i++) {
		node = encrypted->nodes[i];
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "Id", &text);
		if (ret != 0)
			goto out;
		/* the key is the last one that names it */
		for (k = 0; k != count; k++)
			for (j = 0; j < keys[k].idCount; j++)
				if (keys[k].ids[j] != NULL &&
				    MlnBufferCompare(MLN_HEAP_GENERAL, keys[k].ids[j], text) == 0) {
					found = k;
					break;
				}
		if (found == count)
			goto format;
		MlnBufferDestroy(MLN_HEAP_GENERAL, text);
		text = NULL;
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "EncryptionMethod", MLN_NEMO_NS_XENC,
				      &methods);
		if (ret == 0) {
			if (methods->count != 1)
				goto format;
			ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, methods->nodes[0], NULL,
						 "Algorithm", &algorithm);
			if (ret != 0)
				goto out;
			length = MlnStrlen("http://www.w3.org/2001/04/xmlenc#aes128-cbc");
			if (length != algorithm->size ||
			    MlnMemcmp("http://www.w3.org/2001/04/xmlenc#aes128-cbc", algorithm->data,
				      length) != 0)
				goto format;
		} else if (ret != MLN_XML_ERROR_NOT_FOUND) {
			goto out;
		}
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "CipherData/CipherValue",
				      MLN_NEMO_NS_XENC, &values);
		if (ret != 0)
			goto out;
		if (values->count != 1)
			goto format;
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, values->nodes[0], &text);
		if (ret != 0)
			goto out;
		ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &cipher);
		if (ret != 0)
			goto out;
		MlnBufferDestroy(MLN_HEAP_GENERAL, text);
		text = NULL;
		if (cipher->size <= 16)
			goto format;
		ret = MlnImpl_81080D18(keys[found].key, 2, 1, NULL, NULL, cipher, &text);
		if (ret != 0)
			goto out;
		ret = MlnXmlNodeReplaceChildXml(MLN_HEAP_GENERAL, text->size, text->data, node, parent);
		if (ret != 0)
			goto out;
		MlnBufferDestroy(MLN_HEAP_GENERAL, text);
		text = NULL;
		MlnXmlListFree(MLN_HEAP_GENERAL, methods);
		methods = NULL;
		MlnBufferDestroy(MLN_HEAP_GENERAL, algorithm);
		algorithm = NULL;
		MlnXmlListFree(MLN_HEAP_GENERAL, values);
		values = NULL;
		MlnBufferDestroy(MLN_HEAP_GENERAL, cipher);
		cipher = NULL;
	}
	ret = 0;
	goto out;
format:
	ret = MLN_NEMO_ERROR_FORMAT;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnBufferDestroy(MLN_HEAP_GENERAL, cipher);
	MlnBufferDestroy(MLN_HEAP_GENERAL, algorithm);
	MlnXmlListFree(MLN_HEAP_GENERAL, values);
	MlnXmlListFree(MLN_HEAP_GENERAL, methods);
	return ret;
}

/* the digest of a part: SHA-1 or SHA-256 by its size, nothing for 0 */
__attribute__((noinline)) unsigned int MlnImpl_81084D14(const void *data, unsigned int size, unsigned char *digest,
			      int digestSize)
{
	if (data == NULL)
		return 5;
	switch (digestSize) {
	case 20:
		return MlnImpl_810570A8(data, size, digest);
	case 32:
		return MlnImpl_8105713C(data, size, digest);
	case 0:
		return 0;
	default:
		return 5;
	}
}

/* a certificate chain, leaf first, verified as a path from the anchor's child */
static inline __attribute__((always_inline)) unsigned int
MlnNemoVerifyChain(MlnBuffer **certificates, int count, int purpose)
{
	MlnX509Certificates path;
	unsigned short i;
	unsigned int ret;

	path.count = 0;
	path.items = NULL;
	if (certificates == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
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
	ret = MlnImpl_81057314(purpose, &path);
out:
	MlnFree(MLN_HEAP_GENERAL, path.items);
	return ret;
}

/*
 * a BinarySecurityToken of the value type given: its certificate chain, verified for the
 * purpose, and the leaf; nothing when the token is of another type
 */
unsigned int MlnImpl_81084D58(const MlnBuffer *valueType, const char *type, MlnXmlNode *token,
			      int purpose, MlnBuffer **leaf, MlnBuffer ***certificates, int *count)
{
	MlnBuffer *made = NULL;
	MlnBuffer *text = NULL;
	MlnBuffer **chain = NULL;
	int length = 0;
	unsigned int ret;

	if (MlnBytesCompare(MLN_HEAP_GENERAL, valueType->size, valueType->data, MlnStrlen(type),
			    type) != 0) {
		ret = 0;
		goto give;
	}
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, token, &text);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81059600(text, &chain, &length);
	if (ret != 0)
		goto out;
	ret = MlnNemoVerifyChain(chain, length, purpose);
	if (ret != 0)
		goto out;
	if (length <= 0) {
		ret = MLN_NEMO_ERROR_FORMAT;
		goto out;
	}
	made = MlnBufferCreate(MLN_HEAP_GENERAL, chain[0]->size, chain[0]->data);
	if (made == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
give:
	*leaf = made;
	made = NULL;
	*certificates = chain;
	chain = NULL;
	*count = length;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, made);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnNemoCertificatesFree(chain, length);
	return ret;
}

/* whether a certificate's subject is the name the service was given */
unsigned int MlnImpl_81084F3C(const MlnBuffer *name, const MlnBuffer *certificate)
{
	MlnBuffer *subject = NULL;
	MlnX509 *x509 = NULL;
	unsigned int ret;

	if (name == NULL || certificate == NULL)
		goto parameter;
	ret = MlnImpl_81038EF4(certificate, &x509);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81039298(x509, &subject);
	if (ret != 0)
		goto out;
	if (MlnBufferCompare(MLN_HEAP_GENERAL, name, subject) != 0)
		goto parameter;
	goto out;
parameter:
	ret = MLN_NEMO_ERROR_PARAMETER;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, subject);
	MlnImpl_81038E90(x509);
	return ret;
}

/* a peer's key: its certificate, the base64 subject key identifier and the RSA public key */
unsigned int MlnImpl_81084FC8(const MlnBuffer *certificate, MlnNemoPeerKey **out)
{
	MlnNemoPeerKey *key = NULL;
	MlnBuffer *keyId = NULL;
	MlnX509 *x509 = NULL;
	unsigned int ret;

	if (certificate == NULL || out == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	key = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*key));
	if (key == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemset(key, 0, sizeof(*key));
	key->certificate = MlnBufferCreate(MLN_HEAP_GENERAL, certificate->size, certificate->data);
	if (key->certificate == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	ret = MlnImpl_81038EF4(certificate, &x509);
	if (ret != 0)
		goto out;
	ret = MlnImpl_810390C0(x509, &keyId);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81026814(MLN_HEAP_GENERAL, keyId, 0, 0, &key->keyId);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81039228(x509, &key->modulus, &key->exponent);
	if (ret == 0) {
		*out = key;
		key = NULL;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, keyId);
	MlnImpl_81079A2C(key);
	MlnImpl_81038E90(x509);
	return ret;
}

/* a Nonce of the Security header: the nonce in base64 lines, for its usage */
unsigned int MlnImpl_810850D4(const MlnBuffer *nonce, const char *usage, MlnXmlNode **out)
{
	MlnXmlBuildAttribute attributes[3];
	MlnXmlBuildContent content;
	MlnXmlNode *node = NULL, *made;
	MlnBuffer *encoded = NULL;
	unsigned int ret;

	if (nonce == NULL || nonce->size > 128 || usage == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	ret = MlnImpl_81026814(MLN_HEAP_GENERAL, nonce, 72, 0, &encoded);
	if (ret != 0)
		goto out;
	content.kind = MLN_XML_BUILD_BUFFER;
	content.value.buffer = encoded;
	attributes[0].name = "EncodingType";
	attributes[0].value = MLN_NEMO_BASE64;
	attributes[1].name = "nemosec:Usage";
	attributes[1].value = usage;
	attributes[2].name = "xmlns:nemosec";
	attributes[2].value = MLN_NEMO_NS_SECURITY;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Nonce", "wsse", MLN_NEMO_NS_SECEXT, 3,
				 attributes, 1, &content, 1, &node);
	if (ret == 0) {
		made = node;
		encoded = NULL;
		node = NULL;
		*out = made;
	}
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, node);
	MlnBufferDestroy(MLN_HEAP_GENERAL, encoded);
	return ret;
}

/* a SecurityTokenReference to a SAML assertion by its id, for its usage */
unsigned int MlnImpl_81085204(const MlnBuffer *id, const char *usage, MlnXmlNode **out)
{
	MlnXmlBuildAttribute type, use;
	MlnXmlBuildContent idContent, keyContent;
	MlnXmlNode *reference = NULL, *made;
	MlnXmlNode *identifier = NULL;
	unsigned int ret;

	if (id == NULL || usage == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	type.name = "ValueType";
	type.value = "http://docs.oasis-open.org/wss/oasis-wss-saml-token-profile-1.0#SAMLAssertionID";
	idContent.kind = MLN_XML_BUILD_BUFFER;
	idContent.value.buffer = (MlnBuffer *)id;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "KeyIdentifier", "wsse", MLN_NEMO_NS_SECEXT, 1,
				 &type, 1, &idContent, 0, &identifier);
	if (ret != 0)
		goto out;
	keyContent.kind = MLN_XML_BUILD_NODE;
	keyContent.value.node = identifier;
	use.name = "nemosec:Usage";
	use.value = usage;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "SecurityTokenReference", "wsse",
				 MLN_NEMO_NS_SECEXT, 1, &use, 1, &keyContent, 0, &reference);
	if (ret == 0) {
		made = reference;
		reference = NULL;
		*out = made;
	}
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, identifier);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, reference);
	return ret;
}

/*
 * a BinarySecurityToken of a certificate chain as an X509PKIPathv1, root first: the signer's
 * (chain 0) for its step, or the encryption key's of a response (chain 1)
 */
unsigned int MlnImpl_81085338(int chain, int count, MlnBuffer **certificates, unsigned int step,
			      MlnXmlNode **out)
{
	MlnXmlBuildAttribute attributes[4];
	MlnXmlBuildContent content;
	MlnX509Certificates *path = NULL;
	MlnXmlNode *node = NULL, *made;
	MlnBuffer *encoded = NULL;
	MlnBuffer *text = NULL;
	int i;
	unsigned int ret;

	attributes[0].name = "ValueType";
	attributes[0].value =
		"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-x509-token-profile-1.0#X509PKIPathv1";
	attributes[1].name = "EncodingType";
	attributes[1].value = MLN_NEMO_BASE64;
	attributes[2].name = "xmlns:nemosec";
	attributes[2].value = MLN_NEMO_NS_SECURITY;
	attributes[3].name = "nemosec:Usage";
	if (chain == 0) {
		if (step == MLN_NEMO_STEP_REQUEST)
			attributes[3].value = MLN_NEMO_USAGE("request-signingKey");
		else if (step == MLN_NEMO_STEP_CONFIRMATION)
			attributes[3].value = MLN_NEMO_USAGE("confirmation-signingKey");
		else
			attributes[3].value = MLN_NEMO_USAGE("fault-signingKey");
	} else if (chain == 1) {
		attributes[3].value = MLN_NEMO_USAGE("response-encryptionKey");
	} else {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	if (count == 0) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	path = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*path));
	if (path == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	path->count = count;
	path->items = MlnAllocate(MLN_HEAP_GENERAL, count * sizeof(*path->items));
	if (path->items == NULL)
		goto fail;
	for (i = path->count - 1; i >= 0; i--)
		path->items[i] = NULL;
	for (i = path->count - 1; i >= 0; i--) {
		path->items[i] = MlnBufferCreate(MLN_HEAP_GENERAL,
						 certificates[path->count - 1 - i]->size,
						 certificates[path->count - 1 - i]->data);
		if (path->items[i] == NULL)
			goto fail;
	}
	ret = MlnImpl_8103A888(path, &encoded);
	if (ret != 0) {
		if (ret == 3)
			ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	ret = MlnImpl_81026814(MLN_HEAP_GENERAL, encoded, 72, 0, &text);
	if (ret != 0)
		goto out;
	content.kind = MLN_XML_BUILD_BUFFER;
	content.value.buffer = text;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "BinarySecurityToken", "wsse",
				 MLN_NEMO_NS_SECEXT, 4, attributes, 1, &content, 0, &node);
	if (ret == 0) {
		made = node;
		node = NULL;
		*out = made;
	}
	goto out;
fail:
	if (path->items != NULL) {
		for (i = path->count - 1; i >= 0; i--)
			if (path->items[i] != NULL)
				MlnBufferDestroy(MLN_HEAP_GENERAL, path->items[i]);
		MlnFree(MLN_HEAP_GENERAL, path->items);
	}
	MlnFree(MLN_HEAP_GENERAL, path);
	path = NULL;
	ret = MLN_ERROR_NO_MEMORY;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, encoded);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, node);
	if (path != NULL) {
		for (i = 0; i < path->count; i++)
			MlnBufferDestroy(MLN_HEAP_GENERAL, path->items[i]);
		MlnFree(MLN_HEAP_GENERAL, path->items);
		MlnFree(MLN_HEAP_GENERAL, path);
	}
	return ret;
}

/* the Id of a signed part: "sigid" and the index in four digits, its high byte first */
__attribute__((noinline)) unsigned int MlnImpl_81085678(unsigned short index, char **id)
{
	unsigned int length;
	char *made;
	unsigned int ret;

	length = MlnStrlen("sigid");
	made = MlnAllocate(MLN_HEAP_GENERAL, length + 5);
	if (made == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	MlnMemcpy(made, "sigid", length);
	ret = MlnImpl_810529E8(index >> 8, 2, 2, made + length);
	if (ret != 0)
		goto out;
	ret = MlnImpl_810529E8(index & 0xff, 2, 2, made + length + 2);
	if (ret != 0)
		goto out;
	made[length + 4] = '\0';
	*id = made;
	made = NULL;
out:
	MlnFree(MLN_HEAP_GENERAL, made);
	return ret;
}

/*
 * a node encrypted as an EncryptedData of the type given: AES-128-CBC with the key, under a
 * random IV that leads the CipherValue
 */
unsigned int MlnImpl_81085718(const unsigned char *key, const char *id, MlnXmlNode *node,
			      const char *type, MlnXmlNode **out)
{
	unsigned char iv[16];
	MlnXmlBuildAttribute attributes[2];
	MlnXmlBuildContent contents[2];
	MlnXmlBuildAttribute algorithm;
	MlnXmlBuildContent valueContent, dataContent;
	void *cipher = NULL;
	MlnBuffer *xml = NULL;
	MlnBuffer *text = NULL;
	MlnBuffer *buffer;
	MlnXmlNode *method = NULL, *value = NULL, *data = NULL;
	void *random = NULL;
	unsigned int random32;
	unsigned int size;
	unsigned int ret;
	int i;

	if (key == NULL || id == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		buffer = NULL;
		goto out;
	}
	if (node == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		buffer = NULL;
		goto out;
	}
	ret = MlnXmlNodeToText(MLN_HEAP_GENERAL, node, &xml);
	if (ret != 0) {
		buffer = NULL;
		goto out;
	}
	ret = MlnImpl_8105714C(&random);
	if (ret != 0) {
		buffer = NULL;
		goto out;
	}
	for (i = 0; i < 16; i++) {
		ret = MlnImpl_810571C4(random, 4, &random32);
		if (ret != 0) {
			buffer = NULL;
			goto out;
		}
		iv[i] = (random32 >> 16) ^ (random32 >> 24);
	}
	ret = MlnImpl_81054880(&cipher, MLN_CIPHER_AES | MLN_CIPHER_KEY_ENCRYPT |
			       MLN_CIPHER_MODE_CBC | MLN_CIPHER_PAD_PKCS, iv, 16, NULL, key, 16);
	if (ret != 0) {
		buffer = NULL;
		goto out;
	}
	size = (xml->size + 16) & ~15U;
	buffer = MlnBufferCreate(MLN_HEAP_GENERAL, size + 16, NULL);
	if (buffer == NULL) {
		ret = MLN_ERROR_NO_MEMORY;
		goto out;
	}
	ret = MlnImpl_81054AC8(cipher, xml->data, xml->size, buffer->data + 16, &size);
	if (ret != 0)
		goto out;
	MlnMemcpy(buffer->data, iv, 16);
	algorithm.name = "Algorithm";
	algorithm.value = "http://www.w3.org/2001/04/xmlenc#aes128-cbc";
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "EncryptionMethod", "xenc", NULL, 1,
				 &algorithm, 0, NULL, 0, &method);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81026814(MLN_HEAP_GENERAL, buffer, 72, 0, &text);
	if (ret != 0)
		goto out;
	valueContent.kind = MLN_XML_BUILD_BUFFER;
	valueContent.value.buffer = text;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "CipherValue", "xenc", NULL, 0, NULL, 1,
				 &valueContent, 0, &value);
	if (ret != 0)
		goto out;
	dataContent.kind = MLN_XML_BUILD_NODE;
	dataContent.value.node = value;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "CipherData", "xenc", NULL, 0, NULL, 1,
				 &dataContent, 0, &data);
	if (ret != 0)
		goto out;
	attributes[0].name = "Id";
	attributes[0].value = id;
	attributes[1].name = "Type";
	attributes[1].value = type;
	contents[0].kind = MLN_XML_BUILD_NODE;
	contents[0].value.node = method;
	contents[1].kind = MLN_XML_BUILD_NODE;
	contents[1].value.node = data;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "EncryptedData", "xenc", MLN_NEMO_NS_XENC, 2,
				 attributes, 2, contents, 0, out);
out:
	MlnImpl_81057244(random);
	MlnBufferDestroy(MLN_HEAP_GENERAL, xml);
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, method);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, value);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, data);
	if (cipher != NULL)
		MlnImpl_81054A90(cipher);
	return ret;
}

/* marks a signed part with its id: Id for the body (kind 0), wsu:Id for a header (kind 1) */
__attribute__((noinline)) unsigned int MlnImpl_81085A88(char *id, int kind, MlnXmlNode *node)
{
	unsigned int ret;

	if (id == NULL || node == NULL)
		return MLN_NEMO_ERROR_PARAMETER;
	switch (kind) {
	case 0:
		return MlnXmlNodeAddAttribute(MLN_HEAP_GENERAL, node, "Id", id);
	case 1:
		ret = MlnXmlNodeAddAttribute(MLN_HEAP_GENERAL, node, "xmlns:wsu",
					     MLN_NEMO_NS_UTILITY);
		if (ret != 0)
			return ret;
		return MlnXmlNodeAddAttribute(MLN_HEAP_GENERAL, node, "wsu:Id", id);
	default:
		return MLN_NEMO_ERROR_PARAMETER;
	}
}

/*
 * the digest of a part to sign: marked first with the signature id of its index (but kind 2,
 * the part already has one), the reference to it ("#" and the id) handed out
 */
unsigned int MlnImpl_81085B24(MlnXmlNode *node, int kind, unsigned char *digest,
			      unsigned int digestSize, unsigned short index, char **id)
{
	char *reference = NULL, *made;
	char *signatureId = NULL;
	MlnBuffer *xml = NULL;
	unsigned int ret;

	if (node == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	if (kind != 2) {
		ret = MlnImpl_81085678(index, &signatureId);
		if (ret != 0)
			goto out;
		ret = MlnImpl_8108418C(signatureId, &reference);
		if (ret != 0)
			goto out;
		ret = MlnImpl_81085A88(signatureId, kind, node);
		if (ret != 0)
			goto out;
	}
	ret = MlnXmlNodeToXml(MLN_HEAP_GENERAL, node, &xml);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81084D14(xml->data, xml->size, digest, digestSize);
	if (ret != 0)
		goto out;
	if (id != NULL) {
		made = reference;
		reference = NULL;
		*id = made;
	}
out:
	MlnFree(MLN_HEAP_GENERAL, signatureId);
	MlnFree(MLN_HEAP_GENERAL, reference);
	MlnBufferDestroy(MLN_HEAP_GENERAL, xml);
	return ret;
}

/*
 * the Signature of a message's parts: their SignedInfo, its canonical form signed by the HCI's
 * signing key, with the key information about the certificate chain given
 */
unsigned int MlnImpl_81085C24(unsigned int count, int method, int *transforms, char **uris,
			      unsigned char **digests, int keyInfo, MlnBuffer *key,
			      MlnXmlNode **out)
{
	MlnXmlNode *signedInfo = NULL;
	MlnXmlNode *signature = NULL, *made;
	MlnBuffer *xml = NULL;
	MlnBuffer *value = NULL;
	void *handle = NULL;
	void *signingKey = NULL;
	int hash;
	unsigned int ret;

	if (key == NULL || uris == NULL || digests == NULL || out == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	ret = MlnImpl_8105C7F0(count, method, transforms, (const char *const *)uris, digests,
			       &signedInfo);
	if (ret != 0)
		goto out;
	ret = MlnXmlNodeToXml(MLN_HEAP_GENERAL, signedInfo, &xml);
	if (ret != 0)
		goto out;
	hash = method;
	if (hash != 0)
		hash = hash == 1 ? 1 : 2;
	ret = MlnImpl_81080BFC(&handle);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81080C3C(1, &signingKey);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81080C90(signingKey, hash, xml, &value);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8105C234(signedInfo, value, keyInfo, &key, "NemoIntegrity", &signature);
	if (ret == 0) {
		made = signature;
		signature = NULL;
		*out = made;
	}
out:
	MlnImpl_81080C80(signingKey);
	MlnImpl_81080C1C(handle);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, signedInfo);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, signature);
	MlnBufferDestroy(MLN_HEAP_GENERAL, xml);
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	return ret;
}

/* whether a step of a kind of message, or the step answering it, is kept fresh */
int MlnImpl_81085D84(unsigned int kind, unsigned int step)
{
	unsigned char usage, nextUsage;
	unsigned int next;
	unsigned char fresh;

	if (kind > 14 || MlnImpl_81072138(kind, step, &usage) != 0)
		return 0;
	switch (step) {
	case MLN_NEMO_STEP_REQUEST:
		next = MLN_NEMO_STEP_RESPONSE;
		break;
	case MLN_NEMO_STEP_RESPONSE:
		next = MLN_NEMO_STEP_CONFIRMATION;
		break;
	default:
		return 0;
	}
	fresh = usage & MLN_NEMO_USAGE_FRESH;
	if (fresh)
		return 1;
	if (MlnImpl_81072138(kind, next, &nextUsage) != 0)
		return fresh;
	return (nextUsage & MLN_NEMO_USAGE_FRESH) != 0;
}

/*
 * a Nonce of a message read: the peer's for this step (handed out), or the one returned, which
 * must be the nonce sent where the step returns it; its digest is taken when it is signed
 */
unsigned int MlnImpl_81085E20(MlnXmlNode *nonce, unsigned int kind, unsigned int step,
			      unsigned int *found, unsigned int *signedParts,
			      const MlnBuffer *sent, MlnBuffer **out, int *count,
			      unsigned char *digest, int digestSize, MlnBuffer **id)
{
	MlnBuffer *nonceId = NULL, *madeId;
	MlnBuffer *xml = NULL;
	MlnBuffer *text = NULL;
	MlnBuffer *decoded = NULL;
	MlnBuffer *usage = NULL;
	MlnBuffer *value;
	unsigned int foundPart, signedPart;
	int noId, digested;
	unsigned int ret;

	if (nonce == NULL || found == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		value = NULL;
		goto out;
	}
	if (signedParts == NULL || count == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		value = NULL;
		goto out;
	}
	if (digest == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		value = NULL;
		goto out;
	}
	noId = MlnXmlGetAttribute(MLN_HEAP_GENERAL, nonce, NULL, "wsu:Id", &nonceId);
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, nonce, &text);
	if (ret != 0) {
		value = NULL;
		goto out;
	}
	ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &decoded);
	if (ret != 0) {
		value = NULL;
		goto out;
	}
	if (decoded->size > 128) {
		ret = MLN_NEMO_ERROR_NONCE;
		value = NULL;
		goto out;
	}
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, nonce, NULL, "nemosec:Usage", &usage);
	if (ret != 0) {
		value = NULL;
		goto out;
	}
	switch (step) {
	case MLN_NEMO_STEP_REQUEST:
		if (!MLN_NEMO_IS(usage, MLN_NEMO_USAGE("request-nonce")))
			goto returned;
		break;
	case MLN_NEMO_STEP_RESPONSE:
		if (!MLN_NEMO_IS(usage, MLN_NEMO_USAGE("response-nonce")))
			goto returned;
		break;
	case MLN_NEMO_STEP_FAULT:
		if (!MLN_NEMO_IS(usage, MLN_NEMO_USAGE("fault-nonce")))
			goto returned;
		break;
	default:
		goto returned;
	}
	value = decoded;
	decoded = NULL;
	signedPart = nonceId != NULL ? MLN_NEMO_SIGNED_NONCE : 0;
	foundPart = MLN_NEMO_FOUND_NONCE;
	goto digest;
returned:
	if (!MLN_NEMO_IS(usage, MLN_NEMO_USAGE("response-returnedNonce")) &&
	    !MLN_NEMO_IS(usage, MLN_NEMO_USAGE("confirmation-returnedNonce")) &&
	    !MLN_NEMO_IS(usage, MLN_NEMO_USAGE("fault-returnedNonce"))) {
		ret = MLN_NEMO_ERROR_FORMAT;
		value = NULL;
		goto out;
	}
	if (sent == NULL) {
		value = NULL;
		ret = MLN_ERROR_INVALID;
		goto out;
	}
	if (MlnImpl_810721BC(kind, step) ||
	    (step == MLN_NEMO_STEP_REQUEST && kind == MLN_NEMO_KIND_TRANSFER)) {
		if (MlnBufferCompare(MLN_HEAP_GENERAL, sent, decoded) != 0) {
			ret = MLN_NEMO_ERROR_NONCE;
			value = NULL;
			goto out;
		}
	}
	value = NULL;
	signedPart = nonceId != NULL ? MLN_NEMO_SIGNED_RETURNED_NONCE : 0;
	foundPart = MLN_NEMO_FOUND_RETURNED_NONCE;
digest:
	if (noId != 0) {
		digested = 0;
	} else {
		ret = MlnXmlNodeToXml(MLN_HEAP_GENERAL, nonce, &xml);
		if (ret != 0)
			goto out;
		ret = MlnImpl_81084D14(xml->data, xml->size, digest, digestSize);
		if (ret != 0)
			goto out;
		digested = 1;
	}
	madeId = nonceId;
	nonceId = NULL;
	*count += digested;
	*found |= foundPart;
	*signedParts |= signedPart;
	*id = madeId;
	*out = value;
	value = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	MlnBufferDestroy(MLN_HEAP_GENERAL, nonceId);
	MlnBufferDestroy(MLN_HEAP_GENERAL, xml);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnBufferDestroy(MLN_HEAP_GENERAL, decoded);
	MlnBufferDestroy(MLN_HEAP_GENERAL, usage);
	return ret;
}

/* whether a kind of message is answered with an encrypted response */
__attribute__((noinline)) int MlnImpl_81086254(unsigned int kind)
{
	unsigned char usage;

	if (kind > 14)
		return 0;
	usage = MlnNemoUsage[kind][1];
	return usage != MLN_NEMO_USAGE_NONE && (usage & MLN_NEMO_USAGE_ENCRYPTED) != 0;
}

/* a certificate chain read, freed */
static inline __attribute__((always_inline)) void MlnNemoChainFree(MlnBuffer **chain, int count)
{
	int i;

	if (chain == NULL)
		return;
	for (i = 0; i < count; i++)
		if (chain[i] != NULL)
			MlnBufferDestroy(MLN_HEAP_GENERAL, chain[i]);
	MlnFree(MLN_HEAP_GENERAL, chain);
}

/*
 * the BinarySecurityTokens of a message read: the message key's (a SymmetricKey) and the
 * certificate chains (X509PKIPathv1) of the signer and, in a request, of the key the
 * response is to be encrypted for; the digests of those signed, checked against what the
 * step's usage needs
 */
unsigned int MlnImpl_81086284(MlnXmlNode *security, unsigned int kind, unsigned int step,
			      int signerKind, int peerKind, unsigned int *encrypted,
			      unsigned int *signedParts, MlnBuffer **keyToken,
			      MlnBuffer **signingCertificate, MlnBuffer **encryptionCertificate,
			      unsigned char *digest, unsigned char *secondDigest, int digestSize,
			      int secondDigestSize, MlnBuffer **id, MlnBuffer **secondId,
			      MlnBuffer ***certificates, int *certificateCount,
			      MlnBuffer ***secondCertificates, int *secondCertificateCount)
{
	unsigned char usage;
	MlnXmlNodeList *tokens = NULL;
	MlnXmlNode *token;
	MlnBuffer *valueType = NULL;
	MlnBuffer *xml = NULL;
	MlnBuffer *text = NULL;
	MlnBuffer *key = NULL;
	MlnBuffer *keyId = NULL;
	MlnBuffer *peerId = NULL;
	MlnBuffer *tokenUsage = NULL;
	MlnBuffer *leaf = NULL;
	MlnBuffer **peerChain = NULL;
	MlnBuffer **signerChain = NULL;
	int peerCount = 0, signerCount = 0;
	MlnBuffer *signer = NULL;
	MlnBuffer *peer = NULL;
	unsigned int encryptedParts = 0, signedFound = 0;
	int haveKey = 0, haveSigner = 0, havePeer = 0;
	unsigned int ret, result;
	int i, count;

	if (kind > 14) {
		ret = MLN_NEMO_ERROR_KIND;
		goto out;
	}
	ret = MlnImpl_81072138(kind, step, &usage);
	if (ret != 0)
		goto out;
	result = MlnXmlFindNodes(MLN_HEAP_GENERAL, security, "BinarySecurityToken",
				 MLN_NEMO_NS_SECEXT, &tokens);
	if (result != 0) {
		if (result != MLN_XML_ERROR_NOT_FOUND) {
			ret = result;
			goto out;
		}
		goto check;
	}
	count = tokens->count;
	for (i = 0; i < count; i++) {
		token = tokens->nodes[i];
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, token, NULL, "ValueType", &valueType);
		if (ret != 0)
			goto out;
		if (MLN_NEMO_IS(valueType, "http://nemo.intertrust.com/2005/10/security/BST/SymmetricKey")) {
			MlnBufferDestroy(MLN_HEAP_GENERAL, text);
			text = NULL;
			ret = MlnXmlGetText(MLN_HEAP_GENERAL, token, &text);
			if (ret != 0)
				goto out;
			MlnBufferDestroy(MLN_HEAP_GENERAL, key);
			key = NULL;
			ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &key);
			if (ret != 0)
				goto out;
			MlnBufferDestroy(MLN_HEAP_GENERAL, keyId);
			keyId = NULL;
			result = MlnXmlGetAttribute(MLN_HEAP_GENERAL, token, NULL, "wsu:Id", &keyId);
			if (result != 0) {
				if (result != MLN_XML_ERROR_NOT_FOUND) {
					ret = result;
					goto out;
				}
			} else {
				ret = MlnXmlNodeToXml(MLN_HEAP_GENERAL, token, &xml);
				if (ret != 0)
					goto out;
				ret = MlnImpl_81084D14(xml->data, xml->size, digest, digestSize);
				if (ret != 0)
					goto out;
				if (usage & MLN_NEMO_USAGE_SIGNED)
					signedFound |= MLN_NEMO_SIGNED_MESSAGE_KEY;
			}
			haveKey = 1;
			encryptedParts = MLN_NEMO_ENCRYPTED_KEY;
		} else if (MLN_NEMO_IS(valueType,
				       "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-x509-token-profile-1.0#X509PKIPathv1")) {
			MlnBufferDestroy(MLN_HEAP_GENERAL, tokenUsage);
			tokenUsage = NULL;
			ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, token, NULL, "nemosec:Usage",
						 &tokenUsage);
			if (ret != 0)
				goto out;
			if (step == MLN_NEMO_STEP_REQUEST) {
				MlnBufferDestroy(MLN_HEAP_GENERAL, leaf);
				leaf = NULL;
				MlnNemoChainFree(signerChain, signerCount);
				signerChain = NULL;
				ret = MlnImpl_81084D58(tokenUsage, MLN_NEMO_USAGE("request-signingKey"),
						       token, signerKind, &leaf, &signerChain,
						       &signerCount);
				if (ret != 0)
					goto out;
				if (leaf != NULL) {
					signer = leaf;
					haveSigner = 1;
					leaf = NULL;
				}
				MlnNemoChainFree(peerChain, peerCount);
				peerChain = NULL;
				ret = MlnImpl_81084D58(tokenUsage,
						       MLN_NEMO_USAGE("response-encryptionKey"), token,
						       peerKind, &leaf, &peerChain, &peerCount);
				if (ret != 0)
					goto out;
				if (leaf != NULL) {
					peer = leaf;
					leaf = NULL;
					MlnBufferDestroy(MLN_HEAP_GENERAL, peerId);
					peerId = NULL;
					result = MlnXmlGetAttribute(MLN_HEAP_GENERAL, token, NULL,
								    "wsu:Id", &peerId);
					if (result != 0) {
						if (result != MLN_XML_ERROR_NOT_FOUND) {
							ret = result;
							goto out;
						}
					} else {
						ret = MlnXmlNodeToXml(MLN_HEAP_GENERAL, token, &xml);
						if (ret != 0)
							goto out;
						ret = MlnImpl_81084D14(xml->data, xml->size, secondDigest,
								       secondDigestSize);
						if (ret != 0)
							goto out;
						if (usage & MLN_NEMO_USAGE_SIGNED)
							signedFound |= MLN_NEMO_SIGNED_CERTIFICATES;
					}
					havePeer = 1;
				}
			} else if (step == MLN_NEMO_STEP_CONFIRMATION) {
				MlnBufferDestroy(MLN_HEAP_GENERAL, leaf);
				leaf = NULL;
				MlnNemoChainFree(signerChain, signerCount);
				signerChain = NULL;
				ret = MlnImpl_81084D58(tokenUsage,
						       MLN_NEMO_USAGE("confirmation-signingKey"), token,
						       signerKind, &leaf, &signerChain, &signerCount);
				if (ret != 0)
					goto out;
				if (leaf != NULL) {
					signer = leaf;
					leaf = NULL;
					haveSigner = 1;
				}
			}
			MlnBufferDestroy(MLN_HEAP_GENERAL, tokenUsage);
			tokenUsage = NULL;
		} else {
			ret = MLN_NEMO_ERROR_FORMAT;
			goto out;
		}
		MlnBufferDestroy(MLN_HEAP_GENERAL, xml);
		xml = NULL;
		MlnBufferDestroy(MLN_HEAP_GENERAL, valueType);
		valueType = NULL;
	}
	ret = 0;
check:
	if (((usage & MLN_NEMO_USAGE_ENCRYPTED) && !haveKey) ||
	    (!havePeer && step == MLN_NEMO_STEP_REQUEST && MlnImpl_81086254(kind))) {
		ret = MLN_NEMO_ERROR_FORMAT;
		goto out;
	}
	if ((usage & MLN_NEMO_USAGE_SIGNED) &&
	    (step == MLN_NEMO_STEP_REQUEST || step == MLN_NEMO_STEP_CONFIRMATION) && !haveSigner) {
		ret = MLN_NEMO_ERROR_FORMAT;
		goto out;
	}
	*encrypted |= encryptedParts;
	*signedParts |= signedFound;
	*keyToken = key;
	*id = keyId;
	*secondId = peerId;
	*signingCertificate = signer;
	key = NULL;
	*encryptionCertificate = peer;
	peer = NULL;
	keyId = NULL;
	peerId = NULL;
	*certificates = peerChain;
	peerChain = NULL;
	*certificateCount = peerCount;
	*secondCertificates = signerChain;
	signerChain = NULL;
	signer = NULL;
	*secondCertificateCount = signerCount;
out:
	MlnXmlListFree(MLN_HEAP_GENERAL, tokens);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnBufferDestroy(MLN_HEAP_GENERAL, key);
	MlnBufferDestroy(MLN_HEAP_GENERAL, signer);
	MlnBufferDestroy(MLN_HEAP_GENERAL, peer);
	MlnBufferDestroy(MLN_HEAP_GENERAL, tokenUsage);
	MlnBufferDestroy(MLN_HEAP_GENERAL, leaf);
	MlnBufferDestroy(MLN_HEAP_GENERAL, valueType);
	MlnBufferDestroy(MLN_HEAP_GENERAL, xml);
	MlnBufferDestroy(MLN_HEAP_GENERAL, keyId);
	MlnBufferDestroy(MLN_HEAP_GENERAL, peerId);
	MlnNemoChainFree(peerChain, peerCount);
	MlnNemoChainFree(signerChain, signerCount);
	return ret;
}
