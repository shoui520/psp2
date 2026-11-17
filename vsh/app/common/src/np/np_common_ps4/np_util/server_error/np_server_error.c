/*
 * SceNpCommonPs4: the error an NP server answers with (np_util/server_error/np_server_error.c): its JSON body read
 * through the lexer, the NP error it carries or the HTTP status made one, and the headers of the requests
 * and responses that concern it.
 */

#include "../../np_common_ps4_internal.h"

/* the lexer's callback for the body of an error: "error":{"code":N} (config 0x81028a40) and also the
   body's "code":N (config 0x81028a8c); the error's code, if a number below 0x1000000 */
int npCommonPs4ServerErrorOnCode(void *lexer, int event, int depth, int node, int arg4,
	const struct NpCommonPs4LexerToken *token, int arg6, struct NpCommonPs4ServerErrorContext *context)
{
	int result;
	int code;

	result = 0;
	if (event == 6 && depth == 1 && node == 0) {
		if (token->type == 3) {
			code = SceNpCommonPs4_32155BF8(token->text, NULL, 10);
			if (code >= 0 && code <= 0xFFFFFF) {
				context->code = code;
				context->valid = 1;
			} else {
				result = NP_COMMON_PS4_ERROR_80550687;
			}
		} else {
			result = NP_COMMON_PS4_ERROR_80550687;
		}
	}
	return result;
}

int npCommonPs4ServerErrorOnCodeEither(void *lexer, int event, int depth, int node, int arg4,
	const struct NpCommonPs4LexerToken *token, int arg6, struct NpCommonPs4ServerErrorContext *context)
{
	int result;
	int code;

	result = 0;
	if (event == 6 && ((depth == 1 && node == 0) || (depth == 0 && node == 1))) {
		if (token->type == 3) {
			code = SceNpCommonPs4_32155BF8(token->text, NULL, 10);
			if (code >= 0 && code <= 0xFFFFFF) {
				context->code = code;
				context->valid = 1;
			} else {
				result = NP_COMMON_PS4_ERROR_80550687;
			}
		} else {
			result = NP_COMMON_PS4_ERROR_80550687;
		}
	}
	return result;
}

int SceNpCommonPs4_AC633131(void *lexer, struct NpCommonPs4ServerErrorContext *context)
{
	if (context != NULL) {
		context->valid = 0;
		context->code = -1;
		return SceNpCommonPs4_738FF77D(lexer, g_serverErrorConfig, (NpCommonPs4LexerCallback)npCommonPs4ServerErrorOnCode, context);
	}
	return NP_COMMON_PS4_ERROR_JSON_INVALID_ARGUMENT;
}

int SceNpCommonPs4_358A8F68(void *lexer, struct NpCommonPs4ServerErrorContext *context)
{
	if (context != NULL) {
		context->valid = 0;
		context->code = -1;
		return SceNpCommonPs4_738FF77D(lexer, g_serverErrorConfigEither, (NpCommonPs4LexerCallback)npCommonPs4ServerErrorOnCodeEither, context);
	}
	return NP_COMMON_PS4_ERROR_JSON_INVALID_ARGUMENT;
}

int SceNpCommonPs4_AD229434(void *lexer, const void *data, int size)
{
	return SceNpCommonPs4_6C941C30(lexer, data, size);
}

/* the NP error of a response: the one its body carries, else its HTTP status made one */
int _sceNpServerErrorJsonGetErrorCode(int status, const void *body, int size, SceUInt8 either)
{
	char lexer[1856];
	struct NpCommonPs4ServerErrorContext context;
	int ret;

	if (body != NULL
		&& size != 0
		&& (sceClibMemset(lexer, 0, 0x740),
			sceClibMemset(&context, 0, 8),
			(either
				? (context.valid = 0,
					context.code = -1,
					SceNpCommonPs4_738FF77D(lexer, g_serverErrorConfigEither, (NpCommonPs4LexerCallback)npCommonPs4ServerErrorOnCodeEither, &context))
				: (context.valid = 0,
					context.code = -1,
					SceNpCommonPs4_738FF77D(lexer, g_serverErrorConfig, (NpCommonPs4LexerCallback)npCommonPs4ServerErrorOnCode, &context)))
				>= 0)
		&& SceNpCommonPs4_6C941C30(lexer, body, size) >= 0
		&& context.valid == 1) {
		if (context.code == -1) {
			ret = 0x82FFFFFF;
			if (status >= 100 && status <= 599)
				ret = status | 0x82F00000;
		} else {
			ret = 0x82FFFFFF;
			if (context.code >= 0 && context.code <= 0xBFFFFF)
				ret = context.code | 0x82000000;
		}
		return ret;
	}
	ret = 0x82FFFFFF;
	if (status >= 100 && status <= 599)
		ret = status | 0x82F00000;
	NP_COMMON_PS4_ASSERT(ret < 0, "_sceNpServerErrorJsonGetErrorCode", "np_util\\server_error\\np_server_error.c", 298, "ret < 0");
	return ret;
}

int SceNpCommonPs4_FB63A152(int status, const void *body, int size)
{
	return _sceNpServerErrorJsonGetErrorCode(status, body, size, 0);
}

int SceNpCommonPs4_CF591432(int status, const void *body, int size)
{
	return _sceNpServerErrorJsonGetErrorCode(status, body, size, 1);
}

int SceNpCommonPs4_69090D80(int requestId)
{
	if (SceNpCommon_BBC13621() == 1)
		return sceHttpAddRequestHeader(requestId, "X-SCE-Debug-Settings", "NpDebug", 0);
	return NP_COMMON_PS4_ERROR_80550689;
}

int SceNpCommonPs4_EF0A4C60(int requestId, const char *prefix)
{
	int result;
	char *header;
	SceSize headerSize;
	const char *fieldValue;
	SceSize length;
	SceSize require;
	SceSize prefixLength;
	SceSize size;
	char notification[1024];
	char escaped[1024];
	char unescaped[1024];

	header = NULL;
	headerSize = 0;
	fieldValue = NULL;
	length = 0;
	require = 0;
	if (SceNpCommon_BBC13621() == 1) {
		sceClibMemset(notification, 0, 0x400);
		sceClibMemset(escaped, 0, 0x400);
		sceClibMemset(unescaped, 0, 0x400);
		result = sceHttpGetAllResponseHeaders(requestId, &header, &headerSize);
		if (result >= 0) {
			result = sceHttpParseResponseHeader(header, headerSize, "X-SCE-Notification", &fieldValue, &length);
			if (result >= 0 && length != 0 && fieldValue != NULL) {
				size = 1023;
				if (length < 0x400)
					size = length;
				sceClibStrncpy(escaped, fieldValue, size);
				result = sceHttpUriUnescape(unescaped, &require, 0x3FF, escaped);
				if (result >= 0 && require != 0 && unescaped[0] != 0) {
					prefixLength = 0;
					if (prefix != NULL) {
						prefixLength = sceClibStrnlen(prefix, 0xFFFFFFFF);
						sceClibStrncpy(notification, prefix, 0x400);
						if (sceClibStrncmp(unescaped, "ServiceLabel", 12) == 0)
							sceClibStrncpy(&notification[prefixLength], " ", 1024 - prefixLength);
						else
							sceClibStrncpy(&notification[prefixLength], "\n", 1024 - prefixLength);
					}
					size = require;
					if (size > 1023 - prefixLength)
						size = 1024 - prefixLength;
					sceClibStrncat(notification, unescaped, size);
					SceNpManager_72936716(notification, 0);
				}
			}
		}
	} else {
		result = NP_COMMON_PS4_ERROR_80550689;
	}
	return result;
}

