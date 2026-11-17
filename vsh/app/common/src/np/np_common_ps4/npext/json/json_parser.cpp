/*
 * SceNpCommonPs4: JSON parser (npext/json/json_parser.cpp): the lexer's events turned into calls of a
 * handler object, which json_doc.cpp builds a tree of values from.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

extern const void *g_jsonParserVtable[];	/* 0x8102a7c8 */

int SceNpCommonPs4_E20FCABC(int a1, int a2, int a3, int a4, const char *name, int *value, int sign, void *parser);

struct NpCommonPs4JsonParser *SceNpCommonPs4_4577A207(struct NpCommonPs4JsonParser *parser, struct NpCommonPs4Allocator *pool)
{
	parser->pool = pool;
	parser->vtable = NP_COMMON_PS4_VTABLE(g_jsonParserVtable);
	parser->buffer = NULL;
	parser->handler = NULL;
	parser->bufferSize = 0;
	return parser;
}

struct NpCommonPs4JsonParser *SceNpCommonPs4_4F720AFD(struct NpCommonPs4JsonParser *parser, struct NpCommonPs4Allocator *pool)
{
	parser->pool = pool;
	parser->vtable = NP_COMMON_PS4_VTABLE(g_jsonParserVtable);
	parser->buffer = NULL;
	parser->handler = NULL;
	parser->bufferSize = 0;
	return parser;
}

struct NpCommonPs4JsonParser *SceNpCommonPs4_582CD582(struct NpCommonPs4JsonParser *parser)
{
	parser->vtable = NP_COMMON_PS4_VTABLE(g_jsonParserVtable);
	SceNpCommonPs4_38CC9AF5(parser->pool, parser->buffer);
	parser->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return parser;
}

int SceNpCommonPs4_270FF1F6(struct NpCommonPs4JsonParser *parser)
{
	parser->vtable = NP_COMMON_PS4_VTABLE(g_jsonParserVtable);
	SceNpCommonPs4_38CC9AF5(parser->pool, parser->buffer);
	parser->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(parser);
}

struct NpCommonPs4JsonParser *SceNpCommonPs4_0A3AA2EF(struct NpCommonPs4JsonParser *parser)
{
	parser->vtable = NP_COMMON_PS4_VTABLE(g_jsonParserVtable);
	SceNpCommonPs4_38CC9AF5(parser->pool, parser->buffer);
	parser->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return parser;
}

int SceNpCommonPs4_89322005(struct NpCommonPs4JsonParser *parser, void *config, const void *const *const *eventHandler)
{
	int result;

	NP_COMMON_PS4_ASSERT(eventHandler != NULL, "Init", "npext\\json\\json_parser.cpp", 36, "eventHandler != 0");
	result = SceNpCommonPs4_EB1B7375(parser->lexer, config, (void *)SceNpCommonPs4_E20FCABC, parser);
	if (result >= 0) {
		parser->handler = eventHandler;
		return 0;
	}
	return result;
}

int SceNpCommonPs4_FE98E2A7(struct NpCommonPs4JsonParser *parser, void *data, int size)
{
	int result;

	result = SceNpCommonPs4_F5EE219F(parser->lexer, data, size);
	if (result >= 0)
		return 0;
	return result;
}

int SceNpCommonPs4_7CC0800E(struct NpCommonPs4JsonParser *parser, int unused, void *stream)
{
	char *buffer;
	int result;
	int error;
	SceSize length;

	buffer = parser->buffer;
	if (buffer == NULL) {
		buffer = (char *)SceNpCommonPs4_299ADFC3(parser->pool, 2048);
		parser->buffer = buffer;
		if (buffer == NULL)
			return ((int *)parser->pool)[4];
		parser->bufferSize = 2048;
	}
	result = ((int (*)(void *, int, char *, SceSize, SceSize *))(*(const void *const **)stream)[2])(stream, unused, buffer,
		parser->bufferSize, &length);
	if (result >= 0) {
		if (length != 0) {
			error = SceNpCommonPs4_F5EE219F(parser->lexer, parser->buffer, length);
			result = 0;
			if (error < 0)
				result = error;
			if (result >= 0)
				result = 0;
		} else {
			result = 0;
		}
	}
	return result;
}

int SceNpCommonPs4_98D2A52F(struct NpCommonPs4JsonParser *parser, int unused, int event, int a4, int a5,
	const char *name, int *value, int sign)
{
	int result;

	switch (event) {
	case 1:
		result = ((int (*)(const void *, int, int))(*parser->handler)[0])(parser->handler, a4, a5);
		break;
	case 2:
		result = ((int (*)(const void *, int, int))(*parser->handler)[1])(parser->handler, a4, a5);
		break;
	case 3:
		result = ((int (*)(const void *, int, int))(*parser->handler)[4])(parser->handler, a4, a5);
		break;
	case 4:
		result = ((int (*)(const void *, int, int))(*parser->handler)[5])(parser->handler, a4, a5);
		break;
	case 5:
		NP_COMMON_PS4_ASSERT(name != NULL, "jsonParseCallback", "npext\\json\\json_parser.cpp", 185, "name != 0");
		result = ((int (*)(const void *, int, int, const char *))(*parser->handler)[2])(parser->handler, a4, a5, name);
		break;
	case 6:
		NP_COMMON_PS4_ASSERT(value != NULL, "jsonParseCallback", "npext\\json\\json_parser.cpp", 189, "value != 0");
		if (sign < 0)
			result = ((int (*)(const void *, int, int, const char *, int, int *))(*parser->handler)[3])(parser->handler, a4,
				a5, name, *value, value + 1);
		else
			result = ((int (*)(const void *, int, int, int, int, int *))(*parser->handler)[6])(parser->handler, a4, a5,
				sign, *value, value + 1);
		break;
	default:
		result = 0;
		break;
	}
	return result;
}

int SceNpCommonPs4_E20FCABC(int a1, int a2, int a3, int a4, const char *name, int *value, int sign, void *parser)
{
	return SceNpCommonPs4_98D2A52F((struct NpCommonPs4JsonParser *)parser, a1, a2, a3, a4, name, value, sign);
}

const void *g_jsonParserVtable[] = {		/* 0x8102a7c8 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_582CD582,
	(const void *)SceNpCommonPs4_270FF1F6,
};

}
