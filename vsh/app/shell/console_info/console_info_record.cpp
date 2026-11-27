/*
 * console_info_record.cpp: a kind's record of the console information (0x810E5C7C-0x810E74FC):
 * its folders (ur0:temp/ci/<kind>/<record>, ur0:ci/prev/<kind>), its entry files, each a header of
 * "name=value" fields joined by '&' and then the record's lines, closed by their count, the time
 * stamp of the last record closed, and the header fields' values: the system software's version,
 * the console's codes and IDs, the NP account. The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/string.h>
#include <std/stdio.h>
#include <common/shared_ptr.h>
#include <file/file.h>
#include <file/localfile.h>
#include <dir/dir.h>
#include <rtc/rtc.h>
#include <kernel/iofilemgr.h>
extern "C" {
#include <kernel/openpsid.h>
}
#include <ss_mgr/ss_mgr.h>
#include <update_mgr/update_mgr.h>
#include <vshbridge/vshbridge.h>
#include "console_info.h"

using namespace paf;

extern "C" int SceNpManager_CFAEB050(void *accountId);

/* the package whose version the header gives (PLACEHOLDER name) */
#define CONSOLE_INFO_SPKG_TYPE_SYSTEM	1

/* the NP account's IDs (0x81555F38), the header line being written (0x81555F58) */
static char s_header[CONSOLE_INFO_BUFFER_SIZE];
static SceUInt64 s_accountId[4];

/* where in the entry the field being written starts, and where its "pn=" and "nd=" values are
   (0x81544488, 0x8154448C, 0x81544490) */
static int s_fieldOffset = 0;
static int s_pageOffset = 0;
static int s_lineCountOffset = 0;

/* (0x810E5C7C) the account's IDs, big-endian in NP's copy */
__attribute__((noinline)) int consoleInfoLoadAccountId(void)
{
	sce_paf_memset(s_accountId, 0, sizeof(s_accountId));

	int ret = SceNpManager_CFAEB050(s_accountId);

	if (ret == 0) {
		for (int i = 0; i < 4; i++)
			s_accountId[i] = consoleInfoSwap64(s_accountId[i]);
	}
	return ret;
}

/* (0x810E5D38) */
int consoleInfoMakeDir(const char *path)
{
	if (!Dir::IsExist(path)) {
		int ret = Dir::CreateRecursive(path);

		if (ret != 0)
			return ret;
	}
	return 0;
}

/* (0x810E5D58) */
__attribute__((noinline)) const char *consoleInfoTypeName(int type)
{
	switch (type) {
	case CONSOLE_INFO_TYPE_NS:
		return "ns";
	case CONSOLE_INFO_TYPE_CI:
		return "ci";
	case CONSOLE_INFO_TYPE_MI:
		return "mi";
	case CONSOLE_INFO_TYPE_UC:
		return "uc";
	}
	return NULL;
}

/* (0x810E5DA0) ur0:ci/prev/<kind> */
int ConsoleInfoRecord::GetPrevDir(string *path)
{
	path->clear();
	*path += CONSOLE_INFO_PREV_DIR;
	*path += "/";
	*path += consoleInfoTypeName(m_type);
	return 0;
}

/* (0x810E5EDE) ur0:temp/ci/<kind>/<record> */
int ConsoleInfoRecord::GetTempDir(string *path)
{
	char name[9];

	path->clear();
	*path += CONSOLE_INFO_TEMP_DIR;
	*path += "/";
	*path += consoleInfoTypeName(m_type);
	*path += "/";
	sce_paf_snprintf(name, sizeof(name), "%p", this);
	*path += name;
	return 0;
}

/* (0x810E6148) */
int ConsoleInfoRecord::MakeDirs()
{
	string temp;

	GetTempDir(&temp);

	int ret = consoleInfoMakeDir(temp.c_str());

	if (ret != 0)
		return ret;

	string prev;

	GetPrevDir(&prev);
	ret = consoleInfoMakeDir(prev.c_str());
	if (ret != 0)
		return ret;
	return 0;
}

/* (0x810E623E) */
ConsoleInfoRecord::ConsoleInfoRecord(int type, int entrySize)
	: m_type(type), m_entrySize(entrySize), m_count(0), m_remaining(0)
{
	MakeDirs();
	consoleInfoLoadAccountId();
}

/* (0x810E6272, 0x810E62D6) */
ConsoleInfoRecord::~ConsoleInfoRecord()
{
}

/* (0x810E62F0) */
int ConsoleInfoRecord::OpenEntryFile(unsigned int index, common::SharedPtr<File> *file,
				     uint32_t flags)
{
	if (index > 0xFFFF)
		return CONSOLE_INFO_ERROR_INVALID_INDEX;

	string path;

	path = GetEntryPath(index);

	int err = 0;

	*file = LocalFile::Open(path.c_str(), flags, CONSOLE_INFO_FILE_MODE, &err);
	if (err != 0)
		return err;
	return 0;
}

/* (0x810E646C) an entry read; the entries of the console and machine information get their
   "pn=" page and count */
int ConsoleInfoRecord::ReadEntry(int index, void *buf, int *size)
{
	char page[16];
	common::SharedPtr<File> file;
	int ret = OpenEntryFile(index, &file, SCE_O_RDONLY);

	if (ret != 0)
		return ret;

	int read = file->Read(buf, m_entrySize);

	file->Close();
	if (read < 0)
		return read;
	if (m_type == CONSOLE_INFO_TYPE_CI || m_type == CONSOLE_INFO_TYPE_MI) {
		sce_paf_snprintf(page, 13, "pn=%04x,%04x", index + 1, m_count);
		sce_paf_memcpy((char *)buf + s_pageOffset, page, 12);
	}
	*size = read;
	return 0;
}

/* (0x810E660C) ur0:ci/prev/<kind>/complete_time.dat */
int ConsoleInfoRecord::OpenTimestampFile(common::SharedPtr<File> *file, uint32_t flags)
{
	string path;

	GetPrevDir(&path);
	path += "/";
	path += "complete_time.dat";

	int err = 0;

	*file = LocalFile::Open(path.c_str(), flags, CONSOLE_INFO_FILE_MODE, &err);
	if (err != 0) {
		if (err == CONSOLE_INFO_ERROR_NOT_FOUND)
			return CONSOLE_INFO_ERROR_NOT_FOUND;
		return err;
	}
	return 0;
}

/* (0x810E6842) */
int ConsoleInfoRecord::WriteTimestamp()
{
	rtc::Tick tick;
	int ret = rtc::GetCurrentTick(&tick);

	if (ret != 0)
		return ret;

	common::SharedPtr<File> file;

	ret = OpenTimestampFile(&file, CONSOLE_INFO_FILE_WRITE);
	if (ret != 0)
		return ret;

	int written = file->Write(&tick, sizeof(tick));

	file->Close();
	if (written < 0)
		return written;
	return 0;
}

/* (0x810E6998) 0 when no record was closed yet */
int ConsoleInfoRecord::ReadTimestamp(SceUInt64 *tick)
{
	common::SharedPtr<File> file;
	int ret = OpenTimestampFile(&file, SCE_O_RDONLY);

	if (ret != 0) {
		if (ret == CONSOLE_INFO_ERROR_NOT_FOUND) {
			*tick = 0;
			return 0;
		}
		return ret;
	}

	int read = file->Read(tick, sizeof(*tick));

	file->Close();
	if (read < 0)
		return read;
	return 0;
}

/* (0x810E6B48) */
int ConsoleInfoRecord::RemoveTempDir()
{
	int ret = Dir::RemoveRecursive(CONSOLE_INFO_TEMP_DIR);

	if (ret != 0)
		return ret;
	return 0;
}

/* (0x810E6B62) the next entry's file */
int ConsoleInfoRecord::OpenNewEntry()
{
	if (m_file.get() != NULL)
		return CONSOLE_INFO_ERROR_NOT_INITIALIZED;

	int ret = OpenEntryFile(m_count, &m_file, CONSOLE_INFO_FILE_WRITE);

	if (ret < 0)
		return ret;
	m_count++;
	m_remaining = m_entrySize;
	m_written = 0;
	return 0;
}

/* (0x810E6BA0) */
int ConsoleInfoRecord::CloseEntry()
{
	if (m_file.get() == NULL)
		return CONSOLE_INFO_ERROR_NOT_INITIALIZED;

	m_file->Close();
	m_file.reset();
	return 0;
}

/* (0x810E6C0C) the header's fields, in a new entry when one does not fit */
int ConsoleInfoRecord::WriteHeader()
{
	if (m_file.get() == NULL)
		return CONSOLE_INFO_ERROR_NOT_INITIALIZED;

	unsigned int count = GetFieldCount();
	const Field *field = GetFields();

	for (unsigned int i = 0; i < count; i++, field++) {
		int len = field->maxLen + sce_paf_strlen(field->name) + 2;
		int ret;

		if (m_remaining < len) {
			CloseEntry();
			ret = OpenNewEntry();
			if (ret != 0)
				return ret;
		} else if (i != 0) {
			ret = m_file->Write("&", 1);
			if (ret < 0)
				return ret;
			m_remaining--;
		}

		s_fieldOffset = m_entrySize - m_remaining;
		sce_paf_snprintf(s_header, sizeof(s_header), "%s=", field->name);

		int valueLen = 0;
		int nameLen = sce_paf_strlen(s_header);

		ret = field->format(field, s_header + nameLen, &valueLen);
		if (ret >= 0) {
			ret = 0;
			len = nameLen + valueLen;
		}
		if (ret != 0)
			return ret;
		ret = m_file->Write(s_header, len);
		if (ret < 0)
			return ret;
		m_remaining -= len;
	}
	return 0;
}

/* (0x810E6D7A) */
int ConsoleInfoRecord::NewEntry()
{
	int ret = OpenNewEntry();

	if (ret != 0)
		return ret;
	ret = WriteHeader();
	if (ret != 0)
		return ret;
	ret = m_file->Write("&", 1);
	if (ret < 0)
		return ret;
	return 0;
}

/* (0x810E6DDC) the entry's count of lines written into its "nd=" field */
int ConsoleInfoRecord::FinishEntry()
{
	if (m_file.get() == NULL)
		return CONSOLE_INFO_ERROR_NOT_INITIALIZED;

	SceInt64 ret = m_file->Seek(s_lineCountOffset, SCE_SEEK_SET);

	if (ret < 0)
		return ret;

	char count[6];

	sce_paf_snprintf(count, sizeof(count), "nd=%02x", m_written);
	ret = m_file->Write(count, 5);
	if (ret < 0)
		return ret;
	ret = m_file->Seek(0, SCE_SEEK_END);
	if (ret < 0)
		return ret;
	return 0;
}

/* (0x810E6E90) */
int ConsoleInfoRecord::FinishAndCloseEntry()
{
	int ret = FinishEntry();

	if (ret != 0)
		return ret;

	int err = CloseEntry();

	if (err != 0)
		return err;
	return ret;
}

/* (0x810E6EAC) the line in m_line written, in a new entry when it does not fit */
int ConsoleInfoRecord::AppendLine(SceSize len, SceUInt8 count)
{
	if (m_file.get() == NULL)
		return CONSOLE_INFO_ERROR_NOT_INITIALIZED;

	int separator = 0;
	int ret;

	if (m_remaining < len + 10) {
		ret = FinishAndCloseEntry();
		if (ret < 0)
			return ret;
		ret = NewEntry();
		if (ret != 0)
			return ret;
	} else if (m_written != 0) {
		ret = m_file->Write("&", 1);
		if (ret < 0)
			return ret;
		separator = 1;
	}
	ret = m_file->Write(m_line, len);
	if (ret < 0)
		return ret;
	m_remaining -= len + separator;
	m_written += count;
	return 0;
}

/* (0x810E6F48) */
int consoleInfoGetSystemVersion(int *version)
{
	SceSblUsSpkgInfo info;
	int ret = sceSblUsGetSpkgInfo(CONSOLE_INFO_SPKG_TYPE_SYSTEM, &info);

	if (ret != 0)
		return ret;
	*version = info.version;
	return 0;
}

/* (0x810E6F80) */
int consoleInfoFormatSystemVersion(const ConsoleInfoRecord::Field *field, char *buf, int *len)
{
	SceSblUsSpkgInfo info;
	int ret = sceSblUsGetSpkgInfo(CONSOLE_INFO_SPKG_TYPE_SYSTEM, &info);

	if (ret != 0)
		return ret;
	sce_paf_snprintf(buf, 9, "%08x", info.version);
	*len = 8;
	return 0;
}

/* (0x810E6FD0) */
int consoleInfoFormatPsCode(const ConsoleInfoRecord::Field *field, char *buf, int *len)
{
	ScePsCode code;

	sce_paf_memset(&code, 0, sizeof(code));

	int ret = vshSblAimgrGetPscode(&code);

	if (ret != 0)
		return ret;
	sce_paf_snprintf(buf, 5, "%04x", code.product_code);
	*len = 4;
	return 0;
}

/* (0x810E7028) */
int consoleInfoFormatOpenPsId(const ConsoleInfoRecord::Field *field, char *buf, int *len)
{
	SceKernelOpenPsId openPsId;

	sce_paf_memset(&openPsId, 0, sizeof(openPsId));

	int ret = sceKernelGetOpenPsId(&openPsId);

	if (ret != 0)
		return ret;

	SceUInt64 id[2];

	sce_paf_memcpy(id, &openPsId, sizeof(id));
	id[0] = consoleInfoSwap64(id[0]);
	id[1] = consoleInfoSwap64(id[1]);
	sce_paf_snprintf(buf, 33, "%016llx%016llx", id[0], id[1]);
	*len = 32;
	return 0;
}

/* (0x810E7188) */
int consoleInfoFormatPlatform(const ConsoleInfoRecord::Field *field, char *buf, int *len)
{
	switch (consoleInfoGetPlatform()) {
	case 1:
		buf[0] = '1';
		break;
	case 2:
		buf[0] = '2';
		break;
	default:
		buf[0] = '1';
		break;
	}
	buf[1] = '\0';
	*len = 1;
	return 0;
}

/* (0x810E71BA) */
int consoleInfoFormatAccountId(const ConsoleInfoRecord::Field *field, char *buf, int *len)
{
	sce_paf_snprintf(buf, 65, "%016llx%016llx%016llx%016llx", s_accountId[0], s_accountId[1],
			 s_accountId[2], s_accountId[3]);
	*len = 64;
	return 0;
}

/* (0x810E7206) */
int consoleInfoFormatTelephony(const ConsoleInfoRecord::Field *field, char *buf, int *len)
{
	if (consoleInfoGetPlatform() == 2) {
		buf[0] = '2';
	} else {
		int active = 0;
		int ret = consoleInfoGetTelephony(&active);

		if (ret != 0)
			return ret;
		buf[0] = active ? '1' : '0';
	}
	buf[1] = '\0';
	*len = 1;
	return 0;
}

/* (0x810E7260) the version, account, Open PSID and product code, joined by ',' */
int consoleInfoFormatConsole(const ConsoleInfoRecord::Field *field, char *buf, int *len)
{
	int pos = 0;
	int n;
	int ret = consoleInfoFormatSystemVersion(NULL, buf, &n);

	if (ret != 0)
		return ret;
	pos += n;
	buf[pos++] = ',';
	consoleInfoFormatAccountId(NULL, buf + pos, &n);
	pos += n;
	buf[pos++] = ',';
	ret = consoleInfoFormatOpenPsId(NULL, buf + pos, &n);
	if (ret != 0)
		return ret;
	pos += n;
	buf[pos++] = ',';
	ret = consoleInfoFormatPsCode(NULL, buf + pos, &n);
	if (ret != 0)
		return ret;
	pos += n;
	buf[pos] = '\0';
	*len = pos;
	return 0;
}

/* (0x810E7390) the page's place, filled by ReadEntry */
int consoleInfoFormatPage(const ConsoleInfoRecord::Field *field, char *buf, int *len)
{
	s_pageOffset = s_fieldOffset;
	buf[0] = '0';
	buf[1] = '0';
	buf[2] = '0';
	buf[3] = '1';
	buf[4] = ',';
	buf[5] = '0';
	buf[6] = '0';
	buf[7] = '0';
	buf[8] = '1';
	buf[9] = '\0';
	*len = 9;
	return 0;
}

/* (0x810E73C8) the count's place, filled by FinishEntry */
int consoleInfoFormatLineCount(const ConsoleInfoRecord::Field *field, char *buf, int *len)
{
	buf[0] = '0';
	buf[1] = '0';
	s_lineCountOffset = s_fieldOffset;
	buf[2] = '\0';
	*len = 2;
	return 0;
}

/* (0x810E73EC) ur0:temp/ci/<kind>/<record>/<index> */
string ConsoleInfoRecord::GetEntryPath(int index)
{
	string path;
	char name[5];

	GetTempDir(&path);
	path += "/";
	sce_paf_snprintf(name, sizeof(name), "%04x", index);
	path += name;
	return path;
}

/* (0x81409E2C: one "return 0" the build shares) the base record has no header */
const ConsoleInfoRecord::Field *ConsoleInfoRecord::GetFields()
{
	return NULL;
}

unsigned int ConsoleInfoRecord::GetFieldCount()
{
	return 0;
}
