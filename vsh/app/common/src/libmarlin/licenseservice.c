/*
 * The license service's request and response over a NEMO exchange: license acquisition
 * (0x810681E4-). The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_LICENSE_SERVICE_PREFIX	"kfls"
#define MLN_LICENSE_SERVICE_URI							\
	"urn:marlin:broadband:1-1:nemo:services:schemas:license-service"

/* a license acquisition request: for the node given, with the license payload given */
int MlnImpl_810681E4(MlnExchange *exchange, MlnBufferGroups **credentials,
		     unsigned int nodeCount, MlnXmlNode **nodes, MlnXmlNode *node,
		     const MlnBuffer *name, const MlnBuffer *payload, MlnBuffer **request)
{
	MlnNemoMessage *message;
	MlnBuffer *sent = NULL;
	MlnBuffer *value = NULL;
	MlnXmlNode *parts[3];		/* the request built, unused, the payload */
	MlnXmlBuildContent contents[2];
	MlnXmlNode **copies;
	char *string = NULL;
	unsigned int i;
	int ret;

	if (exchange == NULL || exchange->protocol != MLN_EXCHANGE_LICENSE ||
	    exchange->message.keys[0] == NULL || exchange->message.keys[1] == NULL) {
		ret = 4;
		goto fail;
	}
	MlnMemset(parts, 0, sizeof(parts));
	ret = MlnXmlNodeGetElement(MLN_HEAP_GENERAL, node, &parts[0]);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_81078EB8(payload, MLN_LICENSE_SERVICE_PREFIX, MLN_LICENSE_SERVICE_URI,
			       &parts[2]);
	if (ret != 0)
		goto fail;
	contents[0].kind = MLN_XML_BUILD_NODE;
	contents[0].value.node = parts[0];
	contents[1].kind = MLN_XML_BUILD_NODE;
	contents[1].value.node = parts[2];
	ret = MlnXmlElementBuild(MLN_HEAP_GENERAL, "LicenseAcquisitionRequestPayload",
				 MLN_LICENSE_SERVICE_PREFIX, MLN_LICENSE_SERVICE_URI, 0, NULL, 2,
				 contents, 1, &parts[0]);
	if (ret != 0)
		goto fail;

	message = &exchange->message;
	message->integrity = 1;
	message->confidentiality = 1;
	message->body = parts[0];
	parts[2] = NULL;
	parts[0] = NULL;
	message->credentials = *credentials;
	*credentials = NULL;
	if (nodes != NULL) {
		copies = MlnAllocate(MLN_HEAP_GENERAL, nodeCount * sizeof(*copies));
		if (copies == NULL) {
			ret = 2;
			goto fail;
		}
		if (nodeCount != 0) {
			for (i = 0; i < nodeCount; i++)
				copies[i] = MlnXmlNodeRetain(MLN_HEAP_GENERAL, nodes[i]);
		} else {
			nodeCount = 0;
		}
	} else {
		copies = NULL;
		nodeCount = 0;
	}
	message->nodeCount = nodeCount;
	message->nodes = copies;
	message->received = NULL;
	message->transferKey = NULL;
	message->peer = MlnBufferCreate(MLN_HEAP_GENERAL, name->size, name->data);
	if (message->peer == NULL) {
		ret = 2;
		goto fail;
	}
	ret = MlnImpl_81079EE8(message->credentials->first, &value);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_8107A04C(&message->header, MLN_NEMO_PART_SENDER, value);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_8107A04C(&message->header, MLN_NEMO_PART_REQUESTER, value);
	if (ret != 0)
		goto fail;
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	value = NULL;
	value = MlnBufferCreate(MLN_HEAP_GENERAL,
				MlnStrlen("urn:marlin:broadband:1-1:license-service:requestLicense"),
				"urn:marlin:broadband:1-1:license-service:requestLicense");
	if (value == NULL) {
		ret = 2;
		goto fail;
	}
	ret = MlnImpl_8107A04C(&message->header, MLN_NEMO_PART_ACTION, value);
	if (ret != 0)
		goto fail;
	MlnBufferDestroy(MLN_HEAP_GENERAL, value);
	value = NULL;
	ret = MlnImpl_81072240(message, 8, 1, &sent);
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
	MlnXmlNodeFree(MLN_HEAP_GENERAL, parts[0]);
	MlnXmlNodeFree(MLN_HEAP_GENERAL, parts[2]);
	return ret;
}

/* the license of a license acquisition response, as XML text */
unsigned int MlnImpl_810684CC(MlnExchange *exchange, const MlnBuffer *response,
			      MlnBuffer **license)
{
	MlnXmlNode *payload = NULL;
	MlnXmlNodeList *bundles = NULL;
	MlnBuffer *text = NULL;
	unsigned int ret;

	if (exchange == NULL || exchange->protocol != MLN_EXCHANGE_LICENSE) {
		ret = 4;
		goto fail;
	}
	ret = MlnImpl_81074EC4(response->size, response->data, &exchange->message,
			       "LicenseAcquisitionResponsePayload", MLN_LICENSE_SERVICE_URI, 8, 2, 9,
			       10, &payload);
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
	*license = text;
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
