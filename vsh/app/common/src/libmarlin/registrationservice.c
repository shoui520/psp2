/*
 * The registration service's requests and responses over a NEMO exchange: node acquisition,
 * link acquisition, the confirmation of DRM objects, deregistration, and the steps of a
 * license transfer (0x81066CE0-). The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_REGISTRATION_SERVICE_PREFIX	"kfrs"
#define MLN_REGISTRATION_SERVICE_URI						\
	"urn:marlin:broadband:1-1:nemo:services:schemas:registration-service"
#define MLN_MARLIN_CORE_URI		"urn:marlin:core:1-3:schemas"

/*
 * Name a request's sender in its header, twice, from the sender's credentials, and set the
 * action; value holds what is not yet freed when this fails.
 */
static inline __attribute__((always_inline)) unsigned int
MlnServiceRequestParts(MlnNemoMessage *message, const char *action, MlnBuffer **value)
{
	unsigned int ret;

	ret = MlnImpl_81079EE8(message->credentials->first, value);
	if (ret != 0)
		return ret;
	ret = MlnImpl_8107A04C(&message->header, MLN_NEMO_PART_SENDER, *value);
	if (ret != 0)
		return ret;
	ret = MlnImpl_8107A04C(&message->header, MLN_NEMO_PART_REQUESTER, *value);
	if (ret != 0)
		return ret;
	MlnBufferDestroy(MLN_HEAP_GENERAL, *value);
	*value = NULL;
	*value = MlnBufferCreate(MLN_HEAP_GENERAL, MlnStrlen(action), action);
	if (*value == NULL)
		return 2;
	ret = MlnImpl_8107A04C(&message->header, MLN_NEMO_PART_ACTION, *value);
	if (ret != 0)
		return ret;
	MlnBufferDestroy(MLN_HEAP_GENERAL, *value);
	*value = NULL;
	return 0;
}

/* a node acquisition request: the registration payload given, for the service of that name */
int MlnImpl_81066CE0(MlnExchange *exchange, MlnBufferGroups **credentials,
		     unsigned int nodeCount, MlnXmlNode **nodes, const MlnBuffer *name,
		     const MlnBuffer *payload, MlnBuffer **request)
{
	MlnNemoMessage *message;
	MlnXmlNode *body = NULL;
	MlnBuffer *sent = NULL;
	MlnBuffer *value = NULL;
	MlnXmlBuildContent content;
	MlnXmlNode **copies;
	char *string = NULL;
	unsigned int i;
	int ret;

	if (exchange == NULL || exchange->protocol != MLN_EXCHANGE_REGISTRATION ||
	    exchange->message.keys[0] == NULL || exchange->message.keys[1] == NULL) {
		ret = 4;
		goto fail;
	}
	ret = MlnImpl_81078EB8(payload, MLN_REGISTRATION_SERVICE_PREFIX,
			       MLN_REGISTRATION_SERVICE_URI, &body);
	if (ret != 0)
		goto fail;
	content.kind = MLN_XML_BUILD_NODE;
	content.value.node = body;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "NodeAcquisitionRequestPayload",
				 MLN_REGISTRATION_SERVICE_PREFIX, MLN_REGISTRATION_SERVICE_URI, 0,
				 NULL, 1, &content, 1, &body);
	if (ret != 0)
		goto fail;

	message = &exchange->message;
	message->digestMethod = 1;
	message->signatureMethod = 1;
	message->body = body;
	body = NULL;
	message->credentials = *credentials;
	*credentials = NULL;
	if (nodes != NULL) {
		copies = MlnAllocate(MLN_HEAP_GENERAL, nodeCount * sizeof(*copies));
		if (copies == NULL) {
			ret = 2;
			goto fail;
		}
		for (i = 0; i < nodeCount; i++)
			copies[i] = MlnXmlNodeRetain(MLN_HEAP_GENERAL, nodes[i]);
	} else {
		copies = NULL;
		nodeCount = 0;
	}
	message->nodeCount = nodeCount;
	message->nodes = copies;
	message->nonce = NULL;
	message->peerNonce = NULL;
	message->peer = MlnBufferCreate(MLN_HEAP_GENERAL, name->size, name->data);
	if (message->peer == NULL) {
		ret = 2;
		goto fail;
	}
	ret = MlnServiceRequestParts(message,
				     "urn:marlin:broadband:1-1:registration-service:nodeAcquisition",
				     &value);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_81072240(message, 11, 1, &sent);
	if (ret == 0) {
		*request = sent;
		sent = NULL;
		goto out;
	}
fail:
	ret = MlnImpl_810685FC(ret);
	MlnImpl_81068798(exchange);
out:
	MlnFree(MLN_HEAP_GENERAL, string);
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	MlnBufferDestroy(MLN_HEAP_GENERAL, sent);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, body);
	return ret;
}

/* the node of a node acquisition response, as XML text */
unsigned int MlnImpl_81066F74(MlnExchange *exchange, const MlnBuffer *response,
			      MlnBuffer **node)
{
	MlnXmlNode *payload = NULL;
	MlnXmlNodeList *bundles = NULL;
	MlnBuffer *text = NULL;
	unsigned int ret;

	if (exchange == NULL || exchange->protocol != MLN_EXCHANGE_REGISTRATION) {
		ret = 4;
		goto fail;
	}
	ret = MlnImpl_81074EC4(response->size, response->data, &exchange->message,
			       "NodeAcquisitionResponsePayload", MLN_REGISTRATION_SERVICE_URI, 11, 2,
			       9, 10, &payload);
	if (ret != 0)
		goto fail;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, payload, "Bundle", MLN_OCTOPUS_NAMESPACE,
			      &bundles);
	if (ret != 0)
		goto fail;
	if (bundles->count != 1) {
		ret = MLN_EXCHANGE_ERROR_FORMAT;
		goto fail;
	}
	ret = MlnXmlNodeToText(MLN_HEAP_GENERAL, bundles->nodes[0], &text);
	if (ret != 0)
		goto fail;
	MlnXmlListFree(MLN_HEAP_GENERAL, bundles);
	bundles = NULL;
	*node = text;
	text = NULL;
	goto out;
fail:
	ret = MlnImpl_810685FC(ret);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, payload);
	MlnXmlListFree(MLN_HEAP_GENERAL, bundles);
	return ret;
}

/*
 * A link acquisition request: a link from a node (or the node that replaces it) to another,
 * with the registration payload given.
 */
int MlnImpl_810670A0(MlnExchange *exchange, MlnBufferGroups **credentials, int nodeCount,
		     MlnXmlNode **nodes, MlnXmlNode *from, MlnXmlNode *fromOverride,
		     MlnXmlNode *to, const MlnBuffer *name, const MlnBuffer *payload,
		     MlnBuffer **request)
{
	MlnNemoMessage *message;
	MlnXmlNodeList *toBundles = NULL;
	MlnXmlNodeList *overrideBundles = NULL;
	MlnXmlNodeList *fromBundles = NULL;
	MlnXmlNodeList *fromList;
	MlnXmlNode *built = NULL;
	MlnXmlNode *body = NULL;
	MlnBuffer *sent = NULL;
	MlnBuffer *value = NULL;
	MlnXmlBuildContent contents[3];
	MlnXmlNode **copies;
	char *string = NULL;
	int ret;
	int i;

	if (exchange == NULL || exchange->protocol != MLN_EXCHANGE_REGISTRATION ||
	    exchange->message.keys[0] == NULL || exchange->message.keys[1] == NULL) {
		ret = 4;
		goto fail;
	}
	ret = MlnImpl_81078EB8(payload, MLN_REGISTRATION_SERVICE_PREFIX,
			       MLN_REGISTRATION_SERVICE_URI, &body);
	if (ret != 0)
		goto fail;
	if (fromOverride != NULL) {
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, fromOverride, "Bundle",
				      MLN_OCTOPUS_NAMESPACE, &overrideBundles);
		if (ret != 0)
			goto fail;
		fromList = overrideBundles;
		if (fromList->count != 1) {
			ret = MLN_EXCHANGE_ERROR_FORMAT;
			goto fail;
		}
	} else {
		ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, from, "Bundle", MLN_OCTOPUS_NAMESPACE,
				      &fromBundles);
		if (ret != 0)
			goto fail;
		fromList = fromBundles;
		if (fromList->count != 1) {
			ret = MLN_EXCHANGE_ERROR_FORMAT;
			goto fail;
		}
	}
	contents[0].kind = MLN_XML_BUILD_NODE;
	contents[0].value.node = fromList->nodes[0];
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, to, "Bundle", MLN_OCTOPUS_NAMESPACE, &toBundles);
	if (ret != 0)
		goto fail;
	if (toBundles->count != 1) {
		ret = MLN_EXCHANGE_ERROR_FORMAT;
		goto fail;
	}
	contents[1].kind = MLN_XML_BUILD_NODE;
	contents[1].value.node = toBundles->nodes[0];
	contents[2].kind = MLN_XML_BUILD_NODE;
	contents[2].value.node = body;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "LinkAcquisitionRequestPayload",
				 MLN_REGISTRATION_SERVICE_PREFIX, MLN_REGISTRATION_SERVICE_URI, 0,
				 NULL, 3, contents, 0, &built);
	if (ret != 0)
		goto fail;
	MlnXmlListFree(MLN_HEAP_GENERAL, toBundles);
	toBundles = NULL;
	MlnXmlListFree(MLN_HEAP_GENERAL, overrideBundles);
	overrideBundles = NULL;
	MlnXmlListFree(MLN_HEAP_GENERAL, fromBundles);
	fromBundles = NULL;
	MlnXmlNodeFree(MLN_HEAP_GENERAL, body);

	message = &exchange->message;
	message->digestMethod = 1;
	message->signatureMethod = 1;
	message->body = built;
	built = NULL;
	body = NULL;
	message->credentials = *credentials;
	*credentials = NULL;
	if (nodes != NULL) {
		copies = MlnAllocate(MLN_HEAP_GENERAL, nodeCount * sizeof(*copies));
		if (copies == NULL) {
			ret = 2;
			goto fail;
		}
		for (i = 0; i < nodeCount; i++)
			copies[i] = MlnXmlNodeRetain(MLN_HEAP_GENERAL, nodes[i]);
	} else {
		copies = NULL;
		nodeCount = 0;
	}
	message->nodeCount = nodeCount;
	message->nodes = copies;
	message->nonce = NULL;
	message->peerNonce = NULL;
	message->peer = MlnBufferCreate(MLN_HEAP_GENERAL, name->size, name->data);
	if (message->peer == NULL) {
		ret = 2;
		goto fail;
	}
	ret = MlnServiceRequestParts(message,
				     "urn:marlin:broadband:1-1:registration-service:linkAcquisition",
				     &value);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_81072240(message, 11, 1, &sent);
	if (ret == 0) {
		*request = sent;
		sent = NULL;
		goto out;
	}
fail:
	ret = MlnImpl_810685FC(ret);
	MlnImpl_81068798(exchange);
out:
	MlnFree(MLN_HEAP_GENERAL, string);
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	MlnBufferDestroy(MLN_HEAP_GENERAL, sent);
	MlnXmlListFree(MLN_HEAP_GENERAL, toBundles);
	MlnXmlListFree(MLN_HEAP_GENERAL, overrideBundles);
	MlnXmlListFree(MLN_HEAP_GENERAL, fromBundles);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, built);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, body);
	return ret;
}

/* the link of a link acquisition response, as XML text, and the agent it carries */
unsigned int MlnImpl_81067494(MlnExchange *exchange, const MlnBuffer *response,
			      MlnBuffer **link, MlnXmlNode **agent)
{
	MlnXmlNode *payload = NULL;
	MlnXmlNode *carrier = NULL;
	MlnXmlNodeList *bundles = NULL;
	MlnBuffer *text = NULL;
	unsigned int ret;

	if (exchange == NULL || exchange->protocol != MLN_EXCHANGE_REGISTRATION) {
		ret = 4;
		goto fail;
	}
	ret = MlnImpl_81074EC4(response->size, response->data, &exchange->message,
			       "LinkAcquisitionResponsePayload", MLN_REGISTRATION_SERVICE_URI, 12, 2,
			       9, 10, &payload);
	if (ret != 0)
		goto fail;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, payload, "Bundle", MLN_OCTOPUS_NAMESPACE,
			      &bundles);
	if (ret != 0)
		goto fail;
	if (bundles->count != 1) {
		ret = MLN_EXCHANGE_ERROR_FORMAT;
		goto fail;
	}
	ret = MlnXmlNodeToText(MLN_HEAP_GENERAL, bundles->nodes[0], &text);
	if (ret != 0)
		goto fail;
	MlnXmlListFree(MLN_HEAP_GENERAL, bundles);
	bundles = NULL;
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, payload, "AgentCarrier", MLN_MARLIN_CORE_URI,
			     &carrier);
	if (ret != MLN_XML_ERROR_NOT_FOUND && ret != 0)
		goto fail;
	*agent = carrier;
	carrier = NULL;
	*link = text;
	text = NULL;
	ret = 0;
	goto out;
fail:
	ret = MlnImpl_810685FC(ret);
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, carrier);
	MlnBufferDestroy(MLN_HEAP_GENERAL, text);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, payload);
	MlnXmlListFree(MLN_HEAP_GENERAL, bundles);
	return ret;
}

/* a DRM objects confirmation, carrying the node given (if any) */
int MlnImpl_81067618(MlnExchange *exchange, MlnXmlNode *node, MlnBuffer **reply)
{
	MlnNemoMessage *message;
	MlnXmlNode *confirm = NULL;
	MlnBuffer *sent = NULL;
	MlnBuffer *value = NULL;
	MlnXmlBuildContent content;
	int count;
	int ret;

	if (exchange == NULL || exchange->protocol != MLN_EXCHANGE_REGISTRATION ||
	    exchange->message.keys[0] == NULL || exchange->message.keys[1] == NULL) {
		ret = 4;
		goto fail;
	}
	count = 0;
	if (node != NULL) {
		content.value.node = node;
		count = 1;
		content.kind = MLN_XML_BUILD_NODE;
	}
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "DRMObjectConfirmPayload",
				 MLN_REGISTRATION_SERVICE_PREFIX, MLN_REGISTRATION_SERVICE_URI, 0,
				 NULL, count, &content, 0, &confirm);
	if (ret != 0)
		goto fail;
	message = &exchange->message;
	ret = MlnServiceRequestParts(message,
				     "urn:marlin:broadband:1-1:registration-service:confirmDRMObjects",
				     &value);
	if (ret != 0)
		goto fail;
	message->digestMethod = 1;
	message->signatureMethod = 1;
	if (message->body != NULL)
		MlnXmlNodeFree(MLN_HEAP_GENERAL, message->body);
	message->body = confirm;
	confirm = NULL;
	if (message->peer == NULL) {
		ret = 4;
		goto fail;
	}
	ret = MlnImpl_81072240(message, 12, 4, &sent);
	if (ret == 0) {
		*reply = sent;
		sent = NULL;
		goto out;
	}
fail:
	ret = MlnImpl_810685FC(ret);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	MlnBufferDestroy(MLN_HEAP_GENERAL, sent);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, confirm);
	return ret;
}

/* a deregistration request: of a node from another, with the registration payload given */
int MlnImpl_81067814(MlnExchange *exchange, MlnBufferGroups **credentials,
		     unsigned int nodeCount, MlnXmlNode **nodes, MlnXmlNode *from,
		     MlnXmlNode *to, const MlnBuffer *name, const MlnBuffer *payload,
		     MlnBuffer **request)
{
	MlnNemoMessage *message;
	MlnXmlNodeList *fromBundles = NULL;
	MlnXmlNodeList *toBundles = NULL;
	MlnXmlNode *built = NULL;
	MlnXmlNode *body = NULL;
	MlnBuffer *sent = NULL;
	MlnBuffer *value = NULL;
	MlnXmlBuildContent contents[3];
	MlnXmlNode **copies;
	char *string = NULL;
	unsigned int i;
	int ret;

	if (exchange == NULL || exchange->protocol != MLN_EXCHANGE_REGISTRATION ||
	    exchange->message.keys[0] == NULL || exchange->message.keys[1] == NULL) {
		ret = 4;
		goto fail;
	}
	ret = MlnImpl_81078EB8(payload, MLN_REGISTRATION_SERVICE_PREFIX,
			       MLN_REGISTRATION_SERVICE_URI, &body);
	if (ret != 0)
		goto fail;
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, from, "Bundle", MLN_OCTOPUS_NAMESPACE,
			      &fromBundles);
	if (ret != 0)
		goto fail;
	if (fromBundles->count != 1) {
		ret = MLN_EXCHANGE_ERROR_FORMAT;
		goto fail;
	}
	ret = MlnXmlFindNodes(MLN_HEAP_GENERAL, to, "Bundle", MLN_OCTOPUS_NAMESPACE, &toBundles);
	if (ret != 0)
		goto fail;
	if (toBundles->count != 1) {
		ret = MLN_EXCHANGE_ERROR_FORMAT;
		goto fail;
	}
	contents[0].kind = MLN_XML_BUILD_NODE;
	contents[0].value.node = fromBundles->nodes[0];
	contents[1].kind = MLN_XML_BUILD_NODE;
	contents[1].value.node = toBundles->nodes[0];
	contents[2].kind = MLN_XML_BUILD_NODE;
	contents[2].value.node = body;
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "DeregistrationRequestPayload",
				 MLN_REGISTRATION_SERVICE_PREFIX, MLN_REGISTRATION_SERVICE_URI, 0,
				 NULL, 3, contents, 0, &built);
	if (ret != 0)
		goto fail;
	MlnXmlListFree(MLN_HEAP_GENERAL, fromBundles);
	fromBundles = NULL;
	MlnXmlListFree(MLN_HEAP_GENERAL, toBundles);
	toBundles = NULL;
	MlnXmlNodeFree(MLN_HEAP_GENERAL, body);

	message = &exchange->message;
	message->digestMethod = 1;
	message->signatureMethod = 1;
	message->body = built;
	built = NULL;
	body = NULL;
	message->credentials = *credentials;
	*credentials = NULL;
	if (nodes != NULL) {
		copies = MlnAllocate(MLN_HEAP_GENERAL, nodeCount * sizeof(*copies));
		if (copies == NULL) {
			ret = 2;
			goto fail;
		}
		i = 0;
		while (i < nodeCount) {
			copies[i] = MlnXmlNodeRetain(MLN_HEAP_GENERAL, nodes[i]);
			i++;
		}
	} else {
		copies = NULL;
		nodeCount = 0;
	}
	message->nodeCount = nodeCount;
	message->nodes = copies;
	message->nonce = NULL;
	message->peerNonce = NULL;
	message->peer = MlnBufferCreate(MLN_HEAP_GENERAL, name->size, name->data);
	if (message->peer == NULL) {
		ret = 2;
		goto fail;
	}
	ret = MlnServiceRequestParts(message,
				     "urn:marlin:broadband:1-1:registration-service:deregistration",
				     &value);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_81072240(message, 13, 1, &sent);
	if (ret == 0) {
		*request = sent;
		sent = NULL;
		goto out;
	}
fail:
	ret = MlnImpl_810685FC(ret);
	MlnImpl_81068798(exchange);
out:
	MlnFree(MLN_HEAP_GENERAL, string);
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	MlnBufferDestroy(MLN_HEAP_GENERAL, sent);
	MlnXmlListFree(MLN_HEAP_GENERAL, fromBundles);
	MlnXmlListFree(MLN_HEAP_GENERAL, toBundles);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, built);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, body);
	return ret;
}

/* the agent a deregistration response carries, if any */
unsigned int MlnImpl_81067BA4(MlnExchange *exchange, const MlnBuffer *response,
			      MlnXmlNode **agent)
{
	MlnXmlNode *payload = NULL;
	MlnXmlNode *carrier = NULL;
	MlnXmlNodeList *bundles = NULL;
	unsigned int ret;

	if (exchange == NULL || exchange->protocol != MLN_EXCHANGE_REGISTRATION) {
		ret = 4;
		goto fail;
	}
	ret = MlnImpl_81074EC4(response->size, response->data, &exchange->message,
			       "DeregistrationResponsePayload", MLN_REGISTRATION_SERVICE_URI, 13, 2,
			       9, 10, &payload);
	if (ret != 0)
		goto fail;
	ret = MlnXmlFindNode(MLN_HEAP_GENERAL, payload, "AgentCarrier", MLN_MARLIN_CORE_URI,
			     &carrier);
	if (ret != MLN_XML_ERROR_NOT_FOUND && ret != 0)
		goto fail;
	*agent = carrier;
	carrier = NULL;
	ret = 0;
	goto out;
fail:
	ret = MlnImpl_810685FC(ret);
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, carrier);
	MlnXmlListFree(MLN_HEAP_GENERAL, bundles);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, payload);
	return ret;
}

/*
 * A step of a license transfer: the document given sent to the peer, by the sender of the
 * credentials given. The first step names the peer and the move; a later one confirms.
 */
int MlnImpl_81067CA4(MlnExchange *exchange, int step, MlnBufferGroups **credentials,
		     const MlnBuffer *peer, MlnXmlNode **document, MlnBuffer **request)
{
	MlnNemoMessage *message;
	MlnBuffer *sent = NULL;
	MlnBuffer *node = NULL;
	MlnBuffer *action = NULL;
	unsigned int i;
	int ret;

	if (exchange == NULL) {
		ret = 4;
		goto fail;
	}
	if (exchange->protocol != MLN_EXCHANGE_TRANSFER) {
		ret = 4;
		goto fail;
	}
	if (credentials == NULL || document == NULL || request == NULL) {
		ret = 3;
		goto fail;
	}
	if (peer == NULL && step == 1) {
		ret = 3;
		goto fail;
	}

	message = &exchange->message;
	if (step == 1)
		message->header.present = 0;
	message->digestMethod = 1;
	message->signatureMethod = 1;
	MlnXmlNodeFree(MLN_HEAP_GENERAL, message->body);
	message->body = *document;
	*document = NULL;
	if (message->credentials != NULL)
		MlnImpl_81042A88(message->credentials);
	message->credentials = *credentials;
	*credentials = NULL;
	if (message->nodeCount != 0 && message->nodes != NULL) {
		i = 0;
		do
			MlnXmlNodeFree(MLN_HEAP_GENERAL, message->nodes[i++]);
		while (i < message->nodeCount);
		MlnFree(MLN_HEAP_GENERAL, message->nodes);
		message->nodeCount = 0;
		message->nodes = NULL;
	}

	if (step == 1) {
		if (message->nonce != NULL)
			MlnBufferDestroy(MLN_HEAP_GENERAL, message->nonce);
		message->nonce = NULL;
		if (message->peer != NULL) {
			if (MlnBufferCompare(MLN_HEAP_GENERAL, message->peer, peer) != 0) {
				ret = 5;
				goto fail;
			}
		} else {
			message->peer = MlnBufferCreate(MLN_HEAP_GENERAL, peer->size, peer->data);
			if (message->peer == NULL) {
				ret = 2;
				goto fail;
			}
		}
		/* the step goes back to the sender of the message before */
		if (message->header.parts[4] != NULL) {
			ret = MlnImpl_8107A04C(&message->header, MLN_NEMO_PART_RECIPIENT,
					       message->header.parts[4]);
			if (ret != 0)
				goto fail;
			message->header.present = MLN_NEMO_PART_RECIPIENT;
		}
	}
	ret = MlnImpl_81079EE8(message->credentials->first, &node);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_8107A04C(&message->header, MLN_NEMO_PART_SENDER, node);
	if (ret != 0)
		goto fail;
	if (step != 1) {
		ret = MlnImpl_8107A04C(&message->header, MLN_NEMO_PART_CONFIRMER, node);
		if (ret != 0)
			goto fail;
		ret = MlnImpl_8107A04C(&message->header, MLN_NEMO_PART_ORIGIN,
				       message->header.parts[8]);
		if (ret != 0)
			goto fail;
		MlnBufferDestroy(MLN_HEAP_GENERAL, node);
		node = NULL;
		ret = MlnImpl_81072240(message, 7, step, &sent);
		if (ret != 0)
			goto fail;
	} else {
		ret = MlnImpl_8107A04C(&message->header, MLN_NEMO_PART_REQUESTER, node);
		if (ret != 0)
			goto fail;
		MlnBufferDestroy(MLN_HEAP_GENERAL, node);
		node = NULL;
		action = MlnBufferCreate(MLN_HEAP_GENERAL,
					 MlnStrlen("urn:marlin:core:1-0:service:license-transfer:move"),
					 "urn:marlin:core:1-0:service:license-transfer:move");
		if (action == NULL) {
			ret = 2;
			goto fail;
		}
		ret = MlnImpl_8107A04C(&message->header, MLN_NEMO_PART_ACTION, action);
		if (ret != 0)
			goto fail;
		MlnBufferDestroy(MLN_HEAP_GENERAL, action);
		action = NULL;
		ret = MlnImpl_81072240(message, 7, 1, &sent);
		if (ret != 0)
			goto fail;
		if (message->peerNonce != NULL) {
			MlnBufferDestroy(MLN_HEAP_GENERAL, message->peerNonce);
			message->peerNonce = NULL;
		}
	}
	*request = sent;
	sent = NULL;
	goto out;
fail:
	ret = MlnImpl_810685FC(ret);
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, node);
	MlnBufferDestroy(MLN_HEAP_GENERAL, action);
	MlnBufferDestroy(MLN_HEAP_GENERAL, sent);
	return ret;
}

/* the payload of that name a license transfer step's response carries */
unsigned int MlnImpl_81068060(MlnExchange *exchange, int step, const char *name,
			      const MlnBuffer *response, MlnXmlNode **document)
{
	MlnNemoMessage *message;
	MlnXmlNode *payload = NULL;
	MlnBufferGroups *credentials = NULL;
	unsigned int ret;

	if (exchange == NULL || exchange->protocol != MLN_EXCHANGE_TRANSFER) {
		ret = 4;
		goto fail;
	}
	if (name == NULL || response == NULL || document == NULL) {
		ret = 3;
		goto fail;
	}
	message = &exchange->message;
	if (step == 1) {
		if (message->bundles != NULL)
			MlnXmlListFree(MLN_HEAP_GENERAL, message->bundles);
		if (message->peerNonce != NULL)
			MlnBufferDestroy(MLN_HEAP_GENERAL, message->peerNonce);
		message->bundles = NULL;
		message->peerNonce = NULL;
	}
	ret = MlnImpl_81064E18(&credentials);
	if (ret != 0)
		goto fail;
	if (message->credentials != NULL) {
		if (MlnBufferCompare(MLN_HEAP_GENERAL, message->credentials->first,
				     credentials->first) != 0) {
			ret = 5;
			goto fail;
		}
	} else {
		message->credentials = credentials;
		credentials = NULL;
	}
	ret = MlnImpl_81074EC4(response->size, response->data, message, name, "", 7, step, 11, 12,
			       &payload);
	if (ret != 0)
		goto fail;
	*document = payload;
	payload = NULL;
	goto out;
fail:
	ret = MlnImpl_810685FC(ret);
out:
	MlnXmlNodeFree(MLN_HEAP_GENERAL, payload);
	MlnImpl_81042A88(credentials);
	return ret;
}
