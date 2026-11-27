/*
 * system_update_registry.cpp: the system update's settings (0x81132B02-0x811331B4): the
 * console's language and region, the update list's url (the registry's server_url, else
 * Sony's server for the console's region), whether the download may use the phone line, and
 * a file removed through the system update's own process.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <kernel/iofilemgr.h>
#include <libnetctl.h>
#include <regmgr/regmgr.h>
#include <vshbridge/vshbridge.h>
#include <ss_mgr/ss_mgr.h>

#include <system_update/system_update_registry.h>
#include "system_update_version.h"

using namespace paf;

#define SYSTEM_UPDATE_ERROR_REGISTRY	((int)0x801825FD)	/* PLACEHOLDER names */
#define SYSTEM_UPDATE_ERROR_NO_SERVER	((int)0x801824A2)
#define SYSTEM_UPDATE_ERROR_HTTP	((int)0x801824C0)
#define SYSTEM_UPDATE_ERROR_HTTP_SIZE	((int)0x801824C3)
#define SYSTEM_UPDATE_ERROR_PHONE	((int)0x801825F9)
#define SYSTEM_UPDATE_ERROR_ARGUMENT	((int)0x80182005)

#define SYSTEM_UPDATE_CLIENT_REMOVE	1001	/* PLACEHOLDER name */

/* the console's language (type 0) or region (1: the registry's, or the PS code's product) */
SystemUpdateResult Bgdl_81132B02(int type, int *value, bool usePscode)
{
	SystemUpdateResult result;
	int error = 0;
	int detail = 0;

	if (type == 0) {
		int language;
		int ret = sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "language", &language);

		if (ret != 0) {
			error = SYSTEM_UPDATE_ERROR_REGISTRY;
			detail = ret;
		} else {
			*value = language;
		}
	} else if (type == 1) {
		if (usePscode) {
			ScePsCode code;
			int ret = vshSblAimgrGetPscode(&code);

			if (ret != 0) {
				error = SYSTEM_UPDATE_ERROR_REGISTRY;
				detail = ret;
			} else {
				*value = code.product_code;
			}
		} else {
			int ret = sceRegMgrSrvGetRegion();

			if (ret <= 0) {
				error = SYSTEM_UPDATE_ERROR_REGISTRY;
				detail = ret;
			} else {
				*value = ret;
			}
		}
	} else {
		error = SYSTEM_UPDATE_ERROR;
	}
	result.error = error;
	result.detail = detail;
	return result;
}

/* the first line of the file at path appended to *str */
int Bgdl_81132BAE(const char *path, string *str)
{
	SceIoStat stat;
	char buf[1024];
	int max;
	int ret;

	if (path == NULL || str == NULL)
		return SYSTEM_UPDATE_ERROR_ARGUMENT;

	stat.st_size = 0;
	max = sizeof(buf);
	ret = sceIoGetstat(path, &stat);
	if (ret < 0)
		return ret;
	if (stat.st_size + 1 < (SceOff)sizeof(buf))
		max = (int)stat.st_size + 1;

	SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 6);

	if (fd < 0)
		return fd;

	SceSSize n = sceIoRead(fd, buf, sizeof(buf));

	sceIoClose(fd);
	if (n < 0)
		return n;

	int len = 0;

	buf[max - 1] = '\0';
	if (max > 0) {
		bool found = false;

		len = 0;
		do {
			char ch = buf[len];

			if (ch == '\r' || ch == '\n' || ch == '\0') {
				buf[len] = '\0';
				found = true;
				break;
			}
			len++;
		} while (len < max);
		if (!found)
			len = 0;
	}
	str->append(buf, len);
	return 0;
}

/* Sony's update list for the console's region, with the console's own query */
string Bgdl_81132CDC(void)
{
	char region[128];

	if (vshSblUtMgrHasComTestFlag() == 1) {
		sce_paf_memset(region, 0, sizeof(region));
		if (Bgdl_8126E1AE(region) == 0)
			return string(region);
	}

	string cdn;

	if (vshSblAimgrIsCEX() != 1 || Bgdl_811361D4(&cdn) < 0)
		cdn = "jp";

	char url[113];

	sce_paf_snprintf(url, sizeof(url),
			 "http://f%s01.psp2.update.playstation.net/update/psp2/list/%s/psp2-updatelist.xml",
			 cdn.c_str(), cdn.c_str());

	string list(url);
	string query;

	if (Bgdl_81132BAE("ur0:ci/prev/uc/uc.dat", &query) == 0) {
		list += "?";
		list.append(query);
	}
	return list;
}

/* the update list's url (type 0) */
SystemUpdateResult Bgdl_81132EDA(int type, string *url)
{
	SystemUpdateResult result;
	int error;

	if (type != 0) {
		error = SYSTEM_UPDATE_ERROR;
	} else {
		error = 0;

		char buf[512];
		string server = "";

		sce_paf_memset(buf, 0, sizeof(buf));
		url->assign(NULL, 0);

		int ret = vshSblAimgrIsCEX();
		bool cex = ret == 1;

		if (ret >= 0) {
			SystemUpdateVersionInfo info;

			if (Bgdl_8137C396(&info) >= 0) {
				bool hasServer = false;

				if (!cex || info.flag == 0) {
					if (sceRegMgrGetKeyStr("/CONFIG/SYSTEM/UPDATE", "server_url", buf,
							       sizeof(buf)) == 0)
						server += buf;
					if (server.length() != 0) {
						*url = server;
						error = 0;
						hasServer = true;
					}
				}
				if (!hasServer) {
					if (cex)
						*url = Bgdl_81132CDC();
					else
						error = SYSTEM_UPDATE_ERROR_NO_SERVER;
				}
			} else {
				error = 0;
			}
		} else {
			error = 0;
		}
	}
	result.error = error;
	result.detail = 0;
	return result;
}

/* the download may use the phone line: 0 when it is not on one */
int Bgdl_811330A6(void)
{
	return Bgdl_8126E216(2) == 0 ? 1 : 0;
}

/* a download of size checked against the phone line's limit */
int Bgdl_811330BA(SceInt64 size)
{
	SceNetCtlInfo info;
	int ret = sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_DEVICE, &info);

	if (ret != 0)
		return ret;
	if (info.device <= SCE_NET_CTL_DEVICE_WIRED)
		return 0;
	if (info.device > SCE_NET_CTL_DEVICE_PHONE)
		return SYSTEM_UPDATE_ERROR_HTTP;
	if (Bgdl_8126E216(2) != 0)
		return SYSTEM_UPDATE_ERROR_PHONE;

	SceUInt64 max = 0;

	ret = Bgdl_8126E252(2, &max);
	if (ret != 0)
		return ret;
	if ((SceInt64)max >= size)
		return 0;
	return SYSTEM_UPDATE_ERROR_HTTP_SIZE;
}

/* the file at path removed by the system update's own process */
int SystemUpdateClient::Bgdl_81133156(const char *path)
{
	int result;

	if (m_client == NULL)
		return -4;
	result = 0;
	m_client->invokeSyncMethod(SYSTEM_UPDATE_CLIENT_REMOVE, path, sce_paf_strlen(path), &result,
				   NULL, 0, 0);
	return result;
}
