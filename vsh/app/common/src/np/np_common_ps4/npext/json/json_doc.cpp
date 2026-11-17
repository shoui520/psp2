/*
 * SceNpCommonPs4: JSON document (npext/json/json_doc.cpp): a parser event handler that builds a tree of
 * JSON values, and the writer that turns a tree back into text. The names of the handler's functions are
 * ours; Sony's names are only known for the writer's.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

extern const void *g_jsonDocumentVtable[];	/* 0x8102a768 */
extern const void *g_jsonObjectBaseVtable[];	/* 0x8102a7b8 */

int npCommonPs4JsonDocumentMakeParser(struct NpCommonPs4JsonDocument *document);
int SceNpCommonPs4_E80812CB(struct NpCommonPs4JsonWriter *writer, struct NpCommonPs4JsonValue *value);
__attribute__((noinline)) int npCommonPs4JsonDocumentBeginObject(struct NpCommonPs4JsonDocument *document);
__attribute__((noinline)) int npCommonPs4JsonDocumentEndObject(struct NpCommonPs4JsonDocument *document);
__attribute__((noinline)) int npCommonPs4JsonDocumentName(struct NpCommonPs4JsonDocument *document, int unused1, int unused2, const char *name);
__attribute__((noinline)) int npCommonPs4JsonDocumentValue(struct NpCommonPs4JsonDocument *document, int unused1, int unused2, int unused3,
	int type, const char *text);
__attribute__((noinline)) int npCommonPs4JsonDocumentBeginArray(struct NpCommonPs4JsonDocument *document);
__attribute__((noinline)) int npCommonPs4JsonDocumentEndArray(struct NpCommonPs4JsonDocument *document);
__attribute__((noinline)) int npCommonPs4JsonDocumentValueUnsigned(struct NpCommonPs4JsonDocument *document, int unused1, int unused2,
	int unused3, int type, const char *text);

struct NpCommonPs4JsonDocument *SceNpCommonPs4_FAE00F18(struct NpCommonPs4JsonDocument *document,
	struct NpCommonPs4Allocator *pool, void *config)
{
	document->pool = pool;
	document->config = config;
	document->vtable = NP_COMMON_PS4_VTABLE(g_jsonDocumentVtable);
	document->handlerVtable = &g_jsonDocumentVtable[13];
	document->parser = NULL;
	document->current = NULL;
	document->name = NULL;
	document->root = NULL;
	return document;
}

struct NpCommonPs4JsonDocument *SceNpCommonPs4_40ED8144(struct NpCommonPs4JsonDocument *document,
	struct NpCommonPs4Allocator *pool, void *config)
{
	document->pool = pool;
	document->config = config;
	document->vtable = NP_COMMON_PS4_VTABLE(g_jsonDocumentVtable);
	document->handlerVtable = &g_jsonDocumentVtable[13];
	document->parser = NULL;
	document->current = NULL;
	document->name = NULL;
	document->root = NULL;
	return document;
}

struct NpCommonPs4JsonDocument *SceNpCommonPs4_B09D64DB(struct NpCommonPs4JsonDocument *document)
{
	struct NpCommonPs4JsonValue *root;
	struct NpCommonPs4JsonParser *parser;

	root = document->root;
	document->vtable = NP_COMMON_PS4_VTABLE(g_jsonDocumentVtable);
	document->handlerVtable = &g_jsonDocumentVtable[13];
	if (root != NULL)
		((void (*)(struct NpCommonPs4JsonValue *))root->vtable[1])(root);
	parser = document->parser;
	if (parser != NULL)
		((void (*)(struct NpCommonPs4JsonParser *))parser->vtable[1])(parser);
	SceNpCommonPs4_38CC9AF5(document->pool, document->name);
	document->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return document;
}

int SceNpCommonPs4_2ACDD442(struct NpCommonPs4JsonDocument *document)
{
	struct NpCommonPs4JsonValue *root;
	struct NpCommonPs4JsonParser *parser;

	root = document->root;
	document->vtable = NP_COMMON_PS4_VTABLE(g_jsonDocumentVtable);
	document->handlerVtable = &g_jsonDocumentVtable[13];
	if (root != NULL)
		((void (*)(struct NpCommonPs4JsonValue *))root->vtable[1])(root);
	parser = document->parser;
	if (parser != NULL)
		((void (*)(struct NpCommonPs4JsonParser *))parser->vtable[1])(parser);
	SceNpCommonPs4_38CC9AF5(document->pool, document->name);
	document->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(document);
}

struct NpCommonPs4JsonDocument *SceNpCommonPs4_0F3C2694(struct NpCommonPs4JsonDocument *document)
{
	struct NpCommonPs4JsonValue *root;
	struct NpCommonPs4JsonParser *parser;

	root = document->root;
	document->vtable = NP_COMMON_PS4_VTABLE(g_jsonDocumentVtable);
	document->handlerVtable = &g_jsonDocumentVtable[13];
	if (root != NULL)
		((void (*)(struct NpCommonPs4JsonValue *))root->vtable[1])(root);
	parser = document->parser;
	if (parser != NULL)
		((void (*)(struct NpCommonPs4JsonParser *))parser->vtable[1])(parser);
	SceNpCommonPs4_38CC9AF5(document->pool, document->name);
	document->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return document;
}

int npCommonPs4JsonDocumentMakeParser(struct NpCommonPs4JsonDocument *document)
{
	struct NpCommonPs4JsonParser *parser;
	void *memory;
	int result;
	struct NpCommonPs4JsonParser *old;

	parser = NULL;
	memory = SceNpCommonPs4_D154D84F(1876, document->pool);
	if (memory == NULL || (parser = SceNpCommonPs4_4577A207((struct NpCommonPs4JsonParser *)memory, document->pool)) == NULL) {
		result = ((int *)document->pool)[4];
fail:
		if (parser != NULL)
			((void (*)(struct NpCommonPs4JsonParser *))parser->vtable[1])(parser);
		return result;
	}
	result = SceNpCommonPs4_89322005(parser, document->config, &document->handlerVtable);
	if (result < 0)
		goto fail;
	old = document->parser;
	if (old != NULL)
		((void (*)(struct NpCommonPs4JsonParser *))old->vtable[1])(old);
	document->parser = parser;
	return 0;
}

int SceNpCommonPs4_9A770D9A(struct NpCommonPs4JsonDocument *document, void *data, int arg)
{
	int result;

	result = npCommonPs4JsonDocumentMakeParser(document);
	if (result >= 0) {
		result = SceNpCommonPs4_FE98E2A7(document->parser, data, arg);
		if (result >= 0)
			return 0;
	}
	return result;
}

int SceNpCommonPs4_B3078188(struct NpCommonPs4JsonDocument *document, struct NpCommonPs4JsonValue **pRoot)
{
	struct NpCommonPs4JsonValue *root;

	root = document->root;
	if (root == NULL)
		return NP_COMMON_PS4_ERROR_80559E0C;
	if (root->type != NP_COMMON_PS4_JSON_TYPE_OBJECT)
		return NP_COMMON_PS4_ERROR_80559E0C;
	*pRoot = root;
	document->root = NULL;
	return 0;
}

#define NP_COMMON_PS4_JSON_DESTROY(value) (((void (*)(struct NpCommonPs4JsonValue *))(value)->vtable[1])(value))

/* append the text to the writer; with no buffer it only counts */
static int jsonAppend(struct NpCommonPs4JsonWriter *writer, const char *text)
{
	if (writer->buffer != NULL &&
	    sceClibStrlcpy(writer->buffer + writer->length, text, writer->size - writer->length) >= writer->size - writer->length)
		return NP_COMMON_PS4_ERROR_80559E0D;
	writer->length += sceClibStrnlen(text, 0xFFFFFFFF);
	return 0;
}

/* add a value to the container the document is inside of */
static int jsonDocumentAdd(struct NpCommonPs4JsonDocument *document, struct NpCommonPs4JsonValue *value)
{
	struct NpCommonPs4JsonValue *current;
	int result;
	int type;

	current = document->current;
	if (current == NULL)
		return NP_COMMON_PS4_ERROR_80559E0C;
	type = current->type;
	if (type == NP_COMMON_PS4_JSON_TYPE_ARRAY) {
		result = SceNpCommonPs4_413D4254(current, value);
		if (result >= 0)
			return 0;
		return result;
	}
	if (type != NP_COMMON_PS4_JSON_TYPE_OBJECT)
		return NP_COMMON_PS4_ERROR_80559E0C;
	if (document->name == NULL)
		return NP_COMMON_PS4_ERROR_80559E0C;
	result = SceNpCommonPs4_7833EEC2(current, document->name, value);
	if (result < 0)
		return result;
	SceNpCommonPs4_38CC9AF5(document->pool, document->name);
	document->name = NULL;
	return 0;
}

__attribute__((noinline)) int npCommonPs4JsonDocumentBeginObject(struct NpCommonPs4JsonDocument *document)
{
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonValue *memory;
	int result;

	value = NULL;
	memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, document->pool);
	if (memory != NULL) {
		SceNpCommonPs4_D309C0B3(memory, document->pool, NP_COMMON_PS4_JSON_TYPE_OBJECT);
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonObjectValueVtable);
		value = memory;
		if (document->root == NULL) {
			document->root = memory;
			document->current = value;
			return 0;
		}
		result = jsonDocumentAdd(document, value);
		if (result >= 0) {
			document->current = value;
			return 0;
		}
	} else {
		result = ((int *)document->pool)[4];
	}
	if (value != NULL)
		NP_COMMON_PS4_JSON_DESTROY(value);
	return result;
}

__attribute__((noinline)) int npCommonPs4JsonDocumentEndObject(struct NpCommonPs4JsonDocument *document)
{
	document->current = document->current->parent;
	return 0;
}

__attribute__((noinline)) int npCommonPs4JsonDocumentName(struct NpCommonPs4JsonDocument *document, int unused1, int unused2, const char *name)
{
	char *copy;

	if (document->name != NULL) {
		((void (*)(struct NpCommonPs4Allocator *, void *, int))SceNpCommonPs4_38CC9AF5)(document->pool, document->name,
			unused2);
		document->name = NULL;
	}
	copy = npCommonPs4AllocatorDuplicateString(document->pool, name);
	document->name = copy;
	if (copy != NULL)
		return 0;
	return ((int *)document->pool)[4];
}

/* make the value an event describes: 0 null, 1 true, 2 false, 3 number, 4 string */
static int jsonDocumentCreateValue(struct NpCommonPs4JsonDocument *document, struct NpCommonPs4JsonValue **pValue,
	int type, const char *text)
{
	struct NpCommonPs4JsonValue *memory;
	struct NpCommonPs4JsonValue *value;
	int result;
	int error;

	*pValue = NULL;
	switch (type) {
	case 0:
		memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, document->pool);
		value = NULL;
		if (memory != NULL) {
			SceNpCommonPs4_D309C0B3(memory, document->pool, NP_COMMON_PS4_JSON_TYPE_NULL);
			memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonNullValueVtable);
			value = memory;
		}
		if (value != NULL) {
			result = 0;
			memory = value;
		} else {
			result = ((int *)document->pool)[4];
		}
		if (result >= 0) {
			*pValue = memory;
			return 0;
		}
		return result;
	case 1:
		memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, document->pool);
		value = NULL;
		if (memory != NULL) {
			SceNpCommonPs4_D309C0B3(memory, document->pool, NP_COMMON_PS4_JSON_TYPE_BOOL);
			memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonBoolValueVtable);
			value = memory;
		}
		if (value != NULL) {
			error = SceNpCommonPs4_AE224009(value, 1);
			if (error >= 0) {
				result = 0;
				memory = value;
				if (result >= 0) {
					*pValue = memory;
					return 0;
				}
				return result;
			}
		} else {
			error = ((int *)document->pool)[4];
		}
		if (value != NULL)
			NP_COMMON_PS4_JSON_DESTROY(value);
		result = error;
		if (result >= 0) {
			*pValue = memory;
			return 0;
		}
		return result;
	case 2:
		memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, document->pool);
		value = NULL;
		if (memory != NULL) {
			SceNpCommonPs4_D309C0B3(memory, document->pool, NP_COMMON_PS4_JSON_TYPE_BOOL);
			memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonBoolValueVtable);
			value = memory;
		}
		if (value != NULL) {
			error = SceNpCommonPs4_AE224009(value, 0);
			if (error >= 0) {
				result = 0;
				memory = value;
				if (result >= 0) {
					*pValue = memory;
					return 0;
				}
				return result;
			}
		} else {
			error = ((int *)document->pool)[4];
		}
		if (value != NULL)
			NP_COMMON_PS4_JSON_DESTROY(value);
		result = error;
		if (result >= 0) {
			*pValue = memory;
			return 0;
		}
		return result;
	case 3:
		memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, document->pool);
		value = NULL;
		if (memory != NULL) {
			SceNpCommonPs4_D309C0B3(memory, document->pool, NP_COMMON_PS4_JSON_TYPE_NUMBER);
			memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonNumberValueVtable);
			value = memory;
		}
		if (value != NULL) {
			error = SceNpCommonPs4_943736E5(value, text);
			if (error >= 0) {
				result = 0;
				memory = value;
				if (result >= 0) {
					*pValue = memory;
					return 0;
				}
				return result;
			}
		} else {
			error = ((int *)document->pool)[4];
		}
		if (value != NULL)
			NP_COMMON_PS4_JSON_DESTROY(value);
		result = error;
		if (result >= 0) {
			*pValue = memory;
			return 0;
		}
		return result;
	case 4:
		memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, document->pool);
		value = NULL;
		if (memory != NULL) {
			SceNpCommonPs4_D309C0B3(memory, document->pool, NP_COMMON_PS4_JSON_TYPE_STRING);
			memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonStringValueVtable);
			value = memory;
		}
		if (value != NULL) {
			error = SceNpCommonPs4_CAD8A67F(value, text);
			if (error >= 0) {
				result = 0;
				memory = value;
				if (result >= 0) {
					*pValue = memory;
					return 0;
				}
				return result;
			}
		} else {
			error = ((int *)document->pool)[4];
		}
		if (value != NULL)
			NP_COMMON_PS4_JSON_DESTROY(value);
		result = error;
		if (result >= 0) {
			*pValue = memory;
			return 0;
		}
		return result;
	default:
		return 0;
	}
done:
	if (result >= 0) {
		*pValue = memory;
		return 0;
	}
	return result;
}

__attribute__((noinline)) int npCommonPs4JsonDocumentValue(struct NpCommonPs4JsonDocument *document, int unused1, int unused2, int unused3,
	int type, const char *text)
{
	struct NpCommonPs4JsonValue *value;
	int result;

	value = NULL;
	result = jsonDocumentCreateValue(document, &value, type, text);
	if (result < 0 || (result = (document->root != NULL ? jsonDocumentAdd(document, value) : NP_COMMON_PS4_ERROR_80559E0C)) < 0) {
		if (value != NULL)
			NP_COMMON_PS4_JSON_DESTROY(value);
		return result;
	}
	return 0;
}

__attribute__((noinline)) int npCommonPs4JsonDocumentBeginArray(struct NpCommonPs4JsonDocument *document)
{
	struct NpCommonPs4JsonValue *value;
	struct NpCommonPs4JsonValue *memory;
	int result;

	value = NULL;
	memory = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_D154D84F(20, document->pool);
	if (memory != NULL) {
		SceNpCommonPs4_D309C0B3(memory, document->pool, NP_COMMON_PS4_JSON_TYPE_ARRAY);
		memory->vtable = NP_COMMON_PS4_VTABLE(g_jsonArrayValueVtable);
		value = memory;
		if (document->root == NULL) {
			document->root = memory;
			document->current = value;
			return 0;
		}
		result = jsonDocumentAdd(document, value);
		if (result >= 0) {
			document->current = value;
			return 0;
		}
	} else {
		result = ((int *)document->pool)[4];
	}
	if (value != NULL)
		NP_COMMON_PS4_JSON_DESTROY(value);
	return result;
}

__attribute__((noinline)) int npCommonPs4JsonDocumentEndArray(struct NpCommonPs4JsonDocument *document)
{
	document->current = document->current->parent;
	return 0;
}

__attribute__((noinline)) int npCommonPs4JsonDocumentValueUnsigned(struct NpCommonPs4JsonDocument *document, int unused1, int unused2,
	int unused3, int type, const char *text)
{
	struct NpCommonPs4JsonValue *value;
	int result;

	value = NULL;
	result = jsonDocumentCreateValue(document, &value, type, text);
	if (result < 0 || (result = (document->root != NULL ? jsonDocumentAdd(document, value) : NP_COMMON_PS4_ERROR_80559E0C)) < 0) {
		if (value != NULL)
			NP_COMMON_PS4_JSON_DESTROY(value);
		return result;
	}
	return 0;
}

struct NpCommonPs4JsonObject *SceNpCommonPs4_4A1E54C6(struct NpCommonPs4JsonObject *object, struct NpCommonPs4Allocator *pool)
{
	object->pool = pool;
	object->vtable = NP_COMMON_PS4_VTABLE(g_jsonObjectBaseVtable);
	return object;
}

struct NpCommonPs4JsonObject *SceNpCommonPs4_497FE2B3(struct NpCommonPs4JsonObject *object, struct NpCommonPs4Allocator *pool)
{
	object->pool = pool;
	object->vtable = NP_COMMON_PS4_VTABLE(g_jsonObjectBaseVtable);
	return object;
}

struct NpCommonPs4JsonObject *SceNpCommonPs4_74F1FA26(struct NpCommonPs4JsonObject *object)
{
	object->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return object;
}

int SceNpCommonPs4_D9E5F9C1(struct NpCommonPs4JsonObject *object)
{
	object->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(object);
}

struct NpCommonPs4JsonObject *SceNpCommonPs4_06A8F278(struct NpCommonPs4JsonObject *object)
{
	object->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return object;
}

int SceNpCommonPs4_0DE9CE07(struct NpCommonPs4JsonWriter *writer, const char *text)
{
	if (writer->buffer != NULL &&
	    sceClibStrlcpy(writer->buffer + writer->length, text, writer->size - writer->length) >= writer->size - writer->length)
		return NP_COMMON_PS4_ERROR_80559E0D;
	writer->length += sceClibStrnlen(text, 0xFFFFFFFF);
	return 0;
}

int SceNpCommonPs4_70DA387B(struct NpCommonPs4JsonWriter *writer, const char *src)
{
	const char *p;
	const char *start;
	const char *last;
	int ch;
	SceSize length;
	SceSize offset;
	int result;

	p = src;
	NP_COMMON_PS4_ASSERT(src != NULL, "buildEscapedStr", "npext\\json\\json_doc.cpp", 597, "str != 0");
	ch = *p;
	while (ch != 0) {
		start = p;
		while (1) {
			last = p;
			if (ch == 0 || ch == '"' || ch == '\\' || ch == '/' || ch == 8 || ch == 12 || ch == 10 || ch == 13 || ch == 9)
				break;
			ch = *++p;
		}
		if (start < p) {
			length = p - start;
			if (writer->buffer != NULL) {
				offset = writer->length;
				if (writer->size - offset < length + 1)
					return NP_COMMON_PS4_ERROR_80559E0D;
				sceClibStrncpy(writer->buffer + offset, start, p - start);
				writer->buffer[writer->length + length] = 0;
			}
			writer->length += length;
			ch = *p;
		}
		switch (ch) {
		case 8:
			result = jsonAppend(writer, "\\b");
			if (result < 0)
				return result;
			p++;
			ch = last[1];
			break;
		case 9:
			result = jsonAppend(writer, "\\t");
			if (result < 0)
				return result;
			p++;
			ch = last[1];
			break;
		case 10:
			result = jsonAppend(writer, "\\n");
			if (result < 0)
				return result;
			p++;
			ch = last[1];
			break;
		case 11:
			break;
		case 12:
			result = jsonAppend(writer, "\\f");
			if (result < 0)
				return result;
			p++;
			ch = last[1];
			break;
		case 13:
			result = jsonAppend(writer, "\\r");
			if (result < 0)
				return result;
			p++;
			ch = last[1];
			break;
		case 34:
			result = jsonAppend(writer, "\\\"");
			if (result < 0)
				return result;
			p++;
			ch = last[1];
			break;
		case 47:
			result = jsonAppend(writer, "\\/");
			if (result < 0)
				return result;
			p++;
			ch = last[1];
			break;
		case 92:
			result = jsonAppend(writer, "\\\\");
			if (result < 0)
				return result;
			p++;
			ch = last[1];
			break;
		}
	}
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_EFFAF9E0(struct NpCommonPs4JsonWriter *writer, struct NpCommonPs4JsonValue *object)
{
	int result;
	int count;
	int i;
	struct NpCommonPs4JsonValue *value;
	char *name;

	result = jsonAppend(writer, "{");
	if (result >= 0) {
		count = SceNpCommonPs4_9C3F1645(object);
		i = 0;
		if (count > 0) {
			while (1) {
				if (i != 0) {
					result = jsonAppend(writer, ",");
					if (result < 0)
						break;
				}
				value = (struct NpCommonPs4JsonValue *)SceNpCommonPs4_713C8A08(object, i, &name);
				NP_COMMON_PS4_ASSERT(value != NULL, "buildObject", "npext\\json\\json_doc.cpp", 812, "value");
				NP_COMMON_PS4_ASSERT(name != NULL, "buildObject", "npext\\json\\json_doc.cpp", 813, "name");
				result = jsonAppend(writer, "\"");
				if (result < 0)
					break;
				result = SceNpCommonPs4_70DA387B(writer, name);
				if (result < 0)
					break;
				result = jsonAppend(writer, "\"");
				if (result < 0)
					break;
				result = jsonAppend(writer, ":");
				if (result < 0)
					break;
				result = SceNpCommonPs4_E80812CB(writer, value);
				if (result < 0)
					break;
				if (++i >= count)
					goto close;
			}
		} else {
close:
			result = jsonAppend(writer, "}");
			if (result >= 0)
				result = 0;
		}
	}
	return result;
}

__attribute__((noinline)) int SceNpCommonPs4_1FB6D6BD(struct NpCommonPs4JsonWriter *writer, struct NpCommonPs4JsonValue *array)
{
	int result;
	int count;
	int i;
	struct NpCommonPs4JsonValue *item;

	result = jsonAppend(writer, "[");
	if (result >= 0) {
		count = SceNpCommonPs4_1E21D357(array);
		i = 0;
		if (count > 0) {
			while (1) {
				if (i != 0) {
					result = jsonAppend(writer, ",");
					if (result < 0)
						break;
				}
				item = SceNpCommonPs4_2A4A268C(array, i);
				NP_COMMON_PS4_ASSERT(item != NULL, "buildArray", "npext\\json\\json_doc.cpp", 772, "item_value");
				result = SceNpCommonPs4_E80812CB(writer, item);
				if (result < 0)
					break;
				if (++i >= count)
					goto close;
			}
		} else {
close:
			result = jsonAppend(writer, "]");
			if (result >= 0)
				return 0;
		}
	}
	return result;
}

__attribute__((noinline)) int SceNpCommonPs4_8037715A(struct NpCommonPs4JsonWriter *writer, struct NpCommonPs4JsonValue *string)
{
	const char *text;
	int result;

	text = SceNpCommonPs4_ED539A9D(string);
	result = jsonAppend(writer, "\"");
	if (result >= 0) {
		result = SceNpCommonPs4_70DA387B(writer, text);
		if (result >= 0) {
			result = jsonAppend(writer, "\"");
			if (result >= 0)
				return 0;
		}
	}
	return result;
}

__attribute__((noinline)) int SceNpCommonPs4_893ADD4A(struct NpCommonPs4JsonWriter *writer, struct NpCommonPs4JsonValue *number)
{
	const char *text;

	text = SceNpCommonPs4_FAE43676(number);
	return jsonAppend(writer, text);
}

__attribute__((noinline)) int SceNpCommonPs4_6350296C(struct NpCommonPs4JsonWriter *writer, struct NpCommonPs4JsonValue *boolean)
{
	const char *text;

	text = "false";
	if (SceNpCommonPs4_C1277FAD(boolean) != 0)
		text = "true";
	return jsonAppend(writer, text);
}

__attribute__((noinline)) int SceNpCommonPs4_D8AC411E(struct NpCommonPs4JsonWriter *writer)
{
	return jsonAppend(writer, "null");
}

int SceNpCommonPs4_E80812CB(struct NpCommonPs4JsonWriter *writer, struct NpCommonPs4JsonValue *value)
{
	int result;

	result = (int)value;
	switch (value->type) {
	case NP_COMMON_PS4_JSON_TYPE_NULL:
		result = SceNpCommonPs4_D8AC411E(writer);
		break;
	case NP_COMMON_PS4_JSON_TYPE_BOOL:
		result = SceNpCommonPs4_6350296C(writer, npCommonPs4JsonValueAsBool(value));
		break;
	case NP_COMMON_PS4_JSON_TYPE_NUMBER:
		result = jsonAppend(writer, SceNpCommonPs4_FAE43676(npCommonPs4JsonValueAsNumber(value)));
		break;
	case NP_COMMON_PS4_JSON_TYPE_STRING:
		result = SceNpCommonPs4_8037715A(writer, npCommonPs4JsonValueAsString(value));
		break;
	case NP_COMMON_PS4_JSON_TYPE_ARRAY:
		result = SceNpCommonPs4_1FB6D6BD(writer, npCommonPs4JsonValueAsArray(value));
		break;
	case NP_COMMON_PS4_JSON_TYPE_OBJECT:
		result = SceNpCommonPs4_EFFAF9E0(writer, SceNpCommonPs4_15A2D6E3(value));
		break;
	default:
		return result;
	}
	return result;
}

int SceNpCommonPs4_4A94BDE6(struct NpCommonPs4JsonTree *document, void *buffer, SceSize size, SceSize *pRequired)
{
	struct NpCommonPs4JsonWriter writer;
	int result;

	writer.buffer = (char *)buffer;
	writer.size = size;
	writer.length = 0;
	if (buffer != NULL)
		sceClibMemset(buffer, 0, size);
	result = SceNpCommonPs4_E80812CB(&writer, document->root);
	if (result >= 0) {
		if (pRequired != NULL)
			*pRequired = writer.length + 1;
		result = 0;
	}
	return result;
}

int SceNpCommonPs4_5F8A1E12(struct NpCommonPs4JsonTree *document)
{
	struct NpCommonPs4JsonWriter writer;
	int size;

	writer.size = 0;
	writer.length = 0;
	writer.buffer = NULL;
	if (SceNpCommonPs4_E80812CB(&writer, document->root) < 0) {
		NP_COMMON_PS4_ASSERT(0, "BuildBufSize", "npext\\json\\json_doc.cpp", 899, "ret == 0");
	} else {
		size = writer.length + 1;
	}
	return size;
}

int SceNpCommonPs4_95A72E4A(const char *src, void *buffer, SceSize size, SceSize *pRequired)
{
	struct NpCommonPs4JsonWriter writer;
	int result;

	writer.buffer = (char *)buffer;
	writer.size = size;
	writer.length = 0;
	if (buffer != NULL)
		sceClibMemset(buffer, 0, size);
	if (src != NULL) {
		result = SceNpCommonPs4_70DA387B(&writer, src);
		if (result >= 0) {
			if (pRequired != NULL)
				*pRequired = writer.length + 1;
			result = 0;
		}
	} else {
		result = NP_COMMON_PS4_ERROR_80559E11;
	}
	return result;
}

int SceNpCommonPs4_CADA66A8(const char *src)
{
	struct NpCommonPs4JsonWriter writer;
	int size;

	writer.buffer = NULL;
	writer.size = 0;
	writer.length = 0;
	if (src != NULL && SceNpCommonPs4_70DA387B(&writer, src) >= 0) {
		size = writer.length + 1;
	} else {
		NP_COMMON_PS4_ASSERT(0, "EscapeJsonStringBufSize", "npext\\json\\json_doc.cpp", 935, "ret == 0");
	}
	return size;
}

int npCommonPs4JsonDocumentBeginObjectThunk(char *handler)
{
	return npCommonPs4JsonDocumentBeginObject((struct NpCommonPs4JsonDocument *)(handler - 4));
}

int npCommonPs4JsonDocumentEndObjectThunk(char *handler)
{
	return npCommonPs4JsonDocumentEndObject((struct NpCommonPs4JsonDocument *)(handler - 4));
}

int npCommonPs4JsonDocumentNameThunk(char *handler, int unused1, int unused2, const char *name)
{
	return npCommonPs4JsonDocumentName((struct NpCommonPs4JsonDocument *)(handler - 4), unused1, unused2, name);
}

int SceNpCommonPs4_635808F6(char *handler, int unused1, int unused2, int unused3, int type, const char *text)
{
	return npCommonPs4JsonDocumentValue((struct NpCommonPs4JsonDocument *)(handler - 4), unused1, unused2, unused3,
		type, text);
}

int npCommonPs4JsonDocumentBeginArrayThunk(char *handler)
{
	return npCommonPs4JsonDocumentBeginArray((struct NpCommonPs4JsonDocument *)(handler - 4));
}

int npCommonPs4JsonDocumentEndArrayThunk(char *handler)
{
	return npCommonPs4JsonDocumentEndArray((struct NpCommonPs4JsonDocument *)(handler - 4));
}

int SceNpCommonPs4_75A33E28(char *handler, int unused1, int unused2, int unused3, int type, const char *text)
{
	return npCommonPs4JsonDocumentValueUnsigned((struct NpCommonPs4JsonDocument *)(handler - 4), unused1, unused2,
		unused3, type, text);
}

const void *g_jsonDocumentVtable[] = {		/* 0x8102a768: the vtables of the document and of its handler base */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_B09D64DB,
	(const void *)SceNpCommonPs4_2ACDD442,
	(const void *)npCommonPs4JsonDocumentBeginObject,
	(const void *)npCommonPs4JsonDocumentEndObject,
	(const void *)npCommonPs4JsonDocumentName,
	(const void *)npCommonPs4JsonDocumentValue,
	(const void *)npCommonPs4JsonDocumentBeginArray,
	(const void *)npCommonPs4JsonDocumentEndArray,
	(const void *)npCommonPs4JsonDocumentValueUnsigned,
	(const void *)-4,
	NULL,
	(const void *)npCommonPs4JsonDocumentBeginObjectThunk,
	(const void *)npCommonPs4JsonDocumentEndObjectThunk,
	(const void *)npCommonPs4JsonDocumentNameThunk,
	(const void *)SceNpCommonPs4_635808F6,
	(const void *)npCommonPs4JsonDocumentBeginArrayThunk,
	(const void *)npCommonPs4JsonDocumentEndArrayThunk,
	(const void *)SceNpCommonPs4_75A33E28,
};

const void *g_jsonObjectBaseVtable[] = {	/* 0x8102a7b8 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_74F1FA26,
	(const void *)SceNpCommonPs4_D9E5F9C1,
};

}
