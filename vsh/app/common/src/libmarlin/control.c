/*
 * The core context: the node, the bundle and its content keys, the link graph and the key
 * store a control program runs on, and the control VM with the actions it has initialized
 * (0x81061F04-). The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_CORE_ERROR_VM		0x23020001
#define MLN_CORE_ERROR_NODE		0x23020002
#define MLN_CORE_ERROR_LINKS		0x23020003
#define MLN_CORE_ERROR_BUNDLE		0x23020004
#define MLN_CORE_ERROR_REFUSED		0x23020006	/* the action is not allowed */
#define MLN_CORE_ERROR_NO_KEYS		0x23020007	/* no key store */
#define MLN_CORE_ERROR_KEYS		0x23020008
#define MLN_CORE_ERROR_DESCRIBE		0x2302000A	/* the Describe routine failed */
#define MLN_CORE_ERROR_CHECK		0x2302000B	/* the Check routine failed */
#define MLN_CORE_ERROR_NO_DESCRIBE	0x2302000C	/* the action has no Describe routine */
#define MLN_CORE_ERROR_ASSERTION	0x2302000E
#define MLN_CORE_ERROR_INIT		0x2302000F
#define MLN_CORE_ERROR_INIT_STATUS	0x23020010
#define MLN_CORE_ERROR_RELEASE_STATUS	0x23020011	/* the Release routine failed */

#define MLN_KEY_STORE_ERROR_FOUND	0x23040001	/* the key was there already */
#define MLN_VM_ERROR_NO_ROUTINE		0x23090004	/* the program has no such routine */
#define MLN_STORE_ERROR_NO_ASSERTION	0x230A000B	/* the store holds no such assertion */

/* the actions whose routines were initialized, released with the control */
#define MLN_CORE_ACTION_PLAY		0x1
#define MLN_CORE_ACTION_WRITE_TO_MEDIUM	0x2
#define MLN_CORE_ACTION_TRANSFER	0x4
#define MLN_CORE_ACTION_EXPORT		0x8

/* the routines of an action (MLN_CORE_ACTION_* is 1 << its index) and of the control */
typedef struct MlnCoreRoutines {
	const char *init;
	const char *release;
	const char *check;
	const char *perform;
	const char *describe;
} MlnCoreRoutines;

#define MLN_CORE_CONTROL	4	/* the index of the control's own routines */

/* what the control VM is given of its host */
typedef struct MlnCoreHost {
	void *graph;
	void *functions[7];
} MlnCoreHost;

struct MlnCoreContext {
	MlnOctopusNode *node;
	MlnOctopusBundle *bundle;
	int contentKeysLoaded;		/* a bit per content key of the bundle */
	void *graph;			/* the links and nodes (MlnImpl_8106EC8C) */
	void *keys;			/* the key store (MlnImpl_810710F8) */
	MlnCoreHost host;
	void *vm;			/* the control VM (MlnImpl_8106C54C) */
	int actions;			/* MLN_CORE_ACTION_* */
};

/* the parts of an agent's routine name, kept beside the routines */
static const char s_corePrefix[] = "Control.Agents.";
static const char s_coreSeparator[] = ".";

/* an agent's routine name, "Control.Agents.<agent>.<action>" */
static inline __attribute__((always_inline)) int MlnCoreRoutineName(char *out, unsigned int size,
							      const char *agent, const char *action)
{
	unsigned int length;

	if (out == NULL || agent == NULL || action == NULL)
		return 3;
	if (MlnStrlen(agent) > 0xFFFFFFED)
		return 5;
	if (MlnStrlen(agent) + 18 > ~MlnStrlen(action))
		return 5;
	if (size < MlnStrlen(s_corePrefix) + MlnStrlen(s_coreSeparator) + 1 + MlnStrlen(agent) +
		   MlnStrlen(action))
		return 5;
	MlnStrncpy(out, s_corePrefix, size);
	MlnStrncat(out, agent, size - 15);
	length = MlnStrlen(agent);
	MlnStrncat(out, s_coreSeparator, size - 15 - length);
	MlnStrncat(out, action, size - 16 - length);
	return 0;
}

int MlnImpl_81061F04(char *out, unsigned int size, const char *agent, const char *action)
{
	return MlnCoreRoutineName(out, size, agent, action);
}

static const MlnCoreRoutines s_coreRoutines[] = {
	{ "Control.Actions.Play.Init", "Control.Actions.Play.Release",
	  "Control.Actions.Play.Check", "Control.Actions.Play.Perform",
	  "Control.Actions.Play.Describe" },
	{ "Control.Actions.WriteToMedium.Init", "Control.Actions.WriteToMedium.Release",
	  "Control.Actions.WriteToMedium.Check", "Control.Actions.WriteToMedium.Perform",
	  "Control.Actions.WriteToMedium.Describe" },
	{ "Control.Actions.Transfer.Init", "Control.Actions.Transfer.Release",
	  "Control.Actions.Transfer.Check", "Control.Actions.Transfer.Perform",
	  "Control.Actions.Transfer.Describe" },
	{ "Control.Actions.Export.Init", "Control.Actions.Export.Release",
	  "Control.Actions.Export.Check", "Control.Actions.Export.Perform",
	  "Control.Actions.Export.Describe" },
	{ "Control.Init", "Control.Release", NULL, NULL, NULL },
};

/* make a context */
unsigned int MlnImpl_81062024(MlnCoreContext **context)
{
	MlnCoreContext *new;

	new = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*new));
	if (new == NULL)
		return 2;
	MlnMemset(new, 0, sizeof(*new));
	*context = new;
	return 0;
}

/* release the control: the initialized actions' routines and the VM, and the graph */
static inline void MlnCoreRelease(MlnCoreContext *context)
{
	if (context->vm != NULL) {
		if (context->actions & MLN_CORE_ACTION_PLAY)
			MlnImpl_8106C794(context->vm, "Control.Actions.Play.Release");
		if (context->actions & MLN_CORE_ACTION_WRITE_TO_MEDIUM)
			MlnImpl_8106C794(context->vm, "Control.Actions.WriteToMedium.Release");
		if (context->actions & MLN_CORE_ACTION_TRANSFER)
			MlnImpl_8106C794(context->vm, "Control.Actions.Transfer.Release");
		if (context->actions & MLN_CORE_ACTION_EXPORT)
			MlnImpl_8106C794(context->vm, "Control.Actions.Export.Release");
		MlnImpl_8106C794(context->vm, "Control.Release");
		MlnImpl_8106C6C4(context->vm);
		context->vm = NULL;
	}
	if (context->graph != NULL) {
		MlnImpl_8106ED54(context->graph);
		context->graph = NULL;
	}
	context->bundle = NULL;
}

/* free a context */
void MlnImpl_81062060(MlnCoreContext *context)
{
	if (context == NULL)
		return;
	MlnCoreRelease(context);
	if (context->keys != NULL)
		MlnImpl_81071278(context->keys);
	MlnFree(MLN_HEAP_GENERAL, context);
}

/* the context's node: the key store made anew for another node, the node added to the graph */
unsigned int MlnImpl_81062164(MlnCoreContext *context, MlnOctopusNode *node)
{
	unsigned int ret;

	if (context == NULL)
		return 4;
	if (node == NULL) {
		ret = MLN_CORE_ERROR_NODE;
		goto fail;
	}
	if (context->node != NULL) {
		MlnImpl_81071278(context->keys);
		context->keys = NULL;
	} else if (context->keys != NULL) {
		goto keys;
	}
	ret = MlnImpl_810710F8(&context->keys);
	if (ret != 0)
		goto fail;
keys:
	if (context->graph == NULL) {
		ret = MlnImpl_8106EC8C(&context->graph);
		if (ret != 0)
			goto fail;
	}
	ret = MlnImpl_8106EE80(context->graph, node);
	if (ret != 0)
		goto fail;
	context->node = node;
	return 0;
fail:
	if (context->keys != NULL) {
		MlnImpl_81071278(context->keys);
		context->keys = NULL;
	}
	return ret;
}

/* a node's ScubaKeys extension among its extensions, or NULL */
static inline const MlnOctopusExtension *MlnCoreScubaKeys(const MlnOctopusArray *extensions)
{
	const MlnOctopusExtension *extension;
	int i;

	for (i = 0; i < extensions->count; i++) {
		extension = extensions->items[i];
		if (extension == NULL)
			return NULL;
		if (extension->type == MLN_OCTOPUS_EXTENSION_SCUBA)
			return extension;
	}
	return NULL;
}

/* add links to the graph, and the private and public keys their nodes' ScubaKeys carry */
unsigned int MlnImpl_81062224(MlnCoreContext *context, MlnOctopusArray *links)
{
	const MlnOctopusExtension *extension;
	const MlnOctopusScubaKeys *scuba;
	const MlnOctopusArray *extensions;
	const MlnOctopusArray *keys;
	MlnOctopusLink *link;
	unsigned int ret;
	int count;
	int n;
	int i;
	int j;

	if (context == NULL)
		return 4;
	if (links == NULL)
		return MLN_CORE_ERROR_LINKS;
	if (context->keys == NULL) {
		ret = MlnImpl_810710F8(&context->keys);
		if (ret != 0)
			return ret;
	}
	if (context->graph == NULL) {
		ret = MlnImpl_8106EC8C(&context->graph);
		if (ret != 0)
			return ret;
	}
	count = links->count;
	for (i = 0; i < count; i++) {
		link = links->items[i];
		extensions = link->extensions;
		if (extensions == NULL)
			continue;
		extension = MlnCoreScubaKeys(extensions);
		if (extension == NULL)
			return 5;
		scuba = extension->scubaKeys;
		if (scuba == NULL)
			return 5;
		keys = scuba->secretKeys;
		if (keys != NULL) {
			n = keys->count;
			for (j = 0; j < n; j++) {
				ret = MlnImpl_810712F0(context->keys, keys->items[j]);
				if (ret != MLN_KEY_STORE_ERROR_FOUND && ret != 0)
					return ret;
			}
		}
		keys = scuba->privateKeys;
		if (keys != NULL) {
			n = keys->count;
			for (j = 0; j < n; j++) {
				ret = MlnImpl_8107132C(context->keys, keys->items[j]);
				if (ret != MLN_KEY_STORE_ERROR_FOUND && ret != 0)
					return ret;
			}
		}
	}
	return MlnImpl_8106EEBC(context->graph, links);
}

/* add a SAML assertion to the graph */
unsigned int MlnImpl_810623F0(MlnCoreContext *context, MlnOctopusSamlAssertion *assertion)
{
	unsigned int ret;

	if (context == NULL)
		return 4;
	if (assertion == NULL)
		return MLN_CORE_ERROR_ASSERTION;
	if (context->graph == NULL) {
		ret = MlnImpl_8106EC8C(&context->graph);
		if (ret != 0)
			return ret;
	}
	return MlnImpl_8106F264(context->graph, assertion);
}

/* give the graph the store's pending key pair */
unsigned int MlnImpl_81062450(MlnCoreContext *context, MlnArray *pair)
{
	unsigned int ret;

	if (context == NULL)
		return 4;
	if (context->graph == NULL) {
		ret = MlnImpl_8106EC8C(&context->graph);
		if (ret != 0)
			return ret;
	}
	return MlnImpl_810706F4(context->graph, pair);
}

/* the graph's pending key pair */
unsigned int MlnImpl_8106249C(MlnCoreContext *context, MlnArray **pair)
{
	MlnArray *pending = NULL;
	unsigned int ret;

	if (context == NULL) {
		ret = 4;
		goto out;
	}
	if (context->graph == NULL) {
		ret = MlnImpl_8106EC8C(&context->graph);
		if (ret != 0)
			goto out;
	}
	ret = MlnImpl_81070808(context->graph, &pending);
	if (ret != 0)
		goto out;
	*pair = pending;
	pending = NULL;
out:
	MlnArrayDestroy(MLN_HEAP_GENERAL, pending);
	return ret;
}

/*
 * Take a bundle: its links go into the graph, and its control's code module is loaded into
 * a new control VM, which runs Control.Init when the program has it.
 */
unsigned int MlnImpl_81062524(MlnCoreContext *context, MlnOctopusBundle *bundle)
{
	const MlnOctopusControl *control;
	unsigned int status = 0;
	unsigned int ret;

	if (context == NULL)
		return MLN_CORE_ERROR_INVALID;
	if (bundle == NULL || bundle->control == NULL || bundle->controller == NULL ||
	    bundle->contentKeys == NULL || bundle->protectors == NULL)
		return MLN_CORE_ERROR_BUNDLE;
	context->bundle = bundle;
	if (context->graph == NULL) {
		ret = MlnImpl_8106EC8C(&context->graph);
		if (ret != 0)
			goto fail;
	}
	ret = MlnImpl_8106EEA8(context->graph, bundle);
	if (ret != 0)
		goto fail;

	context->host.graph = context->graph;
	context->host.functions[0] = (void *)MlnImpl_8106F504;
	context->host.functions[1] = (void *)MlnImpl_81070318;
	context->host.functions[2] = (void *)MlnImpl_8106F314;
	context->host.functions[3] = (void *)MlnImpl_8106F388;
	context->host.functions[4] = (void *)MlnImpl_8106F3F4;
	context->host.functions[5] = (void *)MlnImpl_8106F438;
	context->host.functions[6] = (void *)MlnImpl_81070424;
	ret = MlnImpl_8106C54C(0x8000, &context->host, &context->vm);
	if (ret != 0)
		goto fail;
	control = context->bundle->control;
	ret = MlnImpl_8106C6F8(context->vm, control->codeModule, control->uid, control->protocol,
			       control->type, control->signer, control->attributes);
	if (ret != 0)
		goto fail;
	ret = MlnImpl_8106C788(context->vm, context->bundle->control->uid);
	if (ret != 0)
		goto fail;

	ret = MlnImpl_8106C794(context->vm, "Control.Init");
	if (ret == MLN_VM_ERROR_NO_ROUTINE)
		return 0;
	if (ret != 0)
		return MLN_CORE_ERROR_INIT;
	ret = MlnImpl_8106C970(context->vm, &status);
	if (ret == 0)
		return status != 0 ? MLN_CORE_ERROR_INIT_STATUS : 0;
fail:
	switch (ret & 0xFFFF0000) {
	case 0x23040000:
		return MLN_CORE_ERROR_KEYS;
	case 0x23090000:
		return MLN_CORE_ERROR_VM;
	}
	return ret;
}

/*
 * The content key that protects an id: the protector whose targets hold it, its content
 * key, which the controller must control; the key is added to the key store.
 */
unsigned int MlnImpl_8106273C(MlnCoreContext *context, MlnBuffer *buffer,
			      MlnOctopusContentKey **contentKey)
{
	const MlnOctopusBundle *bundle;
	const MlnOctopusProtector *protector;
	const MlnOctopusArray *targets;
	MlnOctopusContentKey *key;
	const MlnOctopusReference *target;
	unsigned int ret;
	int count;
	int n;
	int i;
	int j;

	if (context == NULL)
		return 4;
	bundle = context->bundle;
	if (bundle == NULL)
		return 4;
	if (context->keys == NULL) {
		ret = MlnImpl_810710F8(&context->keys);
		if (ret != 0)
			return ret;
	}
	count = bundle->contentKeyCount;
	for (i = 0; i < count; i++) {
		protector = bundle->protectors->items[i];
		n = protector->protectedTargets->count;
		for (j = 0; j < n; j++) {
			target = protector->protectedTargets->items[j];
			if (MlnBufferCompare(MLN_HEAP_GENERAL, target->uid, buffer) == 0)
				goto protected;
		}
	}
	return 3;

protected:
	for (i = 0; i < count; i++) {
		key = bundle->contentKeys->items[i];
		if (MlnBufferCompare(MLN_HEAP_GENERAL, key->uid, protector->contentKey->uid) == 0)
			break;
	}
	if (i >= count)
		return 3;

	targets = bundle->controller->controlledTargets;
	n = targets->count;
	if (n > 0) {
		for (j = 0;; j++) {
			if (j == n)
				return 3;
			target = bundle->controller->controlledTargets->items[j];
			if (MlnBufferCompare(MLN_HEAP_GENERAL, key->uid, target->uid) == 0)
				break;
		}
	} else if (n == 0) {
		return 3;
	}
	ret = MlnImpl_810712F0(context->keys, key->secretKey);
	if (ret == MLN_KEY_STORE_ERROR_FOUND || ret == 0) {
		context->contentKeysLoaded |= 1 << i;
		*contentKey = key;
		return 0;
	}
	return ret;
}

/* release the context's control and graph, keeping its node and key store */
void MlnImpl_810628E4(MlnCoreContext *context)
{
	if (context == NULL)
		return;
	MlnCoreRelease(context);
}

/*
 * Check whether an action is allowed with these parameters: the action's routines are
 * initialized once, then its Check routine runs; its result, when the action is
 * refused or allowed, is returned in out.
 */
unsigned int MlnImpl_810629C8(MlnCoreContext *context, int kind,
			      MlnPlanktonParameterBlock *parameters, MlnBuffer **out)
{
	unsigned int status;
	unsigned int value;
	MlnBuffer *result = NULL;
	unsigned int type;
	unsigned int ret;

	if (context == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto fail;
	}
	if ((unsigned int)kind > MLN_CORE_CONTROL) {
		ret = 3;
		goto fail;
	}
	if (parameters != NULL) {
		ret = MlnImpl_8106EE94(context->graph, parameters);
		if (ret != 0)
			goto out;
	}
	if (!((context->actions >> kind) & 1)) {
		ret = MlnImpl_8106C794(context->vm, s_coreRoutines[kind].init);
		if (ret != MLN_VM_ERROR_NO_ROUTINE) {
			if (ret != 0) {
				ret = MLN_CORE_ERROR_INIT;
				goto out;
			}
			ret = MlnImpl_8106C970(context->vm, &status);
			if (ret != 0)
				goto out;
			if (status != 0) {
				ret = MLN_CORE_ERROR_INIT_STATUS;
				goto out;
			}
		}
		context->actions |= 1 << kind;
	}

	ret = MlnImpl_8106C794(context->vm, s_coreRoutines[kind].check);
	if (ret == MLN_VM_ERROR_NO_ROUTINE) {
		ret = MLN_CORE_ERROR_REFUSED;
		goto out;
	}
	if (ret != 0)
		goto out;
	ret = MlnImpl_8106C970(context->vm, &status);
	if (ret != 0)
		goto out;
	if (status != 0) {
		ret = MLN_CORE_ERROR_CHECK;
		goto out;
	}
	ret = MlnImpl_8106C970(context->vm, &value);
	if (ret != 0)
		goto out;
	if (value == 0) {
		ret = MLN_CORE_ERROR_CHECK;
		goto out;
	}
	ret = MlnImpl_8106C9F4(context->vm, value, &result, &type);
	if (ret == 0 && type == 1)
		ret = MLN_CORE_ERROR_REFUSED;
out:
	if (result != NULL) {
		if ((ret == 0 || ret == MLN_CORE_ERROR_REFUSED) && out != NULL)
			*out = result;
		else
			MlnBufferDestroy(MLN_HEAP_GENERAL, result);
	}
	if (ret == 0)
		return 0;
fail:
	switch (ret & 0xFFFF0000) {
	case 0x23040000:
		return MLN_CORE_ERROR_KEYS;
	case 0x23090000:
		return MLN_CORE_ERROR_VM;
	}
	return ret;
}

/*
 * Perform an action with these parameters: the action's routines are initialized once,
 * then its Perform routine runs; when it allows the action, each content key it loaded
 * is opened with the broadcast key block and returned in contentKeys, by its index in
 * the bundle, and its result in rights.
 */
unsigned int MlnImpl_81062BE8(MlnCoreContext *context, int kind, MlnPlanktonParameterBlock *parameters,
			      MlnOctopusContentKey **contentKeys, MlnBuffer **rights)
{
	MlnOctopusArray *keys;
	MlnOctopusContentKey *key;
	unsigned int status;
	unsigned int value;
	MlnBuffer *result = NULL;
	unsigned int type;
	unsigned int ret;
	int count;
	int i;

	if (context == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto fail;
	}
	if (context->keys == NULL) {
		ret = MLN_CORE_ERROR_NO_KEYS;
		goto fail;
	}
	if (context->graph == NULL) {
		ret = MlnImpl_8106EC8C(&context->graph);
		if (ret != 0)
			goto out;
	}
	if (contentKeys == NULL || rights == NULL || (unsigned int)kind > MLN_CORE_CONTROL) {
		ret = 3;
		goto out;
	}
	if (parameters != NULL) {
		ret = MlnImpl_8106EE94(context->graph, parameters);
		if (ret != 0)
			goto out;
	}
	if (!((context->actions >> kind) & 1)) {
		ret = MlnImpl_8106C794(context->vm, s_coreRoutines[kind].init);
		if (ret != MLN_VM_ERROR_NO_ROUTINE) {
			if (ret != 0) {
				ret = MLN_CORE_ERROR_INIT;
				goto out;
			}
			ret = MlnImpl_8106C970(context->vm, &status);
			if (ret != 0)
				goto out;
			if (status != 0) {
				ret = MLN_CORE_ERROR_INIT_STATUS;
				goto out;
			}
		}
		context->actions |= 1 << kind;
	}

	ret = MlnImpl_8106C794(context->vm, s_coreRoutines[kind].perform);
	if (ret != 0) {
		if (ret == MLN_VM_ERROR_NO_ROUTINE)
			ret = MLN_CORE_ERROR_REFUSED;
		goto out;
	}
	ret = MlnImpl_8106C970(context->vm, &status);
	if (ret != 0)
		goto out;
	if (status != 0) {
		ret = MLN_CORE_ERROR_CHECK;
		goto out;
	}
	ret = MlnImpl_8106C970(context->vm, &value);
	if (ret != 0)
		goto out;
	if (value == 0) {
		ret = MLN_CORE_ERROR_CHECK;
		goto out;
	}
	ret = MlnImpl_8106C9F4(context->vm, value, &result, &type);
	if (ret != 0)
		goto out;
	if (type == 1) {
		ret = MLN_CORE_ERROR_REFUSED;
		goto out;
	}

	keys = context->bundle->contentKeys;
	count = keys->count;
	for (i = 0; i < count; i++) {
		if (!((context->contentKeysLoaded >> i) & 1))
			continue;
		key = keys->items[i];
		ret = MlnImpl_8107148C(context->keys, key->secretKey->uid,
				       context->bundle->broadcastKeyBlock);
		if (ret != 0)
			break;
		contentKeys[i] = key;
	}
out:
	if (result != NULL) {
		if ((ret == 0 || ret == MLN_CORE_ERROR_REFUSED) && rights != NULL)
			*rights = result;
		else
			MlnBufferDestroy(MLN_HEAP_GENERAL, result);
	}
	if (ret == 0)
		return 0;
fail:
	switch (ret & 0xFFFF0000) {
	case 0x23040000:
		return MLN_CORE_ERROR_KEYS;
	case 0x23090000:
		return MLN_CORE_ERROR_VM;
	}
	return ret;
}

/* take the content keys an action opened out of the key store again */
unsigned int MlnImpl_81062EDC(MlnCoreContext *context, int count,
			      MlnOctopusContentKey **contentKeys)
{
	void *keys = context->keys;
	int i;

	for (i = 0; i < count; i++) {
		if (contentKeys[i] != NULL)
			MlnImpl_81071368(keys, contentKeys[i]->secretKey);
	}
	return 0;
}

/*
 * Describe an action with these parameters: the action's routines are initialized once,
 * then its Describe routine runs; its result is returned in out.
 */
unsigned int MlnImpl_81062F20(MlnCoreContext *context, int kind,
			      MlnPlanktonParameterBlock *parameters, MlnBuffer **out)
{
	unsigned int status;
	unsigned int value;
	MlnBuffer *result = NULL;
	unsigned int type;
	unsigned int ret;

	if (context == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto done;
	}
	if ((unsigned int)kind > MLN_CORE_CONTROL) {
		ret = 3;
		goto done;
	}
	if (parameters != NULL) {
		ret = MlnImpl_8106EE94(context->graph, parameters);
		if (ret != 0)
			goto fail;
	}
	if (!((context->actions >> kind) & 1)) {
		ret = MlnImpl_8106C794(context->vm, s_coreRoutines[kind].init);
		if (ret != MLN_VM_ERROR_NO_ROUTINE) {
			if (ret != 0) {
				ret = MLN_CORE_ERROR_INIT;
				goto done;
			}
			ret = MlnImpl_8106C970(context->vm, &status);
			if (ret != 0)
				goto fail;
			if (status != 0) {
				ret = MLN_CORE_ERROR_INIT_STATUS;
				goto done;
			}
		}
		context->actions |= 1 << kind;
	}

	ret = MlnImpl_8106C794(context->vm, s_coreRoutines[kind].describe);
	if (ret != 0) {
		if (ret == MLN_VM_ERROR_NO_ROUTINE) {
			ret = MLN_CORE_ERROR_NO_DESCRIBE;
			goto done;
		}
		goto fail;
	}
	ret = MlnImpl_8106C970(context->vm, &status);
	if (ret != 0)
		goto fail;
	if (status != 0) {
		ret = MLN_CORE_ERROR_DESCRIBE;
		goto done;
	}
	ret = MlnImpl_8106C970(context->vm, &value);
	if (ret != 0)
		goto fail;
	if (value != 0) {
		ret = MlnImpl_8106C9F4(context->vm, value, &result, &type);
		if (ret != 0)
			goto fail;
	}
	*out = result;
	result = NULL;
	MlnBufferDestroy(MLN_HEAP_GENERAL, result);
	return 0;
done:
	MlnBufferDestroy(MLN_HEAP_GENERAL, result);
	return ret;

fail:
	MlnBufferDestroy(MLN_HEAP_GENERAL, result);
	switch (ret & 0xFFFF0000) {
	case 0x23040000:
		return MLN_CORE_ERROR_KEYS;
	case 0x23090000:
		return MLN_CORE_ERROR_VM;
	}
	return ret;
}

/*
 * Describe a link's constraint: its control runs in a control VM of its own, Init,
 * Describe and Release; Describe's result is returned in out.
 */
unsigned int MlnImpl_8106314C(MlnCoreContext *context, MlnOctopusControl *control,
			      MlnBuffer **out)
{
	void *vm = NULL;
	unsigned int status = 0;
	unsigned int value = 0;
	unsigned int released;
	unsigned int type;
	MlnCoreHost host;
	unsigned int ret;
	unsigned int r;

	if (context == NULL || context->graph == NULL)
		return MLN_CORE_ERROR_INVALID;
	if (out == NULL)
		return 3;
	if (control == NULL)
		return MLN_CORE_ERROR_NO_DESCRIBE;

	host.graph = context->graph;
	host.functions[0] = (void *)MlnImpl_8106F504;
	host.functions[1] = (void *)MlnImpl_81070318;
	host.functions[2] = (void *)MlnImpl_8106F314;
	host.functions[3] = (void *)MlnImpl_8106F388;
	host.functions[4] = (void *)MlnImpl_8106F3F4;
	host.functions[5] = (void *)MlnImpl_8106F438;
	host.functions[6] = (void *)MlnImpl_81070424;
	ret = MlnImpl_8106C54C(0x8000, &host, &vm);
	if (ret != 0)
		goto destroy;
	ret = MlnImpl_8106C6F8(vm, control->codeModule, control->uid, control->protocol,
			       control->type, control->signer, control->attributes);
	if (ret != 0)
		goto destroy;
	ret = MlnImpl_8106C788(vm, control->uid);
	if (ret != 0)
		goto destroy;

	r = MlnImpl_8106C794(vm, "Control.Link.Constraint.Init");
	if (r != MLN_VM_ERROR_NO_ROUTINE) {
		if (r != 0) {
			ret = MLN_CORE_ERROR_INIT;
			goto destroy;
		}
		ret = MlnImpl_8106C970(vm, &status);
		if (ret != 0)
			goto destroy;
		if (status != 0) {
			ret = MLN_CORE_ERROR_INIT_STATUS;
			goto destroy;
		}
	}

	r = MlnImpl_8106C794(vm, "Control.Link.Constraint.Describe");
	if (r != MLN_VM_ERROR_NO_ROUTINE) {
		if (r != 0) {
			ret = MLN_CORE_ERROR_INIT;
		} else {
			ret = MlnImpl_8106C970(vm, &status);
			if (ret == 0) {
				ret = MlnImpl_8106C970(vm, &value);
				if (ret == 0) {
					if (status != 0)
						ret = MLN_CORE_ERROR_DESCRIBE;
					else if (value != 0)
						ret = MlnImpl_8106C9F4(vm, value, out, &type);
					else
						ret = MLN_CORE_ERROR_NO_DESCRIBE;
				}
			}
		}
	}

	if (vm == NULL)
		return ret;
	released = 0;
	r = MlnImpl_8106C794(vm, "Control.Link.Constraint.Release");
	if (r == MLN_VM_ERROR_NO_ROUTINE) {
		r = 0;
	} else if (r != 0) {
		r = MLN_CORE_ERROR_INIT;
	} else {
		r = MlnImpl_8106C970(vm, &released);
		if (r == 0 && released != 0)
			r = MLN_CORE_ERROR_RELEASE_STATUS;
	}
	if (ret == 0)
		ret = r;
	MlnImpl_8106C6C4(vm);
	return ret;
destroy:
	if (vm != NULL)
		MlnImpl_8106C6C4(vm);
	return ret;
}

/* the stored assertions of kinds 0 and 3 added to a graph */
static inline unsigned int MlnCoreAddAssertions(void *graph)
{
	MlnOctopusSamlAssertion *assertion = NULL;
	unsigned int ret;

	ret = MlnImpl_81044DEC(0, &assertion);
	if (ret == 0) {
		ret = MlnImpl_8106F264(graph, assertion);
		if (ret != 0)
			return ret;
	} else if (ret != MLN_STORE_ERROR_NO_ASSERTION) {
		return ret;
	}
	ret = MlnImpl_81044DEC(3, &assertion);
	if (ret == 0)
		return MlnImpl_8106F264(graph, assertion);
	if (ret != MLN_STORE_ERROR_NO_ASSERTION)
		return ret;
	return 0;
}

/* the stored node and links added to a graph */
static inline unsigned int MlnCoreAddStore(void *graph)
{
	MlnOctopusNode *node = NULL;
	MlnBuffer *record = NULL;
	MlnOctopusLink *link = NULL;
	MlnStoreLink *links = NULL;
	MlnOctopusArray *array = NULL;
	unsigned int ret;
	int count;
	int i;

	ret = MlnImpl_81043B94(&node);
	if (ret != 0) {
		if (ret == 2)
			ret = 2;
		else if (ret == 6)
			ret = 6;
		else
			ret = 5;
		goto out;
	}
	ret = MlnImpl_8106EE80(graph, node);
	if (ret != 0) {
		ret = ret == 2 ? 2 : 5;
		goto out;
	}
	node = NULL;
	ret = MlnImpl_8104411C(&count, &links);
	if (ret != 0) {
		if (ret == 2)
			ret = 2;
		else if (ret != 6)
			ret = 5;
		goto out;
	}
	if (count <= 0)
		goto out;
	array = MlnImpl_8103B758(count);
	if (array == NULL) {
		ret = 2;
		goto out;
	}
	for (i = 0; i < count; i++) {
		ret = MlnImpl_810442D4(&links, &record);
		if (ret != 0) {
			if (ret == 2)
				ret = 2;
			else if (ret == 6)
				ret = 6;
			else
				ret = 5;
			goto out;
		}
		ret = MlnImpl_810422E8(record, &link);
		if (ret != 0) {
			if (ret == 2) {
				ret = 2;
			} else {
				ret = 5;
				MlnImpl_81043F1C(record);
			}
			goto out;
		}
		MlnBufferDestroy(MLN_HEAP_GENERAL, record);
		record = NULL;
		array->items[i] = link;
		link = NULL;
	}
	if (MlnImpl_8106EEBC(graph, array) != 0) {
		ret = 5;
		goto out;
	}
	array = NULL;
out:
	MlnImpl_8104435C(links);
	if (node != NULL)
		MlnImpl_8103B8F8(node);
	MlnBufferDestroy(MLN_HEAP_GENERAL, record);
	if (array != NULL)
		MlnImpl_8103C57C(array, MLN_OCTOPUS_LINK);
	return ret;
}

/*
 * Run an agent: the controls' code modules are loaded into a control VM over a graph of
 * the stored node, links and assertions, and the agent's Init, Run and Release routines
 * run. Run's status, the data it returns and the pair the graph holds are returned.
 */
unsigned int MlnImpl_8106340C(int count, MlnOctopusControl **controls, const MlnBuffer *agent,
			      void *value, MlnPlanktonParameterBlock *parameters, MlnArray *pair,
			      unsigned int *status, MlnBuffer **data, MlnArray **result)
{
	char *name = NULL;
	void *vm = NULL;
	unsigned int initStatus = 0;
	unsigned int runStatus = 0;
	unsigned int runData = 0;	/* an address in the VM */
	unsigned int runSize = 0;
	MlnArray *resultPair = NULL;
	void *graph = NULL;
	char *routine = NULL;
	MlnBuffer *buffer = NULL;
	unsigned int released;
	MlnOctopusControl *control;
	MlnCoreHost host;
	unsigned int size;
	unsigned int ret;
	unsigned int r;
	int release;
	int i;

	if (count == 0 || controls == NULL || agent == NULL || status == NULL || data == NULL ||
	    result == NULL) {
		ret = 3;
		goto out;
	}

	ret = MlnImpl_8106EC8C(&graph);
	if (ret == 0)
		ret = MlnCoreAddAssertions(graph);
	if (ret != 0) {
		MlnImpl_8106F2C8(graph);
		MlnImpl_8106ED54(graph);
		graph = NULL;
		goto out;
	}
	ret = MlnImpl_8106F300(graph, value);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8106EE94(graph, parameters);
	if (ret != 0)
		goto out;
	ret = MlnImpl_810706F4(graph, pair);
	if (ret != 0)
		goto out;
	ret = MlnCoreAddStore(graph);
	if (ret != 0)
		goto out;

	host.graph = graph;
	host.functions[0] = (void *)MlnImpl_8106F504;
	host.functions[1] = (void *)MlnImpl_81070318;
	host.functions[2] = (void *)MlnImpl_8106F314;
	host.functions[3] = (void *)MlnImpl_8106F388;
	host.functions[4] = (void *)MlnImpl_8106F3F4;
	host.functions[5] = (void *)MlnImpl_8106F438;
	host.functions[6] = (void *)MlnImpl_81070424;
	ret = MlnImpl_8106C54C(0x8000, &host, &vm);
	if (ret != 0)
		goto out;
	for (i = 0; i < count; i++) {
		control = controls[i];
		ret = MlnImpl_8106C6F8(vm, control->codeModule, control->uid, control->protocol,
				       control->type, control->signer, control->attributes);
		if (ret != 0)
			goto out;
	}
	ret = MlnImpl_8106C788(vm, controls[0]->uid);
	if (ret != 0)
		goto out;

	ret = MlnBufferToString(MLN_HEAP_GENERAL, agent, &name);
	if (ret != 0)
		goto out;
	size = agent->size + 2 + MlnStrlen("Control.Agents.") + MlnStrlen("Release");
	routine = MlnAllocate(MLN_HEAP_GENERAL, size);
	if (routine == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(routine, 0, size);
	ret = MlnImpl_81061F04(routine, size, name, "Init");
	if (ret != 0)
		goto out;
	r = MlnImpl_8106C794(vm, routine);
	if (r == 0) {
		ret = MlnImpl_8106C970(vm, &initStatus);
		if (ret != 0)
			goto out;
		release = 0;
	} else if (r == MLN_VM_ERROR_NO_ROUTINE) {
		release = 1;
	} else {
		ret = MLN_CORE_ERROR_INIT;
		goto release;
	}
	if (initStatus != 0) {
		runStatus = initStatus;
		goto done;
	}

	release = 1;
	MlnMemset(routine, 0, size);
	ret = MlnImpl_81061F04(routine, size, name, "Run");
	if (ret != 0)
		goto release;
	ret = MlnImpl_8106C794(vm, routine);
	if (ret != 0)
		goto release;
	ret = MlnImpl_8106C970(vm, &runStatus);
	if (ret != 0)
		goto release;
	if (runStatus == 0) {
		ret = MlnImpl_8106C970(vm, &runData);
		if (ret != 0)
			goto release;
		ret = MlnImpl_8106C970(vm, &runSize);
		if (ret != 0)
			goto release;
		if (runData != 0 && runSize != 0) {
			buffer = MlnBufferCreate(MLN_HEAP_GENERAL, runSize, NULL);
			if (buffer == NULL) {
				ret = 2;
				goto release;
			}
			ret = MlnImpl_8106C7A4(vm, runData, runSize, buffer->data);
			if (ret != 0)
				goto release;
		}
	}
	if (runStatus == 0) {
		ret = MlnImpl_81070808(graph, &resultPair);
		if (ret != 0)
			goto release;
	}
done:
	*status = runStatus;
	*data = buffer;
	buffer = NULL;
	*result = resultPair;
	resultPair = NULL;
	if (!release)
		goto out;

release:
	MlnMemset(routine, 0, size);
	if (MlnCoreRoutineName(routine, size, name, "Release") == 0) {
		released = 0;
		r = MlnImpl_8106C794(vm, routine);
		if (r == MLN_VM_ERROR_NO_ROUTINE) {
			r = 0;
		} else if (r != 0) {
			r = MLN_CORE_ERROR_INIT;
		} else {
			r = MlnImpl_8106C970(vm, &released);
			if (r == 0 && released != 0)
				r = MLN_CORE_ERROR_RELEASE_STATUS;
		}
		if (ret == 0)
			ret = r;
	}
out:
	if (resultPair != NULL)
		MlnArrayDestroy(MLN_HEAP_GENERAL, resultPair);
	if (graph != NULL) {
		MlnImpl_8106F2C8(graph);
		MlnImpl_8106ED54(graph);
	}
	if (vm != NULL) {
		MlnImpl_8106C6C4(vm);
		vm = NULL;
	}
	if (buffer != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	MlnFree(MLN_HEAP_GENERAL, name);
	MlnFree(MLN_HEAP_GENERAL, routine);
	if (ret == 0)
		return 0;
	switch (ret & 0xFFFF0000) {
	case 0x23040000:
		return MLN_CORE_ERROR_KEYS;
	case 0x23090000:
		return MLN_CORE_ERROR_VM;
	}
	return ret;
}

/*
 * Run a service object's routine over its data: the data and its size, then the
 * object's value, go to the control VM, and the routine named by the object runs;
 * unless the object wants no result, the routine's result is returned in rights.
 */
unsigned int MlnImpl_81063CE4(MlnCoreContext *context, const MlnServiceObject *object,
			      unsigned int size, const unsigned char *data, MlnBuffer **rights)
{
	MlnBuffer *result = NULL;
	char *routine = NULL;
	unsigned int status;
	unsigned int value;
	unsigned int type;
	unsigned int ret;

	if (object == NULL) {
		ret = 3;
		goto out;
	}
	if (context == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto out;
	}
	if (context->keys == NULL) {
		ret = MLN_CORE_ERROR_NO_KEYS;
		goto out;
	}
	if (context->graph == NULL) {
		ret = MlnImpl_8106EC8C(&context->graph);
		if (ret != 0)
			goto out;
	}
	routine = MlnAllocate(MLN_HEAP_GENERAL, object->buffer->size + 1);
	if (routine == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemcpy(routine, object->buffer->data, object->buffer->size);
	routine[object->buffer->size] = 0;

	ret = MlnImpl_8106C8CC(context->vm, size, data);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8106C86C(context->vm, size);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8106C86C(context->vm, object->second);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8106C794(context->vm, routine);
	if (ret != 0) {
		if (ret == MLN_VM_ERROR_NO_ROUTINE)
			ret = MLN_CORE_ERROR_REFUSED;
		goto out;
	}
	ret = MlnImpl_8106C970(context->vm, &status);
	if (ret != 0)
		goto out;
	if (status != 0) {
		ret = MLN_CORE_ERROR_CHECK;
		goto out;
	}
	if (object->first != 0)
		goto out;
	ret = MlnImpl_8106C970(context->vm, &value);
	if (ret != 0)
		goto out;
	if (value == 0) {
		ret = MLN_CORE_ERROR_CHECK;
		goto out;
	}
	ret = MlnImpl_8106C9F4(context->vm, value, &result, &type);
	if (ret == 0 && type == 1)
		ret = MLN_CORE_ERROR_REFUSED;
out:
	MlnFree(MLN_HEAP_GENERAL, routine);
	if (result != NULL) {
		if ((ret == 0 || ret == MLN_CORE_ERROR_REFUSED) && rights != NULL)
			*rights = result;
		else
			MlnBufferDestroy(MLN_HEAP_GENERAL, result);
	}
	if (ret == 0)
		return 0;
	switch (ret & 0xFFFF0000) {
	case 0x23040000:
		return MLN_CORE_ERROR_KEYS;
	case 0x23090000:
		return MLN_CORE_ERROR_VM;
	}
	return ret;
}

/* destroy count buffers of a list, and the list */
static inline void MlnCoreBuffersFree(MlnBuffer **buffers, int count)
{
	int i;

	for (i = 0; i < count; i++)
		MlnBufferDestroy(MLN_HEAP_GENERAL, buffers[i]);
	MlnFree(MLN_HEAP_GENERAL, buffers);
}

/*
 * Export the bundle's content keys to an RSA public key: for each content key, its secret
 * key encrypted to the RSA key, the secret key's uid and the content key's uid.
 */
unsigned int MlnImpl_81063F38(MlnCoreContext *context, void *rsa, int *count,
			      MlnBuffer ***encryptedKeys, MlnBuffer ***keyIds,
			      MlnBuffer ***contentKeyIds)
{
	const MlnOctopusArray *contentKeys;
	const MlnOctopusContentKey *key;
	const MlnBuffer *source;
	MlnBuffer **encrypted;
	MlnBuffer **ids;
	MlnBuffer **contentIds;
	unsigned int ret;
	int n;
	int i;

	if (context == NULL) {
		ret = MLN_CORE_ERROR_INVALID;
		goto fail;
	}
	if (context->keys == NULL) {
		ret = MLN_CORE_ERROR_NO_KEYS;
		goto fail;
	}
	if (rsa == NULL || count == NULL || encryptedKeys == NULL || keyIds == NULL ||
	    contentKeyIds == NULL) {
		ret = 3;
		goto fail;
	}

	contentKeys = context->bundle->contentKeys;
	n = contentKeys->count;
	encrypted = MlnAllocate(MLN_HEAP_GENERAL, n * sizeof(MlnBuffer *));
	if (encrypted == NULL) {
		ret = 2;
		goto fail;
	}
	MlnMemset(encrypted, 0, n * sizeof(MlnBuffer *));
	ids = MlnAllocate(MLN_HEAP_GENERAL, n * sizeof(MlnBuffer *));
	if (ids == NULL) {
		ret = 2;
		goto encrypted;
	}
	MlnMemset(ids, 0, n * sizeof(MlnBuffer *));
	contentIds = MlnAllocate(MLN_HEAP_GENERAL, n * sizeof(MlnBuffer *));
	if (contentIds == NULL) {
		ret = 2;
		goto ids;
	}
	MlnMemset(contentIds, 0, n * sizeof(MlnBuffer *));

	for (i = 0; i < n; i++) {
		key = contentKeys->items[i];
		source = key->secretKey->encryptedKey;
		if (source == NULL)
			source = key->secretKey->data;
		encrypted[i] = MlnBufferCreate(MLN_HEAP_GENERAL, source->size, source->data);
		if (encrypted[i] == NULL) {
			ret = 2;
			goto contentIds;
		}
		ret = MlnImpl_810715FC(context->keys, key->secretKey->keyName,
				       key->secretKey->encryptionMethod, rsa, encrypted[i]);
		if (ret != 0)
			goto contentIds;
		ids[i] = MlnBufferCreate(MLN_HEAP_GENERAL, key->secretKey->uid->size,
					 key->secretKey->uid->data);
		if (ids[i] == NULL) {
			ret = 2;
			goto contentIds;
		}
		contentIds[i] = MlnBufferCreate(MLN_HEAP_GENERAL, key->uid->size, key->uid->data);
		if (contentIds[i] == NULL) {
			ret = 2;
			goto contentIds;
		}
	}
	*encryptedKeys = encrypted;
	*keyIds = ids;
	*contentKeyIds = contentIds;
	*count = n;
	return 0;

contentIds:
	MlnCoreBuffersFree(contentIds, n);
ids:
	MlnCoreBuffersFree(ids, n);
encrypted:
	MlnCoreBuffersFree(encrypted, n);
fail:
	switch (ret & 0xFFFF0000) {
	case 0x23040000:
		return MLN_CORE_ERROR_KEYS;
	case 0x23090000:
		return MLN_CORE_ERROR_VM;
	}
	return ret;
}
