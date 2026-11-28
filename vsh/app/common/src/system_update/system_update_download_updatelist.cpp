/*
 * system_update_download_updatelist.cpp: the jobs that download the update list
 * (0x8112F2D0-0x81130840): the list read over HTTP or the PC's MTP connection and parsed into
 * the list, the image chosen from its first entry, and the service's job that also asks for
 * the image's size and date.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <file/httpfile.h>
#include <libnetctl.h>
#include <updater_core/updater_core.h>

#include "system_update_download_updatelist.h"
#include "system_update_file.h"
#include "system_update_registry.h"

using namespace paf;

#define SYSTEM_UPDATE_ERROR_CANCELLED	((int)0x801825F6)	/* PLACEHOLDER names */
#define SYSTEM_UPDATE_ERROR_NO_ENTRY	((int)0x801825F7)
#define SYSTEM_UPDATE_ERROR_HTTP	((int)0x801824C0)
#define SYSTEM_UPDATE_ERROR_TYPE	((int)0x80182420)
#define SYSTEM_UPDATE_ERROR_NO_IMAGE	((int)0x80182421)
#define SYSTEM_UPDATE_ERROR_MTP1	((int)0x801824E2)
#define SYSTEM_UPDATE_ERROR_MTP2	((int)0x801824E1)
#define MTP_HTTP_ERROR_CONNECT1		((int)0x8054001F)	/* PLACEHOLDER names */
#define MTP_HTTP_ERROR_CONNECT2		((int)0x80540020)

#define UPDATE_LIST_SIZE_MAX		0xFFFF

static char s_buffer[2048];				/* (0x815642A8) the list as it is read */

static int Bgdl_81130828(void *arg);

SystemUpdateDownloadJob::SystemUpdateDownloadJob(SystemUpdateListener *listener,
						 SystemUpdateList *list, int type)
	: SystemUpdateJob(listener), m_list(list)
{
	m_list->list.clear();
	m_file.reset();
	m_type = type;
	m_func = NULL;
	m_arg = NULL;
}

SystemUpdateDownloadJob::~SystemUpdateDownloadJob()
{
}

/* the list's file made and read */
int SystemUpdateDownloadJob::Run()
{
	File *file = NULL;

	if (m_type == 0)
		file = new HttpFile();
	else if (m_type == 1 || m_type == 2)
		file = new MtpHttpFile();

	common::SharedPtr<File> ptr(file);

	m_file = ptr;

	SystemUpdateResult result = Prepare();

	if (result.error == 0) {
		bool reserved = false;

		result = Download(&reserved);
		if (result.error == 0)
			result = Parse();
	}
	SetResult(result);
	return 0;
}

void SystemUpdateDownloadJob::Cancel()
{
	SystemUpdateJob::Cancel();
	if (m_file.get() != NULL)
		m_file->Abort();
}

void SystemUpdateDownloadJob::OnFinished(int result)
{
	common::SlotJobItem::Finish(result);
}

/* (folded into SystemUpdateListJob's) */
void SystemUpdateDownloadJob::Finish(int result)
{
	SetResult(GetResult());
	OnFinished(result);
}

/* the list read from its server and parsed into the list */
SystemUpdateResult SystemUpdateDownloadJob::Download(bool *reserved)
{
	SystemUpdateResult result;

	if (m_list == NULL) {
		result.error = SYSTEM_UPDATE_ERROR;
		result.detail = 0;
		return result;
	}
	if (m_file.get() == NULL) {
		result.error = SYSTEM_UPDATE_ERROR;
		result.detail = 0;
		return result;
	}

	string url;

	result = Bgdl_81132EDA(0, &url);
	if (result.error != 0)
		return result;
	if (m_cancelled) {
		result.error = SYSTEM_UPDATE_ERROR_CANCELLED;
		return result;
	}

	common::SharedPtr<File> file = m_file;

	result = Fetch(file, url);
	if (result.error != 0)
		return result;

	sce_paf_memset(s_buffer, 0, sizeof(s_buffer));

	string unusedPath;
	vector<SystemUpdateData> unusedList;
	UpdateListParser parser;
	SceInt64 size = file->GetFileSize();
	bool isUnboundedSize = false;

	if (size < 0) {
		if (size == SCE_PAF_ERROR_FILE_UNKNOWN_SIZE) {
			size = UPDATE_LIST_SIZE_MAX;
			isUnboundedSize = true;
		} else {
			result.error = SYSTEM_UPDATE_ERROR;
			result.detail = (int)size;
		}
	} else if (size <= 0) {
		result.error = SYSTEM_UPDATE_ERROR;
	}

	if (result.error == 0) {
		SceUInt32 total = (SceUInt32)size;

		if (size > UPDATE_LIST_SIZE_MAX) {
			result.error = SYSTEM_UPDATE_ERROR;
		} else {
			UpdateListReader reader(m_list, UPDATE_LIST_SIZE_MAX);
			SceInt64 read = 0;
			bool ended = false;

			reader.Bgdl_81134D70();
			parser.Bgdl_81135D9C(&reader);
			while (total > read) {
				if (m_cancelled == true)
					break;

				int n = file->Read(s_buffer, sizeof(s_buffer));

				if (n <= 0) {
					if (n != 0 || !isUnboundedSize) {
						if (m_type == 0)
							result.error = SYSTEM_UPDATE_ERROR_HTTP;
						if (m_type == 1 || m_type == 2)
							result.error = Bgdl_81130C9C(m_type, n);
						if (n == 0)
							result.error = SYSTEM_UPDATE_ERROR;
						result.detail = n;
					} else {
						ended = true;
					}
					break;
				}
				read += n;
				result = parser.Bgdl_81135E32(s_buffer, n);
				if (result.error != 0)
					break;
			}
			parser.Bgdl_81135E1A();
			if (result.error == 0) {
				if (isUnboundedSize) {
					if (!ended && total != read)
						result.error = SYSTEM_UPDATE_ERROR;
				} else if (total != read) {
					result.error = SYSTEM_UPDATE_ERROR;
				}
			}
		}
	}
	file->Close();
	SetResult(result);
	return result;
}

SystemUpdateResult SystemUpdateDownloadJob::Prepare(void)
{
	SystemUpdateResult result;

	result.error = 0;
	result.detail = 0;
	return result;
}

/* (folded into Prepare) */
SystemUpdateResult SystemUpdateDownloadJob::Parse(void)
{
	SystemUpdateResult result;

	result.error = 0;
	result.detail = 0;
	return result;
}

/* (folded into Prepare) */
SystemUpdateResult SystemUpdateListDownloadJob::Bgdl_8112FB08(void)
{
	SystemUpdateResult result;

	result.error = 0;
	result.detail = 0;
	return result;
}

/* the list's file opened at url */
SystemUpdateResult SystemUpdateDownloadJob::Fetch(common::SharedPtr<File> file, string url)
{
	common::SharedPtr<File> opened = file;
	SystemUpdateResult result;
	int ret = 0;

	result.error = 0;
	result.detail = 0;
	if (opened.get() == NULL) {
		result.error = SYSTEM_UPDATE_ERROR;
		result.detail = 0;
		return result;
	}

	if (m_type == 0) {
		HttpFile::OpenArg arg;

		if (Bgdl_811330A6() == 0)
			arg.open_flag = HttpFile::OpenArg::SUPPRESS_3G_ACCESS;
		arg.ParseUrl(url.c_str());
		ret = opened->Open(&arg);
		result = Bgdl_81130D9E(opened, ret, UPDATE_LIST_SIZE_MAX);
		if (result.error != 0)
			return result;
	} else if (m_type == 1 || m_type == 2) {
		MtpHttpFileOpenArg arg;

		arg.file_type = MTP_HTTP_FILE_TYPE;
		arg.bufferPad = result.error;
		arg.offset = 0;
		arg.bufferSize = 0x10000;
		arg.options[0] = 0x10000100;
		arg.options[1] = result.error;
		arg.options[2] = result.error;
		arg.url = url.c_str();
		arg.kind = m_type == 1 ? 1 : 2;
		ret = opened->Open(&arg);
	}

	if (ret != 0) {
		if (m_type == 0) {
			result.error = SYSTEM_UPDATE_ERROR_HTTP;
		} else if (m_type == 1 || m_type == 2) {
			result.error = Bgdl_81130C9C(m_type, ret);
			if (ret == MTP_HTTP_ERROR_CONNECT1 || ret == MTP_HTTP_ERROR_CONNECT2) {
				if (m_type == 1)
					result.error = SYSTEM_UPDATE_ERROR_MTP1;
				if (m_type == 2)
					result.error = SYSTEM_UPDATE_ERROR_MTP2;
			}
		}
		result.detail = ret;
		return result;
	}

	if (m_type == 0 && m_listener == NULL && m_func != NULL && Bgdl_8126E2AE(2) == 0) {
		SceNetCtlInfo info;

		ret = sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_DEVICE, &info);
		if (ret != 0) {
			result.error = SYSTEM_UPDATE_ERROR_HTTP;
			result.detail = ret;
		} else if (info.device == SCE_NET_CTL_DEVICE_PHONE) {
			thread::SyncCall::main_thread_synccall.Call(Bgdl_81130828, this);
		}
	}
	return result;
}

/* the image of data to download: one of a kind installed, else the one for any */
SystemUpdateResult Bgdl_8112FFEA(SystemUpdateData *data, string *url, SceInt64 *size)
{
	SystemUpdateResult result;
	string kindUrl = "";
	string anyUrl = "";
	SceInt64 anySize = 0;
	SceInt64 kindSize = 0;
	bool haveAny = false;
	bool haveKind = false;

	result.error = 0;
	result.detail = 0;
	for (SceUInt32 i = 0; i < data->images.size(); i++) {
		SystemUpdateImage *image = &data->images[i];

		if (image->kind == 0 && haveAny != true) {
			anyUrl = image->url;
			anySize = data->images[i].size;
			haveAny = true;
		} else {
			int kind = 0;
			int ret = sceUpdaterNetworkCheckBaseVersion((SceUInt32)image->type, &kind);

			if (ret != 0) {
				result.error = SYSTEM_UPDATE_ERROR;
				result.detail = ret;
				return result;
			}
			if (kind == 1 && haveKind != true) {
				kindUrl = data->images[i].url;
				haveKind = true;
				kindSize = data->images[i].size;
			}
		}
	}

	bool installed = false;
	int ret = Bgdl_81135CA0(&installed);

	if (ret < 0) {
		result.error = SYSTEM_UPDATE_ERROR;
		result.detail = ret;
		return result;
	}
	if (kindUrl.length() != 0 && installed == true) {
		*url = kindUrl;
		*size = kindSize < 0 ? 0 : kindSize;
	} else if (anyUrl.length() != 0) {
		*url = anyUrl;
		*size = anySize < 0 ? 0 : anySize;
	} else {
		result.error = SYSTEM_UPDATE_ERROR_NO_IMAGE;
	}
	return result;
}

/* the list's first entry: its image's url and its version given */
void SystemUpdateListDownloadJob::Finish(int result)
{
	SystemUpdateResult res = GetResult();
	string url = "";
	SceInt64 size = 0;

	if (res.error == 0) {
		if (m_list->list.size() != 0) {
			res = CheckType(m_list->list[0].type);
			if (res.error == 0) {
				res = Bgdl_8112FFEA(&m_list->list[0], &url, &size);
				if (res.error == 0) {
					Bgdl_811363D6(size);
					*m_version = m_list->list[0].version;
					*m_url = url;
				}
			}
		} else {
			res.error = SYSTEM_UPDATE_ERROR_NO_ENTRY;
		}
	}

	SystemUpdateResult ended = Bgdl_8112FB08();

	if (res.error == 0 && ended.error != 0)
		res.error = ended.error;
	SetResult(res);
	OnFinished(result);
}

/* an entry's kind of update: one the system takes */
SystemUpdateResult SystemUpdateListDownloadJob::CheckType(int type)
{
	SystemUpdateResult result;
	int error = 0;
	int kind = 0;
	int ret = sceUpdaterNetworkPreCheckVersion((SceUInt32)type, &kind);

	if (ret != 0) {
		error = Bgdl_81130840(ret, NULL);
	} else if (kind == 1) {
		bool ok;

		ret = Bgdl_81135CA8(type, &ok);
		if (ret < 0) {
			error = Bgdl_81130840(ret, NULL);
		} else {
			ret = 0;
			if (!ok)
				error = SYSTEM_UPDATE_ERROR_TYPE;
		}
	} else {
		error = SYSTEM_UPDATE_ERROR_TYPE;
		ret = 0;
	}
	result.error = error;
	result.detail = ret;
	return result;
}

SystemUpdateListJob::SystemUpdateListJob(SystemUpdateListener *listener, string *version,
					 string *url, SceInt64 *size, string *lastModified,
					 SystemUpdateList *list, int type)
	: SystemUpdateListDownloadJob(listener, list, type)
{
	m_url = url;
	m_version = version;
	m_size = size;
	m_lastModified = lastModified;
}

void SystemUpdateListJob::OnFinished(int result)
{
	if (m_listener != NULL)
		m_listener->Bgdl_8112CFD8(GetResult());
}

void SystemUpdateListJob::Finish(int result)
{
	SetResult(GetResult());
	OnFinished(result);
}

/* the image's size and date asked of its server */
SystemUpdateResult SystemUpdateListJob::Bgdl_81130466(const char *url, SceInt64 *size,
						      string *lastModified, SceInt64 knownSize)
{
	SystemUpdateResult result;
	int error = 0;
	int detail = 0;
	int ret;
	common::SharedPtr<HttpFile> file = HttpFile::Open(url, 1, 0, &ret);

	if (file.get() == NULL || ret != 0) {
		if (file.get() != NULL && file->IsOpened() == true)
			file->Close();
		result.error = SYSTEM_UPDATE_ERROR_HTTP;
		result.detail = ret;
		return result;
	}

	SceInt64 fileSize = file->GetFileSize();

	if (fileSize == SCE_PAF_ERROR_FILE_UNKNOWN_SIZE && knownSize > 0)
		fileSize = knownSize;
	if (fileSize <= 0) {
		error = SYSTEM_UPDATE_ERROR;
		detail = (int)fileSize;
	} else {
		*size = fileSize;

		string date = "";
		int ret2 = file->GetResponseHeader("Last-Modified", date);

		if (ret2 != 0) {
			error = SYSTEM_UPDATE_ERROR;
			detail = ret2;
		} else {
			*lastModified = date;
		}
	}
	result.error = error;
	result.detail = detail;
	return result;
}

/* the first entry's image, its size and its date */
SystemUpdateResult SystemUpdateListJob::Parse(void)
{
	SystemUpdateResult result;
	int detail = 0;
	string url = "";
	SceInt64 size = 0;

	if (m_list->list.size() != 0) {
		SystemUpdateResult res = CheckType(m_list->list[0].type);

		detail = res.detail;
		if (res.error == 0) {
			res = Bgdl_8112FFEA(&m_list->list[0], &url, &size);
			detail = res.detail;
			if (res.error == 0) {
				Bgdl_811363D6(size);
				*m_version = m_list->list[0].version;
				*m_url = url;
			}
		}
	}

	int error = SYSTEM_UPDATE_ERROR;

	if (m_url->length() != 0) {
		SystemUpdateResult res = Bgdl_81130466(m_url->c_str(), m_size, m_lastModified, size);

		error = res.error;
		detail = res.detail;
	}
	result.error = error;
	result.detail = detail;
	return result;
}

/* the kind checked, and the system's own version too */
SystemUpdateResult SystemUpdateListJob::CheckType(int type)
{
	SystemUpdateResult result;
	int error = 0;
	int kind = 0;
	int ret = sceUpdaterNetworkPreCheckVersion((SceUInt32)type, &kind);

	if (ret != 0) {
		error = Bgdl_81130840(ret, NULL);
	} else if (kind == 1) {
		bool ok;

		ret = Bgdl_81135CA8(type, &ok);
		if (ret < 0) {
			error = Bgdl_81130840(ret, NULL);
		} else {
			ret = 0;
			if (!ok)
				error = SYSTEM_UPDATE_ERROR_TYPE;
		}
	} else {
		error = SYSTEM_UPDATE_ERROR_TYPE;
		ret = 0;
	}
	if (error == 0) {
		bool newer = false;
		int ret2 = Bgdl_81135D24(type, &newer);

		if (ret2 < 0) {
			error = SYSTEM_UPDATE_ERROR;
			ret = ret2;
		} else if (newer != true) {
			error = SYSTEM_UPDATE_ERROR_TYPE;
		}
	}
	result.error = error;
	result.detail = ret;
	return result;
}

/* the function given called, on the main thread */
static int Bgdl_81130828(void *arg)
{
	SystemUpdateDownloadJob *job = (SystemUpdateDownloadJob *)arg;

	if (job == NULL)
		return 0;
	job->m_func(0, job->m_arg);
	return 0;
}
