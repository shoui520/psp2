/*
 * A service configuration's endpoints: each Nemo node's id and keys, its roles, and the address
 * the WSDL gives for the service of a role.
 */
#include "mln_internal.h"

#define MLN_NS_WSSE \
	"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd"
#define MLN_NS_SAML		"urn:oasis:names:tc:SAML:1.0:assertion"
#define MLN_NS_NEMO_CORE	"http://nemo.intertrust.com/2005/10/core"
#define MLN_NS_WSDL		"http://schemas.xmlsoap.org/wsdl/"
#define MLN_NS_WSDL_SOAP	"http://schemas.xmlsoap.org/wsdl/soap/"
#define MLN_USAGE_REQUEST_KEY \
	"http://nemo.intertrust.com/2005/10/security/secure-protocol/basic/1.0#request-encryptionKey"
#define MLN_USAGE_RESPONSE_KEY \
	"http://nemo.intertrust.com/2005/10/security/secure-protocol/basic/1.0#response-signingKey"
#define MLN_USAGE_ROLE		"http://nemo.intertrust.com/2004/attribute/role"

/* the elements of a name under a node; none is an empty list */
__attribute__((noinline)) int MlnImpl_81081890(MlnXmlNode *node, const char *name, const char *ns, MlnXmlNodeList **list)
{
	int ret = MlnXmlFindNodes(MLN_HEAP_SECURE, node, name, ns, list);

	if (ret == 0)
		return 0;
	if (ret == MLN_XML_ERROR_NOT_FOUND)
		return 0;
	return ret == 2 ? 2 : 7;
}

/*
 * a node's id attribute; as Sony's code is written, a node that has one also gets 7 back (only a
 * node without an id gives 0)
 */
__attribute__((noinline)) int MlnImpl_810818F0(MlnXmlNode *node, MlnBuffer **id)
{
	MlnBuffer *value = NULL;
	int ret;

	ret = MlnXmlGetAttribute(MLN_HEAP_SECURE, node, NULL, "id", &value);
	if (ret == 0)
		*id = value;
	value = NULL;
	if (ret == MLN_XML_ERROR_NOT_FOUND) {
		ret = 0;
		*id = NULL;
	} else {
		ret = 7;
	}
	MlnBufferDestroy(MLN_HEAP_SECURE, value);
	return ret;
}

/*
 * a SecurityTokenReference's embedded token: a key (the BinarySecurityToken's text) or, with no
 * key asked for, a role (the SAML Assertion as XML text, a list of one buffer)
 */
__attribute__((noinline)) int MlnImpl_8108196C(MlnXmlNode *node, MlnBuffer **key, MlnConfigList *roles)
{
	MlnXmlNode *embedded = NULL;
	MlnXmlNode *token = NULL;
	MlnBuffer *text = NULL;
	MlnBuffer **items = NULL;
	int ret;

	ret = MlnXmlFindNode(MLN_HEAP_SECURE, node, "Embedded", MLN_NS_WSSE, &embedded);
	if (ret != 0) {
		ret = ret == 2 ? 2 : MLN_CONFIG_ERROR_FORMAT;
		goto out;
	}
	if (key != NULL) {
		ret = MlnXmlFindNode(MLN_HEAP_SECURE, embedded, "BinarySecurityToken", MLN_NS_WSSE,
				     &token);
		if (ret != 0) {
			ret = ret == 2 ? 2 : MLN_CONFIG_ERROR_FORMAT;
			goto out;
		}
		ret = MlnXmlGetText(MLN_HEAP_SECURE, token, &text);
		if (ret != 0) {
			ret = ret == 2 ? 2 : MLN_CONFIG_ERROR_FORMAT;
			goto out;
		}
		*key = text;
		text = NULL;
		goto out;
	}
	ret = MlnXmlFindNode(MLN_HEAP_SECURE, embedded, "Assertion", MLN_NS_SAML, &token);
	if (ret != 0) {
		ret = ret == 2 ? 2 : MLN_CONFIG_ERROR_FORMAT;
		goto out;
	}
	roles->kind = 1;
	roles->count = 1;
	items = MlnAllocate(MLN_HEAP_SECURE, sizeof(*items));
	if (items == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(items, 0, roles->count * sizeof(*items));
	ret = MlnXmlNodeToText(MLN_HEAP_SECURE, token, &text);
	if (ret != 0) {
		ret = ret == 2 ? 2 : 7;
		goto out;
	}
	items[0] = text;
	roles->items = items;
	items = NULL;
	text = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_SECURE, text);
	MlnFree(MLN_HEAP_SECURE, items);
	MlnXmlNodeFree(MLN_HEAP_SECURE, token);
	MlnXmlNodeFree(MLN_HEAP_SECURE, embedded);
	return ret;
}

__attribute__((noinline)) void MlnImpl_81081B2C(MlnServiceEndpoint *endpoint)
{
	if (endpoint == NULL)
		return;
	MlnBufferDestroy(MLN_HEAP_SECURE, endpoint->id);
	if (endpoint->nodeId != NULL)
		MlnBufferDestroy(MLN_HEAP_SECURE, endpoint->nodeId);
	if (endpoint->requestEncryptionKey != NULL)
		MlnBufferDestroy(MLN_HEAP_SECURE, endpoint->requestEncryptionKey);
	if (endpoint->responseSigningKey != NULL)
		MlnBufferDestroy(MLN_HEAP_SECURE, endpoint->responseSigningKey);
	MlnImpl_81033AE8(&endpoint->roles);
	if (endpoint->location != NULL)
		MlnFree(MLN_HEAP_SECURE, endpoint->location);
	MlnImpl_81033AE8(&endpoint->spareList);
	MlnFree(MLN_HEAP_SECURE, endpoint);
}

/* an endpoint's NodeInfo: its node id and the keys and roles its token references give */
static int MlnEndpointReadNodeInfo(MlnXmlNode *node, MlnServiceEndpoint *endpoint)
{
	MlnXmlNodeList *list = NULL;
	MlnXmlNode *info = NULL;
	MlnBuffer *value = NULL;
	int ret, i, count;

	ret = MlnXmlFindNode(MLN_HEAP_SECURE, node, "NodeInfo", MLN_NS_NEMO_CORE, &info);
	if (ret != 0)
		goto parse;
	ret = MlnImpl_81081890(info, "NodeId", MLN_NS_NEMO_CORE, &list);
	if (ret != 0)
		goto out;
	if (list != NULL) {
		if (list->count != 1) {
			ret = MLN_CONFIG_ERROR_FORMAT;
			goto out;
		}
		ret = MlnXmlGetText(MLN_HEAP_SECURE, list->nodes[0], &value);
		if (ret != 0)
			goto parse;
		endpoint->nodeId = value;
		value = NULL;
		MlnXmlListFree(MLN_HEAP_SECURE, list);
		list = NULL;
	} else {
		endpoint->nodeId = NULL;
	}
	endpoint->requestEncryptionKey = NULL;
	endpoint->responseSigningKey = NULL;
	endpoint->roles.count = 0;
	endpoint->roles.items = NULL;
	endpoint->roles.kind = 1;
	ret = MlnImpl_81081890(info, "SecurityTokenReference", MLN_NS_WSSE, &list);
	if (ret != 0)
		goto out;
	count = list != NULL ? list->count : 0;
	for (i = 0; i < count; i++) {
		ret = MlnXmlGetAttribute(MLN_HEAP_SECURE, list->nodes[i], NULL, "nemosec:TargetUsage",
					 &value);
		if (ret != 0)
			goto parse;
		if (value->size == sizeof(MLN_USAGE_REQUEST_KEY) - 1 &&
		    MlnMemcmp(value->data, MLN_USAGE_REQUEST_KEY, value->size) == 0) {
			ret = MlnImpl_8108196C(list->nodes[i], &endpoint->requestEncryptionKey, NULL);
			if (ret != 0)
				goto out;
		} else if (value->size == sizeof(MLN_USAGE_RESPONSE_KEY) - 1 &&
			   MlnMemcmp(value->data, MLN_USAGE_RESPONSE_KEY, value->size) == 0) {
			ret = MlnImpl_8108196C(list->nodes[i], &endpoint->responseSigningKey, NULL);
			if (ret != 0)
				goto out;
		} else if (value->size == sizeof(MLN_USAGE_ROLE) - 1 &&
			   MlnMemcmp(value->data, MLN_USAGE_ROLE, value->size) == 0) {
			if (endpoint->roles.items != NULL) {
				ret = MLN_CONFIG_ERROR_FORMAT;
				goto out;
			}
			ret = MlnImpl_8108196C(list->nodes[i], NULL, &endpoint->roles);
			if (ret != 0)
				goto out;
		}
		MlnBufferDestroy(MLN_HEAP_SECURE, value);
		value = NULL;
	}
	ret = 0;
	goto out;
parse:
	ret = ret == 2 ? 2 : MLN_CONFIG_ERROR_FORMAT;
out:
	MlnBufferDestroy(MLN_HEAP_SECURE, value);
	MlnXmlNodeFree(MLN_HEAP_SECURE, info);
	MlnXmlListFree(MLN_HEAP_SECURE, list);
	return ret;
}

/* the address the WSDL's service of a role gives, as a string */
static int MlnEndpointReadLocation(MlnXmlNode *node, const char *role, MlnServiceEndpoint *endpoint)
{
	MlnXmlNodeList *services = NULL;
	MlnXmlNode *found = NULL;
	MlnXmlNode *address = NULL;
	MlnBuffer *value = NULL;
	char *location = NULL;
	int ret, i;

	ret = MlnXmlFindNode(MLN_HEAP_SECURE, node, "definitions", MLN_NS_WSDL, &found);
	if (ret == 0)
		ret = MlnXmlFindNodes(MLN_HEAP_SECURE, found, "service", MLN_NS_WSDL, &services);
	if (ret != 0)
		goto parse;
	for (i = 0; i < services->count; i++) {
		MlnXmlNode *service;
		char *copy;

		if (MlnXmlGetAttribute(MLN_HEAP_SECURE, services->nodes[i], NULL, "name",
				       &value) != 0)
			break;
		if (value->size != MlnStrlen(role) || MlnMemcmp(value->data, role, value->size) != 0) {
			MlnBufferDestroy(MLN_HEAP_SECURE, value);
			value = NULL;
			continue;
		}
		service = services->nodes[i];
		if (service == NULL)
			break;
		MlnXmlNodeFree(MLN_HEAP_SECURE, found);
		found = NULL;
		ret = MlnXmlFindNode(MLN_HEAP_SECURE, service, "port", MLN_NS_WSDL, &found);
		if (ret != 0)
			goto parse;
		ret = MlnXmlFindNode(MLN_HEAP_SECURE, found, "address", MLN_NS_WSDL_SOAP, &address);
		if (ret != 0) {
			if (ret != 2)
				break;
			goto out;
		}
		MlnBufferDestroy(MLN_HEAP_SECURE, value);
		value = NULL;
		ret = MlnXmlGetAttribute(MLN_HEAP_SECURE, address, NULL, "location", &value);
		if (ret != 0)
			break;
		copy = MlnAllocate(MLN_HEAP_SECURE, value->size + 1);
		if (copy == NULL) {
			ret = 2;
			goto out;
		}
		MlnMemcpy(copy, value->data, value->size);
		copy[value->size] = '\0';
		endpoint->location = copy;
		goto out;
	}
	ret = MLN_CONFIG_ERROR_FORMAT;
	goto out;
parse:
	if (ret != 2)
		ret = MLN_CONFIG_ERROR_FORMAT;
out:
	MlnBufferDestroy(MLN_HEAP_SECURE, value);
	MlnFree(MLN_HEAP_SECURE, location);
	MlnXmlNodeFree(MLN_HEAP_SECURE, found);
	MlnXmlNodeFree(MLN_HEAP_SECURE, address);
	MlnXmlListFree(MLN_HEAP_SECURE, services);
	return ret;
}

/* a service's endpoints, one per node of the list (taken and freed), as a list of kind 5 */
int MlnImpl_81081BB4(MlnXmlNodeList **list, MlnConfigList *out, const char *role)
{
	MlnXmlNodeList *nodes = *list;
	MlnServiceEndpoint **endpoints;
	MlnServiceEndpoint *endpoint;
	int count, i;
	int ret;

	if (nodes == NULL)
		return 0;
	count = nodes->count;
	endpoints = MlnAllocate(MLN_HEAP_SECURE, count * sizeof(*endpoints));
	if (endpoints == NULL) {
		endpoint = NULL;
		ret = 2;
		goto out;
	}
	MlnMemset(endpoints, 0, count * sizeof(*endpoints));
	for (i = 0; i < count; i++) {
		endpoint = MlnAllocate(MLN_HEAP_SECURE, sizeof(*endpoint));
		if (endpoint == NULL) {
			ret = 2;
			goto out;
		}
		MlnMemset(endpoint, 0, sizeof(*endpoint));
		ret = MlnImpl_810818F0(nodes->nodes[i], &endpoint->id);
		if (ret != 0)
			goto out;
		ret = MlnEndpointReadNodeInfo(nodes->nodes[i], endpoint);
		if (ret != 0)
			goto out;
		ret = MlnEndpointReadLocation(nodes->nodes[i], role, endpoint);
		if (ret != 0)
			goto out;
		endpoints[i] = endpoint;
	}
	out->kind = 5;
	out->items = endpoints;
	endpoints = NULL;
	endpoint = NULL;
	ret = 0;
	out->count = nodes->count;
out:
	MlnImpl_81081B2C(endpoint);
	if (endpoints != NULL) {
		for (i = 0; i < count; i++)
			MlnImpl_81081B2C(endpoints[i]);
		MlnFree(MLN_HEAP_SECURE, endpoints);
	}
	MlnXmlListFree(MLN_HEAP_SECURE, *list);
	*list = NULL;
	return ret;
}
