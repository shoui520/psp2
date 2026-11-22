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
