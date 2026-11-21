/*
 * system_update_file.cpp: the system update's local files (0x81130840-0x811329B0): the errors
 * their calls and the downloads give, the files removed (or handed to the system update's own
 * process to remove), the update's information file made, written and read, and the update's
 * state taken from it.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <file/localfile.h>
#include <kernel/iofilemgr.h>

#include "system_update_file.h"

using namespace paf;

#define SYSTEM_UPDATE_ERROR_NO_FILE	((int)0x80182400)	/* PLACEHOLDER names */
#define SYSTEM_UPDATE_ERROR_NO_UPDATE	((int)0x80182404)
#define SYSTEM_UPDATE_ERROR_NO_PACKAGE	((int)0x80182413)
#define SYSTEM_UPDATE_ERROR_BROKEN	((int)0x80182421)
#define SYSTEM_UPDATE_ERROR_READ	((int)0x80182440)
#define SYSTEM_UPDATE_ERROR_WRITE	((int)0x80182441)
#define SYSTEM_UPDATE_ERROR_STORAGE	((int)0x80182460)	/* +1 internal, +2 the memory card */
#define SYSTEM_UPDATE_ERROR_NO_DEVICE	((int)0x80182481)	/* -1 internal, +1 the memory card */
#define SYSTEM_UPDATE_ERROR_NOT_THERE	((int)0x801824A0)
#define SYSTEM_UPDATE_ERROR_CHECK	((int)0x801824A1)
#define SYSTEM_UPDATE_ERROR_HTTP	((int)0x801824C0)
#define SYSTEM_UPDATE_ERROR_HTTP_TIMEOUT ((int)0x801824C1)
#define SYSTEM_UPDATE_ERROR_HTTP_STATUS	((int)0x801824C2)
#define SYSTEM_UPDATE_ERROR_HTTP_SIZE	((int)0x801824C3)
#define SYSTEM_UPDATE_ERROR_MTP		((int)0x801824E0)

/* where an update's file is (PLACEHOLDER names) */
#define SYSTEM_UPDATE_STORAGE_INTERNAL	0
#define SYSTEM_UPDATE_STORAGE_OTHER	1
#define SYSTEM_UPDATE_STORAGE_CARD	2

static bool s_useClient;				/* (0x81564AA8) */
static bool s_clientReady = true;			/* (0x8154489C) */
static SystemUpdateClient *s_client = NULL;		/* (0x815448A0) */

/* an error of the update's files or of its download, as the system update gives it */
int Bgdl_81130840(int error, const char *path)
{
	int storage = SYSTEM_UPDATE_STORAGE_OTHER;
	int ret;

	if (path == NULL)
		path = Bgdl_8137C484(0);

	string where(path);

	if (where.find("gro0:/PSP2/UPDATE") != string::npos ||
	    where.find("grw0:/PSP2/UPDATE") != string::npos ||
	    where.find("sd0:/PSP2/UPDATE") != string::npos)
		storage = SYSTEM_UPDATE_STORAGE_INTERNAL;
	if (where.find("ux0:/data/PSP2/UPDATE") != string::npos)
		storage = SYSTEM_UPDATE_STORAGE_CARD;

	switch (error) {
	case (int)0x800F0231:
	case (int)0x800F0237:
	case (int)0x8018010D:
	case (int)0x8018011D:
	case (int)0x8018280A:
		ret = SYSTEM_UPDATE_ERROR_BROKEN;
		break;
	case (int)0x80180007:
	case (int)0x8018010E:
	case (int)0x8018011E:
		ret = SYSTEM_UPDATE_ERROR_NO_PACKAGE;
		break;
	case (int)0x80180012:
	case (int)0x80180022:
	case (int)0x80180032:
	case (int)0x80180052:
	case (int)0x801800A7:
	case (int)0x8018010A:
	case (int)0x8018010F:
	case (int)0x8018011A:
	case (int)0x8018011F:
		ret = SYSTEM_UPDATE_ERROR_STORAGE;
		if (storage == SYSTEM_UPDATE_STORAGE_INTERNAL)
			ret = SYSTEM_UPDATE_ERROR_STORAGE + 1;
		if (storage == SYSTEM_UPDATE_STORAGE_CARD)
			ret = SYSTEM_UPDATE_ERROR_STORAGE + 2;
		break;
	case (int)0x801804E3:
	case (int)0x801804E4:
		ret = SYSTEM_UPDATE_ERROR;
		break;
	case (int)0x80182802:
		ret = SYSTEM_UPDATE_ERROR_CHECK;
		break;
	case SCE_PAF_ERROR_FILE_NO_ENTRY:
		ret = SYSTEM_UPDATE_ERROR_NO_FILE;
		break;
	case SCE_PAF_ERROR_FILE_NO_DEVICE:
		ret = SYSTEM_UPDATE_ERROR;
		if (storage == SYSTEM_UPDATE_STORAGE_INTERNAL)
			ret = SYSTEM_UPDATE_ERROR_NO_DEVICE;
		if (storage == SYSTEM_UPDATE_STORAGE_CARD)
			ret = SYSTEM_UPDATE_ERROR_NO_DEVICE + 1;
		break;
	default:
		ret = SYSTEM_UPDATE_ERROR;
		break;
	}
	return ret;
}

/* an error of a download over the PC's MTP connection, as the system update gives it */
int Bgdl_81130C9C(int type, int error)
{
	int ret;

	if (error >= 0)
		return 0;

	ret = SYSTEM_UPDATE_ERROR_MTP;
	switch (error) {
	case (int)0x8054001B:
	case (int)0x8054001C:
		if (type == 1)
			ret = SYSTEM_UPDATE_ERROR_MTP + 9;
		if (type == 2)
			return SYSTEM_UPDATE_ERROR_MTP + 8;
		break;
	case (int)0x80540034:
		if (type == 1)
			ret = SYSTEM_UPDATE_ERROR_MTP + 2;
		if (type == 2)
			return SYSTEM_UPDATE_ERROR_MTP + 1;
		break;
	case (int)0x805400FD:
		return SYSTEM_UPDATE_ERROR_MTP + 2;
	case (int)0x805400FE:
		return SYSTEM_UPDATE_ERROR_MTP + 1;
	case (int)0x8054001F:
	case (int)0x80540020:
		break;
	case (int)0x80540016:
		if (type == 1)
			ret = SYSTEM_UPDATE_ERROR_MTP + 4;
		if (type == 2)
			return SYSTEM_UPDATE_ERROR_MTP + 3;
		break;
	case (int)0x80540017:
		return SYSTEM_UPDATE_ERROR_MTP + 5;
	case (int)0x80540018:
		return SYSTEM_UPDATE_ERROR_MTP + 6;
	case (int)0x80540019:
		return SYSTEM_UPDATE_ERROR_MTP + 7;
	default:
		break;
	}
	return ret;
}

/* an HTTP file opened with error: its size checked */
SystemUpdateResult Bgdl_81130D9E(common::SharedPtr<File> file, int error, SceInt64 knownSize)
{
	SystemUpdateResult result;

	if (error != 0) {
		if (error == (int)0x80AF50E3 || error == (int)0x80AF50B4) {
			result.error = SYSTEM_UPDATE_ERROR_HTTP_TIMEOUT;
			result.detail = error;
			return result;
		}
		if (error != SCE_PAF_ERROR_FILE_UNKNOWN_SIZE) {
			if (error < (int)0x80AF5080 || error > (int)0x80AF50FF) {
				result.error = SYSTEM_UPDATE_ERROR;
				result.detail = error;
				return result;
			}
			if (error < (int)0x80AF5093 || error >= (int)0x80AF50A0) {
				result.error = SYSTEM_UPDATE_ERROR_HTTP_STATUS;
				result.detail = error;
				return result;
			}
		}
	}

	SceInt64 size = file->GetFileSize();

	if (size < 0 && size == SCE_PAF_ERROR_FILE_UNKNOWN_SIZE && knownSize > 0)
		size = knownSize;
	if (size < 0) {
		result.error = SYSTEM_UPDATE_ERROR;
		result.detail = (int)size;
		return result;
	}

	int ret = Bgdl_811330BA(size);

	if (ret != 0) {
		result.error = SYSTEM_UPDATE_ERROR;
		result.detail = ret;
		if (ret == SYSTEM_UPDATE_ERROR_HTTP_SIZE) {
			result.error = SYSTEM_UPDATE_ERROR_HTTP;
			result.detail = SYSTEM_UPDATE_ERROR_HTTP_SIZE;
		}
	} else {
		result.error = 0;
		result.detail = 0;
	}
	return result;
}

int Bgdl_81130EC4(const char *path)
{
	return LocalFile::Exists(path);
}

/* the file at path removed, by the system update's own process when it asks for that */
SystemUpdateResult Bgdl_81130ECC(const char *path)
{
	SystemUpdateResult result;

	if (LocalFile::Exists(path) == true) {
		if (s_useClient == true && s_clientReady != false) {
			if (s_client != NULL)
				s_client->Bgdl_81133156(path);
		} else {
			int ret = LocalFile::RemoveFile(path);

			if (ret < 0) {
				result.error = SYSTEM_UPDATE_ERROR_WRITE;
				result.detail = ret;
				return result;
			}
		}
	}
	result.error = 0;
	result.detail = 0;
	return result;
}

/* the file at path made anew */
SystemUpdateResult Bgdl_81130F2A(const char *path)
{
	SystemUpdateResult result;
	int error = 0;
	int detail = 0;

	if (LocalFile::Exists(path) == true) {
		result = Bgdl_81130ECC(path);
		if (result.error != 0)
			return result;
	}

	int ret;
	common::SharedPtr<File> file = LocalFile::Open(path, SCE_O_RDWR | SCE_O_CREAT | SCE_O_EXCL,
						       6, &ret);

	if (file.get() != NULL && ret == 0) {
		file->Close();
	} else {
		if (file.get() != NULL && file->IsOpened())
			file->Close();
		error = SYSTEM_UPDATE_ERROR_WRITE;
		detail = ret;
	}
	result.error = error;
	result.detail = detail;
	return result;
}

SystemUpdateCtrl::SystemUpdateCtrl(int type, const char *infoPath, const char *pupPath)
	: m_type(type), m_infoPath(infoPath), m_pupPath(pupPath)
{
}

SystemUpdateCtrl::~SystemUpdateCtrl()
{
}

SystemUpdateResult SystemUpdateCtrl::Bgdl_811310CA(void)
{
	Bgdl_81130ECC(m_infoPath);
	return Bgdl_81130ECC(m_pupPath);
}

SystemUpdateResult SystemUpdateCtrl::Bgdl_81131168(SystemUpdateInfo info)
{
	SystemUpdateResult result;
	int error = 0;
	int detail = 0;

	if (LocalFile::Exists(m_infoPath) != true) {
		error = SYSTEM_UPDATE_ERROR;
	} else {
		int ret;
		common::SharedPtr<File> file = LocalFile::Open(m_infoPath, SCE_O_RDWR, 6, &ret);

		if (file.get() == NULL) {
			error = SYSTEM_UPDATE_ERROR_READ;
		} else if (ret != 0) {
			if (file->IsOpened())
				file->Close();
			error = SYSTEM_UPDATE_ERROR_READ;
		} else {
			int n = file->Write(&info, sizeof(info));

			if (n != sizeof(info)) {
				error = SYSTEM_UPDATE_ERROR_WRITE;
				detail = n;
			}
			file->Close();
		}
	}
	if (error != 0)
		Bgdl_811310CA();
	result.error = error;
	result.detail = detail;
	return result;
}

SystemUpdateResult SystemUpdateCtrl::Bgdl_81131280(SystemUpdateInfo info)
{
	SystemUpdateResult result = Bgdl_81130F2A(m_infoPath);

	if (result.error == 0)
		result = Bgdl_81131168(info);
	if (result.error != 0)
		Bgdl_811310CA();
	return result;
}

/* the information of an update from version, url and lastModified */
SystemUpdateResult SystemUpdateCtrl::Bgdl_81131A18(string version, string url, string lastModified,
						   SceInt64 offset, SceInt64 size, int state,
						   SystemUpdateInfo *info)
{
	SystemUpdateResult result;

	if (version.length() < sizeof(info->version)) {
		sce_paf_memset(info->version, 0, sizeof(info->version));
		sce_paf_memcpy(info->version, version.c_str(), version.length());
	}
	if (url.length() < sizeof(info->url)) {
		sce_paf_memset(info->url, 0, sizeof(info->url));
		sce_paf_memcpy(info->url, url.c_str(), url.length());
	}
	if (lastModified.length() < sizeof(info->lastModified)) {
		sce_paf_memset(info->lastModified, 0, sizeof(info->lastModified));
		sce_paf_memcpy(info->lastModified, lastModified.c_str(), lastModified.length());
	}
	info->offset = offset;
	info->size = size;
	info->state = state;
	result.error = 0;
	result.detail = 0;
	return result;
}

bool SystemUpdateCtrl::Bgdl_81131ABC(void)
{
	return LocalFile::Exists(m_pupPath);
}

/* the update's file looked for, when the information says it is being downloaded */
__attribute__((noinline)) SystemUpdateResult SystemUpdateCtrl::Bgdl_81131AC6(SystemUpdateInfo *info, bool *there,
						   bool resuming)
{
	SystemUpdateResult result;
	int error = 0;

	*there = true;
	if (info->state == 1 || info->state == 2) {
		*there = false;
		if (LocalFile::Exists(m_pupPath) == true) {
			/* both ways the same in Sony's code */
			if (LocalFile::GetFileSize(m_pupPath) == info->offset)
				*there = true;
			else
				*there = true;
		} else if (info->state != 1 || !resuming) {
			error = SYSTEM_UPDATE_ERROR_NO_UPDATE;
		}
	}
	result.error = error;
	result.detail = 0;
	return result;
}

bool SystemUpdateCtrl::Bgdl_81131B54(void)
{
	return LocalFile::Exists(m_infoPath);
}

SystemUpdateResult SystemUpdateCtrl::Bgdl_81131B5E(SystemUpdateInfo *info, bool keep)
{
	SystemUpdateResult result;
	int error = 0;
	int detail = 0;

	if (LocalFile::Exists(m_infoPath) != true) {
		error = SYSTEM_UPDATE_ERROR;
	} else {
		int ret;
		common::SharedPtr<File> file = LocalFile::Open(m_infoPath, SCE_O_RDONLY, 6, &ret);

		if (file.get() == NULL || ret != 0) {
			error = SYSTEM_UPDATE_ERROR_READ;
		} else if (file->GetFileSize() < (SceInt64)sizeof(*info)) {
			file->Close();
			if (!keep)
				Bgdl_81130ECC(m_infoPath);
			error = SYSTEM_UPDATE_ERROR;
		} else {
			int n = file->Read(info, sizeof(*info));

			if (n != sizeof(*info)) {
				error = SYSTEM_UPDATE_ERROR_READ;
				detail = n;
			}
			file->Close();
		}
	}
	if (error != 0 && !keep)
		Bgdl_811310CA();
	result.error = error;
	result.detail = detail;
	return result;
}

/* the information's state moved on: 0 downloading from offset, 1 none, 2 downloaded */
SystemUpdateResult SystemUpdateCtrl::Bgdl_81131CE6(int state, SystemUpdateInfo *info,
						   SceInt64 offset)
{
	SystemUpdateResult result;
	int error;
	int detail = 0;

	if (LocalFile::Exists(m_infoPath) != true) {
		error = SYSTEM_UPDATE_ERROR;
	} else {
		result = Bgdl_81131B5E(info, false);
		error = result.error;
		detail = result.detail;
		if (error == 0) {
			if (state == 0) {
				info->offset = offset;
				info->state = 1;
				m_offset = offset;
			} else if (state == 1) {
				info->state = 0;
			} else if (state == 2) {
				info->state = 2;
				info->offset = info->size;
			}

			bool there = false;

			result = Bgdl_81131AC6(info, &there, false);
			error = result.error;
			detail = result.detail;
			if (error == 0) {
				if (there == true) {
					result = Bgdl_81131168(*info);
					error = result.error;
					detail = result.detail;
				} else {
					error = SYSTEM_UPDATE_ERROR_NOT_THERE;
				}
			}
		}
	}
	if (error != 0)
		Bgdl_811310CA();
	result.error = error;
	result.detail = detail;
	return result;
}

SystemUpdateResult SystemUpdateCtrl::Bgdl_81132558(SystemUpdateStatus *status, bool keep,
						   bool resuming)
{
	SystemUpdateResult result;
	int error = 0;
	int detail = 0;
	SystemUpdateInfo info;

	status->state = 3;
	status->value = 0;
	if (LocalFile::Exists(m_infoPath) == true) {
		result = Bgdl_81131B5E(&info, keep);
		error = result.error;
		detail = result.detail;
		if (result.error == 0) {
			status->type = info.type;
			if (info.state == 2) {
				bool there = false;

				result = Bgdl_81131AC6(&info, &there, false);
				error = result.error;
				detail = result.detail;
				if (result.error == 0 && there == true && info.offset == info.size) {
					status->value = info.offset;
					status->state = 2;
					status->version = info.version;
				}
			} else if (info.state != 0) {
				if (info.state == 1) {
					bool there = false;

					result = Bgdl_81131AC6(&info, &there, resuming);
					error = result.error;
					detail = result.detail;
					if (result.error == 0 && there == true) {
						status->state = 0;

						SceInt64 size = LocalFile::GetFileSize(m_pupPath);

						if (size >= 0)
							status->value = size;
						else
							error = SYSTEM_UPDATE_ERROR;
						status->version = info.version;
					}
				}
			} else {
				status->state = 1;
			}
			status->url = info.url;
		}
		if (error != 0 && !keep)
			Bgdl_811310CA();
	}
	result.error = error;
	result.detail = detail;
	return result;
}

SystemUpdateResult SystemUpdateCtrl::Bgdl_81132734(SystemUpdateStatus *status)
{
	return Bgdl_81132558(status, false, false);
}

/* the information checked: the same kind, url and size (its date compared and not used) */
SystemUpdateResult SystemUpdateCtrl::Bgdl_8113276C(string url, string lastModified, SceInt64 size,
						   bool *ok)
{
	SystemUpdateResult result;
	SystemUpdateInfo info;
	int error;
	int detail = 0;

	*ok = false;
	if (LocalFile::Exists(m_infoPath) == true) {
		result = Bgdl_81131B5E(&info, false);
		error = result.error;
		detail = result.detail;
		if (error == 0) {
			if (m_type != info.type)
				return result;

			bool same = false;
			{
				string infoUrl(info.url);

				if (sce_paf_strcmp(url.substr(0, url.length()).c_str(),
						   infoUrl.c_str()) == 0)
					same = true;
			}
			if (same && size == info.size) {
				string infoDate(info.lastModified);

				sce_paf_strcmp(lastModified.substr(0, lastModified.length()).c_str(),
					       infoDate.c_str());
				result.error = error;
				*ok = true;
				result.detail = detail;
				return result;
			}
		}
	} else {
		error = SYSTEM_UPDATE_ERROR;
	}
	if (error != 0)
		Bgdl_811310CA();
	result.error = error;
	result.detail = detail;
	return result;
}

void Bgdl_81132986(bool value)
{
	s_useClient = value;
}

void Bgdl_81132992(void)
{
	s_clientReady = true;
}

void Bgdl_811329A2(SystemUpdateClient *client)
{
	s_client = client;
}
