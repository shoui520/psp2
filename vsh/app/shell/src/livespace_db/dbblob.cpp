/*
 * DbBlob: a blob of a database row as a paf::File (an icon's image in tbl_appinfo_icon).
 */
#include <kernel/iofilemgr.h>
#include <regmgr/regmgr.h>
#include <np/np_manager.h>
#include <framework.h>
#include <rtc/rtc.h>
#include <dbutil/connection.h>
#include "livespace_db_internal.h"
#include "../../plugins.h"

#define DB_BLOB_ERROR_NOT_FOUND	(-2146422271)	/* 0x80102001: dbutil's, the offset past the end */

/*
 * Opens a DbBlob on the blob in column of table's row rowid in database, with transaction
 * called round each access (NULL: none, the blob stays open). result takes the open's error.
 */
paf::common::SharedPtr<DbBlob> DbBlobOpen(const char *database, const char *table,
	const char *column, SceInt64 rowid, int flags, DbBlobTransaction transaction, void *arg,
	int *result)
{
	DbBlob *blob = new DbBlob;

	if (blob == NULL) {
		if (result != NULL)
			*result = SCE_PAF_ERROR_FILE_NO_MEMORY;
		return paf::common::SharedPtr<DbBlob>();
	}

	DbBlob::OpenArg openArg(database, table, column, rowid, flags, transaction, arg);
	int error = blob->Open(&openArg);

	if (result != NULL)
		*result = error;
	if (!blob->IsOpened()) {
		delete blob;
		return paf::common::SharedPtr<DbBlob>();
	}
	return paf::common::SharedPtr<DbBlob>(blob);
}

/* ends an access's transaction (see BeginTransaction), closing the blob */
inline void DbBlob::EndTransaction(bool readOnly)
{
	if (m_blob != NULL) {
		m_connection->CloseBlob(m_blob);
		m_blob = NULL;
	}
	if (m_arg->transaction != NULL)
		m_arg->transaction(DB_BLOB_TRANSACTION_END, readOnly, m_arg->arg, &m_connection);
}

int32_t DbBlob::Close()
{
	m_mutex.Lock();
	if (m_arg != NULL && m_arg->transaction == NULL)
		EndTransaction(m_arg->flags == 0);
	delete m_arg;
	m_arg = NULL;
	m_mutex.Unlock();
	return 0;
}

DbBlob::~DbBlob()
{
	Close();
}

int32_t DbBlob::GetFileType() const
{
	return DB_BLOB_FILE_TYPE;
}

/* 7, and 15 while open writable */
int32_t DbBlob::GetCapability() const
{
	paf::thread::RMutex *mutex = const_cast<paf::thread::RMutex *>(&m_mutex);
	int32_t capability = 7;

	mutex->Lock();
	if (m_arg != NULL && m_arg->flags == 1)
		capability = 15;
	mutex->Unlock();
	return capability;
}

/* begins an access's transaction (when the blob has a transaction function) and opens the blob */
int DbBlob::BeginTransaction(bool readOnly)
{
	OpenArg *arg;
	int result;

	if (m_arg->transaction != NULL) {
		result = m_arg->transaction(DB_BLOB_TRANSACTION_BEGIN, readOnly, m_arg->arg,
			&m_connection);
		if (result != 0)
			return result;
	}
	arg = m_arg;
	result = m_connection->OpenBlob(arg->database, arg->table, arg->column, arg->rowid,
		arg->flags, &m_blob);
	arg = m_arg;
	if (result != 0) {
		if (result == DB_BLOB_ERROR_NOT_FOUND)
			result = LSDB_ERROR_NOT_FOUND;
		if (arg->transaction != NULL)
			arg->transaction(DB_BLOB_TRANSACTION_END, readOnly, arg->arg, &m_connection);
	}
	return result;
}

int64_t DbBlob::GetFileSize() const
{
	DbBlob *blob = const_cast<DbBlob *>(this);
	int size;
	int result;
	bool began;

	blob->m_mutex.Lock();
	if (m_arg == NULL) {
		blob->m_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	size = 0;
	began = false;
	if (m_arg->transaction != NULL && m_blob == NULL) {
		result = blob->BeginTransaction(true);
		if (result != 0) {
			blob->m_mutex.Unlock();
			return result;
		}
		began = true;
	}
	result = m_connection->GetBlobSize(m_blob, &size);
	if (began)
		blob->EndTransaction(true);
	if (result == 0)
		result = size;
	blob->m_mutex.Unlock();
	return result;
}

int32_t DbBlob::Open(const paf::File::OpenArg *param)
{
	const OpenArg *arg = (const OpenArg *)param;
	int result;

	m_mutex.Lock();
	if (arg == NULL || GetFileType() != arg->file_type || arg->database.size() == 0 ||
	    arg->table.size() == 0 || arg->column.size() == 0 || arg->rowid == 0) {
		m_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	}
	m_arg = new OpenArg(*arg);
	if (arg->transaction == NULL) {
		result = BeginTransaction(arg->flags == 0);
		if (result != 0) {
			delete m_arg;
			m_arg = NULL;
			m_mutex.Unlock();
			return result;
		}
	}
	m_offset = 0;
	m_mutex.Unlock();
	return 0;
}

int32_t DbBlob::OpenAsync(const paf::File::OpenArg *param)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t DbBlob::CloseAsync()
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

bool DbBlob::IsOpened() const
{
	return m_arg != NULL;
}

bool DbBlob::IsEof()
{
	return GetFileSize() <= m_offset;
}

int32_t DbBlob::Abort()
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

/* reads up to nbyte at the offset, at most to the blob's end; returns the bytes asked for */
int32_t DbBlob::Read(void *buf, size_t nbyte)
{
	int size;
	int result;

	m_mutex.Lock();
	if (m_arg == NULL) {
		m_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	size = nbyte;
	if (m_arg->transaction != NULL) {
		result = BeginTransaction(true);
		if (result != 0) {
			m_mutex.Unlock();
			return result;
		}
	}
	result = m_connection->ReadBlob(m_blob, buf, nbyte, m_offset);
	if (result == DB_BLOB_ERROR_NOT_FOUND) {
		int fileSize = GetFileSize();

		if (m_offset > fileSize) {
			result = 0;
			size = 0;
		} else {
			if (fileSize - m_offset < (int)nbyte)
				nbyte = fileSize - m_offset;
			size = nbyte;
			result = m_connection->ReadBlob(m_blob, buf, nbyte, m_offset);
		}
	}
	if (result == 0)
		m_offset += size;
	if (m_arg->transaction != NULL)
		EndTransaction(true);
	m_mutex.Unlock();
	return size;
}

int32_t DbBlob::ReadAsync(void *buf, size_t nbyte)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t DbBlob::Write(const void *buf, size_t nbyte)
{
	int result;

	m_mutex.Lock();
	if (m_arg == NULL) {
		m_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NOT_OPENED;
	}
	if (m_arg->flags == 0) {
		m_mutex.Unlock();
		return SCE_PAF_ERROR_FILE_NO_ACCESS;
	}
	if (m_arg->transaction != NULL) {
		result = BeginTransaction(false);
		if (result != 0) {
			m_mutex.Unlock();
			return result;
		}
	}
	result = m_connection->WriteBlob(m_blob, buf, nbyte, m_offset);
	if (result == 0)
		m_offset += nbyte;
	if (m_arg->transaction != NULL)
		EndTransaction(result != 0);
	m_mutex.Unlock();
	return nbyte;
}

int32_t DbBlob::WriteAsync(const void *buf, size_t nbyte)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

/* the offset is an int: a position outside it, before the start, is not taken */
int64_t DbBlob::Seek(int64_t offset, int32_t whence)
{
	SceInt64 position = 0;

	switch (whence) {
	case SCE_SEEK_SET:
		position = (int)offset;
		break;
	case SCE_SEEK_CUR:
		position = m_offset + (int)offset;
		break;
	case SCE_SEEK_END:
		position = (int)GetFileSize() + (int)offset;
		break;
	}
	if (position >= 0)
		m_offset = (int)position;
	return m_offset;
}

int32_t DbBlob::SeekAsync(int64_t offset, int32_t whence)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t DbBlob::Flush()
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t DbBlob::FlushAsync()
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t DbBlob::WaitAsync(int64_t *result)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

int32_t DbBlob::PollAsync(int64_t *result)
{
	return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
}

/*
 * The database's exports: opening and closing it, its language and configuration text, and
 * the system's region, language, country and the user's age it is kept for.
 */

/* NP's content rating, or the function SceLsdb_AADCDCFC sets in its place */
typedef int (*LsdbContentRatingFunction)(int *isRestricted, int *age);

static LsdbContentRatingFunction s_contentRatingFunction = NULL;
static paf::rtc::DateTime s_birthDate;

extern "C" {

/* opens the database for state (see LsdbDatabase::Open) */
int SceLsdb_2944E643(int state)
{
	int result = LsdbDatabase::GetInstance()->Open(state, false);

	LsdbTimeInit();
	return result;
}

int SceLsdb_8A288259(void)
{
	LsdbTimeTerm();
	return LsdbDatabase::GetInstance()->Close();
}

/* closes the database and opens an empty one in memory in its place */
int SceLsdb_F3A75448(void)
{
	LsdbTimeTerm();
	LsdbDatabase::GetInstance()->Close();
	return LsdbDatabase::GetInstance()->Open(0, true);
}

int SceLsdb_C6678B98(int language)
{
	return LsdbDatabase::GetInstance()->SetLanguage(language);
}

int SceLsdb_0CF73C33(void)
{
	return LsdbDatabase::GetInstance()->GetLanguage();
}

int SceLsdb_6AFF7E71(void)
{
	return LsdbDatabase::GetInstance()->ResetEvents();
}

int SceLsdb_B0E42B67(void)
{
	return LsdbDatabase::GetInstance()->GetOpenResult();
}

int SceLsdb_D4B1C54E(paf::string *text)
{
	return LsdbDatabase::GetInstance()->GetConfigText(text);
}

int SceLsdb_8D08C3E7(const paf::string *text)
{
	return LsdbDatabase::GetInstance()->SetConfigText(text);
}

/* the system language (paf's locale), English without paf */
int SceLsdb_68059E87(void)
{
	int language = 1;
	paf::Framework *framework = paf::Framework::Instance();

	if (framework != NULL)
		language = framework->GetLocale();
	return language;
}

}

int LsdbGetRegion(void)
{
	int region = sceRegMgrSrvGetRegion();

	if (region <= 0)
		return 0;
	return region;
}

extern "C" {

/* the region's name, empty when it has none */
paf::string SceLsdb_40C684B8(void)
{
	char name[5] = "";
	const char *region = NULL;

	if (sceRegMgrSrvGetRegionStr(name, sizeof(name)) > 0)
		region = name;
	return paf::string(region);
}

/*
 * A language: with source 1, NP's (-1 without one); otherwise language, the system's when it
 * is not one (0 to 19).
 */
int SceLsdb_60B535FD(int source, int language)
{
	if (source == 1) {
		char lang[6];

		if (sceRegMgrGetKeyStr("/CONFIG/NP", "lang", lang, sizeof(lang)) != 0)
			return -1;

		paf::string code(lang);

		return LsdbLanguageFromCode(&code);
	}
	if (language < 0 || language > 19)
		return SceLsdb_68059E87();
	return language;
}

/* NP's language code, empty without one */
paf::string SceLsdb_1CDFECB7(void)
{
	char lang[6] = "";

	if (sceRegMgrGetKeyStr("/CONFIG/NP", "lang", lang, sizeof(lang)) != 0)
		return paf::string((const char *)NULL);
	return paf::string(lang);
}

/* the country code: with source 2, the mobile network's when it has one, else NP's */
paf::string SceLsdb_88FDC55C(int source)
{
	if (source == 2) {
		paf::Plugin *plugin = paf::Plugin::Find("telephony_plugin");

		if (plugin != NULL) {
			TelephonyPluginInterface *iface =
				(TelephonyPluginInterface *)plugin->GetInterface(1);

			if (iface != NULL) {
				TelephonyModem *modem = iface->getModem();

				if (modem != NULL) {
					paf::string country;

					if (modem->vtable->getCountryCode(modem, &country) == 0)
						return country;
				}
			}
		}
	}

	char country[3] = "";

	if (sceRegMgrGetKeyStr("/CONFIG/NP", "country", country, sizeof(country)) != 0)
		return paf::string((const char *)NULL);
	return paf::string(country);
}

bool SceLsdb_69B32F6C(int value)
{
	return value != 0;
}

void SceLsdb_AADCDCFC(void *function)
{
	s_contentRatingFunction = (LsdbContentRatingFunction)function;
}

/*
 * The user's age: NP's content rating age (-1 without a restriction), else from the birth date
 * in the registry and the network time; 0 when neither is known.
 */
int SceLsdb_1673AD48(void)
{
	int age = 0;
	int isRestricted = 0;
	int year;
	int month;
	int day;
	paf::rtc::Tick now;
	paf::rtc::Tick birth;
	paf::rtc::DateTime today;
	paf::rtc::DateTime birthDate;
	int result;

	if (s_contentRatingFunction != NULL)
		result = s_contentRatingFunction(&isRestricted, &age);
	else
		result = sceNpManagerGetContentRatingFlag(&isRestricted, &age);
	if (result == 0)
		return isRestricted ? age : -1;
	result = sceRegMgrGetKeyInt("/CONFIG/NP", "yob", &year);
	if (result == 0) {
		result = sceRegMgrGetKeyInt("/CONFIG/NP", "mob", &month);
		if (result == 0)
			result = sceRegMgrGetKeyInt("/CONFIG/NP", "dob", &day);
	}
	if (result != 0)
		return 0;
	if (SceLsdb_76C1997A(&now) != 0)
		return 0;
	birth = 0;
	birthDate = s_birthDate;
	birthDate.year = year;
	birthDate.month = month;
	birthDate.day = day;
	if (paf::rtc::ConvertDateTimeToTick(&birthDate, &birth) != 0 || birth > now)
		return 0;
	if (paf::rtc::ConvertTickToDateTime(&now, &today, NULL) != 0)
		return 0;
	age = today.year - year;
	if (today.month < month || (today.month == month && today.day < day))
		age--;
	return age;
}

/* NP's environment, empty without one */
paf::string SceLsdb_C2B7D81B(void)
{
	char env[17];

	if (sceRegMgrGetKeyStr("/CONFIG/NP", "env", env, sizeof(env)) != 0)
		return paf::string((const char *)NULL);
	return paf::string(env);
}

}

/* whether the application database is to be made again (the shell's registry) */
int LsdbGetAppDbRebuildFlag(void)
{
	int flag = 0;
	int result = sceRegMgrGetKeyInt("/CONFIG/SHELL", "appdb_rebuild_flag", &flag);

	if (result >= 0)
		result = flag;
	return result;
}

int LsdbSetAppDbRebuildFlag(int flag)
{
	return sceRegMgrSetKeyInt("/CONFIG/SHELL", "appdb_rebuild_flag", flag);
}
