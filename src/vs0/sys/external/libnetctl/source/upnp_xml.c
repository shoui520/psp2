/*
 * SceNetCtl UPnP client: streaming XML parser.
 *
 * Data may arrive in pieces of any size; the parser keeps its state between
 * calls.  Only elements listed in the schema for the current depth are
 * tracked: their start, attributes, end and (for leaf elements) text content
 * are reported to the callback.  Text content is entity-decoded and kept as
 * whole UTF-8 characters.
 */

#include "netctl_internal.h"
#include "upnp.h"

__attribute__((noinline))
int netCtlUpnpXmlInit(UpnpXmlParser *parser, const UpnpXmlSchema *schema,
		      UpnpXmlCallback callback, void *arg)
{
	sceClibMemset(parser, 0, sizeof(*parser));
	parser->schema = schema;
	parser->callback = callback;
	parser->arg = arg;
	return 0;
}

static inline __attribute__((always_inline))
int netCtlUpnpXmlMatch(const UpnpXmlParser *parser, const UpnpXmlElement *element)
{
	SceSize nameLen, len;

	if (element->matchSuffix == 0)
		return sceClibStrncasecmp(element->name, parser->name, UPNP_XML_NAME_SIZE) == 0;
	if (element->matchSuffix == 1) {
		nameLen = sceClibStrnlen(element->name, UPNP_XML_NAME_SIZE);
		len = sceClibStrnlen(parser->name, UPNP_XML_NAME_SIZE);
		if (len >= nameLen &&
		    sceClibStrncasecmp(element->name, parser->name + (len - nameLen),
				       UPNP_XML_NAME_SIZE) == 0)
			return 1;
	}
	return 0;
}

static inline __attribute__((always_inline)) int netCtlUpnpXmlIsSpace(int c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

__attribute__((noinline))
int netCtlUpnpXmlParse(UpnpXmlParser *parser, const char *data, int size)
{
	const unsigned char *cur = (const unsigned char *)data;
	const UpnpXmlElement *list, *element;
	int state = parser->state;
	int ret = 0;
	int pos = 0;
	int matched;
	unsigned char decoded[6];
	SceSize outLen, consumed, need, n;
	SceUInt32 depth, index;
	SceSize len;
	unsigned int c;

	while (pos < size) {
		switch (state) {
		case UPNP_XML_STATE_TEXT:
			if (*cur == '<') {
				parser->closing = 0;
				parser->flags = 0;
				parser->len = 0;
				sceClibMemset(parser->name, 0, sizeof(parser->name));
				sceClibMemset(parser->value, 0, sizeof(parser->value));
				state = UPNP_XML_STATE_TAG;
			}
			cur++;
			pos = (const char *)cur - data;
			break;

		case UPNP_XML_STATE_TAG:
			if (*cur == '/' && parser->len == 0) {
				cur++;
				pos = (const char *)cur - data;
				state = UPNP_XML_STATE_END_TAG;
				if (pos >= size)
					break;
			}
			/* fall through */
		case UPNP_XML_STATE_END_TAG:
			len = parser->len;
			c = *cur;
			if (len < UPNP_XML_NAME_SIZE - 1 && !netCtlUpnpXmlIsSpace(c) && c != '/' &&
			    c != '<' && c != '>') {
				parser->name[len] = c;
				parser->len = len + 1;
				cur++;
				pos = (const char *)cur - data;
				break;
			}
			if (c == '/')
				parser->closing = 1;
			parser->name[len] = '\0';

			if (state == UPNP_XML_STATE_TAG) {
				/* start tag: track it when the schema lists it */
				matched = 0;
				if (parser->schema->depth > parser->depth) {
					list = parser->schema->levels[parser->depth];
					for (index = 0; sceClibStrnlen(list[index].name,
								       UPNP_XML_NAME_SIZE) != 0;
					     index++)
						if (netCtlUpnpXmlMatch(parser, &list[index]))
							break;
					if (sceClibStrnlen(list[index].name, UPNP_XML_NAME_SIZE) != 0) {
						parser->index[parser->depth] = index;
						ret = parser->callback(parser, UPNP_XML_EVENT_START,
								       parser->depth, index,
								       parser->name, NULL,
								       parser->arg);
						matched = 1;
					}
				}
				if (ret < 0)
					return ret;
				state = matched ? UPNP_XML_STATE_IN_TAG : UPNP_XML_STATE_TEXT;
				break;
			}
			if (state == UPNP_XML_STATE_END_TAG) {
				matched = 0;
				depth = parser->depth;
				if (depth != 0 && parser->schema->depth >= depth) {
					index = parser->index[depth - 1];
					element = &parser->schema->levels[depth - 1][index];
					if (parser->closing == 1)
						matched = 1;
					if (netCtlUpnpXmlMatch(parser, element))
						matched = 1;
					if (matched) {
						if (element->hasText == 1)
							ret = parser->callback(parser,
									       UPNP_XML_EVENT_TEXT,
									       parser->depth - 1,
									       index, parser->value,
									       NULL, parser->arg);
						else
							ret = parser->callback(parser,
									       UPNP_XML_EVENT_END,
									       parser->depth - 1,
									       index, NULL, NULL,
									       parser->arg);
					}
				}
				if (ret < 0)
					return ret;
				if (matched == 1)
					parser->depth--;
				state = UPNP_XML_STATE_TEXT;
			}
			cur++;
			pos = (const char *)cur - data;
			break;

		case UPNP_XML_STATE_IN_TAG:
			c = *cur;
			if (c == '>') {
				depth = parser->depth;
				parser->depth = depth + 1;
				if (cur[-1] == '/')
					parser->closing = 1;
				if (parser->closing == 1) {
					/* <name/>: report the end at once */
					state = UPNP_XML_STATE_END_TAG;
					parser->flags = 0;
					break;
				}
				if (parser->schema->levels[depth][parser->index[depth]].hasText == 1) {
					state = UPNP_XML_STATE_CONTENT;
					parser->len = 0;
					sceClibMemset(parser->value, 0, sizeof(parser->value));
					parser->pendingLen = 0;
					parser->pendingNeed = 0;
					sceClibMemset(parser->pending, 0, sizeof(parser->pending));
				} else {
					state = UPNP_XML_STATE_TEXT;
				}
				cur++;
			} else if (c == '/' || netCtlUpnpXmlIsSpace(c)) {
				cur++;
			} else {
				parser->flags = 0;
				parser->len = 0;
				sceClibMemset(parser->name, 0, sizeof(parser->name));
				sceClibMemset(parser->value, 0, sizeof(parser->value));
				state = UPNP_XML_STATE_ATTR_NAME;
				break;
			}
			pos = (const char *)cur - data;
			break;

		case UPNP_XML_STATE_ATTR_NAME:
			len = parser->len;
			if (len < UPNP_XML_NAME_SIZE - 1) {
				c = *cur;
				if (parser->flags != 1) {
					if (!netCtlUpnpXmlIsSpace(c) && c != '/' && c != '=' &&
					    c != '>') {
						parser->name[len] = c;
						parser->len = len + 1;
						cur++;
						pos = (const char *)cur - data;
						break;
					}
					parser->name[len] = '\0';
					parser->flags = 1;
					c = *cur;
				}
				if (c == '=') {
					cur++;
					pos = (const char *)cur - data;
					state = UPNP_XML_STATE_ATTR_VALUE;
					parser->flags = 0;
					parser->len = 0;
					break;
				}
				if (c == '/' || netCtlUpnpXmlIsSpace(c)) {
					if (c == '/')
						parser->closing = 1;
					cur++;
					pos = (const char *)cur - data;
					break;
				}
			}
			/* attribute without value */
			ret = parser->callback(parser, UPNP_XML_EVENT_ATTRIBUTE, parser->depth,
					       parser->index[parser->depth], parser->name,
					       parser->value, parser->arg);
			if (ret < 0)
				return ret;
			state = UPNP_XML_STATE_IN_TAG;
			break;

		case UPNP_XML_STATE_ATTR_VALUE:
			len = parser->len;
			if (len < UPNP_XML_VALUE_SIZE - 1) {
				c = *cur;
				if ((parser->flags & UPNP_XML_VALUE_QUOTED) && c != '\'' && c != '"') {
					parser->value[len] = c;
					parser->len = len + 1;
					cur++;
					pos = (const char *)cur - data;
					break;
				}
				switch (c) {
				case ' ':
				case '\t':
				case '\n':
				case '\r':
					if ((parser->flags & UPNP_XML_VALUE_STARTED) == 0) {
						cur++;
						pos = (const char *)cur - data;
						continue;
					}
					break;
				case '/':
					break;
				case '>':
					goto value_end;
				case '"':
				case '\'':
					cur++;
					if (parser->flags & UPNP_XML_VALUE_QUOTED)
						parser->flags &= ~UPNP_XML_VALUE_QUOTED;
					else
						parser->flags |= UPNP_XML_VALUE_STARTED |
								 UPNP_XML_VALUE_QUOTED;
					pos = (const char *)cur - data;
					continue;
				default:
					parser->flags |= UPNP_XML_VALUE_STARTED;
					parser->value[len] = *cur++;
					parser->len = len + 1;
					pos = (const char *)cur - data;
					continue;
				}
				if (c == '/')
					parser->closing = 1;
			}
value_end:
			parser->value[len] = '\0';
			ret = parser->callback(parser, UPNP_XML_EVENT_ATTRIBUTE, parser->depth,
					       parser->index[parser->depth], parser->name,
					       parser->value, parser->arg);
			if (ret < 0)
				return ret;
			state = UPNP_XML_STATE_IN_TAG;
			break;

		case UPNP_XML_STATE_CONTENT:
			n = size - pos;
			need = parser->pendingNeed;
			if (need != 0) {
				/* rest of a character split between two calls */
				if (need <= n)
					n = need;
				sceClibMemcpy(parser->pending + parser->pendingLen, cur, n);
				parser->pendingLen += n;
				parser->pendingNeed -= n;
				if (parser->pendingNeed != 0) {
					cur += n;
					pos = (const char *)cur - data;
					break;
				}
				ret = netCtlUpnpXmlDecodeChar((char *)decoded, sizeof(decoded),
							      (const unsigned char *)parser->pending,
							      parser->pendingLen, &outLen, &consumed);
				if (ret == UPNP_XML_ERROR_NO_SPACE) {
					parser->pendingNeed = consumed - parser->pendingLen;
					cur += n;
					pos = (const char *)cur - data;
					break;
				}
				if (ret < 0) {
					outLen = consumed = 1;
					sceClibMemcpy(decoded, parser->pending, 1);
				} else {
					parser->pendingLen = 0;
					parser->pendingNeed = 0;
				}
			} else {
				if (n >= 6)
					n = 6;
				ret = netCtlUpnpXmlDecodeChar((char *)decoded, sizeof(decoded), cur, n,
							      &outLen, &consumed);
				if (ret == UPNP_XML_ERROR_NO_SPACE) {
					sceClibMemcpy(parser->pending, cur, n);
					parser->pendingLen = n;
					parser->pendingNeed = consumed - n;
					cur += n;
					pos = (const char *)cur - data;
					break;
				}
				if (ret < 0) {
					outLen = consumed = 1;
					sceClibMemcpy(decoded, cur, 1);
					n = 1;
				} else {
					n = consumed;
				}
			}
			len = parser->len;
			if (len + outLen >= UPNP_XML_VALUE_SIZE ||
			    (outLen == 1 && consumed == 1 && decoded[0] == '<')) {
				/* end of the text: a tag starts */
				parser->value[len] = '\0';
				parser->len = 0;
				sceClibMemset(parser->name, 0, sizeof(parser->name));
				cur++;
				parser->closing = 0;
				parser->flags = 0;
				state = UPNP_XML_STATE_TAG;
				pos = (const char *)cur - data;
				break;
			}
			sceClibMemcpy(parser->value + len, decoded, outLen);
			cur += n;
			pos = (const char *)cur - data;
			parser->len += outLen;
			break;

		default:
			goto end;
		}
	}
	ret = 0;
end:
	parser->state = state;
	return ret;
}

/* Copies text, decoding &lt; &gt; &amp; &apos; &quot;; with out NULL only counts. */
__attribute__((noinline))
int netCtlUpnpXmlUnescape(char *out, SceSize outSize, const char *in, SceSize inLen,
			  SceSize *outLen)
{
	const char *end;
	SceSize count = 0;
	int write = 1;
	int ret;

	if (in == NULL || inLen == 0)
		return UPNP_XML_ERROR_INVALID;
	if (out == NULL) {
		if (outLen == NULL)
			return UPNP_XML_ERROR_INVALID;
		write = 0;
	}
	end = in + inLen;
	ret = 0;
	while (in < end) {
		count++;
		if (count > outSize && write)
			return UPNP_XML_ERROR_NO_SPACE;
		if (*in == '&') {
			if (in[1] == 'l' && in[2] == 't' && in[3] == ';') {
				if (write)
					*out++ = '<';
				in += 4;
			} else if (in[1] == 'g' && in[2] == 't' && in[3] == ';') {
				if (write)
					*out++ = '>';
				in += 4;
			} else if (in[1] == 'a' && in[2] == 'm' && in[3] == 'p' && in[4] == ';') {
				if (write)
					*out++ = '&';
				in += 5;
			} else if (in[1] == 'a' && in[2] == 'p' && in[3] == 'o' && in[4] == 's' &&
				   in[5] == ';') {
				if (write)
					*out++ = '\'';
				in += 6;
			} else if (in[1] == 'q' && in[2] == 'u' && in[3] == 'o' && in[4] == 't' &&
				   in[5] == ';') {
				if (write)
					*out++ = '"';
				in += 6;
			} else {
				if (write)
					*out++ = '&';
				in++;
			}
		} else {
			if (write)
				*out++ = *in;
			in++;
		}
	}
	if (count + 1 > outSize) {
		if (write)
			return UPNP_XML_ERROR_NO_SPACE;
	} else if (write) {
		*out = '\0';
	}
	if (outLen != NULL)
		*outLen = count + 1;
	return ret;
}

static inline __attribute__((always_inline)) int netCtlUpnpXmlIsTrail(unsigned char c)
{
	return (c & 0xc0) == 0x80;
}

/*
 * Decodes the first character of the input: one entity or one UTF-8
 * sequence.  UPNP_XML_ERROR_NO_SPACE with *consumed set when the input is
 * too short for it.
 */
__attribute__((noinline))
int netCtlUpnpXmlDecodeChar(char *out, SceSize outSize, const unsigned char *in, SceSize inLen,
			    SceSize *outLen, SceSize *consumed)
{
	char decoded[8];
	SceSize len = 0;
	SceSize used = 0;
	int write = 1;
	int ret;
	unsigned char c;

	if (in == NULL || inLen == 0)
		return UPNP_XML_ERROR_INVALID;
	if (out == NULL) {
		if (outLen == NULL)
			return UPNP_XML_ERROR_INVALID;
		write = 0;
	}
	c = in[0];
	if ((c & 0x80) == 0) {
		len = 1;
		if (c != '&') {
			used = 1;
		} else if (inLen < 4) {
			ret = UPNP_XML_ERROR_NO_SPACE;
			used = 4;
			goto out;
		} else if (in[1] == 'l' && in[2] == 't' && in[3] == ';') {
			used = 4;
		} else if (in[1] == 'g' && in[2] == 't' && in[3] == ';') {
			used = 4;
		} else if (inLen < 5) {
			ret = UPNP_XML_ERROR_NO_SPACE;
			used = 5;
			goto out;
		} else if (in[1] == 'a' && in[2] == 'm' && in[3] == 'p' && in[4] == ';') {
			used = 5;
		} else {
			used = 6;
			if (inLen < 6) {
				ret = UPNP_XML_ERROR_NO_SPACE;
				goto out;
			}
			if (!(in[1] == 'a' && in[2] == 'p' && in[3] == 'o' && in[4] == 's' &&
			      in[5] == ';') &&
			    !(in[1] == 'q' && in[2] == 'u' && in[3] == 'o' && in[4] == 't' &&
			      in[5] == ';'))
				used = 1;
		}
	} else {
		if ((c & 0xe0) == 0xc0 && c >= 0xc2) {
			if (inLen < 2) {
				ret = UPNP_XML_ERROR_NO_SPACE;
				used = 2;
				goto out;
			}
			if (netCtlUpnpXmlIsTrail(in[1])) {
				used = len = 2;
				goto decode;
			}
		}
		if ((c & 0xf0) == 0xe0) {
			if (inLen < 3) {
				ret = UPNP_XML_ERROR_NO_SPACE;
				used = 3;
				goto out;
			}
			if (netCtlUpnpXmlIsTrail(in[1]) && netCtlUpnpXmlIsTrail(in[2])) {
				used = len = 3;
				goto decode;
			}
		}
		if ((c & 0xf8) == 0xf0) {
			if (inLen < 4) {
				ret = UPNP_XML_ERROR_NO_SPACE;
				used = 4;
				goto out;
			}
			/* Sony quirk: an invalid 4-byte sequence consumes nothing */
			if (netCtlUpnpXmlIsTrail(in[1]) && netCtlUpnpXmlIsTrail(in[2]) &&
			    netCtlUpnpXmlIsTrail(in[3]))
				used = len = 4;
		} else {
			used = len = 1;
		}
	}
decode:
	if (len > outSize && write) {
		ret = UPNP_XML_ERROR_NO_SPACE;
		goto out;
	}
	ret = netCtlUpnpXmlUnescape(decoded, 7, (const char *)in, inLen, NULL);
	if (ret < 0)
		return ret;
	if (write)
		sceClibMemcpy(out, decoded, len);
out:
	if (outLen != NULL)
		*outLen = len;
	if (consumed != NULL)
		*consumed = used;
	return ret;
}
