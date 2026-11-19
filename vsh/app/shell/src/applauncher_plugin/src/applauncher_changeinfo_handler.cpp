/*
 * applauncher_changeinfo_handler.cpp: the SAX handler of the change information (0x810CAC4C-
 * 0x810CAFB2): each <changes app_ver="..."> newer than the installed version, its CDATA text
 * kept, and the versions as text. The file's name and the names here are ours (PLACEHOLDER): no
 * record of the 3.6x shell names them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <bXCe/bxce_internal.h>
#include "applauncher.h"
#include "applauncher_home.h"
#include "applauncher_changeinfo.h"

using namespace paf;

/* (0x810CAC4C) */
ApplauncherChangeInfoHandler::ApplauncherChangeInfoHandler(ApplauncherChangeInfo *info,
							   SceUInt32 since)
	: m_info(info), m_since(since)
{
	m_newer = false;
	sce_paf_memset(&m_changes, 0, sizeof(m_changes));
}

/* an app_ver ("1.23") as a number (123), the dots left out; size counts its end (0x810CAC78) */
static __attribute__((noinline)) int applauncherChangeInfoParseVersion(const char *text,
									 SceSize size,
									 SceUInt32 *version)
{
	SceSize length = 0;

	if (text != NULL) {
		for (const char *p = text; length < size; ++length) {
			if (*p++ == '\0')
				break;
		}
	}
	if (length > size - 1)
		return APPLAUNCHER_ERROR_CHANGE_INFO_READ;

	SceUInt32 value = 0;
	SceUInt32 scale = 1;

	for (int i = length - 1; i >= 0; --i) {
		if (text[i] != '.') {
			value += (text[i] - '0') * scale;
			scale *= 10;
		}
	}
	*version = value;

	return 0;
}

/* (0x810CACE0) */
int ApplauncherChangeInfoHandler::startElement(void *userData, const char *name, int length,
					       const bXCe::AttributeList *attrList)
{
	if (sce_paf_strncmp(name, "changes", 8) == 0) {
		SceUInt32 version = 0;

		for (int i = 0; i < attrList->getLength(); ++i) {
			const bXCe::Attr *attr = (const bXCe::Attr *)attrList->getAttribute(i);

			if (sce_paf_strncmp(attr->getName(), "app_ver", 8) == 0) {
				m_changes.result = applauncherChangeInfoParseVersion(attr->getValue(),
					attr->getValueLength() + 1, &version);
				m_changes.version = version;
				if (version > m_since)
					m_newer = true;
				return 0;
			}
		}
	}
	return 0;
}

/* (0x810CAD82) */
int ApplauncherChangeInfoHandler::endElement(void *userData, const char *name, int length)
{
	if (sce_paf_strncmp(name, "changes", 8) == 0) {
		sce_paf_memset(&m_changes, 0, sizeof(m_changes));
		m_newer = false;
	}
	return 0;
}

/* a <changes>'s CDATA kept, its "]]>" left out (0x810CADB0) */
int ApplauncherChangeInfoHandler::skippedText(void *userData, int tokenType, const char *text,
					      int length)
{
	if (m_newer && m_changes.result >= 0 && m_changes.version != 0) {
		string changes;

		changes.append(text, length);

		string::size_type pos = changes.rfind("]]>", string::npos, sce_paf_strlen("]]>"));

		if (pos != string::npos)
			changes.erase(pos, pos + 3);
		m_info->AddEntry(m_changes.version, changes);
	}
	return 0;
}

/* (0x810CAE92) */
int applauncherChangeInfoVersionToString(SceUInt32 version, string *text)
{
	char buf[8];

	if (version > 9999)
		return APPLAUNCHER_ERROR_CHANGE_INFO_READ;

	SceUInt32 value = version;
	int length = 5, dot = 2, i = 4;

	if (value < 1000) {
		length = 4;
		dot = 1;
		i = 3;
	}
	buf[length] = '\0';
	for (; i >= 0; --i) {
		if (i == dot) {
			buf[i] = '.';
		} else {
			buf[i] = (value % 10) | '0';
			value /= 10;
		}
	}
	*text = buf;

	return 0;
}
