/*
 * The index of a title's EULA: the files index.xml lists, each for a country and a language or
 * for any, and the one the account's region takes. The file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <std/new>
#include <std/stdlib.h>
#include <std/string.h>
#include "npeula_dialog_plugin.h"

__attribute__((noinline)) int EulaFile::Create(EulaFile **file)
{
	void *mem = sce_paf_malloc(sizeof(EulaFile));

	if (mem == NULL)
		return NPEULA_ERROR_NO_MEMORY;
	*file = new (mem) EulaFile();
	return SCE_OK;
}

void EulaFile::Destroy(EulaFile *file)
{
	file->~EulaFile();
	sce_paf_free(file);
}

/* the file's country, language, name and version */
inline int EulaFile::Set(const SceNpCountryCode *country, const char *name, SceUInt32 version,
			 int language)
{
	if (sceClibStrnlen(name, sizeof(m_name)) > sizeof(m_name) - 1)
		return NPEULA_ERROR_NAME_TOO_LONG;
	sceClibMemset(m_name, 0, sizeof(m_name));
	sceClibStrncpy(m_name, name, sizeof(m_name) - 1);
	m_language = language;
	m_name[sizeof(m_name) - 1] = '\0';
	m_country = *country;
	m_version = version;
	return SCE_OK;
}

EulaIndex::~EulaIndex()
{
	int i;

	for (i = 0; i < m_count; i++) {
		if (m_files[i] != NULL) {
			EulaFile::Destroy(m_files[i]);
			m_files[i] = NULL;
		}
	}
}

int EulaIndex::Create(const SceNpCommunicationId *npCommId, EulaIndex **index)
{
	void *mem = sce_paf_malloc(sizeof(EulaIndex));

	if (mem == NULL)
		return NPEULA_ERROR_NO_MEMORY;
	*index = new (mem) EulaIndex(npCommId);
	return SCE_OK;
}

void EulaIndex::Destroy(EulaIndex *index)
{
	index->~EulaIndex();
	sce_paf_free(index);
}

/* a copy of the file at the end of the index */
int EulaIndex::AddFile(const EulaFile *file)
{
	EulaFile *copy;
	int ret;

	if (file == NULL)
		return -1;
	if (m_count >= NPEULA_INDEX_FILES_MAX)
		return NPEULA_ERROR_TOO_MANY_FILES;
	copy = NULL;
	ret = EulaFile::Create(&copy);
	if (ret >= 0) {
		ret = copy->Set(&file->m_country, file->m_name, file->m_version, file->m_language);
		if (ret >= 0) {
			m_files[m_count] = copy;
			m_count++;
			return SCE_OK;
		}
	}
	if (copy != NULL)
		EulaFile::Destroy(copy);
	return ret;
}

/*
 * The file for the account: the country's file in its language, else the country's file for any
 * language, else the file for any country in the language, else the file for any country and
 * language.
 */
int EulaIndex::Find(int language, const SceNpCountryCode *country, EulaFile **file)
{
	EulaFile *countryLanguage = NULL;
	EulaFile *countryAny = NULL;
	EulaFile *anyLanguage = NULL;
	EulaFile *any = NULL;
	EulaFile *entry;
	int i;

	if (m_count <= 0)
		return NPEULA_ERROR_NOT_FOUND;
	for (i = 0; i < m_count; i++) {
		entry = m_files[i];
		if (entry->m_country.data[0] != '\0' || entry->m_country.data[1] != '\0') {
			if (entry->m_country.data[0] == country->data[0] &&
			    entry->m_country.data[1] == country->data[1]) {
				if (entry->m_language == language)
					countryLanguage = m_files[i];
				else if (entry->m_language < 0)
					countryAny = m_files[i];
			}
		} else {
			if (entry->m_language == language)
				anyLanguage = m_files[i];
			else if (entry->m_language < 0)
				any = m_files[i];
		}
	}
	if (countryLanguage != NULL) {
		*file = countryLanguage;
		return SCE_OK;
	}
	if (countryAny != NULL) {
		*file = countryAny;
		return SCE_OK;
	}
	if (anyLanguage != NULL) {
		*file = anyLanguage;
		return SCE_OK;
	}
	if (any != NULL) {
		*file = any;
		return SCE_OK;
	}
	return NPEULA_ERROR_NOT_FOUND;
}
