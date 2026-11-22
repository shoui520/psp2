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

	if (fault->parts[MLN_NEMO_FAULT_REDIRECT_URL] != NULL) {
		text.kind = MLN_XML_BUILD_BUFFER;
		text.value.buffer = fault->parts[MLN_NEMO_FAULT_REDIRECT_URL];
		ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "RedirectURL", "exc",
					 MLN_NEMO_NS_EXCEPTIONS, 0, NULL, 1, &text, 0, &redirect);
		if (ret != 0)
			goto out;
		contents[0].kind = MLN_XML_BUILD_NODE;
		contents[0].value.node = redirect;
		count = 1;
	}
	if (fault->parts[MLN_NEMO_FAULT_DETAILS] != NULL) {
		text.kind = MLN_XML_BUILD_BUFFER;
		text.value.buffer = fault->parts[MLN_NEMO_FAULT_DETAILS];
		ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Details", "exc", MLN_NEMO_NS_EXCEPTIONS,
					 0, NULL, 1, &text, 0, &details);
		if (ret != 0)
			goto out;
		contents[count].kind = MLN_XML_BUILD_NODE;
		contents[count].value.node = details;
		count++;
	}
	if (fault->parts[MLN_NEMO_FAULT_NAME] != NULL) {
		size = fault->parts[MLN_NEMO_FAULT_NAME]->size;
		name = MlnAllocate(MLN_HEAP_GENERAL, size + 1);
		if (name == NULL) {
			ret = 2;
			goto out;
		}
		MlnMemcpy(name, fault->parts[MLN_NEMO_FAULT_NAME]->data, size);
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
		missing = !(present & MLN_NEMO_PART_MESSAGE_ID);
		if ((step == MLN_NEMO_STEP_RESPONSE || step == MLN_NEMO_STEP_CONFIRMATION ||
		     step == MLN_NEMO_STEP_FAULT) && !(present & MLN_NEMO_PART_ORIGINATES_FROM))
			missing = 1;
		if (kind != MLN_NEMO_KIND_TRANSFER && step == MLN_NEMO_STEP_REQUEST &&
		    (present & (MLN_NEMO_PART_ORIGINATES_FROM | MLN_NEMO_PART_RELATES_TO)))
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

/* what a message read carries, signed and encrypted: the parts its usage needs */
#define MLN_NEMO_SIGNED_TIMESTAMP	0x001
#define MLN_NEMO_SIGNED_NONCE		0x002
#define MLN_NEMO_SIGNED_RETURNED_NONCE	0x004
#define MLN_NEMO_SIGNED_BODY		0x008
#define MLN_NEMO_SIGNED_MESSAGE_KEY	0x010	/* the message key's token */
#define MLN_NEMO_SIGNED_PROTOCOL	0x020
#define MLN_NEMO_SIGNED_MESSAGE_ID	0x040	/* the header's MessageID node */
#define MLN_NEMO_SIGNED_ORIGINATES_FROM	0x080	/* the header's RelatesTo originatesFrom node */
#define MLN_NEMO_SIGNED_CERTIFICATES	0x100	/* a request's second certificate chain */
#define MLN_NEMO_SIGNED_FAULT		0x200	/* a fault's details */
#define MLN_NEMO_SIGNED_PROFILE		0x400
#define MLN_NEMO_SIGNED_TO_NODE		0x800
#define MLN_NEMO_SIGNED_REQUIRED \
	(MLN_NEMO_SIGNED_TO_NODE | MLN_NEMO_SIGNED_PROFILE | MLN_NEMO_SIGNED_PROTOCOL | \
	 MLN_NEMO_SIGNED_BODY)

#define MLN_NEMO_FOUND_TIMESTAMP	0x1
#define MLN_NEMO_FOUND_NONCE		0x2
#define MLN_NEMO_FOUND_RETURNED_NONCE	0x4

#define MLN_NEMO_ENCRYPTED_KEY		0x1
#define MLN_NEMO_ENCRYPTED_BODY		0x2

/* the references a signature read covers: the parts', then the header nodes' from here */
#define MLN_NEMO_READ_PARTS		16
#define MLN_NEMO_READ_REFERENCES	32

#define MLN_NEMO_NS_SOAP	"http://schemas.xmlsoap.org/soap/envelope/"
#define MLN_NEMO_ROLE_USAGE	"http://nemo.intertrust.com/2004/attribute/role"
#define MLN_NEMO_PROFILE	"urn:marlin:core:1.0:nemo:protocol:profile:1"
#define MLN_NEMO_PROFILE_USAGE	"http://nemo.intertrust.com/2005/10/security/profile"

/* whether a buffer read holds a string */
#define MLN_NEMO_IS(buffer, s) \
	(MlnBytesCompare(MLN_HEAP_GENERAL, (buffer)->size, (buffer)->data, MlnStrlen(s), s) == 0)

/* the step a message read declares, and its ProtocolDeclaration */
static inline __attribute__((always_inline)) unsigned int MlnNemoReadProtocol(
	MlnXmlNode *security, MlnXmlNode **declaration, unsigned int *step)
{
	MlnXmlNode *node = NULL;
	MlnXmlNode *stepNode = NULL;
	MlnBuffer *type = NULL;
	unsigned int ret;

	if (security == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, security, "ProtocolDeclaration",
			     MLN_NEMO_NS_SECURITY, &node);
	if (ret != 0)
		goto out;
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, node, "Step", MLN_NEMO_NS_SECURITY, &stepNode);
	if (ret != 0)
		goto out;
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, stepNode, NULL, "Type", &type);
	if (ret != 0)
		goto out;
	if (MLN_NEMO_IS(type, "response"))
		*step = MLN_NEMO_STEP_RESPONSE;
	else if (MLN_NEMO_IS(type, "request"))
		*step = MLN_NEMO_STEP_REQUEST;
	else if (MLN_NEMO_IS(type, "confirmation"))
		*step = MLN_NEMO_STEP_CONFIRMATION;
	else if (MLN_NEMO_IS(type, "fault"))
		*step = MLN_NEMO_STEP_FAULT;
	else {
		ret = MLN_NEMO_ERROR_STEP;
		goto out;
	}
	*declaration = node;
	node = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, type);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, node);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, stepNode);
	return ret;
}

/* the Security header's EncryptedData decrypted in place with the message keys */
static inline __attribute__((always_inline)) unsigned int MlnNemoReadDecrypt(
	MlnXmlNode *security, unsigned int usage, MlnNemoMessageKey *keys, int keyCount)
{
	MlnXmlNodeList *list = NULL;
	unsigned int ret;

	if (security == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
	} else {
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, security, "EncryptedData", MLN_NEMO_NS_XENC,
				      &list);
		if (ret == 0)
			ret = MlnImpl_810849E0(list, security, keys, keyCount);
		else if (ret == MLN_XML_ERROR_NOT_FOUND)
			ret = (usage & MLN_NEMO_USAGE_ENCRYPTED) ? MLN_NEMO_ERROR_UNSECURED : 0;
	}
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	return ret;
}

/*
 * A part's wsu:Id and the digest of its XML, a reference the signature has to
 * cover: *id stays NULL for a part without an Id.
 */
static inline __attribute__((always_inline)) unsigned int MlnNemoReadReference(
	MlnXmlNode *node, int digestSize, unsigned char *digest, MlnBuffer **id)
{
	MlnBuffer *xml = NULL;
	unsigned int ret;

	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "wsu:Id", id);
	if (ret == MLN_XML_ERROR_NOT_FOUND)
		return 0;
	if (ret == 0)
		ret = MlnXmlNodeToXml(MLN_HEAP_GENERAL, node, &xml);
	if (ret == 0)
		ret = MlnImpl_81084D14(xml->data, xml->size, digest, digestSize);
	MlnBufferDestroy(MLN_HEAP_GENERAL, xml);
	return ret;
}

/* the Profile a message read names, which has to be Marlin's */
static inline __attribute__((always_inline)) unsigned int MlnNemoReadProfile(
	MlnXmlNode *security, MlnXmlNode **profile)
{
	MlnXmlNode *node = NULL;
	MlnBuffer *uri = NULL;
	MlnBuffer *usage = NULL;
	unsigned int ret;

	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, security, "Profile", MLN_NEMO_NS_SECURITY, &node);
	if (ret != 0)
		goto out;
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "URI", &uri);
	if (ret != 0)
		goto out;
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, node, NULL, "nemosec:Usage", &usage);
	if (ret != 0)
		goto out;
	if (MlnBytesCompare(MLN_HEAP_GENERAL, MlnStrlen(MLN_NEMO_PROFILE), MLN_NEMO_PROFILE,
			    uri->size, uri->data) != 0 ||
	    MlnBytesCompare(MLN_HEAP_GENERAL, MlnStrlen(MLN_NEMO_PROFILE_USAGE),
			    MLN_NEMO_PROFILE_USAGE, usage->size, usage->data) != 0) {
		ret = MLN_NEMO_ERROR_FORMAT;
		goto out;
	}
	*profile = node;
	node = NULL;
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, node);
	MlnBufferDestroy(MLN_HEAP_GENERAL, uri);
	MlnBufferDestroy(MLN_HEAP_GENERAL, usage);
	return ret;
}

/*
 * The header nodes' references, kept from MLN_NEMO_READ_PARTS on: the
 * sender's and the origin's marked signed.
 */
static inline __attribute__((always_inline)) unsigned int MlnNemoReadHeaderReferences(
	const MlnNemoHeaderNodes *nodes, int digestSize,
	unsigned char (*digests)[MLN_NEMO_DIGEST_MAX], MlnBuffer **ids, unsigned int *signedParts,
	int *count)
{
	MlnBuffer *xml = NULL;
	MlnNemoHeaderNode *entry;
	unsigned int ret;
	int n = 0;
	int i;

	if (nodes == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	ret = 0;
	for (i = 0; i < nodes->count; i++) {
		entry = &nodes->items[i];
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, entry->node, NULL, "wsu:Id", &ids[n]);
		if (ret == MLN_XML_ERROR_NOT_FOUND) {
			ret = 0;
			continue;
		}
		if (ret != 0)
			break;
		n++;
		ret = MlnXmlNodeToXml(MLN_HEAP_GENERAL, entry->node, &xml);
		if (ret == 0)
			ret = MlnImpl_81084D14(xml->data, xml->size, digests[n - 1], digestSize);
		if (ret != 0)
			break;
		MlnBufferDestroy(MLN_HEAP_GENERAL, xml);
		xml = NULL;
		if (entry->type == MLN_NEMO_PART_MESSAGE_ID)
			*signedParts |= MLN_NEMO_SIGNED_MESSAGE_ID;
		if (entry->type == MLN_NEMO_PART_ORIGINATES_FROM)
			*signedParts |= MLN_NEMO_SIGNED_ORIGINATES_FROM;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, xml);
	if (ret != 0) {
		for (i = 0; i < n; i++) {
			MlnBufferDestroy(MLN_HEAP_GENERAL, ids[i]);
			ids[i] = NULL;
		}
		return ret;
	}
	*count = n;
	return 0;
}

/*
 * The Timestamp of a message read: its Created time kept in the message, and
 * refused when older than the message's limit by the secure clock.
 */
static inline __attribute__((always_inline)) unsigned int MlnNemoReadTimestamp(
	MlnXmlNode *security, MlnNemoMessage *message, int digestSize, unsigned char *digest,
	MlnBuffer **ids, int *count, unsigned int *found, unsigned int *signedParts)
{
	MlnXmlNode *timestamp = NULL;
	MlnXmlNode *created = NULL;
	MlnBuffer *id = NULL;
	MlnBuffer *text = NULL;
	MlnBuffer *again = NULL;
	MlnDateTime time;
	int limit = message->timestampLimit;
	int seconds;
	int now;
	int mode;
	unsigned int ret;

	if (security == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, security, "Timestamp", MLN_NEMO_NS_UTILITY,
			     &timestamp);
	if (ret == MLN_XML_ERROR_NOT_FOUND) {
		ret = 0;
		ids[*count] = NULL;
		goto out;
	}
	if (ret != 0)
		goto out;
	ret = MlnNemoReadReference(timestamp, digestSize, digest, &id);
	if (ret != 0)
		goto out;
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, timestamp, "Created", MLN_NEMO_NS_UTILITY,
			     &created);
	if (ret != 0)
		goto out;
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, created, &text);
	if (ret != 0)
		goto out;
	if (text == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	ret = MlnImpl_81052D84(&now, &mode);
	if (ret == 0x220B0001)
		limit = 0;
	else if (ret != 0) {
		ret = 5;
		goto out;
	}
	if (MlnXmlParseDateTime(MLN_HEAP_GENERAL, text->size, (const char *)text->data,
				&time) != 0) {
		ret = MLN_NEMO_ERROR_FORMAT;
		goto out;
	}
	ret = MlnTimeToSeconds(&time, &seconds);
	if (ret != 0)
		goto out;
	if (limit != 0 && (unsigned int)(seconds + limit) < (unsigned int)now) {
		ret = MLN_NEMO_ERROR_EXPIRED;
		goto out;
	}
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, created, &again);
	if (ret != 0)
		goto out;
	ret = MlnXmlParseDateTime(MLN_HEAP_GENERAL, again->size, (const char *)again->data, &time);
	if (ret != 0)
		goto out;
	ret = MlnTimeToSeconds(&time, &message->timestamp);
	if (ret != 0)
		goto out;
	*found |= MLN_NEMO_FOUND_TIMESTAMP;
	ids[*count] = id;
	if (id != NULL) {
		*count += 1;
		*signedParts |= MLN_NEMO_SIGNED_TIMESTAMP;
	}
	id = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, again);
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, timestamp);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, created);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	return ret;
}

/* the Nonce of a message read (at most two: the peer's and the one returned), checked */
static inline __attribute__((always_inline)) unsigned int MlnNemoReadNonces(
	MlnXmlNode *security, unsigned int kind, unsigned int step, const MlnBuffer *sent,
	unsigned int *found, unsigned int *signedParts, int digestSize,
	unsigned char (*digests)[MLN_NEMO_DIGEST_MAX], MlnBuffer **ids, int *count,
	MlnBuffer **nonce)
{
	MlnXmlNodeList *list = NULL;
	MlnBuffer *first = NULL;
	MlnBuffer *second = NULL;
	int index = *count;
	int n;
	unsigned int ret;

	if (security == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, security, "Nonce", MLN_NEMO_NS_SECEXT, &list);
	if (ret == 0) {
		n = list->count;
		if (n > 2) {
			ret = MLN_NEMO_ERROR_FORMAT;
			goto out;
		}
		ret = MlnImpl_81085E20(list->nodes[0], kind, step, found, signedParts, sent,
				       &first, count, digests[index], digestSize, &ids[index]);
		if (ret != 0)
			goto out;
		if (n == 2) {
			ret = MlnImpl_81085E20(list->nodes[1], kind, step, found, signedParts,
					       sent, &second, count, digests[index + 1],
					       digestSize, &ids[index + 1]);
			if (ret != 0)
				goto out;
		}
	} else if (ret != MLN_XML_ERROR_NOT_FOUND) {
		goto out;
	}
	ret = 0;
	if (second != NULL) {
		if (first != NULL) {
			ret = MLN_NEMO_ERROR_FORMAT;
			goto out;
		}
		*nonce = second;
		second = NULL;
	} else {
		*nonce = first;
		first = NULL;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, first);
	MlnBufferDestroy(MLN_HEAP_GENERAL, second);
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	return ret;
}

/* the ToNode of a message read, which has to name this node (the credentials' first) */
static inline __attribute__((always_inline)) unsigned int MlnNemoReadToNode(
	MlnXmlNode *security, const MlnBuffer *node, int digestSize, unsigned char *digest,
	MlnBuffer **ids, int *count, unsigned int *signedParts, int *missing)
{
	MlnXmlNode *toNode = NULL;
	MlnBuffer *id = NULL;
	MlnBuffer *text = NULL;
	unsigned int ret;

	*missing = 0;
	if (security == NULL || node == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, security, "ToNode", MLN_NEMO_NS_SECURITY, &toNode);
	if (ret == 0)
		ret = MlnNemoReadReference(toNode, digestSize, digest, &id);
	if (ret == 0)
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, toNode, &text);
	if (ret != 0) {
		*missing = ret == MLN_XML_ERROR_NOT_FOUND;
		goto out;
	}
	if (MlnBufferCompare(MLN_HEAP_GENERAL, node, text) != 0) {
		ret = MLN_NEMO_ERROR_TO_NODE;
		goto out;
	}
	if (ids[*count] != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, ids[*count]);
	ids[*count] = id;
	if (id != NULL) {
		*count += 1;
		*signedParts |= MLN_NEMO_SIGNED_TO_NODE;
	}
	id = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, toNode);
	return ret;
}

/* a fault's FaultDetails, in the SOAP header: *details set when there are some */
static inline __attribute__((always_inline)) unsigned int MlnNemoReadFaultDetails(
	MlnXmlNode *header, unsigned int step, int digestSize, unsigned char *digest,
	MlnBuffer **ids, int *count, unsigned int *signedParts, int *details)
{
	MlnXmlNode *node = NULL;
	MlnBuffer *id = NULL;
	unsigned int ret;

	if (header == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	ret = 0;
	if (step == MLN_NEMO_STEP_FAULT) {
		ret = MlnXmlFindNode(MLN_HEAP_GENERAL, header, "FaultDetails", MLN_NEMO_NS_CORE,
				     &node);
		if (ret == 0) {
			ret = MlnNemoReadReference(node, digestSize, digest, &id);
			if (ret != 0)
				goto out;
			*details = 1;
		} else if (ret != MLN_XML_ERROR_NOT_FOUND) {
			goto out;
		}
		ret = 0;
	}
	ids[*count] = id;
	if (id != NULL) {
		*count += 1;
		*signedParts |= MLN_NEMO_SIGNED_FAULT;
	}
	id = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, node);
	return ret;
}

/* the message key's token checked against the keys the EncryptedKeys unwrapped */
static inline __attribute__((always_inline)) unsigned int MlnNemoReadKeyToken(
	const MlnBuffer *token, MlnNemoMessageKey *keys, int keyCount)
{
	unsigned char sha[20];
	MlnBuffer *digest;
	unsigned short i;
	int valid = 0;
	unsigned int ret;

	if (keys == NULL)
		return MLN_NEMO_ERROR_PARAMETER;
	MlnImpl_810570A8(token->data, token->size, sha);
	digest = MlnBufferCreate(MLN_HEAP_GENERAL, sizeof(sha), sha);
	if (digest == NULL)
		return 2;
	ret = MLN_NEMO_ERROR_FORMAT;
	for (i = 0; i < keyCount; i++) {
		ret = MlnImpl_81080FF4(keys[i].key, 0, NULL, digest, &valid);
		if (ret != 0)
			break;
		if (valid == 1)
			break;
		ret = MLN_NEMO_ERROR_FORMAT;
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, digest);
	return ret;
}

/*
 * The payload of a message read, the body decrypted first when it is
 * encrypted: the element named, or a SOAP Fault (*fault set).
 */
static inline __attribute__((always_inline)) unsigned int MlnNemoReadBody(
	MlnXmlNode *body, const char *name, const char *uri, MlnNemoMessageKey *keys,
	int keyCount, unsigned int *encrypted, MlnXmlNode **payload, int *fault)
{
	MlnXmlNodeList *list = NULL;
	MlnXmlNode *node = NULL;
	int decrypted = 0;
	int isFault = 0;
	unsigned int ret;

	if (body == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, body, "EncryptedData", MLN_NEMO_NS_XENC, &list);
	if (ret == 0) {
		if (list->count != 1) {
			ret = MLN_NEMO_ERROR_FORMAT;
			goto out;
		}
		ret = MlnImpl_810849E0(list, body, keys, keyCount);
		if (ret != 0)
			goto out;
		decrypted = 1;
	} else if (ret != MLN_XML_ERROR_NOT_FOUND) {
		goto out;
	}
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, body, name, uri, &node);
	if (ret == MLN_XML_ERROR_NOT_FOUND) {
		ret = MlnXmlFindNode(MLN_HEAP_GENERAL, body, "Fault", MLN_NEMO_NS_SOAP, &node);
		isFault = 1;
	}
	if (ret != 0)
		goto out;
	if (decrypted)
		*encrypted |= MLN_NEMO_ENCRYPTED_BODY;
	*fault = isFault;
	*payload = node;
	node = NULL;
out:
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, node);
	return ret;
}

/*
 * A request's sender: the NameIdentifier of the assertion its role's
 * SecurityTokenReference names (NULL when none of them matches).
 */
static inline __attribute__((always_inline)) unsigned int MlnNemoReadRequester(
	MlnXmlNode *security, const MlnXmlNodeList *assertions, MlnBuffer **requester)
{
	MlnXmlNodeList *references = NULL;
	MlnXmlNode *keyIdentifier = NULL;
	MlnXmlNode *subject = NULL;
	MlnXmlNode *nameIdentifier = NULL;
	MlnXmlNode *reference;
	MlnXmlNode *assertion;
	MlnBuffer *usage = NULL;
	MlnBuffer *keyId = NULL;
	MlnBuffer *assertionId = NULL;
	MlnBuffer *name = NULL;
	unsigned short i;
	unsigned short j;
	int count;
	unsigned int ret;

	if (security == NULL || assertions == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	/* Sony bounds the references by the assertions' count */
	count = assertions->count;
	if (count <= 0) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, security, "SecurityTokenReference",
			      MLN_NEMO_NS_SECEXT, &references);
	if (ret != 0)
		goto out;
	for (i = 0; ; ) {
		reference = references->nodes[i];
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, reference, NULL, "nemosec:Usage", &usage);
		if (ret != 0)
			goto out;
		if (MlnBytesCompare(MLN_HEAP_GENERAL, MlnStrlen(MLN_NEMO_ROLE_USAGE),
				    MLN_NEMO_ROLE_USAGE, usage->size, usage->data) == 0)
			break;
		MlnBufferDestroy(MLN_HEAP_GENERAL, usage);
		usage = NULL;
		if (++i >= count) {
			ret = MLN_NEMO_ERROR_FORMAT;
			goto out;
		}
	}
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, reference, "KeyIdentifier", MLN_NEMO_NS_SECEXT,
			     &keyIdentifier);
	if (ret != 0)
		goto out;
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, keyIdentifier, &keyId);
	if (ret != 0)
		goto out;
	if (i >= count) {
		ret = MLN_NEMO_ERROR_FORMAT;
		goto out;
	}
	for (j = 0; j < count; j++) {
		assertion = assertions->nodes[j];
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, assertion, NULL, "AssertionID",
					 &assertionId);
		if (ret != 0)
			goto out;
		if (MlnBufferCompare(MLN_HEAP_GENERAL, keyId, assertionId) == 0) {
			ret = MlnXmlFindNode(MLN_HEAP_GENERAL, assertion,
					     "AttributeStatement/Subject", MLN_NEMO_NS_SAML,
					     &subject);
			if (ret != 0)
				goto out;
			ret = MlnXmlFindNode(MLN_HEAP_GENERAL, subject, "NameIdentifier",
					     MLN_NEMO_NS_SAML, &nameIdentifier);
			if (ret != 0)
				goto out;
			ret = MlnXmlGetText(MLN_HEAP_GENERAL, nameIdentifier, &name);
			if (ret != 0)
				goto out;
			break;
		}
		MlnBufferDestroy(MLN_HEAP_GENERAL, assertionId);
		assertionId = NULL;
	}
	*requester = name;
	name = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, usage);
	MlnBufferDestroy(MLN_HEAP_GENERAL, name);
	MlnBufferDestroy(MLN_HEAP_GENERAL, keyId);
	MlnBufferDestroy(MLN_HEAP_GENERAL, assertionId);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, nameIdentifier);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, keyIdentifier);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, subject);
	MlnXmlListFree(MLN_HEAP_GENERAL, references);
	return ret;
}

/* a license transfer request's sender, which has to hold the device role */
static inline __attribute__((always_inline)) unsigned int MlnNemoReadDeviceRole(
	MlnXmlNode *security)
{
	MlnXmlNodeList *assertions = NULL;
	MlnXmlNodeList *attributes;
	MlnXmlNode *assertion;
	MlnBuffer *attributeName;
	MlnBuffer *token = NULL;
	MlnBuffer *name = NULL;
	MlnBuffer role;
	unsigned short i;
	int k;
	unsigned int ret;

	role.size = 27;
	role.data = (unsigned char *)"urn:marlin:core:role:device";
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, security, "Assertion", MLN_NEMO_NS_SAML,
			      &assertions);
	if (ret != 0)
		return ret;
	for (i = 0; i < assertions->count; i++) {
		assertion = assertions->nodes[i];
		attributes = NULL;
		attributeName = NULL;
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, assertion, "AttributeStatement/Attribute",
				      MLN_NEMO_NS_SAML, &attributes);
		if (ret == 0 && attributes->count == 0)
			ret = MLN_NEMO_ERROR_FORMAT;
		if (ret != 0)
			goto fail;
		for (k = 0; k < attributes->count; k++) {
			ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, attributes->nodes[k],
						 MLN_NEMO_NS_SAML, "AttributeName",
						 &attributeName);
			if (ret != 0)
				goto fail;
			if (MlnBytesCompare(MLN_HEAP_GENERAL, 4, "role", attributeName->size,
					    attributeName->data) == 0) {
				MlnBufferDestroy(MLN_HEAP_GENERAL, attributeName);
				MlnXmlListFree(MLN_HEAP_GENERAL, attributes);
				ret = MlnXmlNodeToText(MLN_HEAP_GENERAL, assertion, &token);
				if (ret == 0)
					ret = MlnImpl_81042B58(token, 0, &role, &name);
				MlnXmlListFree(MLN_HEAP_GENERAL, assertions);
				MlnBufferDestroy(MLN_HEAP_GENERAL, token);
				MlnBufferDestroy(MLN_HEAP_GENERAL, name);
				return ret;
			}
			MlnBufferDestroy(MLN_HEAP_GENERAL, attributeName);
			attributeName = NULL;
		}
		MlnBufferDestroy(MLN_HEAP_GENERAL, attributeName);
		MlnXmlListFree(MLN_HEAP_GENERAL, attributes);
	}
	MlnXmlListFree(MLN_HEAP_GENERAL, assertions);
	return MLN_NEMO_ERROR_FORMAT;
fail:
	MlnBufferDestroy(MLN_HEAP_GENERAL, attributeName);
	MlnXmlListFree(MLN_HEAP_GENERAL, attributes);
	MlnXmlListFree(MLN_HEAP_GENERAL, assertions);
	return ret;
}

/*
 * The signature of a message read verified with the signing certificate
 * against the sender's name, each reference digest as read.
 */
static inline __attribute__((always_inline)) unsigned int MlnNemoReadVerify(
	MlnXmlNode *signature, MlnXmlNode *security, const MlnBuffer *peer,
	const MlnNemoPeerKey *signer, unsigned int usage, int signerKind, unsigned short count,
	MlnBuffer **ids, unsigned char (*digests)[MLN_NEMO_DIGEST_MAX])
{
	unsigned char *digestList[MLN_NEMO_READ_PARTS];
	int digestTypes[MLN_NEMO_READ_PARTS];
	int failed[MLN_NEMO_READ_PARTS];
	unsigned int ret;
	int i;

	if (security == NULL || peer == NULL || count > MLN_NEMO_READ_PARTS - 1)
		return MLN_NEMO_ERROR_PARAMETER;
	if (signature == NULL)
		return (usage & MLN_NEMO_USAGE_SIGNED) ? MLN_NEMO_ERROR_UNSECURED : 0;
	if (signer == NULL)
		return MLN_NEMO_ERROR_PARAMETER;
	ret = MlnImpl_81084F3C(peer, signer->certificate);
	if (ret != 0)
		return ret;
	for (i = 0; i < count; i++) {
		digestList[i] = digests[i];
		digestTypes[i] = 1;
	}
	ret = MlnImpl_8105BA50(signature, security, signerKind, count, ids, digestList,
			       digestTypes, signer->certificate, failed);
	if (ret != 0)
		return ret;
	for (i = 0; i < count; i++)
		if (failed[i] != 0)
			return MLN_NEMO_ERROR_FORMAT;
	return 0;
}

/* whether a message read carries, signed and encrypted, what its usage needs */
static inline __attribute__((always_inline)) int MlnNemoReadSecured(
	unsigned int kind, unsigned int step, unsigned char found, unsigned short signedParts,
	unsigned int encrypted, int details)
{
	unsigned char usage;

	if (MlnImpl_81072138(kind, step, &usage) != 0)
		return 0;
	if ((usage & MLN_NEMO_USAGE_TIMESTAMP) && !(found & MLN_NEMO_FOUND_TIMESTAMP))
		return 0;
	if (usage & MLN_NEMO_USAGE_FRESH) {
		if (MlnImpl_81085D84(kind, step) && !(found & MLN_NEMO_FOUND_NONCE))
			return 0;
		if (MlnImpl_810721BC(kind, step) && !(found & MLN_NEMO_FOUND_RETURNED_NONCE))
			return 0;
	}
	if (usage & MLN_NEMO_USAGE_SIGNED) {
		if ((signedParts & MLN_NEMO_SIGNED_REQUIRED) != MLN_NEMO_SIGNED_REQUIRED)
			return 0;
		if ((found & MLN_NEMO_FOUND_TIMESTAMP) && !(signedParts & MLN_NEMO_SIGNED_TIMESTAMP))
			return 0;
		if (details && !(signedParts & MLN_NEMO_SIGNED_FAULT))
			return 0;
		if ((usage & MLN_NEMO_USAGE_ENCRYPTED) && !(signedParts & MLN_NEMO_SIGNED_MESSAGE_KEY))
			return 0;
		if (MlnImpl_81085D84(kind, step) && !(signedParts & MLN_NEMO_SIGNED_NONCE))
			return 0;
		if (MlnImpl_810721BC(kind, step) && !(signedParts & MLN_NEMO_SIGNED_RETURNED_NONCE))
			return 0;
		if (step == MLN_NEMO_STEP_REQUEST && MlnImpl_81086254(kind) &&
		    !(signedParts & MLN_NEMO_SIGNED_CERTIFICATES))
			return 0;
		if ((step == MLN_NEMO_STEP_REQUEST || step == MLN_NEMO_STEP_RESPONSE) &&
		    !(signedParts & MLN_NEMO_SIGNED_MESSAGE_ID))
			return 0;
		if ((step == MLN_NEMO_STEP_CONFIRMATION || step == MLN_NEMO_STEP_RESPONSE) &&
		    !(signedParts & MLN_NEMO_SIGNED_ORIGINATES_FROM))
			return 0;
	}
	if ((usage & MLN_NEMO_USAGE_ENCRYPTED) &&
	    encrypted != (MLN_NEMO_ENCRYPTED_KEY | MLN_NEMO_ENCRYPTED_BODY))
		return 0;
	if (usage == 0x0f && !(signedParts & MLN_NEMO_SIGNED_TO_NODE))
		return 0;
	return 1;
}

/* a message read's error as NEMO's */
static inline __attribute__((always_inline)) unsigned int MlnNemoReadError(unsigned int ret)
{
	unsigned int domain;

	if (ret == 0x22090001 || ret == 0x23110001)
		return MLN_NEMO_ERROR_UNKNOWN_TIME;
	domain = ret & 0xFFFF0000;
	if (domain == 0x10010000 || domain == 0x22090000)
		return MLN_NEMO_ERROR_FORMAT;
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

/* the message keys a message read unwrapped, freed with their references */
static inline __attribute__((always_inline)) void MlnNemoMessageKeysFree(MlnNemoMessageKey *keys,
									  unsigned short count)
{
	MlnNemoMessageKey *entry;
	unsigned short j;
	int i;

	if (keys == NULL)
		return;
	for (i = 0; i < count; i++) {
		entry = &keys[i];
		if (entry->ids != NULL)
			for (j = 0; j < entry->idCount; j++)
				MlnBufferDestroy(MLN_HEAP_GENERAL, entry->ids[j]);
		MlnFree(MLN_HEAP_GENERAL, entry->ids);
		MlnImpl_81080C80(entry->key);
	}
	MlnFree(MLN_HEAP_GENERAL, keys);
}

/* a certificate chain read, freed */
static inline __attribute__((always_inline)) void MlnNemoCertificatesFree(MlnBuffer **certificates,
									   int count)
{
	unsigned short i;

	if (certificates == NULL)
		return;
	for (i = 0; i < count; i++)
		if (certificates[i] != NULL)
			MlnBufferDestroy(MLN_HEAP_GENERAL, certificates[i]);
	MlnFree(MLN_HEAP_GENERAL, certificates);
}

/*
 * A message of a kind read at a step: the SOAP envelope parsed, its Security
 * header decrypted and checked (the step declared, the profile, the header's
 * parts, the timestamp, the nonces, the recipient, the certificates and
 * assertions, the signature over every part the usage needs), the peer's keys
 * and nonce kept in the message and the payload named returned in *payloadOut.
 * A fault's exceptions are kept in the message instead (MLN_NEMO_ERROR_FAULT).
 */
unsigned int MlnImpl_81074EC4(unsigned int size, const void *data, MlnNemoMessage *message,
			      const char *name, const char *uri, unsigned char kind,
			      unsigned char step, int signerKind, int peerKind,
			      MlnXmlNode **payloadOut)
{
	unsigned char digests[MLN_NEMO_READ_REFERENCES][MLN_NEMO_DIGEST_MAX];
	MlnBuffer *ids[MLN_NEMO_READ_REFERENCES] = { NULL };
	unsigned char keyDigest[MLN_NEMO_DIGEST_MAX];
	unsigned char secondDigest[MLN_NEMO_DIGEST_MAX];
	void *hci = NULL;
	MlnXmlNode *document = NULL;
	MlnXmlNode *envelope = NULL;
	MlnXmlNode *header = NULL;
	MlnXmlNode *security = NULL;
	MlnXmlNode *body = NULL;
	MlnXmlNode *signature = NULL;
	MlnXmlNode *declaration = NULL;
	MlnXmlNode *profile;
	MlnXmlNode *payload = NULL;
	MlnXmlNodeList *assertions;
	MlnNemoMessageKey *keys = NULL;
	MlnNemoHeaderNodes *headerNodes = NULL;
	MlnNemoFaults *faults = NULL;
	MlnBuffer *keyToken = NULL;
	MlnBuffer *signingCertificate = NULL;
	MlnBuffer *encryptionCertificate = NULL;
	MlnBuffer *keyId = NULL;
	MlnBuffer *secondId = NULL;
	MlnBuffer **certificates = NULL;
	MlnBuffer **secondCertificates = NULL;
	MlnBuffer *nonce = NULL;
	MlnBuffer *requester = NULL;
	MlnBuffer *id;
	unsigned int found = 0;
	unsigned int signedParts = 0;
	unsigned int encrypted = 0;
	unsigned int readStep = 0;
	unsigned int present;
	unsigned char usage;
	int certificateCount = 0;
	int secondCertificateCount = 0;
	int keyCount = 0;
	int count = 0;
	int digestSize = 0;
	int headerCount = 0;
	int faultRead;
	int details = 0;
	int isFault = 0;
	int missing;
	int bad;
	unsigned short k;
	unsigned int ret;
	unsigned int r;
	int i;

	ret = MlnImpl_81080BFC(&hci);
	if (ret != 0)
		goto out;
	if (data == NULL || message == NULL || name == NULL || uri == NULL ||
	    message->credentials == NULL) {
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

	/* the envelope, its body and its headers */
	ret = MlnXmlParse(MLN_HEAP_GENERAL, size, data, &document);
	if (ret != 0)
		goto out;
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, document, "Envelope", MLN_NEMO_NS_SOAP, &envelope);
	if (ret != 0)
		goto out;
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, envelope, "Body", MLN_NEMO_NS_SOAP, &body);
	if (ret != 0)
		goto out;
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, envelope, "Header", MLN_NEMO_NS_SOAP, &header);
	if (ret == MLN_XML_ERROR_NOT_FOUND) {
		/* only a SOAP fault comes without a header */
		r = MlnImpl_81083F90(body, NULL, message);
		if (r != 0)
			ret = r;
		goto out;
	}
	if (ret != 0)
		goto out;
	ret = MlnImpl_81083F90(body, header, message);
	if (ret != 0)
		goto out;
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, header, "Security", MLN_NEMO_NS_SECEXT, &security);
	if (ret != 0)
		goto out;
	ret = MlnNemoReadProtocol(security, &declaration, &readStep);
	if (ret != 0)
		goto out;

	/* the message keys, the Security header decrypted, the signature's digest */
	ret = MlnImpl_81084204(security, message, &keys, &keyCount);
	if (ret != 0)
		goto out;
	faultRead = readStep == MLN_NEMO_STEP_FAULT;
	ret = MlnNemoReadDecrypt(security, usage, keys, keyCount);
	if (ret != 0 && !(ret == MLN_NEMO_ERROR_UNSECURED && faultRead))
		goto out;
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, security, "Signature", MLN_NEMO_NS_DSIG, &signature);
	if (ret == 0) {
		ret = MlnImpl_8105BF78(signature, &digestSize);
		if (ret == 3)
			ret = MLN_NEMO_ERROR_PARAMETER;
		else if (ret == 0x22090006)
			ret = MLN_NEMO_ERROR_FORMAT;
		if (ret != 0)
			goto out;
	} else if (ret != MLN_XML_ERROR_NOT_FOUND) {
		goto out;
	}

	/* the protocol declaration's reference, the step */
	if (count > MLN_NEMO_READ_PARTS) {
		ret = 5;
		goto out;
	}
	id = NULL;
	if (declaration == NULL)
		ret = MLN_NEMO_ERROR_PARAMETER;
	else
		ret = MlnNemoReadReference(declaration, digestSize, digests[count], &id);
	if (ret != 0) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, id);
		goto out;
	}
	ids[count] = id;
	if (id != NULL) {
		count++;
		signedParts |= MLN_NEMO_SIGNED_PROTOCOL;
	}
	if (step != readStep && readStep != MLN_NEMO_STEP_FAULT) {
		ret = MLN_NEMO_ERROR_STEP;
		goto out;
	}

	/* the profile */
	if (count > MLN_NEMO_READ_PARTS) {
		ret = 5;
		goto out;
	}
	id = NULL;
	profile = NULL;
	if (security == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
	} else {
		ret = MlnNemoReadProfile(security, &profile);
		if (ret == 0)
			ret = MlnNemoReadReference(profile, digestSize, digests[count], &id);
	}
	if (ret == 0) {
		ids[count] = id;
		if (id != NULL) {
			count++;
			signedParts |= MLN_NEMO_SIGNED_PROFILE;
		}
		id = NULL;
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, profile);
	if (ret != 0)
		goto out;

	/* the SOAP header's parts, as the step needs them */
	MlnImpl_810798CC(&message->header);
	ret = MlnImpl_81078688(header, &message->header, &headerNodes);
	if (ret != 0)
		goto out;
	present = message->header.present;
	if (!(present & MLN_NEMO_PART_ACTION)) {
		bad = 1;
	} else {
		bad = !(present & MLN_NEMO_PART_MESSAGE_ID);
		if ((readStep == MLN_NEMO_STEP_RESPONSE || readStep == MLN_NEMO_STEP_CONFIRMATION ||
		     readStep == MLN_NEMO_STEP_FAULT) && !(present & MLN_NEMO_PART_ORIGINATES_FROM))
			bad = 1;
		if (readStep == MLN_NEMO_STEP_REQUEST && kind != MLN_NEMO_KIND_TRANSFER &&
		    (present & (MLN_NEMO_PART_ORIGINATES_FROM | MLN_NEMO_PART_RELATES_TO)))
			bad = 1;
	}
	if (bad && !faultRead) {
		ret = MLN_NEMO_ERROR_HEADER;
		goto out;
	}
	ret = MlnImpl_810789DC(&message->header, readStep);
	if (ret != 0 &&
	    !((ret == MLN_NEMO_ERROR_RELATES_TO || ret == MLN_NEMO_ERROR_PARAMETER) && faultRead))
		goto out;

	/* the header nodes' references, after the parts' */
	ret = MlnNemoReadHeaderReferences(headerNodes, digestSize, &digests[MLN_NEMO_READ_PARTS],
					  &ids[MLN_NEMO_READ_PARTS], &signedParts, &headerCount);
	if (ret != 0)
		goto out;
	for (k = 0; k < headerCount; k++) {
		if (count > MLN_NEMO_READ_PARTS) {
			ret = 5;
			goto out;
		}
		MlnMemcpy(digests[count], digests[MLN_NEMO_READ_PARTS + k], MLN_NEMO_DIGEST_MAX);
		ids[count] = ids[MLN_NEMO_READ_PARTS + k];
		ids[MLN_NEMO_READ_PARTS + k] = NULL;
		count++;
	}

	/* the timestamp, the nonces, the recipient, a fault's details */
	if (count > MLN_NEMO_READ_PARTS) {
		ret = 5;
		goto out;
	}
	ret = MlnNemoReadTimestamp(security, message, digestSize, digests[count], ids, &count,
				   &found, &signedParts);
	if (ret != 0)
		goto out;
	if (count > MLN_NEMO_READ_PARTS - 1) {
		ret = 5;
		goto out;
	}
	ret = MlnNemoReadNonces(security, kind, readStep, message->nonce, &found, &signedParts,
				digestSize, digests, ids, &count, &nonce);
	if (ret != 0)
		goto out;
	if (count > MLN_NEMO_READ_PARTS) {
		ret = 5;
		goto out;
	}
	ret = MlnNemoReadToNode(security, message->credentials->first, digestSize, digests[count],
				ids, &count, &signedParts, &missing);
	if (ret != 0 && !(missing && faultRead))
		goto out;
	if (count > MLN_NEMO_READ_PARTS) {
		ret = 5;
		goto out;
	}
	ret = MlnNemoReadFaultDetails(header, readStep, digestSize, digests[count], ids, &count,
				      &signedParts, &details);
	if (ret != 0)
		goto out;

	/* the certificates and the message key's token */
	ret = MlnImpl_81086284(security, kind, readStep, signerKind, peerKind, &encrypted,
			       &signedParts, &keyToken, &signingCertificate, &encryptionCertificate,
			       keyDigest, secondDigest, digestSize, digestSize, &keyId, &secondId,
			       &certificates, &certificateCount, &secondCertificates,
			       &secondCertificateCount);
	if (ret != 0 && !(ret == MLN_NEMO_ERROR_FORMAT && faultRead))
		goto out;
	if (keyToken != NULL) {
		ret = MlnNemoReadKeyToken(keyToken, keys, keyCount);
		if (ret != 0)
			goto out;
	}
	if (keyId != NULL) {
		MlnMemcpy(digests[count], keyDigest, MLN_NEMO_DIGEST_MAX);
		ids[count++] = keyId;
		keyId = NULL;
	}
	if (secondId != NULL) {
		MlnMemcpy(digests[count], secondDigest, MLN_NEMO_DIGEST_MAX);
		ids[count++] = secondId;
		secondId = NULL;
	}

	/* the payload and the body's reference */
	ret = MlnNemoReadBody(body, name, uri, keys, keyCount, &encrypted, &payload, &isFault);
	if (ret != 0)
		goto out;
	if (details && !isFault) {
		ret = MLN_NEMO_ERROR_FORMAT;
		goto out;
	}
	if (count > MLN_NEMO_READ_PARTS) {
		ret = 5;
		goto out;
	}
	id = NULL;
	if (body == NULL)
		ret = MLN_NEMO_ERROR_PARAMETER;
	else
		ret = MlnNemoReadReference(body, digestSize, digests[count], &id);
	if (ret != 0) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, id);
		goto out;
	}
	ids[count] = id;
	if (id != NULL) {
		count++;
		signedParts |= MLN_NEMO_SIGNED_BODY;
	}

	/* a fault's exceptions */
	if (payload == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	if (readStep == MLN_NEMO_STEP_FAULT) {
		if (!isFault) {
			ret = MLN_NEMO_ERROR_FORMAT;
			goto out;
		}
		ret = MlnImpl_81079594(payload, header, &faults);
	} else if (isFault) {
		ret = MlnImpl_81079594(payload, NULL, &faults);
	}
	if (ret != 0)
		goto out;

	/* the assertions, a request's sender named by its role's */
	if (message->bundles != NULL) {
		ret = 5;
		goto out;
	}
	if (security == NULL) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	assertions = NULL;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, security, "Assertion", MLN_NEMO_NS_SAML,
			      &assertions);
	if (ret == 0) {
		message->bundles = assertions;
		assertions = NULL;
	}
	MlnXmlListFree(MLN_HEAP_GENERAL, assertions);
	if (ret == MLN_XML_ERROR_NOT_FOUND) {
		if (readStep == MLN_NEMO_STEP_REQUEST) {
			ret = MLN_NEMO_ERROR_FORMAT;
			goto out;
		}
	} else if (ret != 0) {
		goto out;
	} else if (readStep == MLN_NEMO_STEP_REQUEST) {
		ret = MlnNemoReadRequester(security, message->bundles, &requester);
		if (ret != 0)
			goto out;
	}

	/* the signer's key, a transfer's device role, the signature */
	if (signingCertificate != NULL) {
		MlnImpl_81079A2C(message->keys[1]);
		message->keys[1] = NULL;
		ret = MlnImpl_81084FC8(signingCertificate, &message->keys[1]);
		if (ret != 0)
			goto out;
	}
	if (kind == MLN_NEMO_KIND_TRANSFER && readStep == MLN_NEMO_STEP_REQUEST) {
		ret = MlnNemoReadDeviceRole(security);
		if (ret != 0)
			goto out;
	}
	if (requester != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, message->peer);
		message->peer = MlnBufferCreate(MLN_HEAP_GENERAL, requester->size, requester->data);
	}
	ret = MlnNemoReadVerify(signature, security, message->peer, message->keys[1], usage,
				signerKind, (unsigned short)count, ids, digests);
	if (ret != 0)
		goto out;
	if (!MlnNemoReadSecured(kind, readStep, found, signedParts, encrypted, details) &&
	    readStep != MLN_NEMO_STEP_FAULT) {
		ret = MLN_NEMO_ERROR_UNSECURED;
		goto out;
	}

	/* what the message gave kept */
	if (message->peerNonce != NULL) {
		ret = 5;
		goto out;
	}
	message->peerNonce = nonce;
	nonce = NULL;
	if (encryptionCertificate != NULL) {
		MlnImpl_81079A2C(message->keys[0]);
		message->keys[0] = NULL;
		ret = MlnImpl_81084FC8(encryptionCertificate, &message->keys[0]);
		if (ret != 0)
			goto out;
	}
	if (isFault) {
		message->faults = faults;
		faults = NULL;
		ret = MLN_NEMO_ERROR_FAULT;
	} else {
		*payloadOut = payload;
		payload = NULL;
		ret = 0;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, requester);
	MlnBufferDestroy(MLN_HEAP_GENERAL, nonce);
	MlnNemoMessageKeysFree(keys, keyCount);
	MlnImpl_810794DC(faults);
	for (i = 0; i < MLN_NEMO_READ_PARTS; i++)
		MlnBufferDestroy(MLN_HEAP_GENERAL, ids[i]);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, payload);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, body);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, header);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, security);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, signature);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, declaration);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, document);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, envelope);
	MlnImpl_81079854(headerNodes);
	MlnBufferDestroy(MLN_HEAP_GENERAL, keyToken);
	MlnBufferDestroy(MLN_HEAP_GENERAL, signingCertificate);
	MlnBufferDestroy(MLN_HEAP_GENERAL, encryptionCertificate);
	MlnBufferDestroy(MLN_HEAP_GENERAL, keyId);
	MlnBufferDestroy(MLN_HEAP_GENERAL, secondId);
	MlnNemoCertificatesFree(certificates, certificateCount);
	MlnNemoCertificatesFree(secondCertificates, secondCertificateCount);
	for (i = MLN_NEMO_READ_PARTS; i < MLN_NEMO_READ_REFERENCES; i++)
		if (ids[i] != NULL)
			MlnBufferDestroy(MLN_HEAP_GENERAL, ids[i]);
	MlnImpl_81080C1C(hci);
	return MlnNemoReadError(ret);
}

#define MLN_NEMO_NS_ADDRESSING	"http://www.w3.org/2005/08/addressing"
#define MLN_NEMO_ORIGINATES_FROM "http://nemo.intertrust.com/2004/addressing/originatesFrom"
#define MLN_NEMO_REPLY		"http://www.w3.org/2005/08/addressing/reply"
#define MLN_NEMO_HEADER_NODES_MAX 16

/*
 * A WS-Addressing header read into its part (an endpoint's Address for From,
 * ReplyTo and FaultTo), its element kept among the header's nodes.
 */
__attribute__((noinline))
unsigned int MlnImpl_81077DE8(MlnXmlNode *soapHeader, const char *name, unsigned int part,
			      unsigned int *present, MlnNemoHeaderNodes *nodes, unsigned int *index,
			      MlnBuffer **value)
{
	MlnXmlNodeList *elements = NULL;
	MlnXmlNodeList *addresses = NULL;
	MlnXmlNode *node;
	MlnBuffer *text = NULL;
	MlnNemoHeaderNode *entry;
	unsigned int ret;

	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, soapHeader, name, MLN_NEMO_NS_ADDRESSING,
			      &elements);
	if (ret != 0) {
		if (ret == MLN_XML_ERROR_NOT_FOUND)
			ret = 0;
		goto out;
	}
	if (elements->count != 1) {
		ret = MLN_NEMO_ERROR_FORMAT;
		goto out;
	}
	if (part == MLN_NEMO_PART_FROM || part == MLN_NEMO_PART_REPLY_TO ||
	    part == MLN_NEMO_PART_FAULT_TO) {
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, elements->nodes[0], "Address",
				      MLN_NEMO_NS_ADDRESSING, &addresses);
		if (ret != 0)
			goto out;
		if (addresses->count != 1) {
			ret = MLN_NEMO_ERROR_FORMAT;
			goto out;
		}
		node = addresses->nodes[0];
	} else {
		node = elements->nodes[0];
	}
	ret = MlnXmlGetText(MLN_HEAP_GENERAL, node, &text);
	if (ret != 0)
		goto out;
	*value = text;
	text = NULL;
	*present |= part;
	if (*index >= MLN_NEMO_HEADER_NODES_MAX) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	nodes->items[*index].type = part;
	entry = &nodes->items[*index];
	entry->node = MlnXmlNodeRetain(MLN_HEAP_GENERAL, node);
	*index += 1;
	nodes->count++;
out:
	MlnXmlListFree(MLN_HEAP_GENERAL, elements);
	MlnXmlListFree(MLN_HEAP_GENERAL, addresses);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	return ret;
}

/* an element of the header made from a part: its text, or an endpoint holding it */
static inline __attribute__((always_inline)) unsigned int MlnNemoHeaderElement(
	const char *name, int attributeCount, const MlnXmlBuildAttribute *attributes,
	MlnBuffer *value, MlnXmlNode **out)
{
	MlnXmlBuildContent content;

	content.kind = MLN_XML_BUILD_BUFFER;
	content.value.buffer = value;
	return MlnXmlElementBuild(MLN_HEAP_GENERAL, name, "wsa", MLN_NEMO_NS_ADDRESSING,
				  attributeCount, attributes, 1, &content, 0, out);
}

static inline __attribute__((always_inline)) unsigned int MlnNemoHeaderEndpoint(
	const char *name, MlnBuffer *value, MlnXmlNode **address, MlnXmlNode **out)
{
	MlnXmlBuildContent content;
	unsigned int ret;

	ret = MlnNemoHeaderElement("Address", 0, NULL, value, address);
	if (ret != 0)
		return ret;
	content.kind = MLN_XML_BUILD_NODE;
	content.value.node = *address;
	return MlnXmlElementBuild(MLN_HEAP_GENERAL, name, "wsa", MLN_NEMO_NS_ADDRESSING, 0, NULL,
				  1, &content, 0, out);
}

/* the SOAP header's WS-Addressing elements made from a header's parts */
__attribute__((noinline))
unsigned int MlnImpl_81077FAC(MlnNemoHeader *header, MlnNemoHeaderNodes **out)
{
	MlnNemoHeaderNodes *nodes;
	MlnXmlNode *from = NULL;
	MlnXmlNode *replyTo = NULL;
	MlnXmlNode *faultTo = NULL;
	MlnXmlBuildAttribute relationship;
	unsigned int present;
	unsigned int n = 0;
	unsigned int ret;

	if (header == NULL || header->count == 0) {
		*out = NULL;
		return 0;
	}
	nodes = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*nodes));
	if (nodes == NULL ||
	    (nodes->items = MlnAllocate(MLN_HEAP_GENERAL,
					header->count * sizeof(*nodes->items))) == NULL) {
		ret = 2;
		goto out;
	}
	nodes->count = 0;
	present = header->present;
	if (present & MLN_NEMO_PART_TO) {
		ret = MlnNemoHeaderElement("To", 0, NULL,
					   MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_TO),
					   &nodes->items[n].node);
		if (ret != 0)
			goto out;
		nodes->items[n++].type = MLN_NEMO_PART_TO;
		nodes->count++;
		present = header->present;
	}
	if (present & MLN_NEMO_PART_ACTION) {
		ret = MlnNemoHeaderElement("Action", 0, NULL,
					   MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_ACTION),
					   &nodes->items[n].node);
		if (ret != 0)
			goto out;
		nodes->items[n++].type = MLN_NEMO_PART_ACTION;
		nodes->count++;
		present = header->present;
	}
	if (present & MLN_NEMO_PART_ORIGINATES_FROM) {
		relationship.name = "RelationshipType";
		relationship.value = MLN_NEMO_ORIGINATES_FROM;
		ret = MlnNemoHeaderElement("RelatesTo", 1, &relationship,
					   MLN_NEMO_HEADER_PART(header,
								MLN_NEMO_PART_ORIGINATES_FROM),
					   &nodes->items[n].node);
		if (ret != 0)
			goto out;
		nodes->items[n++].type = MLN_NEMO_PART_ORIGINATES_FROM;
		nodes->count++;
		present = header->present;
	}
	if (present & MLN_NEMO_PART_RELATES_TO) {
		relationship.name = "RelationshipType";
		relationship.value = MLN_NEMO_REPLY;
		ret = MlnNemoHeaderElement("RelatesTo", 1, &relationship,
					   MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_RELATES_TO),
					   &nodes->items[n].node);
		if (ret != 0)
			goto out;
		nodes->items[n++].type = MLN_NEMO_PART_RELATES_TO;
		nodes->count++;
		present = header->present;
	}
	if (present & MLN_NEMO_PART_MESSAGE_ID) {
		ret = MlnNemoHeaderElement("MessageID", 0, NULL,
					   MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_MESSAGE_ID),
					   &nodes->items[n].node);
		if (ret != 0)
			goto out;
		nodes->items[n++].type = MLN_NEMO_PART_MESSAGE_ID;
		nodes->count++;
		present = header->present;
	}
	if (present & MLN_NEMO_PART_FROM) {
		ret = MlnNemoHeaderEndpoint("From", MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_FROM),
					    &from, &nodes->items[n].node);
		if (ret != 0)
			goto out;
		nodes->items[n++].type = MLN_NEMO_PART_FROM;
		nodes->count++;
		present = header->present;
	}
	if (present & MLN_NEMO_PART_REPLY_TO) {
		ret = MlnNemoHeaderEndpoint("ReplyTo",
					    MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_REPLY_TO),
					    &replyTo, &nodes->items[n].node);
		if (ret != 0)
			goto out;
		nodes->items[n++].type = MLN_NEMO_PART_REPLY_TO;
		nodes->count++;
		present = header->present;
	}
	if (present & MLN_NEMO_PART_FAULT_TO) {
		ret = MlnNemoHeaderEndpoint("FaultTo",
					    MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_FAULT_TO),
					    &faultTo, &nodes->items[n].node);
		if (ret != 0)
			goto out;
		nodes->items[n].type = MLN_NEMO_PART_FAULT_TO;
		nodes->count++;
	}
	*out = nodes;
	nodes = NULL;
	ret = 0;
out:
	MlnImpl_81079854(nodes);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, from);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, replyTo);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, faultTo);
	return ret;
}

/* a SOAP header's WS-Addressing parts read into a header, their elements kept */
__attribute__((noinline))
unsigned int MlnImpl_81078688(MlnXmlNode *soapHeader, MlnNemoHeader *header,
			      MlnNemoHeaderNodes **out)
{
	MlnNemoHeaderNodes *nodes = NULL;
	MlnXmlNodeList *list = NULL;
	MlnXmlNode *relatesTo;
	MlnBuffer *relationship;
	unsigned int index = 0;
	unsigned int ret;

	if (header == NULL || soapHeader == NULL) {
		*out = NULL;
		return 0;
	}
	/* a header to read into holds no more than a request's MessageID */
	if (header->present & ~MLN_NEMO_PART_REQUEST_ID) {
		ret = MLN_NEMO_ERROR_PARAMETER;
		goto out;
	}
	nodes = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*nodes));
	if (nodes == NULL ||
	    (nodes->items = MlnAllocate(MLN_HEAP_GENERAL,
					MLN_NEMO_HEADER_NODES_MAX * sizeof(*nodes->items))) == NULL) {
		ret = 2;
		goto out;
	}
	nodes->count = 0;
	ret = MlnImpl_81077DE8(soapHeader, "To", MLN_NEMO_PART_TO, &header->present, nodes, &index,
			       &MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_TO));
	if (ret != 0)
		goto out;
	ret = MlnImpl_81077DE8(soapHeader, "Action", MLN_NEMO_PART_ACTION, &header->present, nodes,
			       &index, &MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_ACTION));
	if (ret != 0)
		goto out;

	/* a RelatesTo, either relationship read as the originatesFrom */
	relatesTo = NULL;
	relationship = NULL;
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, soapHeader, "RelatesTo", MLN_NEMO_NS_ADDRESSING,
			     &relatesTo);
	if (ret == 0) {
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, relatesTo, NULL, "RelationshipType",
					 &relationship);
		if (ret != 0)
			goto relates;
		/* Sony compares the buffer itself, not its data */
		if (!(relationship->size == 57 &&
		      MlnMemcmp(relationship, MLN_NEMO_ORIGINATES_FROM, 57) != 0) &&
		    (relationship->size != 42 || MlnMemcmp(relationship, MLN_NEMO_REPLY, 42) == 0)) {
			ret = MLN_NEMO_ERROR_FORMAT;
			goto relates;
		}
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, relatesTo,
				    &MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_ORIGINATES_FROM));
		if (ret != 0)
			goto relates;
		header->present |= MLN_NEMO_PART_ORIGINATES_FROM;
		if (index >= MLN_NEMO_HEADER_NODES_MAX) {
			ret = MLN_NEMO_ERROR_PARAMETER;
			goto relates;
		}
		nodes->items[index].type = MLN_NEMO_PART_ORIGINATES_FROM;
		nodes->items[index++].node = relatesTo;
		relatesTo = NULL;
		nodes->count++;
	} else if (ret == MLN_XML_ERROR_NOT_FOUND) {
		ret = 0;
	}
relates:
	MlnBufferDestroy(MLN_HEAP_GENERAL, relationship);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, relatesTo);
	if (ret != 0)
		goto out;

	ret = MlnImpl_81077DE8(soapHeader, "MessageID", MLN_NEMO_PART_MESSAGE_ID, &header->present,
			       nodes, &index,
			       &MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_MESSAGE_ID));
	if (ret != 0)
		goto out;
	ret = MlnImpl_81077DE8(soapHeader, "From", MLN_NEMO_PART_FROM, &header->present, nodes,
			       &index, &MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_FROM));
	if (ret != 0)
		goto out;
	ret = MlnImpl_81077DE8(soapHeader, "ReplyTo", MLN_NEMO_PART_REPLY_TO, &header->present,
			       nodes, &index, &MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_REPLY_TO));
	if (ret != 0)
		goto out;
	ret = MlnImpl_81077DE8(soapHeader, "FaultTo", MLN_NEMO_PART_FAULT_TO, &header->present,
			       nodes, &index, &MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_FAULT_TO));
	if (ret != 0)
		goto out;
	header->count += nodes->count;
	*out = nodes;
	nodes = NULL;
out:
	MlnXmlListFree(MLN_HEAP_GENERAL, list);
	MlnImpl_81079854(nodes);
	return ret;
}

/*
 * A header's RelatesTo checked: a request's against the originatesFrom it
 * has to carry, its MessageID then kept as the request's; a reply's against
 * the request's MessageID.
 */
__attribute__((noinline))
unsigned int MlnImpl_810789DC(MlnNemoHeader *header, unsigned int step)
{
	MlnBuffer *messageId;
	MlnBuffer *expected;
	MlnBuffer *originatesFrom;
	MlnBuffer *requestId;

	if (header == NULL)
		return MLN_NEMO_ERROR_PARAMETER;
	if (step == MLN_NEMO_STEP_REQUEST) {
		messageId = MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_MESSAGE_ID);
		if (messageId == NULL)
			return MLN_NEMO_ERROR_PARAMETER;
		expected = MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_EXPECTED_ORIGIN);
		if (expected != NULL) {
			originatesFrom = MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_ORIGINATES_FROM);
			if (originatesFrom == NULL ||
			    MlnBufferCompare(MLN_HEAP_GENERAL, originatesFrom, expected) != 0)
				return MLN_NEMO_ERROR_RELATES_TO;
		}
		MlnBufferDestroy(MLN_HEAP_GENERAL,
				 MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_REQUEST_ID));
		MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_REQUEST_ID) = NULL;
		messageId = MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_MESSAGE_ID);
		MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_REQUEST_ID) =
			MlnBufferCreate(MLN_HEAP_GENERAL, messageId->size, messageId->data);
		if (MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_REQUEST_ID) == NULL)
			return 2;
		header->count++;
		header->present |= MLN_NEMO_PART_REQUEST_ID;
		return 0;
	}
	originatesFrom = MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_ORIGINATES_FROM);
	if (originatesFrom == NULL)
		return MLN_NEMO_ERROR_RELATES_TO;
	requestId = MLN_NEMO_HEADER_PART(header, MLN_NEMO_PART_REQUEST_ID);
	if (requestId == NULL)
		return MLN_NEMO_ERROR_RELATES_TO;
	if (MlnBufferCompare(MLN_HEAP_GENERAL, originatesFrom, requestId) != 0)
		return MLN_NEMO_ERROR_RELATES_TO;
	return 0;
}

#define MLN_NEMO_FAULTS_READ_MAX	25

/* a SOAP Body holding a payload */
unsigned int MlnImpl_81078AB8(MlnXmlNode *body, MlnXmlNode **out)
{
	MlnXmlBuildAttribute attributes[3];
	MlnXmlBuildContent content;

	content.kind = MLN_XML_BUILD_NODE;
	content.value.node = body;
	attributes[0].name = "wsu:Id";
	attributes[0].value = "soapBody";
	attributes[1].name = "xmlns:wsu";
	attributes[1].value = MLN_NEMO_NS_UTILITY;
	attributes[2].name = "xmlns:senv";
	attributes[2].value = MLN_NEMO_NS_SOAP;
	return MlnXmlElementBuild(MLN_HEAP_GENERAL, "Body", "senv", NULL, 3, attributes, 1,
				  &content, 0, out);
}

/* a SOAP Envelope of a header's nodes and a body, serialized */
__attribute__((noinline))
unsigned int MlnImpl_81078B5C(MlnXmlNode **header, unsigned char count, MlnXmlNode *body,
			      MlnBuffer **out)
{
	MlnXmlBuildContent headerContents[MLN_NEMO_HEADER_NODES_MAX];
	MlnXmlBuildContent contents[2];
	MlnXmlBuildAttribute xenc;
	MlnXmlNode *nodes[3];		/* the Header, the Body, the Envelope */
	unsigned int n;
	unsigned int ret;
	unsigned int i;

	MlnMemset(nodes, 0, sizeof(nodes));
	n = 0;
	if (header != NULL) {
		if (count > MLN_NEMO_HEADER_NODES_MAX) {
			ret = MLN_NEMO_ERROR_PARAMETER;
			goto out;
		}
		for (i = 0; i < count; i++) {
			headerContents[i].kind = MLN_XML_BUILD_NODE;
			headerContents[i].value.node = header[i];
		}
		ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Header", "senv", NULL, 0, NULL, count,
					 headerContents, 0, &nodes[0]);
		if (ret != 0)
			goto out;
		n = 1;
	}
	if (body != NULL) {
		ret = MlnImpl_81078AB8(body, &nodes[n]);
		if (ret != 0)
			goto out;
		n++;
	}
	for (i = 0; i < n; i++) {
		contents[i].kind = MLN_XML_BUILD_NODE;
		contents[i].value.node = nodes[i];
	}
	xenc.name = "xmlns:xenc";
	xenc.value = MLN_NEMO_NS_XENC;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Envelope", "senv", MLN_NEMO_NS_SOAP, 1, &xenc, n,
				 contents, 1, &nodes[2]);
	if (ret != 0)
		goto out;
	/* the Envelope holds them now */
	for (i = 0; i < n; i++)
		nodes[i] = NULL;
	ret = MlnXmlNodeSerialize(MLN_HEAP_GENERAL, nodes[2], NULL, NULL, out);
out:
	if (ret != 0)
		ret = MlnImpl_8107A340(ret);
	if (nodes[0] != NULL)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, nodes[0]);
	if (nodes[1] != NULL)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, nodes[1]);
	if (nodes[2] != NULL)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, nodes[2]);
	return ret;
}

/* a payload's XML in a BusinessToken of a service's namespace */
unsigned int MlnImpl_81078EB8(const MlnBuffer *xml, const char *prefix, const char *uri,
			      MlnXmlNode **node)
{
	MlnXmlBuildContent content;
	MlnXmlNode *token = NULL;
	MlnBuffer *copy;
	unsigned int ret;

	copy = MlnBufferCreate(MLN_HEAP_GENERAL, xml->size, xml->data);
	if (copy == NULL) {
		ret = 2;
		goto out;
	}
	content.kind = MLN_XML_BUILD_BUFFER;
	content.value.buffer = copy;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "BusinessToken", prefix, uri, 0, NULL, 1,
				 &content, 1, &token);
	if (ret == 0) {
		/* the token holds the copy now */
		copy = NULL;
		*node = token;
		token = NULL;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, copy);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, token);
	return ret;
}

/* a ServiceException read: its name, RedirectURL and Details, each optional */
__attribute__((noinline))
unsigned int MlnImpl_81078F70(MlnXmlNode *exception, MlnNemoFault **out)
{
	MlnXmlNode *redirect = NULL;
	MlnXmlNode *details = NULL;
	MlnNemoFault *fault;
	unsigned int ret;

	fault = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*fault));
	if (fault == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(fault, 0, sizeof(*fault));
	ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, exception, NULL, "name", &fault->parts[MLN_NEMO_FAULT_NAME]);
	if (ret != 0 && ret != MLN_XML_ERROR_NOT_FOUND)
		goto fail;
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, exception, "RedirectURL", MLN_NEMO_NS_EXCEPTIONS,
			     &redirect);
	if (ret == 0)
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, redirect, &fault->parts[MLN_NEMO_FAULT_REDIRECT_URL]);
	if (ret != 0 && ret != MLN_XML_ERROR_NOT_FOUND)
		goto fail;
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, exception, "Details", MLN_NEMO_NS_EXCEPTIONS,
			     &details);
	if (ret == 0)
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, details, &fault->parts[MLN_NEMO_FAULT_DETAILS]);
	if (ret != 0 && ret != MLN_XML_ERROR_NOT_FOUND)
		goto fail;
	*out = fault;
	ret = 0;
	goto out;
fail:
	MlnBufferDestroy(MLN_HEAP_GENERAL, fault->parts[MLN_NEMO_FAULT_NAME]);
	MlnBufferDestroy(MLN_HEAP_GENERAL, fault->parts[MLN_NEMO_FAULT_REDIRECT_URL]);
	MlnBufferDestroy(MLN_HEAP_GENERAL, fault->parts[MLN_NEMO_FAULT_DETAILS]);
	MlnFree(MLN_HEAP_GENERAL, fault);
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, redirect);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, details);
	return ret;
}

/* random bytes from the secure generator, a word at a time */
__attribute__((noinline))
unsigned int MlnImpl_810790EC(int size, unsigned char *data)
{
	void *random = NULL;
	unsigned int word;
	unsigned int ret;
	int words;
	int rest;
	int i;

	ret = MlnImpl_8105714C(&random);
	if (ret != 0)
		goto out;
	words = size / 4;
	for (i = 0; i < words; i++) {
		ret = MlnImpl_810571C4(random, sizeof(word), &word);
		if (ret != 0)
			goto out;
		data[0] = word;
		data[1] = word >> 8;
		data[2] = word >> 16;
		data[3] = word >> 24;
		data += 4;
	}
	rest = size - 4 * words;
	if (rest > 0) {
		ret = MlnImpl_810571C4(random, sizeof(word), &word);
		if (ret != 0)
			goto out;
		data[0] = word;
		if (rest != 1) {
			data[1] = word >> 8;
			if (rest != 2)
				data[2] = word >> 16;
		}
	}
out:
	MlnImpl_81057244(random);
	return ret;
}

/* random bytes from the secure generator, each from a word of it */
__attribute__((noinline))
unsigned int MlnImpl_810791F0(int size, unsigned char *data)
{
	void *random = NULL;
	unsigned char word[4];
	unsigned char *end;
	unsigned int ret;

	ret = MlnImpl_8105714C(&random);
	if (ret == 0 && size > 0) {
		end = data + size;
		do {
			ret = MlnImpl_810571C4(random, sizeof(word), word);
			if (ret != 0)
				break;
			*data++ = word[0] ^ word[2];
		} while (data != end);
	}
	MlnImpl_81057244(random);
	return ret;
}

/* a ServiceException read, freed */
inline __attribute__((always_inline)) unsigned int MlnImpl_81079270(MlnNemoFault *fault)
{
	if (fault != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, fault->parts[MLN_NEMO_FAULT_NAME]);
		MlnBufferDestroy(MLN_HEAP_GENERAL, fault->parts[MLN_NEMO_FAULT_REDIRECT_URL]);
		MlnBufferDestroy(MLN_HEAP_GENERAL, fault->parts[MLN_NEMO_FAULT_DETAILS]);
		MlnFree(MLN_HEAP_GENERAL, fault);
	}
	return 0;
}

/*
 * The ServiceExceptions of a fault's detail and of its FaultDetails header,
 * at most MLN_NEMO_FAULTS_READ_MAX in all.
 */
__attribute__((noinline))
unsigned int MlnImpl_810792B4(MlnXmlNode *detail, MlnXmlNode *faultDetails, MlnNemoFault ***items,
			      unsigned int *count)
{
	MlnXmlNodeList *first = NULL;
	MlnXmlNodeList *second = NULL;
	MlnNemoFault **faults;
	unsigned int firstCount;
	unsigned int secondCount;
	unsigned int total;
	unsigned int ret;
	unsigned int i;

	if (detail == NULL && faultDetails == NULL)
		return 0;
	firstCount = 0;
	if (detail != NULL) {
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, detail, "ServiceException",
				      MLN_NEMO_NS_EXCEPTIONS, &first);
		if (ret != 0)
			goto out;
		firstCount = first->count;
		if ((int)firstCount >= MLN_NEMO_FAULTS_READ_MAX)
			firstCount = MLN_NEMO_FAULTS_READ_MAX;
	}
	secondCount = 0;
	if (faultDetails != NULL) {
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, faultDetails, "ServiceException",
				      MLN_NEMO_NS_EXCEPTIONS, &second);
		if (ret != 0)
			goto out;
		secondCount = second->count;
		if ((int)secondCount >= MLN_NEMO_FAULTS_READ_MAX)
			secondCount = MLN_NEMO_FAULTS_READ_MAX;
	}
	total = firstCount + secondCount;
	if (total >= MLN_NEMO_FAULTS_READ_MAX)
		total = MLN_NEMO_FAULTS_READ_MAX;
	faults = MlnAllocate(MLN_HEAP_GENERAL, total * sizeof(*faults));
	if (faults == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(faults, 0, total * sizeof(*faults));
	for (i = 0; i < firstCount; i++) {
		ret = MlnImpl_81078F70(first->nodes[i], &faults[i]);
		if (ret != 0)
			goto fail;
	}
	for (i = 0; i < total - firstCount; i++) {
		ret = MlnImpl_81078F70(second->nodes[i], &faults[firstCount + i]);
		if (ret != 0)
			goto fail;
	}
	*items = faults;
	ret = 0;
	*count = total;
	goto out;
fail:
	for (i = 0; i < total; i++)
		MlnImpl_81079270(faults[i]);
	MlnFree(MLN_HEAP_GENERAL, faults);
out:
	MlnXmlListFree(MLN_HEAP_GENERAL, first);
	MlnXmlListFree(MLN_HEAP_GENERAL, second);
	return ret;
}

/* a fault read, freed with its exceptions */
__attribute__((noinline))
unsigned int MlnImpl_810794DC(MlnNemoFaults *faults)
{
	unsigned int i;

	if (faults != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, faults->code);
		MlnBufferDestroy(MLN_HEAP_GENERAL, faults->string);
		MlnBufferDestroy(MLN_HEAP_GENERAL, faults->actor);
		if (faults->items != NULL) {
			for (i = 0; i < faults->count; i++)
				MlnImpl_81079270(faults->items[i]);
			MlnFree(MLN_HEAP_GENERAL, faults->items);
		}
		MlnFree(MLN_HEAP_GENERAL, faults);
	}
	return 0;
}
