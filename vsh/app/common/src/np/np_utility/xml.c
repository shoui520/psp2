/*
 * SceNpUtility: the XML the servers answer in. A streaming parser reports the elements,
 * attributes and text of a schema to a handler; a schema and a handler are paired for each
 * answer: the searchjid and matching2 servers' lists, the online ID's search, one profile,
 * several profiles and the comment's check.
 */

#include "np_utility_internal.h"

/* the servers' lists: searchjid, then matching2 and agent */
static const NpXmlElement s_searchjid[] = { { "searchjid", 0 }, { "", 1 } };
static const NpXmlElement s_searchjidFqdn[] = { { "searchjid-fqdn", 1 }, { "", 1 } };
static const NpXmlElement *const s_searchjidLevels[] = { s_searchjid, s_searchjidFqdn };
static const NpXmlSchema s_searchjidSchema = { s_searchjidLevels, 2 };

static const NpXmlElement s_matching2[] = { { "matching2", 0 }, { "", 1 } };
static const NpXmlElement s_agentFqdn[] = { { "agent-fqdn", 1 }, { "", 1 } };
static const NpXmlElement *const s_matching2Levels[] = { s_matching2, s_agentFqdn };
static const NpXmlSchema s_matching2Schema = { s_matching2Levels, 2 };

/* the online ID's search */
static const NpXmlElement s_searchjidAnswer[] = { { "searchjid", 0 }, { "", 1 } };
static const NpXmlElement s_jid[] = { { "jid", 1 }, { "", 1 } };
static const NpXmlElement *const s_jidLevels[] = { s_searchjidAnswer, s_jid };
static const NpXmlSchema s_jidSchema = { s_jidLevels, 2 };

/* a profile */
static const NpXmlElement s_profile[] = { { "profile", 0 }, { "", 1 } };
static const NpXmlElement s_profileElements[] = {
	{ "jid", 1 }, { "onlinename", 1 }, { "country", 1 }, { "language1", 1 },
	{ "language2", 1 }, { "language3", 1 }, { "aboutme", 1 }, { "avatarurl", 1 },
	{ "ucbgp", 1 }, { "panelurl", 1 }, { "plusicon", 1 }, { "ptlp", 1 }, { "", 1 },
};
static const NpXmlElement *const s_profileLevels[] = { s_profile, s_profileElements };
static const NpXmlSchema s_profileSchema = { s_profileLevels, 2 };

/* several profiles */
static const NpXmlElement s_profiles[] = { { "profile", 0 }, { "", 1 } };
static const NpXmlElement s_list[] = { { "list", 0 }, { "", 1 } };
static const NpXmlElement s_profilesProfile[] = { { "profile", 0 }, { "", 1 } };
static const NpXmlElement s_profilesElements[] = {
	{ "jid", 1 }, { "onlinename", 1 }, { "country", 1 }, { "language1", 1},
	{ "language2", 1 }, { "language3", 1 }, { "aboutme", 1 }, { "avatarurl", 1 },
	{ "ucbgp", 1 }, { "panelurl", 1 }, { "plusicon", 1 }, { "", 1 },
};
static const NpXmlElement *const s_profilesLevels[] = {
	s_profiles, s_list, s_profilesProfile, s_profilesElements,
};
static const NpXmlSchema s_profilesSchema = { s_profilesLevels, 4 };

/* the comment's check */
static const NpXmlElement s_npfilter[] = { { "npfilter", 0 }, { "", 1 } };
static const NpXmlElement s_message[] = { { "message", 1 }, { "", 1 } };
static const NpXmlElement *const s_npfilterLevels[] = { s_npfilter, s_message };
static const NpXmlSchema s_npfilterSchema = { s_npfilterLevels, 2 };

/* the most characters of a UTF-8 sequence of 1 to 6 bytes and the bits its first byte starts with */
static const SceUInt32 s_utf8Limit[6] = { 0x7F, 0x7FF, 0xFFFF, 0x1FFFFF, 0x3FFFFFF, 0x7FFFFFFF };
static const SceUInt32 s_utf8Lead[6] = { 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

__attribute__((noinline)) int SceNpUtility_31423835(NpLookupXmlParser *parser,
						    const NpXmlSchema *schema,
						    NpLookupXmlHandler handler, void *arg)
{
	sceClibMemset(parser, 0, sizeof(*parser));
	parser->schema = schema;
	parser->handler = handler;
	parser->arg = arg;
	return 0;
}

/*
 * The character a numeric reference ("&#233;", "&#xE9;") at the start of src stands for, as
 * UTF-8; *ok says src starts one. *size gets the characters' length and *used the bytes the
 * reference takes, SCE_NP_UTIL_ERROR_INSUFFICIENT when the reference is cut by src's end.
 */
static __attribute__((noinline)) int decodeReference(char *dst, SceSize dstSize,
						     const SceUInt8 *src, SceSize srcSize,
						     SceSize *size, SceSize *used, char *ok)
{
	const SceUInt8 *digits;
	const SceUInt8 *end = NULL;
	const SceUInt32 *limit;
	SceUInt32 code;
	SceSize length = 0;
	SceSize consumed;
	SceSize count;
	SceSize max;
	char text[16];
	int hex = 0;
	int ret = 0;
	int i;

	sceClibMemset(text, 0, 13);
	if (dst == NULL || src == NULL || size == NULL || used == NULL || ok == NULL ||
	    srcSize < 3)
		return ret;
	*ok = 0;
	if ((src[0] & 0x80) != 0 || (src[1] & 0x80) != 0)
		return ret;
	if (src[0] != '&' || src[1] != '#')
		return ret;
	if ((src[2] & 0x80) != 0)
		return ret;
	digits = src + 2;
	if (src[2] == 'x') {
		hex = 1;
		digits = src + 3;
	}
	max = srcSize;
	if (max > 12)
		max = 12;
	for (count = 0; count < max; count++) {
		if ((digits[count] & 0x80) != 0)
			return ret;
		if (digits[count] == ';') {
			if (count == 0)
				return ret;
			end = digits + count;
			break;
		}
		if (hex) {
			if ((digits[count] < '0' || digits[count] > '9') &&
			    (digits[count] < 'A' || digits[count] > 'F') &&
			    (digits[count] < 'a' || digits[count] > 'f'))
				return ret;
		} else if (digits[count] < '0' || digits[count] > '9') {
			return ret;
		}
	}

	*ok = 1;
	if (end != NULL) {
		consumed = end - src + 1;
		sceClibMemcpy_safe(text, digits, end - digits);
		if (hex)
			code = SceNpCommon_4C4AA451(text, NULL, 16);
		else
			code = SceNpCommon_4C4AA451(text, NULL, 10);
		if (code < 9 || code == 11 || code == 12 || (code > 13 && code < 32) ||
		    (code > 0xD7FF && code < 0xE000) || (code > 0xFFFD && code < 0x10000) ||
		    code > 0x10FFFF) {
			*ok = 0;
			return ret;
		}
		limit = s_utf8Limit;
		for (i = 1; i < 7; i++) {
			if (code <= *limit)
				break;
			limit++;
		}
		if (i > 6) {
			*ok = 0;
			return ret;
		}
		length = i;
		if (dstSize >= i) {
			if (i > 1) {
				for (count = i - 1; count != 0; count--) {
					dst[count] = (code & 0x3F) | 0x80;
					code >>= 6;
				}
			}
			dst[0] = code | s_utf8Lead[i - 1];
		} else {
			ret = SCE_NP_UTIL_ERROR_INSUFFICIENT;
		}
	} else {
		ret = SCE_NP_UTIL_ERROR_INSUFFICIENT;
		consumed = srcSize + 1;
	}
	*size = length;
	*used = consumed;
	return ret;
}

/* decodeReference, or the first character of escaped text when src starts no reference */
static inline int decodeCharacter(char *dst, const char *src, SceSize srcSize, SceSize *size,
				  SceSize *used)
{
	char ok = 0;
	int ret;

	ret = decodeReference(dst, 6, (const SceUInt8 *)src, srcSize, size, used, &ok);
	if (ok == 0)
		ret = SceNpCommon_38B54F30(dst, 6, src, srcSize, size, used);
	return ret;
}

/*
 * Parses the next size bytes of an XML document; the parser keeps its state between calls.
 * The handler gets each element of the schema as it starts and ends (with its text, for an
 * element that has text) and each attribute.
 */
__attribute__((noinline)) int SceNpUtility_C01F7C6A(NpLookupXmlParser *parser, const char *data,
						    int size)
{
	const NpXmlElement *elements;
	const char *p = data;
	SceUInt32 state = parser->state;
	SceUInt32 depth;
	SceUInt32 index;
	SceSize length;
	SceSize n;
	SceSize charSize;
	SceSize charUsed;
	char ch[8];
	int called;
	int ret = 0;
	int c;

	while ((int)(p - data) < size) {
		switch (state) {
		case NP_LOOKUP_XML_STATE_TEXT_OUTSIDE:
			if (*p == '<') {
				parser->length = 0;
				parser->closed = 0;
				parser->flags = 0;
				sceClibMemset(parser->name, 0, sizeof(parser->name));
				sceClibMemset(parser->value, 0, sizeof(parser->value));
				state = NP_LOOKUP_XML_STATE_TAG;
			}
			p++;
			continue;

		case NP_LOOKUP_XML_STATE_TEXT:
			n = size - (p - data);
			if (parser->pendingNeeded != 0) {
				if (n >= parser->pendingNeeded)
					n = parser->pendingNeeded;
				sceClibMemcpy_safe(parser->pending + parser->pendingLength, p, n);
				parser->pendingLength += n;
				parser->pendingNeeded -= n;
				if (parser->pendingNeeded != 0) {
					p += n;
					continue;
				}
				ret = decodeCharacter(ch, parser->pending, parser->pendingLength,
						      &charSize, &charUsed);
				if (ret == SCE_NP_UTIL_ERROR_INSUFFICIENT) {
					p += n;
					parser->pendingNeeded = charUsed - parser->pendingLength;
					continue;
				}
				if (ret >= 0) {
					parser->pendingLength = 0;
					parser->pendingNeeded = 0;
				} else {
					charSize = 1;
					charUsed = 1;
					sceClibMemcpy_safe(ch, parser->pending, 1);
				}
			} else {
				if (n >= 6)
					n = 6;
				ret = decodeCharacter(ch, p, n, &charSize, &charUsed);
				if (ret == SCE_NP_UTIL_ERROR_INSUFFICIENT) {
					sceClibMemcpy_safe(parser->pending, p, n);
					parser->pendingLength = n;
					parser->pendingNeeded = charUsed - n;
					p += n;
					continue;
				}
				if (ret >= 0) {
					n = charUsed;
				} else {
					n = 1;
					charSize = 1;
					charUsed = 1;
					sceClibMemcpy_safe(ch, p, 1);
				}
			}
			length = parser->length;
			if (length + charSize < sizeof(parser->value) &&
			    (charSize != 1 || charUsed != 1 || ch[0] != '<')) {
				sceClibMemcpy_safe(parser->value + length, ch, charSize);
				p += n;
				parser->length += charSize;
				continue;
			}
			parser->value[length] = '\0';
			parser->length = 0;
			sceClibMemset(parser->name, 0, sizeof(parser->name));
			parser->closed = 0;
			parser->flags = 0;
			p++;
			state = NP_LOOKUP_XML_STATE_TAG;
			continue;

		case NP_LOOKUP_XML_STATE_TAG:
			if (*p == '/' && parser->length == 0) {
				p++;
				state = NP_LOOKUP_XML_STATE_END_TAG;
				if ((int)(p - data) >= size)
					goto out;
			}
			/* fall through */
		case NP_LOOKUP_XML_STATE_END_TAG:
			length = parser->length;
			c = *(const SceUInt8 *)p;
			if (length < sizeof(parser->name) - 1 && c != ' ' && c != '\t' &&
			    c != '\r' && c != '\n' && c != '/' && c != '<' && c != '>') {
				parser->name[length] = c;
				parser->length = length + 1;
				p++;
				continue;
			}
			if (c == '/')
				parser->closed = 1;
			parser->name[length] = '\0';
			if (state == NP_LOOKUP_XML_STATE_END_TAG) {
				ret = 0;
				depth = parser->depth;
				called = 0;
				if (depth != 0 && depth <= parser->schema->levelCount) {
					elements = parser->schema->levels[depth - 1];
					index = parser->index[depth - 1];
					if (parser->closed == 1 ||
					    sceClibStrncasecmp(elements[index].name, parser->name,
							       sizeof(parser->name)) == 0) {
						if (elements[index].hasText == 1)
							ret = parser->handler(parser,
									      NP_XML_EVENT_TEXT,
									      parser->depth - 1, index,
									      parser->value,
									      parser->value,
									      parser->arg);
						else
							ret = parser->handler(parser,
									      NP_XML_EVENT_END,
									      parser->depth - 1, index,
									      NULL, NULL,
									      parser->arg);
						called = 1;
					}
				}
				if (ret < 0)
					return ret;
				if (called == 1)
					parser->depth--;
				state = NP_LOOKUP_XML_STATE_TEXT_OUTSIDE;
				p++;
				continue;
			}
			ret = 0;
			depth = parser->depth;
			called = 0;
			if (depth < parser->schema->levelCount) {
				elements = parser->schema->levels[depth];
				for (index = 0; sceClibStrnlen(elements[index].name,
							       sizeof(parser->name)) != 0; index++) {
					if (sceClibStrncasecmp(elements[index].name, parser->name,
							       sizeof(parser->name)) == 0)
						break;
				}
				if (sceClibStrnlen(elements[index].name, sizeof(parser->name)) != 0) {
					parser->index[parser->depth] = index;
					ret = parser->handler(parser, NP_XML_EVENT_START,
							      parser->depth, index, parser->name,
							      NULL, parser->arg);
					called = 1;
				}
			}
			if (ret < 0)
				return ret;
			if (called == 0) {
				state = NP_LOOKUP_XML_STATE_TEXT_OUTSIDE;
				continue;
			}
			state = NP_LOOKUP_XML_STATE_ATTRIBUTES;
			continue;

		case NP_LOOKUP_XML_STATE_ATTRIBUTES:
			c = *(const SceUInt8 *)p;
			if (c == '>') {
				depth = parser->depth++;
				if (p[-1] == '/')
					parser->closed = 1;
				if (parser->closed == 1) {
					state = NP_LOOKUP_XML_STATE_END_TAG;
					parser->flags = 0;
					continue;
				}
				if (parser->schema->levels[depth][parser->index[depth]].hasText == 1) {
					state = NP_LOOKUP_XML_STATE_TEXT;
					parser->length = 0;
					sceClibMemset(parser->value, 0, sizeof(parser->value));
					parser->pendingLength = 0;
					parser->pendingNeeded = 0;
					sceClibMemset(parser->pending, 0, 6);
				} else {
					state = NP_LOOKUP_XML_STATE_TEXT_OUTSIDE;
				}
				p++;
				continue;
			}
			if (c == '/' || c == ' ' || c == '\r' || c == '\n' || c == '\t') {
				p++;
				continue;
			}
			parser->flags = 0;
			parser->length = 0;
			sceClibMemset(parser->name, 0, sizeof(parser->name));
			sceClibMemset(parser->value, 0, sizeof(parser->value));
			state = NP_LOOKUP_XML_STATE_ATTRIBUTE_NAME;
			continue;

		case NP_LOOKUP_XML_STATE_ATTRIBUTE_NAME:
			length = parser->length;
			if (length >= sizeof(parser->name) - 1)
				goto attribute;
			c = *(const SceUInt8 *)p;
			if (parser->flags != 1) {
				if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '/' &&
				    c != '=' && c != '>') {
					parser->name[length] = c;
					parser->length = length + 1;
					p++;
					continue;
				}
				parser->name[length] = '\0';
				parser->flags = 1;
				c = *(const SceUInt8 *)p;
			}
			switch (c) {
			case '/':
				parser->closed = 1;
				/* fall through */
			case '\t':
			case '\n':
			case '\r':
			case ' ':
				p++;
				continue;
			case '=':
				p++;
				state = NP_LOOKUP_XML_STATE_ATTRIBUTE_VALUE;
				parser->flags = 0;
				parser->length = 0;
				continue;
			default:
				goto attribute;
			}

		case NP_LOOKUP_XML_STATE_ATTRIBUTE_VALUE:
			length = parser->length;
			if (length < sizeof(parser->value) - 1) {
				c = *(const SceUInt8 *)p;
				if ((parser->flags & NP_LOOKUP_XML_FLAG_QUOTED) != 0 && c != '\'' &&
				    c != '"') {
					parser->value[length] = c;
					p++;
					parser->length = length + 1;
					continue;
				}
				switch (c) {
				case '\t':
				case '\n':
				case '\r':
				case ' ':
					if ((parser->flags & NP_LOOKUP_XML_FLAG_STARTED) == 0) {
						p++;
						continue;
					}
					break;
				case '"':
				case '\'':
					if ((parser->flags & NP_LOOKUP_XML_FLAG_QUOTED) != 0)
						parser->flags &= ~NP_LOOKUP_XML_FLAG_QUOTED;
					else
						parser->flags |= NP_LOOKUP_XML_FLAG_STARTED |
								 NP_LOOKUP_XML_FLAG_QUOTED;
					p++;
					continue;
				case '/':
				case '>':
					break;
				default:
					parser->flags |= NP_LOOKUP_XML_FLAG_STARTED;
					parser->value[length] = *p;
					parser->length = length + 1;
					p++;
					continue;
				}
				if (c == '/')
					parser->closed = 1;
			}
			parser->value[length] = '\0';
attribute:
			ret = parser->handler(parser, NP_XML_EVENT_ATTRIBUTE, parser->depth,
					      parser->index[parser->depth], parser->name,
					      parser->value, parser->arg);
			if (ret < 0)
				return ret;
			state = NP_LOOKUP_XML_STATE_ATTRIBUTES;
			continue;

		default:
			goto out;
		}
	}
out:
	parser->state = state;
	return 0;
}

int SceNpUtility_E2135903(NpLookupXmlParser *parser, const char *buf, int size)
{
	return SceNpUtility_C01F7C6A(parser, buf, size);
}

/*
 * The result attribute of a response's root element: 0 when it is "00", else the server's error
 * code, or a bad response when it is not two hexadecimal digits.
 */
static inline int parseResult(int event, SceUInt32 depth, SceUInt32 index, const char *name,
			      const char *value)
{
	int code;
	int ret = 0;

	if (event == NP_XML_EVENT_ATTRIBUTE && depth == 0 && index == 0 &&
	    sceClibStrncmp("result", name, 0x80) == 0) {
		if (value != NULL && sceClibStrnlen(value, 0x7FFFFFFF) == 2) {
			code = SceNpCommon_5B770B80(value, NULL, 16);
			ret = code;
			if (code < 0 || code > 0xFF) {
				ret = SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
			} else if (code != 0) {
				ret = code - 2141911040;
			}
		} else {
			ret = SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		}
	}
	return ret;
}

int SceNpUtility_E9544993(int event, SceUInt32 depth, SceUInt32 index, const char *name,
			  const char *value)
{
	return parseResult(event, depth, index, name, value);
}

/*
 * The avatar URL of a profile: the id attribute of the element, then its text into url; the
 * flags note the attribute was seen, *count how many were read.
 */
int SceNpUtility_C02139E5(int event, const char *name, const char *value, char *url, int *id,
			  SceUInt32 *flags, int *count)
{
	if (event == NP_XML_EVENT_ATTRIBUTE) {
		if (sceClibStrncmp("id", name, 0x80) == 0 && id != NULL)
			*id = SceNpCommon_CC8E6AC6(value);
		if (flags != NULL)
			*flags |= 8;
	}
	if (event == NP_XML_EVENT_TEXT) {
		if (sceClibStrnlen(name, 0x7FFFFFFF) > 0x7F)
			return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		sceClibStrncpy(url, name, 0x7F);
		if (flags != NULL) {
			if ((*flags & 8) == 0)
				return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
			*flags &= ~8;
		}
		if (count != NULL)
			++*count;
	}
	return 0;
}

/* the servers' list parser's state at the end of the answer */
int SceNpUtility_AB499EA4(NpLookupServersParse *state)
{
	if ((state->flags & 0x40) == 0)
		return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
	return 0;
}

/* the servers' list: a server's id, status and port, then its name */
static int serversHandler(NpLookupXmlParser *parser, int event, SceUInt32 depth, SceUInt32 index,
			  const char *name, const char *value, void *arg)
{
	NpLookupServersParse *state = arg;
	NpLookupServer *server;
	NpLookupServer *next;
	SceSize length;
	SceUInt32 flags;
	int ret;

	if (event == NP_XML_EVENT_ATTRIBUTE && depth == 1 && index == 0) {
		if (sceClibStrncmp("id", name, 0x80) != 0) {
			if (sceClibStrncmp("status", name, 0x80) != 0) {
				if (sceClibStrncmp("port", name, 0x80) == 0) {
					if (value == NULL) {
						ret = SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
						goto error;
					}
					state->port = SceNpCommon_CC8E6AC6(value);
					state->flags |= 0x20;
				}
			} else {
				if (value == NULL) {
					ret = SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
					goto error;
				}
				if (sceClibStrncasecmp(value, "alive", 6) != 0) {
					ret = sceClibStrncasecmp(value, "maintenance", 12);
					flags = state->flags;
					if (ret == 0)
						state->status = NP_LOOKUP_SERVER_STATUS_MAINTENANCE;
				} else {
					flags = state->flags;
					state->status = NP_LOOKUP_SERVER_STATUS_ALIVE;
				}
				state->flags = flags | 0x10;
			}
		} else {
			if (value == NULL) {
				ret = SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
				goto error;
			}
			state->id = SceNpCommon_CC8E6AC6(value);
			state->flags |= 8;
		}
	}
	if (event == NP_XML_EVENT_TEXT && depth == 1 && index == 0) {
		if (*name == '\0' ||
		    (state->type == 2 && ((state->flags & 8) == 0 || (state->flags & 0x10) == 0 ||
					  (state->flags & 0x20) == 0))) {
			ret = SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
			goto error;
		}
		length = sceClibStrnlen(name, 0x7FFFFFFF);
		server = SceNpUtility_8CA24D19(length + 17, state->service);
		if (server == NULL) {
			ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
			goto error;
		}
		sceClibMemset(server, 0, 16);
		sceClibStrncpy((char *)server + 16, name, sceClibStrnlen(name, 0x7FFFFFFF) + 1);
		server->name = (char *)server + 16;
		server->id = state->id;
		state->flags &= ~8;
		state->id = 0;
		server->status = state->status;
		state->flags &= ~0x10;
		state->status = 0;
		server->port = state->port;
		state->flags &= ~0x20;
		state->port = 0;
		if (*state->head == NULL)
			*state->head = server;
		if (state->tail != NULL)
			state->tail->next = server;
		state->tail = server;
		state->flags |= 4;
	}
	if (event == NP_XML_EVENT_END && depth == 0 && index == 0) {
		if ((state->flags & 4) == 0) {
			ret = SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
			goto error;
		}
		state->flags |= 0x40;
	}
	return 0;
error:
	for (server = *state->head; server != NULL; server = next) {
		next = server->next;
		SceNpUtility_6C1161AD(server, state->service);
	}
	*state->head = NULL;
	return ret;
}

/* the servers' list parser: type 1 searchjid, type 2 matching2 and agent */
int SceNpUtility_15227842(NpLookupXmlParser *parser, int type, int reserved,
			  NpLookupServersParse *state)
{
	state->reserved = reserved;
	state->type = type;
	state->flags = 0;
	*state->head = NULL;
	state->tail = NULL;
	if (type > 1) {
		if (type <= 2)
			return SceNpUtility_31423835(parser, &s_matching2Schema, serversHandler,
						     state);
		return NP_LOOKUP_ERROR_UNKNOWN_SERVERS;
	}
	if (type <= 0)
		return NP_LOOKUP_ERROR_UNKNOWN_SERVERS;
	return SceNpUtility_31423835(parser, &s_searchjidSchema, serversHandler, state);
}

/* the online ID's search: the NP ID of the answer's jid */
static int jidHandler(NpLookupXmlParser *parser, int event, SceUInt32 depth, SceUInt32 index,
		      const char *name, const char *value, void *arg)
{
	NpLookupJidParse *state = arg;
	int ret;

	ret = parseResult(event, depth, index, name, value);
	if (ret >= 0) {
		state->flags |= 1;
		if (event == NP_XML_EVENT_TEXT && depth == 1 && index == 0) {
			ret = SceNpUtility_54C48F92(state->npId, name, state->service);
			if (ret < 0)
				return ret;
			state->flags |= 2;
		}
		if (event == NP_XML_EVENT_END && depth == 0 && index == 0) {
			if ((state->flags & 1) == 0 || (state->flags & 2) == 0)
				return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
			state->flags |= 4;
		}
		return 0;
	}
	return ret;
}

int SceNpUtility_2580CDAF(NpLookupXmlParser *parser, NpLookupJidParse *state)
{
	state->flags = 0;
	return SceNpUtility_31423835(parser, &s_jidSchema, jidHandler, state);
}

int SceNpUtility_5D75C414(NpLookupJidParse *state)
{
	if ((state->flags & 4) == 0)
		return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
	return 0;
}

/* a profile's elements: the attributes and text each reads into profile */
static int profileHandler(NpLookupXmlParser *parser, int event, SceUInt32 depth, SceUInt32 index,
			  const char *name, const char *value, void *arg)
{
	NpLookupProfileParse *state = arg;
	SceInt32 language;
	int ret;

	ret = parseResult(event, depth, index, name, value);
	if (ret < 0)
		return ret;
	state->flags |= 1;
	if (event == NP_XML_EVENT_TEXT && depth == 1 && index == 0) {
		ret = SceNpUtility_54C48F92(state->profile, name, state->service);
		if (ret < 0)
			return ret;
		state->flags |= 2;
	}
	if (depth == 1 && index == 1) {
		SceInt32 *update = &state->update;
		SceRtcTick *date = &state->date;

		if (event == NP_XML_EVENT_TEXT && state->profile->onlineName != NULL)
			sceClibStrncpy(state->profile->onlineName, name, sizeof(state->profile->onlineName));
		if (event == NP_XML_EVENT_ATTRIBUTE) {
			if (update == NULL || sceClibStrncmp("upd", name, 0x80) != 0) {
				if (date != NULL && sceClibStrncmp("date", name, 0x80) == 0 &&
				    sceRtcParseDateTime(date, value) < 0) {
					ret = SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
					goto check;
				}
			} else {
				*update = SceNpCommon_CC8E6AC6(value);
			}
		}
		ret = 0;
check:
		if (ret < 0)
			return ret;
		if (event == NP_XML_EVENT_TEXT)
			state->flags |= 4;
	}
	if (event == NP_XML_EVENT_TEXT && depth == 1 && index == 2) {
		sceClibMemcpy_safe(&state->profile->countryCode, name, 2);
		state->flags |= 8;
	}
	if (event == NP_XML_EVENT_TEXT && depth == 1 && (index == 3 || index == 4 || index == 5)) {
		language = -1;
		if (sceClibStrnlen(name, 0x7FFFFFFF) != 0) {
			language = SceNpCommon_CC8E6AC6(name);
			if (language > 22)
				language = 1;
		}
		if (index > 3) {
			if (index > 4) {
				if (sceClibStrnlen(name, 0x7FFFFFFF) != 0)
					state->profile->languages.language3 = language;
				else
					state->profile->languages.language3 = -1;
				state->flags |= 0x40;
			} else {
				if (sceClibStrnlen(name, 0x7FFFFFFF) != 0)
					state->profile->languages.language2 = language;
				else
					state->profile->languages.language2 = -1;
				state->flags |= 0x20;
			}
		} else {
			state->profile->languages.language1 = language;
			state->flags |= 0x10;
		}
	}
	if (event == NP_XML_EVENT_TEXT && depth == 1 && index == 6) {
		if (*name != '\0') {
			if (sceClibStrnlen(name, 0x7FFFFFFF) + 1 > sizeof(state->profile->aboutMe))
				return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
			sceClibStrncpy(state->profile->aboutMe.data, name, sizeof(state->profile->aboutMe));
		}
		state->flags |= 0x80;
	}
	if (depth == 1 && index == 7) {
		ret = SceNpUtility_C02139E5(event, name, value, state->profile->icon.data, &state->profile->iconId,
					    NULL, NULL);
		if (ret < 0)
			return ret;
		if (event == NP_XML_EVENT_TEXT)
			state->flags |= 0x100;
	}
	if (event == NP_XML_EVENT_TEXT && depth == 1 && index == 8)
		state->profile->color.ucbgp = SceNpCommon_CF843378(name, NULL, 16);
	if (depth == 1 && index == 9) {
		if (event == NP_XML_EVENT_START)
			state->profile->color.panel.type = 2;
		if (event == NP_XML_EVENT_TEXT) {
			if (sceClibStrnlen(name, 0x7FFFFFFF) > sizeof(state->profile->panelUrl))
				return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
			sceClibStrncpy(state->profile->panelUrl, name, sizeof(state->profile->panelUrl));
		}
		if (event == NP_XML_EVENT_ATTRIBUTE) {
			if (sceClibStrncmp("id", name, 0x80) != 0) {
				if (sceClibStrncmp("bgc", name, 0x80) == 0) {
					if (value == NULL)
						return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
					state->profile->color.panel.bgColor =
						SceNpCommon_CF843378(value, NULL, 16);
				}
			} else {
				if (value == NULL)
					return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
				state->profile->panelId = SceNpCommon_CC8E6AC6(value);
			}
		}
	}
	if (event == NP_XML_EVENT_TEXT && depth == 1 && index == 10)
		state->profile->plusIcon = SceNpCommon_CC8E6AC6(name);
	if (event == NP_XML_EVENT_TEXT && depth == 1 && index == 11) {
		state->ptlp = SceNpCommon_CC8E6AC6(name);
		state->flags |= 0x200;
	}
	if (event == NP_XML_EVENT_END && depth == 0 && index == 0) {
		if ((state->flags & 1) == 0 || (state->flags & 4) == 0 || (state->flags & 8) == 0 ||
		    (state->flags & 0x10) == 0 || (state->flags & 0x20) == 0 ||
		    (state->flags & 0x40) == 0 || (state->flags & 0x80) == 0 ||
		    (state->flags & 0x100) == 0 || (state->flags & 0x200) == 0)
			return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		if (state->mode == 1 && (state->flags & 2) == 0)
			return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		state->flags |= 0x400;
	}
	return 0;
}

/* the profile parser; mode 1 expects the user's JID, mode 2 not */
int SceNpUtility_839F805E(NpLookupXmlParser *parser, int mode, NpLookupProfileParse *state)
{
	state->mode = mode;
	state->flags = 0;
	sceClibMemset(state->profile, 0, sizeof(*state->profile));
	state->profile->color.panel.bgColor = 0;
	state->profile->color.panel.type = 0;
	state->profile->plusIcon = 0;
	state->profile->languages.language2 = -1;
	state->profile->languages.language3 = -1;
	state->profile->languages.language1 = -1;
	return SceNpUtility_31423835(parser, &s_profileSchema, profileHandler, state);
}

int SceNpUtility_F32ED701(NpLookupProfileParse *state)
{
	if ((state->flags & 0x400) == 0)
		return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
	return 0;
}

/* several profiles: each profile element names its user's JID; its elements fill that profile */
static int profilesHandler(NpLookupXmlParser *parser, int event, SceUInt32 depth, SceUInt32 index,
			   const char *name, const char *value, void *arg)
{
	NpLookupProfilesParse *state = arg;
	NpLookupProfile *profile = NULL;
	SceNpId npId;
	SceUInt32 i;
	SceInt32 language;
	int ret;

	if (state->inProfile != 0) {
		if (state->index < 0 || (SceUInt32)state->index > state->count - 1)
			return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
		profile = &state->profiles[state->index];
		if (profile == NULL)
			return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	}
	ret = parseResult(event, depth, index, name, value);
	if (ret < 0)
		return ret;
	state->flags |= 1;
	if (event == NP_XML_EVENT_END && depth == 1 && index == 0)
		state->flags |= 0x400;
	if (depth == 2 && index == 0) {
		if (event == NP_XML_EVENT_END) {
			state->answered++;
			if (state->inProfile == 0)
				return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
			if ((state->flags & 1) != 0 && (state->flags & 2) != 0 &&
			    (state->flags & 4) != 0 && (state->flags & 8) != 0 &&
			    (state->flags & 0x10) != 0 && (state->flags & 0x20) != 0 &&
			    (state->flags & 0x40) != 0 && (state->flags & 0x80) != 0 &&
			    (state->flags & 0x100) != 0)
				state->results[state->index] = 0;
			else
				state->results[state->index] = SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
			state->inProfile = 0;
			state->index = -1;
		}
		if (event == NP_XML_EVENT_ATTRIBUTE && sceClibStrncmp("jid", name, 0x80) == 0) {
			if (value == NULL)
				return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
			sceClibMemset(&npId, 0, sizeof(npId));
			ret = SceNpUtility_54C48F92(&npId, value, state->service);
			if (ret < 0)
				return ret;
			for (i = 0; i < state->count; i++) {
				ret = sceNpCmpNpId(&state->profiles[i].npId, &npId);
				if (ret == 0) {
					state->flags = 0;
					state->inProfile = 1;
					state->index = i;
					break;
				}
				if (ret != SCE_NP_UTIL_ERROR_NOT_MATCH && ret < 0)
					return ret;
			}
			if (state->inProfile == 0)
				return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
			state->flags |= 2;
			state->results[state->index] = SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		}
	}
	if (event == NP_XML_EVENT_TEXT && depth == 3 && index == 0) {
		if (profile == NULL)
			return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		ret = SceNpUtility_54C48F92(&profile->npId, name, state->service);
		if (ret < 0)
			return ret;
		state->flags |= 2;
	}
	if (depth == 3 && index == 1) {
		if (profile == NULL)
			return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		if (event == NP_XML_EVENT_TEXT && profile->onlineName != NULL)
			sceClibStrncpy(profile->onlineName, name, sizeof(profile->onlineName));
		if (event == NP_XML_EVENT_TEXT)
			state->flags |= 4;
	}
	if (event == NP_XML_EVENT_TEXT && depth == 3 && index == 2) {
		if (profile == NULL)
			return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		sceClibMemcpy_safe(&profile->countryCode, name, 2);
		state->flags |= 8;
	}
	if (event == NP_XML_EVENT_TEXT && depth == 3 && (index == 3 || index == 4 || index == 5)) {
		if (profile == NULL)
			return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		language = -1;
		if (sceClibStrnlen(name, 0x7FFFFFFF) != 0) {
			language = SceNpCommon_CC8E6AC6(name);
			if (language > 22)
				language = 1;
		}
		if (index > 3) {
			if (index > 4) {
				if (sceClibStrnlen(name, 0x7FFFFFFF) != 0)
					profile->languages.language3 = language;
				else
					profile->languages.language3 = -1;
				state->flags |= 0x40;
			} else {
				if (sceClibStrnlen(name, 0x7FFFFFFF) != 0)
					profile->languages.language2 = language;
				else
					profile->languages.language2 = -1;
				state->flags |= 0x20;
			}
		} else {
			profile->languages.language1 = language;
			state->flags |= 0x10;
		}
	}
	if (event == NP_XML_EVENT_TEXT && depth == 3 && index == 6) {
		if (profile == NULL)
			return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		if (*name != '\0') {
			if (sceClibStrnlen(name, 0x7FFFFFFF) + 1 > sizeof(profile->aboutMe))
				return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
			sceClibStrncpy(profile->aboutMe.data, name, sizeof(profile->aboutMe));
		}
		state->flags |= 0x80;
	}
	if (depth == 3 && index == 7) {
		if (profile == NULL)
			return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		ret = SceNpUtility_C02139E5(event, name, value, profile->icon.data, &profile->iconId,
					    NULL, NULL);
		if (ret < 0)
			return ret;
		if (event == NP_XML_EVENT_TEXT)
			state->flags |= 0x100;
	}
	if (event == NP_XML_EVENT_TEXT && depth == 3 && index == 8) {
		if (profile == NULL)
			return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		profile->color.ucbgp = SceNpCommon_CF843378(name, NULL, 16);
	}
	if (depth == 3 && index == 9) {
		if (profile == NULL)
			return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		if (event == NP_XML_EVENT_START)
			profile->color.panel.type = 2;
		if (event == NP_XML_EVENT_TEXT) {
			if (sceClibStrnlen(name, 0x7FFFFFFF) > sizeof(profile->panelUrl))
				return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
			sceClibStrncpy(profile->panelUrl, name, sizeof(profile->panelUrl));
		}
		if (event == NP_XML_EVENT_ATTRIBUTE) {
			if (sceClibStrncmp("id", name, 0x80) != 0) {
				if (sceClibStrncmp("bgc", name, 0x80) == 0) {
					if (value == NULL)
						return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
					profile->color.panel.bgColor =
						SceNpCommon_CF843378(value, NULL, 16);
				}
			} else {
				if (value == NULL)
					return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
				profile->panelId = SceNpCommon_CC8E6AC6(value);
			}
		}
	}
	if (event == NP_XML_EVENT_TEXT && depth == 3 && index == 10) {
		if (profile == NULL)
			return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		profile->plusIcon = SceNpCommon_CC8E6AC6(name);
	}
	return 0;
}

int SceNpUtility_04AA56CB(NpLookupXmlParser *parser, NpLookupProfilesParse *state)
{
	state->index = -1;
	state->inProfile = 0;
	state->answered = 0;
	state->flags = 0;
	return SceNpUtility_31423835(parser, &s_profilesSchema, profilesHandler, state);
}

int SceNpUtility_A3767D45(NpLookupProfilesParse *state)
{
	if ((state->flags & 0x400) == 0)
		return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
	return 0;
}

/* the comment's check: the check attribute, and the sanitized text of a sanitize request */
static int censorHandler(NpLookupXmlParser *parser, int event, SceUInt32 depth, SceUInt32 index,
			 const char *name, const char *value, void *arg)
{
	NpLookupCensorParse *state = arg;
	char *sanitized;
	int ret;

	ret = parseResult(event, depth, index, name, value);
	if (ret < 0)
		return ret;
	state->flags |= 1;
	if (depth == 1 && index == 0) {
		if (event == NP_XML_EVENT_TEXT && state->sanitize == 1) {
			if (sceClibStrnlen(name, 0x7FFFFFFF) > sceClibStrnlen(state->text, 0x400))
				return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
			sanitized = state->sanitized;
			if (sanitized == NULL)
				return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
			state->flags |= 4;
			sceClibStrncpy(sanitized, name, sceClibStrnlen(state->text, 0x400));
		}
		if (event == NP_XML_EVENT_ATTRIBUTE && sceClibStrncmp("check", name, 0x80) == 0) {
			state->flags |= 2;
			if (value == NULL)
				return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
			if (sceClibStrncmp("ok", value, 0x80) != 0) {
				if (sceClibStrncmp("ng", value, 0x80) != 0)
					return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
				state->passed = 0;
			} else {
				state->passed = 1;
			}
		}
	}
	if (event == NP_XML_EVENT_END && depth == 0 && index == 0) {
		if ((state->flags & 1) == 0 || (state->flags & 2) == 0)
			return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		if (state->sanitize == 1 && (state->flags & 4) == 0)
			return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
		state->flags |= 8;
	}
	return 0;
}

int SceNpUtility_67147C73(NpLookupXmlParser *parser, NpLookupCensorParse *state)
{
	state->passed = 0;
	state->flags = 0;
	return SceNpUtility_31423835(parser, &s_npfilterSchema, censorHandler, state);
}

int SceNpUtility_A9D5542A(NpLookupCensorParse *state)
{
	if ((state->flags & 8) == 0)
		return SCE_NP_COMMUNITY_ERROR_BAD_RESPONSE;
	return 0;
}
