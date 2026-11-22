/*
 * NEMO's secure messages (0x81072138-): a service exchange's SOAP message
 * made with its WS-Security header (the protocol declaration, nonces, a
 * timestamp, the recipient, the sender's certificates and assertions), signed
 * with XML-DSig and encrypted with XML-Enc under a message key the
 * recipient's public key wraps, and such a message read back. The file's
 * name is ours.
 */
#include "mln_internal.h"

/* what a step of a kind of message carries (MlnNemoUsage) */
#define MLN_NEMO_USAGE_SIGNED		0x01	/* the parts are signed */
#define MLN_NEMO_USAGE_FRESH		0x02	/* timestamped, the peer's nonce returned */
#define MLN_NEMO_USAGE_TIMESTAMP	0x04	/* a timestamp is required */
#define MLN_NEMO_USAGE_ENCRYPTED	0x08	/* the body under a message key */
#define MLN_NEMO_USAGE_NONE		0x80	/* the kind has no such step */

/* the slots of a message's Security header, signed in MlnNemoSignedParts' order */
#define MLN_NEMO_SLOT_PROTOCOL		0	/* the ProtocolDeclaration */
#define MLN_NEMO_SLOT_PROFILE		1
#define MLN_NEMO_SLOT_FAULT		2	/* a fault's details, signed, sent outside */
#define MLN_NEMO_SLOT_RETURNED_NONCE	3
#define MLN_NEMO_SLOT_NONCE		4
#define MLN_NEMO_SLOT_TIMESTAMP		5
#define MLN_NEMO_SLOT_TO_NODE		6
#define MLN_NEMO_SLOT_ENCRYPTED_KEY	8
#define MLN_NEMO_SLOT_MESSAGE_KEY	9	/* the message key's token, encrypted */
#define MLN_NEMO_SLOT_CERTIFICATES	10	/* a request's second certificate chain */
#define MLN_NEMO_SLOT_SIGNER		11	/* the signer's certificate chain */
#define MLN_NEMO_SLOT_ASSERTION		14	/* the sender's role assertion */
#define MLN_NEMO_SLOT_ROLE		15
#define MLN_NEMO_SLOT_NODE_ASSERTIONS	16	/* the nodes' assertions, at most 15 */
#define MLN_NEMO_SLOT_NODE_ROLES	31
#define MLN_NEMO_SLOT_SIGNATURE		46
#define MLN_NEMO_SLOTS			52

#define MLN_NEMO_NODES_MAX		15
#define MLN_NEMO_HEADER_MAX		17
#define MLN_NEMO_REFERENCES_MAX		16
#define MLN_NEMO_DIGEST_MAX		32
#define MLN_NEMO_MESSAGE_KEY_SIZE	16
#define MLN_NEMO_FAULTS_MAX		64

#define MLN_NEMO_NS_SECURITY	"http://nemo.intertrust.com/2005/10/security"
#define MLN_NEMO_NS_CORE	"http://nemo.intertrust.com/2005/10/core"
#define MLN_NEMO_NS_EXCEPTIONS	"urn:marlin:core:1-2:nemo:services:schemas:exceptions"
#define MLN_NEMO_NS_SAML	"urn:oasis:names:tc:SAML:1.0:assertion"
#define MLN_NEMO_NS_SECEXT \
	"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd"
#define MLN_NEMO_NS_UTILITY \
	"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd"
#define MLN_NEMO_NS_DSIG	"http://www.w3.org/2000/09/xmldsig#"
#define MLN_NEMO_NS_XENC	"http://www.w3.org/2001/04/xmlenc#"
#define MLN_NEMO_BASE64 \
	"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary"
#define MLN_NEMO_PROTOCOL	"http://nemo.intertrust.com/2005/10/security/secure-protocol/basic/1.0"
#define MLN_NEMO_USAGE(part)	MLN_NEMO_PROTOCOL "#" part
#define MLN_NEMO_XENC_ELEMENT	"http://www.w3.org/2001/04/xmlenc#Element"
#define MLN_NEMO_XENC_CONTENT	"http://www.w3.org/2001/04/xmlenc#Content"

/* the usages of a step, by kind: request, response, confirmation, fault */
static const unsigned char MlnNemoUsage[15][4] = {
	{ 0x00, 0x00, 0x80, 0x00 },
	{ 0x00, 0x01, 0x80, 0x01 },
	{ 0x02, 0x07, 0x80, 0x07 },
	{ 0x02, 0x07, 0x80, 0x07 },
	{ 0x00, 0x01, 0x80, 0x01 },
	{ 0x07, 0x07, 0x07, 0x07 },
	{ 0x02, 0x07, 0x80, 0x07 },
	{ 0x07, 0x07, 0x07, 0x07 },
	{ 0x0f, 0x0f, 0x80, 0x0f },
	{ 0x0b, 0x0f, 0x80, 0x0f },
	{ 0x0f, 0x0f, 0x80, 0x0f },
	{ 0x0f, 0x0f, 0x80, 0x0f },
	{ 0x0f, 0x0f, 0x0f, 0x0f },
	{ 0x0f, 0x0f, 0x0f, 0x0f },
	{ 0x0f, 0x0f, 0x0f, 0x0f },
};

/* the usages of a step of a kind of message */
unsigned int MlnImpl_81072138(unsigned int kind, unsigned int step, unsigned char *usage)
{
	unsigned int index;

	switch (step) {
	case MLN_NEMO_STEP_REQUEST:
		index = 0;
		break;
	case MLN_NEMO_STEP_RESPONSE:
		index = 1;
		break;
	case MLN_NEMO_STEP_CONFIRMATION:
		index = 2;
		break;
	case MLN_NEMO_STEP_FAULT:
		index = 3;
		break;
	default:
		return MLN_NEMO_ERROR_STEP;
	}
	*usage = MlnNemoUsage[kind][index];
	if (*usage == MLN_NEMO_USAGE_NONE)
		return MLN_NEMO_ERROR_PARAMETER;
	return 0;
}

/* whether a reply of a kind of message returns the request's nonce */
int MlnImpl_810721BC(unsigned int kind, unsigned int step)
{
	unsigned char usage;

	if (step == MLN_NEMO_STEP_REQUEST || kind > 14)
		return 0;
	if (MlnImpl_81072138(kind, step, &usage) != 0)
		return 0;
	return (usage & MLN_NEMO_USAGE_FRESH) != 0;
}

/* a reference of the ProtocolDeclaration: the part a usage names */
static inline __attribute__((always_inline)) unsigned int MlnNemoReference(const char *usage,
									   const char *uri,
									   MlnXmlNode **out)
{
	MlnXmlBuildAttribute attributes[2];
	MlnXmlNode *node = NULL;
	unsigned int ret;

	attributes[0].name = "nemosec:TargetUsage";
	attributes[0].value = usage;
	attributes[1].name = "URI";
	attributes[1].value = uri;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Reference", "nemosec", MLN_NEMO_NS_SECURITY, 2,
				 attributes, 0, NULL, 0, &node);
	if (ret == 0) {
		*out = node;
		node = NULL;
	}
	MlnXmlNodeFree(MLN_HEAP_GENERAL, node);
	return ret;
}

/* the ProtocolDeclaration: the step, and the signature's and message key's references */
static inline __attribute__((always_inline)) unsigned int
MlnNemoProtocolDeclaration(unsigned int step, unsigned int usage, MlnXmlNode **out)
{
	MlnXmlBuildAttribute attributes[2];
	MlnXmlBuildAttribute type;
	MlnXmlBuildContent contents[3];
	MlnXmlNode *stepNode = NULL;
	MlnXmlNode *declaration = NULL;
	MlnXmlNode *integrity = NULL;
	MlnXmlNode *confidentiality = NULL;
	const char *target;
	unsigned int count;
	unsigned int ret;

	type.name = "Type";
	if (step == MLN_NEMO_STEP_RESPONSE)
		type.value = "response";
	else if (step == MLN_NEMO_STEP_CONFIRMATION)
		type.value = "confirmation";
	else if (step == MLN_NEMO_STEP_REQUEST)
		type.value = "request";
	else
		type.value = "fault";
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Step", "nemosec", NULL, 1, &type, 0, NULL, 0,
				 &stepNode);
	if (ret != 0)
		goto out;
	contents[0].kind = MLN_XML_BUILD_NODE;
	contents[0].value.node = stepNode;
	count = 1;
	if (usage & MLN_NEMO_USAGE_SIGNED) {
		if (step == MLN_NEMO_STEP_RESPONSE)
			target = MLN_NEMO_USAGE("response-signature");
		else if (step == MLN_NEMO_STEP_REQUEST)
			target = MLN_NEMO_USAGE("request-signature");
		else if (step == MLN_NEMO_STEP_CONFIRMATION)
			target = MLN_NEMO_USAGE("confirmation-signature");
		else
			target = MLN_NEMO_USAGE("fault-signature");
		ret = MlnNemoReference(target, "#NemoIntegrity", &integrity);
		if (ret != 0)
			goto out;
		contents[1].kind = MLN_XML_BUILD_NODE;
		contents[1].value.node = integrity;
		count = 2;
	}
	if (usage & MLN_NEMO_USAGE_ENCRYPTED) {
		if (step == MLN_NEMO_STEP_RESPONSE)
			target = MLN_NEMO_USAGE("response-encryptedMessageKey");
		else if (step == MLN_NEMO_STEP_REQUEST)
			target = MLN_NEMO_USAGE("request-encryptedMessageKey");
		else if (step == MLN_NEMO_STEP_CONFIRMATION)
			target = MLN_NEMO_USAGE("confirmation-encryptedMessageKey");
		else
			target = MLN_NEMO_USAGE("fault-encryptedMessageKey");
		ret = MlnNemoReference(target, "#NemoConfidentiality", &confidentiality);
		if (ret != 0)
			goto out;
		contents[count].kind = MLN_XML_BUILD_NODE;
		contents[count].value.node = confidentiality;
		count++;
	}
	attributes[0].name = "URI";
	attributes[0].value = MLN_NEMO_PROTOCOL;
	attributes[1].name = "nemosec:Usage";
	attributes[1].value = "http://nemo.intertrust.com/2005/10/security/secure-protocol";
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "ProtocolDeclaration", "nemosec",
				 MLN_NEMO_NS_SECURITY, 2, attributes, count, contents, 0,
				 &declaration);
	if (ret == 0) {
		*out = declaration;
		declaration = NULL;
	}
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, stepNode);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, integrity);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, confidentiality);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, declaration);
	return ret;
}

/* the profile: Marlin's NEMO protocol profile */
static inline __attribute__((always_inline)) unsigned int MlnNemoProfile(MlnXmlNode **out)
{
	MlnXmlBuildAttribute attributes[2];
	MlnXmlNode *node = NULL;
	unsigned int ret;

	attributes[0].name = "nemosec:Usage";
	attributes[0].value = "http://nemo.intertrust.com/2005/10/security/profile";
	attributes[1].name = "URI";
	attributes[1].value = "urn:marlin:core:1.0:nemo:protocol:profile:1";
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Profile", "nemosec", MLN_NEMO_NS_SECURITY, 2,
				 attributes, 0, NULL, 0, &node);
	if (ret == 0) {
		*out = node;
		node = NULL;
	}
	MlnXmlNodeFree(MLN_HEAP_GENERAL, node);
	return ret;
}

/* a ServiceException of a fault: its name, redirection and details */
static inline __attribute__((always_inline)) unsigned int
MlnNemoServiceException(const MlnNemoFault *fault, MlnXmlNode **out)
{
	MlnXmlBuildAttribute attribute;
	MlnXmlBuildContent contents[2];
	MlnXmlBuildContent text;
	MlnXmlNode *exception = NULL;
	MlnXmlNode *redirect = NULL;
	MlnXmlNode *details = NULL;
	char *name = NULL;
	unsigned int attributeCount;
	unsigned int count = 0;
	unsigned int size;
	unsigned int ret;

	if (fault->redirectUrl != NULL) {
		text.kind = MLN_XML_BUILD_BUFFER;
		text.value.buffer = fault->redirectUrl;
		ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "RedirectURL", "exc",
					 MLN_NEMO_NS_EXCEPTIONS, 0, NULL, 1, &text, 0, &redirect);
		if (ret != 0)
			goto out;
		contents[0].kind = MLN_XML_BUILD_NODE;
		contents[0].value.node = redirect;
		count = 1;
	}
	if (fault->details != NULL) {
		text.kind = MLN_XML_BUILD_BUFFER;
		text.value.buffer = fault->details;
		ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Details", "exc", MLN_NEMO_NS_EXCEPTIONS,
					 0, NULL, 1, &text, 0, &details);
		if (ret != 0)
			goto out;
		contents[count].kind = MLN_XML_BUILD_NODE;
		contents[count].value.node = details;
		count++;
	}
	if (fault->name != NULL) {
		size = fault->name->size;
		name = MlnAllocate(MLN_HEAP_GENERAL, size + 1);
		if (name == NULL) {
			ret = 2;
			goto out;
		}
		MlnMemcpy(name, fault->name->data, size);
		name[size] = '\0';
		attribute.name = "name";
		attribute.value = name;
		attributeCount = 1;
	} else {
		attributeCount = 0;
	}
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "ServiceException", "exc",
				 MLN_NEMO_NS_EXCEPTIONS, attributeCount, &attribute, count, contents,
				 0, &exception);
	if (ret == 0) {
		*out = exception;
		exception = NULL;
	}
out:
	MlnFree(MLN_HEAP_GENERAL, name);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, redirect);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, details);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, exception);
	return ret;
}

/* a fault's FaultDetails: a ServiceException for each of its exceptions */
static inline __attribute__((always_inline)) unsigned int
MlnNemoFaultDetails(const MlnNemoFaults *faults, MlnXmlNode **out)
{
	MlnXmlNode *exceptions[MLN_NEMO_FAULTS_MAX];
	MlnXmlBuildContent contents[MLN_NEMO_FAULTS_MAX];
	MlnXmlNode *details = NULL;
	unsigned int count = faults->count;
	unsigned int ret;
	unsigned int i;

	for (i = 0; i < count; i++)
		exceptions[i] = NULL;
	for (i = 0; ; ) {
		ret = MlnNemoServiceException(faults->items[i], &exceptions[i]);
		if (ret != 0)
			break;
		contents[i].kind = MLN_XML_BUILD_NODE;
		contents[i].value.node = exceptions[i];
		if (++i == count) {
			ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "FaultDetails", "nemoc",
						 MLN_NEMO_NS_CORE, 0, NULL, count, contents, 0,
						 &details);
			if (ret == 0) {
				*out = details;
				details = NULL;
			}
			break;
		}
	}
	MlnXmlNodeFree(MLN_HEAP_GENERAL, details);
	for (i = 0; i < count; i++)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, exceptions[i]);
	return ret;
}

/* the peer's nonce returned; a fault without one goes without */
static inline __attribute__((always_inline)) unsigned int
MlnNemoReturnedNonce(const MlnNemoMessage *message, unsigned int step, int fault,
		     MlnXmlNode **out)
{
	const char *usage;
	unsigned int ret;

	if (message->peerNonce == NULL || message->peerNonce->size > 0x80) {
		ret = MLN_NEMO_ERROR_PARAMETER;
	} else {
		if (step == MLN_NEMO_STEP_RESPONSE)
			usage = MLN_NEMO_USAGE("response-returnedNonce");
		else if (step == MLN_NEMO_STEP_CONFIRMATION)
			usage = MLN_NEMO_USAGE("confirmation-returnedNonce");
		else
			usage = MLN_NEMO_USAGE("fault-returnedNonce");
		ret = MlnImpl_810850D4(message->peerNonce, usage, out);
	}
	if (ret == MLN_NEMO_ERROR_PARAMETER && fault)
		return 0;
	return ret;
}

/* a nonce of 16 random bytes, kept to check the reply's */
static inline __attribute__((always_inline)) unsigned int
MlnNemoNonce(MlnNemoMessage *message, unsigned int step, MlnXmlNode **out)
{
	const char *usage;
	unsigned int ret;

	MlnBufferDestroy(MLN_HEAP_GENERAL, message->nonce);
	message->nonce = NULL;
	if (step == MLN_NEMO_STEP_RESPONSE) {
		usage = MLN_NEMO_USAGE("response-nonce");
	} else {
		usage = MLN_NEMO_USAGE("fault-nonce");
		if (step == MLN_NEMO_STEP_REQUEST)
			usage = MLN_NEMO_USAGE("request-nonce");
	}
	message->nonce = MlnBufferCreate(MLN_HEAP_GENERAL, 16, NULL);
	if (message->nonce == NULL)
		return 2;
	ret = MlnImpl_810790EC(message->nonce->size, message->nonce->data);
	if (ret != 0)
		return ret;
	return MlnImpl_810850D4(message->nonce, usage, out);
}

/* the Timestamp: the time the message is made */
static inline __attribute__((always_inline)) unsigned int MlnNemoTimestamp(unsigned int step,
									   MlnXmlNode **out)
{
	MlnXmlBuildAttribute attributes[2];
	MlnXmlBuildContent content;
	MlnXmlNode *created = NULL;
	MlnXmlNode *timestamp = NULL;
	char *time = NULL;
	const char *usage;
	unsigned int ret;

	if (MlnImpl_81079D78(&time) != 0) {
		ret = 5;
		goto out;
	}
	if (step == MLN_NEMO_STEP_RESPONSE) {
		usage = MLN_NEMO_USAGE("response-timestamp");
	} else if (step == MLN_NEMO_STEP_CONFIRMATION) {
		usage = MLN_NEMO_USAGE("confirmation-timestamp");
	} else {
		usage = MLN_NEMO_USAGE("fault-timestamp");
		if (step == MLN_NEMO_STEP_REQUEST)
			usage = MLN_NEMO_USAGE("request-timestamp");
	}
	content.kind = MLN_XML_BUILD_STRING;
	content.value.string = time;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Created", "wsu", MLN_NEMO_NS_UTILITY, 0, NULL,
				 1, &content, 0, &created);
	if (ret != 0)
		goto out;
	content.kind = MLN_XML_BUILD_NODE;
	content.value.node = created;
	attributes[0].name = "xmlns:nemosec";
	attributes[0].value = MLN_NEMO_NS_SECURITY;
	attributes[1].name = "nemosec:Usage";
	attributes[1].value = usage;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Timestamp", "wsu", MLN_NEMO_NS_UTILITY, 2,
				 attributes, 1, &content, 0, &timestamp);
	if (ret == 0) {
		*out = timestamp;
		timestamp = NULL;
	}
out:
	MlnFree(MLN_HEAP_GENERAL, time);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, created);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, timestamp);
	return ret;
}

/* the ToNode: the name of the node the message is for */
static inline __attribute__((always_inline)) unsigned int
MlnNemoToNode(MlnBuffer *peer, unsigned int step, MlnXmlNode **out)
{
	MlnXmlBuildAttribute attributes[2];
	MlnXmlBuildContent content;
	MlnXmlNode *node = NULL;
	const char *usage;
	unsigned int ret;

	if (step == MLN_NEMO_STEP_RESPONSE) {
		usage = MLN_NEMO_USAGE("response-toNode");
	} else if (step == MLN_NEMO_STEP_CONFIRMATION) {
		usage = MLN_NEMO_USAGE("confirmation-toNode");
	} else {
		usage = MLN_NEMO_USAGE("request-toNode");
		if (step != MLN_NEMO_STEP_REQUEST)
			usage = MLN_NEMO_USAGE("fault-toNode");
	}
	content.kind = MLN_XML_BUILD_BUFFER;
	content.value.buffer = peer;
	attributes[0].name = "xmlns:nemosec";
	attributes[0].value = MLN_NEMO_NS_SECURITY;
	attributes[1].name = "nemosec:Usage";
	attributes[1].value = usage;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "ToNode", "nemosec", MLN_NEMO_NS_SECURITY, 2,
				 attributes, 1, &content, 0, &node);
	if (ret == 0) {
		*out = node;
		node = NULL;
	}
	MlnXmlNodeFree(MLN_HEAP_GENERAL, node);
	return ret;
}

/* the OAEP parameters the message key is wrapped with: none */
#define MLN_NEMO_OAEP_PARAMS	""

/*
 * The EncryptedKey: the message key wrapped with RSA-OAEP under the
 * recipient's public key, the certificate named by its key identifier, with
 * the references to what the key encrypts.
 */
static inline __attribute__((always_inline)) unsigned int
MlnNemoEncryptedKey(const MlnNemoPeerKey *recipient, unsigned char *messageKey, MlnXmlNode **out)
{
	MlnXmlBuildAttribute attributes[2];
	MlnXmlBuildAttribute attribute;
	MlnXmlBuildContent methodContents[2];
	MlnXmlBuildContent contents[4];
	MlnXmlBuildContent content;
	MlnXmlNode *method = NULL;
	MlnXmlNode *parameters = NULL;
	MlnXmlNode *digest = NULL;
	MlnXmlNode *reference = NULL;
	MlnXmlNode *keyInfo = NULL;
	MlnXmlNode *value = NULL;
	MlnXmlNode *data = NULL;
	MlnXmlNode *keyReference = NULL;
	MlnXmlNode *bodyReference = NULL;
	MlnXmlNode *signatureReference = NULL;
	MlnXmlNode *list = NULL;
	MlnXmlNode *encryptedKey = NULL;
	MlnBuffer *cipher = NULL;
	MlnBuffer *cipherText = NULL;
	MlnBuffer *oaep = NULL;
	MlnBuffer *oaepText = NULL;
	void *rsa = NULL;
	MlnBuffer *keyId;
	MlnBuffer key;
	unsigned int count;
	unsigned int ret;

	if (recipient == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	key.size = MLN_NEMO_MESSAGE_KEY_SIZE;
	key.data = messageKey;
	keyId = recipient->keyId;
	ret = MlnImpl_81055670(&rsa, recipient->modulus, recipient->exponent, NULL, NULL, NULL,
			       NULL, NULL, NULL);
	if (ret != 0)
		goto out;
	oaep = MlnBufferCreate(MLN_HEAP_GENERAL, MlnStrlen(MLN_NEMO_OAEP_PARAMS),
			       MLN_NEMO_OAEP_PARAMS);
	if (oaep == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnImpl_81055D28(rsa, &key, oaep, &cipher);
	if (ret != 0)
		goto out;

	attribute.name = "Algorithm";
	attribute.value = "http://www.w3.org/2000/09/xmldsig#sha1";
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "DigestMethod", "ds", MLN_NEMO_NS_DSIG, 1,
				 &attribute, 0, NULL, 0, &digest);
	if (ret != 0)
		goto out;
	methodContents[0].kind = MLN_XML_BUILD_NODE;
	methodContents[0].value.node = digest;
	count = 1;
	if (MlnBytesCompare(MLN_HEAP_GENERAL, MlnStrlen(MLN_NEMO_OAEP_PARAMS),
			    MLN_NEMO_OAEP_PARAMS, MlnStrlen(MLN_NEMO_OAEP_PARAMS),
			    MLN_NEMO_OAEP_PARAMS) != 0) {
		ret = MlnImpl_81026814(MLN_HEAP_GENERAL, oaep, 72, 0, &oaepText);
		if (ret != 0)
			goto out;
		content.kind = MLN_XML_BUILD_BUFFER;
		content.value.buffer = oaepText;
		ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "OAEPparams", "xenc", NULL, 0, NULL, 1,
					 &content, 0, &parameters);
		if (ret != 0)
			goto out;
		methodContents[1].kind = MLN_XML_BUILD_NODE;
		methodContents[1].value.node = parameters;
		count = 2;
	}
	attribute.name = "Algorithm";
	attribute.value = "http://www.w3.org/2001/04/xmlenc#rsa-oaep-mgf1p";
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "EncryptionMethod", "xenc", NULL, 1, &attribute,
				 count, methodContents, 0, &method);
	if (ret != 0)
		goto out;

	content.kind = MLN_XML_BUILD_BUFFER;
	content.value.buffer = keyId;
	attributes[0].name = "ValueType";
	attributes[0].value = "http://docs.oasis-open.org/wss/2004/01/"
			      "oasis-200401-wss-x509-token-profile-1.0#X509v3SubjectKeyIdentifier";
	attributes[1].name = "EncodingType";
	attributes[1].value = MLN_NEMO_BASE64;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "KeyIdentifier", "wsse", NULL, 2, attributes, 1,
				 &content, 0, &reference);
	if (ret != 0)
		goto out;
	content.kind = MLN_XML_BUILD_NODE;
	content.value.node = reference;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "SecurityTokenReference", "wsse",
				 MLN_NEMO_NS_SECEXT, 0, NULL, 1, &content, 1, &reference);
	if (ret != 0)
		goto out;
	content.kind = MLN_XML_BUILD_NODE;
	content.value.node = reference;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "KeyInfo", "ds", MLN_NEMO_NS_DSIG, 0, NULL, 1,
				 &content, 0, &keyInfo);
	if (ret != 0)
		goto out;

	ret = MlnImpl_81026814(MLN_HEAP_GENERAL, cipher, 72, 0, &cipherText);
	if (ret != 0)
		goto out;
	content.kind = MLN_XML_BUILD_BUFFER;
	content.value.buffer = cipherText;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "CipherValue", "xenc", NULL, 0, NULL, 1,
				 &content, 0, &value);
	if (ret != 0)
		goto out;
	content.kind = MLN_XML_BUILD_NODE;
	content.value.node = value;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "CipherData", "xenc", NULL, 0, NULL, 1,
				 &content, 0, &data);
	if (ret != 0)
		goto out;

	attribute.name = "URI";
	attribute.value = "#EncryptedMessageKey";
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "DataReference", "xenc", NULL, 1, &attribute, 0,
				 NULL, 0, &keyReference);
	if (ret != 0)
		goto out;
	contents[0].kind = MLN_XML_BUILD_NODE;
	contents[0].value.node = keyReference;
	attribute.name = "URI";
	attribute.value = "#EncryptedBody";
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "DataReference", "xenc", NULL, 1, &attribute, 0,
				 NULL, 0, &bodyReference);
	if (ret != 0)
		goto out;
	contents[1].kind = MLN_XML_BUILD_NODE;
	contents[1].value.node = bodyReference;
	attribute.name = "URI";
	attribute.value = "#EncryptedSignature";
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "DataReference", "xenc", NULL, 1, &attribute, 0,
				 NULL, 0, &signatureReference);
	if (ret != 0)
		goto out;
	contents[2].kind = MLN_XML_BUILD_NODE;
	contents[2].value.node = signatureReference;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "ReferenceList", "xenc", NULL, 0, NULL, 3,
				 contents, 0, &list);
	if (ret != 0)
		goto out;

	contents[0].kind = MLN_XML_BUILD_NODE;
	contents[0].value.node = method;
	contents[1].kind = MLN_XML_BUILD_NODE;
	contents[1].value.node = keyInfo;
	contents[2].kind = MLN_XML_BUILD_NODE;
	contents[2].value.node = data;
	contents[3].kind = MLN_XML_BUILD_NODE;
	contents[3].value.node = list;
	attribute.name = "Id";
	attribute.value = "NemoConfidentiality";
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "EncryptedKey", "xenc", MLN_NEMO_NS_XENC, 1,
				 &attribute, 4, contents, 0, &encryptedKey);
	if (ret == 0) {
		*out = encryptedKey;
		encryptedKey = NULL;
	}
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, method);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, parameters);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, digest);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, reference);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, keyInfo);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, value);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, data);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, keyReference);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, bodyReference);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, signatureReference);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, list);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, encryptedKey);
	MlnBufferDestroy(MLN_HEAP_GENERAL, cipher);
	MlnBufferDestroy(MLN_HEAP_GENERAL, cipherText);
	MlnBufferDestroy(MLN_HEAP_GENERAL, oaep);
	MlnBufferDestroy(MLN_HEAP_GENERAL, oaepText);
	MlnImpl_81055ABC(rsa);
	return ret;
}

/* the message key as a BinarySecurityToken, encrypted into the header later */
static inline __attribute__((always_inline)) unsigned int
MlnNemoMessageKeyToken(unsigned char *messageKey, unsigned int step, MlnXmlNode **out)
{
	MlnXmlBuildAttribute attributes[4];
	MlnXmlBuildContent content;
	MlnXmlNode *token = NULL;
	MlnBuffer *text = NULL;
	MlnBuffer key;
	const char *usage;
	unsigned int ret;

	key.size = MLN_NEMO_MESSAGE_KEY_SIZE;
	key.data = messageKey;
	ret = MlnImpl_81026814(MLN_HEAP_GENERAL, &key, 72, 0, &text);
	if (ret == 0) {
		content.kind = MLN_XML_BUILD_BUFFER;
		content.value.buffer = text;
		attributes[0].name = "ValueType";
		attributes[0].value = "http://nemo.intertrust.com/2005/10/security/BST/SymmetricKey";
		attributes[1].name = "EncodingType";
		attributes[1].value = MLN_NEMO_BASE64;
		if (step == MLN_NEMO_STEP_RESPONSE) {
			usage = MLN_NEMO_USAGE("response-messageKey");
		} else if (step == MLN_NEMO_STEP_CONFIRMATION) {
			usage = MLN_NEMO_USAGE("confirmation-messageKey");
		} else {
			usage = MLN_NEMO_USAGE("request-messageKey");
			if (step != MLN_NEMO_STEP_REQUEST)
				usage = MLN_NEMO_USAGE("fault-messageKey");
		}
		attributes[2].name = "xmlns:nemosec";
		attributes[2].value = MLN_NEMO_NS_SECURITY;
		attributes[3].name = "nemosec:Usage";
		attributes[3].value = usage;
		ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "BinarySecurityToken", "wsse",
					 MLN_NEMO_NS_SECEXT, 4, attributes, 1, &content, 0, &token);
		if (ret == 0) {
			*out = token;
			token = NULL;
		}
	}
	MlnXmlNodeFree(MLN_HEAP_GENERAL, token);
	if (text != NULL) {
		if (text->data != NULL)
			MlnMemset(text->data, 0, text->size);
		MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	}
	return ret;
}

/* a sender's assertion: the one Assertion of the XML, by its AssertionID */
static inline __attribute__((always_inline)) unsigned int
MlnNemoAssertion(MlnXmlNode *node, MlnBuffer **id, MlnXmlNode **out)
{
	MlnXmlNodeList *list = NULL;
	MlnBuffer *value = NULL;
	MlnXmlNode *assertion;
	unsigned int ret;

	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, node, "Assertion", MLN_NEMO_NS_SAML, &list);
	if (ret != 0)
		goto out;
	if (list->count != 1) {
		ret = MLN_NEMO_ERROR_FORMAT;
		goto out;
	}
	assertion = list->nodes[0];
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, assertion, NULL, "AssertionID", &value);
	if (ret != 0)
		goto out;
	*id = value;
	value = NULL;
	*out = MlnXmlNodeRetain(MLN_HEAP_GENERAL, assertion);
out:
	MlnFree(MLN_HEAP_GENERAL, value);
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	return ret;
}

/*
 * The signature over the message's parts: the body, the message key's token,
 * the header's parts in MlnNemoSignedParts' order, the signer's certificate
 * chain and the header's own nodes, each referenced by the Id given it. A
 * response or a fault names the signer by its certificate, the others by the
 * certificate chain's Id.
 */
static inline __attribute__((always_inline)) unsigned int
MlnNemoSignature(const MlnNemoMessage *message, unsigned int step, int fault,
		 const MlnNemoHeaderNodes *headerNodes, MlnXmlNode **parts, MlnXmlNode *body,
		 MlnXmlNode *token, MlnXmlNode **out)
{
	const unsigned short order[8] = {
		MLN_NEMO_SLOT_PROTOCOL, MLN_NEMO_SLOT_PROFILE, MLN_NEMO_SLOT_TO_NODE,
		MLN_NEMO_SLOT_TIMESTAMP, MLN_NEMO_SLOT_NONCE, MLN_NEMO_SLOT_RETURNED_NONCE,
		MLN_NEMO_SLOT_CERTIFICATES, MLN_NEMO_SLOT_FAULT,
	};
	unsigned char digests[MLN_NEMO_REFERENCES_MAX][MLN_NEMO_DIGEST_MAX];
	unsigned char headerDigests[MLN_NEMO_REFERENCES_MAX][MLN_NEMO_DIGEST_MAX];
	unsigned char *digestList[MLN_NEMO_REFERENCES_MAX];
	char *uris[MLN_NEMO_REFERENCES_MAX];
	char *headerUris[MLN_NEMO_REFERENCES_MAX];
	int transforms[MLN_NEMO_REFERENCES_MAX];
	MlnBufferGroups *credentials = message->credentials;
	const MlnNemoHeaderNode *entry;
	MlnXmlNode *signature = NULL;
	MlnXmlNode *part;
	MlnX509 *certificate = NULL;
	MlnBuffer *der = NULL;
	MlnBuffer *text = NULL;
	MlnBuffer *keyName = NULL;
	MlnBuffer *signer;
	char *signerId = NULL;
	char *signerUri = NULL;
	int method = message->signatureMethod;
	int digestMethod = message->digestMethod;
	unsigned int digestSize;
	unsigned short index;
	unsigned int count = 0;
	unsigned int added;
	unsigned int ret;
	unsigned int i;
	int named;

	__builtin_memset(uris, 0, sizeof(uris));
	if (credentials == NULL || method > 1 || digestMethod != method) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto done;
	}
	switch (digestMethod) {
	case 0:
		digestSize = 20;
		break;
	case 1:
		digestSize = 32;
		break;
	default:
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto done;
	}

	if (body != NULL) {
		ret = MlnImpl_81085B24(body, 2, digests[0], digestSize, 0, NULL);
		if (ret == 0)
			ret = MlnImpl_8108418C("soapBody", &uris[0]);
		if (ret != 0)
			goto done;
		count = 1;
	}
	index = 1;
	if (token != NULL) {
		ret = MlnImpl_81085B24(token, 1, digests[count], digestSize, index, &uris[count]);
		if (ret != 0)
			goto done;
		count++;
		index++;
	}
	for (i = 0; i < 8; i++) {
		part = parts[order[i]];
		if (part == NULL)
			continue;
		ret = MlnImpl_81085B24(part, 1, digests[count], digestSize, index, &uris[count]);
		if (ret != 0)
			goto done;
		count++;
		index++;
	}
	for (i = 0; i < count; i++) {
		transforms[i] = 0;
		digestList[i] = digests[i];
	}

	if (parts[MLN_NEMO_SLOT_SIGNER] != NULL) {
		ret = MlnImpl_81085678(index, &signerId);
		if (ret != 0)
			goto done;
		ret = MlnImpl_8108418C(signerId, &signerUri);
		index++;
		if (ret != 0)
			goto done;
		ret = MlnImpl_81085A88(signerId, 1, parts[MLN_NEMO_SLOT_SIGNER]);
		if (ret != 0)
			goto done;
	}
	if (headerNodes != NULL) {
		added = 0;
		for (i = 0; (int)i < headerNodes->count; i++) {
			entry = &headerNodes->items[i];
			if (entry->type == 16) {
				ret = MlnImpl_81085B24(entry->node, 1, headerDigests[added], digestSize,
						       index++, &headerUris[added]);
			} else if (step != MLN_NEMO_STEP_REQUEST && entry->type == 4) {
				headerUris[added] = NULL;
				ret = MlnImpl_81085B24(entry->node, 1, headerDigests[added], digestSize,
						       index++, &headerUris[added]);
			} else {
				continue;
			}
			if (ret != 0) {
				for (i = 0; i < added; i++)
					MlnFree(MLN_HEAP_GENERAL, headerUris[i]);
				goto done;
			}
			added++;
		}
		if (added != 0) {
			__builtin_memset(&transforms[count], 0, added * sizeof(transforms[0]));
			for (i = 0; i < added; i++)
				uris[count + i] = headerUris[i];
			for (i = 0; i < added; i++)
				digestList[count + i] = headerDigests[i];
			count += added;
		}
	}

	named = fault;
	if (step == MLN_NEMO_STEP_RESPONSE)
		named = 1;
	signer = *credentials->lists[0].items;
	if (named) {
		if (signer == NULL) {
			ret = MLN_NEMO_ERROR_PARAMETER;
			goto out;
		}
		ret = MlnImpl_81038EF4(signer, &certificate);
		if (ret != 0)
			goto out;
		ret = MlnImpl_810390C0(certificate, &der);
		if (ret != 0)
			goto out;
		ret = MlnImpl_81026814(MLN_HEAP_GENERAL, der, 72, 0, &text);
		if (ret != 0)
			goto out;
		ret = MlnImpl_81085C24(count, method, transforms, uris, digestList,
				       MLN_NEMO_KEY_INFO_CERTIFICATE, text, &signature);
	} else {
		if (signerUri == NULL) {
			ret = MLN_NEMO_ERROR_PARAMETER;
			goto out;
		}
		keyName = MlnBufferCreate(MLN_HEAP_GENERAL, MlnStrlen(signerUri) + 1, signerUri);
		if (keyName == NULL) {
			ret = 2;
			goto out;
		}
		ret = MlnImpl_81085C24(count, method, transforms, uris, digestList,
				       MLN_NEMO_KEY_INFO_NAME, keyName, &signature);
	}
	if (ret == 0) {
		*out = signature;
		signature = NULL;
	}
out:
	MlnImpl_81038E90(certificate);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnBufferDestroy(MLN_HEAP_GENERAL, der);
	MlnBufferDestroy(MLN_HEAP_GENERAL, keyName);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, signature);
done:
	MlnFree(MLN_HEAP_GENERAL, signerUri);
	MlnFree(MLN_HEAP_GENERAL, signerId);
	for (i = 0; i < count; i++)
		MlnFree(MLN_HEAP_GENERAL, uris[i]);
	return ret;
}

/* an error of the message's parts as the exchange reports it */
static inline __attribute__((always_inline)) unsigned int MlnNemoError(unsigned int ret)
{
	unsigned int domain;

	if (ret == MLN_NEMO_ERROR_FORMAT)
		return MLN_NEMO_ERROR_PARAMETER;
	domain = ret & 0xFFFF0000;
	if (domain == 0x10010000 || domain == 0x220A0000)
		return 5;
	if (domain == 0x22060000 || domain == 0x22030000)
		return MLN_NEMO_ERROR_FAILED;
	if (domain == 0x23110000) {
		if (ret - 0x23110004 > 1)
			return MLN_NEMO_ERROR_FAILED;
		return ret;
	}
	if ((ret >> 17) == 0x1100)
		return MLN_NEMO_ERROR_CRYPTO;
	if (ret == 0 || ret == 2)
		return ret;
	if (ret == 3)
		return MLN_NEMO_ERROR_PARAMETER;
	if (domain != 0x23050000 && ret != 0x230A0004 && ret != 6)
		return 5;
	return ret;
}

/*
 * A message of a kind at a step made: its body (encrypted when the step's
 * usage says so) under a SOAP header of the message's header nodes and a
 * Security header, serialized into *out.
 */
unsigned int MlnImpl_81072240(MlnNemoMessage *message, unsigned char kind, unsigned char step,
			      MlnBuffer **out)
{
	MlnXmlNode *parts[MLN_NEMO_SLOTS];
	MlnXmlNode *header[MLN_NEMO_HEADER_MAX];
	MlnXmlBuildContent contents[MLN_NEMO_SLOTS];
	MlnXmlBuildAttribute attributes[3];
	unsigned char messageKey[MLN_NEMO_MESSAGE_KEY_SIZE];
	MlnNemoHeaderNodes *headerNodes = NULL;
	MlnXmlNode *details = NULL;
	MlnXmlNode *body = NULL;
	MlnXmlNode *signedBody = NULL;
	MlnXmlNode *token = NULL;
	MlnXmlNode *signature = NULL;
	MlnXmlNode *security;
	MlnXmlNode *node;
	MlnBuffer *assertionId = NULL;
	MlnBuffer *nodeId = NULL;
	MlnBuffer *assertion;
	MlnXmlNode *document;
	MlnBuffer *peer;
	unsigned char usage;
	unsigned int present;
	unsigned int count;
	unsigned int total;
	unsigned int ret;
	unsigned int i;
	int missing;
	int fault;

	for (i = 0; i < MLN_NEMO_SLOTS; i++)
		parts[i] = NULL;
	for (i = 0; i < MLN_NEMO_HEADER_MAX; i++)
		header[i] = NULL;
	if ((unsigned char)(step - 1) > 7 || message == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	if (kind > 14) {
		ret = MLN_NEMO_ERROR_KIND;
		goto out;
	}
	ret = MlnImpl_81072138(kind, step, &usage);
	if (ret != 0)
		goto out;
	if (message->keys[0] != NULL && step == MLN_NEMO_STEP_FAULT)
		usage &= ~MLN_NEMO_USAGE_ENCRYPTED;

	ret = MlnNemoProtocolDeclaration(step, usage, &parts[MLN_NEMO_SLOT_PROTOCOL]);
	if (ret != 0)
		goto out;
	ret = MlnNemoProfile(&parts[MLN_NEMO_SLOT_PROFILE]);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81077FAC(&message->header, &headerNodes);
	if (ret != 0)
		goto out;

	/* an action and a sender; a reply's origin; a request's own parts only */
	present = message->header.present;
	missing = 1;
	if (present & MLN_NEMO_PART_ACTION) {
		missing = !(present & MLN_NEMO_PART_SENDER);
		if ((step == MLN_NEMO_STEP_RESPONSE || step == MLN_NEMO_STEP_CONFIRMATION ||
		     step == MLN_NEMO_STEP_FAULT) && !(present & MLN_NEMO_PART_ORIGIN))
			missing = 1;
		if (kind != MLN_NEMO_KIND_TRANSFER && step == MLN_NEMO_STEP_REQUEST &&
		    (present & (MLN_NEMO_PART_ORIGIN | MLN_NEMO_PART_RECIPIENT)))
			missing = 1;
	}
	if (missing && step != MLN_NEMO_STEP_FAULT) {
		ret = MLN_NEMO_ERROR_HEADER;
		goto out;
	}
	fault = step == MLN_NEMO_STEP_FAULT;

	switch (step) {
	case MLN_NEMO_STEP_REQUEST:
		if (message->header.parts[3] == NULL)
			break;
		ret = MlnNemoReturnedNonce(message, step, fault,
					   &parts[MLN_NEMO_SLOT_RETURNED_NONCE]);
		if (ret != 0)
			goto out;
		break;
	case MLN_NEMO_STEP_FAULT:
		if (message->faults != NULL && message->faults->items != NULL &&
		    message->faults->count > 1) {
			ret = MlnNemoFaultDetails(message->faults, &details);
			if (ret != 0)
				goto out;
		}
		/* fall through */
	case MLN_NEMO_STEP_RESPONSE:
	case MLN_NEMO_STEP_CONFIRMATION:
		if (!MlnImpl_810721BC(kind, step))
			break;
		ret = MlnNemoReturnedNonce(message, step, fault,
					   &parts[MLN_NEMO_SLOT_RETURNED_NONCE]);
		if (ret != 0)
			goto out;
		break;
	}
	if (MlnImpl_81085D84(kind, step)) {
		ret = MlnNemoNonce(message, step, &parts[MLN_NEMO_SLOT_NONCE]);
		if (ret != 0)
			goto out;
	}
	if (usage & MLN_NEMO_USAGE_FRESH) {
		ret = MlnNemoTimestamp(step, &parts[MLN_NEMO_SLOT_TIMESTAMP]);
		if (ret != 0)
			goto out;
	}
	if ((usage & MLN_NEMO_USAGE_TIMESTAMP) && parts[MLN_NEMO_SLOT_TIMESTAMP] == NULL) {
		ret = 5;
		goto out;
	}

	/* the recipient: the node its certificate names */
	peer = message->peer;
	if (peer != NULL) {
		if (usage & MLN_NEMO_USAGE_ENCRYPTED) {
			if (message->keys[0] == NULL) {
				ret = MLN_NEMO_ERROR_PARAMETER;
				goto out;
			}
			ret = MlnImpl_81084F3C(peer, message->keys[0]->certificate);
			if (ret != 0)
				goto out;
			peer = message->peer;
			if (peer == NULL) {
				ret = MLN_NEMO_ERROR_PARAMETER;
				goto out;
			}
		}
		ret = MlnNemoToNode(peer, step, &parts[MLN_NEMO_SLOT_TO_NODE]);
		if (ret != 0)
			goto out;
	} else if (step != MLN_NEMO_STEP_FAULT) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}

	if (usage & MLN_NEMO_USAGE_ENCRYPTED) {
		ret = MlnImpl_810791F0(MLN_NEMO_MESSAGE_KEY_SIZE, messageKey);
		if (ret != 0)
			goto out;
		ret = MlnNemoEncryptedKey(message->keys[0], messageKey,
					  &parts[MLN_NEMO_SLOT_ENCRYPTED_KEY]);
		if (ret != 0)
			goto out;
		ret = MlnNemoMessageKeyToken(messageKey, step, &token);
		if (ret != 0)
			goto out;
	}

	/* the sender's certificate chains and assertions */
	if (step == MLN_NEMO_STEP_REQUEST && MlnImpl_81086254(kind)) {
		ret = MlnImpl_81085338(1, message->credentials->lists[1].count,
				       message->credentials->lists[1].items, MLN_NEMO_STEP_REQUEST,
				       &parts[MLN_NEMO_SLOT_CERTIFICATES]);
		if (ret != 0)
			goto out;
	}
	if ((usage & MLN_NEMO_USAGE_SIGNED) &&
	    (step == MLN_NEMO_STEP_REQUEST || step == MLN_NEMO_STEP_CONFIRMATION)) {
		ret = MlnImpl_81085338(0, message->credentials->lists[0].count,
				       message->credentials->lists[0].items, step,
				       &parts[MLN_NEMO_SLOT_SIGNER]);
		if (ret != 0)
			goto out;
	}
	if (step == MLN_NEMO_STEP_REQUEST) {
		assertion = message->credentials->last;
		if (assertion != NULL) {
			document = NULL;
			ret = MlnXmlParse(MLN_HEAP_GENERAL, assertion->size, assertion->data,
					  &document);
			if (ret == 0)
				ret = MlnNemoAssertion(document, &assertionId,
						       &parts[MLN_NEMO_SLOT_ASSERTION]);
			MlnXmlNodeFree(MLN_HEAP_GENERAL, document);
			if (ret != 0)
				goto out;
			ret = MlnImpl_81085204(assertionId,
					       "http://nemo.intertrust.com/2004/attribute/role",
					       &parts[MLN_NEMO_SLOT_ROLE]);
			if (ret != 0)
				goto out;
		}
		if (message->nodes != NULL && message->nodeCount != 0) {
			for (i = 0; ; ) {
				node = message->nodes[i];
				if (node == NULL) {
					ret = MLN_NEMO_ERROR_PARAMETER;
					goto out;
				}
				ret = MlnNemoAssertion(node, &nodeId,
						       &parts[MLN_NEMO_SLOT_NODE_ASSERTIONS + i]);
				if (ret != 0)
					goto out;
				ret = MlnImpl_81085204(nodeId,
						       "urn:marlin:broadband:1.0:nemo:services:"
						       "datacertification-service:assertion",
						       &parts[MLN_NEMO_SLOT_NODE_ROLES + i]);
				if (ret != 0)
					goto out;
				MlnBufferDestroy(MLN_HEAP_GENERAL, nodeId);
				nodeId = NULL;
				i++;
				if (message->nodeCount <= i || i == MLN_NEMO_NODES_MAX)
					break;
			}
		}
	}

	if (usage & MLN_NEMO_USAGE_SIGNED) {
		ret = MlnImpl_81078AB8(message->body, &signedBody);
		if (ret != 0)
			goto out;
		parts[MLN_NEMO_SLOT_FAULT] = details;
		details = NULL;
		ret = MlnNemoSignature(message, step, fault, headerNodes, parts, signedBody, token,
				       &signature);
		if (ret != 0)
			goto out;
		details = parts[MLN_NEMO_SLOT_FAULT];
		parts[MLN_NEMO_SLOT_FAULT] = NULL;
		if (usage & MLN_NEMO_USAGE_ENCRYPTED) {
			ret = MlnImpl_81085718(messageKey, "EncryptedSignature", signature,
					       MLN_NEMO_XENC_ELEMENT, &parts[MLN_NEMO_SLOT_SIGNATURE]);
			if (ret != 0)
				goto out;
			goto encrypted;
		}
		ret = MlnXmlNodeToElement(MLN_HEAP_GENERAL, signature,
					  &parts[MLN_NEMO_SLOT_SIGNATURE]);
		if (ret != 0)
			goto out;
		signature = NULL;
	} else if (usage & MLN_NEMO_USAGE_ENCRYPTED) {
encrypted:
		ret = MlnImpl_81085718(messageKey, "EncryptedMessageKey", token,
				       MLN_NEMO_XENC_ELEMENT, &parts[MLN_NEMO_SLOT_MESSAGE_KEY]);
		if (ret != 0)
			goto out;
	}

	/* the SOAP header: the header's nodes, the Security header, a fault's details */
	total = 0;
	if (headerNodes != NULL)
		for (total = 0; (int)total < headerNodes->count; total++)
			header[total] = MlnXmlNodeRetain(MLN_HEAP_GENERAL,
							 headerNodes->items[total].node);
	count = 0;
	for (i = 0; i < MLN_NEMO_SLOTS; i++) {
		if (parts[i] == NULL)
			continue;
		contents[count].kind = MLN_XML_BUILD_NODE;
		contents[count].value.node = parts[i];
		count++;
	}
	attributes[0].name = "xmlns:nemosec";
	attributes[0].value = MLN_NEMO_NS_SECURITY;
	attributes[1].name = "xmlns:ds";
	attributes[1].value = MLN_NEMO_NS_DSIG;
	attributes[2].name = "xmlns:xenc";
	attributes[2].value = MLN_NEMO_NS_XENC;
	security = NULL;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Security", "wsse", MLN_NEMO_NS_SECEXT, 3,
				 attributes, count, contents, 1, &security);
	if (ret != 0) {
		MlnXmlNodeFree(MLN_HEAP_GENERAL, security);
		goto out;
	}
	header[total] = security;
	for (i = 0; i < MLN_NEMO_SLOTS; i++)
		parts[i] = NULL;
	security = NULL;
	MlnXmlNodeFree(MLN_HEAP_GENERAL, security);
	total++;
	if (details != NULL)
		header[total++] = MlnXmlNodeRetain(MLN_HEAP_GENERAL, details);
	if (usage & MLN_NEMO_USAGE_ENCRYPTED) {
		ret = MlnImpl_81085718(messageKey, "EncryptedBody", message->body,
				       MLN_NEMO_XENC_CONTENT, &body);
		if (ret != 0)
			goto out;
	} else {
		body = MlnXmlNodeRetain(MLN_HEAP_GENERAL, message->body);
	}
	ret = MlnImpl_81078B5C(header, (unsigned char)total, body, out);
out:
	MlnMemset(messageKey, 0, sizeof(messageKey));
	for (i = 0; i < MLN_NEMO_SLOTS; i++)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, parts[i]);
	for (i = 0; i < MLN_NEMO_HEADER_MAX; i++)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, header[i]);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, token);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, signature);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, signedBody);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, body);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, details);
	MlnBufferDestroy(MLN_HEAP_GENERAL, assertionId);
	MlnBufferDestroy(MLN_HEAP_GENERAL, nodeId);
	MlnImpl_81079854(headerNodes);
	return MlnNemoError(ret);
}
