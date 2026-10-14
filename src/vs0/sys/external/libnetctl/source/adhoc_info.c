/*
 * SceNetCtl ad-hoc peer information: the record a console announces about
 * itself to ad-hoc peers (user name, system software version) and its
 * decoding for sceNetCtlAdhocGetPeerList.
 */

#include "netctl_internal.h"

#define NETCTL_REG_ID_SIGNIN_ACCOUNT	0x00450f32	/* 8-byte account id, nonzero when signed in */
#define NETCTL_REG_ID_USERNAME		0x00598438
#define NETCTL_SYSTEM_DATA_ONLINE_ID	0
#define NETCTL_ERROR_ERRNO_ENOENT	0x80010002	/* SCE_ERROR_ERRNO_ENOENT: no data file */

/* sceKernelGetSystemSwVersion output (0x28 bytes) */
typedef struct NetCtlSwVersion {
	SceSize size;
	char versionString[28];
	SceUInt64 version;
} NetCtlSwVersion;

/*
 * Builds the peer information record: format and revision bytes, the user's
 * name (online id when signed in, else the local user name), the system
 * software version (big endian) and its string, written field by field.
 */
int netCtlAdhocInfoPack(NetCtlAdhocPeerRecord *record)
{
	int ret;
	char *p;
	SceUInt8 format;
	SceUInt8 revision;
	SceUInt64 account;
	char username[17];
	NetCtlSwVersion version;
	char onlineId[0x148];

	revision = 1;
	format = 1;
	sceClibMemset(record, 0, sizeof(*record));
	p = (char *)record;
	sceClibMemcpy_safe(p, &format, sizeof(format));
	p += sizeof(format);
	sceClibMemcpy_safe(p, &revision, sizeof(revision));
	p += sizeof(revision);
	ret = sceRegMgrUtilityGetBin(NETCTL_REG_ID_SIGNIN_ACCOUNT, &account, sizeof(account));
	if (ret >= 0)
		ret = account != 0;
	if (ret < 0)
		return ret;
	if (ret == 0) {
		sceClibMemset(username, 0, sizeof(username));
		ret = sceRegMgrUtilityGetStr(NETCTL_REG_ID_USERNAME, username, sizeof(username));
		if (ret < 0)
			return ret;
		sceClibMemcpy_safe(p, username, 16);
	} else {
		ret = sceAppMgrGetSystemDataFile(NETCTL_SYSTEM_DATA_ONLINE_ID, onlineId, sizeof(onlineId));
		if (ret == (int)NETCTL_ERROR_ERRNO_ENOENT)
			sceClibMemset(onlineId, 0, sizeof(onlineId));
		else if (ret < 0)
			return ret;
		sceClibMemcpy_safe(p, onlineId, NETCTL_PEER_NAME_SIZE);
	}
	p += NETCTL_PEER_NAME_SIZE;
	version.size = sizeof(version);
	ret = sceKernelGetSystemSwVersion(&version);
	if (ret < 0)
		return ret;
	version.version = sceNetHtonll(version.version);
	sceClibMemcpy_safe(p, &version.version, sizeof(version.version));
	p += sizeof(version.version);
	sceClibMemcpy_safe(p, version.versionString, sizeof(version.versionString));
	return 0;
}

/* Decodes a peer information record; every output is optional. */
int netCtlAdhocInfoUnpack(const NetCtlAdhocPeerRecord *record, char *name, SceUInt64 *swVersion,
			  char *swVersionString, SceUInt32 *appVersion)
{
	const char *p;
	SceUInt8 format;
	SceUInt8 revision;

	p = (const char *)record;
	sceClibMemcpy_safe(&format, p, sizeof(format));
	p += sizeof(format);
	sceClibMemcpy_safe(&revision, p, sizeof(revision));
	p += sizeof(revision);
	if (name != NULL)
		sceClibMemcpy_safe(name, p, NETCTL_PEER_NAME_SIZE);
	p += NETCTL_PEER_NAME_SIZE;
	if (swVersion != NULL) {
		sceClibMemcpy_safe(swVersion, p, sizeof(*swVersion));
		*swVersion = sceNetNtohll(*swVersion);
	}
	p += sizeof(*swVersion);
	if (swVersionString != NULL)
		sceClibMemcpy_safe(swVersionString, p, NETCTL_PEER_VERSION_STRING_SIZE);
	p += NETCTL_PEER_VERSION_STRING_SIZE;
	if (appVersion != NULL) {
		sceClibMemcpy_safe(appVersion, p, sizeof(*appVersion));
		*appVersion = sceNetNtohl(*appVersion);
	}
	return 0;
}
