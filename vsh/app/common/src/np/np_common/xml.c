/*
 * SceNpCommon: XML text. Escaping and unescaping of the five predefined entities, the first
 * character of a UTF-8 or escaped string, and a streaming parser that reports the elements,
 * attributes and text of a schema to a callback.
 */

#include "np_common_internal.h"

/* 5: a control character, 6: a C1 or other character XML does not allow; 0: allowed */
static const SceUInt8 s_xmlInvalidCharacter[256] = {
	5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 5, 5, 0, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6,
	6, 6, 6, 6, 6, 0, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/* copies size bytes with a terminator */
static inline int npCopyBytes(char *dst, SceSize dstSize, const char *src, SceSize size)
{
	const char *end;
	SceSize length = 0;

	if (dst == NULL || src == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	for (end = src + size; src < end; src++) {
		if (++length > dstSize)
			return SCE_NP_UTIL_ERROR_INSUFFICIENT;
		*dst++ = *src;
	}
	if (length + 1 > dstSize)
		return SCE_NP_UTIL_ERROR_INSUFFICIENT;
	*dst = '\0';
	return 0;
}

/*
 * The length of the UTF-8 sequence at s that starts with lead, 1 for a byte that starts none, 0 for a broken
 * four-byte one; SCE_NP_UTIL_ERROR_INSUFFICIENT when size cuts the sequence.
 */
static inline int npUtf8CharSize(char lead, const char *s, SceSize size, SceSize *length)
{
	if ((lead & 0xe0) == 0xc0 && (SceUInt8)lead >= 0xc2) {
		if (size < 2) {
			*length = 2;
			return SCE_NP_UTIL_ERROR_INSUFFICIENT;
		}
		if ((s[1] & 0xc0) == 0x80) {
			*length = 2;
			return 0;
		}
	}
	if ((lead & 0xf0) == 0xe0) {
		if (size < 3) {
			*length = 3;
			return SCE_NP_UTIL_ERROR_INSUFFICIENT;
		}
		if ((s[1] & 0xc0) == 0x80 && (s[2] & 0xc0) == 0x80) {
			*length = 3;
			return 0;
		}
	}
	if ((lead & 0xf8) == 0xf0) {
		if (size < 4) {
			*length = 4;
			return SCE_NP_UTIL_ERROR_INSUFFICIENT;
		}
		if ((s[1] & 0xc0) == 0x80 && (s[2] & 0xc0) == 0x80 && (s[3] & 0xc0) == 0x80)
			*length = 4;
		return 0;
	}
	*length = 1;
	return 0;
}

/*
 * The first UTF-8 character of src: *size gets its length, *used the bytes it takes (0 for a
 * broken four-byte sequence). Without dst it only measures.
 */
int SceNpCommon_13293ADC(char *dst, SceSize dstSize, const char *src, SceSize srcSize,
			 SceSize *size, SceSize *used)
{
	char buf[8];
	char lead;
	SceSize length = 0;
	SceSize copied = 0;
	int write = 1;
	int ret;

	if (src == NULL || srcSize == 0)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if (dst == NULL) {
		if (size == NULL)
			return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
		write = 0;
	}
	lead = src[0];
	if ((lead & 0x80) == 0) {
		length = 1;
	} else {
		ret = npUtf8CharSize(lead, src, srcSize, &length);
		if (ret < 0)
			goto out;
	}
	if (length > dstSize && write) {
		ret = SCE_NP_UTIL_ERROR_INSUFFICIENT;
		copied = length;
		goto out;
	}
	ret = npCopyBytes(buf, 7, src, srcSize);
	if (ret < 0)
		return ret;
	copied = length;
	if (write)
		sceClibMemcpy_safe(dst, buf, length);
out:
	if (size != NULL)
		*size = copied;
	if (used != NULL)
		*used = length;
	return ret;
}

/*
 * Escapes UTF-8 text for XML. Without dst it only measures; *required gets the size with the
 * terminator.
 */
int SceNpCommon_C2BF5C1F(char *dst, SceSize dstSize, const SceUInt8 *src, SceSize srcSize,
			 SceSize *required)
{
	const SceUInt8 *end;
	SceSize length = 0;
	int write = 1;
	int c;

	if (src == NULL || srcSize == 0)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if (dst == NULL) {
		if (required == NULL)
			return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
		write = 0;
	}
	end = src + srcSize;
	while (src < end) {
		c = *src;
		if ((c & 0x80) != 0) {
			if ((c & 0xe0) == 0xc0 && c >= 0xc2 && (src[1] & 0xc0) == 0x80) {
				length += 2;
				if (length <= dstSize) {
					if (write) {
						dst[0] = c;
						dst[1] = src[1];
						dst += 2;
					}
				} else if (write) {
					return SCE_NP_UTIL_ERROR_INSUFFICIENT;
				}
				src += 2;
			} else if ((c & 0xf0) == 0xe0 && (src[1] & 0xc0) == 0x80 &&
				   (src[2] & 0xc0) == 0x80) {
				length += 3;
				if (length <= dstSize) {
					if (write) {
						dst[0] = c;
						dst[1] = src[1];
						dst[2] = src[2];
						dst += 3;
					}
				} else if (write) {
					return SCE_NP_UTIL_ERROR_INSUFFICIENT;
				}
				src += 3;
			} else if ((c & 0xf8) == 0xf0 && (src[1] & 0xc0) == 0x80 &&
				   (src[2] & 0xc0) == 0x80 && (src[3] & 0xc0) == 0x80) {
				length += 4;
				if (length <= dstSize) {
					if (write) {
						dst[0] = c;
						dst[1] = src[1];
						dst[2] = src[2];
						dst[3] = src[3];
						dst += 4;
					}
				} else if (write) {
					return SCE_NP_UTIL_ERROR_INSUFFICIENT;
				}
				src += 4;
			} else {
				return SCE_NP_UTIL_ERROR_INVALID_CHARACTER;
			}
			continue;
		}
		if (s_xmlInvalidCharacter[c] != 0)
			return SCE_NP_UTIL_ERROR_INVALID_CHARACTER;
		if (c == '<') {
			length += 4;
			if (length <= dstSize) {
				if (write) {
					dst[0] = '&';
					dst[1] = 'l';
					dst[2] = 't';
					dst[3] = ';';
					dst += 4;
				}
			} else if (write) {
				return SCE_NP_UTIL_ERROR_INSUFFICIENT;
			}
		} else if (c == '>') {
			length += 4;
			if (length <= dstSize) {
				if (write) {
					dst[0] = '&';
					dst[1] = 'g';
					dst[2] = 't';
					dst[3] = ';';
					dst += 4;
				}
			} else if (write) {
				return SCE_NP_UTIL_ERROR_INSUFFICIENT;
			}
		} else if (c == '&') {
			length += 5;
			if (length <= dstSize) {
				if (write) {
					dst[0] = '&';
					dst[1] = 'a';
					dst[2] = 'm';
					dst[3] = 'p';
					dst[4] = ';';
					dst += 5;
				}
			} else if (write) {
				return SCE_NP_UTIL_ERROR_INSUFFICIENT;
			}
		} else if (c == '\'') {
			length += 6;
			if (length <= dstSize) {
				if (write) {
					dst[0] = '&';
					dst[1] = 'a';
					dst[2] = 'p';
					dst[3] = 'o';
					dst[4] = 's';
					dst[5] = ';';
					dst += 6;
				}
			} else if (write) {
				return SCE_NP_UTIL_ERROR_INSUFFICIENT;
			}
		} else if (c == '"') {
			length += 6;
			if (length <= dstSize) {
				if (write) {
					dst[0] = '&';
					dst[1] = 'q';
					dst[2] = 'u';
					dst[3] = 'o';
					dst[4] = 't';
					dst[5] = ';';
					dst += 6;
				}
			} else if (write) {
				return SCE_NP_UTIL_ERROR_INSUFFICIENT;
			}
		} else if (++length <= dstSize) {
			if (write)
				*dst++ = c;
		} else if (write) {
			return SCE_NP_UTIL_ERROR_INSUFFICIENT;
		}
		src++;
	}
	length++;
	if (length <= dstSize) {
		if (write)
			*dst = '\0';
	} else if (write) {
		return SCE_NP_UTIL_ERROR_INSUFFICIENT;
	}
	if (required != NULL)
		*required = length;
	return 0;
}

/*
 * Replaces the five entities of XML text by their characters; any other '&' is kept. Without
 * dst it only measures; *required gets the size with the terminator.
 */
int SceNpCommon_389F8C14(char *dst, SceSize dstSize, const SceUInt8 *src, SceSize srcSize,
			 SceSize *required)
{
	const SceUInt8 *end;
	SceSize length = 0;
	int write = 1;

	if (src == NULL || srcSize == 0)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if (dst == NULL) {
		if (required == NULL)
			return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
		write = 0;
	}
	for (end = src + srcSize; src < end; ) {
		if (++length > dstSize && write)
			return SCE_NP_UTIL_ERROR_INSUFFICIENT;
		if (src[0] != '&') {
			if (write)
				*dst++ = src[0];
			src++;
		} else if (src[1] == 'l' && src[2] == 't' && src[3] == ';') {
			if (write)
				*dst++ = '<';
			src += 4;
		} else if (src[1] == 'g' && src[2] == 't' && src[3] == ';') {
			if (write)
				*dst++ = '>';
			src += 4;
		} else if (src[1] == 'a' && src[2] == 'm' && src[3] == 'p' && src[4] == ';') {
			if (write)
				*dst++ = '&';
			src += 5;
		} else if (src[1] == 'a' && src[2] == 'p' && src[3] == 'o' && src[4] == 's' &&
			   src[5] == ';') {
			if (write)
				*dst++ = '\'';
			src += 6;
		} else if (src[1] == 'q' && src[2] == 'u' && src[3] == 'o' && src[4] == 't' &&
			   src[5] == ';') {
			if (write)
				*dst++ = '"';
			src += 6;
		} else {
			if (write)
				*dst++ = '&';
			src++;
		}
	}
	length++;
	if (length <= dstSize) {
		if (write)
			*dst = '\0';
	} else if (write) {
		return SCE_NP_UTIL_ERROR_INSUFFICIENT;
	}
	if (required != NULL)
		*required = length;
	return 0;
}

/*
 * The first character of escaped XML text: *size gets the length of the character it stands
 * for, *used the bytes it takes (an entity, or a UTF-8 sequence). Without dst it only measures.
 */
int SceNpCommon_38B54F30(char *dst, SceSize dstSize, const char *src, SceSize srcSize,
			 SceSize *size, SceSize *used)
{
	char buf[8];
	char lead;
	SceSize length = 0;
	SceSize consumed = 0;
	int write = 1;
	int ret;

	if (src == NULL || srcSize == 0)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if (dst == NULL) {
		if (size == NULL)
			return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
		write = 0;
	}
	lead = src[0];
	if ((lead & 0x80) == 0) {
		length = 1;
		if (lead != '&') {
			consumed = 1;
		} else if (srcSize < 4) {
			consumed = 4;
			ret = SCE_NP_UTIL_ERROR_INSUFFICIENT;
			goto out;
		} else if ((src[1] == 'l' && src[2] == 't' && src[3] == ';') ||
			   (src[1] == 'g' && src[2] == 't' && src[3] == ';')) {
			consumed = 4;
		} else if (srcSize < 5) {
			consumed = 5;
			ret = SCE_NP_UTIL_ERROR_INSUFFICIENT;
			goto out;
		} else if (src[1] == 'a' && src[2] == 'm' && src[3] == 'p' && src[4] == ';') {
			consumed = 5;
		} else {
			consumed = 6;
			if (srcSize < 6) {
				ret = SCE_NP_UTIL_ERROR_INSUFFICIENT;
				goto out;
			}
			if (!(src[1] == 'a' && src[2] == 'p' && src[3] == 'o' && src[4] == 's' &&
			      src[5] == ';') &&
			    !(src[1] == 'q' && src[2] == 'u' && src[3] == 'o' && src[4] == 't' &&
			      src[5] == ';'))
				consumed = 1;
		}
	} else {
		ret = npUtf8CharSize(lead, src, srcSize, &consumed);
		if (ret < 0)
			goto out;
		length = consumed;
	}

	if (length > dstSize && write) {
		ret = SCE_NP_UTIL_ERROR_INSUFFICIENT;
		goto out;
	}
	ret = SceNpCommon_389F8C14(buf, 7, (const SceUInt8 *)src, srcSize, NULL);
	if (ret < 0)
		return ret;
	if (write)
		sceClibMemcpy_safe(dst, buf, length);
out:
	if (size != NULL)
		*size = length;
	if (used != NULL)
		*used = consumed;
	return ret;
}

int SceNpCommon_650D111B(NpXmlParser *parser, const NpXmlSchema *schema,
			 NpXmlCallback callback, void *arg)
{
	sceClibMemset(parser, 0, sizeof(*parser));
	parser->schema = schema;
	parser->callback = callback;
	parser->arg = arg;
	return 0;
}

/*
 * Parses the next size bytes of an XML document; the parser keeps its state between calls.
 * The callback gets each element of the schema as it starts and ends (with its text, for an
 * element that has text) and each attribute.
 */
int SceNpCommon_88B18A5B(NpXmlParser *parser, const char *data, int size)
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
		case NP_XML_STATE_TEXT_OUTSIDE:
			if (*p == '<') {
				parser->length = 0;
				parser->closed = 0;
				parser->flags = 0;
				sceClibMemset(parser->name, 0, sizeof(parser->name));
				sceClibMemset(parser->value, 0, sizeof(parser->value));
				state = NP_XML_STATE_TAG;
			}
			p++;
			continue;

		case NP_XML_STATE_TEXT:
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
				ret = SceNpCommon_38B54F30(ch, 6, parser->pending,
							   parser->pendingLength, &charSize,
							   &charUsed);
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
				ret = SceNpCommon_38B54F30(ch, 6, p, n, &charSize,
							   &charUsed);
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
			state = NP_XML_STATE_TAG;
			continue;

		case NP_XML_STATE_TAG:
			if (*p == '/' && parser->length == 0) {
				p++;
				state = NP_XML_STATE_END_TAG;
				if ((int)(p - data) >= size)
					goto out;
			}
			/* fall through */
		case NP_XML_STATE_END_TAG:
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
			if (state == NP_XML_STATE_END_TAG) {
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
							ret = parser->callback(parser,
									       NP_XML_EVENT_TEXT,
									       parser->depth - 1, index,
									       parser->value,
									       parser->value,
									       parser->arg);
						else
							ret = parser->callback(parser,
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
				state = NP_XML_STATE_TEXT_OUTSIDE;
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
					ret = parser->callback(parser, NP_XML_EVENT_START, parser->depth,
							       index, parser->name, NULL, parser->arg);
					called = 1;
				}
			}
			if (ret < 0)
				return ret;
			if (called == 0) {
				state = NP_XML_STATE_TEXT_OUTSIDE;
				continue;
			}
			state = NP_XML_STATE_ATTRIBUTES;
			continue;

		case NP_XML_STATE_ATTRIBUTES:
			c = *(const SceUInt8 *)p;
			if (c == '>') {
				depth = parser->depth++;
				if (p[-1] == '/')
					parser->closed = 1;
				if (parser->closed == 1) {
					state = NP_XML_STATE_END_TAG;
					parser->flags = 0;
					continue;
				}
				if (parser->schema->levels[depth][parser->index[depth]].hasText == 1) {
					state = NP_XML_STATE_TEXT;
					parser->length = 0;
					sceClibMemset(parser->value, 0, sizeof(parser->value));
					parser->pendingLength = 0;
					parser->pendingNeeded = 0;
					sceClibMemset(parser->pending, 0, 6);
				} else {
					state = NP_XML_STATE_TEXT_OUTSIDE;
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
			state = NP_XML_STATE_ATTRIBUTE_NAME;
			continue;

		case NP_XML_STATE_ATTRIBUTE_NAME:
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
				state = NP_XML_STATE_ATTRIBUTE_VALUE;
				parser->flags = 0;
				parser->length = 0;
				continue;
			default:
				goto attribute;
			}

		case NP_XML_STATE_ATTRIBUTE_VALUE:
			length = parser->length;
			if (length < sizeof(parser->value) - 1) {
				c = *(const SceUInt8 *)p;
				if ((parser->flags & NP_XML_FLAG_QUOTED) != 0 && c != '\'' && c != '"') {
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
					if ((parser->flags & NP_XML_FLAG_STARTED) == 0) {
						p++;
						continue;
					}
					break;
				case '"':
				case '\'':
					if ((parser->flags & NP_XML_FLAG_QUOTED) != 0)
						parser->flags &= ~NP_XML_FLAG_QUOTED;
					else
						parser->flags |= NP_XML_FLAG_STARTED | NP_XML_FLAG_QUOTED;
					p++;
					continue;
				case '/':
				case '>':
					break;
				default:
					parser->flags |= NP_XML_FLAG_STARTED;
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
			ret = parser->callback(parser, NP_XML_EVENT_ATTRIBUTE, parser->depth,
					       parser->index[parser->depth], parser->name,
					       parser->value, parser->arg);
			if (ret < 0)
				return ret;
			state = NP_XML_STATE_ATTRIBUTES;
			continue;

		default:
			goto out;
		}
	}
out:
	parser->state = state;
	return 0;
}
