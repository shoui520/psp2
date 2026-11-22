/*
 * A registration service's context and requests: an element wrapped in a new element, the
 * context of a session, the stored credentials and the registration request taken out of
 * the store (0x81064CD8-). The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_REGISTRATION_ERROR_OPERATION	0x23100001	/* the transfer is not a move */
#define MLN_REGISTRATION_ERROR_EXTENSION	0x23100002	/* not one extension in the license */
#define MLN_REGISTRATION_ERROR_CATEGORY	0x23100004	/* the category is not a number */
#define MLN_REGISTRATION_ERROR_STATUS	0x23100003	/* the answer has no status block */
#define MLN_REGISTRATION_ERROR_LICENSE	0x23100005	/* the license is not XML */
#define MLN_REGISTRATION_ERROR_REQUEST	0x23100006	/* the request is not XML */

/* a big-endian word of a status block */
#define MLN_REGISTRATION_WORD(p, i)	((p)[4 * (i)] << 24 | (p)[4 * (i) + 1] << 16 |	\
					 (p)[4 * (i) + 2] << 8 | (p)[4 * (i) + 3])

/* what a registration session keeps between its steps (12 bytes) */
typedef struct MlnRegistrationContext {
	MlnBuffer *buffer;	/* the license transferred */
	MlnXmlNode *control;	/* its extension's Control */
	MlnXmlNode *signature;	/* and the extension's Signature */
} MlnRegistrationContext;

/* an element wrapped in a new element of that name, without a namespace prefix */
int MlnImpl_81064CD8(MlnXmlNode *node, const char *name, MlnXmlNode **out)
{
	MlnXmlNode *element = NULL;
	MlnXmlBuildContent content;
	int ret;

	if (node == NULL || name == NULL || out == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnXmlNodeGetElement(MLN_HEAP_GENERAL, node, &element);
	if (ret == 0) {
		content.kind = MLN_XML_BUILD_NODE;
		content.value.node = element;
		ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, name, "", NULL, 0, NULL, 1, &content, 0,
					 out);
	}
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, element);
	return ret;
}

/* a new, empty context */
unsigned int MlnImpl_81064D8C(void **context)
{
	MlnRegistrationContext *new;

	if (context == NULL)
		return 3;
	new = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*new));
	if (new == NULL)
		return 2;
	MlnMemset(new, 0, sizeof(*new));
	*context = new;
	return 0;
}

/* free a context */
unsigned int MlnImpl_81064DD4(void *context)
{
	MlnRegistrationContext *registration = context;

	if (registration != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, registration->buffer);
		MlnXmlNodeFree(MLN_HEAP_GENERAL, registration->signature);
		MlnXmlNodeFree(MLN_HEAP_GENERAL, registration->control);
		MlnFree(MLN_HEAP_GENERAL, registration);
	}
	return 0;
}

/* the stored credentials (MlnImpl_81064340) */
unsigned int MlnImpl_81064E18(MlnBufferGroups **info)
{
	MlnBufferGroups *groups = NULL;
	unsigned int ret;

	if (info == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_81064340(&groups);
	if (ret == 0) {
		*info = groups;
		groups = NULL;
	}
out:
	MlnImpl_81042A88(groups);
	return ret;
}

/* the stored registration request (MlnImpl_81043AE4 step 1) */
unsigned int MlnImpl_81064E68(MlnBuffer **request)
{
	MlnBuffer *buffer = NULL;
	unsigned int ret;

	if (request == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_81043AE4(1, &buffer);
	if (ret == 0) {
		*request = buffer;
		buffer = NULL;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}

/* the license, the operation and the request of a transfer under one setup payload */
static inline int MlnRegistrationSetupPayload(MlnXmlNode *license, MlnXmlNode *operation,
					      MlnXmlNode *request, MlnXmlBuildContent *contents,
					      MlnXmlNode **out)
{
	MlnXmlNode *licenseElement = NULL;
	MlnXmlNode *operationElement = NULL;
	MlnXmlNode *requestElement = NULL;
	int ret;

	if (license == NULL || operation == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnXmlNodeGetElement(MLN_HEAP_GENERAL, license, &licenseElement);
	if (ret != 0)
		goto out;
	contents[0].kind = MLN_XML_BUILD_NODE;
	contents[0].value.node = licenseElement;
	ret = MlnXmlNodeGetElement(MLN_HEAP_GENERAL, operation, &operationElement);
	if (ret != 0)
		goto out;
	contents[1].kind = MLN_XML_BUILD_NODE;
	contents[1].value.node = operationElement;
	ret = MlnXmlNodeGetElement(MLN_HEAP_GENERAL, request, &requestElement);
	if (ret != 0)
		goto out;
	contents[2].kind = MLN_XML_BUILD_NODE;
	contents[2].value.node = requestElement;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "LicenseTransferSetupPayload", "", NULL, 0, NULL,
				 3, contents, 0, out);
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, licenseElement);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, operationElement);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, requestElement);
	return ret;
}

/*
 * A license transfer (move) request: the license, its LicensePart and License wrappers,
 * the move operation and the request, as a LicenseTransferRequestPayload.
 */
unsigned int MlnImpl_81064EC0(void *context, MlnBuffer *request, const MlnBuffer *data,
			      MlnXmlNode **document)
{
	MlnXmlNode *licenseDocument = NULL;
	MlnXmlNode *requestDocument = NULL;
	MlnXmlNode *part = NULL;
	MlnXmlNode *license = NULL;
	MlnXmlNode *operation = NULL;
	MlnXmlNode *setup = NULL;
	MlnXmlNode *payload = NULL;
	MlnXmlBuildContent contents[3];
	unsigned int ret;

	if (context == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	if (data == NULL || data->size == 0 || data->data == NULL || request == NULL ||
	    request->size == 0 || request->data == NULL || document == NULL) {
		ret = 3;
		goto out;
	}
	if (MlnXmlParse(MLN_HEAP_GENERAL, data->size, data->data, &licenseDocument) != 0) {
		ret = MLN_REGISTRATION_ERROR_LICENSE;
		goto out;
	}
	if (MlnXmlParse(MLN_HEAP_GENERAL, request->size, request->data, &requestDocument) != 0) {
		ret = MLN_REGISTRATION_ERROR_REQUEST;
		goto out;
	}
	ret = MlnImpl_81064CD8(licenseDocument, "LicensePart", &part);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81064CD8(part, "License", &license);
	if (ret != 0)
		goto out;
	contents[0].kind = MLN_XML_BUILD_STRING;
	contents[0].value.string = "urn:marlin:core:1-0:service:license-transfer:move";
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Operation", "", NULL, 0, NULL, 1, contents, 0,
				 &operation);
	if (ret != 0)
		goto out;
	ret = MlnRegistrationSetupPayload(license, operation, requestDocument, contents, &setup);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81064CD8(setup, "LicenseTransferRequestPayload", &payload);
	if (ret != 0)
		goto out;
	*document = payload;
	payload = NULL;
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, licenseDocument);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, requestDocument);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, part);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, license);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, operation);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, setup);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, payload);
	return ret;
}

/*
 * The bundles of a license transfer setup payload, after checking its operation is a move:
 * the payload's Bundle and the one in its License/LicensePart.
 */
unsigned int MlnImpl_810651F4(MlnXmlNode *document, MlnBuffer **request, MlnBuffer **token)
{
	MlnXmlNode *setup = NULL;
	MlnXmlNode *operation = NULL;
	MlnXmlNode *licenseBundle = NULL;
	MlnXmlNode *bundle = NULL;
	MlnBuffer *operationText = NULL;
	MlnBuffer *bundleText = NULL;
	MlnBuffer *licenseText = NULL;
	MlnBuffer *op = NULL;
	MlnBuffer *first = NULL;
	MlnBuffer *second = NULL;
	unsigned int ret;

	if (document == NULL || request == NULL || token == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, document, "LicenseTransferSetupPayload", NULL, &setup);
	if (ret == 0)
		ret = MlnXmlFindNode(MLN_HEAP_GENERAL, setup, "Operation", NULL, &operation);
	if (ret == 0)
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, operation, &operationText);
	if (ret == 0)
		ret = MlnXmlFindNode(MLN_HEAP_GENERAL, setup, "Bundle", NULL, &bundle);
	if (ret == 0)
		ret = MlnXmlNodeToText(MLN_HEAP_GENERAL, bundle, &bundleText);
	if (ret == 0)
		ret = MlnXmlFindNode(MLN_HEAP_GENERAL, setup, "License/LicensePart/Bundle", NULL,
				     &licenseBundle);
	if (ret == 0)
		ret = MlnXmlNodeToText(MLN_HEAP_GENERAL, licenseBundle, &licenseText);
	if (ret == 0) {
		second = licenseText;
		first = bundleText;
		op = operationText;
		licenseText = NULL;
		bundleText = NULL;
		operationText = NULL;
	}
	MlnXmlNodeFree(MLN_HEAP_GENERAL, setup);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, operation);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, bundle);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, licenseBundle);
	MlnBufferDestroy(MLN_HEAP_GENERAL, operationText);
	MlnBufferDestroy(MLN_HEAP_GENERAL, bundleText);
	MlnBufferDestroy(MLN_HEAP_GENERAL, licenseText);
	if (ret != 0)
		goto out;

	if (MlnBytesCompare(MLN_HEAP_GENERAL, op->size, op->data,
			    MlnStrlen("urn:marlin:core:1-0:service:license-transfer:move"),
			    "urn:marlin:core:1-0:service:license-transfer:move") != 0) {
		ret = MLN_REGISTRATION_ERROR_OPERATION;
	} else {
		*token = second;
		second = NULL;
		*request = first;
		first = NULL;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, second);
	MlnBufferDestroy(MLN_HEAP_GENERAL, first);
	MlnBufferDestroy(MLN_HEAP_GENERAL, op);
	return ret;
}

/* an Octopus Bundle of the agent and its context */
static inline int MlnRegistrationBundle(MlnXmlNode *agent, MlnXmlNode *context,
					MlnXmlBuildContent *contents, MlnXmlNode **out)
{
	MlnXmlNode *agentElement = NULL;
	MlnXmlNode *contextElement = NULL;
	int ret;

	ret = MlnXmlNodeGetElement(MLN_HEAP_GENERAL, agent, &agentElement);
	if (ret != 0)
		goto out;
	contents[0].kind = MLN_XML_BUILD_NODE;
	contents[0].value.node = agentElement;
	ret = MlnXmlNodeGetElement(MLN_HEAP_GENERAL, context, &contextElement);
	if (ret != 0)
		goto out;
	contents[1].kind = MLN_XML_BUILD_NODE;
	contents[1].value.node = contextElement;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "Bundle", "oct",
				 "http://www.octopus-drm.com/profiles/base/1.0", 0, NULL, 2, contents,
				 0, out);
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, agentElement);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, contextElement);
	return ret;
}

/* an AgentCarrier of the bundle and the parameter, with the agent's ids as attributes */
static inline int MlnRegistrationAgentCarrier(MlnXmlNode *bundle, MlnXmlNode *parameter,
					      const MlnBuffer *agentId,
					      const MlnBuffer *contextId,
					      const MlnBuffer *controlId,
					      MlnXmlBuildContent *contents, MlnXmlNode **out)
{
	MlnXmlNode *bundleElement = NULL;
	MlnXmlNode *parameterElement = NULL;
	char *name = NULL;
	char *context = NULL;
	char *control = NULL;
	int ret;

	if (bundle == NULL || agentId->size == 0 || agentId->data == NULL ||
	    contextId->size == 0 || contextId->data == NULL || controlId->size == 0 ||
	    controlId->data == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnXmlNodeGetElement(MLN_HEAP_GENERAL, bundle, &bundleElement);
	if (ret != 0)
		goto out;
	contents[0].kind = MLN_XML_BUILD_NODE;
	contents[0].value.node = bundleElement;
	ret = MlnXmlNodeGetElement(MLN_HEAP_GENERAL, parameter, &parameterElement);
	if (ret != 0)
		goto out;
	contents[1].kind = MLN_XML_BUILD_NODE;
	contents[1].value.node = parameterElement;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "AgentCarrier", "mc",
				 "urn:marlin:core:1-3:schemas", 0, NULL, 2, contents, 0, out);
	if (ret != 0)
		goto out;
	ret = MlnBufferToString(MLN_HEAP_GENERAL, agentId, &name);
	if (ret != 0)
		goto out;
	ret = MlnBufferToString(MLN_HEAP_GENERAL, contextId, &context);
	if (ret != 0)
		goto out;
	ret = MlnBufferToString(MLN_HEAP_GENERAL, controlId, &control);
	if (ret != 0)
		goto out;
	ret = MlnXmlNodeAddAttribute(MLN_HEAP_GENERAL, *out, "contextId", context);
	if (ret != 0)
		goto out;
	ret = MlnXmlNodeAddAttribute(MLN_HEAP_GENERAL, *out, "controlId", control);
	if (ret != 0)
		goto out;
	ret = MlnXmlNodeAddAttribute(MLN_HEAP_GENERAL, *out, "name", name);
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, bundleElement);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, parameterElement);
	MlnFree(MLN_HEAP_GENERAL, name);
	MlnFree(MLN_HEAP_GENERAL, context);
	MlnFree(MLN_HEAP_GENERAL, control);
	return ret;
}

/*
 * A license transfer response running an agent: the agent and its context in a Bundle,
 * carried with the parameter in an AgentCarrier named by the agent's ids.
 */
unsigned int MlnImpl_81065430(const MlnBuffer *agentId, const MlnBuffer *contextId,
			      const MlnBuffer *controlId, MlnXmlNode *parameter, MlnXmlNode *agent,
			      MlnXmlNode *context, MlnXmlNode **document)
{
	MlnXmlNode *bundle = NULL;
	MlnXmlNode *carrier = NULL;
	MlnXmlNode *run = NULL;
	MlnXmlNode *payload = NULL;
	MlnXmlBuildContent contents[2];
	unsigned int ret;

	if (agentId == NULL || agentId->size == 0 || agentId->data == NULL || contextId == NULL ||
	    contextId->size == 0 || contextId->data == NULL || controlId == NULL ||
	    controlId->size == 0 || controlId->data == NULL || parameter == NULL ||
	    agent == NULL || context == NULL || document == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnRegistrationBundle(agent, context, contents, &bundle);
	if (ret != 0)
		goto out;
	ret = MlnRegistrationAgentCarrier(bundle, parameter, agentId, contextId, controlId,
					  contents, &carrier);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81064CD8(carrier, "LicenseTransferRunAgentPayload", &run);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81064CD8(run, "LicenseTransferResponsePayload", &payload);
	if (ret != 0)
		goto out;
	*document = payload;
	payload = NULL;
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, bundle);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, carrier);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, run);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, payload);
	return ret;
}

/* the AgentCarrier of a license transfer run-agent payload */
unsigned int MlnImpl_81065848(MlnXmlNode *document, MlnXmlNode **payload)
{
	MlnXmlNode *run = NULL;
	MlnXmlNode *carrier = NULL;
	MlnXmlNode *result = NULL;
	unsigned int ret;

	if (document == NULL || payload == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, document, "LicenseTransferRunAgentPayload", "", &run);
	if (ret == 0)
		ret = MlnXmlFindNode(MLN_HEAP_GENERAL, run, "AgentCarrier",
				     "urn:marlin:core:1-3:schemas", &carrier);
	if (ret == 0) {
		result = carrier;
		carrier = NULL;
	}
	MlnXmlNodeFree(MLN_HEAP_GENERAL, run);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, carrier);
	if (ret == 0) {
		*payload = result;
		result = NULL;
	}
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, result);
	return ret;
}

/* the ExtendedStatusBlock of a teardown: the answer's four status words as decimal text */
static inline unsigned int MlnRegistrationStatusBlock(const MlnBuffer *answer, MlnXmlNode **out,
						      int *categorized)
{
	MlnXmlNode *block = NULL;
	const unsigned char *words;
	char globalFlags[21];
	char category[21];
	char subcategory[21];
	char localFlags[21];
	unsigned int ret;
	int length;

	MlnMemset(globalFlags, 0, sizeof(globalFlags));
	MlnMemset(category, 0, sizeof(category));
	MlnMemset(subcategory, 0, sizeof(subcategory));
	MlnMemset(localFlags, 0, sizeof(localFlags));
	if (answer->size <= 0xF) {
		ret = MLN_REGISTRATION_ERROR_STATUS;
		goto out;
	}
	words = answer->data;
	length = MlnImpl_81052CA8(MLN_REGISTRATION_WORD(words, 0), 1);
	if (length < 0) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_810529E8(MLN_REGISTRATION_WORD(words, 0), 1, length, globalFlags);
	if (ret != 0)
		goto out;
	length = MlnImpl_81052CA8(MLN_REGISTRATION_WORD(words, 1), 1);
	if (length < 0) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_810529E8(MLN_REGISTRATION_WORD(words, 1), 1, length, category);
	if (ret != 0)
		goto out;
	*categorized = MLN_REGISTRATION_WORD(words, 1) != 0;
	length = MlnImpl_81052CA8(MLN_REGISTRATION_WORD(words, 2), 1);
	if (length < 0) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_810529E8(MLN_REGISTRATION_WORD(words, 2), 1, length, subcategory);
	if (ret != 0)
		goto out;
	length = MlnImpl_81052CA8(MLN_REGISTRATION_WORD(words, 3), 1);
	if (length < 0) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_810529E8(MLN_REGISTRATION_WORD(words, 3), 1, length, localFlags);
	if (ret != 0)
		goto out;

	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "ExtendedStatusBlock", "pk",
				 MLN_PLANKTON_NAMESPACE, 0, NULL, 0, NULL, 0, &block);
	if (ret == 0)
		ret = MlnXmlNodeAddAttribute(MLN_HEAP_GENERAL, block, "globalFlags", globalFlags);
	if (ret == 0)
		ret = MlnXmlNodeAddAttribute(MLN_HEAP_GENERAL, block, "category", category);
	if (ret == 0)
		ret = MlnXmlNodeAddAttribute(MLN_HEAP_GENERAL, block, "subcategory", subcategory);
	if (ret == 0)
		ret = MlnXmlNodeAddAttribute(MLN_HEAP_GENERAL, block, "localFlags", localFlags);
	if (ret == 0) {
		*out = block;
		block = NULL;
	}
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, block);
	return ret;
}

/*
 * A content key of the license's bundle, its secret key named by the device's key pair and
 * its cipher value replaced by the key encrypted for the new device.
 */
static inline unsigned int MlnRegistrationContentKey(MlnXmlNode *bundle,
						     const MlnBuffer *contentKeyId,
						     const MlnBuffer *keyId,
						     const MlnBuffer *encryptedKey,
						     const MlnBuffer *pair, MlnXmlNode **out)
{
	MlnXmlNode *contentKey = NULL;
	MlnXmlNode *keyName = NULL;
	MlnXmlNode *cipherValue = NULL;
	MlnXmlNodeList *cipherValues = NULL;
	char *contentKeyPath = NULL;
	char *keyNamePath = NULL;
	char *cipherValuePath = NULL;
	char *encryptedKeyPath = NULL;
	char *name = NULL;
	char *value = NULL;
	MlnBuffer *encoded = NULL;
	unsigned int ret;

	ret = MlnImpl_81082EA4("//ContentKey[@uid='", contentKeyId, "']", &contentKeyPath);
	if (ret == 0)
		ret = MlnXmlFindNode(MLN_HEAP_GENERAL, bundle, contentKeyPath, NULL, &contentKey);
	if (ret == 0)
		ret = MlnImpl_81082EA4("SecretKey[@uid='", keyId, "']//KeyName", &keyNamePath);
	if (ret == 0)
		ret = MlnXmlFindNode(MLN_HEAP_GENERAL, contentKey, keyNamePath, NULL, &keyName);
	if (ret == 0)
		ret = MlnBufferToString(MLN_HEAP_GENERAL, pair, &name);
	if (ret == 0)
		ret = MlnXmlNodeSetText(MLN_HEAP_GENERAL, keyName, name);
	if (ret == 0)
		ret = MlnImpl_81082EA4("SecretKey[@uid='", keyId, "']//CipherValue",
				       &cipherValuePath);
	if (ret == 0)
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, contentKey, cipherValuePath, NULL,
				      &cipherValues);
	if (ret != 0)
		goto out;

	/* a key wrapped in an EncryptedKey has a cipher value of its own as well */
	if (cipherValues->count <= 0) {
		ret = 5;
		goto out;
	}
	if (cipherValues->count == 1) {
		cipherValue = MlnXmlNodeRetain(MLN_HEAP_GENERAL, cipherValues->nodes[0]);
	} else {
		ret = MlnImpl_81082EA4("SecretKey[@uid='", keyId, "']//EncryptedKey//CipherValue",
				       &encryptedKeyPath);
		if (ret == 0)
			ret = MlnXmlFindNode(MLN_HEAP_GENERAL, contentKey, encryptedKeyPath, NULL,
					     &cipherValue);
		if (ret != 0)
			goto out;
	}
	ret = MlnImpl_81026814(MLN_HEAP_GENERAL, encryptedKey, 0, 0, &encoded);
	if (ret == 0)
		ret = MlnBufferToString(MLN_HEAP_GENERAL, encoded, &value);
	if (ret == 0)
		ret = MlnXmlNodeSetText(MLN_HEAP_GENERAL, cipherValue, value);
	if (ret == 0) {
		*out = contentKey;
		contentKey = NULL;
	}
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, contentKey);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, keyName);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, cipherValue);
	MlnXmlListFree(MLN_HEAP_GENERAL, cipherValues);
	MlnFree(MLN_HEAP_GENERAL, contentKeyPath);
	MlnFree(MLN_HEAP_GENERAL, keyNamePath);
	MlnFree(MLN_HEAP_GENERAL, cipherValuePath);
	MlnFree(MLN_HEAP_GENERAL, encryptedKeyPath);
	MlnFree(MLN_HEAP_GENERAL, name);
	MlnFree(MLN_HEAP_GENERAL, value);
	MlnBufferDestroy(MLN_HEAP_GENERAL, encoded);
	return ret;
}

/* the ContentKeyList of a teardown: every content key of the license for the new device */
static inline unsigned int MlnRegistrationContentKeyList(const MlnRegistrationContext *registration,
							 int count, MlnBuffer **encryptedKeys,
							 MlnBuffer **keyIds,
							 MlnBuffer **contentKeyIds,
							 const MlnBuffer *pair, MlnXmlNode **out)
{
	const MlnBuffer *license = registration->buffer;
	MlnXmlNode *bundle = NULL;
	MlnXmlNode *contentKey = NULL;
	MlnXmlNodeList *contentKeys = NULL;
	MlnXmlBuildContent *contents;
	unsigned int ret;
	int i;

	if (license == NULL || count == 0 || encryptedKeys == NULL || keyIds == NULL ||
	    contentKeyIds == NULL) {
		ret = 3;
		goto out;
	}
	for (i = 0; i < count; i++) {
		if (encryptedKeys[i] == NULL || encryptedKeys[i]->size == 0 ||
		    encryptedKeys[i]->data == NULL || keyIds[i] == NULL || keyIds[i]->size == 0 ||
		    keyIds[i]->data == NULL || contentKeyIds[i] == NULL ||
		    contentKeyIds[i]->size == 0 || contentKeyIds[i]->data == NULL) {
			ret = 3;
			goto out;
		}
	}
	if (pair == NULL || pair->size == 0 || pair->data == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnXmlParse(MLN_HEAP_GENERAL, license->size, license->data, &bundle);
	if (ret == 0)
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, bundle, "/Bundle/ContentKey",
				      MLN_OCTOPUS_NAMESPACE, &contentKeys);
	if (ret != 0)
		goto out;
	if (contentKeys == NULL || contentKeys->count != count) {
		ret = 5;
		goto out;
	}
	contents = MlnAllocate(MLN_HEAP_GENERAL, count * sizeof(*contents));
	if (contents == NULL) {
		ret = 2;
		goto out;
	}
	for (i = 0; i < count; i++)
		contents[i].value.node = NULL;

	for (i = 0; i < count; i++) {
		ret = MlnRegistrationContentKey(bundle, contentKeyIds[i], keyIds[i],
						encryptedKeys[i], pair, &contentKey);
		if (ret != 0)
			break;
		contents[i].kind = MLN_XML_BUILD_NODE;
		contents[i].value.node = contentKey;
		contentKey = NULL;
	}
	if (ret == 0)
		ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "ContentKeyList", "", NULL, 0, NULL,
					 count, contents, 0, out);
	for (i = 0; i < count; i++)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, contents[i].value.node);
	MlnFree(MLN_HEAP_GENERAL, contents);
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, bundle);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, contentKey);
	MlnXmlListFree(MLN_HEAP_GENERAL, contentKeys);
	return ret;
}

/*
 * A license transfer teardown payload: the status the answer gives and, unless it reports a
 * failure category, the license's content keys for the new device.
 */
unsigned int MlnImpl_81065920(void *context, int count, MlnBuffer **encryptedKeys,
			      MlnBuffer **keyIds, MlnBuffer **contentKeyIds, MlnBuffer *pair,
			      MlnBuffer *answer, MlnXmlNode **document)
{
	MlnRegistrationContext *registration = context;
	MlnXmlNode *status = NULL;
	MlnXmlNode *list = NULL;
	MlnXmlNode *teardown = NULL;
	MlnXmlBuildContent contents[2];
	int categorized;
	unsigned int ret;
	int n;

	if (registration == NULL) {
		ret = 4;
		goto out;
	}
	if (answer == NULL || answer->size == 0 || answer->data == NULL || document == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnRegistrationStatusBlock(answer, &status, &categorized);
	if (ret != 0)
		goto out;
	if (!categorized) {
		ret = MlnRegistrationContentKeyList(registration, count, encryptedKeys, keyIds,
						    contentKeyIds, pair, &list);
		if (ret != 0)
			goto out;
	}
	if (status == NULL) {
		ret = 3;
		goto out;
	}
	contents[0].kind = MLN_XML_BUILD_NODE;
	contents[0].value.node = status;
	n = 1;
	if (!categorized) {
		if (list == NULL) {
			ret = 3;
			goto out;
		}
		contents[1].kind = MLN_XML_BUILD_NODE;
		contents[1].value.node = list;
		n = 2;
	}
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "LicenseTransferTeardownPayload", "", NULL, 0,
				 NULL, n, contents, 0, &teardown);
	if (ret == 0) {
		*document = teardown;
		teardown = NULL;
	}
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, status);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, list);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, teardown);
	return ret;
}

/*
 * Keep a license in the context: the license, and the Control and the Signature of the
 * bundle its one extension carries.
 */
unsigned int MlnImpl_8106632C(void *context, const MlnBuffer *license)
{
	MlnRegistrationContext *registration = context;
	MlnXmlNode *document = NULL;
	MlnXmlNodeList *extensions = NULL;
	MlnXmlNode *control = NULL;
	MlnXmlNode *signature = NULL;
	MlnXmlNode *bundle = NULL;
	MlnBuffer *copy = NULL;
	unsigned int ret;

	if (registration == NULL) {
		ret = 4;
		goto out;
	}
	if (license == NULL || license->size == 0 || license->data == NULL) {
		ret = 3;
		goto out;
	}
	copy = MlnBufferCreate(MLN_HEAP_GENERAL, license->size, license->data);
	if (copy == NULL) {
		ret = 2;
		goto out;
	}
	ret = MlnXmlParse(MLN_HEAP_GENERAL, license->size, license->data, &document);
	if (ret != 0)
		goto out;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, document, "/Bundle/Extension",
			      MLN_OCTOPUS_NAMESPACE, &extensions);
	if (ret != 0)
		goto out;
	if (extensions->count != 1) {
		ret = MLN_REGISTRATION_ERROR_EXTENSION;
		goto out;
	}
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, extensions->nodes[0], "Bundle",
			     MLN_OCTOPUS_NAMESPACE, &bundle);
	if (ret == 0)
		ret = MlnXmlFindNode(MLN_HEAP_GENERAL, bundle, "Control", MLN_OCTOPUS_NAMESPACE,
				     &control);
	if (ret == 0)
		ret = MlnXmlFindNode(MLN_HEAP_GENERAL, bundle, "Signature", MLN_XMLDSIG_NAMESPACE,
				     &signature);
	if (ret != 0)
		goto out;

	if (registration->control != NULL)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, registration->control);
	registration->control = control;
	control = NULL;
	if (registration->signature != NULL)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, registration->signature);
	registration->buffer = copy;
	copy = NULL;
	registration->signature = signature;
	signature = NULL;
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, document);
	MlnXmlListFree(MLN_HEAP_GENERAL, extensions);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, control);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, signature);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, bundle);
	MlnBufferDestroy(MLN_HEAP_GENERAL, copy);
	return ret;
}

/* the Control and the Signature the context keeps */
unsigned int MlnImpl_81066548(void *context, MlnXmlNode **control, MlnXmlNode **signature)
{
	MlnRegistrationContext *registration = context;

	if (registration == NULL)
		return 4;
	if (control == NULL || signature == NULL)
		return 3;
	*control = registration->control;
	*signature = registration->signature;
	return 0;
}

/* the category of the ExtendedStatusBlock a teardown's answer carries */
unsigned int MlnImpl_81066588(void *context, MlnXmlNode *document, int *category)
{
	MlnXmlNode *block = NULL;
	MlnBuffer *text = NULL;
	char *string = NULL;
	unsigned int ret;
	int value;

	if (context == NULL) {
		ret = 4;
		goto out;
	}
	if (document == NULL || category == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, document, "ExtendedStatusBlock",
			     MLN_PLANKTON_NAMESPACE, &block);
	if (ret == 0)
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, block, MLN_PLANKTON_NAMESPACE, "category",
					 &text);
	if (ret == 0)
		ret = MlnBufferToString(MLN_HEAP_GENERAL, text, &string);
	if (ret == 0) {
		value = MlnImpl_81052BD0(string);
		if (value < 0)
			ret = MLN_REGISTRATION_ERROR_CATEGORY;
		else
			*category = value;
	}
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, block);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnFree(MLN_HEAP_GENERAL, string);
	return ret;
}

/* a ValueBlock of type Date */
static inline unsigned int MlnRegistrationDateBlock(int date, MlnXmlNode **out)
{
	MlnXmlBuildAttribute attribute;
	MlnXmlBuildContent content;
	MlnXmlNode *block = NULL;
	unsigned int ret;

	attribute.name = "type";
	attribute.value = "Date";
	content.kind = MLN_XML_BUILD_INT;
	content.value.integer = date;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "ValueBlock", "pk", MLN_PLANKTON_NAMESPACE, 1,
				 &attribute, 1, &content, 0, &block);
	if (ret != 0) {
		if (block != NULL)
			MlnXmlNodeFree(MLN_HEAP_GENERAL, block);
		return ret;
	}
	*out = block;
	return 0;
}

/* a ParameterBlock of that name holding a value block */
static inline unsigned int MlnRegistrationParameterBlock(MlnXmlNode *value, const char *name,
							 MlnXmlNode **out)
{
	MlnXmlBuildContent content;
	unsigned int ret;

	MlnMemset(&content, 0, sizeof(content));
	ret = MlnXmlNodeGetElement(MLN_HEAP_GENERAL, value, &content.value.node);
	if (ret == 0) {
		content.kind = MLN_XML_BUILD_NODE;
		ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "ParameterBlock", "pk",
					 MLN_PLANKTON_NAMESPACE, 0, NULL, 1, &content, 0, out);
		if (ret == 0)
			ret = MlnXmlNodeAddAttribute(MLN_HEAP_GENERAL, *out, "name", name);
	}
	if (content.value.node != NULL)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, content.value.node);
	return ret;
}

/* a parameter of a run-agent request: a ParameterBlock of that name holding a date */
unsigned int MlnImpl_8106668C(const MlnBuffer *name, int date, MlnXmlNode **node)
{
	MlnXmlNode *value = NULL;
	MlnXmlNode *block = NULL;
	char *string = NULL;
	unsigned int ret;

	if (name == NULL || name->size == 0 || name->data == NULL || node == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnRegistrationDateBlock(date, &value);
	if (ret != 0)
		goto out;
	ret = MlnBufferToString(MLN_HEAP_GENERAL, name, &string);
	if (ret != 0)
		goto out;
	if (string == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnRegistrationParameterBlock(value, string, &block);
	if (ret == 0) {
		*node = block;
		block = NULL;
	}
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, value);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, block);
	MlnFree(MLN_HEAP_GENERAL, string);
	return ret;
}

/*
 * A license transfer request payload carrying an agent's result: the result wrapped in a
 * LicenseTransferAgentResultPayload, wrapped in turn in the request.
 */
unsigned int MlnImpl_81066888(void *context, MlnXmlNode *node, MlnXmlNode **document)
{
	MlnXmlNode *result = NULL;
	MlnXmlNode *request = NULL;
	unsigned int ret;

	if (context == NULL) {
		ret = 4;
		goto out;
	}
	if (node == NULL || document == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_81064CD8(node, "LicenseTransferAgentResultPayload", &result);
	if (ret == 0)
		ret = MlnImpl_81064CD8(result, "LicenseTransferRequestPayload", &request);
	if (ret == 0) {
		*document = request;
		request = NULL;
	}
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, result);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, request);
	return ret;
}

/* the result code and the decoded result of an agent's result payload */
unsigned int MlnImpl_81066A28(MlnXmlNode *document, int *value, MlnBuffer **payload)
{
	MlnXmlNode *result;
	MlnXmlNode *block;
	MlnBuffer *code;
	MlnBuffer *text;
	char *string;
	unsigned int ret;

	if (document == NULL || value == NULL || payload == NULL)
		return 3;
	result = NULL;
	block = NULL;
	code = NULL;
	text = NULL;
	string = NULL;
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, document, "LicenseTransferAgentResultPayload", "",
			     &result);
	if (ret == 0)
		ret = MlnXmlFindNode(MLN_HEAP_GENERAL, result, "AgentResultBlock",
				     "urn:marlin:core:1-3:schemas", &block);
	if (ret == 0)
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, block, NULL, "resultCode", &code);
	if (ret == 0)
		ret = MlnBufferToString(MLN_HEAP_GENERAL, code, &string);
	if (ret == 0) {
		*value = MlnImpl_81052BD0(string);
		ret = MlnXmlGetText(MLN_HEAP_GENERAL, block, &text);
		if (ret == 0 && text != NULL)
			ret = MlnImpl_81026A9C(MLN_HEAP_GENERAL, text, payload);
	}
	MlnXmlNodeFree(MLN_HEAP_GENERAL, result);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, block);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnBufferDestroy(MLN_HEAP_GENERAL, code);
	MlnFree(MLN_HEAP_GENERAL, string);
	return ret;
}

/*
 * What a license transfer request asks for: the Id of the node the license moves to and the
 * operation (always "Move"), and the node itself.
 */
unsigned int MlnImpl_81066B84(const MlnBuffer *request, MlnBuffer **transfer, MlnXmlNode **node)
{
	MlnXmlNode *document = NULL;
	MlnXmlNode *target = NULL;
	MlnBuffer *id = NULL;
	unsigned int ret;

	if (request == NULL || request->size == 0 || request->data == NULL || transfer == NULL ||
	    node == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnXmlParse(MLN_HEAP_GENERAL, request->size, request->data, &document);
	if (ret == 0)
		ret = MlnXmlFindNode(MLN_HEAP_GENERAL, document, "Bundle/Node",
				     MLN_OCTOPUS_NAMESPACE, &target);
	if (ret == 0)
		ret = MlnXmlGetAttribute(MLN_HEAP_GENERAL, target, MLN_OCTOPUS_NAMESPACE, "Id", &id);
	if (ret != 0)
		goto out;
	transfer[0] = MlnBufferCreate(MLN_HEAP_GENERAL, id->size, id->data);
	if (transfer[0] == NULL ||
	    (transfer[1] = MlnBufferCreate(MLN_HEAP_GENERAL, 4, "Move")) == NULL) {
		ret = 2;
		goto out;
	}
	*node = target;
	target = NULL;
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, document);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, target);
	MlnBufferDestroy(MLN_HEAP_GENERAL, id);
	return ret;
}
