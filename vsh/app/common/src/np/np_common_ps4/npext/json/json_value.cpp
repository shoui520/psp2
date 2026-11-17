/*
 * SceNpCommonPs4: JSON values (npext/json/json_value.cpp): a value of one of six types (null, bool, number,
 * string, array, object) that owns its data, and the containers of arrays and objects.
 */

#include "../../np_common_ps4_internal.h"

#define NP_COMMON_PS4_JSON_DESTROY(value) (((void (*)(void *))(*(const void *const **)(value))[1])(value))

extern "C" {

extern const void *g_jsonValueVtable[];		/* 0x8102a7d8 */
extern const void *g_jsonItemArrayVtable[];	/* 0x8102a8b0 */
extern const void *g_jsonFieldSetVtable[];	/* 0x8102a8c0 */
extern const void *g_jsonBoolDataVtable[];	/* 0x8102a880 */
extern const void *g_jsonTextDataVtable[];	/* 0x8102a890 */
extern const void *g_jsonStringDataVtable[];	/* 0x8102a8a0 */

__attribute__((noinline)) struct NpCommonPs4JsonValue *SceNpCommonPs4_B83E1A6A(struct NpCommonPs4JsonValue *value)
{
	void *data;

	value->vtable = NP_COMMON_PS4_VTABLE(g_jsonValueVtable);
	switch (value->type) {
	case NP_COMMON_PS4_JSON_TYPE_NULL:
		break;
	case NP_COMMON_PS4_JSON_TYPE_BOOL:
		data = value->data;
		if (data != NULL)
			NP_COMMON_PS4_JSON_DESTROY(data);
		break;
	case NP_COMMON_PS4_JSON_TYPE_NUMBER:
		data = value->data;
		if (data != NULL)
			NP_COMMON_PS4_JSON_DESTROY(data);
		break;
	case NP_COMMON_PS4_JSON_TYPE_STRING:
		data = value->data;
		if (data != NULL)
			NP_COMMON_PS4_JSON_DESTROY(data);
		break;
	case NP_COMMON_PS4_JSON_TYPE_ARRAY:
		data = value->data;
		if (data != NULL)
			NP_COMMON_PS4_JSON_DESTROY(data);
		break;
	case NP_COMMON_PS4_JSON_TYPE_OBJECT:
		data = value->data;
		if (data != NULL)
			NP_COMMON_PS4_JSON_DESTROY(data);
		break;
	default:
		break;
	}
	value->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return value;
}

int SceNpCommonPs4_62759A9A(struct NpCommonPs4JsonValue *value)
{
	SceNpCommonPs4_B83E1A6A(value);
	return SceNpCommonPs4_BB5282C8(value);
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_F9AC735E(struct NpCommonPs4JsonValue *value)
{
	return SceNpCommonPs4_B83E1A6A(value);
}

__attribute__((noinline)) struct NpCommonPs4JsonValue *SceNpCommonPs4_935F32AE(struct NpCommonPs4JsonValue *value, struct NpCommonPs4Allocator *pool,
	int type)
{
	value->pool = pool;
	value->vtable = NP_COMMON_PS4_VTABLE(g_jsonValueVtable);
	value->type = type;
	value->parent = NULL;
	switch (type) {
	case NP_COMMON_PS4_JSON_TYPE_NULL:
		break;
	case NP_COMMON_PS4_JSON_TYPE_BOOL:
		value->data = NULL;
		break;
	case NP_COMMON_PS4_JSON_TYPE_NUMBER:
		value->data = NULL;
		break;
	case NP_COMMON_PS4_JSON_TYPE_STRING:
		value->data = NULL;
		break;
	case NP_COMMON_PS4_JSON_TYPE_ARRAY:
		value->data = NULL;
		break;
	case NP_COMMON_PS4_JSON_TYPE_OBJECT:
		value->data = NULL;
		break;
	default:
		return value;
	}
	return value;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_D309C0B3(struct NpCommonPs4JsonValue *value, struct NpCommonPs4Allocator *pool,
	int type)
{
	return SceNpCommonPs4_935F32AE(value, pool, type);
}

__attribute__((noinline)) struct NpCommonPs4JsonListNode *SceNpCommonPs4_A908E5EA(struct NpCommonPs4JsonItemList *list,
	struct NpCommonPs4Allocator *pool, struct NpCommonPs4JsonValue *value)
{
	struct NpCommonPs4JsonListNode *node;

	NP_COMMON_PS4_ASSERT(value != NULL, "itemArrayAddValue", "npext\\json\\json_value.cpp", 552, "value != 0");
	node = (struct NpCommonPs4JsonListNode *)SceNpCommonPs4_299ADFC3(pool, 12);
	if (node == NULL)
		return NULL;
	node->value = NULL;
	node->next = NULL;
	node->prev = NULL;
	node->value = value;
	node->next = NULL;
	node->prev = list->tail;
	if (list->tail != NULL)
		list->tail->next = node;
	if (list->head == NULL)
		list->head = node;
	list->tail = node;
	return node;
}

int SceNpCommonPs4_67F61239(struct NpCommonPs4JsonItemArray *array, struct NpCommonPs4JsonValue *value,
	struct NpCommonPs4JsonValue *parent)
{
	struct NpCommonPs4JsonListNode *node;

	node = SceNpCommonPs4_A908E5EA(&array->list, array->pool, value);
	if (node == NULL)
		return ((int *)array->pool)[4];
	node->value->parent = parent;
	return 0;
}

int SceNpCommonPs4_413D4254(struct NpCommonPs4JsonValue *array, struct NpCommonPs4JsonValue *item)
{
	struct NpCommonPs4JsonItemArray *items;
	struct NpCommonPs4JsonItemArray *memory;
	struct NpCommonPs4JsonListNode *node;
	int error;

	NP_COMMON_PS4_ASSERT(array->type == NP_COMMON_PS4_JSON_TYPE_ARRAY, "AddItemValue", "npext\\json\\json_value.cpp", 931,
		"m_type == TYPE_ARRAY");
	items = (struct NpCommonPs4JsonItemArray *)array->data;
	if (items == NULL) {
		memory = (struct NpCommonPs4JsonItemArray *)SceNpCommonPs4_D154D84F(16, array->pool);
		if (memory == NULL)
			return ((int *)array->pool)[4];
		memory->pool = array->pool;
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonItemArrayVtable);
		memory->list.head = NULL;
		items = memory;
		memory->list.tail = NULL;
		array->data = memory;
	}
	node = SceNpCommonPs4_A908E5EA(&items->list, items->pool, item);
	if (node != NULL) {
		error = 0;
		node->value->parent = array;
	} else {
		error = ((int *)items->pool)[4];
	}
	if (error >= 0)
		return 0;
	return error;
}

void SceNpCommonPs4_nullsub_8(void)
{
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_5607B8B3(struct NpCommonPs4JsonValue *array, int index)
{
	struct NpCommonPs4JsonItemArray *items;
	struct NpCommonPs4JsonListNode *node;
	struct NpCommonPs4JsonValue *result;
	int count;
	static const char functionName[] = "GetItemValue";

	NP_COMMON_PS4_ASSERT(array->type == NP_COMMON_PS4_JSON_TYPE_ARRAY, functionName, "npext\\json\\json_value.cpp", 960,
		"m_type == TYPE_ARRAY");
	items = (struct NpCommonPs4JsonItemArray *)array->data;
	result = NULL;
	if (items != NULL) {
		count = index;
		node = items->list.head;
		if (node != NULL) {
			while (count != 0) {
				node = node->next;
				count--;
				if (node == NULL)
					return result;
			}
			return node->value;
		}
	}
	return result;
}

__attribute__((noinline)) struct NpCommonPs4JsonValue *SceNpCommonPs4_2A4A268C(struct NpCommonPs4JsonValue *array, int index)
{
	struct NpCommonPs4JsonItemArray *items;
	struct NpCommonPs4JsonListNode *node;
	struct NpCommonPs4JsonValue *result;
	int count;
	static const char functionName[] = "GetItemValue";

	NP_COMMON_PS4_ASSERT(array->type == NP_COMMON_PS4_JSON_TYPE_ARRAY, functionName, "npext\\json\\json_value.cpp", 973,
		"m_type == TYPE_ARRAY");
	items = (struct NpCommonPs4JsonItemArray *)array->data;
	result = NULL;
	if (items != NULL) {
		count = index;
		node = items->list.head;
		if (node != NULL) {
			while (count != 0) {
				node = node->next;
				count--;
				if (node == NULL)
					return result;
			}
			return node->value;
		}
	}
	return result;
}

void SceNpCommonPs4_EA7C55D4(struct NpCommonPs4Allocator *pool, struct NpCommonPs4JsonField *field)
{
	if (field != NULL) {
		SceNpCommonPs4_38CC9AF5(pool, field->name);
		if (field->value != NULL)
			NP_COMMON_PS4_JSON_DESTROY(field->value);
		SceNpCommonPs4_38CC9AF5(pool, field);
	}
}

struct NpCommonPs4JsonField *SceNpCommonPs4_1DD370B6(struct NpCommonPs4Allocator *pool, const char *name)
{
	struct NpCommonPs4JsonField *field;
	char *duplicate;

	field = (struct NpCommonPs4JsonField *)SceNpCommonPs4_299ADFC3(pool, 16);
	if (field != NULL) {
		field->name = NULL;
		field->value = NULL;
		field->next = NULL;
		field->prev = NULL;
		duplicate = npCommonPs4AllocatorDuplicateString(pool, name);
		field->name = duplicate;
		if (duplicate != NULL)
			return field;
		SceNpCommonPs4_38CC9AF5(pool, field->name);
		if (field->value != NULL)
			NP_COMMON_PS4_JSON_DESTROY(field->value);
		SceNpCommonPs4_38CC9AF5(pool, field);
	}
	return NULL;
}

struct NpCommonPs4JsonField *SceNpCommonPs4_332AF121(struct NpCommonPs4JsonFieldList *list,
	struct NpCommonPs4Allocator *pool, const char *name, struct NpCommonPs4JsonValue *value)
{
	struct NpCommonPs4JsonField *field;
	char *duplicate;

	NP_COMMON_PS4_ASSERT(value != NULL, "fieldSetAddField", "npext\\json\\json_value.cpp", 837, "value != 0");
	field = (struct NpCommonPs4JsonField *)SceNpCommonPs4_299ADFC3(pool, 16);
	if (field == NULL)
		return NULL;
	field->name = NULL;
	field->value = NULL;
	field->next = NULL;
	field->prev = NULL;
	duplicate = npCommonPs4AllocatorDuplicateString(pool, name);
	field->name = duplicate;
	if (duplicate == NULL) {
		SceNpCommonPs4_38CC9AF5(pool, field->name);
		if (field->value != NULL)
			NP_COMMON_PS4_JSON_DESTROY(field->value);
		SceNpCommonPs4_38CC9AF5(pool, field);
		return NULL;
	}
	field->value = value;
	field->next = NULL;
	field->prev = list->tail;
	if (list->tail != NULL)
		list->tail->next = field;
	if (list->head == NULL)
		list->head = field;
	list->tail = field;
	return field;
}

struct NpCommonPs4JsonField *SceNpCommonPs4_4134CFB3(struct NpCommonPs4JsonFieldList *list, const char *name)
{
	struct NpCommonPs4JsonField *field;

	field = list->head;
	if (field == NULL)
		return NULL;
	while (sceClibStrncmp(field->name, name, sceClibStrnlen(name, 0xFFFFFFFF)) != 0) {
		field = field->next;
		if (field == NULL)
			return NULL;
	}
	return field;
}

int SceNpCommonPs4_D467A07C(struct NpCommonPs4JsonFieldSet *fields, const char *name, struct NpCommonPs4JsonValue *value,
	struct NpCommonPs4JsonValue *parent)
{
	struct NpCommonPs4JsonField *field;
	int error;

	field = fields->list.head;
	if (field != NULL) {
		while (sceClibStrncmp(field->name, name, sceClibStrnlen(name, 0xFFFFFFFF)) != 0) {
			field = field->next;
			if (field == NULL)
				goto add;
		}
		if (field->value != NULL)
			NP_COMMON_PS4_JSON_DESTROY(field->value);
		field->value = value;
	} else {
add:
		if (SceNpCommonPs4_332AF121(&fields->list, fields->pool, name, value) == NULL) {
			error = ((int *)fields->pool)[4];
			operator delete(NULL);
			return error;
		}
	}
	value->parent = parent;
	return 0;
}

struct NpCommonPs4JsonFieldList *SceNpCommonPs4_C3511483(struct NpCommonPs4JsonFieldList *list)
{
	list->head = NULL;
	list->tail = NULL;
	return list;
}

struct NpCommonPs4JsonFieldSet *SceNpCommonPs4_627506F5(struct NpCommonPs4JsonFieldSet *fields,
	struct NpCommonPs4Allocator *pool)
{
	fields->pool = pool;
	fields->vtable = NP_COMMON_PS4_VTABLE(g_jsonFieldSetVtable);
	fields->list.tail = NULL;
	fields->list.head = NULL;
	return fields;
}

int SceNpCommonPs4_7833EEC2(struct NpCommonPs4JsonValue *object, const char *name, struct NpCommonPs4JsonValue *value)
{
	struct NpCommonPs4JsonFieldSet *fields;
	int result;

	NP_COMMON_PS4_ASSERT(object->type == NP_COMMON_PS4_JSON_TYPE_OBJECT, "SetFieldValue", "npext\\json\\json_value.cpp", 986,
		"m_type == TYPE_OBJECT");
	fields = (struct NpCommonPs4JsonFieldSet *)object->data;
	if (fields == NULL) {
		fields = (struct NpCommonPs4JsonFieldSet *)SceNpCommonPs4_D154D84F(16, object->pool);
		if (fields == NULL)
			return ((int *)object->pool)[4];
		fields->pool = object->pool;
		fields->vtable = NP_COMMON_PS4_VTABLE(g_jsonFieldSetVtable);
		fields->list.head = NULL;
		fields->list.tail = NULL;
		object->data = fields;
	}
	result = SceNpCommonPs4_D467A07C(fields, name, value, object);
	if (result >= 0)
		return 0;
	return result;
}

struct NpCommonPs4JsonField *SceNpCommonPs4_44ECC233(struct NpCommonPs4JsonFieldList *list, int index)
{
	struct NpCommonPs4JsonField *field;

	field = list->head;
	if (field == NULL)
		return field;
	while (index != 0) {
		field = field->next;
		index--;
		if (field == NULL)
			return NULL;
	}
	return field;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_498A2E51(struct NpCommonPs4JsonFieldSet *fields, int index, char **pName)
{
	struct NpCommonPs4JsonField *field;

	field = fields->list.head;
	if (field == NULL)
		return NULL;
	while (index != 0) {
		field = field->next;
		index--;
		if (field == NULL)
			return NULL;
	}
	if (pName != NULL)
		*pName = field->name;
	return field->value;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_0005DF9E(struct NpCommonPs4JsonValue *object, int index, char **pName)
{
	struct NpCommonPs4JsonFieldSet *fields;
	struct NpCommonPs4JsonField *field;
	struct NpCommonPs4JsonValue *result;
	int count;

	static const char functionName[] = "GetFieldValue";

	NP_COMMON_PS4_ASSERT(object->type == NP_COMMON_PS4_JSON_TYPE_OBJECT, functionName, "npext\\json\\json_value.cpp", 1015,
		"m_type == TYPE_OBJECT");
	fields = (struct NpCommonPs4JsonFieldSet *)object->data;
	result = NULL;
	if (fields != NULL) {
		count = index;
		field = fields->list.head;
		if (field != NULL) {
			while (count != 0) {
				field = field->next;
				count--;
				if (field == NULL)
					return result;
			}
			if (pName != NULL)
				*pName = field->name;
			return field->value;
		}
	}
	return result;
}

__attribute__((noinline)) struct NpCommonPs4JsonValue *SceNpCommonPs4_713C8A08(struct NpCommonPs4JsonValue *object, int index, char **pName)
{
	struct NpCommonPs4JsonFieldSet *fields;
	struct NpCommonPs4JsonField *field;
	struct NpCommonPs4JsonValue *result;
	int count;

	static const char functionName[] = "GetFieldValue";

	NP_COMMON_PS4_ASSERT(object->type == NP_COMMON_PS4_JSON_TYPE_OBJECT, functionName, "npext\\json\\json_value.cpp", 1028,
		"m_type == TYPE_OBJECT");
	fields = (struct NpCommonPs4JsonFieldSet *)object->data;
	result = NULL;
	if (fields != NULL) {
		count = index;
		field = fields->list.head;
		if (field != NULL) {
			while (count != 0) {
				field = field->next;
				count--;
				if (field == NULL)
					return result;
			}
			if (pName != NULL)
				*pName = field->name;
			return field->value;
		}
	}
	return result;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_F1F2B46E(struct NpCommonPs4JsonFieldSet *fields, const char *name)
{
	struct NpCommonPs4JsonField *field;

	field = fields->list.head;
	if (field == NULL)
		return NULL;
	while (sceClibStrncmp(field->name, name, sceClibStrnlen(name, 0xFFFFFFFF)) != 0) {
		field = field->next;
		if (field == NULL)
			return NULL;
	}
	return field->value;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_3F3A476E(struct NpCommonPs4JsonValue *object, const char *name)
{
	struct NpCommonPs4JsonFieldSet *fields;
	struct NpCommonPs4JsonField *field;
	struct NpCommonPs4JsonValue *result;

	static const char functionName[] = "GetFieldValue";

	NP_COMMON_PS4_ASSERT(object->type == NP_COMMON_PS4_JSON_TYPE_OBJECT, functionName, "npext\\json\\json_value.cpp", 1041,
		"m_type == TYPE_OBJECT");
	fields = (struct NpCommonPs4JsonFieldSet *)object->data;
	result = NULL;
	if (fields != NULL) {
		field = fields->list.head;
		if (field != NULL) {
			while (sceClibStrncmp(field->name, name, sceClibStrnlen(name, 0xFFFFFFFF)) != 0) {
				field = field->next;
				if (field == NULL)
					return result;
			}
			return field->value;
		}
	}
	return result;
}

__attribute__((noinline)) struct NpCommonPs4JsonValue *SceNpCommonPs4_428DE8A6(struct NpCommonPs4JsonValue *object, const char *name)
{
	struct NpCommonPs4JsonFieldSet *fields;
	struct NpCommonPs4JsonField *field;
	struct NpCommonPs4JsonValue *result;

	static const char functionName[] = "GetFieldValue";

	NP_COMMON_PS4_ASSERT(object->type == NP_COMMON_PS4_JSON_TYPE_OBJECT, functionName, "npext\\json\\json_value.cpp", 1054,
		"m_type == TYPE_OBJECT");
	fields = (struct NpCommonPs4JsonFieldSet *)object->data;
	result = NULL;
	if (fields != NULL) {
		field = fields->list.head;
		if (field != NULL) {
			while (sceClibStrncmp(field->name, name, sceClibStrnlen(name, 0xFFFFFFFF)) != 0) {
				field = field->next;
				if (field == NULL)
					return result;
			}
			return field->value;
		}
	}
	return result;
}

struct NpCommonPs4JsonValue *npCommonPs4JsonValueAsBool(struct NpCommonPs4JsonValue *value)
{
	struct NpCommonPs4JsonValue *result;

	result = NULL;
	if (value->type == NP_COMMON_PS4_JSON_TYPE_BOOL)
		result = value;
	return result;
}

struct NpCommonPs4JsonValue *npCommonPs4JsonValueAsNumber(struct NpCommonPs4JsonValue *value)
{
	struct NpCommonPs4JsonValue *result;

	result = NULL;
	if (value->type == NP_COMMON_PS4_JSON_TYPE_NUMBER)
		result = value;
	return result;
}

struct NpCommonPs4JsonValue *npCommonPs4JsonValueAsString(struct NpCommonPs4JsonValue *value)
{
	struct NpCommonPs4JsonValue *result;

	result = NULL;
	if (value->type == NP_COMMON_PS4_JSON_TYPE_STRING)
		result = value;
	return result;
}

struct NpCommonPs4JsonValue *npCommonPs4JsonValueAsArray(struct NpCommonPs4JsonValue *value)
{
	struct NpCommonPs4JsonValue *result;

	result = NULL;
	if (value->type == NP_COMMON_PS4_JSON_TYPE_ARRAY)
		result = value;
	return result;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_15A2D6E3(struct NpCommonPs4JsonValue *value)
{
	struct NpCommonPs4JsonValue *result;

	result = NULL;
	if (value->type == NP_COMMON_PS4_JSON_TYPE_OBJECT)
		result = value;
	return result;
}

void SceNpCommonPs4_31619280(void)
{
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_2D0351A2(struct NpCommonPs4JsonValue *value, struct NpCommonPs4Allocator *pool)
{
	struct NpCommonPs4JsonValue *copy;

	copy = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, pool);
	if (copy == NULL)
		return NULL;
	SceNpCommonPs4_935F32AE(copy, pool, NP_COMMON_PS4_JSON_TYPE_NULL);
	copy->vtable = NP_COMMON_PS4_VTABLE(g_jsonNullValueVtable);
	return copy;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_A6BD2963(struct NpCommonPs4JsonValue *value, struct NpCommonPs4Allocator *pool)
{
	struct NpCommonPs4JsonValue *copy;

	copy = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, pool);
	if (copy == NULL)
		return NULL;
	SceNpCommonPs4_935F32AE(copy, pool, NP_COMMON_PS4_JSON_TYPE_NULL);
	copy->vtable = NP_COMMON_PS4_VTABLE(g_jsonNullValueVtable);
	return copy;
}

int SceNpCommonPs4_FF11808F(struct NpCommonPs4JsonValue *value)
{
	void *data;

	data = value->data;
	if (data != NULL)
		NP_COMMON_PS4_JSON_DESTROY(data);
	value->data = NULL;
	return 0;
}

struct NpCommonPs4JsonBoolData *SceNpCommonPs4_D997BBF9(struct NpCommonPs4JsonBoolData *data)
{
	data->vtable = NP_COMMON_PS4_VTABLE(g_jsonBoolDataVtable);
	data->value = 0;
	return data;
}

struct NpCommonPs4JsonBoolData *SceNpCommonPs4_64A4C862(struct NpCommonPs4JsonBoolData *data, struct NpCommonPs4Allocator *pool)
{
	struct NpCommonPs4JsonBoolData *copy;

	copy = (struct NpCommonPs4JsonBoolData *)SceNpCommonPs4_D154D84F(8, pool);
	if (copy != NULL) {
		copy->value = 0;
		copy->vtable = NP_COMMON_PS4_VTABLE(g_jsonBoolDataVtable);
		copy->value = data->value;
	}
	return NULL;
}

__attribute__((noinline)) struct NpCommonPs4JsonValue *SceNpCommonPs4_50BC0275(struct NpCommonPs4JsonValue *value, struct NpCommonPs4Allocator *pool)
{
	struct NpCommonPs4JsonValue *result;
	struct NpCommonPs4JsonValue *copy;
	struct NpCommonPs4JsonBoolData *data;
	struct NpCommonPs4JsonBoolData *dataCopy;

	result = NULL;
	copy = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, pool);
	if (copy != NULL) {
		SceNpCommonPs4_935F32AE(copy, pool, NP_COMMON_PS4_JSON_TYPE_BOOL);
		copy->vtable = NP_COMMON_PS4_VTABLE(g_jsonBoolValueVtable);
		data = (struct NpCommonPs4JsonBoolData *)value->data;
		if (data == NULL)
			return copy;
		dataCopy = (struct NpCommonPs4JsonBoolData *)SceNpCommonPs4_D154D84F(8, pool);
		if (dataCopy != NULL) {
			dataCopy->value = 0;
			dataCopy->vtable = NP_COMMON_PS4_VTABLE(g_jsonBoolDataVtable);
			dataCopy->value = data->value;
		}
		copy->data = result;
		NP_COMMON_PS4_JSON_DESTROY(copy);
	}
	return result;
}

int SceNpCommonPs4_C6A17ECD(struct NpCommonPs4JsonBoolData *data, char boolean)
{
	data->value = boolean;
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_AE224009(struct NpCommonPs4JsonValue *value, SceUInt8 boolean)
{
	struct NpCommonPs4JsonBoolData *data;

	NP_COMMON_PS4_ASSERT(value->type == NP_COMMON_PS4_JSON_TYPE_BOOL, "SetBool", "npext\\json\\json_value.cpp", 1217,
		"m_type == TYPE_BOOL");
	data = (struct NpCommonPs4JsonBoolData *)value->data;
	if (data != NULL) {
		data->value = boolean;
	} else {
		data = (struct NpCommonPs4JsonBoolData *)SceNpCommonPs4_D154D84F(8, value->pool);
		if (data == NULL)
			return ((int *)value->pool)[4];
		data->vtable = NP_COMMON_PS4_VTABLE(g_jsonBoolDataVtable);
		data->value = 0;
		data->value = boolean;
		value->data = data;
	}
	return 0;
}

int SceNpCommonPs4_C6BD1713(struct NpCommonPs4JsonBoolData *data)
{
	return data->value;
}

int SceNpCommonPs4_C1277FAD(struct NpCommonPs4JsonValue *value)
{
	struct NpCommonPs4JsonBoolData *data;
	int result;

	NP_COMMON_PS4_ASSERT(value->type == NP_COMMON_PS4_JSON_TYPE_BOOL, "GetBool", "npext\\json\\json_value.cpp", 1255,
		"m_type == TYPE_BOOL");
	data = (struct NpCommonPs4JsonBoolData *)value->data;
	result = 0;
	if (data != NULL)
		return data->value;
	return result;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_D3B1E97E(struct NpCommonPs4JsonValue *value, struct NpCommonPs4Allocator *pool)
{
	return SceNpCommonPs4_50BC0275(value, pool);
}

int SceNpCommonPs4_DD94C5AE(struct NpCommonPs4JsonValue *value)
{
	void *data;

	data = value->data;
	if (data != NULL)
		NP_COMMON_PS4_JSON_DESTROY(data);
	value->data = NULL;
	return 0;
}

struct NpCommonPs4JsonTextData *SceNpCommonPs4_BD5F84AD(struct NpCommonPs4JsonTextData *data, struct NpCommonPs4Allocator *pool)
{
	data->pool = pool;
	data->vtable = NP_COMMON_PS4_VTABLE(g_jsonTextDataVtable);
	data->text = NULL;
	return data;
}

struct NpCommonPs4JsonTextData *SceNpCommonPs4_C04D5BD5(struct NpCommonPs4JsonTextData *data, struct NpCommonPs4Allocator *pool)
{
	struct NpCommonPs4JsonTextData *copy;

	copy = (struct NpCommonPs4JsonTextData *)SceNpCommonPs4_D154D84F(12, pool);
	if (copy != NULL) {
		copy->pool = pool;
		copy->vtable = NP_COMMON_PS4_VTABLE(g_jsonTextDataVtable);
		copy->text = NULL;
		copy->text = npCommonPs4AllocatorDuplicateString(pool, data->text);
		if (copy->text != NULL)
			return copy;
		NP_COMMON_PS4_JSON_DESTROY(copy);
	}
	return NULL;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_33A6B1DF(struct NpCommonPs4JsonValue *value, struct NpCommonPs4Allocator *pool)
{
	struct NpCommonPs4JsonValue *result;
	struct NpCommonPs4JsonValue *copy;
	struct NpCommonPs4JsonTextData *data;
	struct NpCommonPs4JsonTextData *dataCopy;

	result = NULL;
	copy = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, pool);
	if (copy == NULL)
		return result;
	SceNpCommonPs4_935F32AE(copy, pool, NP_COMMON_PS4_JSON_TYPE_NUMBER);
	copy->vtable = NP_COMMON_PS4_VTABLE(g_jsonNumberValueVtable);
	data = (struct NpCommonPs4JsonTextData *)value->data;
	if (data == NULL)
		return copy;
	dataCopy = (struct NpCommonPs4JsonTextData *)SceNpCommonPs4_D154D84F(12, pool);
	if (dataCopy != NULL) {
		dataCopy->pool = pool;
		dataCopy->text = NULL;
		dataCopy->vtable = NP_COMMON_PS4_VTABLE(g_jsonTextDataVtable);
		dataCopy->text = npCommonPs4AllocatorDuplicateString(pool, data->text);
		if (dataCopy->text == NULL) {
			NP_COMMON_PS4_JSON_DESTROY(dataCopy);
			dataCopy = NULL;
		}
	}
	copy->data = dataCopy;
	if (dataCopy != NULL)
		return copy;
	NP_COMMON_PS4_JSON_DESTROY(copy);
	return result;
}

int SceNpCommonPs4_E595BA30(struct NpCommonPs4JsonTextData *data, SceInt32 number)
{
	char *text;
	int result;
	SceSize length;

	result = SceNpCommonPs4_9CAF0FE7(NULL, 0, number, 10, 0, &length);
	if (result >= 0) {
		text = (char *)SceNpCommonPs4_299ADFC3(data->pool, length + 1);
		if (text != NULL) {
			result = SceNpCommonPs4_9CAF0FE7(text, length + 1, number, 10, 0, NULL);
			if (result >= 0) {
				SceNpCommonPs4_38CC9AF5(data->pool, data->text);
				data->text = text;
				result = 0;
			}
		} else {
			result = ((int *)data->pool)[4];
		}
	}
	return result;
}

int SceNpCommonPs4_B3B466CE(struct NpCommonPs4JsonValue *value, SceInt32 number)
{
	struct NpCommonPs4JsonTextData *data;
	struct NpCommonPs4JsonTextData *memory;
	int result;
	static const char functionName[] = "SetNum";

	NP_COMMON_PS4_ASSERT(value->type == NP_COMMON_PS4_JSON_TYPE_NUMBER, functionName, "npext\\json\\json_value.cpp", 1314, "m_type == TYPE_NUMBER");
	data = (struct NpCommonPs4JsonTextData *)value->data;
	if (data != NULL) {
		result = SceNpCommonPs4_E595BA30(data, number);
		if (result >= 0)
			return 0;
		return result;
	}
	data = NULL;
	memory = (struct NpCommonPs4JsonTextData *)SceNpCommonPs4_D154D84F(12, value->pool);
	if (memory != NULL) {
		memory->pool = value->pool;
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonTextDataVtable);
		memory->text = NULL;
		data = memory;
		result = SceNpCommonPs4_E595BA30(data, number);
		if (result >= 0) {
			value->data = data;
			return 0;
		}
	} else {
		result = ((int *)value->pool)[4];
	}
	if (data != NULL)
		NP_COMMON_PS4_JSON_DESTROY(data);
	return result;
}

int SceNpCommonPs4_B6A954DA(struct NpCommonPs4JsonTextData *data, SceUInt32 number)
{
	char *text;
	int result;
	SceSize length;

	result = NpCommonPs4UIntToString(NULL, 0, number, 10, 0, &length);
	if (result >= 0) {
		text = (char *)SceNpCommonPs4_299ADFC3(data->pool, length + 1);
		if (text != NULL) {
			result = NpCommonPs4UIntToString(text, length + 1, number, 10, 0, NULL);
			if (result >= 0) {
				SceNpCommonPs4_38CC9AF5(data->pool, data->text);
				data->text = text;
				result = 0;
			}
		} else {
			result = ((int *)data->pool)[4];
		}
	}
	return result;
}

int SceNpCommonPs4_9AB50830(struct NpCommonPs4JsonValue *value, SceUInt32 number)
{
	struct NpCommonPs4JsonTextData *data;
	struct NpCommonPs4JsonTextData *memory;
	int result;
	static const char functionName[] = "SetNum";

	NP_COMMON_PS4_ASSERT(value->type == NP_COMMON_PS4_JSON_TYPE_NUMBER, functionName, "npext\\json\\json_value.cpp", 1351, "m_type == TYPE_NUMBER");
	data = (struct NpCommonPs4JsonTextData *)value->data;
	if (data != NULL) {
		result = SceNpCommonPs4_B6A954DA(data, number);
		if (result >= 0)
			return 0;
		return result;
	}
	data = NULL;
	memory = (struct NpCommonPs4JsonTextData *)SceNpCommonPs4_D154D84F(12, value->pool);
	if (memory != NULL) {
		memory->pool = value->pool;
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonTextDataVtable);
		memory->text = NULL;
		data = memory;
		result = SceNpCommonPs4_B6A954DA(data, number);
		if (result >= 0) {
			value->data = data;
			return 0;
		}
	} else {
		result = ((int *)value->pool)[4];
	}
	if (data != NULL)
		NP_COMMON_PS4_JSON_DESTROY(data);
	return result;
}

int SceNpCommonPs4_3C9E23DC(struct NpCommonPs4JsonTextData *data, SceInt64 number)
{
	char *text;
	int result;
	SceSize length;

	result = NpCommonPs4Int64ToString(NULL, 0, number, 10, 0, &length);
	if (result >= 0) {
		text = (char *)SceNpCommonPs4_299ADFC3(data->pool, length + 1);
		if (text != NULL) {
			result = NpCommonPs4Int64ToString(text, length + 1, number, 10, 0, NULL);
			if (result >= 0) {
				SceNpCommonPs4_38CC9AF5(data->pool, data->text);
				data->text = text;
				result = 0;
			}
		} else {
			result = ((int *)data->pool)[4];
		}
	}
	return result;
}

int SceNpCommonPs4_78DFC124(struct NpCommonPs4JsonValue *value, SceInt64 number)
{
	struct NpCommonPs4JsonTextData *data;
	struct NpCommonPs4JsonTextData *memory;
	int result;
	static const char functionName[] = "SetNum";

	NP_COMMON_PS4_ASSERT(value->type == NP_COMMON_PS4_JSON_TYPE_NUMBER, functionName, "npext\\json\\json_value.cpp", 1388, "m_type == TYPE_NUMBER");
	data = (struct NpCommonPs4JsonTextData *)value->data;
	if (data != NULL) {
		result = SceNpCommonPs4_3C9E23DC(data, number);
		if (result >= 0)
			return 0;
		return result;
	}
	data = NULL;
	memory = (struct NpCommonPs4JsonTextData *)SceNpCommonPs4_D154D84F(12, value->pool);
	if (memory != NULL) {
		memory->pool = value->pool;
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonTextDataVtable);
		memory->text = NULL;
		data = memory;
		result = SceNpCommonPs4_3C9E23DC(data, number);
		if (result >= 0) {
			value->data = data;
			return 0;
		}
	} else {
		result = ((int *)value->pool)[4];
	}
	if (data != NULL)
		NP_COMMON_PS4_JSON_DESTROY(data);
	return result;
}

int SceNpCommonPs4_A23BA075(struct NpCommonPs4JsonTextData *data, SceUInt64 number)
{
	char *text;
	int result;
	SceSize length;

	result = NpCommonPs4UInt64ToString(NULL, 0, number, 10, 0, &length);
	if (result >= 0) {
		text = (char *)SceNpCommonPs4_299ADFC3(data->pool, length + 1);
		if (text != NULL) {
			result = NpCommonPs4UInt64ToString(text, length + 1, number, 10, 0, NULL);
			if (result >= 0) {
				SceNpCommonPs4_38CC9AF5(data->pool, data->text);
				data->text = text;
				result = 0;
			}
		} else {
			result = ((int *)data->pool)[4];
		}
	}
	return result;
}

int SceNpCommonPs4_CFEC9EA7(struct NpCommonPs4JsonValue *value, SceUInt64 number)
{
	struct NpCommonPs4JsonTextData *data;
	struct NpCommonPs4JsonTextData *memory;
	int result;
	static const char functionName[] = "SetNum";

	NP_COMMON_PS4_ASSERT(value->type == NP_COMMON_PS4_JSON_TYPE_NUMBER, functionName, "npext\\json\\json_value.cpp", 1425, "m_type == TYPE_NUMBER");
	data = (struct NpCommonPs4JsonTextData *)value->data;
	if (data != NULL) {
		result = SceNpCommonPs4_A23BA075(data, number);
		if (result >= 0)
			return 0;
		return result;
	}
	data = NULL;
	memory = (struct NpCommonPs4JsonTextData *)SceNpCommonPs4_D154D84F(12, value->pool);
	if (memory != NULL) {
		memory->pool = value->pool;
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonTextDataVtable);
		memory->text = NULL;
		data = memory;
		result = SceNpCommonPs4_A23BA075(data, number);
		if (result >= 0) {
			value->data = data;
			return 0;
		}
	} else {
		result = ((int *)value->pool)[4];
	}
	if (data != NULL)
		NP_COMMON_PS4_JSON_DESTROY(data);
	return result;
}

int SceNpCommonPs4_56AC9166(struct NpCommonPs4JsonTextData *data, const char *string)
{
	char *copy;
	struct NpCommonPs4Allocator *pool;

	pool = data->pool;
	if (string != NULL) {
		copy = npCommonPs4AllocatorDuplicateString(pool, string);
		pool = data->pool;
		if (copy == NULL)
			return ((int *)pool)[4];
		SceNpCommonPs4_38CC9AF5(pool, data->text);
	} else {
		SceNpCommonPs4_38CC9AF5(pool, data->text);
		copy = NULL;
	}
	data->text = copy;
	return 0;
}

int SceNpCommonPs4_943736E5(struct NpCommonPs4JsonValue *value, const char *string)
{
	struct NpCommonPs4JsonTextData *data;
	struct NpCommonPs4JsonTextData *memory;
	int result;
	static const char functionName[] = "SetNum";

	NP_COMMON_PS4_ASSERT(value->type == NP_COMMON_PS4_JSON_TYPE_NUMBER, functionName, "npext\\json\\json_value.cpp", 1462, "m_type == TYPE_NUMBER");
	data = (struct NpCommonPs4JsonTextData *)value->data;
	if (data != NULL) {
		result = SceNpCommonPs4_56AC9166(data, string);
		if (result >= 0)
			return 0;
		return result;
	}
	data = NULL;
	memory = (struct NpCommonPs4JsonTextData *)SceNpCommonPs4_D154D84F(12, value->pool);
	if (memory != NULL) {
		memory->pool = value->pool;
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonTextDataVtable);
		memory->text = NULL;
		data = memory;
		result = SceNpCommonPs4_56AC9166(data, string);
		if (result >= 0) {
			value->data = data;
			return 0;
		}
	} else {
		result = ((int *)value->pool)[4];
	}
	if (data != NULL)
		NP_COMMON_PS4_JSON_DESTROY(data);
	return result;
}

int SceNpCommonPs4_60D9210E(struct NpCommonPs4JsonTextData *data, SceInt32 *number)
{
	int result;

	if (data->text != NULL) {
		result = SceNpCommonPs4_3AE5ABF3(data->text, number, 10, NULL);
		if (result < 0)
			return result;
	} else {
		*number = 0;
	}
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_417E4AA6(struct NpCommonPs4JsonValue *value, SceInt32 *number)
{
	struct NpCommonPs4JsonTextData *data;
	char *text;
	int result;
	static const char functionName[] = "GetNum";

	NP_COMMON_PS4_ASSERT(value->type == NP_COMMON_PS4_JSON_TYPE_NUMBER, functionName, "npext\\json\\json_value.cpp", 1499, "m_type == TYPE_NUMBER");
	data = (struct NpCommonPs4JsonTextData *)value->data;
	if (data == NULL) {
		*number = 0;
		return 0;
	}
	text = data->text;
	if (text == NULL) {
		*number = 0;
		return 0;
	}
	result = SceNpCommonPs4_3AE5ABF3(text, number, 10, NULL);
	if (result >= 0)
		return 0;
	return result;
}

int SceNpCommonPs4_A2D9B617(struct NpCommonPs4JsonTextData *data, SceUInt32 *number)
{
	int result;

	if (data->text != NULL) {
		result = SceNpCommonPs4_D5B30475(data->text, number, 10, NULL);
		if (result < 0)
			return result;
	} else {
		*number = 0;
	}
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_08E9C5F9(struct NpCommonPs4JsonValue *value, SceUInt32 *number)
{
	struct NpCommonPs4JsonTextData *data;
	char *text;
	int result;
	static const char functionName[] = "GetNum";

	NP_COMMON_PS4_ASSERT(value->type == NP_COMMON_PS4_JSON_TYPE_NUMBER, functionName, "npext\\json\\json_value.cpp", 1520, "m_type == TYPE_NUMBER");
	data = (struct NpCommonPs4JsonTextData *)value->data;
	if (data == NULL) {
		*number = 0;
		return 0;
	}
	text = data->text;
	if (text == NULL) {
		*number = 0;
		return 0;
	}
	result = SceNpCommonPs4_D5B30475(text, number, 10, NULL);
	if (result >= 0)
		return 0;
	return result;
}

int SceNpCommonPs4_7D1F38BD(struct NpCommonPs4JsonTextData *data, SceInt64 *number)
{
	int result;

	if (data->text != NULL) {
		result = NpCommonPs4StrToInt64(data->text, number, 10, NULL);
		if (result < 0)
			return result;
	} else {
		*number = 0;
	}
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_318BF324(struct NpCommonPs4JsonValue *value, SceInt64 *number)
{
	struct NpCommonPs4JsonTextData *data;
	char *text;
	int result;
	static const char functionName[] = "GetNum";

	NP_COMMON_PS4_ASSERT(value->type == NP_COMMON_PS4_JSON_TYPE_NUMBER, functionName, "npext\\json\\json_value.cpp", 1541, "m_type == TYPE_NUMBER");
	data = (struct NpCommonPs4JsonTextData *)value->data;
	if (data == NULL) {
		*number = 0;
		return 0;
	}
	text = data->text;
	if (text == NULL) {
		*number = 0;
		return 0;
	}
	result = NpCommonPs4StrToInt64(text, number, 10, NULL);
	if (result >= 0)
		return 0;
	return result;
}

int SceNpCommonPs4_D4E2FEA6(struct NpCommonPs4JsonTextData *data, SceUInt64 *number)
{
	int result;

	if (data->text != NULL) {
		result = NpCommonPs4StrToUInt64(data->text, number, 10, NULL);
		if (result < 0)
			return result;
	} else {
		*number = 0;
	}
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_F29A11C0(struct NpCommonPs4JsonValue *value, SceUInt64 *number)
{
	struct NpCommonPs4JsonTextData *data;
	char *text;
	int result;
	static const char functionName[] = "GetNum";

	NP_COMMON_PS4_ASSERT(value->type == NP_COMMON_PS4_JSON_TYPE_NUMBER, functionName, "npext\\json\\json_value.cpp", 1562, "m_type == TYPE_NUMBER");
	data = (struct NpCommonPs4JsonTextData *)value->data;
	if (data == NULL) {
		*number = 0;
		return 0;
	}
	text = data->text;
	if (text == NULL) {
		*number = 0;
		return 0;
	}
	result = NpCommonPs4StrToUInt64(text, number, 10, NULL);
	if (result >= 0)
		return 0;
	return result;
}

const char *SceNpCommonPs4_D99E09CC(struct NpCommonPs4JsonTextData *data)
{
	const char *result;

	result = "0";
	if (data->text != NULL)
		return data->text;
	return result;
}

const char *SceNpCommonPs4_FAE43676(struct NpCommonPs4JsonValue *value)
{
	struct NpCommonPs4JsonTextData *data;
	const char *result;
	static const char functionName[] = "GetNumStr";

	NP_COMMON_PS4_ASSERT(value->type == NP_COMMON_PS4_JSON_TYPE_NUMBER, functionName, "npext\\json\\json_value.cpp", 1591, "m_type == TYPE_NUMBER");
	data = (struct NpCommonPs4JsonTextData *)value->data;
	if (data == NULL)
		return "0";
	result = "0";
	if (data->text != NULL)
		return data->text;
	return result;
}

int SceNpCommonPs4_83A224E1(struct NpCommonPs4JsonValue *value, char *buffer, SceSize size)
{
	struct NpCommonPs4JsonTextData *data;
	const char *text;
	const char *string;
	static const char functionName[] = "GetNum";
	static const char inlinedFunctionName[] = "GetNumStr";

	NP_COMMON_PS4_ASSERT(value->type == NP_COMMON_PS4_JSON_TYPE_NUMBER, functionName, "npext\\json\\json_value.cpp", 1583, "m_type == TYPE_NUMBER");
	NP_COMMON_PS4_ASSERT(value->type == NP_COMMON_PS4_JSON_TYPE_NUMBER, inlinedFunctionName, "npext\\json\\json_value.cpp", 1591, "m_type == TYPE_NUMBER");
	data = (struct NpCommonPs4JsonTextData *)value->data;
	if (data != NULL) {
		text = "0";
		if (data->text != NULL)
			text = data->text;
		string = text;
	} else {
		string = "0";
	}
	return NpCommonPs4StringCopy(buffer, size, string);
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_95AA3921(struct NpCommonPs4JsonValue *value, struct NpCommonPs4Allocator *pool)
{
	return SceNpCommonPs4_33A6B1DF(value, pool);
}

int SceNpCommonPs4_8B8D8D16(struct NpCommonPs4JsonValue *value)
{
	void *data;

	data = value->data;
	if (data != NULL)
		NP_COMMON_PS4_JSON_DESTROY(data);
	value->data = NULL;
	return 0;
}

struct NpCommonPs4JsonTextData *SceNpCommonPs4_DF04DFAC(struct NpCommonPs4JsonTextData *data, struct NpCommonPs4Allocator *pool)
{
	data->pool = pool;
	data->vtable = NP_COMMON_PS4_VTABLE(g_jsonStringDataVtable);
	data->text = NULL;
	return data;
}

struct NpCommonPs4JsonTextData *SceNpCommonPs4_98E8FF1E(struct NpCommonPs4JsonTextData *data, struct NpCommonPs4Allocator *pool)
{
	struct NpCommonPs4JsonTextData *copy;

	copy = (struct NpCommonPs4JsonTextData *)SceNpCommonPs4_D154D84F(12, pool);
	if (copy != NULL) {
		copy->pool = pool;
		copy->vtable = NP_COMMON_PS4_VTABLE(g_jsonStringDataVtable);
		copy->text = NULL;
		copy->text = npCommonPs4AllocatorDuplicateString(pool, data->text);
		if (copy->text != NULL)
			return copy;
		NP_COMMON_PS4_JSON_DESTROY(copy);
	}
	return NULL;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_CBA4A1E9(struct NpCommonPs4JsonValue *value, struct NpCommonPs4Allocator *pool)
{
	struct NpCommonPs4JsonValue *result;
	struct NpCommonPs4JsonValue *copy;
	struct NpCommonPs4JsonTextData *data;
	struct NpCommonPs4JsonTextData *dataCopy;

	result = NULL;
	copy = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, pool);
	if (copy == NULL)
		return result;
	SceNpCommonPs4_935F32AE(copy, pool, NP_COMMON_PS4_JSON_TYPE_STRING);
	copy->vtable = NP_COMMON_PS4_VTABLE(g_jsonStringValueVtable);
	data = (struct NpCommonPs4JsonTextData *)value->data;
	if (data == NULL)
		return copy;
	dataCopy = (struct NpCommonPs4JsonTextData *)SceNpCommonPs4_D154D84F(12, pool);
	if (dataCopy != NULL) {
		dataCopy->pool = pool;
		dataCopy->text = NULL;
		dataCopy->vtable = NP_COMMON_PS4_VTABLE(g_jsonStringDataVtable);
		dataCopy->text = npCommonPs4AllocatorDuplicateString(pool, data->text);
		if (dataCopy->text == NULL) {
			NP_COMMON_PS4_JSON_DESTROY(dataCopy);
			dataCopy = NULL;
		}
	}
	copy->data = dataCopy;
	if (dataCopy != NULL)
		return copy;
	NP_COMMON_PS4_JSON_DESTROY(copy);
	return result;
}

int SceNpCommonPs4_3812F463(struct NpCommonPs4JsonTextData *data, const char *string)
{
	char *copy;
	struct NpCommonPs4Allocator *pool;

	pool = data->pool;
	if (string != NULL) {
		copy = npCommonPs4AllocatorDuplicateString(pool, string);
		pool = data->pool;
		if (copy == NULL)
			return ((int *)pool)[4];
		SceNpCommonPs4_38CC9AF5(pool, data->text);
	} else {
		SceNpCommonPs4_38CC9AF5(pool, data->text);
		copy = NULL;
	}
	data->text = copy;
	return 0;
}

int SceNpCommonPs4_CAD8A67F(struct NpCommonPs4JsonValue *value, const char *string)
{
	struct NpCommonPs4JsonTextData *data;
	struct NpCommonPs4JsonTextData *memory;
	int result;
	static const char functionName[] = "SetStr";

	NP_COMMON_PS4_ASSERT(value->type == NP_COMMON_PS4_JSON_TYPE_STRING, functionName, "npext\\json\\json_value.cpp", 1650, "m_type == TYPE_STRING");
	data = (struct NpCommonPs4JsonTextData *)value->data;
	if (data != NULL) {
		result = SceNpCommonPs4_3812F463(data, string);
		if (result >= 0)
			return 0;
		return result;
	}
	data = NULL;
	memory = (struct NpCommonPs4JsonTextData *)SceNpCommonPs4_D154D84F(12, value->pool);
	if (memory != NULL) {
		memory->pool = value->pool;
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonStringDataVtable);
		memory->text = NULL;
		data = memory;
		result = SceNpCommonPs4_3812F463(data, string);
		if (result >= 0) {
			value->data = data;
			return 0;
		}
	} else {
		result = ((int *)value->pool)[4];
	}
	if (data != NULL)
		NP_COMMON_PS4_JSON_DESTROY(data);
	return result;
}

const char *SceNpCommonPs4_525F084C(struct NpCommonPs4JsonTextData *data)
{
	const char *result;

	result = "";
	if (data->text != NULL)
		return data->text;
	return result;
}

const char *SceNpCommonPs4_ED539A9D(struct NpCommonPs4JsonValue *value)
{
	struct NpCommonPs4JsonTextData *data;
	const char *result;
	static const char functionName[] = "GetStr";

	NP_COMMON_PS4_ASSERT(value->type == NP_COMMON_PS4_JSON_TYPE_STRING, functionName, "npext\\json\\json_value.cpp", 1692, "m_type == TYPE_STRING");
	data = (struct NpCommonPs4JsonTextData *)value->data;
	if (data == NULL)
		return "";
	result = "";
	if (data->text != NULL)
		return data->text;
	return result;
}

int SceNpCommonPs4_86E584A0(struct NpCommonPs4JsonValue *value, char *buffer, SceSize size)
{
	struct NpCommonPs4JsonTextData *data;
	const char *text;
	const char *string;
	static const char functionName[] = "GetStr";

	NP_COMMON_PS4_ASSERT(value->type == NP_COMMON_PS4_JSON_TYPE_STRING, functionName, "npext\\json\\json_value.cpp", 1692, "m_type == TYPE_STRING");
	data = (struct NpCommonPs4JsonTextData *)value->data;
	if (data != NULL) {
		text = "";
		if (data->text != NULL)
			text = data->text;
		string = text;
	} else {
		string = "";
	}
	return NpCommonPs4StringCopy(buffer, size, string);
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_6F0CB695(struct NpCommonPs4JsonValue *value, struct NpCommonPs4Allocator *pool)
{
	return SceNpCommonPs4_CBA4A1E9(value, pool);
}

int SceNpCommonPs4_A7B91FD4(struct NpCommonPs4JsonValue *value)
{
	void *data;

	data = value->data;
	if (data != NULL)
		NP_COMMON_PS4_JSON_DESTROY(data);
	value->data = NULL;
	return 0;
}

struct NpCommonPs4JsonItemArray *SceNpCommonPs4_AD454139(struct NpCommonPs4JsonItemArray *array,
	struct NpCommonPs4Allocator *pool, struct NpCommonPs4JsonValue *parent)
{
	struct NpCommonPs4JsonItemArray *copy;
	struct NpCommonPs4JsonListNode *node;
	struct NpCommonPs4JsonValue *item;

	copy = (struct NpCommonPs4JsonItemArray *)SceNpCommonPs4_D154D84F(16, pool);
	if (copy != NULL) {
		copy->pool = pool;
		copy->vtable = NP_COMMON_PS4_VTABLE(g_jsonItemArrayVtable);
		copy->list.head = NULL;
		copy->list.tail = NULL;
		node = array->list.head;
		if (node == NULL)
			return copy;
		while (1) {
			item = ((struct NpCommonPs4JsonValue *(*)(struct NpCommonPs4JsonValue *, struct NpCommonPs4Allocator *))
				(*(const void *const **)node->value)[3])(node->value, pool);
			if (item == NULL)
				break;
			if (SceNpCommonPs4_A908E5EA(&copy->list, pool, item) == NULL) {
				NP_COMMON_PS4_JSON_DESTROY(item);
				break;
			}
			item->parent = parent;
			node = node->next;
			if (node == NULL)
				return copy;
		}
		if (copy != NULL)
			NP_COMMON_PS4_JSON_DESTROY(copy);
	}
	return NULL;
}

__attribute__((noinline)) struct NpCommonPs4JsonValue *SceNpCommonPs4_F333238A(struct NpCommonPs4JsonValue *value, struct NpCommonPs4Allocator *pool)
{
	struct NpCommonPs4JsonValue *copy;
	struct NpCommonPs4JsonItemArray *array;
	struct NpCommonPs4JsonItemArray *items;

	copy = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, pool);
	if (copy != NULL) {
		SceNpCommonPs4_935F32AE(copy, pool, NP_COMMON_PS4_JSON_TYPE_ARRAY);
		copy->vtable = NP_COMMON_PS4_VTABLE(g_jsonArrayValueVtable);
		array = (struct NpCommonPs4JsonItemArray *)value->data;
		if (array == NULL)
			return copy;
		items = SceNpCommonPs4_AD454139(array, pool, copy);
		copy->data = items;
		if (items != NULL)
			return copy;
		NP_COMMON_PS4_JSON_DESTROY(copy);
	}
	return NULL;
}

int SceNpCommonPs4_551F8AFA(struct NpCommonPs4JsonValue *array, SceUInt8 boolean, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonValue *memory;
	int result;
	static const char functionName[] = "AddItemBool";

	NP_COMMON_PS4_ASSERT(array->type == NP_COMMON_PS4_JSON_TYPE_ARRAY, functionName, "npext\\json\\json_value.cpp", 1754, "m_type == TYPE_ARRAY");
	value = NULL;
	memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, array->pool);
	if (memory != NULL) {
		SceNpCommonPs4_935F32AE(memory, array->pool, NP_COMMON_PS4_JSON_TYPE_BOOL);
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonBoolValueVtable);
		value = memory;
		result = SceNpCommonPs4_AE224009(memory, boolean);
		if (result >= 0) {
			result = SceNpCommonPs4_413D4254(array, memory);
			if (result >= 0) {
				if (pItem != NULL)
					*pItem = memory;
				return 0;
			}
		}
	} else {
		result = ((int *)array->pool)[4];
	}
	if (value != NULL)
		NP_COMMON_PS4_JSON_DESTROY(value);
	return result;
}

int SceNpCommonPs4_C19AFAC1(struct NpCommonPs4JsonValue *array, SceInt32 number, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonValue *memory;
	int result;
	static const char functionName[] = "AddItemNumber";

	NP_COMMON_PS4_ASSERT(array->type == NP_COMMON_PS4_JSON_TYPE_ARRAY, functionName, "npext\\json\\json_value.cpp", 1791, "m_type == TYPE_ARRAY");
	value = NULL;
	memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, array->pool);
	if (memory != NULL) {
		SceNpCommonPs4_935F32AE(memory, array->pool, NP_COMMON_PS4_JSON_TYPE_NUMBER);
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonNumberValueVtable);
		value = memory;
		result = SceNpCommonPs4_B3B466CE(memory, number);
		if (result >= 0) {
			result = SceNpCommonPs4_413D4254(array, memory);
			if (result >= 0) {
				if (pItem != NULL)
					*pItem = memory;
				return 0;
			}
		}
	} else {
		result = ((int *)array->pool)[4];
	}
	if (value != NULL)
		NP_COMMON_PS4_JSON_DESTROY(value);
	return result;
}

int SceNpCommonPs4_C0468F44(struct NpCommonPs4JsonValue *array, SceUInt32 number, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonValue *memory;
	int result;
	static const char functionName[] = "AddItemNumber";

	NP_COMMON_PS4_ASSERT(array->type == NP_COMMON_PS4_JSON_TYPE_ARRAY, functionName, "npext\\json\\json_value.cpp", 1828, "m_type == TYPE_ARRAY");
	value = NULL;
	memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, array->pool);
	if (memory != NULL) {
		SceNpCommonPs4_935F32AE(memory, array->pool, NP_COMMON_PS4_JSON_TYPE_NUMBER);
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonNumberValueVtable);
		value = memory;
		result = SceNpCommonPs4_9AB50830(memory, number);
		if (result >= 0) {
			result = SceNpCommonPs4_413D4254(array, memory);
			if (result >= 0) {
				if (pItem != NULL)
					*pItem = memory;
				return 0;
			}
		}
	} else {
		result = ((int *)array->pool)[4];
	}
	if (value != NULL)
		NP_COMMON_PS4_JSON_DESTROY(value);
	return result;
}

int SceNpCommonPs4_4741399A(struct NpCommonPs4JsonValue *array, SceInt64 number, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonValue *memory;
	int result;
	static const char functionName[] = "AddItemNumber";

	NP_COMMON_PS4_ASSERT(array->type == NP_COMMON_PS4_JSON_TYPE_ARRAY, functionName, "npext\\json\\json_value.cpp", 1865, "m_type == TYPE_ARRAY");
	value = NULL;
	memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, array->pool);
	if (memory != NULL) {
		SceNpCommonPs4_935F32AE(memory, array->pool, NP_COMMON_PS4_JSON_TYPE_NUMBER);
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonNumberValueVtable);
		value = memory;
		result = SceNpCommonPs4_78DFC124(memory, number);
		if (result >= 0) {
			result = SceNpCommonPs4_413D4254(array, memory);
			if (result >= 0) {
				if (pItem != NULL)
					*pItem = memory;
				return 0;
			}
		}
	} else {
		result = ((int *)array->pool)[4];
	}
	if (value != NULL)
		NP_COMMON_PS4_JSON_DESTROY(value);
	return result;
}

int SceNpCommonPs4_7AC5F79C(struct NpCommonPs4JsonValue *array, SceUInt64 number, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonValue *memory;
	int result;
	static const char functionName[] = "AddItemNumber";

	NP_COMMON_PS4_ASSERT(array->type == NP_COMMON_PS4_JSON_TYPE_ARRAY, functionName, "npext\\json\\json_value.cpp", 1902, "m_type == TYPE_ARRAY");
	value = NULL;
	memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, array->pool);
	if (memory != NULL) {
		SceNpCommonPs4_935F32AE(memory, array->pool, NP_COMMON_PS4_JSON_TYPE_NUMBER);
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonNumberValueVtable);
		value = memory;
		result = SceNpCommonPs4_CFEC9EA7(memory, number);
		if (result >= 0) {
			result = SceNpCommonPs4_413D4254(array, memory);
			if (result >= 0) {
				if (pItem != NULL)
					*pItem = memory;
				return 0;
			}
		}
	} else {
		result = ((int *)array->pool)[4];
	}
	if (value != NULL)
		NP_COMMON_PS4_JSON_DESTROY(value);
	return result;
}

int SceNpCommonPs4_717CC8D1(struct NpCommonPs4JsonValue *array, const char *string, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonValue *memory;
	int result;
	static const char functionName[] = "AddItemString";

	NP_COMMON_PS4_ASSERT(array->type == NP_COMMON_PS4_JSON_TYPE_ARRAY, functionName, "npext\\json\\json_value.cpp", 1939, "m_type == TYPE_ARRAY");
	value = NULL;
	memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, array->pool);
	if (memory != NULL) {
		SceNpCommonPs4_935F32AE(memory, array->pool, NP_COMMON_PS4_JSON_TYPE_STRING);
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonStringValueVtable);
		value = memory;
		result = SceNpCommonPs4_CAD8A67F(memory, string);
		if (result >= 0) {
			result = SceNpCommonPs4_413D4254(array, memory);
			if (result >= 0) {
				if (pItem != NULL)
					*pItem = memory;
				return 0;
			}
		}
	} else {
		result = ((int *)array->pool)[4];
	}
	if (value != NULL)
		NP_COMMON_PS4_JSON_DESTROY(value);
	return result;
}

int SceNpCommonPs4_17022536(struct NpCommonPs4JsonValue *array, struct NpCommonPs4JsonValue *source, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *copy;
	int result;
	static const char functionName[] = "AddItemArray";

	NP_COMMON_PS4_ASSERT(array->type == NP_COMMON_PS4_JSON_TYPE_ARRAY, functionName, "npext\\json\\json_value.cpp", 1976, "m_type == TYPE_ARRAY");
	copy = SceNpCommonPs4_F333238A(source, array->pool);
	if (copy == NULL) {
		result = ((int *)array->pool)[4];
	} else {
		result = SceNpCommonPs4_413D4254(array, copy);
		if (result >= 0) {
			if (pItem != NULL)
				*pItem = copy;
			return 0;
		}
	}
	if (copy != NULL)
		NP_COMMON_PS4_JSON_DESTROY(copy);
	return result;
}

__attribute__((noinline)) struct NpCommonPs4JsonFieldSet *SceNpCommonPs4_1431510B(struct NpCommonPs4JsonFieldSet *source,
	struct NpCommonPs4Allocator *pool, struct NpCommonPs4JsonValue *parent)
{
	struct NpCommonPs4JsonFieldSet *copy;
	struct NpCommonPs4JsonField *field;
	struct NpCommonPs4JsonValue *value;

	copy = (struct NpCommonPs4JsonFieldSet *)SceNpCommonPs4_D154D84F(16, pool);
	if (copy != NULL) {
		copy->pool = pool;
		copy->vtable = NP_COMMON_PS4_VTABLE(g_jsonFieldSetVtable);
		copy->list.head = NULL;
		copy->list.tail = NULL;
		field = source->list.head;
		if (field == NULL)
			return copy;
		while (1) {
			value = ((struct NpCommonPs4JsonValue *(*)(struct NpCommonPs4JsonValue *, struct NpCommonPs4Allocator *))
				(*(const void *const **)field->value)[3])(field->value, pool);
			if (value == NULL)
				break;
			if (SceNpCommonPs4_332AF121(&copy->list, pool, field->name, value) == NULL) {
				NP_COMMON_PS4_JSON_DESTROY(value);
				break;
			}
			value->parent = parent;
			field = field->next;
			if (field == NULL)
				return copy;
		}
		if (copy != NULL)
			NP_COMMON_PS4_JSON_DESTROY(copy);
	}
	return NULL;
}

__attribute__((noinline)) struct NpCommonPs4JsonValue *SceNpCommonPs4_B355F0C6(struct NpCommonPs4JsonValue *value, struct NpCommonPs4Allocator *pool)
{
	struct NpCommonPs4JsonValue *copy;
	struct NpCommonPs4JsonFieldSet *fields;
	struct NpCommonPs4JsonFieldSet *items;
	struct NpCommonPs4JsonValue *result;

	result = NULL;
	copy = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, pool);
	if (copy != NULL) {
		SceNpCommonPs4_935F32AE(copy, pool, NP_COMMON_PS4_JSON_TYPE_OBJECT);
		copy->vtable = NP_COMMON_PS4_VTABLE(g_jsonObjectValueVtable);
		fields = (struct NpCommonPs4JsonFieldSet *)value->data;
		if (fields == NULL)
			return copy;
		items = SceNpCommonPs4_1431510B(fields, pool, copy);
		copy->data = items;
		if (items != NULL)
			return copy;
		NP_COMMON_PS4_JSON_DESTROY(copy);
	}
	return result;
}

int SceNpCommonPs4_648987A5(struct NpCommonPs4JsonValue *array, struct NpCommonPs4JsonValue *source, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *copy;
	int result;
	static const char functionName[] = "AddItemObject";

	NP_COMMON_PS4_ASSERT(array->type == NP_COMMON_PS4_JSON_TYPE_ARRAY, functionName, "npext\\json\\json_value.cpp", 2007, "m_type == TYPE_ARRAY");
	copy = SceNpCommonPs4_B355F0C6(source, array->pool);
	if (copy == NULL) {
		result = ((int *)array->pool)[4];
	} else {
		result = SceNpCommonPs4_413D4254(array, copy);
		if (result >= 0) {
			if (pItem != NULL)
				*pItem = copy;
			return 0;
		}
	}
	if (copy != NULL)
		NP_COMMON_PS4_JSON_DESTROY(copy);
	return result;
}

int SceNpCommonPs4_8BE78D66(struct NpCommonPs4JsonItemList *list)
{
	struct NpCommonPs4JsonListNode *node;
	int count;

	node = list->head;
	for (count = 0; node != NULL; count++)
		node = node->next;
	return count;
}

int SceNpCommonPs4_723B574A(struct NpCommonPs4JsonItemArray *array)
{
	struct NpCommonPs4JsonListNode *node;
	int count;

	node = array->list.head;
	for (count = 0; node != NULL; count++)
		node = node->next;
	return count;
}

int SceNpCommonPs4_1E21D357(struct NpCommonPs4JsonValue *array)
{
	struct NpCommonPs4JsonItemArray *items;
	struct NpCommonPs4JsonListNode *node;
	int count;
	static const char functionName[] = "GetItemCount";

	NP_COMMON_PS4_ASSERT(array->type == NP_COMMON_PS4_JSON_TYPE_ARRAY, functionName, "npext\\json\\json_value.cpp", 2038, "m_type == TYPE_ARRAY");
	items = (struct NpCommonPs4JsonItemArray *)array->data;
	if (items != NULL) {
		count = 0;
		for (node = items->list.head; node != NULL; node = node->next)
			count++;
		return count;
	}
	return 0;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_7A8C3C10(struct NpCommonPs4JsonValue *array, int index)
{
	struct NpCommonPs4JsonValue *result;

	result = SceNpCommonPs4_2A4A268C(array, index);
	if (result == NULL || result->type != NP_COMMON_PS4_JSON_TYPE_BOOL)
		return NULL;
	return result;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_912F2E72(struct NpCommonPs4JsonValue *array, int index)
{
	struct NpCommonPs4JsonValue *result;

	result = SceNpCommonPs4_2A4A268C(array, index);
	if (result == NULL || result->type != NP_COMMON_PS4_JSON_TYPE_NUMBER)
		return NULL;
	return result;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_85FFAD1E(struct NpCommonPs4JsonValue *array, int index)
{
	struct NpCommonPs4JsonValue *result;

	result = SceNpCommonPs4_2A4A268C(array, index);
	if (result == NULL || result->type != NP_COMMON_PS4_JSON_TYPE_STRING)
		return NULL;
	return result;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_C548F5BB(struct NpCommonPs4JsonValue *array, int index)
{
	struct NpCommonPs4JsonValue *result;

	result = SceNpCommonPs4_2A4A268C(array, index);
	if (result == NULL || result->type != NP_COMMON_PS4_JSON_TYPE_ARRAY)
		return NULL;
	return result;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_CC6D1AC0(struct NpCommonPs4JsonValue *array, int index)
{
	struct NpCommonPs4JsonValue *result;

	result = SceNpCommonPs4_2A4A268C(array, index);
	if (result == NULL || result->type != NP_COMMON_PS4_JSON_TYPE_OBJECT)
		return NULL;
	return result;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_18EA8733(struct NpCommonPs4JsonValue *value, struct NpCommonPs4Allocator *pool)
{
	return SceNpCommonPs4_F333238A(value, pool);
}

int SceNpCommonPs4_9DB48E05(struct NpCommonPs4JsonValue *value)
{
	void *data;

	data = value->data;
	if (data != NULL)
		NP_COMMON_PS4_JSON_DESTROY(data);
	value->data = NULL;
	return 0;
}

int SceNpCommonPs4_8E8E16FF(struct NpCommonPs4JsonValue *object, const char *name, SceUInt8 boolean, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonValue *memory;
	int result;
	static const char functionName[] = "SetFieldBool";

	NP_COMMON_PS4_ASSERT(object->type == NP_COMMON_PS4_JSON_TYPE_OBJECT, functionName, "npext\\json\\json_value.cpp", 2150, "m_type == TYPE_OBJECT");
	value = NULL;
	memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, object->pool);
	if (memory != NULL) {
		SceNpCommonPs4_935F32AE(memory, object->pool, NP_COMMON_PS4_JSON_TYPE_BOOL);
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonBoolValueVtable);
		value = memory;
		result = SceNpCommonPs4_AE224009(memory, boolean);
		if (result >= 0) {
			result = SceNpCommonPs4_7833EEC2(object, name, memory);
			if (result >= 0) {
				if (pItem != NULL)
					*pItem = memory;
				return 0;
			}
		}
	} else {
		result = ((int *)object->pool)[4];
	}
	if (value != NULL)
		NP_COMMON_PS4_JSON_DESTROY(value);
	return result;
}

int SceNpCommonPs4_13B11D92(struct NpCommonPs4JsonValue *object, const char *name, SceInt32 number, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonValue *memory;
	int result;
	static const char functionName[] = "SetFieldNumber";

	NP_COMMON_PS4_ASSERT(object->type == NP_COMMON_PS4_JSON_TYPE_OBJECT, functionName, "npext\\json\\json_value.cpp", 2187, "m_type == TYPE_OBJECT");
	value = NULL;
	memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, object->pool);
	if (memory != NULL) {
		SceNpCommonPs4_935F32AE(memory, object->pool, NP_COMMON_PS4_JSON_TYPE_NUMBER);
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonNumberValueVtable);
		value = memory;
		result = SceNpCommonPs4_B3B466CE(memory, number);
		if (result >= 0) {
			result = SceNpCommonPs4_7833EEC2(object, name, memory);
			if (result >= 0) {
				if (pItem != NULL)
					*pItem = memory;
				return 0;
			}
		}
	} else {
		result = ((int *)object->pool)[4];
	}
	if (value != NULL)
		NP_COMMON_PS4_JSON_DESTROY(value);
	return result;
}

int SceNpCommonPs4_1899E3FC(struct NpCommonPs4JsonValue *object, const char *name, SceUInt32 number, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonValue *memory;
	int result;
	static const char functionName[] = "SetFieldNumber";

	NP_COMMON_PS4_ASSERT(object->type == NP_COMMON_PS4_JSON_TYPE_OBJECT, functionName, "npext\\json\\json_value.cpp", 2224, "m_type == TYPE_OBJECT");
	value = NULL;
	memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, object->pool);
	if (memory != NULL) {
		SceNpCommonPs4_935F32AE(memory, object->pool, NP_COMMON_PS4_JSON_TYPE_NUMBER);
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonNumberValueVtable);
		value = memory;
		result = SceNpCommonPs4_9AB50830(memory, number);
		if (result >= 0) {
			result = SceNpCommonPs4_7833EEC2(object, name, memory);
			if (result >= 0) {
				if (pItem != NULL)
					*pItem = memory;
				return 0;
			}
		}
	} else {
		result = ((int *)object->pool)[4];
	}
	if (value != NULL)
		NP_COMMON_PS4_JSON_DESTROY(value);
	return result;
}

int SceNpCommonPs4_D4CE9188(struct NpCommonPs4JsonValue *object, const char *name, SceInt64 number, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonValue *memory;
	int result;
	static const char functionName[] = "SetFieldNumber";

	NP_COMMON_PS4_ASSERT(object->type == NP_COMMON_PS4_JSON_TYPE_OBJECT, functionName, "npext\\json\\json_value.cpp", 2261, "m_type == TYPE_OBJECT");
	value = NULL;
	memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, object->pool);
	if (memory != NULL) {
		SceNpCommonPs4_935F32AE(memory, object->pool, NP_COMMON_PS4_JSON_TYPE_NUMBER);
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonNumberValueVtable);
		value = memory;
		result = SceNpCommonPs4_78DFC124(memory, number);
		if (result >= 0) {
			result = SceNpCommonPs4_7833EEC2(object, name, memory);
			if (result >= 0) {
				if (pItem != NULL)
					*pItem = memory;
				return 0;
			}
		}
	} else {
		result = ((int *)object->pool)[4];
	}
	if (value != NULL)
		NP_COMMON_PS4_JSON_DESTROY(value);
	return result;
}

int SceNpCommonPs4_27F98F80(struct NpCommonPs4JsonValue *object, const char *name, SceUInt64 number, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonValue *memory;
	int result;
	static const char functionName[] = "SetFieldNumber";

	NP_COMMON_PS4_ASSERT(object->type == NP_COMMON_PS4_JSON_TYPE_OBJECT, functionName, "npext\\json\\json_value.cpp", 2298, "m_type == TYPE_OBJECT");
	value = NULL;
	memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, object->pool);
	if (memory != NULL) {
		SceNpCommonPs4_935F32AE(memory, object->pool, NP_COMMON_PS4_JSON_TYPE_NUMBER);
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonNumberValueVtable);
		value = memory;
		result = SceNpCommonPs4_CFEC9EA7(memory, number);
		if (result >= 0) {
			result = SceNpCommonPs4_7833EEC2(object, name, memory);
			if (result >= 0) {
				if (pItem != NULL)
					*pItem = memory;
				return 0;
			}
		}
	} else {
		result = ((int *)object->pool)[4];
	}
	if (value != NULL)
		NP_COMMON_PS4_JSON_DESTROY(value);
	return result;
}

int SceNpCommonPs4_977CBCE6(struct NpCommonPs4JsonValue *object, const char *name, const char *string, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonValue *memory;
	int result;
	static const char functionName[] = "SetFieldString";

	NP_COMMON_PS4_ASSERT(object->type == NP_COMMON_PS4_JSON_TYPE_OBJECT, functionName, "npext\\json\\json_value.cpp", 2335, "m_type == TYPE_OBJECT");
	value = NULL;
	memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, object->pool);
	if (memory != NULL) {
		SceNpCommonPs4_935F32AE(memory, object->pool, NP_COMMON_PS4_JSON_TYPE_STRING);
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonStringValueVtable);
		value = memory;
		result = SceNpCommonPs4_CAD8A67F(memory, string);
		if (result >= 0) {
			result = SceNpCommonPs4_7833EEC2(object, name, memory);
			if (result >= 0) {
				if (pItem != NULL)
					*pItem = memory;
				return 0;
			}
		}
	} else {
		result = ((int *)object->pool)[4];
	}
	if (value != NULL)
		NP_COMMON_PS4_JSON_DESTROY(value);
	return result;
}

int SceNpCommonPs4_DD8D9726(struct NpCommonPs4JsonValue *object, const char *name, struct NpCommonPs4JsonValue *source, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *copy;
	int result;
	static const char functionName[] = "SetFieldArray";

	NP_COMMON_PS4_ASSERT(object->type == NP_COMMON_PS4_JSON_TYPE_OBJECT, functionName, "npext\\json\\json_value.cpp", 2372, "m_type == TYPE_OBJECT");
	copy = SceNpCommonPs4_F333238A(source, object->pool);
	if (copy == NULL) {
		result = ((int *)object->pool)[4];
	} else {
		result = SceNpCommonPs4_7833EEC2(object, name, copy);
		if (result >= 0) {
			if (pItem != NULL)
				*pItem = copy;
			return 0;
		}
	}
	if (copy != NULL)
		NP_COMMON_PS4_JSON_DESTROY(copy);
	return result;
}

int SceNpCommonPs4_4846AD83(struct NpCommonPs4JsonValue *object, const char *name, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonValue *memory;
	int result;
	static const char functionName[] = "SetFieldArray";

	NP_COMMON_PS4_ASSERT(object->type == NP_COMMON_PS4_JSON_TYPE_OBJECT, functionName, "npext\\json\\json_value.cpp", 2403, "m_type == TYPE_OBJECT");
	value = NULL;
	memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, object->pool);
	if (memory != NULL) {
		SceNpCommonPs4_935F32AE(memory, object->pool, NP_COMMON_PS4_JSON_TYPE_ARRAY);
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonArrayValueVtable);
		value = memory;
		result = SceNpCommonPs4_7833EEC2(object, name, memory);
		if (result >= 0) {
			if (pItem != NULL)
				*pItem = memory;
			return 0;
		}
	} else {
		result = ((int *)object->pool)[4];
	}
	if (value != NULL)
		NP_COMMON_PS4_JSON_DESTROY(value);
	return result;
}

int SceNpCommonPs4_E498EF70(struct NpCommonPs4JsonValue *object, const char *name, struct NpCommonPs4JsonValue *source, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *copy;
	int result;
	static const char functionName[] = "SetFieldObject";

	NP_COMMON_PS4_ASSERT(object->type == NP_COMMON_PS4_JSON_TYPE_OBJECT, functionName, "npext\\json\\json_value.cpp", 2434, "m_type == TYPE_OBJECT");
	copy = SceNpCommonPs4_B355F0C6(source, object->pool);
	if (copy == NULL) {
		result = ((int *)object->pool)[4];
	} else {
		result = SceNpCommonPs4_7833EEC2(object, name, copy);
		if (result >= 0) {
			if (pItem != NULL)
				*pItem = copy;
			return 0;
		}
	}
	if (copy != NULL)
		NP_COMMON_PS4_JSON_DESTROY(copy);
	return result;
}

int SceNpCommonPs4_C47C2F93(struct NpCommonPs4JsonValue *object, const char *name, struct NpCommonPs4JsonValue **pItem)
{
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonValue *memory;
	int result;
	static const char functionName[] = "SetFieldObject";

	NP_COMMON_PS4_ASSERT(object->type == NP_COMMON_PS4_JSON_TYPE_OBJECT, functionName, "npext\\json\\json_value.cpp", 2465, "m_type == TYPE_OBJECT");
	value = NULL;
	memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, object->pool);
	if (memory != NULL) {
		SceNpCommonPs4_935F32AE(memory, object->pool, NP_COMMON_PS4_JSON_TYPE_OBJECT);
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonObjectValueVtable);
		value = memory;
		result = SceNpCommonPs4_7833EEC2(object, name, memory);
		if (result >= 0) {
			if (pItem != NULL)
				*pItem = memory;
			return 0;
		}
	} else {
		result = ((int *)object->pool)[4];
	}
	if (value != NULL)
		NP_COMMON_PS4_JSON_DESTROY(value);
	return result;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_42CAF6ED(struct NpCommonPs4JsonValue *object, const char *name)
{
	struct NpCommonPs4JsonValue *result;

	result = SceNpCommonPs4_428DE8A6(object, name);
	if (result == NULL || result->type != NP_COMMON_PS4_JSON_TYPE_NUMBER)
		return NULL;
	return result;
}

int SceNpCommonPs4_066F0583(struct NpCommonPs4JsonValue *object, const char *name, SceInt32 *number)
{
	struct NpCommonPs4JsonValue *field;
	struct NpCommonPs4JsonTextData *data;
	char *text;
	int result;

	field = SceNpCommonPs4_428DE8A6(object, name);
	if (field == NULL || field->type != NP_COMMON_PS4_JSON_TYPE_NUMBER)
		return NP_COMMON_PS4_ERROR_80559E0E;
	data = (struct NpCommonPs4JsonTextData *)field->data;
	if (data == NULL) {
		*number = 0;
		return 0;
	}
	text = data->text;
	if (text == NULL) {
		*number = 0;
		return 0;
	}
	result = SceNpCommonPs4_3AE5ABF3(text, number, 10, NULL);
	if (result >= 0)
		return 0;
	return result;
}

int SceNpCommonPs4_64EE86C7(struct NpCommonPs4JsonValue *object, const char *name, SceUInt32 *number)
{
	struct NpCommonPs4JsonValue *field;
	int result;

	field = SceNpCommonPs4_428DE8A6(object, name);
	if (field == NULL || field->type != NP_COMMON_PS4_JSON_TYPE_NUMBER)
		return NP_COMMON_PS4_ERROR_80559E0E;
	result = SceNpCommonPs4_08E9C5F9(field, number);
	if (result >= 0)
		return 0;
	return result;
}

int SceNpCommonPs4_54EFA071(struct NpCommonPs4JsonValue *object, const char *name, SceInt64 *number)
{
	struct NpCommonPs4JsonValue *field;
	int result;

	field = SceNpCommonPs4_428DE8A6(object, name);
	if (field == NULL || field->type != NP_COMMON_PS4_JSON_TYPE_NUMBER)
		return NP_COMMON_PS4_ERROR_80559E0E;
	result = SceNpCommonPs4_318BF324(field, number);
	if (result >= 0)
		return 0;
	return result;
}

int SceNpCommonPs4_63174832(struct NpCommonPs4JsonValue *object, const char *name, SceUInt64 *number)
{
	struct NpCommonPs4JsonValue *field;
	int result;

	field = SceNpCommonPs4_428DE8A6(object, name);
	if (field == NULL || field->type != NP_COMMON_PS4_JSON_TYPE_NUMBER)
		return NP_COMMON_PS4_ERROR_80559E0E;
	result = SceNpCommonPs4_F29A11C0(field, number);
	if (result >= 0)
		return 0;
	return result;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_E8740CE6(struct NpCommonPs4JsonValue *object, const char *name)
{
	struct NpCommonPs4JsonValue *result;

	result = SceNpCommonPs4_428DE8A6(object, name);
	if (result == NULL || result->type != NP_COMMON_PS4_JSON_TYPE_STRING)
		return NULL;
	return result;
}

int SceNpCommonPs4_3CE6B5C7(struct NpCommonPs4JsonValue *object, const char *name, char *buffer, SceSize size)
{
	struct NpCommonPs4JsonValue *field;
	struct NpCommonPs4JsonTextData *data;
	const char *text;
	const char *string;
	int result;

	field = SceNpCommonPs4_428DE8A6(object, name);
	if (field == NULL || field->type != NP_COMMON_PS4_JSON_TYPE_STRING)
		return NP_COMMON_PS4_ERROR_80559E0E;
	data = (struct NpCommonPs4JsonTextData *)field->data;
	if (data != NULL) {
		text = "";
		if (data->text != NULL)
			text = data->text;
		string = text;
	} else {
		string = "";
	}
	result = NpCommonPs4StringCopy(buffer, size, string);
	if (result >= 0)
		return 0;
	return result;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_2F351F96(struct NpCommonPs4JsonValue *object, const char *name)
{
	struct NpCommonPs4JsonValue *result;

	result = SceNpCommonPs4_428DE8A6(object, name);
	if (result == NULL || result->type != NP_COMMON_PS4_JSON_TYPE_BOOL)
		return NULL;
	return result;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_6CAA885A(struct NpCommonPs4JsonValue *object, const char *name)
{
	struct NpCommonPs4JsonValue *result;

	result = SceNpCommonPs4_428DE8A6(object, name);
	if (result == NULL || result->type != NP_COMMON_PS4_JSON_TYPE_ARRAY)
		return NULL;
	return result;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_C4525618(struct NpCommonPs4JsonValue *object, const char *name)
{
	struct NpCommonPs4JsonValue *result;

	result = SceNpCommonPs4_428DE8A6(object, name);
	if (result == NULL || result->type != NP_COMMON_PS4_JSON_TYPE_OBJECT)
		return NULL;
	return result;
}

int SceNpCommonPs4_53435832(struct NpCommonPs4JsonFieldList *list)
{
	struct NpCommonPs4JsonField *field;
	int count;

	field = list->head;
	for (count = 0; field != NULL; count++)
		field = field->next;
	return count;
}

int SceNpCommonPs4_449243EA(struct NpCommonPs4JsonFieldSet *fields)
{
	struct NpCommonPs4JsonField *field;
	int count;

	field = fields->list.head;
	for (count = 0; field != NULL; count++)
		field = field->next;
	return count;
}

int SceNpCommonPs4_9C3F1645(struct NpCommonPs4JsonValue *object)
{
	struct NpCommonPs4JsonFieldSet *fields;
	struct NpCommonPs4JsonField *field;
	int count;
	static const char functionName[] = "GetFieldCount";

	NP_COMMON_PS4_ASSERT(object->type == NP_COMMON_PS4_JSON_TYPE_OBJECT, functionName, "npext\\json\\json_value.cpp", 2652, "m_type == TYPE_OBJECT");
	fields = (struct NpCommonPs4JsonFieldSet *)object->data;
	if (fields != NULL) {
		count = 0;
		for (field = fields->list.head; field != NULL; field = field->next)
			count++;
		return count;
	}
	return 0;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_1C339E60(struct NpCommonPs4JsonValue *object, int index, char **pName)
{
	struct NpCommonPs4JsonValue *result;

	result = SceNpCommonPs4_713C8A08(object, index, pName);
	if (result == NULL || result->type != NP_COMMON_PS4_JSON_TYPE_BOOL)
		return NULL;
	return result;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_C43A74AC(struct NpCommonPs4JsonValue *object, int index, char **pName)
{
	struct NpCommonPs4JsonValue *result;

	result = SceNpCommonPs4_713C8A08(object, index, pName);
	if (result == NULL || result->type != NP_COMMON_PS4_JSON_TYPE_NUMBER)
		return NULL;
	return result;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_FAF8B50D(struct NpCommonPs4JsonValue *object, int index, char **pName)
{
	struct NpCommonPs4JsonValue *result;

	result = SceNpCommonPs4_713C8A08(object, index, pName);
	if (result == NULL || result->type != NP_COMMON_PS4_JSON_TYPE_STRING)
		return NULL;
	return result;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_192F6474(struct NpCommonPs4JsonValue *object, int index, char **pName)
{
	struct NpCommonPs4JsonValue *result;

	result = SceNpCommonPs4_713C8A08(object, index, pName);
	if (result == NULL || result->type != NP_COMMON_PS4_JSON_TYPE_ARRAY)
		return NULL;
	return result;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_24D09586(struct NpCommonPs4JsonValue *object, int index, char **pName)
{
	struct NpCommonPs4JsonValue *result;

	result = SceNpCommonPs4_713C8A08(object, index, pName);
	if (result == NULL || result->type != NP_COMMON_PS4_JSON_TYPE_OBJECT)
		return NULL;
	return result;
}

struct NpCommonPs4JsonValue *npCommonPs4JsonObjectValueClone(struct NpCommonPs4JsonValue *value, struct NpCommonPs4Allocator *pool)
{
	return SceNpCommonPs4_B355F0C6(value, pool);
}

struct NpCommonPs4JsonBoolData *SceNpCommonPs4_43E8DEB6(struct NpCommonPs4JsonBoolData *data)
{
	data->vtable = NP_COMMON_PS4_VTABLE(g_jsonBoolDataVtable);
	data->value = 0;
	return data;
}

struct NpCommonPs4JsonObject *SceNpCommonPs4_22D12B6B(struct NpCommonPs4JsonObject *object)
{
	object->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return object;
}

int SceNpCommonPs4_3F48BFF4(struct NpCommonPs4JsonObject *object)
{
	object->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(object);
}

struct NpCommonPs4JsonObject *SceNpCommonPs4_98566A65(struct NpCommonPs4JsonObject *object)
{
	object->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return object;
}

struct NpCommonPs4JsonTextData *SceNpCommonPs4_62791AE1(struct NpCommonPs4JsonTextData *data, struct NpCommonPs4Allocator *pool)
{
	data->pool = pool;
	data->vtable = NP_COMMON_PS4_VTABLE(g_jsonTextDataVtable);
	data->text = NULL;
	return data;
}

struct NpCommonPs4JsonTextData *SceNpCommonPs4_B778425E(struct NpCommonPs4JsonTextData *data)
{
	struct NpCommonPs4Allocator *pool;
	char *text;

	pool = data->pool;
	text = data->text;
	data->vtable = NP_COMMON_PS4_VTABLE(g_jsonTextDataVtable);
	SceNpCommonPs4_38CC9AF5(pool, text);
	data->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return data;
}

int SceNpCommonPs4_280EF5E0(struct NpCommonPs4JsonTextData *data)
{
	struct NpCommonPs4Allocator *pool;
	char *text;

	pool = data->pool;
	text = data->text;
	data->vtable = NP_COMMON_PS4_VTABLE(g_jsonTextDataVtable);
	SceNpCommonPs4_38CC9AF5(pool, text);
	data->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(data);
}

struct NpCommonPs4JsonTextData *SceNpCommonPs4_7E16D757(struct NpCommonPs4JsonTextData *data)
{
	struct NpCommonPs4Allocator *pool;
	char *text;

	pool = data->pool;
	text = data->text;
	data->vtable = NP_COMMON_PS4_VTABLE(g_jsonTextDataVtable);
	SceNpCommonPs4_38CC9AF5(pool, text);
	data->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return data;
}

int SceNpCommonPs4_AD7C6B29(struct NpCommonPs4JsonTextData *data, char *buffer, SceSize size)
{
	const char *text;
	const char *source;

	text = data->text;
	source = "0";
	if (text != NULL)
		source = text;
	if (size < sceClibStrnlen(source, 0xFFFFFFFF) + 1)
		return NP_COMMON_PS4_ERROR_80559E0D;
	source = "0";
	if (data->text != NULL)
		source = data->text;
	sceClibStrncpy(buffer, source, size - 1);
	buffer[size - 1] = 0;
	return 0;
}

struct NpCommonPs4JsonTextData *SceNpCommonPs4_644D637A(struct NpCommonPs4JsonTextData *data, struct NpCommonPs4Allocator *pool)
{
	data->pool = pool;
	data->vtable = NP_COMMON_PS4_VTABLE(g_jsonStringDataVtable);
	data->text = NULL;
	return data;
}

struct NpCommonPs4JsonTextData *SceNpCommonPs4_8CE27F18(struct NpCommonPs4JsonTextData *data)
{
	struct NpCommonPs4Allocator *pool;
	char *text;

	pool = data->pool;
	text = data->text;
	data->vtable = NP_COMMON_PS4_VTABLE(g_jsonStringDataVtable);
	SceNpCommonPs4_38CC9AF5(pool, text);
	data->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return data;
}

int SceNpCommonPs4_1ED87D80(struct NpCommonPs4JsonTextData *data)
{
	struct NpCommonPs4Allocator *pool;
	char *text;

	pool = data->pool;
	text = data->text;
	data->vtable = NP_COMMON_PS4_VTABLE(g_jsonStringDataVtable);
	SceNpCommonPs4_38CC9AF5(pool, text);
	data->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(data);
}

struct NpCommonPs4JsonTextData *SceNpCommonPs4_BA343899(struct NpCommonPs4JsonTextData *data)
{
	struct NpCommonPs4Allocator *pool;
	char *text;

	pool = data->pool;
	text = data->text;
	data->vtable = NP_COMMON_PS4_VTABLE(g_jsonStringDataVtable);
	SceNpCommonPs4_38CC9AF5(pool, text);
	data->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return data;
}

int SceNpCommonPs4_B52FB8BD(struct NpCommonPs4JsonTextData *data, char *buffer, SceSize size)
{
	const char *text;

	text = "";
	if (data->text != NULL)
		text = data->text;
	return NpCommonPs4StringCopy(buffer, size, text);
}

struct NpCommonPs4JsonItemArray *SceNpCommonPs4_72006000(struct NpCommonPs4JsonItemArray *array, struct NpCommonPs4Allocator *pool)
{
	array->pool = pool;
	array->vtable = NP_COMMON_PS4_VTABLE(g_jsonItemArrayVtable);
	array->list.tail = NULL;
	array->list.head = NULL;
	return array;
}

__attribute__((noinline)) void SceNpCommonPs4_CD583157(struct NpCommonPs4JsonItemList *list, struct NpCommonPs4Allocator *pool)
{
	struct NpCommonPs4JsonListNode *node;
	struct NpCommonPs4JsonListNode *next;
	struct NpCommonPs4JsonListNode *prev;
	struct NpCommonPs4JsonValue *value;

	node = list->head;
	if (node != NULL) {
		do {
			next = node->next;
			prev = node->prev;
			if (next != NULL)
				next->prev = prev;
			else
				list->tail = prev;
			if (node->prev != NULL)
				node->prev->next = node->next;
			else
				list->head = node->next;
			value = node->value;
			node->next = NULL;
			node->prev = NULL;
			if (value != NULL)
				NP_COMMON_PS4_JSON_DESTROY(value);
			SceNpCommonPs4_38CC9AF5(pool, node);
			node = next;
		} while (node != NULL);
	}
}

struct NpCommonPs4JsonItemArray *SceNpCommonPs4_7CCD3D42(struct NpCommonPs4JsonItemArray *self)
{
	struct NpCommonPs4Allocator *pool;

	pool = self->pool;
	self->vtable = NP_COMMON_PS4_VTABLE(g_jsonItemArrayVtable);
	SceNpCommonPs4_CD583157(&self->list, pool);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return self;
}

int SceNpCommonPs4_0E43CE5B(struct NpCommonPs4JsonItemArray *self)
{
	struct NpCommonPs4Allocator *pool;

	pool = self->pool;
	self->vtable = NP_COMMON_PS4_VTABLE(g_jsonItemArrayVtable);
	SceNpCommonPs4_CD583157(&self->list, pool);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(self);
}

struct NpCommonPs4JsonItemArray *SceNpCommonPs4_51260012(struct NpCommonPs4JsonItemArray *self)
{
	struct NpCommonPs4Allocator *pool;

	pool = self->pool;
	self->vtable = NP_COMMON_PS4_VTABLE(g_jsonItemArrayVtable);
	SceNpCommonPs4_CD583157(&self->list, pool);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return self;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_720069A1(struct NpCommonPs4JsonItemArray *array, int index)
{
	struct NpCommonPs4JsonListNode *node;

	node = array->list.head;
	if (node == NULL)
		return NULL;
	while (index != 0) {
		node = node->next;
		--index;
		if (node == NULL)
			return NULL;
	}
	return node->value;
}

struct NpCommonPs4JsonFieldSet *SceNpCommonPs4_E65753F9(struct NpCommonPs4JsonFieldSet *fields, struct NpCommonPs4Allocator *pool)
{
	fields->pool = pool;
	fields->vtable = NP_COMMON_PS4_VTABLE(g_jsonFieldSetVtable);
	fields->list.tail = NULL;
	fields->list.head = NULL;
	return fields;
}

__attribute__((noinline)) void SceNpCommonPs4_FAABC04C(struct NpCommonPs4JsonFieldList *list, struct NpCommonPs4Allocator *pool)
{
	struct NpCommonPs4JsonField *field;
	struct NpCommonPs4JsonField *next;
	struct NpCommonPs4JsonField *prev;
	struct NpCommonPs4JsonValue *value;

	field = list->head;
	if (field != NULL) {
		do {
			next = field->next;
			prev = field->prev;
			if (next != NULL)
				next->prev = prev;
			else
				list->tail = prev;
			if (field->prev != NULL)
				field->prev->next = field->next;
			else
				list->head = field->next;
			SceNpCommonPs4_38CC9AF5(pool, field->name);
			field->next = NULL;
			field->prev = NULL;
			value = field->value;
			if (value != NULL)
				NP_COMMON_PS4_JSON_DESTROY(value);
			SceNpCommonPs4_38CC9AF5(pool, field);
			field = next;
		} while (field != NULL);
	}
}

struct NpCommonPs4JsonFieldSet *SceNpCommonPs4_A06DF220(struct NpCommonPs4JsonFieldSet *self)
{
	struct NpCommonPs4Allocator *pool;

	pool = self->pool;
	self->vtable = NP_COMMON_PS4_VTABLE(g_jsonFieldSetVtable);
	SceNpCommonPs4_FAABC04C(&self->list, pool);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return self;
}

int SceNpCommonPs4_01846E6A(struct NpCommonPs4JsonFieldSet *self)
{
	struct NpCommonPs4Allocator *pool;

	pool = self->pool;
	self->vtable = NP_COMMON_PS4_VTABLE(g_jsonFieldSetVtable);
	SceNpCommonPs4_FAABC04C(&self->list, pool);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(self);
}

struct NpCommonPs4JsonFieldSet *SceNpCommonPs4_5D8EB418(struct NpCommonPs4JsonFieldSet *self)
{
	struct NpCommonPs4Allocator *pool;

	pool = self->pool;
	self->vtable = NP_COMMON_PS4_VTABLE(g_jsonFieldSetVtable);
	SceNpCommonPs4_FAABC04C(&self->list, pool);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return self;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_470E70C6(struct NpCommonPs4JsonFieldSet *fields, int index, char **pName)
{
	struct NpCommonPs4JsonField *field;

	field = fields->list.head;
	if (field == NULL)
		return NULL;
	while (index != 0) {
		field = field->next;
		--index;
		if (field == NULL)
			return NULL;
	}
	if (pName != NULL)
		*pName = field->name;
	return field->value;
}

struct NpCommonPs4JsonField *SceNpCommonPs4_3F2AADDE(struct NpCommonPs4JsonFieldList *list, const char *name)
{
	struct NpCommonPs4JsonField *field;

	field = list->head;
	if (field == NULL)
		return NULL;
	while (sceClibStrncmp(field->name, name, sceClibStrnlen(name, 0xFFFFFFFF)) != 0) {
		field = field->next;
		if (field == NULL)
			return NULL;
	}
	return field;
}

struct NpCommonPs4JsonValue *SceNpCommonPs4_F81C3F87(struct NpCommonPs4JsonFieldSet *fields, const char *name)
{
	struct NpCommonPs4JsonField *field;

	field = fields->list.head;
	if (field == NULL)
		return NULL;
	while (sceClibStrncmp(field->name, name, sceClibStrnlen(name, 0xFFFFFFFF)) != 0) {
		field = field->next;
		if (field == NULL)
			return NULL;
	}
	return field->value;
}

void *SceNpCommonPs4_0C922423(SceSize size, struct NpCommonPs4Allocator *allocator)
{
	struct NpCommonPs4Allocator **memory;

	memory = (struct NpCommonPs4Allocator **)allocator->allocate(size + 16, allocator->context);
	if (memory != NULL) {
		*memory = allocator;
		return memory + 4;
	}
	return NULL;
}

void *SceNpCommonPs4_67E1FD95(SceSize size, struct NpCommonPs4Allocator *allocator)
{
	struct NpCommonPs4Allocator **memory;

	memory = (struct NpCommonPs4Allocator **)allocator->allocate(size + 16, allocator->context);
	if (memory != NULL) {
		*memory = allocator;
		return memory + 4;
	}
	return NULL;
}

void SceNpCommonPs4_8522485B(void *pointer)
{
	struct NpCommonPs4Allocator **memory;

	if (pointer != NULL) {
		memory = (struct NpCommonPs4Allocator **)pointer - 4;
		(*memory)->deallocate(memory, (*memory)->context);
	}
}

void SceNpCommonPs4_A2E07F27(void *pointer)
{
	struct NpCommonPs4Allocator **memory;

	if (pointer != NULL) {
		memory = (struct NpCommonPs4Allocator **)pointer - 4;
		(*memory)->deallocate(memory, (*memory)->context);
	}
}

__attribute__((noinline)) void *SceNpCommonPs4_D154D84F(SceSize size, struct NpCommonPs4Allocator *pool)
{
	struct NpCommonPs4Allocator *allocator;
	struct NpCommonPs4Allocator **memory;

	allocator = SceNpCommonPs4_31DB2D97(pool);
	memory = (struct NpCommonPs4Allocator **)allocator->allocate(size + 16, allocator->context);
	if (memory != NULL) {
		*memory = allocator;
		return memory + 4;
	}
	return NULL;
}

}
