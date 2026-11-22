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
