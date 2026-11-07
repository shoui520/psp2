/*
 * The parser of a title's index.xml, through SceNpCommon's streaming XML parser:
 *
 *	<eula npcommid="NPWR00000_00">
 *		<file name="..." version="..." country="us" language="1"></file>
 *		...
 *	</eula>
 *
 * The file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <std/new>
#include <std/stdlib.h>
#include <std/string.h>
#include "npeula_dialog_plugin.h"

/* the parser's state once it has failed */
#define NPEULA_PARSER_STATE_ERROR	3

/* the elements of the index */
static const NpXmlElement s_eulaElements[] = {
	{"eula", 0},
	{"", 1}
};

static const NpXmlElement s_fileElements[] = {
	{"file", 1},
	{"", 1}
};

static const NpXmlElement *const s_indexLevels[] = {
	s_eulaElements,
	s_fileElements
};

static const NpXmlSchema s_indexSchema = {
	s_indexLevels,
	2
};

/* the file whose element the parser is in */
static EulaFile *s_file = NULL;

/* the parser fails with the error, and returns it for every later event */
static inline int ParserError(EulaIndexParser *parser, int error)
{
	parser->m_error = error;
	parser->m_state = NPEULA_PARSER_STATE_ERROR;
	return error;
}

static inline bool IsName(const char *name, const char *element)
{
	return sceClibStrncasecmp(name, element, sce_paf_strlen(element)) == 0;
}

/* the language attribute's decimal number */
static inline int ParseLanguage(const char *str, int *value)
{
	SceSize count = sceClibStrnlen(str, 0x7FFFFFFF);
	SceUInt32 number = 0;
	int digit;
	int c;

	while (count-- != 0 && (c = *str++) != '\0') {
		if ((sceClibLookCtypeTable(c) & SCE_CLIB_CTYPE_DIGIT) == 0)
			return NPEULA_ERROR_INDEX_LANGUAGE;
		digit = c - '0';
		if (number > 0x19999999 || (number == 0x19999999 && digit > 5))
			return NPEULA_ERROR_INDEX_LANGUAGE;
		number = number * 10 + digit;
	}
	*value = number;
	return SCE_OK;
}

/* the version attribute's decimal number of count characters: false when it is not one */
static inline bool ParseVersion(const char *str, SceSize count, SceUInt32 *value)
{
	SceUInt32 number = 0;
	int digit;
	int c;

	while (count-- != 0 && (c = *str++) != '\0') {
		if ((sceClibLookCtypeTable(c) & SCE_CLIB_CTYPE_DIGIT) == 0)
			return false;
		digit = c - '0';
		if (number > 0x19999999 || (number == 0x19999999 && digit > 5))
			return false;
		number = number * 10 + digit;
	}
	*value = number;
	return true;
}

/* an event of the document: each <file> element becomes a file of the index */
static int EulaIndexCallback(NpXmlParser *xml, int event, SceUInt32 depth, SceUInt32 index,
			     const char *name, const char *value, void *arg)
{
	EulaIndexParser *parser = (EulaIndexParser *)arg;
	SceNpCommunicationId npCommId;
	int ret;

	if (parser->m_state != 0)
		return parser->m_error;
	if (event == NP_XML_EVENT_END)
		return SCE_OK;
	if (IsName(name, "eula") && event == NP_XML_EVENT_START)
		return SCE_OK;
	if (IsName(name, "npcommid")) {
		sceClibMemset(&npCommId, 0, sizeof(npCommId));
		SceNpCommon_5BC54899(&npCommId, value);
		if (sce_paf_memcmp(&parser->m_npCommId, &npCommId, sizeof(npCommId)) != 0)
			return ParserError(parser, NPEULA_ERROR_INDEX_ID);
		return SCE_OK;
	}
	if (IsName(name, "file") && event == NP_XML_EVENT_START) {
		if (s_file != NULL)
			return ParserError(parser, NPEULA_ERROR_INDEX_FORMAT);
		EulaFile::Create(&s_file);
		return SCE_OK;
	}
	if (event == NP_XML_EVENT_TEXT) {
		if (s_file != NULL) {
			parser->m_index->AddFile(s_file);
			EulaFile::Destroy(s_file);
			s_file = NULL;
			return SCE_OK;
		}
		return ParserError(parser, NPEULA_ERROR_INDEX_FORMAT);
	}
	if (event == NP_XML_EVENT_ATTRIBUTE && depth == 1 && index == 0 && s_file != NULL) {
		if (IsName(name, "name")) {
			if (*value == '\0')
				return ParserError(parser, NPEULA_ERROR_NAME_TOO_LONG);
			sceClibStrncpy(s_file->m_name, value, sizeof(s_file->m_name));
		} else if (IsName(name, "version")) {
			if (*value == '\0')
				return ParserError(parser, NPEULA_ERROR_INDEX_VERSION);
			if (!ParseVersion(value, sce_paf_strlen(value), &s_file->m_version))
				return SCE_OK;
		} else if (IsName(name, "country")) {
			if (value == NULL || sceClibStrnlen(value, 256) != 2)
				return ParserError(parser, NPEULA_ERROR_INDEX_COUNTRY);
			s_file->m_country.data[0] = value[0];
			s_file->m_country.data[1] = value[1];
		} else if (IsName(name, "language")) {
			if (value == NULL || sceClibStrnlen(value, 256) == 0)
				return ParserError(parser, NPEULA_ERROR_INDEX_LANGUAGE);
			ret = ParseLanguage(value, &s_file->m_language);
			parser->m_error = ret;
			if (ret != SCE_OK)
				return ParserError(parser, NPEULA_ERROR_INDEX_LANGUAGE);
		} else {
			return ParserError(parser, NPEULA_ERROR_INDEX_FORMAT);
		}
	}
	return SCE_OK;
}

EulaIndexParser::EulaIndexParser() : m_index(NULL)
{
	sce_paf_memset(&m_npCommId, 0, sizeof(m_npCommId));
	sce_paf_memset(&m_parser, 0, sizeof(m_parser));
}

EulaIndexParser::~EulaIndexParser()
{
	if (m_index != NULL) {
		EulaIndex::Destroy(m_index);
		m_index = NULL;
	}
}

int EulaIndexParser::Create(EulaIndexParser **parser)
{
	void *mem = sce_paf_malloc(sizeof(EulaIndexParser));

	if (mem == NULL)
		return NPEULA_ERROR_NO_MEMORY;
	*parser = new (mem) EulaIndexParser();
	return SCE_OK;
}

void EulaIndexParser::Destroy(EulaIndexParser *parser)
{
	parser->~EulaIndexParser();
	sce_paf_free(parser);
}

/* the parser for the title's index, with an empty index */
int EulaIndexParser::Init(const SceNpCommunicationId *npCommId)
{
	int ret;

	m_level = -1;
	m_state = 0;
	m_error = 0;
	sce_paf_memcpy(&m_npCommId, npCommId, sizeof(m_npCommId));
	ret = EulaIndex::Create(npCommId, &m_index);
	if (ret < 0)
		ParserError(this, ret);
	ret = SceNpCommon_650D111B(&m_parser, &s_indexSchema, EulaIndexCallback, this);
	if (ret < 0)
		ParserError(this, ret);
	return ret;
}

/* the next size bytes of the document */
int EulaIndexParser::Parse(const char *data, int size, int last)
{
	int ret;

	ret = SceNpCommon_88B18A5B(&m_parser, data, size);
	if (ret < 0)
		return ParserError(this, ret);
	return SCE_OK;
}
