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

#define MLN_NS_ACTION_TOKEN	"urn:marlin:broadband:1-2:nemo:services:action-token"

/* the texts of a list's nodes, as a kind 1 list */
static inline __attribute__((always_inline)) int
MlnConfigListFromTexts(const MlnXmlNodeList *nodes, MlnConfigList *list)
{
	MlnBuffer **buffers;
	MlnBuffer *text = NULL;
	int i, err, ret = 0;

	buffers = MlnAllocate(MLN_HEAP_SECURE, nodes->count * sizeof(*buffers));
	if (buffers == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(buffers, 0, nodes->count * sizeof(*buffers));
	for (i = 0; i < nodes->count; i++) {
		err = MlnXmlGetText(MLN_HEAP_SECURE, nodes->nodes[i], &text);
		if (err != 0) {
			ret = err == 2 ? 2 : 0x1f0001;
			goto out;
		}
		buffers[i] = text;
		text = NULL;
	}
	list->kind = 1;
	list->items = buffers;
	list->count = nodes->count;
	buffers = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_SECURE, text);
	if (buffers != NULL) {
		for (i = 0; i < nodes->count; i++) {
			if (buffers[i] != NULL)
				MlnBufferDestroy(MLN_HEAP_SECURE, buffers[i]);
		}
	}
	MlnFree(MLN_HEAP_SECURE, buffers);
	return ret;
}

/*
 * An action of the token: its id, its Uid elements (at most two) and, but for a generic
 * Action, its BusinessToken. How many Uid and Type elements there are says which of the
 * action's two uids are given.
 */
static inline __attribute__((always_inline)) int
MlnActionParse(MlnXmlNode *node, int type, MlnAction **out)
{
	MlnAction *action;
	MlnXmlNodeList *list = NULL;
	MlnXmlNode *business = NULL;
	MlnBuffer *uid[2] = { NULL, NULL };
	MlnBuffer *businessToken = NULL;
	MlnBuffer *id = NULL;
	int typeCount, uidCount;
	int i, err, ret;

	action = MlnAllocate(MLN_HEAP_SECURE, sizeof(*action));
	if (action == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(action, 0, sizeof(*action));
	ret = MlnImpl_810818F0(node, &id);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81081890(node, "Type", MLN_NS_ACTION_TOKEN, &list);
	if (ret != 0)
		goto out;
	typeCount = list != NULL ? list->count : 0;
	MlnXmlListFree(MLN_HEAP_SECURE, list);
	list = NULL;
	ret = MlnImpl_81081890(node, "Uid", MLN_NS_ACTION_TOKEN, &list);
	if (ret != 0)
		goto out;
	if (list == NULL) {
		uidCount = 0;
	} else {
		uidCount = list->count;
		if (uidCount > 2) {
			ret = 0x1f0001;
			goto out;
		}
	}
	for (i = 0; i < uidCount; i++) {
		if (MlnXmlGetText(MLN_HEAP_SECURE, list->nodes[i], &uid[i]) != 0) {
			ret = 0x1f0001;
			goto out;
		}
	}
	if (type != MLN_ACTION_GENERIC) {
		err = MlnXmlFindNode(MLN_HEAP_SECURE, node, "BusinessToken", MLN_NS_ACTION_TOKEN,
				     &business);
		if (err != 0) {
			ret = err == 2 ? 2 : 0x1f0001;
			goto out;
		}
		err = MlnXmlGetText(MLN_HEAP_SECURE, business, &businessToken);
		if (err != 0) {
			ret = err == 2 ? 2 : 0x1f0001;
			goto out;
		}
		if (type == MLN_ACTION_LICENSE_ACQUISITION) {
			if (uidCount == 0 && typeCount == 1) {
				action->uid[1] = NULL;
			} else if (uidCount == 1 && typeCount == 0) {
				action->uid[1] = uid[0];
				uid[0] = NULL;
			} else {
				ret = 0x1f0001;
				goto out;
			}
		} else if (type == MLN_ACTION_LINK_ACQUISITION || type == MLN_ACTION_DEREGISTRATION) {
			if (uidCount == 1 && typeCount == 1) {
				action->uid[0] = NULL;
				action->uid[1] = uid[0];
				uid[0] = NULL;
			} else if (uidCount == 2 && typeCount == 0) {
				__builtin_memcpy(action->uid, uid, sizeof(uid));
				uid[0] = NULL;
				uid[1] = NULL;
			} else {
				ret = 0x1f0001;
				goto out;
			}
		}
	}
	action->type = type;
	*out = action;
	action->id = id;
	action->businessToken = businessToken;
	id = NULL;
	businessToken = NULL;
	action = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_SECURE, uid[1]);
	MlnBufferDestroy(MLN_HEAP_SECURE, uid[0]);
	MlnBufferDestroy(MLN_HEAP_SECURE, businessToken);
	MlnBufferDestroy(MLN_HEAP_SECURE, id);
	MlnXmlNodeFree(MLN_HEAP_SECURE, business);
	MlnXmlListFree(MLN_HEAP_SECURE, list);
	if (action != NULL) {
		MlnBufferDestroy(MLN_HEAP_SECURE, action->id);
		MlnBufferDestroy(MLN_HEAP_SECURE, action->businessToken);
		MlnBufferDestroy(MLN_HEAP_SECURE, action->uid[0]);
		MlnBufferDestroy(MLN_HEAP_SECURE, action->uid[1]);
		MlnFree(MLN_HEAP_SECURE, action);
	}
	return ret;
}

/*
 * An ActionToken document: its ConfigurationInfo (the service's id and configuration
 * version, its resource locations) and its actions, read into a new token.
 */
int MlnImpl_810342E8(unsigned int size, const void *xml, MlnActionToken **out)
{
	MlnXmlNode *document = NULL;
	MlnXmlNodeList *tokens = NULL;
	MlnXmlNodeList *children = NULL;
	MlnXmlNodeList *locations = NULL;
	MlnBuffer *name = NULL;
	MlnBuffer *version = NULL;
	MlnBuffer *serviceId = NULL;
	MlnBuffer *actionType = NULL;
	MlnActionToken *token = NULL;
	MlnAction **actions = NULL;
	MlnAction *action;
	MlnXmlNode *node;
	unsigned char *local;
	unsigned int length, j;
	int actionCount = 0;
	int type, i, err, ret;

	if (xml == NULL || size == 0 || out == NULL) {
		ret = 4;
		goto out;
	}
	token = MlnAllocate(MLN_HEAP_SECURE, sizeof(*token));
	if (token == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(token, 0, sizeof(*token));
	ret = MlnXmlParse(MLN_HEAP_SECURE, size, xml, &document);
	if (ret != 0) {
		if (ret != 2)
			ret = 0x1f0001;
		goto out;
	}
	err = MlnXmlFindNodes(MLN_HEAP_SECURE, document, "ActionToken", MLN_NS_ACTION_TOKEN,
			      &tokens);
	if (err != 0) {
		ret = err == 0x10010008 ? 0x1f0001 : 7;
		goto out;
	}
	if (tokens->count != 1) {
		ret = 0x1f0001;
		goto out;
	}
	err = MlnXmlFindNodes(MLN_HEAP_SECURE, tokens->nodes[0], "*", MLN_NS_ACTION_TOKEN,
			      &children);
	if (err != 0x10010008 && err != 0) {
		ret = 7;
		goto out;
	}
	if (children->count <= 1) {
		ret = 0x1f0001;
		goto out;
	}
	actions = MlnAllocate(MLN_HEAP_SECURE, (children->count - 1) * sizeof(*actions));
	if (actions == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(actions, 0, (children->count - 1) * sizeof(*actions));
	for (i = 0; i < children->count; i++) {
		node = children->nodes[i];
		if (MlnXmlNodeGetName(MLN_HEAP_SECURE, node, &name) != 0 || name->size == 0) {
			ret = 0x1f0001;
			goto out;
		}
		/* the name without its prefix */
		length = name->size;
		local = name->data;
		for (j = 0; j < length; j++) {
			if (local[j] == ':') {
				length -= j + 1;
				local += j + 1;
				if (length == 0) {
					ret = 0x1f0001;
					goto out;
				}
				break;
			}
		}
		if (length == 17) {
			if (MlnMemcmp(local, "ConfigurationInfo", 17) != 0) {
				ret = 0x1f0001;
				goto out;
			}
			err = MlnXmlGetAttribute(MLN_HEAP_SECURE, node, NULL, "broadbandServiceId",
						 &serviceId);
			if (err != 0) {
				ret = err == 2 ? 2 : 0x1f0001;
				goto out;
			}
			token->broadbandServiceId = serviceId;
			serviceId = NULL;
			err = MlnXmlGetAttribute(MLN_HEAP_SECURE, node, NULL, "configVersion", &version);
			if (err != 0) {
				ret = err == 2 ? 2 : 0x1f0001;
				goto out;
			}
			token->configVersion = version;
			version = NULL;
			err = MlnXmlFindNodes(MLN_HEAP_SECURE, node, "ResourceLocation",
					      MLN_NS_ACTION_TOKEN, &locations);
			if (err != 0) {
				ret = err == 2 ? 2 : 0x1f0001;
				goto out;
			}
			ret = MlnConfigListFromTexts(locations, &token->resourceLocations);
			if (ret != 0)
				goto out;
			MlnXmlListFree(MLN_HEAP_SECURE, locations);
			locations = NULL;
		} else {
			switch (length) {
			case 18:
				if (MlnMemcmp(local, "LicenseAcquisition", 18) != 0) {
					ret = 0x1f0001;
					goto out;
				}
				type = MLN_ACTION_LICENSE_ACQUISITION;
				break;
			case 15:
				if (MlnMemcmp(local, "NodeAcquisition", 15) == 0) {
					type = MLN_ACTION_NODE_ACQUISITION;
				} else if (MlnMemcmp(local, "LinkAcquisition", 15) == 0) {
					type = MLN_ACTION_LINK_ACQUISITION;
				} else {
					ret = 0x1f0001;
					goto out;
				}
				break;
			case 14:
				if (MlnMemcmp(local, "Deregistration", 14) != 0) {
					ret = 0x1f0001;
					goto out;
				}
				type = MLN_ACTION_DEREGISTRATION;
				break;
			default:
				if (length != 6 || MlnMemcmp(local, "Action", 6) != 0) {
					ret = 0x1f0001;
					goto out;
				}
				err = MlnXmlGetAttribute(MLN_HEAP_SECURE, node, NULL, "xsi:type",
							 &actionType);
				if (err != 0) {
					ret = err == 2 ? 2 : 0x1f0001;
					goto out;
				}
				type = MLN_ACTION_GENERIC;
				break;
			}
			action = NULL;
			ret = MlnActionParse(node, type, &action);
			if (ret != 0)
				goto out;
			actions[actionCount++] = action;
		}
		if (name != NULL) {
			MlnBufferDestroy(MLN_HEAP_SECURE, name);
			name = NULL;
		}
	}
	token->actions = actions;
	token->actionCount = actionCount;
	*out = token;
	token = NULL;
	actions = NULL;
	ret = 0;
out:
	MlnBufferDestroy(MLN_HEAP_SECURE, name);
	MlnBufferDestroy(MLN_HEAP_SECURE, version);
	MlnBufferDestroy(MLN_HEAP_SECURE, serviceId);
	MlnBufferDestroy(MLN_HEAP_SECURE, actionType);
	if (actions != NULL) {
		for (i = 0; i < children->count - 1; i++) {
			if (actions[i] != NULL) {
				MlnBufferDestroy(MLN_HEAP_SECURE, actions[i]->id);
				MlnBufferDestroy(MLN_HEAP_SECURE, actions[i]->businessToken);
				MlnBufferDestroy(MLN_HEAP_SECURE, actions[i]->uid[0]);
				MlnBufferDestroy(MLN_HEAP_SECURE, actions[i]->uid[1]);
				MlnFree(MLN_HEAP_SECURE, actions[i]);
			}
		}
		MlnFree(MLN_HEAP_SECURE, actions);
	}
	MlnImpl_81034224(token);
	MlnXmlListFree(MLN_HEAP_SECURE, tokens);
	MlnXmlListFree(MLN_HEAP_SECURE, children);
	MlnXmlListFree(MLN_HEAP_SECURE, locations);
	MlnXmlNodeFree(MLN_HEAP_SECURE, document);
	return ret;
}
