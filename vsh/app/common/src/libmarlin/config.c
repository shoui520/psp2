/*
 * The lists of an action token or a service configuration. The file's name is ours.
 */
#include "mln_internal.h"

/* frees what a list holds, its entries' own lists first; the list itself is the caller's */
int MlnImpl_81033AE8(MlnConfigList *list)
{
	MlnBuffer **buffers;
	MlnBufferPair **pairs;
	MlnConfigListEntry **entries;
	MlnServiceEndpoint **endpoints;
	void *items;
	unsigned int i;

	if (list == NULL)
		return 0;
	items = list->items;
	if (items == NULL)
		return 0;
	switch (list->kind) {
	case 1:
		buffers = list->items;
		for (i = 0; i < list->count; i++) {
			if (buffers[i] != NULL)
				MlnBufferDestroy(MLN_HEAP_SECURE, buffers[i]);
		}
		break;
	case 2:
		pairs = list->items;
		for (i = 0; i < list->count; i++) {
			if (pairs[i]->first != NULL)
				MlnBufferDestroy(MLN_HEAP_SECURE, pairs[i]->first);
			if (pairs[i]->second != NULL)
				MlnBufferDestroy(MLN_HEAP_SECURE, pairs[i]->second);
			if (pairs[i] != NULL)
				MlnFree(MLN_HEAP_SECURE, pairs[i]);
		}
		break;
	case 3:
		pairs = list->items;
		for (i = 0; i < list->count; i++) {
			if (pairs[i]->first != NULL)
				MlnBufferDestroy(MLN_HEAP_SECURE, pairs[i]->first);
			if (pairs[i]->second != NULL)
				MlnBufferDestroy(MLN_HEAP_SECURE, pairs[i]->second);
			if (pairs[i] != NULL)
				MlnFree(MLN_HEAP_SECURE, pairs[i]);
		}
		break;
	case 4:
		entries = list->items;
		for (i = 0; i < list->count; i++) {
			if (entries[i]->key != NULL)
				MlnBufferDestroy(MLN_HEAP_SECURE, entries[i]->key);
			MlnImpl_81033AE8(&entries[i]->values);
			if (entries[i] != NULL)
				MlnFree(MLN_HEAP_SECURE, entries[i]);
		}
		break;
	case 5:
		endpoints = list->items;
		for (i = 0; i < list->count; i++) {
			if (endpoints[i]->id != NULL)
				MlnBufferDestroy(MLN_HEAP_SECURE, endpoints[i]->id);
			if (endpoints[i]->nodeId != NULL)
				MlnBufferDestroy(MLN_HEAP_SECURE, endpoints[i]->nodeId);
			if (endpoints[i]->requestEncryptionKey != NULL)
				MlnBufferDestroy(MLN_HEAP_SECURE, endpoints[i]->requestEncryptionKey);
			if (endpoints[i]->responseSigningKey != NULL)
				MlnBufferDestroy(MLN_HEAP_SECURE, endpoints[i]->responseSigningKey);
			MlnImpl_81033AE8(&endpoints[i]->roles);
			if (endpoints[i]->location != NULL)
				MlnFree(MLN_HEAP_SECURE, endpoints[i]->location);
			if (endpoints[i] != NULL)
				MlnFree(MLN_HEAP_SECURE, endpoints[i]);
		}
		break;
	default:
		return 0;
	}
	MlnFree(MLN_HEAP_SECURE, items);
	return 0;
}

#define MLN_NS_CONFIGURATION	"urn:marlin:broadband:1-2:nemo:services:configuration"
#define MLN_NS_SONY_CONFIGURATION \
	"urn:marlin:organization:sony:nemo:services:schemas:configuration:1-0"

/* a BroadbandServiceConfig document, read into a new configuration */
int MlnImpl_81033D9C(unsigned int size, const void *xml, MlnServiceConfig **out)
{
	MlnXmlNode *document = NULL;
	MlnXmlNodeList *list = NULL;
	MlnXmlNode *root = NULL;
	MlnBuffer *id = NULL;
	MlnBuffer *name = NULL;
	MlnBuffer *version = NULL;
	MlnServiceConfig *config = NULL;
	int ret;

	if (xml == NULL || size == 0 || out == NULL) {
		ret = 4;
		goto out;
	}
	config = MlnAllocate(MLN_HEAP_SECURE, sizeof(*config));
	if (config == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(config, 0, sizeof(*config));
	ret = MlnXmlParse(MLN_HEAP_SECURE, size, xml, &document);
	if (ret != 0) {
		ret = ret == 2 ? 2 : 0x1f0001;
		goto out;
	}
	ret = MlnXmlFindNode(MLN_HEAP_SECURE, document, "BroadbandServiceConfig",
			     MLN_NS_CONFIGURATION, &root);
	if (ret != 0) {
		ret = ret == 2 ? 2 : 0x1f0001;
		goto out;
	}
	if (MlnXmlGetAttribute(MLN_HEAP_SECURE, root, NULL, "broadbandServiceId", &id) != 0) {
		ret = 0x1f0001;
		goto out;
	}
	config->broadbandServiceId = id;
	id = NULL;
	if (MlnXmlGetAttribute(MLN_HEAP_SECURE, root, NULL, "broadbandServiceFriendlyName",
			       &name) != 0) {
		ret = 0x1f0001;
		goto out;
	}
	config->broadbandServiceFriendlyName = name;
	name = NULL;
	if (MlnXmlGetAttribute(MLN_HEAP_SECURE, root, NULL, "configVersion", &version) != 0) {
		ret = 0x1f0001;
		goto out;
	}
	config->configVersion = version;
	version = NULL;
	ret = MlnImpl_81081890(root, "RegistrationServiceConfig", MLN_NS_CONFIGURATION, &list);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81081BB4(&list, &config->registrationService, "registration-service");
	if (ret != 0)
		goto out;
	ret = MlnImpl_81081890(root, "LicenseServiceConfig", MLN_NS_CONFIGURATION, &list);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81081BB4(&list, &config->licenseService, "license-service");
	if (ret != 0)
		goto out;
	ret = MlnImpl_81081890(root, "OctopusPersonalizationServiceConfig",
			       MLN_NS_SONY_CONFIGURATION, &list);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81081BB4(&list, &config->octopusPersonalizationService,
			       "octopus-personalization-service");
	if (ret != 0)
		goto out;
	ret = MlnImpl_81081890(root, "NemoPersonalizationServiceConfig",
			       MLN_NS_SONY_CONFIGURATION, &list);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81081BB4(&list, &config->nemoPersonalizationService,
			       "nemo-personalization-service");
	if (ret != 0)
		goto out;
	ret = MlnImpl_81081890(root, "LtpServiceConfig", MLN_NS_CONFIGURATION, &list);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81081BB4(&list, &config->licenseTransferService, "LicenseTransferService");
	if (ret != 0)
		goto out;
	*out = config;
	config = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_SECURE, id);
	MlnBufferDestroy(MLN_HEAP_SECURE, name);
	MlnBufferDestroy(MLN_HEAP_SECURE, version);
	MlnImpl_810341B8(config);
	MlnXmlNodeFree(MLN_HEAP_SECURE, root);
	MlnXmlListFree(MLN_HEAP_SECURE, list);
	MlnXmlNodeFree(MLN_HEAP_SECURE, document);
	return ret;
}

int MlnImpl_810341B8(MlnServiceConfig *config)
{
	if (config == NULL)
		return 0;
	MlnBufferDestroy(MLN_HEAP_SECURE, config->broadbandServiceId);
	MlnBufferDestroy(MLN_HEAP_SECURE, config->broadbandServiceFriendlyName);
	MlnBufferDestroy(MLN_HEAP_SECURE, config->configVersion);
	MlnImpl_81033AE8(&config->registrationService);
	MlnImpl_81033AE8(&config->licenseService);
	MlnImpl_81033AE8(&config->octopusPersonalizationService);
	MlnImpl_81033AE8(&config->nemoPersonalizationService);
	MlnImpl_81033AE8(&config->licenseTransferService);
	MlnFree(MLN_HEAP_SECURE, config);
	return 0;
}

int MlnImpl_81034224(MlnActionToken *token)
{
	MlnAction *action;
	int i;

	if (token == NULL)
		return 0;
	MlnImpl_81033AE8(&token->resourceLocations);
	if (token->broadbandServiceId != NULL)
		MlnBufferDestroy(MLN_HEAP_SECURE, token->broadbandServiceId);
	if (token->configVersion != NULL)
		MlnBufferDestroy(MLN_HEAP_SECURE, token->configVersion);
	for (i = 0; i < token->actionCount; i++) {
		action = token->actions[i];
		if (action != NULL) {
			MlnBufferDestroy(MLN_HEAP_SECURE, action->id);
			MlnBufferDestroy(MLN_HEAP_SECURE, action->businessToken);
			MlnBufferDestroy(MLN_HEAP_SECURE, action->uid[0]);
			MlnBufferDestroy(MLN_HEAP_SECURE, action->uid[1]);
			MlnFree(MLN_HEAP_SECURE, action);
		}
	}
	MlnFree(MLN_HEAP_SECURE, token->actions);
	MlnFree(MLN_HEAP_SECURE, token);
	return 0;
}
