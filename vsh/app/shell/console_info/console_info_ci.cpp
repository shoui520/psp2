/*
 * console_info_ci.cpp: the console's platform, the "ci" record and its values files
 * (0x810F3910-0x810F4294). An entry of the record is the fields whose values changed since the
 * record was last committed: each values file is stored with the values now, then compared with
 * the previous one, which committing replaces. The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <std/string>
#include <std/string.h>
#include <std/stdio.h>
#include <file/localfile.h>
#include <vshbridge/vshbridge.h>
#include "console_info.h"

using namespace paf;

/* the platforms: a PS Vita, a PS TV */
#define CONSOLE_INFO_PLATFORM_VITA	1
#define CONSOLE_INFO_PLATFORM_DOLCE	2

/* PLACEHOLDER: ur0:ci/file's 32-bit value keeping the system software's version the record was
   committed with, and its 64-bit value set when the record is committed */
#define CONSOLE_INFO_CI_FILE_VERSION	178
#define CONSOLE_INFO_CI_FILE_COMMITTED	2

/* the record's version ("e=") */
#define CONSOLE_INFO_CI_VERSION		'7'

/* the size of a values file's name */
#define CONSOLE_INFO_CI_FILE_NAME_SIZE	64

/* (0x815445B0, 0x815445B4) the platform, read once */
static bool s_platformRead = false;
static int s_platform = CONSOLE_INFO_PLATFORM_VITA;

static int consoleInfoCiFormatVersion(const ConsoleInfoRecord::Field *field, char *buf,
				      int *len);

/* (0x815445B8) the entry's header */
static ConsoleInfoRecord::Field s_ciFields[] = {
	{ "c",	1,	consoleInfoFormatPlatform },
	{ "e",	1,	consoleInfoCiFormatVersion },
	{ "h",	111,	consoleInfoFormatConsole },
	{ "pn",	5,	consoleInfoFormatPage },
	{ "nd",	2,	consoleInfoFormatLineCount },
};

/* (0x815445F4, 0x8154473C) ur0:ci/file's 32-bit and 64-bit values cleared when the record is
   committed */
static int s_committedInts[] = {
	0x12, 0x13, 0x14, 0x15, 0x90, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B,
	0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA,
	0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB3, 0xB4, 0xB5, 0xB6, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9,
	0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xF1,
	0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xFD, 0xFE, 0x103, 0x104, 0x111, 0x112, 0x113,
	0x114, 0x115, 0x116, 0x117, 0x118, 0x119, 0x11A, 0x11B,
};
static int s_committedInt64s[] = { 3 };

/* (0x810F3910) */
int consoleInfoGetPlatform(void)
{
	if (!s_platformRead) {
		s_platformRead = true;
		if (vshSblAimgrIsDolce() != 0)
			s_platform = CONSOLE_INFO_PLATFORM_DOLCE;
	}
	return s_platform;
}

/* (0x810F394C) */
ConsoleInfoCi::ConsoleInfoCi(int entrySize)
	: ConsoleInfoRecord(CONSOLE_INFO_TYPE_CI, entrySize)
{
	m_updated = false;

	int version = -1;

	if (consoleInfoGetSystemVersion(&version) == 0) {
		SceUInt32 committed = 0;

		consoleInfoFileReadInt(CONSOLE_INFO_CI_FILE_VERSION, &committed, 0);
		m_updated = version != committed;
	}
}

/* (0x810F39C4, 0x810F39E8) */
ConsoleInfoCi::~ConsoleInfoCi()
{
}

/* (0x810F3A10) an entry of the fields changed; its count into data */
int ConsoleInfoCi::Write(void *data)
{
	int ret = m_files.Store();

	if (ret != 0)
		return ret;
	ret = NewEntry();
	if (ret == 0) {
		ret = m_files.Compare(this);
		FinishAndCloseEntry();
	}
	if (ret != 0)
		return ret;
	*(int *)data = m_count;
	return 0;
}

/* (0x810F3A52) the counters cleared, the version kept and the values files made the previous */
int ConsoleInfoCi::Commit()
{
	for (unsigned int i = 0; i < sizeof(s_committedInts) / sizeof(s_committedInts[0]); i++)
		consoleInfoFileWriteInt(s_committedInts[i], 0, 0);
	for (unsigned int i = 0; i < sizeof(s_committedInt64s) / sizeof(s_committedInt64s[0]); i++)
		consoleInfoFileWriteInt64(s_committedInt64s[i], 0, 0);
	consoleInfoFileWriteInt64(CONSOLE_INFO_CI_FILE_COMMITTED, (SceUInt64)-1, 0);

	int version = -1;

	if (consoleInfoGetSystemVersion(&version) == 0)
		consoleInfoFileWriteInt(CONSOLE_INFO_CI_FILE_VERSION, version, 0);
	return m_files.Commit();
}

/* (0x810F3AD8) */
const ConsoleInfoRecord::Field *ConsoleInfoCi::GetFields()
{
	return s_ciFields;
}

/* (0x8141FB40, one function for the build's "return 5") */
unsigned int ConsoleInfoCi::GetFieldCount()
{
	return sizeof(s_ciFields) / sizeof(s_ciFields[0]);
}

/* (0x810F3AE2) */
static int consoleInfoCiFormatVersion(const ConsoleInfoRecord::Field *field, char *buf,
				      int *len)
{
	buf[0] = CONSOLE_INFO_CI_VERSION;
	buf[1] = '\0';
	*len = 1;
	return 0;
}

/* (0x810F3AF0) */
ConsoleInfoCiValueFile::ConsoleInfoCiValueFile(int size)
	: m_size(size)
{
}

/* (0x810F3B08) */
int ConsoleInfoCiValueFile::Close()
{
	if (m_file.get() == NULL)
		return 0;
	m_file->Close();
	m_file.reset();
	return 0;
}

/* (0x810F3B70, 0x810F3BDC) */
ConsoleInfoCiValueFile::~ConsoleInfoCiValueFile()
{
	Close();
}

/* (0x810F3BEC) ur0:temp/ci/ci<size>.dat */
string ConsoleInfoCiValueFile::GetTempPath()
{
	char name[CONSOLE_INFO_CI_FILE_NAME_SIZE];
	string path(CONSOLE_INFO_TEMP_DIR);

	path += "/";
	sce_paf_snprintf(name, sizeof(name), "%s%d.dat", consoleInfoTypeName(CONSOLE_INFO_TYPE_CI),
			 m_size);
	path += name;
	return path;
}

/* (0x810F3D16) */
int ConsoleInfoCiValueFile::Open(uint32_t flags)
{
	int error = 0;

	{
		string path = GetTempPath();

		m_file = LocalFile::Open(path.c_str(), flags, CONSOLE_INFO_FILE_MODE, &error);
	}
	return error;
}

/* (0x810F3E34) the values now stored */
int ConsoleInfoCiValueFile::Save()
{
	int ret = Open(CONSOLE_INFO_FILE_WRITE);

	if (ret != 0)
		return ret;
	ret = Store();
	Close();
	return ret;
}

/* (0x810F3E5A) the values stored compared with the previous ones */
int ConsoleInfoCiValueFile::Load(ConsoleInfoCi *ci)
{
	int ret = Open(SCE_O_RDONLY);

	if (ret != 0)
		return ret;
	ret = Compare(ci);
	Close();
	return ret;
}

/* (0x810F3E82) ur0:ci/prev/ci/ci<size>.dat */
string ConsoleInfoCiValueFile::GetPrevPath()
{
	char name[CONSOLE_INFO_CI_FILE_NAME_SIZE];
	string path(CONSOLE_INFO_PREV_DIR);

	path += "/";
	path += "ci";
	path += "/";
	sce_paf_snprintf(name, sizeof(name), "%s%d.dat", consoleInfoTypeName(CONSOLE_INFO_TYPE_CI),
			 m_size);
	path += name;
	return path;
}

/* (0x810F4074) the previous values, made empty when there are none */
int ConsoleInfoCiValueFile::OpenPrev()
{
	int error = 0;

	{
		string path = GetPrevPath();

		m_file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, CONSOLE_INFO_FILE_MODE, &error);
	}
	if (error != 0) {
		string path = GetPrevPath();

		m_file = LocalFile::Open(path.c_str(), SCE_O_RDWR | SCE_O_CREAT,
					 CONSOLE_INFO_FILE_MODE, &error);
	}
	return error;
}
