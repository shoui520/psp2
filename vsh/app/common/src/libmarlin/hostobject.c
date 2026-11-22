/*
 * The host's object tree (0x8106CB50-): the objects the control VM's system
 * calls read, made on demand from the graph of the personality node, the
 * bundle, the SAML assertions and the parameters an action is run with. A
 * container object reads its children through its callbacks; the tree's
 * nodes are "/Octopus/...", "/Marlin/..." and the value lists of eight slots.
 * The file's name is ours.
 */
#include "mln_internal.h"

/* the database below the tree's root: "Octopus/SeaShell/Databases/Marlin" and its suffixes */
#define MLN_HOST_DATABASE		"Octopus/SeaShell/Databases/Marlin"
#define MLN_HOST_DATABASE_SEASHELL	"SeaShell/Databases/Marlin"
#define MLN_HOST_DATABASE_DATABASES	"Databases/Marlin"
#define MLN_HOST_DATABASE_MARLIN	"Marlin"

/* the first of the paths the system calls give a node's index for */
#define MLN_HOST_PATH_INDEXES		4096

/* the tree's nodes */
#define MLN_HOST_NODE_ROOT			0
#define MLN_HOST_NODE_OCTOPUS			1
#define MLN_HOST_NODE_MARLIN			2
#define MLN_HOST_NODE_PERSONALITY		3
#define MLN_HOST_NODE_CONTROL			4
#define MLN_HOST_NODE_CONTROLLER		5
#define MLN_HOST_NODE_ACTION			6
#define MLN_HOST_NODE_LINK			7
#define MLN_HOST_NODE_AGENT			8
#define MLN_HOST_NODE_SEASHELL			9
#define MLN_HOST_NODE_ASSERTIONS		10
#define MLN_HOST_NODE_LICENSE_SUSPENSION	11
#define MLN_HOST_NODE_PERSONALITY_ATTRIBUTES	12
#define MLN_HOST_NODE_CONTROL_ATTRIBUTES	13
#define MLN_HOST_NODE_CONTROLLER_ATTRIBUTES	14
#define MLN_HOST_NODE_ACTION_PARAMETERS		15
#define MLN_HOST_NODE_LINK_ATTRIBUTES		16
#define MLN_HOST_NODE_AGENT_PARAMETERS		17
#define MLN_HOST_NODE_AGENT_SESSION		18
#define MLN_HOST_NODE_DATABASES			19
#define MLN_HOST_NODE_DATABASES_MARLIN		20
#define MLN_HOST_NODE_ASSERTION			21	/* "@<index>", one of the assertions */
#define MLN_HOST_NODE_ASSERTION_ATTRIBUTES	22
#define MLN_HOST_NODE_SINK			23
#define MLN_HOST_NODE_SINK_ATTRIBUTES		24
#define MLN_HOST_NODE_COUNT			25
#define MLN_HOST_NODE_LAST			32	/* the last slot's */

static const MlnHostNode MlnHostNodes[MLN_HOST_NODE_COUNT] = {
	{ MLN_HOST_NODE_ROOT, MLN_HOST_NODE_OCTOPUS, -1, "/" },
	{ MLN_HOST_NODE_OCTOPUS, MLN_HOST_NODE_PERSONALITY, MLN_HOST_NODE_MARLIN, "Octopus" },
	{ MLN_HOST_NODE_MARLIN, MLN_HOST_NODE_ASSERTIONS, -1, "Marlin" },
	{ MLN_HOST_NODE_PERSONALITY, MLN_HOST_NODE_PERSONALITY_ATTRIBUTES, MLN_HOST_NODE_CONTROL,
	  "Personality" },
	{ MLN_HOST_NODE_CONTROL, MLN_HOST_NODE_CONTROL_ATTRIBUTES, MLN_HOST_NODE_CONTROLLER,
	  "Control" },
	{ MLN_HOST_NODE_CONTROLLER, MLN_HOST_NODE_CONTROLLER_ATTRIBUTES, MLN_HOST_NODE_ACTION,
	  "Controller" },
	{ MLN_HOST_NODE_ACTION, MLN_HOST_NODE_ACTION_PARAMETERS, MLN_HOST_NODE_LINK, "Action" },
	{ MLN_HOST_NODE_LINK, MLN_HOST_NODE_LINK_ATTRIBUTES, MLN_HOST_NODE_AGENT, "Link" },
	{ MLN_HOST_NODE_AGENT, MLN_HOST_NODE_AGENT_PARAMETERS, MLN_HOST_NODE_SEASHELL, "Agent" },
	{ MLN_HOST_NODE_SEASHELL, MLN_HOST_NODE_DATABASES, -1, "SeaShell" },
	{ MLN_HOST_NODE_ASSERTIONS, -1, MLN_HOST_NODE_LICENSE_SUSPENSION, "Assertions" },
	{ MLN_HOST_NODE_LICENSE_SUSPENSION, -1, -1, "LicenseSuspension" },
	{ MLN_HOST_NODE_PERSONALITY_ATTRIBUTES, -1, -1, "Attributes" },
	{ MLN_HOST_NODE_CONTROL_ATTRIBUTES, -1, -1, "Attributes" },
	{ MLN_HOST_NODE_CONTROLLER_ATTRIBUTES, -1, -1, "Attributes" },
	{ MLN_HOST_NODE_ACTION_PARAMETERS, MLN_HOST_NODE_SINK, -1, "Parameters" },
	{ MLN_HOST_NODE_LINK_ATTRIBUTES, -1, -1, "Attributes" },
	{ MLN_HOST_NODE_AGENT_PARAMETERS, -1, MLN_HOST_NODE_AGENT_SESSION, "Parameters" },
	{ MLN_HOST_NODE_AGENT_SESSION, -1, -1, "Session" },
	{ MLN_HOST_NODE_DATABASES, MLN_HOST_NODE_DATABASES_MARLIN, -1, "Databases" },
	{ MLN_HOST_NODE_DATABASES_MARLIN, -1, -1, "Marlin" },
	{ MLN_HOST_NODE_ASSERTION, MLN_HOST_NODE_ASSERTION_ATTRIBUTES, -1, "@<index>" },
	{ MLN_HOST_NODE_ASSERTION_ATTRIBUTES, -1, -1, "Attributes" },
	{ MLN_HOST_NODE_SINK, MLN_HOST_NODE_SINK_ATTRIBUTES, -1, "Sink" },
	{ MLN_HOST_NODE_SINK_ATTRIBUTES, -1, -1, "Attributes" },
};

/* an object without children or value: its id, its name and its type */
static inline __attribute__((always_inline)) int MlnHostObjectCreate(int id, unsigned int size,
								      const void *name, int type,
								      MlnHostObject **out)
{
	MlnHostObject *object;
	MlnBuffer *copy = NULL;
	int ret = 2;

	object = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*object));
	if (object == NULL)
		goto out;
	MlnMemset(object, 0, sizeof(*object));
	copy = MlnBufferCreate(MLN_HEAP_GENERAL, size, name);
	if (copy == NULL)
		goto out;
	object->id = id;
	object->name = copy;
	object->type = type;
	object->hasChildren = 0;
	*out = object;
	object = NULL;
	copy = NULL;
	ret = 0;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, copy);
	MlnFree(MLN_HEAP_GENERAL, object);
	return ret;
}

/* the length of a path */
static inline __attribute__((always_inline)) unsigned int MlnHostPathLength(const char *path)
{
	unsigned int length = 0;

	while (path[length] != '\0')
		length++;
	return length;
}

/* a container without children */
int MlnImpl_8106CB50(MlnHostGraph *graph, MlnHostObject *object, int index, MlnHostObject **out)
{
	return 1;
}

int MlnImpl_8106CB58(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out)
{
	return 1;
}

/* LicenseSuspension: no child by any name */
int MlnImpl_8106CB60(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out)
{
	if (out == NULL)
		return 3;
	*out = NULL;
	return 0;
}

int MlnImpl_8106CB78(MlnHostObject *object)
{
	if (object != NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, object->name);
		if (object->type == MLN_HOST_OBJECT_STRING || object->type == MLN_HOST_OBJECT_BYTES)
			MlnBufferDestroy(MLN_HEAP_GENERAL, object->value.buffer);
		MlnFree(MLN_HEAP_GENERAL, object);
	}
	return 0;
}

/*
 * A path the system calls give, from the node of an index: one into the
 * database (an absolute path, or one below the root) gives 1, with the root's
 * index and the path; an index of MLN_HOST_PATH_INDEXES or more is passed on.
 */
__attribute__((noinline))
int MlnImpl_8106CBC4(int index, const char *path, int *outIndex, const char **outPath)
{
	if (*path == '/') {
		while (*++path == '/')
			;
		goto root;
	}
	if (index >= MLN_HOST_PATH_INDEXES) {
		*outIndex = index;
		*outPath = path;
		return 1;
	}
	if (index == MLN_HOST_NODE_ROOT) {
root:
		if (MlnStrncmp(path, MLN_HOST_DATABASE, MlnStrlen(MLN_HOST_DATABASE) - 1) == 0 &&
		    MlnStrlen(path) > MlnStrlen(MLN_HOST_DATABASE) - 1) {
			*outIndex = MLN_HOST_NODE_ROOT;
			*outPath = path;
			return 1;
		}
	/* below the root's children the comparisons decide nothing */
	} else if (index == MLN_HOST_NODE_OCTOPUS) {
		MlnStrncmp(path, MLN_HOST_DATABASE_SEASHELL, MlnStrlen(MLN_HOST_DATABASE_SEASHELL) - 1);
	} else if (index == MLN_HOST_NODE_SEASHELL) {
		MlnStrncmp(path, MLN_HOST_DATABASE_DATABASES,
			   MlnStrlen(MLN_HOST_DATABASE_DATABASES) - 1);
	} else if (index == MLN_HOST_NODE_DATABASES) {
		MlnStrncmp(path, MLN_HOST_DATABASE_MARLIN, MlnStrlen(MLN_HOST_DATABASE_MARLIN) - 1);
	}
	return 0;
}

/*
 * The value list a path names in a parameter block: with top, the path's
 * first name is a parameter's; every other name is that of the parameter
 * block among the values of the list before it (the first parameter's when
 * it is the first).
 */
int MlnImpl_8106CD00(const MlnPlanktonParameterBlock *block, const char *path, unsigned int size,
		     int top, MlnPlanktonValueList **out)
{
	MlnPlanktonValueList *current = NULL;
	MlnPlanktonValueList *parameters;
	MlnPlanktonValueList *list;
	MlnPlanktonParameterBlock *child;
	const char *segment;
	const char *next;
	unsigned int remaining;
	unsigned int length;
	unsigned int count;
	unsigned int i;

	if (block == NULL || path == NULL || out == NULL)
		return 3;
	if (size != 0) {
		remaining = size;
		for (segment = path;; segment = next) {
			/* the name, up to the next slash or the path's end */
			length = 0;
			next = segment;
			while (length != remaining) {
				if (*next == '/') {
					next++;
					remaining = remaining - 1 - length;
					goto named;
				}
				next++;
				length++;
			}
			remaining = 0;
named:
			if (next == NULL)
				break;
			if (top == 1) {
				parameters = block->parameters;
				count = block->count;
				if (parameters == NULL || segment == NULL || count == 0)
					return MLN_HOST_ERROR_NOT_FOUND;
				for (i = 0; i < count; i++) {
					current = &parameters[i];
					if (MlnStrncmp(segment, (const char *)current->name->data,
						       length) == 0)
						break;
				}
				if (i >= count)
					return MLN_HOST_ERROR_NOT_FOUND;
				top = 0;
			} else {
				if (current != NULL) {
					if (current->values == NULL ||
					    current->values->type != MLN_PLANKTON_VALUE_LIST)
						return MLN_HOST_ERROR_NOT_FOUND;
				} else {
					current = block->parameters;
					if (current == NULL)
						return MLN_HOST_ERROR_NOT_FOUND;
				}
				if (segment == NULL || current->values == NULL)
					return MLN_HOST_ERROR_NOT_FOUND;
				list = current->values->value.list;
				if (list == NULL || list->count == 0 || list->values == NULL ||
				    list->values->type != MLN_PLANKTON_PARAMETER)
					return MLN_HOST_ERROR_NOT_FOUND;
				child = list->values->value.parameters;
				if (child == NULL)
					return MLN_HOST_ERROR_NOT_FOUND;
				for (i = 0;;) {
					if (MlnStrncmp(segment,
						       (const char *)child->parameters->name->data,
						       length) == 0)
						break;
					if (++i >= list->count || list->values == NULL ||
					    list->values[i].type != MLN_PLANKTON_PARAMETER)
						return MLN_HOST_ERROR_NOT_FOUND;
					child = list->values[i].value.parameters;
					if (child == NULL)
						return MLN_HOST_ERROR_NOT_FOUND;
				}
				if (i >= list->count)
					return MLN_HOST_ERROR_NOT_FOUND;
				current = list->values[i].value.parameters->parameters;
			}
			if (remaining == 0)
				break;
		}
	}
	*out = current;
	return 0;
}

/* an object of a SAML attribute: an integer (1 without a value) or a string */
int MlnImpl_8106CF58(const MlnOctopusSamlAttribute *attribute, MlnHostObject **out)
{
	MlnHostObject *object;
	int ret;

	if (attribute->type == MLN_OCTOPUS_SAML_INTEGER) {
		ret = MlnHostObjectCreate(-1, attribute->name->size, attribute->name->data,
					  MLN_HOST_OBJECT_INTEGER, &object);
		if (ret != 0)
			return ret;
		object->value.integer = attribute->value.integer;
	} else if (attribute->type == MLN_OCTOPUS_SAML_EMPTY) {
		ret = MlnHostObjectCreate(-1, attribute->name->size, attribute->name->data,
					  MLN_HOST_OBJECT_INTEGER, &object);
		if (ret != 0)
			return ret;
		object->value.integer = 1;
	} else {
		ret = MlnHostObjectCreate(-1, attribute->name->size, attribute->name->data,
					  MLN_HOST_OBJECT_STRING, &object);
		if (ret != 0)
			return ret;
		object->value.buffer = MlnBufferCreate(MLN_HEAP_GENERAL,
						       attribute->value.string->size,
						       attribute->value.string->data);
		if (object->value.buffer == NULL) {
			MlnImpl_8106CB78(object);
			return 2;
		}
	}
	*out = object;
	return 0;
}

/* an assertion's attributes: one by name, one by index */
int MlnImpl_8106D138(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out)
{
	MlnOctopusArray *attributes = object->value.container;
	MlnOctopusSamlAttribute *attribute;
	int i;

	for (i = 0; i < object->childCount; i++) {
		attribute = attributes->items[i];
		if (MlnBytesCompare(MLN_HEAP_GENERAL, size, name, attribute->name->size,
				    attribute->name->data) == 0)
			break;
	}
	if (i < object->childCount)
		return MlnImpl_8106CF58(attribute, out);
	return 0;
}

int MlnImpl_8106D1C8(MlnHostGraph *graph, MlnHostObject *object, int index, MlnHostObject **out)
{
	MlnOctopusArray *attributes = object->value.container;

	if (index < object->childCount)
		return MlnImpl_8106CF58(attributes->items[index], out);
	*out = NULL;
	return 0;
}

/* Session: its ContextId */
int MlnImpl_8106D200(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out)
{
	MlnBuffer *contextId = object->value.buffer;
	MlnHostObject *child;
	int ret;

	if (MlnBytesCompare(MLN_HEAP_GENERAL, size, name, 9, "ContextId") != 0 || contextId == NULL) {
		*out = NULL;
		return 0;
	}
	ret = MlnHostObjectCreate(-1, 9, "ContextId", MLN_HOST_OBJECT_STRING, &child);
	if (ret != 0)
		return ret;
	child->value.buffer = MlnBufferCreate(MLN_HEAP_GENERAL, contextId->size, contextId->data);
	if (child->value.buffer == NULL) {
		MlnImpl_8106CB78(child);
		return 2;
	}
	*out = child;
	return 0;
}

/* an "Id" object of an object's uid */
static inline __attribute__((always_inline)) int MlnHostIdCreate(MlnBuffer *const *uid,
								  MlnHostObject **out)
{
	MlnHostObject *child;
	int ret;

	ret = MlnHostObjectCreate(-1, 2, "Id", MLN_HOST_OBJECT_STRING, &child);
	if (ret != 0)
		return ret;
	child->value.buffer = MlnBufferCreate(MLN_HEAP_GENERAL, (*uid)->size, (*uid)->data);
	if (child->value.buffer == NULL) {
		MlnImpl_8106CB78(child);
		return 2;
	}
	*out = child;
	return 0;
}

/* Controller: its Id */
int MlnImpl_8106D318(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out)
{
	MlnOctopusController *controller = object->value.container;

	if (MlnBytesCompare(MLN_HEAP_GENERAL, size, name, 2, "Id") != 0 || controller->uid == NULL) {
		*out = NULL;
		return 0;
	}
	return MlnHostIdCreate(&controller->uid, out);
}

int MlnImpl_8106D434(MlnHostGraph *graph, MlnHostObject *object, int index, MlnHostObject **out)
{
	MlnOctopusController *controller = object->value.container;

	if (index != 0 || controller->uid == NULL) {
		*out = NULL;
		return 0;
	}
	return MlnHostIdCreate(&controller->uid, out);
}

/* Control: its Id */
int MlnImpl_8106D530(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out)
{
	MlnOctopusControl *control = object->value.container;

	if (MlnBytesCompare(MLN_HEAP_GENERAL, size, name, 2, "Id") != 0 || control->uid == NULL) {
		*out = NULL;
		return 0;
	}
	return MlnHostIdCreate(&control->uid, out);
}

int MlnImpl_8106D64C(MlnHostGraph *graph, MlnHostObject *object, int index, MlnHostObject **out)
{
	MlnOctopusControl *control = object->value.container;

	if (index != 0 || control->uid == NULL) {
		*out = NULL;
		return 0;
	}
	return MlnHostIdCreate(&control->uid, out);
}

/* Personality: its Id */
int MlnImpl_8106D748(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out)
{
	MlnOctopusNode *node = object->value.container;

	if (MlnBytesCompare(MLN_HEAP_GENERAL, size, name, 2, "Id") != 0 || node->uid == NULL) {
		*out = NULL;
		return 0;
	}
	return MlnHostIdCreate(&node->uid, out);
}

int MlnImpl_8106D864(MlnHostGraph *graph, MlnHostObject *object, int index, MlnHostObject **out)
{
	MlnOctopusNode *node = object->value.container;

	if (index != 0 || node->uid == NULL) {
		*out = NULL;
		return 0;
	}
	return MlnHostIdCreate(&node->uid, out);
}

/* an object of an Octopus attribute; a list or an array is a container of its attributes */
int MlnImpl_8106D960(const MlnOctopusAttribute *attribute, MlnHostObject **out)
{
	MlnBuffer unnamed = { 7, (unsigned char *)"unnamed" };
	const MlnBuffer *name = attribute->name;
	MlnHostObject *object;
	int ret;

	if (name == NULL)
		name = &unnamed;
	switch (attribute->type) {
	case MLN_OCTOPUS_ATTRIBUTE_INT:
		ret = MlnHostObjectCreate(-1, name->size, name->data, MLN_HOST_OBJECT_INTEGER,
					  &object);
		if (ret != 0)
			return ret;
		object->value.integer = attribute->value.integer;
		break;
	case MLN_OCTOPUS_ATTRIBUTE_STRING:
		ret = MlnHostObjectCreate(-1, name->size, name->data, MLN_HOST_OBJECT_STRING,
					  &object);
		if (ret != 0)
			return ret;
		object->value.buffer = MlnBufferCreate(MLN_HEAP_GENERAL,
						       attribute->value.string->size,
						       attribute->value.string->data);
		if (object->value.buffer == NULL)
			goto fail;
		break;
	case MLN_OCTOPUS_ATTRIBUTE_BYTES:
		ret = MlnHostObjectCreate(-1, name->size, name->data, MLN_HOST_OBJECT_BYTES,
					  &object);
		if (ret != 0)
			return ret;
		object->value.buffer = MlnBufferCreate(MLN_HEAP_GENERAL,
						       attribute->value.bytes->size,
						       attribute->value.bytes->data);
		if (object->value.buffer == NULL)
			goto fail;
		break;
	case MLN_OCTOPUS_ATTRIBUTE_LIST:
	case MLN_OCTOPUS_ATTRIBUTE_ARRAY:
		ret = MlnHostObjectCreate(-1, name->size, name->data, MLN_HOST_OBJECT_CONTAINER,
					  &object);
		if (ret != 0)
			return ret;
		object->value.container = attribute->value.attributes;
		if (object->value.container == NULL)
			goto fail;
		object->getByIndex = MlnImpl_8106DCC0;
		object->getByName = MlnImpl_8106DC1C;
		object->hasChildren = 1;
		object->childCount = attribute->value.attributes->count;
		break;
	default:
		return 5;
	}
	*out = object;
	return 0;
fail:
	MlnImpl_8106CB78(object);
	return 2;
}

/* an attribute list's attributes: one by name, one by index */
int MlnImpl_8106DC1C(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out)
{
	MlnOctopusArray *attributes = object->value.container;
	MlnOctopusAttribute *attribute;
	int i;

	for (i = 0; i < object->childCount; i++) {
		attribute = attributes->items[i];
		if (attribute->name != NULL &&
		    MlnBytesCompare(MLN_HEAP_GENERAL, size, name, attribute->name->size,
				    attribute->name->data) == 0)
			break;
	}
	if (i >= object->childCount) {
		*out = NULL;
		return 0;
	}
	return MlnImpl_8106D960(attribute, out);
}

int MlnImpl_8106DCC0(MlnHostGraph *graph, MlnHostObject *object, int index, MlnHostObject **out)
{
	MlnOctopusArray *attributes = object->value.container;

	if (index < object->childCount)
		return MlnImpl_8106D960(attributes->items[index], out);
	*out = NULL;
	return 0;
}

/*
 * The object of the value list a path names in a parameter block: a value
 * list of value lists is a container, kept in a free slot; one of a value is
 * that value.
 */
int MlnImpl_8106DCF8(MlnHostGraph *graph, const MlnPlanktonParameterBlock *block, const char *path,
		     int top, MlnHostObject **out)
{
	MlnPlanktonValueList *list = NULL;
	MlnHostObject *object;
	unsigned int length;
	int type;
	int slot;
	int ret;

	if (graph == NULL || block == NULL)
		return 3;
	if (path == NULL || out == NULL)
		return 3;
	length = MlnHostPathLength(path);
	if (MlnImpl_8106CD00(block, path, length, top, &list) == MLN_HOST_ERROR_NOT_FOUND) {
		*out = NULL;
		return 0;
	}
	if (list == NULL)
		return 3;
	if (list->values != NULL && list->values->type == MLN_PLANKTON_VALUE_LIST) {
		if (graph->slots[0].value == NULL)
			slot = 0;
		else if (graph->slots[1].value == NULL)
			slot = 1;
		else if (graph->slots[2].value == NULL)
			slot = 2;
		else if (graph->slots[3].value == NULL)
			slot = 3;
		else if (graph->slots[4].value == NULL)
			slot = 4;
		else if (graph->slots[5].value == NULL)
			slot = 5;
		else if (graph->slots[6].value == NULL)
			slot = 6;
		else if (graph->slots[7].value == NULL)
			slot = 7;
		else
			return MLN_HOST_ERROR_NO_SLOT;
		ret = MlnHostObjectCreate(graph->slots[slot].id, list->name->size, list->name->data,
					  MLN_HOST_OBJECT_CONTAINER, &object);
		if (ret != 0)
			return ret;
		graph->slots[slot].value = list;
		graph->slots[slot].kind = MLN_HOST_SLOT_VALUE_LIST;
		*out = object;
		return 0;
	}
	switch (list->values->type) {
	case MLN_PLANKTON_INTEGER:
	case MLN_PLANKTON_DATE:
		type = MLN_HOST_OBJECT_INTEGER;
		break;
	case MLN_PLANKTON_STRING:
		type = MLN_HOST_OBJECT_STRING;
		break;
	case MLN_PLANKTON_BYTE_ARRAY:
		type = MLN_HOST_OBJECT_BYTES;
		break;
	default:
		return 3;
	}
	ret = MlnHostObjectCreate(-1, list->name->size, list->name->data, type, &object);
	if (ret != 0)
		return ret;
	switch (list->values->type) {
	case MLN_PLANKTON_INTEGER:
		object->value.integer = list->values->value.integer;
		break;
	case MLN_PLANKTON_STRING:
	case MLN_PLANKTON_BYTE_ARRAY:
		object->value.buffer = MlnBufferCreate(MLN_HEAP_GENERAL,
						       list->values->value.buffer->size,
						       list->values->value.buffer->data);
		if (object->value.buffer == NULL) {
			ret = 2;
			goto fail;
		}
		break;
	case MLN_PLANKTON_DATE:
		if (list->values->value.integer < 0) {
			ret = 5;
			goto fail;
		}
		object->value.integer = list->values->value.integer;
		break;
	default:
		ret = 5;
		goto fail;
	}
	*out = object;
	return 0;
fail:
	MlnImpl_8106CB78(object);
	return ret;
}

/* a value list's values, by path */
int MlnImpl_8106E060(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out)
{
	MlnPlanktonParameterBlock block;

	if (graph == NULL || object == NULL)
		return 3;
	if (object->value.container == NULL)
		return 3;
	if (name == NULL || out == NULL)
		return 3;
	block.count = 1;
	block.parameters = object->value.container;
	return MlnImpl_8106DCF8(graph, &block, name, 0, out);
}

/* a parameter block's parameters, by path */
int MlnImpl_8106E0C0(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out)
{
	if (graph == NULL || object == NULL)
		return 3;
	if (object->value.container == NULL)
		return 3;
	if (name == NULL || out == NULL)
		return 3;
	return MlnImpl_8106DCF8(graph, object->value.container, name, 1, out);
}

/* Assertions: an assertion by its index, which becomes the graph's current one */
int MlnImpl_8106E10C(MlnHostGraph *graph, MlnHostObject *object, int index, MlnHostObject **out)
{
	MlnOctopusSamlAssertion **assertions;
	MlnHostObject *child;
	int ret;

	if (object == NULL)
		return 5;
	assertions = object->value.container;
	if (assertions == NULL)
		return 5;
	if ((unsigned int)index >= MLN_HOST_ASSERTIONS || assertions[index] == NULL) {
		*out = NULL;
		return 0;
	}
	ret = MlnHostObjectCreate(MLN_HOST_NODE_ASSERTION, 0, "", MLN_HOST_OBJECT_CONTAINER, &child);
	if (ret != 0)
		return ret;
	child->value.container = assertions[index];
	child->childCount = 1;
	child->getByIndex = NULL;
	child->getByName = MlnImpl_8106E20C;
	graph->current = index;
	*out = child;
	return 0;
}

/* an assertion: its IssueInstant, AssertionID, NotBefore and NotOnOrAfter; times are in minutes */
int MlnImpl_8106E20C(MlnHostGraph *graph, MlnHostObject *object, const char *name,
		     unsigned int size, MlnHostObject **out)
{
	MlnOctopusSamlAssertion *assertion;
	MlnHostObject *child;
	int seconds;
	int ret;

	if (object == NULL || name == NULL)
		return 5;
	if (out == NULL)
		return 5;
	assertion = object->value.container;
	if (assertion == NULL)
		return 5;
	if (MlnBytesCompare(MLN_HEAP_GENERAL, size, name, 12, "IssueInstant") == 0) {
		ret = MlnHostObjectCreate(-1, 12, "IssueInstant", MLN_HOST_OBJECT_INTEGER, &child);
		if (ret != 0)
			return ret;
		ret = MlnTimeToSeconds(&assertion->issueInstant, &seconds);
		if (ret != 0)
			goto fail;
	} else if (MlnBytesCompare(MLN_HEAP_GENERAL, size, name, 11, "AssertionID") == 0 &&
		   assertion->assertionId != NULL) {
		ret = MlnHostObjectCreate(-1, 11, "AssertionID", MLN_HOST_OBJECT_STRING, &child);
		if (ret != 0)
			return ret;
		ret = 2;
		child->value.buffer = MlnBufferCreate(MLN_HEAP_GENERAL, assertion->assertionId->size,
						      assertion->assertionId->data);
		if (child->value.buffer == NULL)
			goto fail;
		*out = child;
		return 0;
	} else if (MlnBytesCompare(MLN_HEAP_GENERAL, size, name, 9, "NotBefore") == 0 &&
		   assertion->notBefore != NULL) {
		ret = MlnHostObjectCreate(-1, 9, "NotBefore", MLN_HOST_OBJECT_INTEGER, &child);
		if (ret != 0)
			return ret;
		ret = MlnTimeToSeconds(assertion->notBefore, &seconds);
		if (ret != 0)
			goto fail;
	} else if (MlnBytesCompare(MLN_HEAP_GENERAL, size, name, 12, "NotOnOrAfter") == 0 &&
		   assertion->notOnOrAfter != NULL) {
		ret = MlnHostObjectCreate(-1, 12, "NotOnOrAfter", MLN_HOST_OBJECT_INTEGER, &child);
		if (ret != 0)
			return ret;
		ret = MlnTimeToSeconds(assertion->notOnOrAfter, &seconds);
		if (ret != 0)
			goto fail;
	} else {
		*out = NULL;
		return 0;
	}
	child->value.integer = seconds / 60;
	*out = child;
	return 0;
fail:
	MlnImpl_8106CB78(child);
	return ret;
}

/*
 * The object of a node of the tree, or of a slot: its value and the
 * callbacks its children are read with. Attributes are the control's, for
 * /Octopus/Control/Attributes. A node without what its object is made of
 * gives no object.
 */
int MlnImpl_8106E57C(MlnHostGraph *graph, int index, MlnOctopusArray *attributes,
		     MlnHostObject **out)
{
	MlnPlanktonValueList *sink = NULL;
	MlnHostObject *object;
	MlnHostSlot *slot;
	unsigned int count;
	int ret;
	int i;

	if (out == NULL)
		return 3;
	if (graph == NULL || index < 0 || index > MLN_HOST_NODE_LAST) {
		*out = NULL;
		return 0;
	}
	if (index < MLN_HOST_NODE_COUNT)
		ret = MlnHostObjectCreate(MlnHostNodes[index].id, MlnStrlen(MlnHostNodes[index].name),
					  MlnHostNodes[index].name, MLN_HOST_OBJECT_CONTAINER, &object);
	else
		ret = MlnHostObjectCreate(index, 0, NULL, MLN_HOST_OBJECT_CONTAINER, &object);
	if (ret != 0)
		return ret;
	switch (index) {
	case MLN_HOST_NODE_ROOT:
	case MLN_HOST_NODE_OCTOPUS:
	case MLN_HOST_NODE_MARLIN:
	case MLN_HOST_NODE_ACTION:
	case MLN_HOST_NODE_AGENT:
	case MLN_HOST_NODE_SEASHELL:
	case MLN_HOST_NODE_DATABASES:
	case MLN_HOST_NODE_DATABASES_MARLIN:
		object->value.container = graph;
		break;
	case MLN_HOST_NODE_PERSONALITY:
		if (graph->personality == NULL)
			goto none;
		object->value.container = graph->personality;
		object->getByIndex = MlnImpl_8106D864;
		if (graph->personality->uid != NULL)
			object->childCount = 1;
		object->getByName = MlnImpl_8106D748;
		break;
	case MLN_HOST_NODE_CONTROL:
		if (graph->bundle != NULL && graph->bundle->control != NULL) {
			object->value.container = graph->bundle->control;
			object->getByIndex = MlnImpl_8106D64C;
			if (graph->bundle->control->uid != NULL)
				object->childCount = 1;
			object->getByName = MlnImpl_8106D530;
		} else {
			if (attributes == NULL)
				goto none;
			object->value.container = NULL;
			object->childCount = 0;
			object->getByIndex = MlnImpl_8106CB50;
			object->getByName = MlnImpl_8106CB58;
		}
		break;
	case MLN_HOST_NODE_CONTROLLER:
		if (graph->bundle == NULL || graph->bundle->controller == NULL)
			goto none;
		object->value.container = graph->bundle->controller;
		object->getByIndex = MlnImpl_8106D434;
		if (graph->bundle->controller->uid != NULL)
			object->childCount = 1;
		object->getByName = MlnImpl_8106D318;
		break;
	case MLN_HOST_NODE_ASSERTIONS:
		count = 0;
		for (i = 0; i < MLN_HOST_ASSERTIONS; i++)
			if (graph->assertions[i] != NULL)
				count++;
		if (count == 0)
			goto none;
		object->childCount = count;
		object->value.container = graph->assertions;
		object->getByName = NULL;
		object->getByIndex = MlnImpl_8106E10C;
		break;
	case MLN_HOST_NODE_LICENSE_SUSPENSION:
		if (graph->licenseSuspension == NULL)
			goto none;
		object->value.container = graph->licenseSuspension;
		object->childCount = 1;
		object->getByIndex = NULL;
		object->getByName = MlnImpl_8106CB60;
		break;
	case MLN_HOST_NODE_PERSONALITY_ATTRIBUTES:
		if (graph->personality == NULL || graph->personality->attributes == NULL)
			goto none;
		object->value.container = graph->personality->attributes;
		object->getByIndex = MlnImpl_8106DCC0;
		object->getByName = MlnImpl_8106DC1C;
		object->childCount = graph->personality->attributes->count;
		break;
	case MLN_HOST_NODE_CONTROL_ATTRIBUTES:
		if (attributes == NULL)
			goto none;
		object->value.container = attributes;
		object->childCount = attributes->count;
		object->getByIndex = MlnImpl_8106DCC0;
		object->getByName = MlnImpl_8106DC1C;
		break;
	case MLN_HOST_NODE_CONTROLLER_ATTRIBUTES:
		if (graph->bundle == NULL || graph->bundle->controller == NULL ||
		    graph->bundle->controller->attributes == NULL)
			goto none;
		object->value.container = graph->bundle->controller->attributes;
		object->getByIndex = MlnImpl_8106DCC0;
		object->getByName = MlnImpl_8106DC1C;
		object->childCount = graph->bundle->controller->attributes->count;
		break;
	case MLN_HOST_NODE_ACTION_PARAMETERS:
	case MLN_HOST_NODE_AGENT_PARAMETERS:
		if (graph->parameters == NULL)
			goto none;
		object->value.container = graph->parameters;
		object->getByName = MlnImpl_8106E0C0;
		object->getByIndex = NULL;
		object->childCount = graph->parameters->count;
		break;
	case MLN_HOST_NODE_AGENT_SESSION:
		if (graph->contextId == NULL)
			goto none;
		object->value.container = graph->contextId;
		object->childCount = 1;
		object->getByIndex = NULL;
		object->getByName = MlnImpl_8106D200;
		break;
	case MLN_HOST_NODE_ASSERTION:
		if (graph->current >= MLN_HOST_ASSERTIONS || graph->assertions[graph->current] == NULL)
			goto none;
		object->value.container = graph->assertions[graph->current];
		object->childCount = 1;
		object->getByIndex = NULL;
		object->getByName = MlnImpl_8106E20C;
		break;
	case MLN_HOST_NODE_ASSERTION_ATTRIBUTES:
		if (graph->current >= MLN_HOST_ASSERTIONS || graph->assertions[graph->current] == NULL)
			goto none;
		object->value.container = graph->assertions[graph->current]->attributes;
		object->getByIndex = MlnImpl_8106D1C8;
		object->getByName = MlnImpl_8106D138;
		object->childCount = graph->assertions[graph->current]->attributes->count;
		break;
	case MLN_HOST_NODE_SINK:
		if (MlnImpl_8106CD00(graph->parameters, "Sink", 4, 1, &sink) != 0 ||
		    graph->parameters == NULL)
			goto none;
		object->value.container = sink;
		object->getByName = MlnImpl_8106E060;
		object->getByIndex = NULL;
		object->childCount = sink->count;
		break;
	case MLN_HOST_NODE_SINK_ATTRIBUTES:
		if (graph->bundle == NULL || graph->bundle->attributes == NULL)
			goto none;
		object->value.container = graph->bundle->attributes;
		object->getByIndex = MlnImpl_8106DCC0;
		object->getByName = MlnImpl_8106DC1C;
		object->childCount = graph->bundle->attributes->count;
		break;
	case MLN_HOST_SLOT_FIRST:
	case MLN_HOST_SLOT_FIRST + 1:
	case MLN_HOST_SLOT_FIRST + 2:
	case MLN_HOST_SLOT_FIRST + 3:
	case MLN_HOST_SLOT_FIRST + 4:
	case MLN_HOST_SLOT_FIRST + 5:
	case MLN_HOST_SLOT_FIRST + 6:
	case MLN_HOST_SLOT_FIRST + 7:
		if (index == graph->slots[0].id)
			i = 0;
		else if (index == graph->slots[1].id)
			i = 1;
		else if (index == graph->slots[2].id)
			i = 2;
		else if (index == graph->slots[3].id)
			i = 3;
		else if (index == graph->slots[4].id)
			i = 4;
		else if (index == graph->slots[5].id)
			i = 5;
		else if (index == graph->slots[6].id)
			i = 6;
		else if (index == graph->slots[7].id)
			i = 7;
		else
			i = MLN_HOST_SLOTS;	/* past the slots: nothing is read there */
		slot = &graph->slots[i];
		if (slot->value == NULL)
			goto none;
		if (slot->kind == MLN_HOST_SLOT_VALUE_LIST) {
			object->value.container = slot->value;
			object->getByName = MlnImpl_8106E060;
			object->getByIndex = NULL;
			object->childCount = ((MlnPlanktonValueList *)slot->value)->count;
		} else if (slot->kind == MLN_HOST_SLOT_ATTRIBUTES) {
			object->value.container = slot->value;
			object->getByName = MlnImpl_8106DC1C;
			object->getByIndex = MlnImpl_8106DCC0;
			object->childCount = ((MlnOctopusArray *)graph->slots[i].value)->count;
		} else {
			goto none;
		}
		break;
	default:
		goto none;
	}
	*out = object;
	return 0;
none:
	*out = NULL;
	MlnImpl_8106CB78(object);
	return 0;
}

/* the slots, free, with their ids */
#define MLN_HOST_SLOT_RESET(graph, i)					\
	do {								\
		(graph)->slots[i].value = NULL;				\
		(graph)->slots[i].kind = -1;				\
		(graph)->slots[i].id = MLN_HOST_SLOT_FIRST + (i);	\
	} while (0)

static inline __attribute__((always_inline)) void MlnHostSlotsReset(MlnHostGraph *graph)
{
	MLN_HOST_SLOT_RESET(graph, 0);
	MLN_HOST_SLOT_RESET(graph, 1);
	MLN_HOST_SLOT_RESET(graph, 2);
	MLN_HOST_SLOT_RESET(graph, 3);
	MLN_HOST_SLOT_RESET(graph, 4);
	MLN_HOST_SLOT_RESET(graph, 5);
	MLN_HOST_SLOT_RESET(graph, 6);
	MLN_HOST_SLOT_RESET(graph, 7);
}

unsigned int MlnImpl_8106EC8C(MlnHostGraph **graph)
{
	MlnHostGraph *created;

	created = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*created));
	if (created == NULL)
		return 2;
	MlnMemset(created, 0, sizeof(*created));
	created->current = -1;
	MlnHostSlotsReset(created);
	*graph = created;
	return 0;
}

unsigned int MlnImpl_8106ED54(MlnHostGraph *graph)
{
	MlnHostLinks *links;
	int i;

	if (graph != NULL) {
		links = graph->links;
		MlnHostSlotsReset(graph);
		if (links != NULL) {
			if (links->targets != NULL)
				MlnImpl_8103C57C(links->targets, MLN_OCTOPUS_PLAIN);
			if (links->links != NULL)
				MlnImpl_8103C57C(links->links, MLN_OCTOPUS_LINK);
			MlnFree(MLN_HEAP_GENERAL, links);
		}
		if (graph->records != NULL)
			MlnImpl_8107B4A0(graph->records);
		if (graph->licenseSuspension != NULL)
			MlnBufferDestroy(MLN_HEAP_GENERAL, graph->licenseSuspension);
		MlnImpl_8103B8F8(graph->personality);
		for (i = 0; i < MLN_HOST_ASSERTIONS; i++)
			MlnImpl_8103CD0C(graph->assertions[i]);
		MlnFree(MLN_HEAP_GENERAL, graph);
	}
	return 0;
}

unsigned int MlnImpl_8106EE80(MlnHostGraph *graph, MlnOctopusNode *node)
{
	if (graph == NULL)
		return 4;
	graph->personality = node;
	return 0;
}

unsigned int MlnImpl_8106EE94(MlnHostGraph *graph, MlnPlanktonParameterBlock *parameters)
{
	if (graph == NULL)
		return 4;
	graph->parameters = parameters;
	return 0;
}

unsigned int MlnImpl_8106EEA8(MlnHostGraph *graph, MlnOctopusBundle *bundle)
{
	if (graph == NULL)
		return 4;
	graph->bundle = bundle;
	return 0;
}

/*
 * The graph's links, and the nodes they reach from the personality: each
 * with the node it is reached from and the control of the link. Without
 * links, the links the graph had are freed.
 */
unsigned int MlnImpl_8106EEBC(MlnHostGraph *graph, MlnOctopusArray *links)
{
	MlnOctopusNode *node;
	MlnHostLinks *old;
	MlnHostLinks *created;
	MlnOctopusArray *found;
	MlnOctopusArray *targets;
	MlnHostLinkTarget *target;
	MlnHostLinkTarget *other;
	MlnOctopusLink *link;
	MlnBuffer *to;
	MlnBuffer *from;
	int linkCount;
	int count;
	int ret;
	int i;
	int j;
	int k;

	if (graph == NULL || graph->personality == NULL)
		return 4;
	old = graph->links;
	if (links != NULL) {
		if (old != NULL) {
			if (old->targets != NULL)
				MlnImpl_8103C57C(old->targets, MLN_OCTOPUS_PLAIN);
			if (old->links != NULL)
				MlnImpl_8103C57C(old->links, MLN_OCTOPUS_LINK);
			MlnFree(MLN_HEAP_GENERAL, old);
		}
		linkCount = links->count;
	} else {
		if (old != NULL) {
			MlnImpl_8103C57C(old->links, MLN_OCTOPUS_LINK);
			return 0;
		}
		linkCount = 0;
	}
	node = graph->personality;
	created = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*created));
	if (created == NULL)
		goto fail;
	if (node == NULL || links == NULL) {
		created->links = links;
		created->targets = NULL;
		graph->links = created;
		return 0;
	}
	found = MlnImpl_8103B758(linkCount + 1);
	if (found == NULL)
		goto fail_free;
	ret = 2;
	target = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*target));
	if (target == NULL)
		goto out;
	target->uid = node->uid;
	target->control = NULL;
	target->from = NULL;
	found->items[0] = target;
	count = 1;
	for (i = 0; i < count; i++) {
		target = found->items[i];
		for (j = 0; j < linkCount; j++) {
			link = links->items[j];
			if (MlnBufferCompare(MLN_HEAP_GENERAL, target->uid, link->from) != 0)
				continue;
			to = link->to;
			from = link->from;
			for (k = 0; k < count; k++) {
				other = found->items[k];
				if (other != NULL &&
				    MlnBufferCompare(MLN_HEAP_GENERAL, other->uid, to) == 0 &&
				    other->from != NULL &&
				    MlnBufferCompare(MLN_HEAP_GENERAL, other->from->uid, from) == 0)
					break;
			}
			if (k < count)
				continue;
			other = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*other));
			if (other == NULL)
				goto out;
			count = k + 1;
			other->uid = link->to;
			other->from = target;
			other->control = link->control;
			found->items[k] = other;
		}
	}
	targets = MlnImpl_8103B758(count);
	if (targets == NULL)
		goto out;
	for (k = 0; k < count; k++) {
		targets->items[k] = found->items[k];
		found->items[k] = NULL;
	}
	created->links = links;
	created->targets = targets;
	ret = 0;
out:
	MlnImpl_8103C57C(found, MLN_OCTOPUS_PLAIN);
	if (ret == 0) {
		graph->links = created;
		return 0;
	}
fail_free:
	MlnFree(MLN_HEAP_GENERAL, created);
fail:
	graph->links = NULL;
	return 2;
}

/* an assertion into the first free place; one the graph does not take is freed */
unsigned int MlnImpl_8106F264(MlnHostGraph *graph, MlnOctopusSamlAssertion *assertion)
{
	unsigned int ret;
	int i;

	if (graph == NULL) {
		ret = 4;
	} else if (assertion == NULL) {
		ret = 0;
	} else {
		ret = 5;
		for (i = 0; i < MLN_HOST_ASSERTIONS; i++) {
			if (graph->assertions[i] == NULL) {
				graph->assertions[i] = assertion;
				assertion = NULL;
				ret = 0;
				break;
			}
		}
	}
	MlnImpl_8103CD0C(assertion);
	return ret;
}

void MlnImpl_8106F2C8(MlnHostGraph *graph)
{
	int i;

	if (graph == NULL)
		return;
	for (i = 0; i < MLN_HOST_ASSERTIONS; i++) {
		if (graph->assertions[i] != NULL) {
			MlnImpl_8103CD0C(graph->assertions[i]);
			graph->assertions[i] = NULL;
		}
	}
}

unsigned int MlnImpl_8106F300(MlnHostGraph *graph, MlnBuffer *contextId)
{
	if (graph == NULL)
		return 4;
	graph->contextId = contextId;
	return 0;
}

/*
 * System.Host.GetTrustedTime: the trusted time in minutes, and whether it is
 * trusted; -5 when there is none.
 */
void MlnImpl_8106F314(MlnCoreHost *host, unsigned int *minutes, int *trusted)
{
	unsigned int now = 0;
	int mode = 0;

	if (MlnImpl_81052D84((int *)&now, &mode) != 0) {
		*minutes = -5;
		*trusted = 0;
		return;
	}
	*minutes = now / 60;
	*trusted = mode == 2;
}

/* System.Host.GetLocalTime: the local time in minutes, or -5 */
void MlnImpl_8106F388(MlnCoreHost *host, int *minutes)
{
	int offset = 0;
	int start;
	int local;

	if (MlnImpl_81052E04((unsigned int *)&start) != 0)
		goto fail;
	local = start;
	if (MlnImpl_81052E60(&offset) != 0)
		goto fail;
	*minutes = (local + offset) / 60;
	return;
fail:
	*minutes = -5;
}

/* System.Host.GetLocalTimeOffset: the local time's offset in minutes, or -5 */
void MlnImpl_8106F3F4(MlnCoreHost *host, int *minutes)
{
	int offset;

	if (MlnImpl_81052E60(&offset) != 0)
		*minutes = -5;
	else
		*minutes = offset / 60;
}

/*
 * System.Host.GetLocalTrustedTime: the trusted time in local minutes, and how
 * far the local clock is ahead of it; -5 and 0 when either clock fails.
 */
void MlnImpl_8106F438(MlnCoreHost *host, int *minutes, int *ahead)
{
	unsigned int now;
	int mode;
	int offset;
	int start;
	int local;
	int trusted;

	if (MlnImpl_81052D84((int *)&now, &mode) != 0)
		goto fail;
	if (MlnImpl_81052E04((unsigned int *)&start) != 0)
		goto fail;
	local = start;
	if (MlnImpl_81052E60(&offset) != 0)
		goto fail;
	local = local / 60 + offset / 60;
	if (local < 0)
		local = 0;
	trusted = offset / 60 + (int)(now / 60);
	if (trusted < 0) {
		trusted = 0;
	} else {
		local -= trusted;
		if (local < 0)
			local = 0;
	}
	if (minutes != NULL)
		*minutes = trusted;
	if (ahead != NULL)
		*ahead = local;
	return;
fail:
	if (minutes != NULL)
		*minutes = -5;
	if (ahead != NULL)
		*ahead = 0;
}

/* a word into a value buffer, the most significant byte first */
#define MLN_HOST_PUT_WORD(buffer, word) do {	\
	(buffer)[0] = (word) >> 24;		\
	(buffer)[1] = (word) >> 16;		\
	(buffer)[2] = (word) >> 8;		\
	(buffer)[3] = (word);			\
} while (0)

/*
 * System.Host.GetObject: the value of the object at a path, below the node of
 * an index. A path into the database is read from it. Otherwise the path is
 * walked from the tree's node: a name is a child of the node, or one the
 * object's callback gives, "@<n>" its n-th child, and a last "@Name", "@Size"
 * or "@Type" the object's name, size or type. A container's value is its id,
 * and one with children is given a slot. The result is 0, 1 with the value's
 * type negative when there is none (-6 not found, -7 too small, -8 not
 * readable), or an error.
 */
unsigned int MlnImpl_8106F504(MlnCoreHost *host, int index, const char *path,
			      unsigned char *buffer, unsigned int size,
			      const unsigned char *signer, unsigned int signerSize,
			      MlnOctopusArray *attributes, int *type, unsigned int *outSize)
{
	MlnHostGraph *graph = host->graph;
	MlnHostObject *current = NULL;
	MlnHostObject *child = NULL;
	int dbIndex = 0;
	const char *dbPath = NULL;
	const char *next;
	MlnBuffer *data;
	unsigned int result;
	unsigned int length;
	unsigned int value;
	unsigned int count;
	unsigned int i;
	int position;
	int node;
	int exotic;
	int scaled;
	int digit;
	int slot;
	int c;

	if (path == NULL || type == NULL || outSize == NULL)
		return 3;
	if (MlnStrlen(path) == 0) {
		*type = -6;
		*outSize = 0;
		return 1;
	}
	if (*path == '/')
		index = 0;
	if (MlnImpl_8106CBC4(index, path, &dbIndex, &dbPath)) {
		result = MlnImpl_8107BDB8(graph->records, signer, signerSize, dbIndex, dbPath,
					  buffer, size, type, outSize);
		if (result == 0x230B0011) {
			*type = -6;
			*outSize = 0;
			return 1;
		}
		if (result == 0x230B0013) {
			*type = -8;
			*outSize = 0;
			return 1;
		}
		if (result == 0x230B0014) {
			*type = -7;
			return 1;
		}
		if (result != 0) {
			*type = -2;
			*outSize = 0;
			return 1;
		}
		return 0;
	}
	result = MlnImpl_8106E57C(graph, index, attributes, &current);
	if (result != 0)
		goto out;

	/* the tree's nodes, as far as the path names them */
	exotic = 0;
	node = -1;
	position = -1;
	for (;;) {
		if (current == NULL)
			goto missing;
		c = *path;
		if (current->id < 0)
			goto callbacks;
		while (c == '/')
			c = *++path;
		if (c == '\0')
			goto value;
		length = 0;
		do
			length++;
		while (path[length] != '/' && path[length] != '\0');
		next = path + length;
		if (c != '@') {
			if ((unsigned int)current->id >= MLN_HOST_NODE_COUNT) {
				exotic = 1;
			} else {
				node = MlnHostNodes[current->id].firstChild;
				if (current->id == MLN_HOST_NODE_AGENT_PARAMETERS)
					exotic = 1;
			}
			for (; node >= 0; node = MlnHostNodes[node].nextSibling)
				if (length == MlnStrlen(MlnHostNodes[node].name) &&
				    MlnMemcmp(MlnHostNodes[node].name, path, length) == 0)
					break;
			if (node >= 0) {
				result = MlnImpl_8106E57C(graph, node, attributes, &child);
				if (result != 0)
					goto out;
			} else if (current->getByName != NULL) {
				result = current->getByName(graph, current, path, length, &child);
				if (result != 0)
					goto out;
			} else {
				child = NULL;
			}
			MlnImpl_8106CB78(current);
			current = child;
			child = NULL;
			path = next;
			continue;
		}
		if (length == 5) {
			if (MlnMemcmp(path, "@Name", 5) == 0 || MlnMemcmp(path, "@Size", 5) == 0 ||
			    MlnMemcmp(path, "@Type", 5) == 0)
				goto callbacks;
		} else if (length == 1) {
			result = 3;
			goto out;
		}
		position = path[1] - '0';
		if ((unsigned char)position > 9) {
			result = 3;
			goto out;
		}
		for (i = 2; i < length; i++) {
			digit = path[i] - '0';
			scaled = position * 10;
			position = scaled + digit;
			if ((unsigned char)digit > 9) {
				result = 3;
				goto out;
			}
			if (scaled > position) {
				MlnImpl_8106CB78(child);
				*type = -6;
				*outSize = 0;
				result = 1;
				goto out;
			}
		}
		if ((unsigned int)current->id < MLN_HOST_NODE_COUNT)
			node = MlnHostNodes[current->id].firstChild;
		if (node >= 0) {
			do {
				result = MlnImpl_8106E57C(graph, node, attributes, &child);
				if (result != 0)
					goto out;
				if (child != NULL)
					position--;
				if (position < 0)
					goto found;
				node = MlnHostNodes[node].nextSibling;
				MlnImpl_8106CB78(child);
				child = NULL;
			} while (node >= 0);
		} else if (position < 0) {
			goto found;
		}
		if (current->getByIndex == NULL) {
			child = NULL;
		} else {
			result = current->getByIndex(graph, current, position, &child);
			if (result != 0)
				goto out;
		}
found:
		MlnImpl_8106CB78(current);
		current = child;
		child = NULL;
		path = next;
	}

	/* below a node the tree does not have: the objects' callbacks */
callbacks:
	if (c == '\0')
		goto value;
	if (current->type == MLN_HOST_OBJECT_CONTAINER && exotic != 1) {
		for (;;) {
			while (c == '/')
				c = *++path;
			length = 0;
			if (c != '\0') {
				do
					length++;
				while (path[length] != '/' && path[length] != '\0');
			}
			next = path + length;
			if (c != '@') {
				if (current->getByName == NULL)
					goto missing;
				result = current->getByName(graph, current, path, length, &child);
				if (result != 0)
					goto out;
			} else {
				if (length == 5 && (MlnMemcmp(path, "@Name", 5) == 0 ||
						    MlnMemcmp(path, "@Size", 5) == 0 ||
						    MlnMemcmp(path, "@Type", 5) == 0)) {
					c = *path;
					break;
				}
				if (length == 1) {
					result = 3;
					goto out;
				}
				position = path[1] - '0';
				if ((unsigned char)position > 9) {
					result = 3;
					goto out;
				}
				for (i = 2; i < length; i++) {
					digit = path[i] - '0';
					scaled = position * 10;
					position = scaled + digit;
					if ((unsigned char)digit > 9) {
						result = 3;
						goto out;
					}
					if (scaled > position)
						goto missing;
				}
				if (current->getByIndex == NULL)
					goto missing;
				result = current->getByIndex(graph, current, position, &child);
				if (result != 0)
					goto out;
			}
			MlnImpl_8106CB78(current);
			current = child;
			child = NULL;
			if (current == NULL)
				goto missing;
			path = next;
			c = *path;
			if (c == '\0')
				goto value;
			if (current->type != MLN_HOST_OBJECT_CONTAINER)
				break;
		}
	}

	/* a last "@Name", "@Size" or "@Type" */
	if (c == '\0' || exotic == 1)
		goto value;
	if (current->type != MLN_HOST_OBJECT_CONTAINER)
		c = *++path;
	if (c == '/' || c == '\0') {
		result = 3;
		goto out;
	}
	length = 0;
	do
		length++;
	while (path[length] != '/' && path[length] != '\0');
	if (path[length] != '\0' || length != 5) {
		result = 3;
		goto out;
	}
	if (MlnMemcmp(path, "@Name", 5) == 0) {
		data = current->name;
		if (data == NULL) {
			result = 3;
			goto out;
		}
		if (size < data->size + 1) {
			*type = -7;
			*outSize = data->size + 1;
			result = 1;
			goto out;
		}
		*type = MLN_HOST_OBJECT_STRING;
		*outSize = data->size + 1;
		MlnMemcpy(buffer, data->data, data->size);
		buffer[data->size] = '\0';
		goto out;
	}
	if (MlnMemcmp(path, "@Size", 5) == 0) {
		switch (current->type) {
		case MLN_HOST_OBJECT_CONTAINER:
			count = 0;
			if ((unsigned int)current->id < MLN_HOST_NODE_COUNT)
				node = MlnHostNodes[current->id].firstChild;
			while (node >= 0) {
				result = MlnImpl_8106E57C(graph, node, attributes, &child);
				if (result != 0)
					goto out;
				node = MlnHostNodes[node].nextSibling;
				if (child != NULL)
					count++;
				MlnImpl_8106CB78(child);
				child = NULL;
			}
			value = count + current->childCount;
			break;
		case MLN_HOST_OBJECT_INTEGER:
			value = 4;
			break;
		case MLN_HOST_OBJECT_STRING:
		case MLN_HOST_OBJECT_BYTES:
			value = current->value.buffer->size;
			break;
		default:
			value = position;
			break;
		}
		if (size < 4) {
			*type = -7;
			*outSize = 4;
			result = 1;
			goto out;
		}
		*type = MLN_HOST_OBJECT_INTEGER;
		*outSize = 4;
		MLN_HOST_PUT_WORD(buffer, value);
		goto out;
	}
	if (MlnMemcmp(path, "@Type", 5) == 0) {
		value = current->type;
		if (size < 4) {
			*type = -7;
			*outSize = 4;
			result = 1;
			goto out;
		}
		*type = MLN_HOST_OBJECT_INTEGER;
		*outSize = 4;
		MLN_HOST_PUT_WORD(buffer, value);
		goto out;
	}
	result = 3;
	goto out;

	/* the object's value */
value:
	switch (current->type) {
	case MLN_HOST_OBJECT_CONTAINER:
		if (size < 4)
			goto small;
		if (current->hasChildren == 1) {
			if (graph == NULL) {
				result = 3;
				goto out;
			}
			for (slot = 0; slot < MLN_HOST_SLOTS; slot++)
				if (graph->slots[slot].value == NULL)
					break;
			if (slot == MLN_HOST_SLOTS) {
				result = MLN_HOST_ERROR_NO_SLOT;
				goto out;
			}
			current->id = graph->slots[slot].id;
			graph->slots[slot].kind = MLN_HOST_SLOT_ATTRIBUTES;
			graph->slots[slot].value = current->value.container;
		}
		*type = MLN_HOST_OBJECT_CONTAINER;
		*outSize = 4;
		MLN_HOST_PUT_WORD(buffer, current->id);
		break;
	case MLN_HOST_OBJECT_INTEGER:
		value = current->value.integer;
		if (size < 4)
			goto small;
		*type = MLN_HOST_OBJECT_INTEGER;
		*outSize = 4;
		MLN_HOST_PUT_WORD(buffer, value);
		break;
	case MLN_HOST_OBJECT_STRING:
		data = current->value.buffer;
		if (data == NULL) {
			result = 3;
			break;
		}
		if (size < data->size + 1) {
			*type = -7;
			*outSize = data->size + 1;
			result = 1;
			break;
		}
		*type = MLN_HOST_OBJECT_STRING;
		*outSize = data->size + 1;
		MlnMemcpy(buffer, data->data, data->size);
		buffer[data->size] = '\0';
		break;
	case MLN_HOST_OBJECT_BYTES:
		data = current->value.buffer;
		if (data == NULL) {
			result = 3;
			break;
		}
		if (size < data->size) {
			*type = -7;
			*outSize = data->size;
			result = 1;
			break;
		}
		*type = MLN_HOST_OBJECT_BYTES;
		*outSize = data->size;
		MlnMemcpy(buffer, data->data, data->size);
		break;
	default:
		result = 5;
		break;
	}
	goto out;

small:
	*type = -7;
	*outSize = 4;
	result = 1;
	goto out;
missing:
	*type = -6;
	*outSize = 0;
	result = 1;
out:
	MlnImpl_8106CB78(current);
	MlnImpl_8106CB78(child);
	return result;
}

/*
 * System.Host.SetObject: an object of the database set, at a path below the
 * node of an index; only the database's objects can be set. The result is 0,
 * or 1 with -6 (not found), -3 (not writable), -8 (not allowed) or -2.
 */
unsigned int MlnImpl_81070318(MlnCoreHost *host, int index, const char *path,
			      const unsigned char *data, unsigned int size,
			      const unsigned char *signer, unsigned int signerSize, int type,
			      int *result)
{
	MlnHostGraph *graph = host->graph;
	int dbIndex = 0;
	const char *dbPath = NULL;
	unsigned int ret;

	if (graph == NULL || path == NULL)
		goto fail;
	if (*path == '/')
		index = 0;
	if (!MlnImpl_8106CBC4(index, path, &dbIndex, &dbPath))
		goto fail;
	ret = MlnImpl_8107C5F0(graph->records, signer, signerSize, dbIndex, dbPath, data, size,
			       type);
	if (ret == 0x230B0011) {
		*result = -6;
		return 1;
	}
	if (ret == 0x230B0012) {
		*result = -3;
		return 1;
	}
	if (ret == 0x230B0013) {
		*result = -8;
		return 1;
	}
	if (ret == 0) {
		*result = 0;
		return 0;
	}
fail:
	*result = -2;
	return 1;
}

/*
 * Octopus.Links.IsNodeReachable: whether the node of a uid is reached from
 * the personality over links whose constraints hold. For each link on the way
 * that carries a control, the control's program is loaded and its
 * Control.Link.Constraint.Init, Check and Release are run; a Check that gives
 * a status, no result or a false one fails the way. *result is 0 when one way
 * holds, -1 when none does.
 */
unsigned int MlnImpl_81070424(MlnCoreHost *host, unsigned int size, const void *uid,
			      int *result)
{
	MlnHostLinks *links;
	MlnHostLinkTarget *target;
	MlnOctopusControl *control;
	MlnVm *vm;
	unsigned int value;
	unsigned int address;
	MlnBuffer *answer;
	unsigned int type;
	unsigned int ret;
	unsigned int check;
	unsigned int r;
	int count;
	int i;

	if (host == NULL || host->graph == NULL) {
		*result = -1;
		return 4;
	}
	links = ((MlnHostGraph *)host->graph)->links;
	if (links == NULL)
		goto none;
	count = links->targets->count;
	if (count <= 0)
		goto none;
	for (i = 0; i < count; i++) {
		target = links->targets->items[i];
		if (MlnBytesCompare(MLN_HEAP_GENERAL, target->uid->size, target->uid->data, size,
				    uid) != 0)
			continue;
		for (; target->from != NULL; target = target->from) {
			control = target->control;
			if (control == NULL)
				continue;
			vm = NULL;
			answer = NULL;
			ret = MlnImpl_8106C54C(0x8000, host, &vm);
			if (ret != 0)
				goto release;
			ret = MlnImpl_8106C6F8(vm, control->codeModule, control->uid,
					       control->protocol, control->type, control->signer,
					       control->attributes);
			if (ret != 0)
				goto release;
			ret = MlnImpl_8106C788(vm, control->uid);
			if (ret != 0)
				goto release;
			r = MlnImpl_8106C794(vm, "Control.Link.Constraint.Init");
			if (r == 0) {
				ret = MlnImpl_8106C970(vm, &value);
				if (ret != 0)
					goto release;
				if (value != 0) {
					ret = 1;
					goto release;
				}
			} else if (r != MLN_VM_ERROR_NO_ROUTINE) {
				ret = 1;
				goto release;
			}
			check = MlnImpl_8106C794(vm, "Control.Link.Constraint.Check");
			if (check == 0) {
				check = MlnImpl_8106C970(vm, &value);
				if (check == 0) {
					if (value != 0) {
						check = 1;
					} else {
						check = MlnImpl_8106C970(vm, &address);
						if (check == 0) {
							if (address == 0) {
								check = 1;
							} else {
								check = MlnImpl_8106C9F4(vm, address,
											 &answer,
											 &type);
								if (check == 0)
									check = type != 0;
							}
						}
					}
				}
			}
			if (vm == NULL) {
				ret = check;
				goto destroy;
			}
			r = MlnImpl_8106C794(vm, "Control.Link.Constraint.Release");
			if (r != MLN_VM_ERROR_NO_ROUTINE) {
				if (r != 0) {
					ret = 1;
				} else {
					ret = MlnImpl_8106C970(vm, &value);
					if (ret == 0)
						ret = value != 0;
				}
			}
			if (check != 0)
				ret = check;
			MlnImpl_8106C6C4(vm);
			vm = NULL;
			goto destroy;
release:
			if (vm != NULL) {
				MlnImpl_8106C6C4(vm);
				vm = NULL;
			}
destroy:
			if (answer != NULL)
				MlnBufferDestroy(MLN_HEAP_GENERAL, answer);
			if (ret != 0)
				goto next;
		}
		*result = 0;
		return 0;
next:
		;
	}
none:
	*result = -1;
	return 0;
}

/*
 * The SeaShell database made anew from its saved state, a pair of buffers:
 * the database and the pending changes. Without a pair, from what is stored.
 */
unsigned int MlnImpl_810706F4(MlnHostGraph *graph, MlnArray *pair)
{
	unsigned char *data = NULL;
	unsigned int size = 0;
	MlnBuffer *state;
	MlnBuffer *pending;
	unsigned int pendingSize;
	unsigned char *pendingData;
	unsigned int ret;

	if (graph == NULL) {
		ret = 4;
		goto out;
	}
	if (pair == NULL) {
		ret = MlnImpl_8107B228(NULL, &size, &data);
		if (ret != 0)
			goto out;
		pendingSize = 0;
		pendingData = NULL;
	} else {
		if (pair->count != 2)
			return 3;
		state = pair->items[0];
		if (state == NULL)
			return 3;
		pending = pair->items[1];
		size = state->size;
		data = state->data;
		if (pending != NULL) {
			pendingSize = pending->size;
			pendingData = pending->data;
		} else {
			pendingSize = 0;
			pendingData = NULL;
		}
	}
	if (graph->records != NULL) {
		ret = MlnImpl_8107B4A0(graph->records);
		if (ret != 0)
			goto out;
		graph->records = NULL;
	}
	ret = MlnImpl_8107B450(&graph->records);
	if (ret != 0)
		goto out;
	ret = MlnImpl_8107B534(graph->records, size, data);
	if (ret != 0)
		goto out;
	if (pendingSize != 0)
		ret = MlnImpl_8107B730(graph->records, pendingSize, pendingData);
out:
	if (pair == NULL)
		MlnFree(MLN_HEAP_GENERAL, data);
	return ret;
}

/* the SeaShell database saved into a pair of buffers, and closed */
unsigned int MlnImpl_81070808(MlnHostGraph *graph, MlnArray **pair)
{
	unsigned int size = 0;
	unsigned char *data = NULL;
	unsigned int words[2] = { 0, 0 };
	MlnBuffer *buffer = NULL;
	MlnArray *array = NULL;
	unsigned int ret;

	if (graph == NULL || graph->records == NULL) {
		ret = 4;
		goto out;
	}
	if (pair == NULL) {
		ret = 3;
		goto out;
	}
	ret = MlnImpl_8107BA3C(graph->records, 0, &size, &data);
	if (ret != 0)
		goto out;
	buffer = MlnBufferCreate(MLN_HEAP_GENERAL, size, data);
	if (buffer == NULL) {
		ret = 2;
		goto out;
	}
	words[0] = (unsigned int)buffer;
	array = MlnWordArrayCreate(MLN_HEAP_GENERAL, 2, words);
	if (array == NULL) {
		ret = 2;
		goto out;
	}
	buffer = NULL;
	ret = MlnImpl_8107B4A0(graph->records);
	if (ret != 0)
		goto out;
	graph->records = NULL;
	*pair = array;
	array = NULL;
out:
	MlnArrayDestroy(MLN_HEAP_GENERAL, array);
	MlnFree(MLN_HEAP_GENERAL, data);
	MlnBufferDestroy(MLN_HEAP_GENERAL, buffer);
	return ret;
}
