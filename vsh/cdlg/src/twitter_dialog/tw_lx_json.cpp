/*
 * SceTwitterDialogPlugin: recursive-descent JSON parser into LxVariant.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string.h>
#include "tw_lx_json.h"

static const LxVariant s_nullVariant;

const LxVariant &LxVariantGetMember(const LxVariant &obj, const LxString &key)
{
	if (obj.m_type != LxVariant::TYPE_MAP || obj.m_value.mapVal == NULL)
		return s_nullVariant;
	paf::map<LxString, LxVariant> *mapObj = obj.m_value.mapVal;
	paf::map<LxString, LxVariant>::iterator it = mapObj->find(key);

	if (it == mapObj->end())
		return s_nullVariant;
	return it->second;
}

LxVariant &LxVariantEnsureMember(LxVariant &obj, const LxString &key)
{
	obj.SetType(LxVariant::TYPE_MAP);
	return (*obj.m_value.mapVal)[key];
}

static int StreamNextChar(LxJsonStream &stream)
{
	if (stream.hasPushedBack) {
		stream.hasPushedBack = false;
		return stream.currentChar;
	}
	stream.currentChar = -1;
	stream.lastError = stream.readChar(&stream.currentChar, stream.ctx);
	return stream.currentChar;
}

static void StreamPushBack(LxJsonStream &stream)
{
	stream.hasPushedBack = true;
}

static int StreamSkipWhitespace(LxJsonStream &stream)
{
	int ch;

	do {
		ch = StreamNextChar(stream);
	} while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
	return ch;
}

static bool AppendByte(LxBuffer &buf, SceSize &len, uint8_t byteVal)
{
	SceSize cap = buf.Capacity();

	if (len >= cap) {
		SceSize nextCap = (cap > 0) ? (cap * 2) : 64;

		if (!buf.Grow(nextCap))
			return false;
	}
	buf.begin[len] = byteVal;
	len++;
	return true;
}

static int ParseHex4(LxJsonStream &stream)
{
	int value = 0;

	for (unsigned int i = 0; i < 4; ++i) {
		int ch = StreamNextChar(stream);

		if (ch == -1)
			return -1;
		int digit;

		if (ch >= '0' && ch <= '9') {
			digit = ch - '0';
		} else if (ch >= 'A' && ch <= 'F') {
			digit = ch - 'A' + 10;
		} else if (ch >= 'a' && ch <= 'f') {
			digit = ch - 'a' + 10;
		} else {
			StreamPushBack(stream);
			return -1;
		}
		value = (value << 4) | digit;
	}
	return value;
}

static bool ParseUnicodeEscape(LxBuffer &buf, SceSize &len, LxJsonStream &stream)
{
	int codePoint = ParseHex4(stream);

	if (codePoint == -1)
		return false;
	if (codePoint >= 0xD800 && codePoint <= 0xDFFF) {
		if (codePoint >= 0xDC00)
			return false;
		if (StreamNextChar(stream) != '\\' || StreamNextChar(stream) != 'u') {
			StreamPushBack(stream);
			return false;
		}
		int lowSurrogate = ParseHex4(stream);

		if (lowSurrogate < 0xDC00 || lowSurrogate > 0xDFFF)
			return false;
		codePoint = (((codePoint & 0x3FF) << 10) | (lowSurrogate & 0x3FF)) + 0x10000;
	}

	if (codePoint < 0x80) {
		AppendByte(buf, len, (uint8_t)codePoint);
	} else if (codePoint < 0x800) {
		AppendByte(buf, len, (uint8_t)(0xC0 | ((codePoint >> 6) & 0x1F)));
		AppendByte(buf, len, (uint8_t)(0x80 | (codePoint & 0x3F)));
	} else if (codePoint < 0x10000) {
		AppendByte(buf, len, (uint8_t)(0xE0 | ((codePoint >> 12) & 0x0F)));
		AppendByte(buf, len, (uint8_t)(0x80 | ((codePoint >> 6) & 0x3F)));
		AppendByte(buf, len, (uint8_t)(0x80 | (codePoint & 0x3F)));
	} else {
		AppendByte(buf, len, (uint8_t)(0xF0 | ((codePoint >> 18) & 0x07)));
		AppendByte(buf, len, (uint8_t)(0x80 | ((codePoint >> 12) & 0x3F)));
		AppendByte(buf, len, (uint8_t)(0x80 | ((codePoint >> 6) & 0x3F)));
		AppendByte(buf, len, (uint8_t)(0x80 | (codePoint & 0x3F)));
	}
	return true;
}

static bool ParseNumber(LxVariant &outValue, LxJsonStream &stream)
{
	stream.numberLen = 0;
	bool isFloat = false;

	for (;;) {
		int ch = StreamNextChar(stream);

		if (ch >= '0' && ch <= '9') {
			AppendByte(stream.numberBuf, stream.numberLen, (uint8_t)ch);
		} else if (ch == '-') {
			if (stream.numberLen != 0)
				isFloat = true;
			AppendByte(stream.numberBuf, stream.numberLen, '-');
		} else if (ch == '+' || ch == '.' || ch == 'e' || ch == 'E') {
			if (stream.numberLen != 0)
				isFloat = true;
			AppendByte(stream.numberBuf, stream.numberLen, (uint8_t)ch);
		} else {
			StreamPushBack(stream);
			break;
		}
	}

	const char *numStr = "";

	if (AppendByte(stream.numberBuf, stream.numberLen, '\0'))
		numStr = (const char *)stream.numberBuf.begin;

	if (!isFloat) {
		bool isNeg = (numStr[0] == '-');
		const char *cursor = isNeg ? (numStr + 1) : numStr;
		uint64_t accum = 0;
		bool overflow = false;

		while (*cursor != '\0') {
			uint64_t nextVal = accum * 10ULL + (uint64_t)(*cursor - '0');

			if (nextVal < accum) {
				overflow = true;
				break;
			}
			accum = nextVal;
			cursor++;
		}
		if (!overflow) {
			if (!isNeg) {
				outValue.SetType(LxVariant::TYPE_UINT64);
				outValue.m_value.uintVal = accum;
				return true;
			}
			if ((accum & 0x8000000000000000ULL) == 0ULL) {
				outValue.SetType(LxVariant::TYPE_INT64);
				outValue.m_value.intVal = -(int64_t)accum;
				return true;
			}
		}
	}

	double dblVal = sce_paf_atof(numStr);

	outValue.SetType(LxVariant::TYPE_DOUBLE);
	outValue.m_value.doubleVal = dblVal;
	return true;
}

static bool ParseStringToken(LxString &outStr, LxJsonStream &stream)
{
	stream.stringLen = 0;

	for (;;) {
		int ch = StreamNextChar(stream);

		if (ch < 32) {
			StreamPushBack(stream);
			return false;
		}
		if (ch == '"') {
			outStr.Assign((const char *)stream.stringBuf.begin, stream.stringLen);
			return true;
		}
		if (ch != '\\') {
			AppendByte(stream.stringBuf, stream.stringLen, (uint8_t)ch);
			continue;
		}
		int esc = StreamNextChar(stream);

		switch (esc) {
		case '"':
			AppendByte(stream.stringBuf, stream.stringLen, '"');
			break;
		case '\\':
			AppendByte(stream.stringBuf, stream.stringLen, '\\');
			break;
		case '/':
			AppendByte(stream.stringBuf, stream.stringLen, '/');
			break;
		case 'b':
			AppendByte(stream.stringBuf, stream.stringLen, '\b');
			break;
		case 'f':
			AppendByte(stream.stringBuf, stream.stringLen, '\f');
			break;
		case 'n':
			AppendByte(stream.stringBuf, stream.stringLen, ' ');
			break;
		case 'r':
			AppendByte(stream.stringBuf, stream.stringLen, '\r');
			break;
		case 't':
			AppendByte(stream.stringBuf, stream.stringLen, '\t');
			break;
		case 'u':
			if (!ParseUnicodeEscape(stream.stringBuf, stream.stringLen, stream))
				return false;
			break;
		default:
			return false;
		}
	}
}

static void PushKeyScope(LxJsonStream &stream)
{
	if (stream.depth < 16) {
		stream.keyStack[stream.depth] = stream.currentKey;
		stream.currentKey = NULL;
	}
	stream.depth++;
}

static void PopKeyScope(LxJsonStream &stream)
{
	stream.depth--;
	if (stream.depth < 16)
		stream.currentKey = stream.keyStack[stream.depth];
}

static bool ParseArray(LxVariant &outValue, LxJsonStream &stream)
{
	PushKeyScope(stream);
	outValue.SetType(LxVariant::TYPE_LIST);
	paf::list<LxVariant> *listObj = outValue.m_value.listVal;

	int ch = StreamSkipWhitespace(stream);

	if (ch == ']') {
		PopKeyScope(stream);
		return true;
	}
	StreamPushBack(stream);

	for (;;) {
		listObj->push_back(LxVariant());
		LxVariant &elem = listObj->back();

		if (!LxJsonParseValue(elem, stream)) {
			PopKeyScope(stream);
			return false;
		}
		if (stream.arrayCallback != NULL) {
			SceInt32 cbRet = stream.arrayCallback->OnArrayElement(
				stream.keyStack, stream.depth, listObj, &elem);

			stream.lastError = cbRet;
			if (cbRet < 0) {
				PopKeyScope(stream);
				return false;
			}
		}
		ch = StreamSkipWhitespace(stream);
		if (ch != ',')
			break;
	}

	bool ok = (ch == ']');

	if (!ok)
		StreamPushBack(stream);
	PopKeyScope(stream);
	return ok;
}

static bool ParseObject(LxVariant &outValue, LxJsonStream &stream)
{
	PushKeyScope(stream);
	outValue.SetType(LxVariant::TYPE_MAP);
	paf::map<LxString, LxVariant> *mapObj = outValue.m_value.mapVal;

	int ch = StreamSkipWhitespace(stream);

	if (ch == '}') {
		PopKeyScope(stream);
		return true;
	}
	StreamPushBack(stream);

	LxString key;

	for (;;) {
		ch = StreamSkipWhitespace(stream);
		if (ch != '"') {
			StreamPushBack(stream);
			PopKeyScope(stream);
			return false;
		}
		if (!ParseStringToken(key, stream)) {
			PopKeyScope(stream);
			return false;
		}
		ch = StreamSkipWhitespace(stream);
		if (ch != ':') {
			StreamPushBack(stream);
			PopKeyScope(stream);
			return false;
		}
		stream.currentKey = key.CStr();
		LxVariant &member = (*mapObj)[key];

		if (!LxJsonParseValue(member, stream)) {
			PopKeyScope(stream);
			return false;
		}
		stream.currentKey = NULL;
		key.Assign(NULL, 0);

		ch = StreamSkipWhitespace(stream);
		if (ch != ',')
			break;
	}

	bool ok = (ch == '}');

	if (!ok)
		StreamPushBack(stream);
	PopKeyScope(stream);
	return ok;
}

static bool MatchLiteral(LxJsonStream &stream, const char *expected)
{
	for (const char *p = expected; *p != '\0'; ++p) {
		if (StreamNextChar(stream) != (unsigned char)*p) {
			StreamPushBack(stream);
			return false;
		}
	}
	return true;
}

bool LxJsonParseValue(LxVariant &outValue, LxJsonStream &stream)
{
	int ch = StreamSkipWhitespace(stream);

	if (ch == '"') {
		outValue.SetType(LxVariant::TYPE_STRING);
		return ParseStringToken(*outValue.m_value.strVal, stream);
	}
	if (ch == '[')
		return ParseArray(outValue, stream);
	if (ch == '{')
		return ParseObject(outValue, stream);
	if (ch == 't') {
		if (!MatchLiteral(stream, "rue"))
			return false;
		outValue.SetType(LxVariant::TYPE_BOOL);
		outValue.m_value.boolVal = true;
		return true;
	}
	if (ch == 'f') {
		if (!MatchLiteral(stream, "alse"))
			return false;
		outValue.SetType(LxVariant::TYPE_BOOL);
		outValue.m_value.boolVal = false;
		return true;
	}
	if (ch == 'n') {
		if (!MatchLiteral(stream, "ull"))
			return false;
		outValue.SetType(LxVariant::TYPE_NULL);
		return true;
	}
	if ((ch >= '0' && ch <= '9') || ch == '-') {
		StreamPushBack(stream);
		return ParseNumber(outValue, stream);
	}
	StreamPushBack(stream);
	return false;
}

struct JsonStringReaderCtx {
	const char *data;
	SceSize len;
	SceSize pos;
};

static SceInt32 ReadCharFromString(int *outCh, void *ctx)
{
	JsonStringReaderCtx *reader = (JsonStringReaderCtx *)ctx;

	if (reader->pos >= reader->len) {
		*outCh = -1;
		return 0;
	}
	*outCh = (unsigned char)reader->data[reader->pos++];
	return 0;
}

bool LxJsonParseString(const char *jsonText, SceSize jsonLen, LxVariant &outValue)
{
	JsonStringReaderCtx reader;

	reader.data = jsonText;
	reader.len = jsonLen;
	reader.pos = 0;

	LxJsonStream stream;

	sce_paf_memset(&stream, 0, sizeof(stream));
	stream.readChar = ReadCharFromString;
	stream.ctx = &reader;
	stream.currentChar = -1;
	stream.numberBuf.Resize(64);
	stream.stringBuf.Resize(256);

	bool ok = LxJsonParseValue(outValue, stream);

	stream.numberBuf.Resize(0);
	stream.stringBuf.Resize(0);
	return ok;
}
