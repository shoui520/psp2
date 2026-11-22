/*
 * A registration service's context and requests: an element wrapped in a new element, the
 * context of a session, the stored credentials and the registration request taken out of
 * the store (0x81064CD8-). The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_REGISTRATION_ERROR_OPERATION	0x23100001	/* the transfer is not a move */
#define MLN_REGISTRATION_ERROR_LICENSE	0x23100005	/* the license is not XML */
#define MLN_REGISTRATION_ERROR_REQUEST	0x23100006	/* the request is not XML */

/* what a registration session keeps between its steps (12 bytes) */
typedef struct MlnRegistrationContext {
	MlnBuffer *buffer;
	MlnXmlNode *request;
	MlnXmlNode *response;
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
		MlnXmlNodeFree(MLN_HEAP_GENERAL, registration->response);
		MlnXmlNodeFree(MLN_HEAP_GENERAL, registration->request);
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
