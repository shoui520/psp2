/*
 * console_info_ci_files.cpp: the "ci" record's four values files (0x810F588C-0x810F5DE8): stored
 * and compared together, and committed by moving each one's file into ur0:ci/prev, the files
 * there kept as "<name>_" until all four are in place. The file's and the names are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/string.h>
#include <file/localfile.h>
#include "console_info.h"

using namespace paf;

/* the values files: 32-bit, 64-bit and text values, and the registry keys' */
#define CONSOLE_INFO_CI_FILES		4

/* the sizes of the values files' values */
#define CONSOLE_INFO_CI_INT_SIZE	4
#define CONSOLE_INFO_CI_INT64_SIZE	8
#define CONSOLE_INFO_CI_STRING_SIZE	1022

/* the suffix of a previous values file kept while the record is committed */
#define CONSOLE_INFO_CI_BACKUP		"_"

/* (0x814098DC, one function for the build's empty ones) nothing to do once the values are
   stored */
static void consoleInfoCiStored(void)
{
}

/* (0x810F588C) */
ConsoleInfoCiFiles::ConsoleInfoCiFiles()
{
	sce_paf_memset(m_files, 0, sizeof(m_files));
	m_files[0] = new ConsoleInfoCiFieldValueFile(CONSOLE_INFO_CI_INT_SIZE,
						     consoleInfoCiGetIntField,
						     consoleInfoCiUpdateCounts, consoleInfoCiStored);
	m_files[1] = new ConsoleInfoCiFieldValueFile(CONSOLE_INFO_CI_INT64_SIZE,
						     consoleInfoCiGetInt64Field);
	m_files[2] = new ConsoleInfoCiFieldValueFile(CONSOLE_INFO_CI_STRING_SIZE,
						     consoleInfoCiGetStringField,
						     consoleInfoCiUpdateStrings, consoleInfoCiStored);
	m_files[3] = new ConsoleInfoCiRegValueFile();
}

/* (0x810F5944, 0x810F597C) */
ConsoleInfoCiFiles::~ConsoleInfoCiFiles()
{
	for (unsigned int i = 0; i < CONSOLE_INFO_CI_FILES; i++) {
		if (m_files[i] != NULL) {
			delete m_files[i];
			m_files[i] = NULL;
		}
	}
}

/* (0x810F59B8) the values now, each into its file */
int ConsoleInfoCiFiles::Store()
{
	for (unsigned int i = 0; i < CONSOLE_INFO_CI_FILES; i++) {
		if (m_files[i] == NULL)
			return CONSOLE_INFO_ERROR_NO_MEMORY;

		int ret = m_files[i]->Save();

		if (ret != 0)
			return ret;
	}
	return 0;
}

/* (0x810F59E0) the lines of the values changed */
int ConsoleInfoCiFiles::Compare(ConsoleInfoCi *ci)
{
	for (unsigned int i = 0; i < CONSOLE_INFO_CI_FILES; i++) {
		if (m_files[i] == NULL)
			return CONSOLE_INFO_ERROR_NO_MEMORY;

		int ret = m_files[i]->Load(ci);

		if (ret != 0)
			return ret;
	}
	return 0;
}

/* (0x810F5A10) the values stored made the previous ones: the previous files kept aside, the new
   ones moved in, and the kept ones removed; a failure puts the previous files back */
int ConsoleInfoCiFiles::Commit()
{
	string prev;
	string backup;
	string temp;
	int ret;
	int i;

	for (i = 0; i < CONSOLE_INFO_CI_FILES; i++) {
		prev = m_files[i]->GetPrevPath();
		backup = prev;
		backup += CONSOLE_INFO_CI_BACKUP;
		LocalFile::RemoveFile(backup.c_str());
		ret = LocalFile::RenameFile(prev.c_str(), backup.c_str());
		if (ret != 0) {
			for (int j = i - 1; j >= 0; j--) {
				prev = m_files[j]->GetPrevPath();
				backup = prev;
				backup += CONSOLE_INFO_CI_BACKUP;
				LocalFile::RenameFile(backup.c_str(), prev.c_str());
			}
			return ret;
		}
	}
	for (i = 0; i < CONSOLE_INFO_CI_FILES; i++) {
		prev = m_files[i]->GetPrevPath();
		temp = m_files[i]->GetTempPath();
		ret = LocalFile::RenameFile(temp.c_str(), prev.c_str());
		if (ret != 0) {
			for (int j = 0; j < CONSOLE_INFO_CI_FILES; j++) {
				prev = m_files[j]->GetPrevPath();
				backup = prev;
				backup += CONSOLE_INFO_CI_BACKUP;
				LocalFile::RemoveFile(prev.c_str());
				LocalFile::RenameFile(backup.c_str(), prev.c_str());
			}
			return ret;
		}
	}
	for (i = 0; i < CONSOLE_INFO_CI_FILES; i++) {
		prev = m_files[i]->GetPrevPath();
		backup = prev;
		backup += CONSOLE_INFO_CI_BACKUP;
		LocalFile::RemoveFile(backup.c_str());
	}
	return 0;
}
