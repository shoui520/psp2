/*
 * ScebXCe's parsers: the SAX parser, which hands each token to the user's handler, and the
 * document builder, which makes a tree of nodes from them; with the readers of character
 * references they share.
 */
#include "bxce_internal.h"
#include "byte_type.h"
#include <xml/xml_node_types.h>

/* UTF-8: the largest code point each length holds, its lead byte, and the trail bytes' bits */
SceUInt32 g_bxceUtf8Max[6] = { 0x7f, 0x7ff, 0xffff, 0x1fffff, 0x3ffffff, 0x7fffffff };
SceUInt32 g_bxceUtf8Lead[6] = { 0x00, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc };
SceUInt32 g_bxceUtf8TrailMask = 0x3f;
SceUInt32 g_bxceUtf8Trail = 0x80;

/* what each predefined entity stands for, in BXCE_REF_* order, and the length of its reference */
const char *g_bxceEntityText[5] = { "&", "\"", "<", ">", "'" };
static const int s_bxceEntityReferenceLength[5] = { 5, 6, 4, 4, 6 };

/* a character reference's code point as UTF-8, NUL-terminated; XML's Char production checked */
int ScebXCe_BD9D727B(SceUInt32 value, char **out, int *length)
{
	char *s = *out;
	int count;

	if ((int)value < 9 || value == 11 || value == 12 ||
	    ((int)value > 13 && (int)value < 32) ||
	    ((int)value > 0xd7ff && (int)value < 0xe000) ||
	    ((int)value > 0xfffd && (int)value < 0x10000) ||
	    (int)value > 0x10ffff) {
		*s = 0;
		*length = 0;
		return SCE_BXCE_ERROR_INVALID_CHARACTER;
	}

	if ((value & 0xff80) == 0) {
		count = 1;
		*s = value;
	} else if (s == NULL) {
		count = 0;
	} else {
		char *p;
		int i;

		for (count = 1; count < 7; count++) {
			if (value <= g_bxceUtf8Max[count - 1])
				break;
		}
		if (count > 1) {
			p = &s[count - 1];
			for (i = count - 1; i != 0; i--) {
				*p-- = (value & g_bxceUtf8TrailMask) | g_bxceUtf8Trail;
				value >>= 6;
			}
		}
		*s = value | g_bxceUtf8Lead[count - 1];
	}

	s[count] = 0;
	*length = count;
	return 0;
}

/* a decimal character reference's digits */
int ScebXCe_CC190FA8(SceUInt32 *value, const char *digits, SceUInt32 length)
{
	SceUInt32 i;
	int result;

	if (length == 0)
		return SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;

	result = 0;
	for (i = 0; i < length; i++) {
		if (s_bxceByteType[(SceUInt8)*digits] != BXCE_BT_DIGIT)
			return SCE_BXCE_ERROR_INVALID_DECIMAL;
		result = result * 10 + *digits - '0';
		digits++;
	}

	*value = result;
	return 0;
}

/* a hexadecimal character reference's digits */
int ScebXCe_DFB92627(SceUInt32 *value, const char *digits, SceUInt32 length)
{
	SceUInt32 i;
	int result;

	if (length == 0)
		return SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;

	result = 0;
	i = 0;
	do {
		char c = *digits;

		if (s_bxceByteType[(SceUInt8)c] == BXCE_BT_DIGIT)
			result = result * 16 + c - '0';
		else if (c >= 'a' && c <= 'f')
			result = result * 16 + c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			result = result * 16 + c - 'A' + 10;
		else
			return SCE_BXCE_ERROR_INVALID_HEXADECIMAL;
		i++;
		digits++;
	} while (i < length);

	*value = result;
	return 0;
}

/* the UTF-8 byte order mark */
__attribute__((noinline)) int bxceIsUtf8Bom(const char *s)
{
	if (s == NULL)
		return 0;
	return (SceUInt8)s[0] == 0xef && (SceUInt8)s[1] == 0xbb && (SceUInt8)s[2] == 0xbf;
}

/*
 * Runs the tokenizer over the chunk and reports each token to the handler. A chunk that ends
 * inside a token suspends the parse (PARSE_SUSPENDED) until the next chunk, unless it is the last.
 */
int bxceSaxParserParseChunk(BxceSaxParser *sax)
{
	static const char piEnd[] = "?>";
	static const char commentEnd[] = "-->";
	static const char cdataEnd[] = "]]>";
	BxceParser *parser = &sax->parser;
	int status;
	int result;
	int ret;

	if (parser->state == 0 || parser->token == BXCE_TOKEN_EMPTY) {
		ret = bxceParserReadToken(parser);
	} else {
		if (parser->state != BXCE_STATE_CONTENT)
			goto next;
		ret = bxceParserReadMarkup(parser);
	}
	if (ret == 0) {
		if (!parser->isFinal) {
			parser->status = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
			return SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
		}
		if (bxceNameStackIsEmpty(parser->nameStack) == 0) {
			sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_UNEXPECTED_EOF);
			parser->status = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
			return SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
		}
		return parser->status;
	}

next:
	for (;;) {
		if (!bxceParserTokenAllowed(parser)) {
			sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_UNEXPECTED_CHAR);
			parser->status = SCE_BXCE_ERROR_XML_UNEXPECTED_CHAR;
			return SCE_BXCE_ERROR_XML_UNEXPECTED_CHAR;
		}

		switch (parser->token) {
		case BXCE_TOKEN_START_TAG:
			if (parser->state == BXCE_STATE_CONTENT)
				parser->state = BXCE_STATE_TAG_NAME;
			if (!bxceParserReadStartTag(parser, &parser->elementName, parser->attrList)) {
				if (!parser->isFinal) {
					parser->status = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
					return SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
				}
				status = parser->status;
				if (status != 0) {
					sax->handler->fatalError(sax->userData, parser->status);
					parser->status = status;
					return status;
				}
				sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_UNEXPECTED_EOF);
				parser->status = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
				return SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
			}
			status = parser->status;
			if (status != 0)
				goto failed;

			bxceParserSkipSpace(parser);
			if (parser->position >= parser->size) {
				result = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
				if (parser->isFinal) {
					sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_UNEXPECTED_EOF);
					result = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
				}
				parser->status = result;
				return result;
			}
			if (parser->state != BXCE_STATE_TAG_CLOSE) {
				parser->emptyElement = bxceParserSkipChar(parser, '/');
				if (parser->position >= parser->size) {
					if (parser->isFinal) {
						sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_UNEXPECTED_EOF);
						result = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
					} else {
						parser->state = BXCE_STATE_TAG_CLOSE;
						result = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
					}
					parser->status = result;
					return result;
				}
			}
			if (bxceParserSkipChar(parser, '>') == 0) {
				sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_EXPECTED_CLOSING_ANGLE_BRACKET);
				parser->status = SCE_BXCE_ERROR_XML_EXPECTED_CLOSING_ANGLE_BRACKET;
				return SCE_BXCE_ERROR_XML_EXPECTED_CLOSING_ANGLE_BRACKET;
			}

			/* a second document element */
			if (parser->documentElementSeen) {
				if (bxceNameStackIsEmpty(parser->nameStack) != 0) {
					sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_INVALID_DOCUMENT_ELEMENT);
					parser->status = SCE_BXCE_ERROR_XML_INVALID_DOCUMENT_ELEMENT;
					break;
				}
			} else {
				parser->documentElementSeen = 1;
			}

			if (sax->handler->startElement(sax->userData, parser->elementName.data,
						       parser->elementName.length, parser->attrList) != 0) {
				sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_PARSE_ABORTED);
				parser->status = SCE_BXCE_ERROR_XML_PARSE_ABORTED;
				if (parser->elementName.data != NULL)
					ScebXCe_EC560647(parser->elementName.data);
				bxceParserResetBuffer(parser, &parser->elementName);
				break;
			}
			ScebXCe_B367042F(parser->attrList);

			if (!parser->emptyElement) {
				/* the name stack keeps the name until the end tag */
				if (bxceNameStackPush(parser->nameStack, parser->elementName.data,
						      parser->elementName.length) != 0) {
					if (parser->elementName.data != NULL)
						ScebXCe_EC560647(parser->elementName.data);
					bxceParserResetBuffer(parser, &parser->elementName);
					sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_NO_MEMORY);
					parser->status = SCE_BXCE_ERROR_NO_MEMORY;
					return SCE_BXCE_ERROR_NO_MEMORY;
				}
				if (parser->elementName.data != NULL)
					bxceParserResetBuffer(parser, &parser->elementName);
			} else {
				if (sax->handler->endElement(sax->userData, parser->elementName.data,
							     parser->elementName.length) != 0) {
					sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_PARSE_ABORTED);
					parser->status = SCE_BXCE_ERROR_XML_PARSE_ABORTED;
					if (parser->elementName.data != NULL)
						ScebXCe_EC560647(parser->elementName.data);
					bxceParserResetBuffer(parser, &parser->elementName);
					break;
				}
				if (parser->elementName.data != NULL) {
					ScebXCe_EC560647(parser->elementName.data);
					if (parser->elementName.data != NULL)
						bxceParserResetBuffer(parser, &parser->elementName);
				}
			}
			if (parser->skipIgnorableWhiteSpace)
				bxceParserSkipSpace(parser);
			break;

		case BXCE_TOKEN_END_TAG:
			if (parser->state == BXCE_STATE_CONTENT)
				parser->state = BXCE_STATE_END_TAG_NAME;
			if (bxceParserReadEndTag(parser) == 0) {
				result = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
				if (parser->isFinal) {
					sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_UNEXPECTED_EOF);
					result = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
				}
				parser->status = result;
				return result;
			}
			status = parser->status;
			if (status != 0) {
				sax->handler->fatalError(sax->userData, status);
				parser->status = status;
				break;
			}
			/* the name came from the name stack: its length is the buffer's capacity */
			if (sax->handler->endElement(sax->userData, parser->elementName.data,
						     parser->elementName.capacity) != 0) {
				sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_PARSE_ABORTED);
				parser->status = SCE_BXCE_ERROR_XML_PARSE_ABORTED;
			}
			if (parser->elementName.data != NULL) {
				ScebXCe_EC560647(parser->elementName.data);
				bxceParserResetBuffer(parser, &parser->elementName);
			}
			if (parser->skipIgnorableWhiteSpace)
				bxceParserSkipSpace(parser);
			break;

		case BXCE_TOKEN_CHAR_DATA:
			parser->state = BXCE_STATE_CHAR_DATA;
			bxceParserScanText(parser, &parser->text, &parser->textLength);
			if (sax->characterOffsets)
				ret = sax->handler->charactersWithOffset(sax->userData, parser->text, parser->textLength,
									 parser->consumed + parser->position - parser->textLength);
			else
				ret = sax->handler->characters(sax->userData, parser->text, parser->textLength);
			if (ret != 0)
				goto aborted;
			break;

		case BXCE_TOKEN_DTD:
			parser->state = BXCE_STATE_DTD;
			if (bxceParserScanDoctype(parser, &parser->skippedText, &parser->skippedTextLength,
						  &parser->terminator) == 0) {
				if (parser->isFinal) {
					sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_END_OF_DTD_NOT_FOUND);
					result = SCE_BXCE_ERROR_XML_END_OF_DTD_NOT_FOUND;
				} else {
					ret = sax->handler->skippedText(sax->userData, BXCE_TOKEN_DTD, parser->skippedText,
									parser->skippedTextLength);
					result = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
					if (ret != 0) {
						sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_PARSE_ABORTED);
						result = SCE_BXCE_ERROR_XML_PARSE_ABORTED;
					}
				}
				parser->status = result;
				return result;
			}
			parser->terminator = '>';
			bxceParserSkipSpace(parser);
			if (sax->handler->skippedText(sax->userData, BXCE_TOKEN_DTD_END, parser->skippedText,
						      parser->skippedTextLength) != 0)
				goto aborted;
			break;

		case BXCE_TOKEN_PI:
			parser->state = BXCE_STATE_PI;
			if (bxceParserScanUntil(parser, &parser->skippedText, &parser->skippedTextLength, piEnd,
						&parser->terminatorMatched, 2) == 0) {
				if (parser->isFinal) {
					sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_INVALID_PI);
					result = SCE_BXCE_ERROR_XML_INVALID_PI;
				} else {
					ret = sax->handler->skippedText(sax->userData, BXCE_TOKEN_PI, parser->skippedText,
									parser->skippedTextLength);
					result = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
					if (ret != 0) {
						sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_PARSE_ABORTED);
						result = SCE_BXCE_ERROR_XML_PARSE_ABORTED;
					}
				}
				parser->status = result;
				return result;
			}
			parser->terminatorMatched = 0;
			bxceParserSkipSpace(parser);
			if (sax->handler->skippedText(sax->userData, BXCE_TOKEN_PI_END, parser->skippedText,
						      parser->skippedTextLength) != 0)
				goto aborted;
			break;

		case BXCE_TOKEN_CDATA:
			parser->state = BXCE_STATE_CDATA;
			if (bxceParserScanUntil(parser, &parser->skippedText, &parser->skippedTextLength, cdataEnd,
						&parser->terminatorMatched, 3) == 0) {
				if (parser->isFinal) {
					sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_END_OF_CDSECT_NOT_FOUND);
					result = SCE_BXCE_ERROR_XML_END_OF_CDSECT_NOT_FOUND;
				} else {
					ret = sax->handler->skippedText(sax->userData, BXCE_TOKEN_CDATA, parser->skippedText,
									parser->skippedTextLength);
					result = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
					if (ret != 0) {
						sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_PARSE_ABORTED);
						result = SCE_BXCE_ERROR_XML_PARSE_ABORTED;
					}
				}
				parser->status = result;
				return result;
			}
			parser->terminatorMatched = 0;
			bxceParserSkipSpace(parser);
			if (sax->handler->skippedText(sax->userData, BXCE_TOKEN_CDATA_END, parser->skippedText,
						      parser->skippedTextLength) != 0)
				goto aborted;
			break;

		case BXCE_TOKEN_COMMENT:
			parser->state = BXCE_STATE_COMMENT;
			if (bxceParserScanUntil(parser, &parser->skippedText, &parser->skippedTextLength, commentEnd,
						&parser->terminatorMatched, 3) == 0) {
				if (parser->isFinal) {
					sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_END_OF_COMMENT_NOT_FOUND);
					result = SCE_BXCE_ERROR_XML_END_OF_COMMENT_NOT_FOUND;
				} else {
					ret = sax->handler->skippedText(sax->userData, BXCE_TOKEN_COMMENT, parser->skippedText,
									parser->skippedTextLength);
					result = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
					if (ret != 0) {
						sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_PARSE_ABORTED);
						result = SCE_BXCE_ERROR_XML_PARSE_ABORTED;
					}
				}
				parser->status = result;
				return result;
			}
			parser->terminatorMatched = 0;
			bxceParserSkipSpace(parser);
			if (sax->handler->skippedText(sax->userData, BXCE_TOKEN_COMMENT_END, parser->skippedText,
						      parser->skippedTextLength) != 0) {
aborted:
				sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_PARSE_ABORTED);
				parser->status = SCE_BXCE_ERROR_XML_PARSE_ABORTED;
			}
			break;

		case BXCE_TOKEN_ENTITY:
			parser->state = BXCE_STATE_REFERENCE;
			if (parser->characters.data == NULL) {
				parser->characters.data = (char *)ScebXCe_B8DDE741(BXCE_BUFFER_INITIAL_SIZE);
				if (parser->characters.data == NULL) {
					sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_NO_MEMORY);
					parser->status = SCE_BXCE_ERROR_NO_MEMORY;
					break;
				}
				parser->characters.length = 0;
				parser->characters.capacity = BXCE_BUFFER_INITIAL_SIZE;
			}
			if (bxceParserReadReference(parser, &parser->characters) != 0) {
				int type;

				status = parser->status;
				if (status != 0) {
failed:
					sax->handler->fatalError(sax->userData, parser->status);
					parser->status = status;
					return status;
				}
				parser->characters.data[parser->characters.length] = 0;
				type = bxceParserLookupReference(parser, &parser->characters);
				if (type == BXCE_REF_ERROR) {
					sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_SYNTAX_ERROR);
					parser->status = SCE_BXCE_ERROR_XML_SYNTAX_ERROR;
				}
				if (type >= BXCE_REF_AMP && type <= BXCE_REF_APOS) {
					const char *text = g_bxceEntityText[type];

					if (sax->characterOffsets)
						ret = sax->handler->charactersWithOffset(sax->userData, text, 1,
							parser->consumed + parser->position - s_bxceEntityReferenceLength[type]);
					else
						ret = sax->handler->characters(sax->userData, text, 1);
					if (ret != 0) {
						sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_PARSE_ABORTED);
						parser->status = SCE_BXCE_ERROR_XML_PARSE_ABORTED;
					}
					parser->characters.length = 0;
				} else if (type == BXCE_REF_CHAR) {
					if (sax->characterOffsets)
						ret = sax->handler->charactersWithOffset(sax->userData, parser->characters.data,
							parser->characters.length, parser->consumed + parser->position);
					else
						ret = sax->handler->characters(sax->userData, parser->characters.data,
									       parser->characters.length);
					if (ret != 0) {
						sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_PARSE_ABORTED);
						parser->status = SCE_BXCE_ERROR_XML_PARSE_ABORTED;
					}
					parser->characters.length = 0;
				} else if (parser->characters.length != 0) {
					if (sax->handler->entityData(sax->userData, type, parser->characters.data,
								     parser->characters.length) != 0) {
						sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_PARSE_ABORTED);
						parser->status = SCE_BXCE_ERROR_XML_PARSE_ABORTED;
					}
					parser->characters.length = 0;
				}
				break;
			}
			result = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
			if (parser->isFinal) {
				sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_EXPECTED_SEMI_COLON);
				result = SCE_BXCE_ERROR_XML_EXPECTED_SEMI_COLON;
			}
			parser->status = result;
			return result;

		default:
			break;
		}

		if (parser->status != 0)
			return parser->status;
		parser->lastToken = parser->token;
		if (bxceParserReadToken(parser) == 0)
			break;
	}

	if (parser->isFinal) {
		if (bxceNameStackIsEmpty(parser->nameStack) == 0) {
			sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_UNEXPECTED_EOF);
			parser->status = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
		}
	} else {
		parser->status = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
		if (parser->state != BXCE_STATE_CONTENT)
			parser->token = BXCE_TOKEN_EMPTY;
	}
	return parser->status;
}

int ScebXCe_C85586BD(BxceSaxParser *sax)
{
	sax->characterOffsets = 0;
	sax->handler = NULL;
	sax->userData = NULL;
	return bxceParserInitialize(&sax->parser);
}

int ScebXCe_928189AB(BxceSaxParser *sax)
{
	bxceParserTerminate(&sax->parser);
	return 0;
}

int ScebXCe_9E06C329(BxceSaxParser *sax)
{
	return bxceParserReset(&sax->parser);
}

/* parses a chunk of the document; isFinal marks the last */
int ScebXCe_4E9A7061(BxceSaxParser *sax, const char *input, int size, SceUInt8 isFinal)
{
	BxceParser *parser = &sax->parser;
	int result;

	if (parser->status != 0) {
		if (parser->status == SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS) {
			parser->status = 0;
		} else {
			result = bxceParserReset(parser);
			parser->status = result;
			if (result != 0)
				return result;
		}
	} else {
		result = bxceParserReset(parser);
		parser->status = result;
		if (result != 0)
			return result;
	}

	if (sax->handler == NULL)
		return SCE_BXCE_ERROR_XML_NULL_HANDLER;

	parser->size = size;
	parser->input = input;
	parser->position = 0;
	parser->isFinal = isFinal;
	if (size > 3 && bxceIsUtf8Bom(input)) {
		parser->input = input + 3;
		parser->size = size - 3;
	}

	if (parser->state == 0) {
		bxceParserSkipSpace(parser);
		if (sax->handler->startDocument(sax->userData) != 0)
			goto aborted;
	}

	if (bxceSaxParserParseChunk(sax) == 0 && isFinal) {
		if (!parser->documentElementSeen) {
			sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_DOCUMENT_ELEMENT_NOT_FOUND);
			parser->status = SCE_BXCE_ERROR_XML_DOCUMENT_ELEMENT_NOT_FOUND;
			return SCE_BXCE_ERROR_XML_DOCUMENT_ELEMENT_NOT_FOUND;
		}
		if (sax->handler->endDocument(sax->userData) != 0) {
aborted:
			sax->handler->fatalError(sax->userData, SCE_BXCE_ERROR_XML_PARSE_ABORTED);
			result = SCE_BXCE_ERROR_XML_PARSE_ABORTED;
			parser->status = SCE_BXCE_ERROR_XML_PARSE_ABORTED;
			goto done;
		}
	}
	result = parser->status;
done:
	if (result == SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS)
		parser->consumed += parser->size;
	return result;
}

/* saves the parse in a free slot and gives the parser a fresh state; the slot, -1 when none is free */
int ScebXCe_3CAF2B78(BxceSaxParser *sax)
{
	BxceParser *parser = &sax->parser;
	BxceParserState *state;
	int slot;

	slot = 0;
	while (parser->saved[slot] != NULL) {
		slot++;
		if (slot >= BXCE_PARSER_SAVED_MAX)
			return -1;
	}

	parser->saved[slot] = (BxceParserState *)ScebXCe_B8DDE741(sizeof(BxceParserState));
	if (parser->saved[slot] == NULL)
		return -2;

	bxceParserSaveState(parser, slot);
	state = parser->saved[slot];
	state->handler = sax->handler;
	state->userData = sax->userData;
	state->consumed = parser->consumed;
	state->characterOffsets = sax->characterOffsets;
	sax->userData = NULL;
	sax->handler = NULL;
	if (bxceParserReset(parser) != 0)
		return -2;
	return slot;
}

/* gives the parse in the slot back to the parser and frees the slot */
int ScebXCe_A12B0F90(BxceSaxParser *sax, SceUInt8 slot)
{
	BxceParser *parser = &sax->parser;
	BxceParserState *state;

	if (bxceParserRestoreState(parser, slot) < 0)
		return -1;

	state = parser->saved[slot];
	sax->userData = state->userData;
	sax->handler = state->handler;
	parser->consumed = state->consumed;
	sax->characterOffsets = state->characterOffsets;
	if (state != NULL)
		ScebXCe_EC560647(state);
	parser->saved[slot] = NULL;
	return 0;
}

/*
 * The document builder's token loop: each token written into the document. A chunk that ends
 * inside a token suspends the parse until the next chunk, unless it is the last.
 */
int bxceDocumentBuilderParseChunk(BxceDocumentBuilder *builder)
{
	static const char piEnd[] = "?>";
	static const char commentEnd[] = "-->";
	static const char cdataEnd[] = "]]>";
	BxceParser *parser = &builder->parser;
	BxceDocument *doc;
	int result;
	int ret;

	if (parser->state == 0 || parser->token == BXCE_TOKEN_EMPTY) {
		ret = bxceParserReadToken(parser);
	} else {
		if (parser->state != BXCE_STATE_CONTENT)
			goto next;
		ret = bxceParserReadMarkup(parser);
	}
	if (ret == 0) {
		if (!parser->isFinal) {
			parser->status = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
			return SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
		}
		if (bxceNameStackIsEmpty(parser->nameStack) == 0) {
			builder->document->status = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
			parser->status = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
			return SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
		}
		return parser->status;
	}

next:
	for (;;) {
		if (!bxceParserTokenAllowed(parser)) {
			builder->document->status = SCE_BXCE_ERROR_XML_UNEXPECTED_CHAR;
			parser->status = SCE_BXCE_ERROR_XML_UNEXPECTED_CHAR;
			return SCE_BXCE_ERROR_XML_UNEXPECTED_CHAR;
		}

		switch (parser->token) {
		case BXCE_TOKEN_START_TAG: {
			BxceNodeId node;

			if (parser->state == BXCE_STATE_CONTENT)
				parser->state = BXCE_STATE_TAG_NAME;
			if (!bxceParserReadStartTag(parser, &parser->elementName, parser->attrList)) {
				if (parser->isFinal) {
					builder->document->status = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
					parser->status = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
					return SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
				}
				parser->status = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
				return SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
			}
			result = parser->status;
			if (result != 0) {
				builder->document->status = result;
				return result;
			}

			bxceParserSkipSpace(parser);
			if (parser->position >= parser->size) {
				result = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
				if (parser->isFinal) {
					result = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
					builder->document->status = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
				}
				parser->status = result;
				return result;
			}
			if (parser->state != BXCE_STATE_TAG_CLOSE) {
				parser->emptyElement = bxceParserSkipChar(parser, '/');
				if (parser->position >= parser->size) {
					if (parser->isFinal) {
						result = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
						builder->document->status = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
					} else {
						parser->state = BXCE_STATE_TAG_CLOSE;
						result = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
					}
					parser->status = result;
					return result;
				}
			}
			if (bxceParserSkipChar(parser, '>') == 0) {
				builder->document->status = SCE_BXCE_ERROR_XML_EXPECTED_CLOSING_ANGLE_BRACKET;
				parser->status = SCE_BXCE_ERROR_XML_EXPECTED_CLOSING_ANGLE_BRACKET;
				return SCE_BXCE_ERROR_XML_EXPECTED_CLOSING_ANGLE_BRACKET;
			}

			/* a second document element */
			if (parser->documentElementSeen) {
				int empty = bxceNameStackIsEmpty(parser->nameStack);

				doc = builder->document;
				if (empty != 0) {
					doc->status = SCE_BXCE_ERROR_XML_INVALID_DOCUMENT_ELEMENT;
					parser->status = SCE_BXCE_ERROR_XML_INVALID_DOCUMENT_ELEMENT;
					break;
				}
			} else {
				parser->documentElementSeen = 1;
				doc = builder->document;
			}

			node = bxceDocumentAppendElement(doc, builder->parent, parser->elementName.data,
							 parser->elementName.length, parser->attrList);
			if (node == 0)
				goto noMemory;
			ScebXCe_B367042F(parser->attrList);

			if (parser->emptyElement) {
				if (parser->elementName.data != NULL)
					ScebXCe_EC560647(parser->elementName.data);
				bxceParserResetBuffer(parser, &parser->elementName);
				parser->token = BXCE_TOKEN_END_TAG;
				bxceDocumentFlushAttributes(builder->document);
				if (builder->document->status != 0)
					goto noMemory;
			} else {
				/* the element is the parent of what follows, until its end tag */
				if (bxceNodeStackPush(builder->nodeStack, builder->parent) != 0)
					goto noMemory;
				builder->parent = node;
				if (bxceNameStackPush(parser->nameStack, parser->elementName.data,
						      parser->elementName.length) != 0)
					goto noMemory;
			}
			if (parser->elementName.data != NULL)
				bxceParserResetBuffer(parser, &parser->elementName);
			if (parser->skipIgnorableWhiteSpace)
				bxceParserSkipSpace(parser);
			break;
		}

		case BXCE_TOKEN_END_TAG:
			if (parser->state == BXCE_STATE_CONTENT)
				parser->state = BXCE_STATE_END_TAG_NAME;
			if (bxceParserReadEndTag(parser) == 0) {
				result = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
				if (parser->isFinal) {
					result = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
					builder->document->status = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
				}
				parser->status = result;
				return result;
			}
			result = parser->status;
			doc = builder->document;
			if (result != 0) {
				doc->status = result;
				break;
			}
			bxceDocumentFlushAttributes(doc);
			if (builder->document->status != 0)
				goto noMemory;
			builder->parent = bxceNodeStackPop(builder->nodeStack);
			if (parser->elementName.data != NULL) {
				ScebXCe_EC560647(parser->elementName.data);
				bxceParserResetBuffer(parser, &parser->elementName);
			}
			if (parser->skipIgnorableWhiteSpace)
				bxceParserSkipSpace(parser);
			break;

		case BXCE_TOKEN_CHAR_DATA:
			parser->state = BXCE_STATE_CHAR_DATA;
			bxceParserScanText(parser, &parser->text, &parser->textLength);
			doc = builder->document;
			if (builder->textNode != 0) {
				if (bxceDocumentExtendText(doc, builder->textNode, parser->text, parser->textLength) != 0)
					goto noMemory;
			} else {
				builder->textNode = bxceDocumentAppendText(doc, builder->parent, parser->text,
									   parser->textLength);
				if (builder->textNode == 0)
					goto noMemory;
			}
			break;

		case BXCE_TOKEN_DTD:
			parser->state = BXCE_STATE_DTD;
			if (bxceParserScanDoctype(parser, &parser->skippedText, &parser->skippedTextLength,
						  &parser->terminator) == 0) {
				result = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
				doc = builder->document;
				if (parser->isFinal) {
					result = SCE_BXCE_ERROR_XML_END_OF_DTD_NOT_FOUND;
					doc->status = SCE_BXCE_ERROR_XML_END_OF_DTD_NOT_FOUND;
				}
				parser->status = result;
				if (!builder->skipIgnorableText) {
					if (builder->skippedNode != 0)
						bxceDocumentExtendText(doc, builder->skippedNode, parser->skippedText,
								       parser->skippedTextLength);
					else
						builder->skippedNode = bxceDocumentAddSkipped(doc, parser->skippedText,
							parser->skippedTextLength, SCE_XML_TOKEN_DOM_SKIP_TEXT_DTD);
					doc = builder->document;
				}
				result = doc->status;
				if (result == 0)
					return parser->status;
				parser->status = result;
				return result;
			}
			parser->terminator = '>';
			bxceParserSkipSpace(parser);
			doc = builder->document;
			if (!builder->skipIgnorableText) {
				if (builder->skippedNode != 0) {
					bxceDocumentExtendText(builder->document, builder->skippedNode, parser->skippedText,
							       parser->skippedTextLength);
					builder->skippedNode = 0;
				} else {
					bxceDocumentAddSkipped(builder->document, parser->skippedText,
							       parser->skippedTextLength, SCE_XML_TOKEN_DOM_SKIP_TEXT_DTD);
				}
				doc = builder->document;
			}
			result = doc->status;
			if (result != 0) {
				parser->status = result;
				return result;
			}
			break;

		case BXCE_TOKEN_PI:
			parser->state = BXCE_STATE_PI;
			if (bxceParserScanUntil(parser, &parser->skippedText, &parser->skippedTextLength, piEnd,
						&parser->terminatorMatched, 2) == 0) {
				result = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
				if (parser->isFinal) {
					result = SCE_BXCE_ERROR_XML_INVALID_PI;
					builder->document->status = SCE_BXCE_ERROR_XML_INVALID_PI;
				}
				parser->status = result;
				if (!builder->skipIgnorableText) {
					if (builder->skippedNode != 0)
						bxceDocumentExtendText(builder->document, builder->skippedNode,
								       parser->skippedText, parser->skippedTextLength);
					else if (bxceNameStackIsEmpty(parser->nameStack) == 0)
						builder->skippedNode = bxceDocumentAppendSkipped(builder->document,
							builder->parent, parser->skippedText, parser->skippedTextLength,
							SCE_XML_TOKEN_DOM_SKIP_TEXT_PI, 0);
					else
						builder->skippedNode = bxceDocumentAddSkipped(builder->document,
							parser->skippedText, parser->skippedTextLength,
							SCE_XML_TOKEN_DOM_SKIP_TEXT_PI);
				}
				result = builder->document->status;
				if (result == 0)
					return parser->status;
				parser->status = result;
				return result;
			}
			parser->terminatorMatched = 0;
			bxceParserSkipSpace(parser);
			if (!builder->skipIgnorableText) {
				if (builder->skippedNode != 0) {
					bxceDocumentExtendText(builder->document, builder->skippedNode, parser->skippedText,
							       parser->skippedTextLength);
					builder->skippedNode = 0;
				} else if (bxceNameStackIsEmpty(parser->nameStack) == 0) {
					bxceDocumentAppendSkipped(builder->document, builder->parent, parser->skippedText,
								  parser->skippedTextLength, SCE_XML_TOKEN_DOM_SKIP_TEXT_PI, 0);
				} else {
					bxceDocumentAddSkipped(builder->document, parser->skippedText,
							       parser->skippedTextLength, SCE_XML_TOKEN_DOM_SKIP_TEXT_PI);
				}
			}
			result = builder->document->status;
			if (result != 0) {
				parser->status = result;
				return result;
			}
			break;

		case BXCE_TOKEN_CDATA: {
			int length;

			parser->state = BXCE_STATE_CDATA;
			if (bxceParserScanUntil(parser, &parser->skippedText, &parser->skippedTextLength, cdataEnd,
						&parser->terminatorMatched, 3) == 0) {
				result = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
				doc = builder->document;
				if (parser->isFinal) {
					result = SCE_BXCE_ERROR_XML_END_OF_CDSECT_NOT_FOUND;
					doc->status = SCE_BXCE_ERROR_XML_END_OF_CDSECT_NOT_FOUND;
				}
				parser->status = result;
				if (!builder->skipIgnorableText) {
					if (builder->skippedNode != 0)
						bxceDocumentExtendText(doc, builder->skippedNode, parser->skippedText,
								       parser->skippedTextLength);
					else
						builder->skippedNode = bxceDocumentAppendSkipped(doc, builder->parent,
							parser->skippedText, parser->skippedTextLength,
							SCE_XML_TOKEN_DOM_SKIP_TEXT_CDATA, 0);
					doc = builder->document;
				}
				result = doc->status;
				if (result == 0)
					return parser->status;
				parser->status = result;
				return result;
			}
			parser->terminatorMatched = 0;
			bxceParserSkipSpace(parser);

			/* the text ends with the "]]>" the scan matched, or the "]]" before the '>' */
			length = parser->skippedTextLength;
			doc = builder->document;
			if (length > 3) {
				const char *end = parser->skippedText + length;
				char last = end[-1];

				if (last == '>' && end[-2] == ']' && end[-3] == ']') {
					parser->skippedTextLength = length - 3;
					length -= 3;
				} else if (*end == '>' && last == ']' && end[-2] == ']') {
					parser->skippedTextLength = length - 3;
					length -= 3;
				}
			}
			if (!builder->skipIgnorableText) {
				if (builder->skippedNode != 0) {
					bxceDocumentExtendText(doc, builder->skippedNode, parser->skippedText, length);
					builder->skippedNode = 0;
				} else {
					bxceDocumentAppendSkipped(doc, builder->parent, parser->skippedText, length,
								  SCE_XML_TOKEN_DOM_SKIP_TEXT_CDATA, 0);
				}
				doc = builder->document;
			}
			result = doc->status;
			if (result != 0) {
				parser->status = result;
				return result;
			}
			break;
		}

		case BXCE_TOKEN_COMMENT:
			/* comments are not kept */
			parser->state = BXCE_STATE_COMMENT;
			if (bxceParserScanUntil(parser, &parser->skippedText, &parser->skippedTextLength, commentEnd,
						&parser->terminatorMatched, 3) == 0) {
				result = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
				doc = builder->document;
				if (parser->isFinal) {
					result = SCE_BXCE_ERROR_XML_END_OF_COMMENT_NOT_FOUND;
					doc->status = SCE_BXCE_ERROR_XML_END_OF_COMMENT_NOT_FOUND;
				}
				parser->status = result;
				if (doc->status != 0) {
					result = doc->status;
					doc->status = result;
					parser->status = result;
				}
				return result;
			}
			parser->terminatorMatched = 0;
			bxceParserSkipSpace(parser);
			result = builder->document->status;
			if (result != 0) {
				parser->status = result;
				return result;
			}
			break;

		case BXCE_TOKEN_ENTITY: {
			const char *data;
			int length;
			int type;

			parser->state = BXCE_STATE_REFERENCE;
			if (parser->characters.data == NULL) {
				parser->characters.data = (char *)ScebXCe_B8DDE741(BXCE_BUFFER_INITIAL_SIZE);
				if (parser->characters.data == NULL)
					goto noMemory;
				parser->characters.length = 0;
				parser->characters.capacity = BXCE_BUFFER_INITIAL_SIZE;
			}
			if (bxceParserReadReference(parser, &parser->characters) == 0) {
				if (parser->isFinal) {
					builder->document->status = SCE_BXCE_ERROR_XML_EXPECTED_SEMI_COLON;
					parser->status = SCE_BXCE_ERROR_XML_EXPECTED_SEMI_COLON;
					return SCE_BXCE_ERROR_XML_EXPECTED_SEMI_COLON;
				}
				parser->status = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
				return SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
			}
			result = parser->status;
			if (result != 0) {
				builder->document->status = result;
				return result;
			}

			parser->characters.data[parser->characters.length] = 0;
			type = bxceParserLookupReference(parser, &parser->characters);
			data = parser->characters.data;
			length = parser->characters.length;
			if (type == BXCE_REF_ERROR) {
				builder->document->status = SCE_BXCE_ERROR_XML_SYNTAX_ERROR;
				parser->status = SCE_BXCE_ERROR_XML_SYNTAX_ERROR;
			}
			if (type >= BXCE_REF_AMP && type <= BXCE_REF_APOS) {
				/* a predefined entity is text */
				const char *text;

				parser->state = BXCE_STATE_CHAR_DATA;
				text = g_bxceEntityText[type];
				if (builder->textNode != 0) {
					if (bxceDocumentExtendText(builder->document, builder->textNode, text, 1) != 0) {
						builder->document->status = SCE_BXCE_ERROR_NO_MEMORY;
						parser->status = SCE_BXCE_ERROR_NO_MEMORY;
					}
				} else {
					builder->textNode = bxceDocumentAppendText(builder->document, builder->parent, text, 1);
					if (builder->textNode == 0) {
						builder->document->status = SCE_BXCE_ERROR_NO_MEMORY;
						parser->status = SCE_BXCE_ERROR_NO_MEMORY;
					}
				}
				parser->characters.length = 0;
				break;
			}
			if (length != 0) {
				/* other references are entity nodes */
				bxceDocumentAppendSkipped(builder->document, builder->parent, data, length,
							  SCE_XML_TOKEN_DOM_ENTITY, type);
				result = builder->document->status;
				if (result != 0) {
					parser->status = result;
					return result;
				}
				parser->characters.length = 0;
			}
			break;
		}

		default:
			break;
		}

		result = parser->status;
		if (result != 0)
			return result;
		parser->lastToken = parser->token;
		if (builder->textNode != 0 && parser->token != BXCE_TOKEN_CHAR_DATA)
			builder->textNode = 0;
		if (bxceParserReadToken(parser) == 0)
			break;
	}

	if (parser->isFinal) {
		if (bxceNameStackIsEmpty(parser->nameStack) == 0) {
			builder->document->status = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
			parser->status = SCE_BXCE_ERROR_XML_UNEXPECTED_EOF;
		}
	} else {
		parser->status = SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS;
		if (parser->state != BXCE_STATE_CONTENT)
			parser->token = BXCE_TOKEN_EMPTY;
	}
	return parser->status;

noMemory:
	builder->document->status = SCE_BXCE_ERROR_NO_MEMORY;
	parser->status = SCE_BXCE_ERROR_NO_MEMORY;
	return SCE_BXCE_ERROR_NO_MEMORY;
}

int ScebXCe_11D1FC0D(BxceDocumentBuilder *builder)
{
	builder->skipIgnorableText = 0;
	builder->parent = 0;
	builder->skippedNode = 0;
	builder->nodeStack = NULL;
	builder->document = NULL;
	return bxceParserInitialize(&builder->parser);
}

int ScebXCe_8FA315F1(BxceDocumentBuilder *builder)
{
	bxceParserTerminate(&builder->parser);
	if (builder->nodeStack != NULL) {
		bxceNodeStackTerminate(builder->nodeStack);
		if (builder->nodeStack != NULL)
			ScebXCe_EC560647(builder->nodeStack);
		builder->nodeStack = NULL;
	}
	if (builder->document != NULL) {
		ScebXCe_B0891B13(builder->document);
		if (builder->document != NULL)
			ScebXCe_EC560647(builder->document);
		builder->document = NULL;
	}
	return 0;
}

/* a new document to write into, its root the parent */
int bxceDocumentBuilderReset(BxceDocumentBuilder *builder)
{
	bxceParserReset(&builder->parser);
	builder->skippedNode = 0;
	if (builder->document != NULL) {
		ScebXCe_B0891B13(builder->document);
		if (builder->document != NULL)
			ScebXCe_EC560647(builder->document);
		builder->document = NULL;
	}
	if (builder->nodeStack != NULL) {
		bxceNodeStackTerminate(builder->nodeStack);
		if (builder->nodeStack != NULL)
			ScebXCe_EC560647(builder->nodeStack);
		builder->nodeStack = NULL;
	}
	builder->parent = 0;

	builder->document = (BxceDocument *)ScebXCe_B8DDE741(sizeof(BxceDocument));
	if (builder->document == NULL)
		return SCE_BXCE_ERROR_NO_MEMORY;
	if (bxceDocumentInitializeWriter(builder->document) != 0) {
		ScebXCe_B0891B13(builder->document);
		if (builder->document != NULL)
			ScebXCe_EC560647(builder->document);
		builder->document = NULL;
		return SCE_BXCE_ERROR_NO_MEMORY;
	}
	builder->parent = ScebXCe_75E1B095(builder->document);

	builder->nodeStack = (BxceNodeStack *)ScebXCe_B8DDE741(sizeof(BxceNodeStack));
	if (builder->nodeStack == NULL)
		return SCE_BXCE_ERROR_NO_MEMORY;
	return bxceNodeStackInitialize(builder->nodeStack);
}

/* parses a chunk of the document into a new document; isFinal marks the last */
int ScebXCe_BC0D9A6B(BxceDocumentBuilder *builder, const char *input, int size, SceUInt8 isFinal)
{
	BxceParser *parser = &builder->parser;
	int result;

	if (parser->status != 0) {
		if (parser->status == SCE_BXCE_ERROR_XML_PARSE_IN_PROGRESS) {
			parser->status = 0;
		} else {
			result = bxceDocumentBuilderReset(builder);
			parser->status = result;
			if (result != 0)
				return result;
		}
	} else {
		result = bxceDocumentBuilderReset(builder);
		parser->status = result;
		if (result != 0)
			return result;
	}

	parser->size = size;
	parser->input = input;
	parser->position = 0;
	parser->isFinal = isFinal;
	if (size > 3 && bxceIsUtf8Bom(input)) {
		parser->input += 3;
		parser->size -= 3;
	}

	if (parser->state == 0)
		bxceParserSkipSpace(parser);

	if (bxceDocumentBuilderParseChunk(builder) == 0 && isFinal && !parser->documentElementSeen) {
		builder->document->status = SCE_BXCE_ERROR_XML_DOCUMENT_ELEMENT_NOT_FOUND;
		parser->status = SCE_BXCE_ERROR_XML_DOCUMENT_ELEMENT_NOT_FOUND;
	}
	return parser->status;
}
