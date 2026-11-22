/*
 * XML signatures (http://www.w3.org/2000/09/xmldsig#): a Signature element read into its
 * parts (0x81059934-). The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_XMLDSIG_ERROR_FORMAT	0x220A0005

#define MLN_WSSE_NAMESPACE \
	"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd"

/*
 * Read a Signature element: its Id, SignedInfo (with each Reference's URI, transforms,
 * digest method and value), the SignatureValue and the KeyInfo's key name, certificates or
 * subject key identifier.
 */
int MlnImpl_81059934(MlnXmlNode *node, MlnXmlDsigSignature **out)
{
	MlnBuffer *id = NULL;
	MlnBuffer *uri = NULL;
	MlnBuffer *canonicalizationMethod = NULL;
	MlnBuffer *signatureMethod = NULL;
	MlnBuffer *text = NULL;
	MlnBuffer *signatureValue = NULL;
	MlnBuffer *signedInfoXml = NULL;
	MlnBuffer *keyName = NULL;
	MlnBuffer *subjectKeyIdentifier = NULL;
	MlnXmlNodeList *elements = NULL;
	MlnXmlNodeList *children = NULL;
	MlnXmlNodeList *values = NULL;
	MlnXmlDsigReference *references = NULL;
	MlnXmlDsigReference *reference;
	MlnXmlDsigSignature *signature;
	MlnBuffer **certificates = NULL;
	MlnXmlNode *signedInfo;
	MlnXmlNode *keyInfo;
	MlnXmlNode *item;
	int referenceCount = 0;
	int certificateCount = 0;
	int transformCount;
	int ret;
	int i;
	int j;

	if (MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "Id", &id) != 0)
		id = NULL;

	/* SignedInfo */
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "SignedInfo", MLN_XMLDSIG_NAMESPACE,
			      &elements);
	if (ret != 0)
		goto out;
	if (elements->count != 1)
		goto format;
	signedInfo = elements->nodes[0];
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, signedInfo, "CanonicalizationMethod",
			      MLN_XMLDSIG_NAMESPACE, &children);
	if (ret != 0)
		goto out;
	if (children->count != 1)
		goto format;
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, children->nodes[0], NULL, "Algorithm",
				 &canonicalizationMethod);
	if (ret != 0)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, children);
	children = NULL;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, signedInfo, "SignatureMethod",
			      MLN_XMLDSIG_NAMESPACE, &children);
	if (ret != 0)
		goto out;
	if (children->count != 1)
		goto format;
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, children->nodes[0], NULL, "Algorithm",
				 &signatureMethod);
	if (ret != 0)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, children);
	children = NULL;

	/* its References */
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, signedInfo, "Reference", MLN_XMLDSIG_NAMESPACE,
			      &children);
	if (ret != 0)
		goto out;
	referenceCount = children->count;
	references = MlnAllocate(MLN_HEAP_GENERAL, referenceCount * sizeof(*references));
	if (references == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(references, 0, referenceCount * sizeof(*references));
	reference = references;
	for (i = 0; i < referenceCount; i++, reference++) {
		item = children->nodes[i];
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, item, NULL, "URI", &uri);
		if (ret != 0)
			goto out;
		reference->uri = uri;
		uri = NULL;
		if (MlnXmlFindNodes(MLN_HEAP_GENERAL, item, "Transforms/Transform",
				    MLN_XMLDSIG_NAMESPACE, &values) == 0) {
			transformCount = values->count;
			reference->transformCount = transformCount;
			reference->transforms = MlnAllocate(MLN_HEAP_GENERAL,
							    transformCount * sizeof(MlnBuffer *));
			if (reference->transforms == NULL) {
				ret = 2;
				goto out;
			}
			MlnMemset(reference->transforms, 0, transformCount * sizeof(MlnBuffer *));
			for (j = 0; j < transformCount; j++) {
				ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, values->nodes[j], NULL,
							 "Algorithm", &reference->transforms[j]);
				if (ret != 0)
					goto out;
			}
		}
		MlnXmlListFree(MLN_HEAP_GENERAL, values);
		values = NULL;
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, item, "DigestMethod", MLN_XMLDSIG_NAMESPACE,
				      &values);
		if (ret != 0)
			goto out;
		if (values->count != 1)
			goto format;
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, values->nodes[0], NULL, "Algorithm",
					 &reference->digestMethod);
		if (ret != 0)
			goto out;
		MlnXmlListFree(MLN_HEAP_GENERAL, values);
		values = NULL;
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, item, "DigestValue", MLN_XMLDSIG_NAMESPACE,
				      &values);
		if (ret != 0)
			goto out;
		if (values->count != 1)
			goto format;
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, values->nodes[0], &text);
		if (ret != 0)
			goto out;
		ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &reference->digestValue);
		if (ret != 0)
			goto out;
		MlnBufferDestroy(MLN_HEAP_GENERAL, text);
		text = NULL;
		MlnXmlListFree(MLN_HEAP_GENERAL, values);
		values = NULL;
	}
	MlnXmlListFree(MLN_HEAP_GENERAL, children);
	children = NULL;
	ret = MlnXmlNodeToXml(MLN_HEAP_GENERAL, elements->nodes[0], &signedInfoXml);
	if (ret != 0)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, elements);
	elements = NULL;

	/* SignatureValue */
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "SignatureValue", MLN_XMLDSIG_NAMESPACE,
			      &elements);
	if (ret != 0)
		goto out;
	if (elements->count != 1)
		goto format;
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, elements->nodes[0], &text);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &signatureValue);
	if (ret != 0)
		goto out;
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	text = NULL;
	MlnXmlListFree(MLN_HEAP_GENERAL, elements);
	elements = NULL;

	/* KeyInfo */
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "KeyInfo", MLN_XMLDSIG_NAMESPACE,
			      &elements);
	if (ret != 0)
		goto out;
	if (elements->count != 1)
		goto format;
	keyInfo = elements->nodes[0];
	if (MlnXmlFindNodes(MLN_HEAP_GENERAL, keyInfo, "KeyName", MLN_XMLDSIG_NAMESPACE,
			    &children) == 0) {
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, children->nodes[0], &keyName);
		if (ret != 0)
			goto out;
	} else if (MlnXmlFindNodes(MLN_HEAP_GENERAL, keyInfo, "X509Data/X509Certificate",
				   MLN_XMLDSIG_NAMESPACE, &children) == 0) {
		certificateCount = children->count;
		certificates = MlnAllocate(MLN_HEAP_GENERAL,
					   certificateCount * sizeof(MlnBuffer *));
		if (certificates == NULL) {
			ret = 2;
			goto out;
		}
		MlnMemset(certificates, 0, certificateCount * sizeof(MlnBuffer *));
		for (i = 0; i < certificateCount; i++) {
			ret = MlnXmlGetText(MLN_HEAP_GENERAL, children->nodes[i], &text);
			if (ret != 0)
				goto out;
			ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &certificates[i]);
			if (ret != 0)
				goto out;
			MlnBufferDestroy(MLN_HEAP_GENERAL, text);
			text = NULL;
		}
	} else {
		if (MlnXmlFindNodes(MLN_HEAP_GENERAL, keyInfo, "X509Data/X509SKI",
				    MLN_XMLDSIG_NAMESPACE, &children) == 0) {
			if (children->count != 1)
				goto format;
			ret = MlnXmlGetText(MLN_HEAP_GENERAL, children->nodes[0], &text);
			if (ret != 0)
				goto out;
			ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &subjectKeyIdentifier);
			if (ret != 0)
				goto out;
		} else {
			ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, keyInfo, "SecurityTokenReference",
					      MLN_WSSE_NAMESPACE, &children);
			if (ret != 0)
				goto out;
			if (children->count != 1)
				goto format;
			ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, children->nodes[0],
					      "X509Data/X509SKI", MLN_XMLDSIG_NAMESPACE, &values);
			if (ret != 0)
				goto out;
			if (values->count != 1)
				goto format;
			ret = MlnXmlGetText(MLN_HEAP_GENERAL, values->nodes[0], &text);
			if (ret != 0)
				goto out;
			MlnXmlListFree(MLN_HEAP_GENERAL, values);
			values = NULL;
			ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &subjectKeyIdentifier);
			if (ret != 0)
				goto out;
		}
		MlnBufferDestroy(MLN_HEAP_GENERAL, text);
		text = NULL;
	}
	MlnXmlListFree(MLN_HEAP_GENERAL, children);
	children = NULL;
	MlnXmlListFree(MLN_HEAP_GENERAL, elements);
	elements = NULL;

	signature = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*signature));
	if (signature == NULL) {
		ret = 2;
		goto out;
	}
	signature->referenceCount = referenceCount;
	signature->references = references;
	signature->id = id;
	id = NULL;
	signature->canonicalizationMethod = canonicalizationMethod;
	canonicalizationMethod = NULL;
	signature->signatureMethod = signatureMethod;
	signatureMethod = NULL;
	signature->signedInfo = signedInfoXml;
	signedInfoXml = NULL;
	signature->signatureValue = signatureValue;
	signatureValue = NULL;
	if (keyName != NULL) {
		signature->keyInfoType = MLN_XMLDSIG_KEYINFO_KEYNAME;
		signature->keyInfo.keyName = keyName;
		keyName = NULL;
	} else if (subjectKeyIdentifier != NULL) {
		signature->keyInfo.keyName = subjectKeyIdentifier;
		signature->keyInfoType = MLN_XMLDSIG_KEYINFO_X509SKI;
		subjectKeyIdentifier = NULL;
	} else {
		signature->keyInfo.x509.certificates = certificates;
		certificates = NULL;
		signature->keyInfoType = MLN_XMLDSIG_KEYINFO_X509;
		signature->keyInfo.x509.count = certificateCount;
	}
	*out = signature;
	references = NULL;
	goto out;
format:
	ret = MLN_XMLDSIG_ERROR_FORMAT;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	MlnBufferDestroy(MLN_HEAP_GENERAL, canonicalizationMethod);
	MlnBufferDestroy(MLN_HEAP_GENERAL, signatureMethod);
	MlnBufferDestroy(MLN_HEAP_GENERAL, uri);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnBufferDestroy(MLN_HEAP_GENERAL, signedInfoXml);
	MlnBufferDestroy(MLN_HEAP_GENERAL, signatureValue);
	MlnBufferDestroy(MLN_HEAP_GENERAL, keyName);
	MlnBufferDestroy(MLN_HEAP_GENERAL, subjectKeyIdentifier);
	if (certificates != NULL) {
		for (i = 0; i < certificateCount; i++) {
			if (certificates[i] != NULL)
				MlnBufferDestroy(MLN_HEAP_GENERAL, certificates[i]);
		}
		MlnFree(MLN_HEAP_GENERAL, certificates);
	}
	if (elements != NULL)
		MlnXmlListFree(MLN_HEAP_GENERAL, elements);
	if (children != NULL)
		MlnXmlListFree(MLN_HEAP_GENERAL, children);
	if (values != NULL)
		MlnXmlListFree(MLN_HEAP_GENERAL, values);
	if (references != NULL) {
		for (i = 0; i < referenceCount; i++)
			MlnImpl_8105D018(&references[i]);
		MlnFree(MLN_HEAP_GENERAL, references);
	}
	return ret;
}

#define MLN_WSSE_X509_SKI \
	"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-x509-token-profile-1.0" \
	"#X509v3SubjectKeyIdentifier"

/* the certificate chain of the document's BinarySecurityToken of a wsu:Id */
static inline int MlnXmlDsigFindToken(MlnXmlNode *document, const MlnBuffer *tokenId,
				       MlnBuffer ***certificates, int *count)
{
	MlnXmlNodeList *tokens = NULL;
	MlnBuffer *wsuId = NULL;
	MlnBuffer *token = NULL;
	int ret;
	int i;

	if (document == NULL || tokenId == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, document, "BinarySecurityToken",
			      MLN_WSSE_NAMESPACE, &tokens);
	if (ret != 0)
		goto out;
	for (i = 0; ; i++) {
		if (i >= tokens->count) {
			ret = MLN_XMLDSIG_ERROR_FORMAT;
			goto out;
		}
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, tokens->nodes[i], NULL, "wsu:Id",
					 &wsuId);
		if (ret == 0) {
			if (MlnBytesCompare(MLN_HEAP_GENERAL, wsuId->size, wsuId->data,
					    tokenId->size, tokenId->data) == 0)
				break;
		} else if (ret != MLN_XML_ERROR_NOT_FOUND) {
			goto out;
		}
		MlnBufferDestroy(MLN_HEAP_GENERAL, wsuId);
		wsuId = NULL;
	}
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, tokens->nodes[i], &token);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81059600(token, certificates, count);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, wsuId);
	MlnBufferDestroy(MLN_HEAP_GENERAL, token);
	MlnXmlListFree(MLN_HEAP_GENERAL, tokens);
	return ret;
}

/*
 * Read a Signature element as MlnImpl_81059934 does; a KeyInfo's SecurityTokenReference may
 * also name a KeyIdentifier, or Reference a BinarySecurityToken of the document by its
 * wsu:Id, whose certificate chain is the signer's.
 */
int MlnImpl_8105A3D8(MlnXmlNode *node, MlnXmlNode *document, MlnXmlDsigSignature **out)
{
	MlnBuffer *id = NULL;
	MlnBuffer *uri = NULL;
	MlnBuffer *canonicalizationMethod = NULL;
	MlnBuffer *signatureMethod = NULL;
	MlnBuffer *text = NULL;
	MlnBuffer *signatureValue = NULL;
	MlnBuffer *signedInfoXml = NULL;
	MlnBuffer *keyName = NULL;
	MlnBuffer *subjectKeyIdentifier = NULL;
	MlnXmlNodeList *elements = NULL;
	MlnXmlNodeList *children = NULL;
	MlnXmlNodeList *values = NULL;
	MlnBuffer *attribute = NULL;
	MlnBuffer *tokenId = NULL;
	MlnXmlDsigReference *references = NULL;
	MlnXmlDsigReference *reference;
	MlnXmlDsigSignature *signature;
	MlnBuffer **certificates = NULL;
	MlnXmlNode *signedInfo;
	MlnXmlNode *keyInfo;
	MlnXmlNode *item;
	int referenceCount = 0;
	int certificateCount = 0;
	int transformCount;
	int ret;
	int i;
	int j;

	if (MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "Id", &id) != 0)
		id = NULL;

	/* SignedInfo */
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "SignedInfo", MLN_XMLDSIG_NAMESPACE,
			      &elements);
	if (ret != 0)
		goto out;
	if (elements->count != 1)
		goto format;
	signedInfo = elements->nodes[0];
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, signedInfo, "CanonicalizationMethod",
			      MLN_XMLDSIG_NAMESPACE, &children);
	if (ret != 0)
		goto out;
	if (children->count != 1)
		goto format;
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, children->nodes[0], NULL, "Algorithm",
				 &canonicalizationMethod);
	if (ret != 0)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, children);
	children = NULL;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, signedInfo, "SignatureMethod",
			      MLN_XMLDSIG_NAMESPACE, &children);
	if (ret != 0)
		goto out;
	if (children->count != 1)
		goto format;
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, children->nodes[0], NULL, "Algorithm",
				 &signatureMethod);
	if (ret != 0)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, children);
	children = NULL;

	/* its References */
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, signedInfo, "Reference", MLN_XMLDSIG_NAMESPACE,
			      &children);
	if (ret != 0)
		goto out;
	referenceCount = children->count;
	references = MlnAllocate(MLN_HEAP_GENERAL, referenceCount * sizeof(*references));
	if (references == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(references, 0, referenceCount * sizeof(*references));
	reference = references;
	for (i = 0; i < referenceCount; i++, reference++) {
		item = children->nodes[i];
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, item, NULL, "URI", &uri);
		if (ret != 0)
			goto out;
		reference->uri = uri;
		uri = NULL;
		if (MlnXmlFindNodes(MLN_HEAP_GENERAL, item, "Transforms/Transform",
				    MLN_XMLDSIG_NAMESPACE, &values) == 0) {
			transformCount = values->count;
			reference->transformCount = transformCount;
			reference->transforms = MlnAllocate(MLN_HEAP_GENERAL,
							    transformCount * sizeof(MlnBuffer *));
			if (reference->transforms == NULL) {
				ret = 2;
				goto out;
			}
			MlnMemset(reference->transforms, 0, transformCount * sizeof(MlnBuffer *));
			for (j = 0; j < transformCount; j++) {
				ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, values->nodes[j], NULL,
							 "Algorithm", &reference->transforms[j]);
				if (ret != 0)
					goto out;
			}
		}
		MlnXmlListFree(MLN_HEAP_GENERAL, values);
		values = NULL;
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, item, "DigestMethod", MLN_XMLDSIG_NAMESPACE,
				      &values);
		if (ret != 0)
			goto out;
		if (values->count != 1)
			goto format;
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, values->nodes[0], NULL, "Algorithm",
					 &reference->digestMethod);
		if (ret != 0)
			goto out;
		MlnXmlListFree(MLN_HEAP_GENERAL, values);
		values = NULL;
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, item, "DigestValue", MLN_XMLDSIG_NAMESPACE,
				      &values);
		if (ret != 0)
			goto out;
		if (values->count != 1)
			goto format;
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, values->nodes[0], &text);
		if (ret != 0)
			goto out;
		ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &reference->digestValue);
		if (ret != 0)
			goto out;
		MlnBufferDestroy(MLN_HEAP_GENERAL, text);
		text = NULL;
		MlnXmlListFree(MLN_HEAP_GENERAL, values);
		values = NULL;
	}
	MlnXmlListFree(MLN_HEAP_GENERAL, children);
	children = NULL;
	ret = MlnXmlNodeToXml(MLN_HEAP_GENERAL, elements->nodes[0], &signedInfoXml);
	if (ret != 0)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, elements);
	elements = NULL;

	/* SignatureValue */
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "SignatureValue", MLN_XMLDSIG_NAMESPACE,
			      &elements);
	if (ret != 0)
		goto out;
	if (elements->count != 1)
		goto format;
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, elements->nodes[0], &text);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &signatureValue);
	if (ret != 0)
		goto out;
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	text = NULL;
	MlnXmlListFree(MLN_HEAP_GENERAL, elements);
	elements = NULL;

	/* KeyInfo */
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "KeyInfo", MLN_XMLDSIG_NAMESPACE,
			      &elements);
	if (ret != 0)
		goto out;
	if (elements->count != 1)
		goto format;
	keyInfo = elements->nodes[0];
	if (MlnXmlFindNodes(MLN_HEAP_GENERAL, keyInfo, "KeyName", MLN_XMLDSIG_NAMESPACE,
			    &children) == 0) {
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, children->nodes[0], &keyName);
		if (ret != 0)
			goto out;
	} else if (MlnXmlFindNodes(MLN_HEAP_GENERAL, keyInfo, "X509Data/X509Certificate",
				   MLN_XMLDSIG_NAMESPACE, &children) == 0) {
		certificateCount = children->count;
		certificates = MlnAllocate(MLN_HEAP_GENERAL,
					   certificateCount * sizeof(MlnBuffer *));
		if (certificates == NULL) {
			ret = 2;
			goto out;
		}
		MlnMemset(certificates, 0, certificateCount * sizeof(MlnBuffer *));
		for (i = 0; i < certificateCount; i++) {
			ret = MlnXmlGetText(MLN_HEAP_GENERAL, children->nodes[i], &text);
			if (ret != 0)
				goto out;
			ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &certificates[i]);
			if (ret != 0)
				goto out;
			MlnBufferDestroy(MLN_HEAP_GENERAL, text);
			text = NULL;
		}
	} else if (MlnXmlFindNodes(MLN_HEAP_GENERAL, keyInfo, "X509Data/X509SKI",
				   MLN_XMLDSIG_NAMESPACE, &children) == 0) {
		if (children->count != 1)
			goto format;
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, children->nodes[0], &text);
		if (ret != 0)
			goto out;
		ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &subjectKeyIdentifier);
		if (ret != 0)
			goto out;
		MlnBufferDestroy(MLN_HEAP_GENERAL, text);
		text = NULL;
	} else {
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, keyInfo, "SecurityTokenReference",
				      MLN_WSSE_NAMESPACE, &children);
		if (ret != 0)
			goto out;
		if (children->count != 1)
			goto format;
		if (MlnXmlFindNodes(MLN_HEAP_GENERAL, children->nodes[0], "X509Data/X509SKI",
				    MLN_XMLDSIG_NAMESPACE, &values) == 0) {
			if (values->count != 1)
				goto format;
			ret = MlnXmlGetText(MLN_HEAP_GENERAL, values->nodes[0], &text);
			if (ret != 0)
				goto out;
			MlnXmlListFree(MLN_HEAP_GENERAL, values);
			values = NULL;
			ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, &subjectKeyIdentifier);
			if (ret != 0)
				goto out;
			MlnBufferDestroy(MLN_HEAP_GENERAL, text);
			text = NULL;
		} else {
			MlnXmlListFree(MLN_HEAP_GENERAL, values);
			values = NULL;
			if (MlnXmlFindNodes(MLN_HEAP_GENERAL, children->nodes[0], "Reference",
					    MLN_WSSE_NAMESPACE, &values) != 0) {
				ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, children->nodes[0],
						      "KeyIdentifier", MLN_WSSE_NAMESPACE, &values);
				if (ret != 0)
					goto out;
				if (values->count != 1)
					goto format;
				ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, values->nodes[0], NULL,
							 "ValueType", &attribute);
				if (ret != 0)
					goto out;
				/* Sony ignores what the comparison gives */
				MlnBytesCompare(MLN_HEAP_GENERAL, MlnStrlen(MLN_WSSE_X509_SKI),
						MLN_WSSE_X509_SKI, attribute->size, attribute->data);
				ret = MlnXmlGetText(MLN_HEAP_GENERAL, values->nodes[0], &text);
				if (ret != 0)
					goto out;
				MlnXmlListFree(MLN_HEAP_GENERAL, values);
				values = NULL;
				ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text,
						       &subjectKeyIdentifier);
				if (ret != 0)
					goto out;
				MlnBufferDestroy(MLN_HEAP_GENERAL, text);
				text = NULL;
			} else {
				if (values->count != 1)
					goto format;
				ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, values->nodes[0], NULL,
							 "URI", &attribute);
				if (ret != 0)
					goto out;
				/* the token's wsu:Id, the URI without its '#' */
				tokenId = MlnBufferCreate(MLN_HEAP_GENERAL, attribute->size - 1,
							  attribute->data + 1);
				MlnBufferDestroy(MLN_HEAP_GENERAL, attribute);
				attribute = NULL;
				ret = MlnXmlDsigFindToken(document, tokenId, &certificates,
							   &certificateCount);
				if (ret != 0)
					goto out;
			}
		}
	}
	MlnXmlListFree(MLN_HEAP_GENERAL, children);
	children = NULL;
	MlnXmlListFree(MLN_HEAP_GENERAL, elements);
	elements = NULL;

	signature = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*signature));
	if (signature == NULL) {
		ret = 2;
		goto out;
	}
	signature->referenceCount = referenceCount;
	signature->references = references;
	signature->id = id;
	id = NULL;
	signature->canonicalizationMethod = canonicalizationMethod;
	canonicalizationMethod = NULL;
	signature->signatureMethod = signatureMethod;
	signatureMethod = NULL;
	signature->signedInfo = signedInfoXml;
	signedInfoXml = NULL;
	signature->signatureValue = signatureValue;
	signatureValue = NULL;
	if (keyName != NULL) {
		signature->keyInfoType = MLN_XMLDSIG_KEYINFO_KEYNAME;
		signature->keyInfo.keyName = keyName;
		keyName = NULL;
	} else if (subjectKeyIdentifier != NULL) {
		signature->keyInfo.keyName = subjectKeyIdentifier;
		signature->keyInfoType = MLN_XMLDSIG_KEYINFO_X509SKI;
		subjectKeyIdentifier = NULL;
	} else {
		signature->keyInfo.x509.certificates = certificates;
		certificates = NULL;
		signature->keyInfoType = MLN_XMLDSIG_KEYINFO_X509;
		signature->keyInfo.x509.count = certificateCount;
	}
	*out = signature;
	references = NULL;
	goto out;
format:
	ret = MLN_XMLDSIG_ERROR_FORMAT;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	MlnBufferDestroy(MLN_HEAP_GENERAL, canonicalizationMethod);
	MlnBufferDestroy(MLN_HEAP_GENERAL, signatureMethod);
	MlnBufferDestroy(MLN_HEAP_GENERAL, uri);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnBufferDestroy(MLN_HEAP_GENERAL, signedInfoXml);
	MlnBufferDestroy(MLN_HEAP_GENERAL, signatureValue);
	MlnBufferDestroy(MLN_HEAP_GENERAL, keyName);
	MlnBufferDestroy(MLN_HEAP_GENERAL, subjectKeyIdentifier);
	MlnBufferDestroy(MLN_HEAP_GENERAL, attribute);
	MlnBufferDestroy(MLN_HEAP_GENERAL, tokenId);
	if (certificates != NULL) {
		for (i = 0; i < certificateCount; i++) {
			if (certificates[i] != NULL)
				MlnBufferDestroy(MLN_HEAP_GENERAL, certificates[i]);
		}
		MlnFree(MLN_HEAP_GENERAL, certificates);
	}
	if (elements != NULL)
		MlnXmlListFree(MLN_HEAP_GENERAL, elements);
	if (children != NULL)
		MlnXmlListFree(MLN_HEAP_GENERAL, children);
	if (values != NULL)
		MlnXmlListFree(MLN_HEAP_GENERAL, values);
	if (references != NULL) {
		for (i = 0; i < referenceCount; i++)
			MlnImpl_8105D018(&references[i]);
		MlnFree(MLN_HEAP_GENERAL, references);
	}
	return ret;
}

/*
 * The index of the id a Reference's URI names, "#<id>", the id itself or an XPointer
 * "#xmlns(...)...[@Id='<id>']"; -1 if none.
 */
int MlnImpl_8105B140(const MlnBuffer *uri, int count, MlnBuffer *const *ids)
{
	const MlnBuffer *id;
	const char *data;
	const char *p;
	int i;

	for (i = 0; i < count; i++) {
		id = ids[i];
		if (id == NULL)
			continue;
		data = (const char *)uri->data;
		if (data[0] == '#' && uri->size == id->size + 1 &&
		    MlnMemcmp(data + 1, id->data, id->size) == 0)
			return i;
		if (MlnBufferCompare(MLN_HEAP_GENERAL, uri, ids[i]) == 0)
			return i;
		if (MlnMemcmp(data, "#xmlns(", 7) != 0)
			continue;
		p = MlnStrstr(data + 7, "[@Id='");
		if (p == NULL)
			continue;
		if (MlnMemcmp(p + 6, ids[i]->data, ids[i]->size) == 0)
			return i;
	}
	return -1;
}

#define MLN_XMLDSIG_EXC_C14N	"http://www.w3.org/2001/10/xml-exc-c14n#"
#define MLN_XMLDSIG_RSA_SHA1	"http://www.w3.org/2000/09/xmldsig#rsa-sha1"
#define MLN_XMLDSIG_RSA_SHA256	"http://www.w3.org/2001/04/xmldsig-more#rsa-sha256"
#define MLN_XMLDSIG_HMAC_SHA1	"http://www.w3.org/2000/09/xmldsig#hmac-sha1"
#define MLN_XMLDSIG_HMAC_SHA256	"http://www.w3.org/2001/04/xmldsig-more#hmac-sha256"

/* whether a buffer holds a string literal */
#define MLN_XMLDSIG_IS(buffer, literal) \
	((buffer)->size == sizeof(literal) - 1 && \
	 MlnMemcmp((buffer)->data, literal, sizeof(literal) - 1) == 0)

/*
 * A signature's method, for exclusive canonicalization only: 0 RSA-SHA1, 1 RSA-SHA256
 * (with certificates or a key identifier), 2 HMAC-SHA1, 3 HMAC-SHA256 (with a key name);
 * -1 otherwise.
 */
int MlnImpl_8105B230(const MlnXmlDsigSignature *signature)
{
	if (!MLN_XMLDSIG_IS(signature->canonicalizationMethod, MLN_XMLDSIG_EXC_C14N))
		return -1;
	if (MLN_XMLDSIG_IS(signature->signatureMethod, MLN_XMLDSIG_RSA_SHA1) &&
	    (signature->keyInfoType == MLN_XMLDSIG_KEYINFO_X509 ||
	     signature->keyInfoType == MLN_XMLDSIG_KEYINFO_X509SKI))
		return 0;
	if (MLN_XMLDSIG_IS(signature->signatureMethod, MLN_XMLDSIG_RSA_SHA256) &&
	    (signature->keyInfoType == MLN_XMLDSIG_KEYINFO_X509 ||
	     signature->keyInfoType == MLN_XMLDSIG_KEYINFO_X509SKI))
		return 1;
	if (MLN_XMLDSIG_IS(signature->signatureMethod, MLN_XMLDSIG_HMAC_SHA1) &&
	    signature->keyInfoType == MLN_XMLDSIG_KEYINFO_KEYNAME)
		return 2;
	if (MLN_XMLDSIG_IS(signature->signatureMethod, MLN_XMLDSIG_HMAC_SHA256) &&
	    signature->keyInfoType == MLN_XMLDSIG_KEYINFO_KEYNAME)
		return 3;
	return -1;
}

#define MLN_XMLDSIG_ERROR_REFERENCE	0x220A0001

#define MLN_XMLDSIG_SHA1		"http://www.w3.org/2000/09/xmldsig#sha1"
#define MLN_XMLDSIG_SHA256		"http://www.w3.org/2001/04/xmlenc#sha256"
#define MLN_XMLDSIG_ENVELOPED		"http://www.w3.org/2000/09/xmldsig#enveloped-signature"
#define MLN_OCTOPUS_CBS			"http://www.octopus-drm.com/octopus/specs/cbs-1_0"
#define MLN_OCTOPUS_CBS_INTERTRUST	"http://www.intertrust.com/Octopus/xmldsig#cbs-1_0"

/*
 * Check a Reference: its transforms are those of a kind (0 none, 1 exclusive
 * canonicalization, 2 enveloped signature, 3 both, 4 Octopus's canonical byte sequence),
 * and its digest value is the one given.
 */
int MlnImpl_8105B364(const MlnXmlDsigReference *reference, int kind, int digestSize,
		     const unsigned char *digest)
{
	const MlnBuffer *method;

	if (reference == NULL)
		return 3;
	switch (kind) {
	case 0:
digest:
		/*
		 * Sony's test of the digest method fails only for a method that is both SHA-1 and
		 * SHA-256, which none is: every method passes.
		 */
		method = reference->digestMethod;
		if (MlnBytesCompare(MLN_HEAP_GENERAL, sizeof(MLN_XMLDSIG_SHA1) - 1,
				    MLN_XMLDSIG_SHA1, method->size, method->data) == 0 &&
		    MlnBytesCompare(MLN_HEAP_GENERAL, sizeof(MLN_XMLDSIG_SHA256) - 1,
				    MLN_XMLDSIG_SHA256, reference->digestMethod->size,
				    reference->digestMethod->data) == 0)
			return MLN_XMLDSIG_ERROR_REFERENCE;
		if (reference->digestValue->size != (unsigned int)digestSize)
			return MLN_XMLDSIG_ERROR_REFERENCE;
		if (MlnMemcmp(reference->digestValue->data, digest, digestSize) != 0)
			return MLN_XMLDSIG_ERROR_REFERENCE;
		return 0;
	case 1:
		if (reference->transformCount != 1 ||
		    !MLN_XMLDSIG_IS(reference->transforms[0], MLN_XMLDSIG_EXC_C14N))
			return MLN_XMLDSIG_ERROR_REFERENCE;
		goto digest;
	case 2:
		if (reference->transformCount != 1 ||
		    !MLN_XMLDSIG_IS(reference->transforms[0], MLN_XMLDSIG_ENVELOPED))
			return MLN_XMLDSIG_ERROR_REFERENCE;
		goto digest;
	case 3:
		if (reference->transformCount != 2 ||
		    !MLN_XMLDSIG_IS(reference->transforms[0], MLN_XMLDSIG_ENVELOPED) ||
		    !MLN_XMLDSIG_IS(reference->transforms[1], MLN_XMLDSIG_EXC_C14N))
			return MLN_XMLDSIG_ERROR_REFERENCE;
		goto digest;
	case 4:
		if (reference->transformCount != 1)
			return MLN_XMLDSIG_ERROR_REFERENCE;
		if (!MLN_XMLDSIG_IS(reference->transforms[0], MLN_OCTOPUS_CBS) &&
		    !MLN_XMLDSIG_IS(reference->transforms[0], MLN_OCTOPUS_CBS_INTERTRUST))
			return MLN_XMLDSIG_ERROR_REFERENCE;
		goto digest;
	default:
		return 3;
	}
}

#define MLN_XMLDSIG_ERROR_METHOD	0x220A0002
#define MLN_XMLDSIG_ERROR_NO_REFERENCE	0x220A0003

/*
 * Verify a Signature element with RSA: each of its References that names one of the ids
 * must have that id's transforms and digest (verified[i] is cleared for each id
 * checked), and the SignedInfo must be signed by the key of its certificate chain, valid
 * for a usage, or by the certificate given whose key identifier the KeyInfo names; the
 * signer's subject is returned.
 */
int MlnImpl_8105B568(MlnXmlNode *signature, int usage, int count, MlnBuffer *const *references,
		     unsigned char **digests, int *digestTypes, const MlnBuffer *certificate,
		     MlnBuffer **signer, int *verified)
{
	MlnXmlDsigSignature *parsed = NULL;
	MlnBuffer *signedInfo = NULL;
	MlnBuffer *keyIdentifier = NULL;
	MlnBuffer *modulus = NULL;
	MlnBuffer *exponent = NULL;
	MlnBuffer *subject = NULL;
	MlnX509 *x509 = NULL;
	MlnXmlNodeList *list = NULL;
	MlnX509Certificates chain = { 0, NULL };
	MlnXmlDsigReference *reference;
	void *rsa = NULL;
	int hash;
	int digestSize;
	int referenceCount;
	int found;
	int index;
	int ret;
	int i;

	ret = MlnImpl_81059934(signature, &parsed);
	if (ret != 0)
		goto out;
	hash = MlnImpl_8105B230(parsed);
	if (hash == 0) {
		digestSize = 20;
	} else if (hash == 1) {
		digestSize = 32;
	} else {
		ret = MLN_XMLDSIG_ERROR_METHOD;
		goto out;
	}

	for (i = 0; i < count; i++)
		verified[i] = 1;
	referenceCount = parsed->referenceCount;
	if (referenceCount <= 0) {
		ret = MLN_XMLDSIG_ERROR_NO_REFERENCE;
		goto out;
	}
	found = 0;
	for (i = 0; i < referenceCount; i++) {
		reference = &parsed->references[i];
		index = MlnImpl_8105B140(reference->uri, count, references);
		if (index < 0)
			continue;
		found = 1;
		ret = MlnImpl_8105B364(reference, digestTypes[index], digestSize, digests[index]);
		if (ret != 0)
			goto out;
		verified[index] = 0;
	}
	if (!found) {
		ret = MLN_XMLDSIG_ERROR_NO_REFERENCE;
		goto out;
	}

	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, signature, "SignedInfo", MLN_XMLDSIG_NAMESPACE,
			      &list);
	if (ret != 0)
		goto out;
	if (list->count != 1)
		goto invalid;
	ret = MlnXmlNodeToXml(MLN_HEAP_GENERAL, list->nodes[0], &signedInfo);
	if (ret != 0)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	list = NULL;

	if (parsed->keyInfoType == MLN_XMLDSIG_KEYINFO_X509) {
		/* the chain from the anchor's child down */
		chain.count = parsed->keyInfo.x509.count;
		chain.items = MlnAllocate(MLN_HEAP_GENERAL, chain.count * sizeof(MlnBuffer *));
		if (chain.items == NULL) {
			ret = 2;
			goto out;
		}
		for (i = 0; i < chain.count; i++)
			chain.items[i] = parsed->keyInfo.x509.certificates[chain.count - 1 - i];
		ret = MlnImpl_81057314(usage, &chain);
		if (ret != 0)
			goto fail;
		MlnFree(MLN_HEAP_GENERAL, chain.items);
		chain.items = NULL;
		ret = MlnImpl_81038EF4(parsed->keyInfo.x509.certificates[0], &x509);
		if (ret != 0)
			goto fail;
		ret = MlnImpl_81039228(x509, &modulus, &exponent);
		if (ret != 0)
			goto fail;
		if (signer != NULL) {
			ret = MlnImpl_81039298(x509, &subject);
			if (ret != 0)
				goto fail;
			*signer = MlnBufferCreate(MLN_HEAP_GENERAL, subject->size, subject->data);
		}
	} else {
		ret = MlnImpl_81038EF4(certificate, &x509);
		if (ret != 0)
			goto fail;
		ret = MlnImpl_810390C0(x509, &keyIdentifier);
		if (ret != 0)
			goto fail;
		if (MlnBufferCompare(MLN_HEAP_GENERAL, keyIdentifier, parsed->keyInfo.keyName) != 0)
			goto invalid;
		MlnBufferDestroy(MLN_HEAP_GENERAL, keyIdentifier);
		keyIdentifier = NULL;
		ret = MlnImpl_81039228(x509, &modulus, &exponent);
		if (ret != 0)
			goto fail;
	}
	MlnImpl_81038E90(x509);
	x509 = NULL;

	ret = MlnImpl_81055670(&rsa, modulus, exponent, NULL, NULL, NULL, NULL, NULL, NULL);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_81056F0C(rsa, hash, signedInfo, parsed->signatureValue);
	if (ret != 0)
		goto fail;
	MlnImpl_81055ABC(rsa);
	rsa = NULL;
	goto out;
fail:
	if ((ret & 0xFFFF0000) == 0)
		goto out;
invalid:
	ret = MLN_XMLDSIG_ERROR_REFERENCE;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, signedInfo);
	MlnBufferDestroy(MLN_HEAP_GENERAL, keyIdentifier);
	MlnBufferDestroy(MLN_HEAP_GENERAL, modulus);
	MlnBufferDestroy(MLN_HEAP_GENERAL, exponent);
	MlnBufferDestroy(MLN_HEAP_GENERAL, subject);
	MlnFree(MLN_HEAP_GENERAL, chain.items);
	if (rsa != NULL)
		MlnImpl_81055ABC(rsa);
	if (x509 != NULL)
		MlnImpl_81038E90(x509);
	if (parsed != NULL)
		MlnImpl_8105D0AC(parsed);
	if (list != NULL)
		MlnXmlListFree(MLN_HEAP_GENERAL, list);
	return ret;
}

/* verify a Signature element as MlnImpl_8105B568 does, without the signer */
int MlnImpl_8105BA14(MlnXmlNode *signature, int usage, int count, MlnBuffer *const *references,
		     unsigned char **digests, int *digestTypes, const MlnBuffer *certificate,
		     int *verified)
{
	return MlnImpl_8105B568(signature, usage, count, references, digests, digestTypes,
				certificate, NULL, verified);
}

/*
 * Verify a Signature element of a document as MlnImpl_8105B568 does, its KeyInfo read by
 * MlnImpl_8105A3D8; the digests are SHA-1 or SHA-256 by the first matching Reference's
 * digest method.
 */
int MlnImpl_8105BA50(MlnXmlNode *signature, MlnXmlNode *document, int usage, int count,
		     MlnBuffer *const *references, unsigned char **digests, int *digestTypes,
		     const MlnBuffer *certificate, int *verified)
{
	MlnXmlDsigSignature *parsed = NULL;
	MlnBuffer *signedInfo = NULL;
	MlnBuffer *keyIdentifier = NULL;
	MlnBuffer *modulus = NULL;
	MlnBuffer *exponent = NULL;
	MlnX509 *x509 = NULL;
	MlnXmlNodeList *list = NULL;
	MlnX509Certificates chain = { 0, NULL };
	MlnXmlDsigReference *reference;
	const MlnBuffer *method;
	void *rsa = NULL;
	unsigned int hash;
	int digestSize;
	int referenceCount;
	int found;
	int index;
	int ret;
	int i;

	ret = MlnImpl_8105A3D8(signature, document, &parsed);
	if (ret != 0)
		goto out;
	hash = MlnImpl_8105B230(parsed);
	if (hash > 1) {
		ret = MLN_XMLDSIG_ERROR_METHOD;
		goto out;
	}

	for (i = 0; i < count; i++)
		verified[i] = 1;
	referenceCount = parsed->referenceCount;
	if (referenceCount <= 0) {
		ret = MLN_XMLDSIG_ERROR_NO_REFERENCE;
		goto out;
	}
	found = 0;
	digestSize = 0;
	for (i = 0; i < referenceCount; i++) {
		reference = &parsed->references[i];
		index = MlnImpl_8105B140(reference->uri, count, references);
		if (index < 0)
			continue;
		if (digestSize == 0) {
			method = reference->digestMethod;
			if (MlnBytesCompare(MLN_HEAP_GENERAL, method->size, method->data,
					    MlnStrlen(MLN_XMLDSIG_SHA256), MLN_XMLDSIG_SHA256) == 0) {
				digestSize = 32;
			} else {
				method = reference->digestMethod;
				digestSize = 20;
				if (MlnBytesCompare(MLN_HEAP_GENERAL, method->size, method->data,
						    MlnStrlen(MLN_XMLDSIG_SHA1),
						    MLN_XMLDSIG_SHA1) != 0)
					goto invalid;
			}
		}
		found = 1;
		ret = MlnImpl_8105B364(reference, digestTypes[index], digestSize, digests[index]);
		if (ret != 0)
			goto out;
		verified[index] = 0;
	}
	if (!found) {
		ret = MLN_XMLDSIG_ERROR_NO_REFERENCE;
		goto out;
	}

	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, signature, "SignedInfo", MLN_XMLDSIG_NAMESPACE,
			      &list);
	if (ret != 0)
		goto out;
	if (list->count != 1)
		goto invalid;
	ret = MlnXmlNodeToXml(MLN_HEAP_GENERAL, list->nodes[0], &signedInfo);
	if (ret != 0)
		goto out;
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	list = NULL;

	if (parsed->keyInfoType == MLN_XMLDSIG_KEYINFO_X509) {
		/* the chain from the anchor's child down */
		chain.count = parsed->keyInfo.x509.count;
		chain.items = MlnAllocate(MLN_HEAP_GENERAL, chain.count * sizeof(MlnBuffer *));
		if (chain.items == NULL) {
			ret = 2;
			goto out;
		}
		for (i = 0; i < chain.count; i++)
			chain.items[i] = parsed->keyInfo.x509.certificates[chain.count - 1 - i];
		ret = MlnImpl_81057314(usage, &chain);
		if (ret != 0)
			goto fail;
		MlnFree(MLN_HEAP_GENERAL, chain.items);
		chain.items = NULL;
		ret = MlnImpl_81038EF4(parsed->keyInfo.x509.certificates[0], &x509);
		if (ret != 0)
			goto fail;
		ret = MlnImpl_81039228(x509, &modulus, &exponent);
		if (ret != 0)
			goto fail;
	} else {
		ret = MlnImpl_81038EF4(certificate, &x509);
		if (ret != 0)
			goto fail;
		ret = MlnImpl_810390C0(x509, &keyIdentifier);
		if (ret != 0)
			goto fail;
		if (MlnBufferCompare(MLN_HEAP_GENERAL, keyIdentifier, parsed->keyInfo.keyName) != 0)
			goto invalid;
		MlnBufferDestroy(MLN_HEAP_GENERAL, keyIdentifier);
		keyIdentifier = NULL;
		ret = MlnImpl_81039228(x509, &modulus, &exponent);
		if (ret != 0)
			goto fail;
	}
	MlnImpl_81038E90(x509);
	x509 = NULL;

	ret = MlnImpl_81055670(&rsa, modulus, exponent, NULL, NULL, NULL, NULL, NULL, NULL);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_81056F0C(rsa, hash != 0, signedInfo, parsed->signatureValue);
	if (ret != 0)
		goto fail;
	MlnImpl_81055ABC(rsa);
	rsa = NULL;
	goto out;
fail:
	if ((ret & 0xFFFF0000) == 0)
		goto out;
invalid:
	ret = MLN_XMLDSIG_ERROR_REFERENCE;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, signedInfo);
	MlnBufferDestroy(MLN_HEAP_GENERAL, keyIdentifier);
	MlnBufferDestroy(MLN_HEAP_GENERAL, modulus);
	MlnBufferDestroy(MLN_HEAP_GENERAL, exponent);
	MlnFree(MLN_HEAP_GENERAL, chain.items);
	if (rsa != NULL)
		MlnImpl_81055ABC(rsa);
	if (x509 != NULL)
		MlnImpl_81038E90(x509);
	if (parsed != NULL)
		MlnImpl_8105D0AC(parsed);
	if (list != NULL)
		MlnXmlListFree(MLN_HEAP_GENERAL, list);
	return ret;
}

#define MLN_XMLDSIG_ERROR_DIGEST	0x220A0006

/*
 * The digest size of a Signature element: its first Reference's SHA-1 or SHA-256 digest,
 * which its RSA signature method must match.
 */
int MlnImpl_8105BF78(MlnXmlNode *signature, int *digestSize)
{
	MlnBuffer *digestMethod = NULL;
	MlnBuffer *signatureMethod = NULL;
	MlnXmlNodeList *references = NULL;
	MlnXmlNode *method = NULL;
	MlnXmlNode *signedInfo = NULL;
	MlnXmlNode *signatureNode = NULL;
	int ret;

	if (signature == NULL || digestSize == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, signature, "SignedInfo", MLN_XMLDSIG_NAMESPACE,
			     &signedInfo);
	if (ret != 0)
		goto out;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, signedInfo, "Reference", MLN_XMLDSIG_NAMESPACE,
			      &references);
	if (ret != 0)
		goto out;
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, references->nodes[0], "DigestMethod",
			     MLN_XMLDSIG_NAMESPACE, &method);
	if (ret != 0)
		goto out;
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, method, NULL, "Algorithm", &digestMethod);
	if (ret != 0)
		goto out;
	if (MlnBytesCompare(MLN_HEAP_GENERAL, digestMethod->size, digestMethod->data,
			    MlnStrlen(MLN_XMLDSIG_SHA256), MLN_XMLDSIG_SHA256) == 0) {
		*digestSize = 32;
	} else {
		if (MlnBytesCompare(MLN_HEAP_GENERAL, digestMethod->size, digestMethod->data,
				    MlnStrlen(MLN_XMLDSIG_SHA1), MLN_XMLDSIG_SHA1) != 0)
			goto invalid;
		*digestSize = 20;
	}
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, signedInfo, "SignatureMethod",
			     MLN_XMLDSIG_NAMESPACE, &signatureNode);
	if (ret != 0)
		goto out;
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, signatureNode, NULL, "Algorithm",
				 &signatureMethod);
	if (ret != 0)
		goto out;
	if (MlnBytesCompare(MLN_HEAP_GENERAL, signatureMethod->size, signatureMethod->data,
			    MlnStrlen(MLN_XMLDSIG_RSA_SHA256), MLN_XMLDSIG_RSA_SHA256) == 0) {
		if (*digestSize != 32)
			goto invalid;
	} else if (MlnBytesCompare(MLN_HEAP_GENERAL, signatureMethod->size,
				   signatureMethod->data, MlnStrlen(MLN_XMLDSIG_RSA_SHA1),
				   MLN_XMLDSIG_RSA_SHA1) != 0 || *digestSize != 20) {
		goto invalid;
	}
	goto out;
invalid:
	ret = MLN_XMLDSIG_ERROR_DIGEST;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, digestMethod);
	MlnBufferDestroy(MLN_HEAP_GENERAL, signatureMethod);
	MlnXmlListFree(MLN_HEAP_GENERAL, references);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, method);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, signedInfo);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, signatureNode);
	return ret;
}

/* free the nodes left in a build's contents */
static inline void MlnXmlDsigFreeContents(MlnXmlBuildContent *contents)
{
	MlnXmlBuildContent *content;

	for (content = contents; content < contents + 8; content++) {
		if (content->kind == MLN_XML_BUILD_NODE && content->value.node != NULL)
			MlnXmlNodeFree(MLN_HEAP_GENERAL, content->value.node);
	}
}

/* how MlnImpl_8105C234 names the signer's key */
#define MLN_XMLDSIG_BUILD_SKI		0	/* an X509Data's X509SKI */
#define MLN_XMLDSIG_BUILD_REFERENCE	(-1)	/* a token by its wsse:Reference URI */
#define MLN_XMLDSIG_BUILD_KEY_ID	(-2)	/* a wsse:KeyIdentifier of the SKI */

#define MLN_WSSE_BASE64 \
	"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0" \
	"#Base64Binary"

/*
 * Build a Signature element of a SignedInfo and its signature value, with a KeyInfo of
 * keyInfo certificates (> 0), an SKI or a security token reference; id is its Id, if any.
 */
int MlnImpl_8105C234(MlnXmlNode *signedInfo, const MlnBuffer *signatureValue, int keyInfo,
		     MlnBuffer **certificates, const char *id, MlnXmlNode **out)
{
	MlnXmlNode *signature = NULL;
	MlnXmlNode *value = NULL;
	MlnXmlNode *key = NULL;
	MlnBuffer *text = NULL;
	MlnXmlBuildContent content;
	MlnXmlBuildAttribute attributes[2];
	MlnXmlBuildContent contents[8];
	int count;
	int ret;
	int i;

	if (signedInfo == NULL || signatureValue == NULL) {
		ret = 3;
		goto out;
	}
	if (certificates == NULL || out == NULL) {
		ret = 3;
		goto out;
	}
	MlnMemset(contents, 0, sizeof(contents));
	ret = MlnImpl_81026814(MLN_HEAP_GENERAL, signatureValue, 0, 0, &text);
	if (ret != 0)
		goto out;
	contents[0].kind = MLN_XML_BUILD_BUFFER;
	contents[0].value.buffer = text;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "SignatureValue", "ds", NULL, 0, NULL, 1,
				 contents, 1, &value);
	if (ret != 0)
		goto out;
	text = NULL;

	if (keyInfo < 0) {
		if (keyInfo == MLN_XMLDSIG_BUILD_KEY_ID) {
			attributes[0].name = "ValueType";
			attributes[0].value = MLN_WSSE_X509_SKI;
			attributes[1].name = "EncodingType";
			attributes[1].value = MLN_WSSE_BASE64;
			content.kind = MLN_XML_BUILD_BUFFER;
			content.value.buffer = certificates[0];
			ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "KeyIdentifier", "wsse", NULL, 2,
						 attributes, 1, &content, 0, &key);
			if (ret != 0)
				goto out;
		} else {
			attributes[0].name = "URI";
			attributes[0].value = (const char *)certificates[0]->data;
			ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Reference", "wsse", NULL, 1,
						 attributes, 0, NULL, 0, &key);
			if (ret != 0)
				goto out;
		}
		contents[0].kind = MLN_XML_BUILD_NODE;
		contents[0].value.node = key;
		ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "SecurityTokenReference", "wsse",
					 MLN_WSSE_NAMESPACE, 0, NULL, 1, contents, 1, &key);
		if (ret != 0)
			goto out;
	} else {
		count = keyInfo;
		if (count != 0) {
			for (i = 0; i < count; i++) {
				contents[0].kind = MLN_XML_BUILD_BUFFER;
				contents[0].value.buffer = certificates[i];
				ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "X509Certificate", "ds",
							 NULL, 0, NULL, 1, contents, 0,
							 &contents[i + 1].value.node);
				if (ret != 0)
					goto out;
				contents[i + 1].kind = MLN_XML_BUILD_NODE;
			}
		} else {
			/* Sony leaves the X509SKI's content kind a string */
			contents[0].kind = MLN_XML_BUILD_BUFFER;
			contents[0].value.buffer = certificates[0];
			ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "X509SKI", "ds", NULL, 0, NULL, 1,
						 contents, 0, &contents[1].value.node);
			if (ret != 0)
				goto out;
			count = 1;
		}
		ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "X509Data", "ds", NULL, 0, NULL, count,
					 &contents[1], 1, &key);
		if (ret != 0)
			goto out;
	}
	contents[0].kind = MLN_XML_BUILD_NODE;
	contents[0].value.node = key;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "KeyInfo", "ds", NULL, 0, NULL, 1, contents, 1,
				 &key);
	if (ret != 0)
		goto out;

	contents[0].kind = MLN_XML_BUILD_NODE;
	contents[0].value.node = signedInfo;
	contents[1].kind = MLN_XML_BUILD_NODE;
	contents[1].value.node = value;
	contents[2].kind = MLN_XML_BUILD_NODE;
	contents[2].value.node = key;
	if (id != NULL) {
		attributes[0].name = "Id";
		attributes[0].value = id;
	}
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Signature", "ds", MLN_XMLDSIG_NAMESPACE,
				 id != NULL, attributes, 3, contents, 0, &signature);
	if (ret != 0)
		goto out;
	*out = signature;
	signature = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	if (signature != NULL)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, signature);
	if (value != NULL)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, value);
	if (key != NULL)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, key);
	if (ret != 0)
		MlnXmlDsigFreeContents(contents);
	return ret;
}

/*
 * Build a SignedInfo element of count References: exclusive canonicalization, RSA with
 * SHA-1 (hash 0) or SHA-256 (1), and for each reference its URI, its transforms (0
 * exclusive canonicalization, 1 the enveloped signature and canonicalization) and its
 * digest.
 */
int MlnImpl_8105C7F0(int count, int hash, const int *transforms, const char *const *uris,
		     unsigned char **digests, MlnXmlNode **out)
{
	MlnXmlNode *signedInfo = NULL;
	MlnXmlNode *first = NULL;
	MlnXmlNode *second = NULL;
	MlnXmlNode *digestValue = NULL;
	MlnBuffer *unused = NULL;
	MlnBuffer *text = NULL;
	MlnBuffer digest;
	MlnXmlBuildAttribute attribute;
	MlnXmlBuildContent contents[8];
	const char *digestMethod;
	unsigned int digestSize;
	int ret;
	int i;

	if (transforms == NULL || uris == NULL) {
		ret = 3;
		goto out;
	}
	if (digests == NULL || out == NULL) {
		ret = 3;
		goto out;
	}
	MlnMemset(contents, 0, sizeof(contents));
	attribute.name = "Algorithm";
	attribute.value = MLN_XMLDSIG_EXC_C14N;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "CanonicalizationMethod", "ds", NULL, 1,
				 &attribute, 0, NULL, 0, &signedInfo);
	if (ret != 0)
		goto out;
	attribute.name = "Algorithm";
	if (hash == 0) {
		digestSize = 20;
		attribute.value = MLN_XMLDSIG_RSA_SHA1;
		digestMethod = MLN_XMLDSIG_SHA1;
	} else if (hash == 1) {
		digestMethod = MLN_XMLDSIG_SHA256;
		digestSize = 32;
		attribute.value = MLN_XMLDSIG_RSA_SHA256;
	} else {
		ret = MLN_XMLDSIG_ERROR_METHOD;
		goto out;
	}
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "SignatureMethod", "ds", NULL, 1, &attribute,
				 0, NULL, 0, &first);
	if (ret != 0)
		goto out;
	contents[0].kind = MLN_XML_BUILD_NODE;
	contents[0].value.node = signedInfo;
	contents[1].kind = MLN_XML_BUILD_NODE;
	contents[1].value.node = first;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "SignedInfo", "ds", MLN_XMLDSIG_NAMESPACE, 0,
				 NULL, 2, contents, 1, &signedInfo);
	if (ret != 0)
		goto out;
	first = NULL;

	for (i = 0; i < count; i++) {
		switch (transforms[i]) {
		case 0:
			attribute.name = "Algorithm";
			attribute.value = MLN_XMLDSIG_EXC_C14N;
			ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Transform", "ds", NULL, 1,
						 &attribute, 0, NULL, 0, &first);
			if (ret != 0)
				goto out;
			contents[0].kind = MLN_XML_BUILD_NODE;
			contents[0].value.node = first;
			ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Transforms", "ds", NULL, 0, NULL,
						 1, contents, 1, &first);
			if (ret != 0)
				goto out;
			break;
		case 1:
			attribute.value = MLN_XMLDSIG_ENVELOPED;
			attribute.name = "Algorithm";
			ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Transform", "ds", NULL, 1,
						 &attribute, 0, NULL, 0, &first);
			if (ret != 0)
				goto out;
			attribute.name = "Algorithm";
			attribute.value = MLN_XMLDSIG_EXC_C14N;
			ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Transform", "ds", NULL, 1,
						 &attribute, 0, NULL, 0, &second);
			if (ret != 0)
				goto out;
			contents[0].kind = MLN_XML_BUILD_NODE;
			contents[0].value.node = first;
			contents[1].kind = MLN_XML_BUILD_NODE;
			contents[1].value.node = second;
			ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Transforms", "ds", NULL, 0, NULL,
						 2, contents, 1, &first);
			if (ret != 0)
				goto out;
			break;
		case 2:
			ret = MLN_XMLDSIG_ERROR_METHOD;
			goto out;
		default:
			ret = 3;
			goto out;
		}
		attribute.value = digestMethod;
		attribute.name = "Algorithm";
		ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "DigestMethod", "ds", NULL, 1, &attribute,
					 0, NULL, 0, &second);
		if (ret != 0)
			goto out;
		digest.size = digestSize;
		digest.data = digests[i];
		ret = MlnImpl_81026814(MLN_HEAP_GENERAL, &digest, 0, 0, &text);
		if (ret != 0)
			goto out;
		contents[0].kind = MLN_XML_BUILD_BUFFER;
		contents[0].value.buffer = text;
		ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "DigestValue", "ds", NULL, 0, NULL, 1,
					 contents, 1, &digestValue);
		if (ret != 0)
			goto out;
		text = NULL;
		attribute.name = "URI";
		attribute.value = uris[i];
		contents[0].kind = MLN_XML_BUILD_NODE;
		contents[0].value.node = first;
		contents[1].kind = MLN_XML_BUILD_NODE;
		contents[1].value.node = second;
		contents[2].kind = MLN_XML_BUILD_NODE;
		contents[2].value.node = digestValue;
		ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Reference", "ds", NULL, 1, &attribute, 3,
					 contents, 1, &first);
		if (ret != 0)
			goto out;
		digestValue = NULL;
		second = NULL;
		ret = MlnXmlNodeAppendChild(MLN_HEAP_GENERAL, signedInfo, first, 1);
		if (ret != 0)
			goto out;
		first = NULL;
	}
	second = NULL;
	*out = signedInfo;
	signedInfo = NULL;
	first = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, unused);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	if (signedInfo != NULL)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, signedInfo);
	if (first != NULL)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, first);
	if (second != NULL)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, second);
	if (digestValue != NULL)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, digestValue);
	if (ret != 0)
		MlnXmlDsigFreeContents(contents);
	return ret;
}

/*
 * Sign references with an RSA key: their SignedInfo, its signature and the KeyInfo
 * (MlnImpl_8105C234's) as a Signature element.
 */
int MlnImpl_8105CF28(int count, int hash, const int *transforms, const char *const *uris,
		     unsigned char **digests, void *rsa, int keyInfo, MlnBuffer **certificates,
		     const char *id, MlnXmlNode **out)
{
	MlnXmlNode *signature = NULL;
	MlnBuffer *value = NULL;
	MlnBuffer *xml = NULL;
	MlnXmlNode *signedInfo = NULL;
	unsigned int ret;

	ret = MlnImpl_8105C7F0(count, hash, transforms, uris, digests, &signedInfo);
	if (ret != 0)
		goto out;
	ret = MlnXmlNodeToXml(MLN_HEAP_GENERAL, signedInfo, &xml);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81056A74(rsa, hash, xml, &value);
	if (ret != 0) {
		if ((ret >> 16) != 0)
			ret = 5;
		goto out;
	}
	ret = MlnImpl_8105C234(signedInfo, value, keyInfo, certificates, id, &signature);
	if (ret == 0) {
		*out = signature;
		signature = NULL;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	MlnBufferDestroy(MLN_HEAP_GENERAL, xml);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, signature);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, signedInfo);
	return ret;
}
