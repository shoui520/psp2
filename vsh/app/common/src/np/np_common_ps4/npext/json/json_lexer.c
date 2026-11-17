/*
 * SceNpCommonPs4: the JSON lexer (npext/json/json_lexer.c): a text taken in pieces, its objects and arrays
 * matched against a config of paths, the matched ones told to a callback (object and array begin and end,
 * keys, values). The file's name and the functions' are ours.
 */

#include "../../np_common_ps4_internal.h"

#define LEXER_ERROR_DEPTH	-2141911549	/* 0x80550603 */

#define LEXER_STATE_START	0
#define LEXER_STATE_OBJECT_BEGIN	1
#define LEXER_STATE_OBJECT_END	2
#define LEXER_STATE_ARRAY_BEGIN	3
#define LEXER_STATE_ARRAY_END	4
#define LEXER_STATE_KEY_BEGIN	5
#define LEXER_STATE_KEY	6
#define LEXER_STATE_COLON	7
#define LEXER_STATE_VALUE	8
#define LEXER_STATE_NEXT	9
#define LEXER_STATE_LITERAL	10
#define LEXER_STATE_NUMBER	11
#define LEXER_STATE_STRING	12
#define LEXER_STATE_SKIP	13

#define LEXER_MODE_ANY	1

/* the node of a depth's list that the object or array being read matches, and its flag */
int npCommonPs4LexerMatch(const struct NpCommonPs4LexerConfig *config, SceSize depth, int mode, int kind, const char *name,
	int *pIndex, int *pFlag)
{
	const struct NpCommonPs4LexerNode *node;
	int index;
	int result;

	result = 0;
	if (pIndex != NULL)
		*pIndex = -1;
	if (pFlag != NULL)
		*pFlag = 0;
	if (depth < config->numLevels) {
		node = config->levels[depth];
		index = 0;
		while (node->kind == NP_COMMON_PS4_LEXER_NODE_KEY || node->kind == NP_COMMON_PS4_LEXER_NODE_ELEMENT) {
			if (mode == LEXER_MODE_ANY) {
				if (node->name == NULL) {
					result = 1;
					if (pIndex != NULL)
						*pIndex = index;
					if (pFlag != NULL)
						*pFlag = node->flag;
					return result;
				}
			} else if (kind == NP_COMMON_PS4_LEXER_NODE_KEY && node->kind == NP_COMMON_PS4_LEXER_NODE_KEY) {
				if (name != NULL && node->name != NULL && sceClibStrncasecmp(node->name, name, 0x80) == 0) {
					result = 1;
					if (pIndex != NULL)
						*pIndex = index;
					if (pFlag != NULL)
						*pFlag = node->flag;
					return result;
				}
			} else if (kind == NP_COMMON_PS4_LEXER_NODE_ELEMENT && node->kind == NP_COMMON_PS4_LEXER_NODE_ELEMENT) {
				result = 1;
				if (pIndex != NULL)
					*pIndex = index;
				if (pFlag != NULL)
					*pFlag = node->flag;
				return result;
			}
			node++;
			index++;
		}
	}
	return result;
}

/* the next character of a string (an escape, a UTF-8 sequence) from the text, or from what the last piece left
   and this one completes; 0 when the piece ends inside one */
int npCommonPs4LexerCopyChar(const char *src, SceSize length, SceSize *pConsumed, char *partial, SceSize *pPartialLength,
	SceSize *pRemaining, char *decoded, SceSize *pSourceLength, SceSize *pLength)
{
	int result;
	int ret;
	SceSize size;

	result = 1;
	if (*pRemaining != 0) {
		if (*pRemaining <= length) {
			*pConsumed = *pRemaining;
			size = *pRemaining;
		} else {
			*pConsumed = length;
			size = length;
		}
		sceClibMemcpy_safe(&partial[*pPartialLength], src, size);
		*pPartialLength += *pConsumed;
		*pRemaining -= *pConsumed;
		if (*pRemaining != 0)
			return 0;
		ret = SceNpCommonPs4_12AA95E3(decoded, 4, partial, *pPartialLength, pLength, (int *)pSourceLength);
		if (ret == NP_COMMON_PS4_ERROR_JSON_INVALID_STRING) {
			result = 0;
			*pRemaining = *pSourceLength - *pPartialLength;
		} else if (ret >= 0) {
			*pRemaining = 0;
			*pPartialLength = 0;
		} else {
			*pSourceLength = 1;
			*pLength = 1;
			sceClibMemcpy_safe(decoded, partial, 1);
		}
	} else {
		size = length;
		if (length >= 6)
			size = 6;
		*pConsumed = size;
		ret = SceNpCommonPs4_12AA95E3(decoded, 4, src, size, pLength, (int *)pSourceLength);
		if (ret == NP_COMMON_PS4_ERROR_JSON_INVALID_STRING) {
			sceClibMemcpy_safe(partial, src, *pConsumed);
			result = 0;
			*pPartialLength = *pConsumed;
			*pRemaining = *pSourceLength - *pConsumed;
		} else if (ret >= 0) {
			*pConsumed = *pSourceLength;
		} else {
			*pSourceLength = 1;
			*pLength = 1;
			sceClibMemcpy_safe(decoded, src, 1);
			*pConsumed = 1;
		}
	}
	return result;
}

int SceNpCommonPs4_738FF77D(struct NpCommonPs4Lexer *lexer, const struct NpCommonPs4LexerConfig *config,
	NpCommonPs4LexerCallback callback, void *context)
{
	if (lexer == NULL)
		return NP_COMMON_PS4_ERROR_JSON_INVALID_ARGUMENT;
	sceClibMemset(lexer, 0, 0x740);
	lexer->config = config;
	lexer->callback = callback;
	lexer->context = context;
	lexer->state = LEXER_STATE_START;
	lexer->depth = 0;
	lexer->mode = LEXER_MODE_ANY;
	return 0;
}

/* a value read: the callback told of it, the key it belongs to if it is a member of an object */
#define LEXER_VALUE_READ(lexer, result) \
	((result) = (lexer)->callback((lexer), NP_COMMON_PS4_LEXER_EVENT_VALUE, (lexer)->depth, \
		(lexer)->nodeIndex[(lexer)->depth], \
		(lexer)->arrayIndex[(lexer)->depth] == -1 ? (lexer)->name : NULL, &(lexer)->token, \
		(lexer)->arrayIndex[(lexer)->depth], (lexer)->context))

/* an object or array ended: the callback told of it, at the depth it was begun at */
#define LEXER_END(lexer, event, result) \
	((result) = (lexer)->callback((lexer), (event), (lexer)->depth, (lexer)->nodeIndex[(lexer)->depth], NULL, NULL, \
		(lexer)->arrayIndex[(lexer)->depth], (lexer)->context))

int SceNpCommonPs4_6C941C30(struct NpCommonPs4Lexer *lexer, const char *src, int size)
{
	const char *p;
	int pos;
	int state;
	int result;
	int c;
	int node;
	int flag;
	int matched;
	int mode;
	SceSize depth;
	int kind;
	int zero;
	SceSize consumed;
	SceSize sourceLength;
	SceSize length;
	char decoded[8];
	const char *literal;

	p = src;
	pos = 0;
	state = lexer->state;
	result = 0;
	zero = 0;
	literal = NULL;
	if (size > 0) {
		do {
			switch (state) {
			case LEXER_STATE_START:
				c = *p;
				if (c == '{') {
					lexer->arrayIndex[lexer->depth] = -1;
					state = LEXER_STATE_OBJECT_BEGIN;
				} else if (c == '[') {
					state = LEXER_STATE_ARRAY_BEGIN;
					lexer->arrayIndex[lexer->depth] = zero;
				} else if (c == '"') {
					state = LEXER_STATE_KEY_BEGIN;
				} else {
					++p;
					pos = p - src;
				}
				break;
			case LEXER_STATE_OBJECT_BEGIN:
				if (*p == '{') {
					depth = lexer->depth;
					if (depth >= 16)
						return LEXER_ERROR_DEPTH;
					kind = NP_COMMON_PS4_LEXER_NODE_ELEMENT;
					if (lexer->arrayIndex[depth] == -1)
						kind = NP_COMMON_PS4_LEXER_NODE_KEY;
					matched = npCommonPs4LexerMatch(lexer->config, depth, lexer->mode, kind, lexer->name, &node, &flag);
					lexer->matched = matched;
					depth = lexer->depth;
					if (matched == 1) {
						lexer->nodeIndex[depth] = node;
						lexer->matchedFlag = flag;
						result = lexer->callback(lexer, NP_COMMON_PS4_LEXER_EVENT_OBJECT_BEGIN, depth, node, NULL, NULL,
							lexer->arrayIndex[depth], lexer->context);
						if (result < 0)
							return result;
						depth = lexer->depth + 1;
						mode = lexer->mode;
						if (lexer->matchedFlag == 1) {
							lexer->depth = depth;
							state = LEXER_STATE_KEY_BEGIN;
							lexer->arrayIndex[depth] = -1;
						} else {
							lexer->depth = depth;
							state = LEXER_STATE_SKIP;
							lexer->arrayIndex[depth] = -1;
							lexer->baseDepth = depth;
						}
					} else {
						mode = lexer->mode;
						if (depth != 0) {
							lexer->depth = depth + 1;
							lexer->baseDepth = depth + 1;
							lexer->arrayIndex[depth + 1] = -1;
							state = LEXER_STATE_SKIP;
						} else {
							state = LEXER_STATE_START;
						}
					}
					if (mode == LEXER_MODE_ANY)
						lexer->mode = zero;
				}
				++p;
				pos = p - src;
				break;
			case LEXER_STATE_OBJECT_END:
				if (*p == '}') {
					if (lexer->matched == 1) {
						depth = lexer->depth;
						if (depth != 0) {
							lexer->depth = depth - 1;
							LEXER_END(lexer, NP_COMMON_PS4_LEXER_EVENT_OBJECT_END, result);
							if (result < 0)
								return result;
						}
					}
					state = LEXER_STATE_START;
					if (lexer->depth != 0)
						state = LEXER_STATE_NEXT;
				}
				++p;
				pos = p - src;
				break;
			case LEXER_STATE_ARRAY_BEGIN:
				if (*p == '[') {
					depth = lexer->depth;
					if (depth >= 16)
						return LEXER_ERROR_DEPTH;
					kind = NP_COMMON_PS4_LEXER_NODE_ELEMENT;
					if (lexer->arrayIndex[depth] == -1)
						kind = NP_COMMON_PS4_LEXER_NODE_KEY;
					matched = npCommonPs4LexerMatch(lexer->config, depth, lexer->mode, kind, lexer->name, &node, &flag);
					lexer->matched = matched;
					depth = lexer->depth;
					if (matched == 1) {
						lexer->nodeIndex[depth] = node;
						lexer->matchedFlag = flag;
						result = lexer->callback(lexer, NP_COMMON_PS4_LEXER_EVENT_ARRAY_BEGIN, depth, node, NULL, NULL,
							lexer->arrayIndex[depth], lexer->context);
						if (result < 0)
							return result;
						depth = lexer->depth;
						mode = lexer->mode;
						matched = lexer->matched;
						if (depth > 1) {
							matched = npCommonPs4LexerMatch(lexer->config, depth + 1, mode, NP_COMMON_PS4_LEXER_NODE_ELEMENT, NULL, &node, &flag);
							depth = lexer->depth;
							mode = lexer->mode;
							lexer->matched = matched;
						}
						if (matched == 1) {
							lexer->depth = depth + 1;
							state = LEXER_STATE_VALUE;
							lexer->nodeIndex[depth + 1] = node;
							lexer->arrayIndex[depth + 1] = zero;
							lexer->matchedFlag = flag;
						} else if (depth != 0) {
							lexer->depth = depth + 1;
							lexer->arrayIndex[depth + 1] = zero;
							state = LEXER_STATE_SKIP;
							lexer->baseDepth = depth + 1;
						} else {
							state = LEXER_STATE_START;
						}
					} else {
						mode = lexer->mode;
						if (depth != 0) {
							lexer->depth = depth + 1;
							lexer->arrayIndex[depth + 1] = zero;
							state = LEXER_STATE_SKIP;
							lexer->baseDepth = depth + 1;
						} else {
							state = LEXER_STATE_START;
						}
					}
					if (mode == LEXER_MODE_ANY)
						lexer->mode = zero;
				}
				++p;
				pos = p - src;
				break;
			case LEXER_STATE_ARRAY_END:
				if (*p == ']') {
					if (lexer->matched == 1) {
						depth = lexer->depth;
						if (depth != 0) {
							lexer->depth = depth - 1;
							LEXER_END(lexer, NP_COMMON_PS4_LEXER_EVENT_ARRAY_END, result);
							if (result < 0)
								return result;
						}
					}
					state = LEXER_STATE_START;
					if (lexer->depth != 0)
						state = LEXER_STATE_NEXT;
				}
				++p;
				pos = p - src;
				break;
			case LEXER_STATE_KEY_BEGIN:
				c = *p;
				if (c == '"') {
					state = LEXER_STATE_KEY;
					sceClibMemset(lexer->name, 0, 0x80);
					lexer->length = zero;
					++p;
					pos = p - src;
				} else if (c == '}' && lexer->arrayIndex[lexer->depth] == -1) {
					state = LEXER_STATE_OBJECT_END;
				} else if (c != ']' || lexer->arrayIndex[lexer->depth] == -1) {
					++p;
					pos = p - src;
				} else {
					state = LEXER_STATE_ARRAY_END;
				}
				break;
			case LEXER_STATE_KEY:
				if (npCommonPs4LexerCopyChar(p, size - pos, &consumed, lexer->partial, &lexer->partialLength,
					&lexer->remaining, decoded, &sourceLength, &length) == 1) {
					if (lexer->length + length >= 0x80 || (length == 1 && sourceLength == 1 && decoded[0] == '"')) {
						state = LEXER_STATE_COLON;
						lexer->name[lexer->length] = zero;
					} else {
						sceClibMemcpy_safe(&lexer->name[lexer->length], decoded, length);
						p += consumed;
						lexer->length += length;
						pos = p - src;
					}
				} else {
					p += consumed;
					pos = p - src;
				}
				break;
			case LEXER_STATE_COLON:
				if (*p == ':') {
					matched = npCommonPs4LexerMatch(lexer->config, lexer->depth, lexer->mode, NP_COMMON_PS4_LEXER_NODE_KEY,
						lexer->name, &node, &flag);
					lexer->matched = matched;
					if (matched == 1) {
						depth = lexer->depth;
						lexer->nodeIndex[depth] = node;
						lexer->matchedFlag = flag;
						result = lexer->callback(lexer, NP_COMMON_PS4_LEXER_EVENT_KEY, depth, node, lexer->name, NULL,
							lexer->arrayIndex[depth], lexer->context);
						if (result < 0)
							return result;
						state = LEXER_STATE_VALUE;
						sceClibMemset(&lexer->token, 0, 0x604);
					} else {
						state = LEXER_STATE_NEXT;
					}
				}
				++p;
				pos = p - src;
				break;
			case LEXER_STATE_VALUE:
				c = *p;
				if (c == '{') {
					state = LEXER_STATE_OBJECT_BEGIN;
				} else if (c == '[') {
					state = LEXER_STATE_ARRAY_BEGIN;
				} else if (c == 't' || c == 'f' || c == 'n') {
					state = LEXER_STATE_LITERAL;
					if (c == 't') {
						literal = "true";
						kind = NP_COMMON_PS4_LEXER_TOKEN_TRUE;
					} else {
						kind = zero;
						literal = "null";
						if (c == 'f') {
							literal = "false";
							kind = NP_COMMON_PS4_LEXER_TOKEN_FALSE;
						}
					}
					++p;
					lexer->token.type = kind;
					pos = p - src;
					lexer->length = 1;
				} else if ((sceClibLookCtypeTable(c) & 4) != 0 || (c = *p) == '-') {
					lexer->token.type = NP_COMMON_PS4_LEXER_TOKEN_NUMBER;
					lexer->token.text[0] = *p++;
					pos = p - src;
					lexer->length = 1;
					state = LEXER_STATE_NUMBER;
				} else if (c == '"') {
					++p;
					pos = p - src;
					lexer->token.type = NP_COMMON_PS4_LEXER_TOKEN_STRING;
					state = LEXER_STATE_STRING;
					lexer->length = zero;
				} else if (c == '}' && lexer->arrayIndex[lexer->depth] == -1) {
					state = LEXER_STATE_OBJECT_END;
				} else if (c != ']' || lexer->arrayIndex[lexer->depth] == -1) {
					++p;
					pos = p - src;
				} else {
					state = LEXER_STATE_ARRAY_END;
				}
				break;
			case LEXER_STATE_NEXT:
				c = *p;
				if (c == ',') {
					if (lexer->arrayIndex[lexer->depth] == -1) {
						state = LEXER_STATE_KEY_BEGIN;
					} else {
						state = LEXER_STATE_VALUE;
						sceClibMemset(&lexer->token, 0, 0x604);
						if (lexer->matched == 1)
							++lexer->arrayIndex[lexer->depth];
					}
					++p;
					pos = p - src;
				} else if (c == '}' && lexer->arrayIndex[lexer->depth] == -1) {
					state = LEXER_STATE_OBJECT_END;
					lexer->matched = 1;
				} else if (c != ']' || lexer->arrayIndex[lexer->depth] == -1) {
					if (c == '{') {
						depth = lexer->depth + 1;
						lexer->depth = depth;
						lexer->arrayIndex[depth] = -1;
						++p;
						lexer->baseDepth = depth;
						pos = p - src;
						state = LEXER_STATE_SKIP;
					} else if (c == '[') {
						depth = lexer->depth + 1;
						lexer->depth = depth;
						++p;
						pos = p - src;
						lexer->arrayIndex[depth] = zero;
						state = LEXER_STATE_SKIP;
						lexer->baseDepth = depth;
					} else {
						++p;
						pos = p - src;
					}
				} else {
					state = LEXER_STATE_ARRAY_END;
					lexer->matched = 1;
				}
				break;
			case LEXER_STATE_LITERAL:
				if (literal != NULL && lexer->length < sceClibStrnlen(literal, 0xFFFFFFFF)
					&& *p == literal[lexer->length]) {
					lexer->length++;
					if (lexer->length >= sceClibStrnlen(literal, 0xFFFFFFFF)) {
						state = LEXER_STATE_NEXT;
						LEXER_VALUE_READ(lexer, result);
						if (result < 0)
							return result;
					}
					++p;
					pos = p - src;
				} else {
					state = LEXER_STATE_NEXT;
				}
				break;
			case LEXER_STATE_NUMBER:
				c = *p;
				length = lexer->length;
				if ((sceClibLookCtypeTable(c) & 4) != 0 || c == '.' || c == '+' || c == '-' || c == 'e' || c == 'E') {
					if (length < 0x5FF) {
						lexer->token.text[length] = *p;
						lexer->length = length + 1;
					}
					++p;
					pos = p - src;
				} else {
					state = LEXER_STATE_NEXT;
					lexer->token.text[length] = zero;
					LEXER_VALUE_READ(lexer, result);
					if (result < 0)
						return result;
				}
				break;
			case LEXER_STATE_STRING:
				if (npCommonPs4LexerCopyChar(p, size - pos, &consumed, lexer->partial, &lexer->partialLength,
					&lexer->remaining, decoded, &sourceLength, &length) == 1) {
					if (lexer->length + length >= 0x600 || (length == 1 && sourceLength == 1 && decoded[0] == '"')) {
						state = LEXER_STATE_NEXT;
						lexer->token.text[lexer->length] = zero;
						LEXER_VALUE_READ(lexer, result);
						if (result < 0)
							return result;
						++p;
						pos = p - src;
					} else {
						sceClibMemcpy_safe(&lexer->token.text[lexer->length], decoded, length);
						p += consumed;
						lexer->length += length;
						pos = p - src;
					}
				} else {
					p += consumed;
					pos = p - src;
				}
				break;
			case LEXER_STATE_SKIP:
				c = *p;
				if (c == '{') {
					depth = lexer->depth;
					if (depth >= 16)
						return LEXER_ERROR_DEPTH;
					lexer->depth = depth + 1;
					lexer->arrayIndex[depth + 1] = -1;
					++p;
					pos = p - src;
				} else if (c == '[') {
					depth = lexer->depth;
					if (depth > 16)
						return LEXER_ERROR_DEPTH;
					++p;
					lexer->depth = depth + 1;
					pos = p - src;
					lexer->arrayIndex[depth + 1] = zero;
				} else if (c == '}' && lexer->arrayIndex[lexer->depth] == -1) {
					depth = lexer->depth;
					if (depth != 0) {
						lexer->depth = depth - 1;
						if (depth - 1 >= lexer->baseDepth) {
							++p;
							pos = p - src;
						} else {
							state = LEXER_STATE_OBJECT_END;
							lexer->baseDepth = zero;
						}
					} else {
						++p;
						pos = p - src;
					}
				} else if (c != ']' || lexer->arrayIndex[lexer->depth] == -1) {
					++p;
					pos = p - src;
				} else {
					depth = lexer->depth;
					if (depth != 0) {
						lexer->depth = depth - 1;
						if (depth - 1 >= lexer->baseDepth) {
							++p;
							pos = p - src;
						} else {
							state = LEXER_STATE_ARRAY_END;
							lexer->baseDepth = zero;
						}
					} else {
						++p;
						pos = p - src;
					}
				}
				break;
			default:
				++p;
				pos = p - src;
				break;
			}
		} while (size > pos);
	}
	lexer->state = state;
	return 0;
}

/* the second lexer's match: a key node without a name matches any key */
int npCommonPs4Lexer2Match(const struct NpCommonPs4LexerConfig *config, SceSize depth, int mode, int kind, const char *name,
	int *pIndex, int *pFlag)
{
	const struct NpCommonPs4LexerNode *node;
	int index;
	int result;

	result = 0;
	if (pIndex != NULL)
		*pIndex = -1;
	if (pFlag != NULL)
		*pFlag = 0;
	if (depth < config->numLevels) {
		node = config->levels[depth];
		index = 0;
		while (node->kind == NP_COMMON_PS4_LEXER_NODE_KEY || node->kind == NP_COMMON_PS4_LEXER_NODE_ELEMENT) {
			if (mode == LEXER_MODE_ANY) {
				if (node->name == NULL) {
					result = 1;
					if (pIndex != NULL)
						*pIndex = index;
					if (pFlag != NULL)
						*pFlag = node->flag;
					return result;
				}
			} else if (kind == NP_COMMON_PS4_LEXER_NODE_KEY && node->kind == NP_COMMON_PS4_LEXER_NODE_KEY) {
				if (node->name == NULL || (name != NULL && sceClibStrncasecmp(node->name, name, 0x80) == 0)) {
					result = 1;
					if (pIndex != NULL)
						*pIndex = index;
					if (pFlag != NULL)
						*pFlag = node->flag;
					return result;
				}
			} else if (kind == NP_COMMON_PS4_LEXER_NODE_ELEMENT && node->kind == NP_COMMON_PS4_LEXER_NODE_ELEMENT) {
				result = 1;
				if (pIndex != NULL)
					*pIndex = index;
				if (pFlag != NULL)
					*pFlag = node->flag;
				return result;
			}
			node++;
			index++;
		}
	}
	return result;
}

/* the second lexer's npCommonPs4LexerCopyChar */
int npCommonPs4Lexer2CopyChar(const char *src, SceSize length, SceSize *pConsumed, char *partial, SceSize *pPartialLength,
	SceSize *pRemaining, char *decoded, SceSize *pSourceLength, SceSize *pLength)
{
	int result;
	int ret;
	SceSize size;

	result = 1;
	if (*pRemaining != 0) {
		if (*pRemaining <= length) {
			*pConsumed = *pRemaining;
			size = *pRemaining;
		} else {
			*pConsumed = length;
			size = length;
		}
		sceClibMemcpy_safe(&partial[*pPartialLength], src, size);
		*pPartialLength += *pConsumed;
		*pRemaining -= *pConsumed;
		if (*pRemaining != 0)
			return 0;
		ret = SceNpCommonPs4_12AA95E3(decoded, 4, partial, *pPartialLength, pLength, (int *)pSourceLength);
		if (ret == NP_COMMON_PS4_ERROR_JSON_INVALID_STRING) {
			result = 0;
			*pRemaining = *pSourceLength - *pPartialLength;
		} else if (ret >= 0) {
			*pRemaining = 0;
			*pPartialLength = 0;
		} else {
			*pSourceLength = 1;
			*pLength = 1;
			sceClibMemcpy_safe(decoded, partial, 1);
		}
	} else {
		size = length;
		if (length >= 6)
			size = 6;
		*pConsumed = size;
		ret = SceNpCommonPs4_12AA95E3(decoded, 4, src, size, pLength, (int *)pSourceLength);
		if (ret == NP_COMMON_PS4_ERROR_JSON_INVALID_STRING) {
			sceClibMemcpy_safe(partial, src, *pConsumed);
			result = 0;
			*pPartialLength = *pConsumed;
			*pRemaining = *pSourceLength - *pConsumed;
		} else if (ret >= 0) {
			*pConsumed = *pSourceLength;
		} else {
			*pSourceLength = 1;
			*pLength = 1;
			sceClibMemcpy_safe(decoded, src, 1);
			*pConsumed = 1;
		}
	}
	return result;
}


int SceNpCommonPs4_EB1B7375(struct NpCommonPs4Lexer *lexer, const struct NpCommonPs4LexerConfig *config,
	NpCommonPs4LexerCallback callback, void *context)
{
	if (lexer == NULL)
		return NP_COMMON_PS4_ERROR_JSON_INVALID_ARGUMENT;
	sceClibMemset(lexer, 0, 0x740);
	lexer->config = config;
	lexer->callback = callback;
	lexer->context = context;
	lexer->state = LEXER_STATE_START;
	lexer->depth = 0;
	lexer->mode = LEXER_MODE_ANY;
	return 0;
}


int SceNpCommonPs4_F5EE219F(struct NpCommonPs4Lexer *lexer, const char *src, int size)
{
	const char *p;
	int pos;
	int state;
	int result;
	int c;
	int node;
	int flag;
	int matched;
	int mode;
	SceSize depth;
	int kind;
	int zero;
	SceSize consumed;
	SceSize sourceLength;
	SceSize length;
	char decoded[8];
	const char *literal;

	p = src;
	pos = 0;
	state = lexer->state;
	result = 0;
	zero = 0;
	literal = NULL;
	if (size > 0) {
		do {
			switch (state) {
			case LEXER_STATE_START:
				c = *p;
				if (c == '{') {
					lexer->arrayIndex[lexer->depth] = -1;
					state = LEXER_STATE_OBJECT_BEGIN;
				} else if (c == '[') {
					state = LEXER_STATE_ARRAY_BEGIN;
					lexer->arrayIndex[lexer->depth] = zero;
				} else if (c == '"') {
					state = LEXER_STATE_KEY_BEGIN;
				} else {
					++p;
					pos = p - src;
				}
				break;
			case LEXER_STATE_OBJECT_BEGIN:
				if (*p == '{') {
					depth = lexer->depth;
					if (depth >= 16)
						return LEXER_ERROR_DEPTH;
					kind = NP_COMMON_PS4_LEXER_NODE_ELEMENT;
					if (lexer->arrayIndex[depth] == -1)
						kind = NP_COMMON_PS4_LEXER_NODE_KEY;
					matched = npCommonPs4Lexer2Match(lexer->config, depth, lexer->mode, kind, lexer->name, &node, &flag);
					lexer->matched = matched;
					depth = lexer->depth;
					if (matched == 1) {
						lexer->nodeIndex[depth] = node;
						lexer->matchedFlag = flag;
						result = lexer->callback(lexer, NP_COMMON_PS4_LEXER_EVENT_OBJECT_BEGIN, depth, node, NULL, NULL,
							lexer->arrayIndex[depth], lexer->context);
						if (result < 0)
							return result;
						depth = lexer->depth + 1;
						mode = lexer->mode;
						if (lexer->matchedFlag == 1) {
							lexer->depth = depth;
							state = LEXER_STATE_KEY_BEGIN;
							lexer->arrayIndex[depth] = -1;
						} else {
							lexer->depth = depth;
							state = LEXER_STATE_SKIP;
							lexer->arrayIndex[depth] = -1;
							lexer->baseDepth = depth;
						}
					} else {
						mode = lexer->mode;
						if (depth != 0) {
							lexer->depth = depth + 1;
							lexer->baseDepth = depth + 1;
							lexer->arrayIndex[depth + 1] = -1;
							state = LEXER_STATE_SKIP;
						} else {
							state = LEXER_STATE_START;
						}
					}
					if (mode == LEXER_MODE_ANY)
						lexer->mode = zero;
				}
				++p;
				pos = p - src;
				break;
			case LEXER_STATE_OBJECT_END:
				if (*p == '}') {
					if (lexer->matched == 1) {
						depth = lexer->depth;
						if (depth != 0) {
							lexer->depth = depth - 1;
							LEXER_END(lexer, NP_COMMON_PS4_LEXER_EVENT_OBJECT_END, result);
							if (result < 0)
								return result;
						}
					}
					state = LEXER_STATE_START;
					if (lexer->depth != 0)
						state = LEXER_STATE_NEXT;
				}
				++p;
				pos = p - src;
				break;
			case LEXER_STATE_ARRAY_BEGIN:
				if (*p == '[') {
					depth = lexer->depth;
					if (depth >= 16)
						return LEXER_ERROR_DEPTH;
					kind = NP_COMMON_PS4_LEXER_NODE_ELEMENT;
					if (lexer->arrayIndex[depth] == -1)
						kind = NP_COMMON_PS4_LEXER_NODE_KEY;
					matched = npCommonPs4Lexer2Match(lexer->config, depth, lexer->mode, kind, lexer->name, &node, &flag);
					lexer->matched = matched;
					depth = lexer->depth;
					if (matched == 1) {
						lexer->nodeIndex[depth] = node;
						lexer->matchedFlag = flag;
						result = lexer->callback(lexer, NP_COMMON_PS4_LEXER_EVENT_ARRAY_BEGIN, depth, node, NULL, NULL,
							lexer->arrayIndex[depth], lexer->context);
						if (result < 0)
							return result;
						depth = lexer->depth;
						mode = lexer->mode;
						matched = lexer->matched;
						if (depth > 1) {
							matched = npCommonPs4Lexer2Match(lexer->config, depth + 1, mode, NP_COMMON_PS4_LEXER_NODE_ELEMENT, NULL, &node, &flag);
							depth = lexer->depth;
							mode = lexer->mode;
							lexer->matched = matched;
						}
						if (matched == 1) {
							lexer->depth = depth + 1;
							state = LEXER_STATE_VALUE;
							lexer->nodeIndex[depth + 1] = node;
							lexer->arrayIndex[depth + 1] = zero;
							lexer->matchedFlag = flag;
						} else if (depth != 0) {
							lexer->depth = depth + 1;
							lexer->arrayIndex[depth + 1] = zero;
							state = LEXER_STATE_SKIP;
							lexer->baseDepth = depth + 1;
						} else {
							state = LEXER_STATE_START;
						}
					} else {
						mode = lexer->mode;
						if (depth != 0) {
							lexer->depth = depth + 1;
							lexer->arrayIndex[depth + 1] = zero;
							state = LEXER_STATE_SKIP;
							lexer->baseDepth = depth + 1;
						} else {
							state = LEXER_STATE_START;
						}
					}
					if (mode == LEXER_MODE_ANY)
						lexer->mode = zero;
				}
				++p;
				pos = p - src;
				break;
			case LEXER_STATE_ARRAY_END:
				if (*p == ']') {
					if (lexer->matched == 1) {
						depth = lexer->depth;
						if (depth != 0) {
							lexer->depth = depth - 1;
							LEXER_END(lexer, NP_COMMON_PS4_LEXER_EVENT_ARRAY_END, result);
							if (result < 0)
								return result;
						}
					}
					state = LEXER_STATE_START;
					if (lexer->depth != 0)
						state = LEXER_STATE_NEXT;
				}
				++p;
				pos = p - src;
				break;
			case LEXER_STATE_KEY_BEGIN:
				c = *p;
				if (c == '"') {
					state = LEXER_STATE_KEY;
					sceClibMemset(lexer->name, 0, 0x80);
					lexer->length = zero;
					++p;
					pos = p - src;
				} else if (c == '}' && lexer->arrayIndex[lexer->depth] == -1) {
					state = LEXER_STATE_OBJECT_END;
				} else if (c != ']' || lexer->arrayIndex[lexer->depth] == -1) {
					++p;
					pos = p - src;
				} else {
					state = LEXER_STATE_ARRAY_END;
				}
				break;
			case LEXER_STATE_KEY:
				if (npCommonPs4Lexer2CopyChar(p, size - pos, &consumed, lexer->partial, &lexer->partialLength,
					&lexer->remaining, decoded, &sourceLength, &length) == 1) {
					if (lexer->length + length >= 0x80 || (length == 1 && sourceLength == 1 && decoded[0] == '"')) {
						state = LEXER_STATE_COLON;
						lexer->name[lexer->length] = zero;
					} else {
						sceClibMemcpy_safe(&lexer->name[lexer->length], decoded, length);
						p += consumed;
						lexer->length += length;
						pos = p - src;
					}
				} else {
					p += consumed;
					pos = p - src;
				}
				break;
			case LEXER_STATE_COLON:
				if (*p == ':') {
					matched = npCommonPs4Lexer2Match(lexer->config, lexer->depth, lexer->mode, NP_COMMON_PS4_LEXER_NODE_KEY,
						lexer->name, &node, &flag);
					lexer->matched = matched;
					if (matched == 1) {
						depth = lexer->depth;
						lexer->nodeIndex[depth] = node;
						lexer->matchedFlag = flag;
						result = lexer->callback(lexer, NP_COMMON_PS4_LEXER_EVENT_KEY, depth, node, lexer->name, NULL,
							lexer->arrayIndex[depth], lexer->context);
						if (result < 0)
							return result;
						state = LEXER_STATE_VALUE;
						sceClibMemset(&lexer->token, 0, 0x604);
					} else {
						state = LEXER_STATE_NEXT;
					}
				}
				++p;
				pos = p - src;
				break;
			case LEXER_STATE_VALUE:
				c = *p;
				if (c == '{') {
					state = LEXER_STATE_OBJECT_BEGIN;
				} else if (c == '[') {
					state = LEXER_STATE_ARRAY_BEGIN;
				} else if (c == 't' || c == 'f' || c == 'n') {
					state = LEXER_STATE_LITERAL;
					if (c == 't') {
						literal = "true";
						kind = NP_COMMON_PS4_LEXER_TOKEN_TRUE;
					} else {
						kind = zero;
						literal = "null";
						if (c == 'f') {
							literal = "false";
							kind = NP_COMMON_PS4_LEXER_TOKEN_FALSE;
						}
					}
					++p;
					lexer->token.type = kind;
					pos = p - src;
					lexer->length = 1;
				} else if ((sceClibLookCtypeTable(c) & 4) != 0 || (c = *p) == '-') {
					lexer->token.type = NP_COMMON_PS4_LEXER_TOKEN_NUMBER;
					lexer->token.text[0] = *p++;
					pos = p - src;
					lexer->length = 1;
					state = LEXER_STATE_NUMBER;
				} else if (c == '"') {
					++p;
					pos = p - src;
					lexer->token.type = NP_COMMON_PS4_LEXER_TOKEN_STRING;
					state = LEXER_STATE_STRING;
					lexer->length = zero;
				} else if (c == '}' && lexer->arrayIndex[lexer->depth] == -1) {
					state = LEXER_STATE_OBJECT_END;
				} else if (c != ']' || lexer->arrayIndex[lexer->depth] == -1) {
					++p;
					pos = p - src;
				} else {
					state = LEXER_STATE_ARRAY_END;
				}
				break;
			case LEXER_STATE_NEXT:
				c = *p;
				if (c == ',') {
					if (lexer->arrayIndex[lexer->depth] == -1) {
						state = LEXER_STATE_KEY_BEGIN;
					} else {
						state = LEXER_STATE_VALUE;
						sceClibMemset(&lexer->token, 0, 0x604);
						if (lexer->matched == 1)
							++lexer->arrayIndex[lexer->depth];
					}
					++p;
					pos = p - src;
				} else if (c == '}' && lexer->arrayIndex[lexer->depth] == -1) {
					state = LEXER_STATE_OBJECT_END;
					lexer->matched = 1;
				} else if (c != ']' || lexer->arrayIndex[lexer->depth] == -1) {
					if (c == '{') {
						depth = lexer->depth + 1;
						lexer->depth = depth;
						lexer->arrayIndex[depth] = -1;
						++p;
						lexer->baseDepth = depth;
						pos = p - src;
						state = LEXER_STATE_SKIP;
					} else if (c == '[') {
						depth = lexer->depth + 1;
						lexer->depth = depth;
						++p;
						pos = p - src;
						lexer->arrayIndex[depth] = zero;
						state = LEXER_STATE_SKIP;
						lexer->baseDepth = depth;
					} else {
						++p;
						pos = p - src;
					}
				} else {
					state = LEXER_STATE_ARRAY_END;
					lexer->matched = 1;
				}
				break;
			case LEXER_STATE_LITERAL:
				if (literal != NULL && lexer->length < sceClibStrnlen(literal, 0x80)
					&& *p == literal[lexer->length]) {
					lexer->length++;
					if (lexer->length >= sceClibStrnlen(literal, 0x80)) {
						state = LEXER_STATE_NEXT;
						LEXER_VALUE_READ(lexer, result);
						if (result < 0)
							return result;
					}
					++p;
					pos = p - src;
				} else {
					state = LEXER_STATE_NEXT;
				}
				break;
			case LEXER_STATE_NUMBER:
				c = *p;
				length = lexer->length;
				if ((sceClibLookCtypeTable(c) & 4) != 0 || c == '.' || c == '+' || c == '-' || c == 'e' || c == 'E') {
					if (length < 0x5FF) {
						lexer->token.text[length] = *p;
						lexer->length = length + 1;
					}
					++p;
					pos = p - src;
				} else {
					state = LEXER_STATE_NEXT;
					lexer->token.text[length] = zero;
					LEXER_VALUE_READ(lexer, result);
					if (result < 0)
						return result;
				}
				break;
			case LEXER_STATE_STRING:
				if (npCommonPs4Lexer2CopyChar(p, size - pos, &consumed, lexer->partial, &lexer->partialLength,
					&lexer->remaining, decoded, &sourceLength, &length) == 1) {
					if (lexer->length + length >= 0x600 || (length == 1 && sourceLength == 1 && decoded[0] == '"')) {
						state = LEXER_STATE_NEXT;
						lexer->token.text[lexer->length] = zero;
						LEXER_VALUE_READ(lexer, result);
						if (result < 0)
							return result;
						++p;
						pos = p - src;
					} else {
						sceClibMemcpy_safe(&lexer->token.text[lexer->length], decoded, length);
						p += consumed;
						lexer->length += length;
						pos = p - src;
					}
				} else {
					p += consumed;
					pos = p - src;
				}
				break;
			case LEXER_STATE_SKIP:
				c = *p;
				if (c == '{') {
					depth = lexer->depth;
					if (depth >= 16)
						return LEXER_ERROR_DEPTH;
					lexer->depth = depth + 1;
					lexer->arrayIndex[depth + 1] = -1;
					++p;
					pos = p - src;
				} else if (c == '[') {
					depth = lexer->depth;
					if (depth >= 16)
						return LEXER_ERROR_DEPTH;
					++p;
					lexer->depth = depth + 1;
					pos = p - src;
					lexer->arrayIndex[depth + 1] = zero;
				} else if (c == '}' && lexer->arrayIndex[lexer->depth] == -1) {
					depth = lexer->depth;
					if (depth != 0) {
						lexer->depth = depth - 1;
						if (depth - 1 >= lexer->baseDepth) {
							++p;
							pos = p - src;
						} else {
							state = LEXER_STATE_OBJECT_END;
							lexer->baseDepth = zero;
						}
					} else {
						++p;
						pos = p - src;
					}
				} else if (c != ']' || lexer->arrayIndex[lexer->depth] == -1) {
					++p;
					pos = p - src;
				} else {
					depth = lexer->depth;
					if (depth != 0) {
						lexer->depth = depth - 1;
						if (depth - 1 >= lexer->baseDepth) {
							++p;
							pos = p - src;
						} else {
							state = LEXER_STATE_ARRAY_END;
							lexer->baseDepth = zero;
						}
					} else {
						++p;
						pos = p - src;
					}
				}
				break;
			default:
				++p;
				pos = p - src;
				break;
			}
		} while (size > pos);
	}
	lexer->state = state;
	return 0;
}

/* the third lexer's match: the length compared of a key is the caller's */
int npCommonPs4Lexer3Match(const struct NpCommonPs4LexerConfig *config, SceSize depth, int mode, int kind, const char *name,
	SceSize nameLength, int *pIndex, int *pFlag)
{
	const struct NpCommonPs4LexerNode *node;
	int index;
	int result;

	result = 0;
	if (pIndex != NULL)
		*pIndex = -1;
	if (pFlag != NULL)
		*pFlag = 0;
	if (depth < config->numLevels) {
		node = config->levels[depth];
		index = 0;
		while (node->kind == NP_COMMON_PS4_LEXER_NODE_KEY || node->kind == NP_COMMON_PS4_LEXER_NODE_ELEMENT) {
			if (mode == LEXER_MODE_ANY) {
				if (node->name == NULL) {
					result = 1;
					if (pIndex != NULL)
						*pIndex = index;
					if (pFlag != NULL)
						*pFlag = node->flag;
					return result;
				}
			} else if (kind == NP_COMMON_PS4_LEXER_NODE_KEY && node->kind == NP_COMMON_PS4_LEXER_NODE_KEY) {
				if (name != NULL && node->name != NULL && sceClibStrncasecmp(node->name, name, nameLength) == 0) {
					result = 1;
					if (pIndex != NULL)
						*pIndex = index;
					if (pFlag != NULL)
						*pFlag = node->flag;
					return result;
				}
			} else if (kind == NP_COMMON_PS4_LEXER_NODE_ELEMENT && node->kind == NP_COMMON_PS4_LEXER_NODE_ELEMENT) {
				result = 1;
				if (pIndex != NULL)
					*pIndex = index;
				if (pFlag != NULL)
					*pFlag = node->flag;
				return result;
			}
			node++;
			index++;
		}
	}
	return result;
}

/* the third lexer's npCommonPs4LexerCopyChar */
int npCommonPs4Lexer3CopyChar(const char *src, SceSize length, SceSize *pConsumed, char *partial, SceSize *pPartialLength,
	SceSize *pRemaining, char *decoded, SceSize *pSourceLength, SceSize *pLength)
{
	int result;
	int ret;
	SceSize size;

	result = 1;
	if (*pRemaining != 0) {
		if (*pRemaining <= length) {
			*pConsumed = *pRemaining;
			size = *pRemaining;
		} else {
			*pConsumed = length;
			size = length;
		}
		sceClibMemcpy_safe(&partial[*pPartialLength], src, size);
		*pPartialLength += *pConsumed;
		*pRemaining -= *pConsumed;
		if (*pRemaining != 0)
			return 0;
		ret = SceNpCommonPs4_12AA95E3(decoded, 4, partial, *pPartialLength, pLength, (int *)pSourceLength);
		if (ret == NP_COMMON_PS4_ERROR_JSON_INVALID_STRING) {
			result = 0;
			*pRemaining = *pSourceLength - *pPartialLength;
		} else if (ret >= 0) {
			*pRemaining = 0;
			*pPartialLength = 0;
		} else {
			*pSourceLength = 1;
			*pLength = 1;
			sceClibMemcpy_safe(decoded, partial, 1);
		}
	} else {
		size = length;
		if (length >= 6)
			size = 6;
		*pConsumed = size;
		ret = SceNpCommonPs4_12AA95E3(decoded, 4, src, size, pLength, (int *)pSourceLength);
		if (ret == NP_COMMON_PS4_ERROR_JSON_INVALID_STRING) {
			sceClibMemcpy_safe(partial, src, *pConsumed);
			result = 0;
			*pPartialLength = *pConsumed;
			*pRemaining = *pSourceLength - *pConsumed;
		} else if (ret >= 0) {
			*pConsumed = *pSourceLength;
		} else {
			*pSourceLength = 1;
			*pLength = 1;
			sceClibMemcpy_safe(decoded, src, 1);
			*pConsumed = 1;
		}
	}
	return result;
}

int SceNpCommonPs4_CAFD09C2(struct NpCommonPs4Lexer3 *lexer, const struct NpCommonPs4LexerConfig *config, char *name, SceSize nameSize,
	char *text, SceSize textSize, NpCommonPs4Lexer3Callback callback, void *context)
{
	if (lexer == NULL || name == NULL || text == NULL)
		return NP_COMMON_PS4_ERROR_JSON_INVALID_ARGUMENT;
	if (nameSize == 0 || textSize < 0x20)
		return NP_COMMON_PS4_ERROR_JSON_INVALID_STRING;
	sceClibMemset(lexer, 0, 0xD0);
	lexer->config = config;
	lexer->name = name;
	lexer->nameSize = nameSize;
	lexer->tokenText = text;
	lexer->tokenSize = textSize;
	lexer->callback = callback;
	lexer->context = context;
	lexer->state = LEXER_STATE_START;
	lexer->depth = 0;
	lexer->mode = LEXER_MODE_ANY;
	return 0;
}

/* a value read: the callback told of it, with a token of the type and the buffer */
#define LEXER3_VALUE_READ(lexer, result) \
	do { \
		token.type = (lexer)->tokenType; \
		token.text = (lexer)->tokenText; \
		(result) = (lexer)->callback((lexer), NP_COMMON_PS4_LEXER_EVENT_VALUE, (lexer)->depth, \
			(lexer)->nodeIndex[(lexer)->depth], \
			(lexer)->arrayIndex[(lexer)->depth] == -1 ? (lexer)->name : NULL, &token, \
			(lexer)->arrayIndex[(lexer)->depth], (lexer)->context); \
	} while (0)

#define LEXER3_END(lexer, event, result) \
	((result) = (lexer)->callback((lexer), (event), (lexer)->depth, (lexer)->nodeIndex[(lexer)->depth], NULL, NULL, \
		(lexer)->arrayIndex[(lexer)->depth], (lexer)->context))

int SceNpCommonPs4_D7AB56B8(struct NpCommonPs4Lexer3 *lexer, const char *src, int size)
{
	const char *p;
	int pos;
	int state;
	int result;
	int c;
	int node;
	int flag;
	int matched;
	int mode;
	SceSize depth;
	int kind;
	int zero;
	SceSize consumed;
	SceSize sourceLength;
	SceSize length;
	char decoded[8];
	struct NpCommonPs4Lexer3Token token;
	const char *literal;

	p = src;
	pos = 0;
	state = lexer->state;
	result = 0;
	zero = 0;
	literal = NULL;
	if (size > 0) {
		do {
			switch (state) {
			case LEXER_STATE_START:
				c = *p;
				if (c == '{') {
					lexer->arrayIndex[lexer->depth] = -1;
					state = LEXER_STATE_OBJECT_BEGIN;
				} else if (c == '[') {
					state = LEXER_STATE_ARRAY_BEGIN;
					lexer->arrayIndex[lexer->depth] = zero;
				} else if (c == '"') {
					state = LEXER_STATE_KEY_BEGIN;
				} else {
					++p;
					pos = p - src;
				}
				break;
			case LEXER_STATE_OBJECT_BEGIN:
				if (*p == '{') {
					depth = lexer->depth;
					if (depth >= 16)
						return LEXER_ERROR_DEPTH;
					kind = NP_COMMON_PS4_LEXER_NODE_ELEMENT;
					if (lexer->arrayIndex[depth] == -1)
						kind = NP_COMMON_PS4_LEXER_NODE_KEY;
					matched = npCommonPs4Lexer3Match(lexer->config, depth, lexer->mode, kind, lexer->name, lexer->nameSize, &node, &flag);
					lexer->matched = matched;
					depth = lexer->depth;
					if (matched == 1) {
						lexer->nodeIndex[depth] = node;
						lexer->matchedFlag = flag;
						result = lexer->callback(lexer, NP_COMMON_PS4_LEXER_EVENT_OBJECT_BEGIN, depth, node, NULL, NULL,
							lexer->arrayIndex[depth], lexer->context);
						if (result < 0)
							return result;
						depth = lexer->depth + 1;
						mode = lexer->mode;
						if (lexer->matchedFlag == 1) {
							lexer->depth = depth;
							state = LEXER_STATE_KEY_BEGIN;
							lexer->arrayIndex[depth] = -1;
						} else {
							lexer->depth = depth;
							state = LEXER_STATE_SKIP;
							lexer->arrayIndex[depth] = -1;
							lexer->baseDepth = depth;
						}
					} else {
						mode = lexer->mode;
						if (depth != 0) {
							lexer->depth = depth + 1;
							lexer->baseDepth = depth + 1;
							lexer->arrayIndex[depth + 1] = -1;
							state = LEXER_STATE_SKIP;
						} else {
							state = LEXER_STATE_START;
						}
					}
					if (mode == LEXER_MODE_ANY)
						lexer->mode = zero;
				}
				++p;
				pos = p - src;
				break;
			case LEXER_STATE_OBJECT_END:
				if (*p == '}') {
					if (lexer->matched == 1) {
						depth = lexer->depth;
						if (depth != 0) {
							lexer->depth = depth - 1;
							LEXER3_END(lexer, NP_COMMON_PS4_LEXER_EVENT_OBJECT_END, result);
							if (result < 0)
								return result;
						}
					}
					state = LEXER_STATE_START;
					if (lexer->depth != 0)
						state = LEXER_STATE_NEXT;
				}
				++p;
				pos = p - src;
				break;
			case LEXER_STATE_ARRAY_BEGIN:
				if (*p == '[') {
					depth = lexer->depth;
					if (depth >= 16)
						return LEXER_ERROR_DEPTH;
					kind = NP_COMMON_PS4_LEXER_NODE_ELEMENT;
					if (lexer->arrayIndex[depth] == -1)
						kind = NP_COMMON_PS4_LEXER_NODE_KEY;
					matched = npCommonPs4Lexer3Match(lexer->config, depth, lexer->mode, kind, lexer->name, lexer->nameSize, &node, &flag);
					lexer->matched = matched;
					depth = lexer->depth;
					if (matched == 1) {
						lexer->nodeIndex[depth] = node;
						lexer->matchedFlag = flag;
						result = lexer->callback(lexer, NP_COMMON_PS4_LEXER_EVENT_ARRAY_BEGIN, depth, node, NULL, NULL,
							lexer->arrayIndex[depth], lexer->context);
						if (result < 0)
							return result;
						depth = lexer->depth;
						mode = lexer->mode;
						matched = lexer->matched;
						if (depth > 1) {
							matched = npCommonPs4Lexer3Match(lexer->config, depth + 1, mode, NP_COMMON_PS4_LEXER_NODE_ELEMENT, NULL, 0, &node, &flag);
							depth = lexer->depth;
							mode = lexer->mode;
							lexer->matched = matched;
						}
						if (matched == 1) {
							lexer->depth = depth + 1;
							state = LEXER_STATE_VALUE;
							lexer->nodeIndex[depth + 1] = node;
							lexer->arrayIndex[depth + 1] = zero;
							lexer->matchedFlag = flag;
						} else if (depth != 0) {
							lexer->depth = depth + 1;
							lexer->arrayIndex[depth + 1] = zero;
							state = LEXER_STATE_SKIP;
							lexer->baseDepth = depth + 1;
						} else {
							state = LEXER_STATE_START;
						}
					} else {
						mode = lexer->mode;
						if (depth != 0) {
							lexer->depth = depth + 1;
							lexer->arrayIndex[depth + 1] = zero;
							state = LEXER_STATE_SKIP;
							lexer->baseDepth = depth + 1;
						} else {
							state = LEXER_STATE_START;
						}
					}
					if (mode == LEXER_MODE_ANY)
						lexer->mode = zero;
				}
				++p;
				pos = p - src;
				break;
			case LEXER_STATE_ARRAY_END:
				if (*p == ']') {
					if (lexer->matched == 1) {
						depth = lexer->depth;
						if (depth != 0) {
							lexer->depth = depth - 1;
							LEXER3_END(lexer, NP_COMMON_PS4_LEXER_EVENT_ARRAY_END, result);
							if (result < 0)
								return result;
						}
					}
					state = LEXER_STATE_START;
					if (lexer->depth != 0)
						state = LEXER_STATE_NEXT;
				}
				++p;
				pos = p - src;
				break;
			case LEXER_STATE_KEY_BEGIN:
				c = *p;
				if (c == '"') {
					state = LEXER_STATE_KEY;
					sceClibMemset(lexer->name, 0, lexer->nameSize);
					lexer->length = zero;
					++p;
					pos = p - src;
				} else if (c == '}' && lexer->arrayIndex[lexer->depth] == -1) {
					state = LEXER_STATE_OBJECT_END;
				} else if (c != ']' || lexer->arrayIndex[lexer->depth] == -1) {
					++p;
					pos = p - src;
				} else {
					state = LEXER_STATE_ARRAY_END;
				}
				break;
			case LEXER_STATE_KEY:
				if (npCommonPs4Lexer3CopyChar(p, size - pos, &consumed, lexer->partial, &lexer->partialLength,
					&lexer->remaining, decoded, &sourceLength, &length) == 1) {
					if (lexer->length + length >= lexer->nameSize || (length == 1 && sourceLength == 1 && decoded[0] == '"')) {
						state = LEXER_STATE_COLON;
						lexer->name[lexer->length] = zero;
					} else {
						sceClibMemcpy_safe(&lexer->name[lexer->length], decoded, length);
						p += consumed;
						lexer->length += length;
						pos = p - src;
					}
				} else {
					p += consumed;
					pos = p - src;
				}
				break;
			case LEXER_STATE_COLON:
				if (*p == ':') {
					matched = npCommonPs4Lexer3Match(lexer->config, lexer->depth, lexer->mode, NP_COMMON_PS4_LEXER_NODE_KEY,
						lexer->name, lexer->nameSize, &node, &flag);
					lexer->matched = matched;
					if (matched == 1) {
						depth = lexer->depth;
						lexer->nodeIndex[depth] = node;
						lexer->matchedFlag = flag;
						result = lexer->callback(lexer, NP_COMMON_PS4_LEXER_EVENT_KEY, depth, node, lexer->name, NULL,
							lexer->arrayIndex[depth], lexer->context);
						if (result < 0)
							return result;
						state = LEXER_STATE_VALUE;
						sceClibMemset(lexer->tokenText, 0, lexer->tokenSize);
					} else {
						state = LEXER_STATE_NEXT;
					}
				}
				++p;
				pos = p - src;
				break;
			case LEXER_STATE_VALUE:
				c = *p;
				if (c == '{') {
					state = LEXER_STATE_OBJECT_BEGIN;
				} else if (c == '[') {
					state = LEXER_STATE_ARRAY_BEGIN;
				} else if (c == 't' || c == 'f' || c == 'n') {
					state = LEXER_STATE_LITERAL;
					if (c == 't') {
						literal = "true";
						kind = NP_COMMON_PS4_LEXER_TOKEN_TRUE;
					} else {
						kind = zero;
						literal = "null";
						if (c == 'f') {
							literal = "false";
							kind = NP_COMMON_PS4_LEXER_TOKEN_FALSE;
						}
					}
					++p;
					lexer->tokenType = kind;
					pos = p - src;
					lexer->length = 1;
				} else if ((sceClibLookCtypeTable(c) & 4) != 0 || (c = *p) == '-') {
					lexer->tokenType = NP_COMMON_PS4_LEXER_TOKEN_NUMBER;
					lexer->tokenText[0] = *p++;
					pos = p - src;
					lexer->length = 1;
					state = LEXER_STATE_NUMBER;
				} else if (c == '"') {
					++p;
					pos = p - src;
					lexer->tokenType = NP_COMMON_PS4_LEXER_TOKEN_STRING;
					state = LEXER_STATE_STRING;
					lexer->length = zero;
				} else if (c == '}' && lexer->arrayIndex[lexer->depth] == -1) {
					state = LEXER_STATE_OBJECT_END;
				} else if (c != ']' || lexer->arrayIndex[lexer->depth] == -1) {
					++p;
					pos = p - src;
				} else {
					state = LEXER_STATE_ARRAY_END;
				}
				break;
			case LEXER_STATE_NEXT:
				c = *p;
				if (c == ',') {
					if (lexer->arrayIndex[lexer->depth] == -1) {
						state = LEXER_STATE_KEY_BEGIN;
					} else {
						state = LEXER_STATE_VALUE;
						sceClibMemset(lexer->tokenText, 0, lexer->tokenSize);
						if (lexer->matched == 1)
							++lexer->arrayIndex[lexer->depth];
					}
					++p;
					pos = p - src;
				} else if (c == '}' && lexer->arrayIndex[lexer->depth] == -1) {
					state = LEXER_STATE_OBJECT_END;
					lexer->matched = 1;
				} else if (c != ']' || lexer->arrayIndex[lexer->depth] == -1) {
					if (c == '{') {
						depth = lexer->depth + 1;
						lexer->depth = depth;
						lexer->arrayIndex[depth] = -1;
						++p;
						lexer->baseDepth = depth;
						pos = p - src;
						state = LEXER_STATE_SKIP;
					} else if (c == '[') {
						depth = lexer->depth + 1;
						lexer->depth = depth;
						++p;
						pos = p - src;
						lexer->arrayIndex[depth] = zero;
						state = LEXER_STATE_SKIP;
						lexer->baseDepth = depth;
					} else {
						++p;
						pos = p - src;
					}
				} else {
					state = LEXER_STATE_ARRAY_END;
					lexer->matched = 1;
				}
				break;
			case LEXER_STATE_LITERAL:
				if (literal != NULL && lexer->length < sceClibStrnlen(literal, 0xFFFFFFFF)
					&& *p == literal[lexer->length]) {
					lexer->length++;
					if (lexer->length >= sceClibStrnlen(literal, 0xFFFFFFFF)) {
						state = LEXER_STATE_NEXT;
						LEXER3_VALUE_READ(lexer, result);
						if (result < 0)
							return result;
					}
					++p;
					pos = p - src;
				} else {
					state = LEXER_STATE_NEXT;
				}
				break;
			case LEXER_STATE_NUMBER:
				c = *p;
				length = lexer->length;
				if ((sceClibLookCtypeTable(c) & 4) != 0 || c == '.' || c == '+' || c == '-' || c == 'e' || c == 'E') {
					if (length < lexer->tokenSize - 1) {
						lexer->tokenText[length] = *p;
						lexer->length = length + 1;
					}
					++p;
					pos = p - src;
				} else {
					state = LEXER_STATE_NEXT;
					lexer->tokenText[length] = zero;
					LEXER3_VALUE_READ(lexer, result);
					if (result < 0)
						return result;
				}
				break;
			case LEXER_STATE_STRING:
				if (npCommonPs4Lexer3CopyChar(p, size - pos, &consumed, lexer->partial, &lexer->partialLength,
					&lexer->remaining, decoded, &sourceLength, &length) == 1) {
					if (lexer->length + length >= lexer->tokenSize || (length == 1 && sourceLength == 1 && decoded[0] == '"')) {
						state = LEXER_STATE_NEXT;
						lexer->tokenText[lexer->length] = zero;
						LEXER3_VALUE_READ(lexer, result);
						if (result < 0)
							return result;
						++p;
						pos = p - src;
					} else {
						sceClibMemcpy_safe(&lexer->tokenText[lexer->length], decoded, length);
						p += consumed;
						lexer->length += length;
						pos = p - src;
					}
				} else {
					p += consumed;
					pos = p - src;
				}
				break;
			case LEXER_STATE_SKIP:
				c = *p;
				if (c == '{') {
					depth = lexer->depth;
					if (depth >= 16)
						return LEXER_ERROR_DEPTH;
					lexer->depth = depth + 1;
					lexer->arrayIndex[depth + 1] = -1;
					++p;
					pos = p - src;
				} else if (c == '[') {
					depth = lexer->depth;
					if (depth > 16)
						return LEXER_ERROR_DEPTH;
					++p;
					lexer->depth = depth + 1;
					pos = p - src;
					lexer->arrayIndex[depth + 1] = zero;
				} else if (c == '}' && lexer->arrayIndex[lexer->depth] == -1) {
					depth = lexer->depth;
					if (depth != 0) {
						lexer->depth = depth - 1;
						if (depth - 1 >= lexer->baseDepth) {
							++p;
							pos = p - src;
						} else {
							state = LEXER_STATE_OBJECT_END;
							lexer->baseDepth = zero;
						}
					} else {
						++p;
						pos = p - src;
					}
				} else if (c != ']' || lexer->arrayIndex[lexer->depth] == -1) {
					++p;
					pos = p - src;
				} else {
					depth = lexer->depth;
					if (depth != 0) {
						lexer->depth = depth - 1;
						if (depth - 1 >= lexer->baseDepth) {
							++p;
							pos = p - src;
						} else {
							state = LEXER_STATE_ARRAY_END;
							lexer->baseDepth = zero;
						}
					} else {
						++p;
						pos = p - src;
					}
				}
				break;
			default:
				++p;
				pos = p - src;
				break;
			}
		} while (size > pos);
	}
	lexer->state = state;
	return 0;
}

