/*
 * SceIniFileProcessor: the scanner. It reads one line of an INI text at a time and says what kind of line
 * it is and where its key, value and line end are.
 */

#include "ini_internal.h"

/* a token with nothing in it, except the line kind and how it went, which a scan sets */
IniToken *iniTokenReset(IniToken *token)
{
	token->line = 0;
	token->error = 0;
	token->offset = 0;
	token->key = NULL;
	token->keyLength = 0;
	token->value = NULL;
	token->valueLength = 0;
	token->newline = NULL;
	token->newlineLength = 0;
	token->position = 0;
	token->carry = 0;
	return token;
}

IniInput *iniInputInit(IniInput *input)
{
	input->checkBom = false;
	input->data = NULL;
	input->size = 0;
	input->kind = 0;
	input->offset = 0;
	return input;
}

/* the line end: "\n", "\r" or "\r\n" (a zero byte also ends a line) */
__attribute__((noinline)) static int iniScanNewline(IniScanner *scanner)
{
	const unsigned char *p = scanner->cursor;
	const unsigned char *end = scanner->end;

	scanner->step = INI_STEP_NEWLINE_BEGIN;
	if (p < end) {
		for (;;) {
			unsigned char c = *p;

			if (c == 13) {
				SceSize length;

				if (p + 1 >= end || p[1] != 10) {
					scanner->token.newline = "\r";
					length = 1;
					scanner->cursor = p + 1;
				} else {
					scanner->token.newline = "\r\n";
					length = 2;
					scanner->cursor = p + 2;
				}
				scanner->token.newlineLength = length;
				scanner->step = INI_STEP_NEWLINE;
				return 0;
			}
			if (c == 10 || c == 0) {
				scanner->step = INI_STEP_NEWLINE;
				scanner->cursor = p + 1;
				scanner->token.newline = "\n";
				scanner->token.newlineLength = 1;
				return 0;
			}
			if (++p >= end)
				break;
		}
	}
	scanner->step = INI_STEP_END;
	return 0;
}

/* a value: up to the end of the line */
__attribute__((noinline)) static int iniScanValue(IniScanner *scanner)
{
	const unsigned char *start = scanner->cursor;
	const unsigned char *p = start;
	const unsigned char *end = scanner->end;

	scanner->token.value = (const char *)start;
	scanner->token.valueLength = 0;
	scanner->step = INI_STEP_VALUE;
	if (start < end) {
		for (;;) {
			unsigned char c = *p;

			if (c == 13 || c == 10 || c == 0) {
				scanner->cursor = p;
				scanner->token.valueLength = p - start;
				iniScanNewline(scanner);
				return 0;
			}
			if (++p >= end)
				break;
		}
	}
	scanner->step = INI_STEP_END;
	scanner->token.valueLength = p - start;
	scanner->token.error = 2;
	return INI_RESULT_END;
}

/* a key: up to the equals sign (a line that ends first has no value) */
__attribute__((noinline)) static int iniScanKey(IniScanner *scanner)
{
	const unsigned char *start = scanner->cursor;
	const unsigned char *p = start;
	const unsigned char *end = scanner->end;
	int ret;

	scanner->token.key = (const char *)start;
	scanner->token.keyLength = 0;
	scanner->step = INI_STEP_KEY;
	if (start < end) {
		for (;;) {
			unsigned char c = *p;

			if (c == '=') {
				SceSize length = p - start;

				scanner->token.keyLength = length;
				if (length != 0) {
					if (length >= 0x80)
						scanner->token.keyLength = 127;
					scanner->cursor = p + 1;
					return 0;
				}
				scanner->token.line = INI_LINE_BLANK;
				ret = iniScanNewline(scanner);
				goto done;
			}
			if (c == 13 || c == 10 || c == 0) {
				scanner->cursor = p;
				scanner->token.line = INI_LINE_BLANK;
				ret = iniScanNewline(scanner);
				goto done;
			}
			if (++p >= end)
				break;
		}
	}
	scanner->step = INI_STEP_END;
	scanner->token.error = 1;
	ret = INI_RESULT_END;
done:
	if (ret == 0)
		return ret;
	scanner->token.line = INI_LINE_BLANK;
	return ret;
}

/* the first line of a line: its kind decides how it is read */
__attribute__((noinline)) static int iniScanLine(IniScanner *scanner)
{
	unsigned c = *scanner->cursor;
	int ret;

	scanner->step = INI_STEP_LINE;
	if (c > 0x3a) {
		if (c > '=') {
			if (c == '[') {
				scanner->token.line = INI_LINE_SECTION;
				return iniScanNewline(scanner);
			}
		} else {
			if (c <= ';') {
				scanner->token.line = INI_LINE_COMMENT;
				return iniScanNewline(scanner);
			}
			if (c > '<') {
				scanner->token.line = INI_LINE_BLANK;
				return iniScanNewline(scanner);
			}
		}
	} else {
		switch (c) {
		case 0:
		case 10:
		case 13:
			scanner->token.line = INI_LINE_EMPTY;
			return iniScanNewline(scanner);
		case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 11: case 12:
			break;
		case 9:
			scanner->token.line = INI_LINE_BLANK;
			return iniScanNewline(scanner);
		default:
			if (c == 32) {
				scanner->token.line = INI_LINE_BLANK;
				return iniScanNewline(scanner);
			}
			break;
		}
	}
	scanner->token.line = INI_LINE_PAIR;
	ret = iniScanKey(scanner);
	if (ret == 0)
		return iniScanValue(scanner);
	return ret;
}

/* a byte order mark: UTF-8 is skipped, UTF-16 is refused */
__attribute__((noinline)) static int iniScanBom(IniScanner *scanner)
{
	const unsigned char *p = scanner->cursor;
	const unsigned char *end = scanner->end;
	SceSize kind = 1;

	scanner->bomKind = 0;
	scanner->step = INI_STEP_BOM;
	if (p + 2 > end)
		return INI_RESULT_END;
	if (p[0] == 0xfe && p[1] == 0xff) {
		scanner->bomKind = 3;
		return INI_RESULT_BAD_ENCODING;
	}
	if (p[0] == 0xff && p[1] == 0xfe) {
		scanner->bomKind = 4;
		return INI_RESULT_BAD_ENCODING;
	}
	if (p + 3 > end)
		return INI_RESULT_TEXT_TOO_SHORT;
	if (p[0] == 0xef && p[1] == 0xbb && p[2] == 0xbf) {
		kind = 2;
		scanner->cursor = p + 3;
	}
	scanner->bomKind = kind;
	return 0;
}

/* reads the next line of the input and gives the token of it */
int iniScannerScan(IniScanner *scanner, const IniInput *input, IniToken *out)
{
	const char *data = input->data;
	SceSize size = input->size;
	SceSize offset = input->offset;
	int ret;

	if (data != NULL && size > offset) {
		scanner->token.carry = 0;
		iniTokenReset(&scanner->token);
		data = input->data;
		size = input->size;
		offset = input->offset;
	}
	scanner->token.offset = offset;
	scanner->step = INI_STEP_START;
	scanner->cursor = (const unsigned char *)data + offset;
	scanner->end = (const unsigned char *)data + size;
	if (input->kind == 2) {
		scanner->token.line = INI_LINE_PAIR;
		ret = iniScanValue(scanner);
	} else {
		if (input->checkBom) {
			ret = iniScanBom(scanner);
			if (ret != 0)
				goto done;
			if (scanner->bomKind != 1)
				scanner->token.offset = scanner->cursor - (const unsigned char *)input->data;
		}
		ret = iniScanLine(scanner);
		if (ret == INI_RESULT_END && scanner->token.error == 1)
			scanner->token.carry = input->size - input->offset;
	}
done:
	if (scanner->step == INI_STEP_NEWLINE)
		scanner->token.position = scanner->cursor - (const unsigned char *)input->data;
	out->line = scanner->token.line;
	out->error = scanner->token.error;
	out->offset = scanner->token.offset;
	out->key = scanner->token.key;
	out->keyLength = scanner->token.keyLength;
	out->value = scanner->token.value;
	out->valueLength = scanner->token.valueLength;
	out->newline = scanner->token.newline;
	out->newlineLength = scanner->token.newlineLength;
	out->position = scanner->token.position;
	out->carry = scanner->token.carry;
	return ret;
}
