/*
 * teleport_app_helper.cpp: device info, LiveSpace DB app listing, MT19937 auth salt,
 * HMAC-SHA256 passcode verification, signed launch allowlist, and launch conflict checks
 * for SceShellTeleport (0x8139B32E..0x813A0F43).
 */
#include "teleport_internal.h"
#include <ces/libces_ucs.h>
#include <common/string_util.h>
#include <file/localfile.h>
#include <framework.h>
#include <kernel/sysmem.h>
#include <kernel/threadmgr.h>
#include <libhmac.h>
#include <libmt19937.h>
#include <libkernel/libkernel.h>
#include <np/np_manager.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <plugin.h>
#include <regmgr/regmgr.h>
#include <vshbridge/vshbridge.h>
#include <string.h>

extern "C" {
bool SceLsdb_41019D89(const LsdbAppInfoPtr *appInfo);
SceInt64 SceLsdb_849E8DC6(const LsdbAppInfoPtr *appInfo);
const char *sceLsdbGetName(const LsdbAppInfoPtr *appInfo);
const char *sceLsdbGetStitle(const LsdbAppInfoPtr *appInfo);
const char *sceLsdbGetAppVer(const LsdbAppInfoPtr *appInfo);
int sceAppMgrGetStatusByName(const char *appName, void *appStatus);
int sceAppMgrDestroyAppByAppId(int appId);
SceUInt32 sceKernelGetSystemTimeLow(void);
void *sceClibMemset(void *dst, int c, SceSize len);
}

extern SceInt32 parentalGetRemainingSec(void);

/* LSDB keys used by SceShellTeleport (0x21CA7EB1, 0x19FAF2AF, 0x8FE4F9AD) */
#define TELEPORT_LSDB_KEY_CATEGORY		0x21CA7EB1u
#define TELEPORT_LSDB_KEY_ATTRIBUTE		0x19FAF2AFu
#define TELEPORT_LSDB_KEY_URI_SCHEME		0x8FE4F9ADu
#define TELEPORT_LSDB_ATTR_TELEPORT_BIT		0x08000000
#define TELEPORT_TOPMENU_DEFAULT_DEVICE_NAME_ID	0x7A7AA5A2u
#define TELEPORT_ALLOWLIST_PATH			"tm0:/teleport/list_launch_teleport.dat"
#define TELEPORT_ALLOWLIST_MAX_BYTES		4096u
#define TELEPORT_ALLOWLIST_MAX_ENTRIES		292u
#define TELEPORT_SHELL_APP_ID			0x1002
#define TELEPORT_APPMGR_FOREGROUND_APP_ID	(-3)
#define TELEPORT_APPMGR_SUSPENDED_APP_ID	(-6)

static void formatSystemVersion(char *outVersion, SceSize outSize)
{
	SceKernelSystemSwVersion swVersion;
	swVersion.size = sizeof(swVersion);
	sceKernelGetSystemSwVersion(&swVersion);

	SceSize dotPos = 0;
	SceSize len = sce_paf_strlen(swVersion.versionString);
	while (dotPos < len && swVersion.versionString[dotPos] != '.')
		++dotPos;

	if (dotPos == 1)
		sce_paf_snprintf(outVersion, outSize, "0%s", swVersion.versionString);
	else
		sce_paf_snprintf(outVersion, outSize, "%s", swVersion.versionString);
}

static paf::string extractAppUriScheme(const LsdbValue &uriValue, const paf::string &titleId)
{
	const char *raw = (uriValue.type == LSDB_VALUE_TEXT && uriValue.text != NULL)
				  ? uriValue.text->c_str()
				  : "";
	paf::string rawStr(raw);
	paf::string scheme;

	SceSize openPos = rawStr.find_first_of("<");
	SceSize closePos = rawStr.find_first_of(">");
	if (openPos != paf::string::npos && closePos != paf::string::npos && closePos > openPos)
		scheme = rawStr.substr(openPos + 1, closePos - openPos - 1);

	if (scheme.empty())
		scheme = paf::string("teleport.") + titleId + ":";

	return scheme;
}

static SceUInt8 parseLowerHexByte(char hiChar, char loChar)
{
	SceUInt8 hi = (hiChar >= 'a' && hiChar <= 'f') ? (SceUInt8)(hiChar - 'a' + 10)
						       : (SceUInt8)(hiChar - '0');
	SceUInt8 lo = (loChar >= 'a' && loChar <= 'f') ? (SceUInt8)(loChar - 'a' + 10)
						       : (SceUInt8)(loChar - '0');
	return (SceUInt8)((hi << 4) | lo);
}

/* 0x8139B32E: fetch device display name (PSN online ID or filtered topmenu default) and OS version */
SceInt32 TeleportAppInfoHelper::GetDeviceInfo(char *outName, char *outVersion)
{
	paf::Framework::Instance();
	paf::Plugin *topmenu = paf::Plugin::Find("topmenu_plugin");
	paf::IDParam nameId("");
	nameId.m_idhash = TELEPORT_TOPMENU_DEFAULT_DEVICE_NAME_ID;
	const wchar_t *rawTitle = topmenu->GetString(nameId);

	paf::wstring wideTitle(rawTitle != NULL ? rawTitle : L"");
	paf::wstring filteredTitle;
	for (SceSize idx = 0; idx < wideTitle.size(); ++idx) {
		SceWChar16 ch = (SceWChar16)wideTitle[idx];
		if (ch > 0x1Fu && (ch < 0x80u || ch > 0xA0u) && ch != 0x00ADu)
			filteredTitle.append(1, (wchar_t)ch);
	}

	paf::string defaultName;
	paf::common::Utf16ToUtf8(filteredTitle, &defaultName);

	SceNpManagerCacheParam cachedParam;
	if (sceNpManagerGetCachedParam(&cachedParam) >= 0)
		sce_paf_snprintf(outName, 0x40u, "%s", cachedParam.npId.handle.data);
	else
		sce_paf_snprintf(outName, 0x40u, "%s", defaultName.c_str());

	formatSystemVersion(outVersion, 0x1Cu);
	return 0;
}

/* 0x8139B538: resolve app version string (OS version for system apps) and short title */
void TeleportAppInfoHelper::GetAppVersionAndTitle(LsdbAppInfoPtr appInfo, paf::string *outVersion,
						  paf::string *outTitle)
{
	if (SceLsdb_41019D89(&appInfo)) {
		char versionBuf[40];
		memset(versionBuf, 0, sizeof(versionBuf));
		formatSystemVersion(versionBuf, 0x1Cu);
		*outVersion = versionBuf;
	} else {
		const char *appVer = sceLsdbGetAppVer(&appInfo);
		*outVersion = (appVer != NULL) ? appVer : "";
	}

	const char *stitle = sceLsdbGetStitle(&appInfo);
	*outTitle = (stitle != NULL) ? stitle : "";
}

/* 0x8139B626: query LSDB for installed titles matching teleport-capable categories and attribute */
SceInt32 TeleportAppInfoHelper::QueryLaunchableTitles(paf::vector<paf::string> *outTitleIds)
{
	static const char *const kCategories[10] = {
		"gc", "gda", "gd", "gp", "gdb", "wda", "gdc", "gpc", "gdd", "gpd"
	};

	LsdbQueryCondition categoryCond;
	categoryCond.key = TELEPORT_LSDB_KEY_CATEGORY;
	categoryCond.op = LSDB_QUERY_OP_EQUAL;
	categoryCond.mask = 0;
	categoryCond.next = NULL;

	for (int idx = 0; idx < 10; ++idx) {
		LsdbValue catVal(kCategories[idx]);
		categoryCond.values.push_back(catVal);
	}

	LsdbQueryCondition attrCond;
	attrCond.key = TELEPORT_LSDB_KEY_ATTRIBUTE;
	attrCond.op = LSDB_QUERY_OP_BITS;
	attrCond.mask = TELEPORT_LSDB_ATTR_TELEPORT_BIT;
	attrCond.next = NULL;

	LsdbValue attrVal;
	attrVal.SetInteger(TELEPORT_LSDB_ATTR_TELEPORT_BIT);
	attrCond.values.push_back(attrVal);

	categoryCond.next = &attrCond;

	AppInfoQClient qclient;
	SceLsdb_A6D8772C(&qclient);
	SceInt32 ret = SceLsdb_E90B81CD(&qclient);
	if (ret == 0)
		ret = SceLsdb_76B75713(&qclient, &categoryCond, outTitleIds, 1);
	SceLsdb_E2101B03(&qclient);
	return ret;
}

/* 0x8139B828: format <apps><app>...</app></apps> XML list for GET /teleport.xml */
SceInt32 TeleportAppInfoHelper::BuildAppsListXml(char *outBuffer, SceSize bufferSize)
{
	AppInfoQClient qclient;
	SceLsdb_A6D8772C(&qclient);
	SceLsdb_E90B81CD(&qclient);

	paf::vector<paf::string> titleIds;
	SceInt32 queryRet = QueryLaunchableTitles(&titleIds);
	if (queryRet != 0) {
		SceLsdb_E2101B03(&qclient);
		return queryRet;
	}

	SceSize written = (SceSize)sce_paf_snprintf(outBuffer, bufferSize, "<apps>");
	SceSize closingLen = sce_paf_strlen("</apps>");

	for (SceSize idx = 0; idx < titleIds.size(); ++idx) {
		LsdbValue uriVal;
		SceInt32 getRet = SceLsdb_65ECEF39(&qclient, &titleIds[idx],
						   TELEPORT_LSDB_KEY_URI_SCHEME, &uriVal);
		if (getRet != 0 && getRet != (SceInt32)LSDB_ERROR_NOT_FOUND)
			continue;

		if (sce_paf_strncmp(titleIds[idx].c_str(), "NPXS10010", titleIds[idx].size()) == 0)
			continue;

		paf::string scheme = extractAppUriScheme(uriVal, titleIds[idx]);

		LsdbAppInfoPtr appInfo(new LsdbAppInfo());
		SceLsdb_D239031A(&qclient, &titleIds[idx], &appInfo);

		paf::string version;
		paf::string title;
		GetAppVersionAndTitle(appInfo, &version, &title);

		const char *stitle = sceLsdbGetStitle(&appInfo);
		title = (stitle != NULL) ? stitle : "";

		char entryBuf[140];
		sce_paf_memset(entryBuf, 0, sizeof(entryBuf));
		SceInt32 entryLen = sce_paf_sprintf(
			entryBuf,
			"<app><name>%s</name><version>%s</version><uri>%s</uri></app>",
			title.c_str(), version.c_str(), scheme.c_str());

		if (written + (SceSize)entryLen + closingLen > bufferSize)
			break;

		written += (SceSize)sce_paf_sprintf(
			outBuffer + written,
			"<app><name>%s</name><version>%s</version><uri>%s</uri></app>",
			title.c_str(), version.c_str(), scheme.c_str());
	}

	written += (SceSize)sce_paf_sprintf(outBuffer + written, "</apps>");
	SceLsdb_E2101B03(&qclient);
	return (SceInt32)written;
}

/* 0x8139BFC4: derive 32-bit PRNG seed from lowercase colon-separated MAC address string */
SceUInt32 TeleportAppInfoHelper::ParseMacSeed(const char *macStr)
{
	SceUInt32 octetFirst = parseLowerHexByte(macStr[0], macStr[1]);
	SceUInt32 octetSecond = parseLowerHexByte(macStr[3], macStr[4]);
	SceUInt32 octetThird = parseLowerHexByte(macStr[6], macStr[7]);
	SceUInt32 octetFourth = parseLowerHexByte(macStr[9], macStr[10]);
	SceUInt32 octetFifth = parseLowerHexByte(macStr[12], macStr[13]);
	SceUInt32 octetSixth = parseLowerHexByte(macStr[15], macStr[16]);

	SceUInt32 lowWord = (octetFirst | (octetSecond << 8) |
			     (octetThird << 16) | (octetFourth << 24));
	return lowWord + octetFifth + (octetSixth << 8);
}

/* 0x8139C118: generate 16-byte random authentication salt formatted as 32 uppercase hex chars */
SceInt32 TeleportAppInfoHelper::GenerateAuthSalt(char *outSaltHex, const char *macStr)
{
	static const char kHexDigits[16] = {
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
	};

	SceUInt32 seed = ParseMacSeed(macStr) ^ sceKernelGetSystemTimeLow();
	SceMt19937Context mtCtx;
	sceMt19937Init(&mtCtx, seed);
	sceMt19937UInt(&mtCtx);

	paf::string saltHex;
	for (int round = 0; round < 16; ++round) {
		sceMt19937UInt(&mtCtx);
		SceUInt32 byteVal = mtCtx.state[mtCtx.count % 0x26Cu] % 0xFFu;
		char pair[4];
		pair[0] = kHexDigits[(byteVal >> 4) & 0xFu];
		pair[1] = kHexDigits[byteVal & 0xFu];
		pair[2] = '\0';
		saltHex += pair;
	}

	sce_paf_snprintf(outSaltHex, 0x21u, "%s", saltHex.c_str());
	return 0;
}

/* 0x8139C328: check if a non-system foreground app conflicts with the requested teleport URI */
SceInt32 TeleportAppInfoHelper::CheckForegroundAppConflict(const char *uri, SceSize uriLen,
							   paf::string *outConflictAppXml,
							   paf::string *outForegroundTitle)
{
	(void)uriLen;
	paf::string uriStr(uri != NULL ? uri : "");

	AppInfoQClient qclient;
	SceLsdb_A6D8772C(&qclient);
	SceLsdb_E90B81CD(&qclient);

	paf::vector<paf::string> titleIds;
	QueryLaunchableTitles(&titleIds);

	SceAppMgrAppStatus fgStatus;
	sceAppMgrGetStatusByAppId(TELEPORT_APPMGR_FOREGROUND_APP_ID, &fgStatus);

	LsdbAppInfoPtr fgAppInfo(new LsdbAppInfo());
	paf::string fgTitleId(fgStatus.appName);
	SceLsdb_D239031A(&qclient, &fgTitleId, &fgAppInfo);

	if (SceLsdb_41019D89(&fgAppInfo)) {
		SceLsdb_E2101B03(&qclient);
		return 0;
	}

	const char *fgStitle = sceLsdbGetStitle(&fgAppInfo);
	*outForegroundTitle = (fgStitle != NULL) ? fgStitle : "";

	for (SceSize idx = 0; idx < titleIds.size(); ++idx) {
		LsdbValue uriVal;
		SceInt32 getRet = SceLsdb_65ECEF39(&qclient, &titleIds[idx],
						   TELEPORT_LSDB_KEY_URI_SCHEME, &uriVal);
		if (getRet != 0 && getRet != (SceInt32)LSDB_ERROR_NOT_FOUND)
			continue;

		paf::string scheme = extractAppUriScheme(uriVal, titleIds[idx]);
		if (sce_paf_strncmp(scheme.c_str(), uriStr.c_str(), scheme.size()) != 0)
			continue;

		LsdbAppInfoPtr targetInfo(new LsdbAppInfo());
		SceLsdb_D239031A(&qclient, &titleIds[idx], &targetInfo);

		if (SceLsdb_849E8DC6(&targetInfo) != 0) {
			SceLsdb_E2101B03(&qclient);
			return 0;
		}

		const char *fgName = sceLsdbGetName(&fgAppInfo);
		if (fgName != NULL && sce_paf_strcmp(fgName, titleIds[idx].c_str()) == 0) {
			SceLsdb_E2101B03(&qclient);
			return 1;
		}

		paf::string targetVer;
		paf::string targetTitle;
		SceLsdb_D239031A(&qclient, &titleIds[idx], &targetInfo);
		GetAppVersionAndTitle(targetInfo, &targetVer, &targetTitle);

		*outConflictAppXml = "<app>";
		outConflictAppXml->append((paf::string("<name>") + targetTitle + "</name>").c_str());
		outConflictAppXml->append((paf::string("<version>") + targetVer + "</version>").c_str());
		outConflictAppXml->append((paf::string("<uri>") + uriStr + "</uri>").c_str());
		*outConflictAppXml += "</app>";

		SceLsdb_E2101B03(&qclient);
		return -1;
	}

	SceLsdb_E2101B03(&qclient);
	return -3;
}

/* 0x8139D354: map a teleport URI to its installed application title ID via LSDB */
SceInt32 TeleportAppInfoHelper::ResolveTitleIdByUri(const paf::string &uri,
						    paf::string *outTitleId)
{
	AppInfoQClient qclient;
	SceLsdb_A6D8772C(&qclient);
	SceLsdb_E90B81CD(&qclient);

	paf::vector<paf::string> titleIds;
	QueryLaunchableTitles(&titleIds);

	for (SceSize idx = 0; idx < titleIds.size(); ++idx) {
		LsdbValue uriVal;
		SceInt32 getRet = SceLsdb_65ECEF39(&qclient, &titleIds[idx],
						   TELEPORT_LSDB_KEY_URI_SCHEME, &uriVal);
		if (getRet != 0 && getRet != (SceInt32)LSDB_ERROR_NOT_FOUND)
			continue;

		paf::string scheme = extractAppUriScheme(uriVal, titleIds[idx]);
		if (sce_paf_strncmp(scheme.c_str(), uri.c_str(), scheme.size()) != 0)
			continue;

		LsdbAppInfoPtr appInfo(new LsdbAppInfo());
		SceLsdb_D239031A(&qclient, &titleIds[idx], &appInfo);
		const char *name = sceLsdbGetName(&appInfo);
		*outTitleId = (name != NULL) ? name : "";

		SceLsdb_E2101B03(&qclient);
		return 0;
	}

	SceLsdb_E2101B03(&qclient);
	return -1;
}

/* 0x8139D8C8: check whether the application corresponding to uri is currently running */
SceInt32 TeleportAppInfoHelper::IsAppRunningByUri(const paf::string &uri)
{
	paf::string titleId;
	ResolveTitleIdByUri(uri, &titleId);

	SceAppMgrAppStatus appStatus;
	if (sceAppMgrGetStatusByName(titleId.c_str(), &appStatus) != 0)
		return 0;
	return 1;
}

/* 0x8139D958: check if a running/suspended app status matches uri; otherwise populate target XML */
SceInt32 TeleportAppInfoHelper::CheckRunningAppByStatus(const SceAppMgrAppStatus &status,
							const paf::string &uri,
							paf::string *outAppXml,
							paf::string *outRunningTitle)
{
	(void)outRunningTitle;
	paf::string uriCopy(uri);

	AppInfoQClient qclient;
	SceLsdb_A6D8772C(&qclient);
	SceLsdb_E90B81CD(&qclient);

	LsdbValue runningUriVal;
	paf::string runningTitleId(status.appName);
	SceLsdb_65ECEF39(&qclient, &runningTitleId, TELEPORT_LSDB_KEY_URI_SCHEME, &runningUriVal);

	paf::string runningScheme = extractAppUriScheme(runningUriVal, runningTitleId);
	if (!runningScheme.empty() &&
	    sce_paf_strncmp(runningScheme.c_str(), uri.c_str(), runningScheme.size()) == 0) {
		SceLsdb_E2101B03(&qclient);
		return 1;
	}

	paf::vector<paf::string> titleIds;
	QueryLaunchableTitles(&titleIds);

	for (SceSize idx = 0; idx < titleIds.size(); ++idx) {
		LsdbValue candUriVal;
		SceInt32 getRet = SceLsdb_65ECEF39(&qclient, &titleIds[idx],
						   TELEPORT_LSDB_KEY_URI_SCHEME, &candUriVal);
		if (getRet != 0 && getRet != (SceInt32)LSDB_ERROR_NOT_FOUND)
			continue;

		paf::string candScheme = extractAppUriScheme(candUriVal, titleIds[idx]);
		if (sce_paf_strncmp(candScheme.c_str(), uriCopy.c_str(), candScheme.size()) != 0)
			continue;

		LsdbAppInfoPtr appInfo(new LsdbAppInfo());
		SceLsdb_D239031A(&qclient, &titleIds[idx], &appInfo);
		SceLsdb_D239031A(&qclient, &titleIds[idx], &appInfo);

		paf::string version;
		paf::string title;
		GetAppVersionAndTitle(appInfo, &version, &title);

		*outAppXml = "<app>";
		outAppXml->append((paf::string("<name>") + title + "</name>").c_str());
		outAppXml->append((paf::string("<version>") + version + "</version>").c_str());
		outAppXml->append((paf::string("<uri>") + candScheme + "</uri>").c_str());
		*outAppXml += "</app>";
	}

	SceLsdb_E2101B03(&qclient);
	return 0;
}

/* 0x8139E75E: check suspended app slot (-6); return 0 if none/shell, 1 if same app, -1 on conflict */
SceInt32 TeleportAppInfoHelper::CheckSuspendedAppConflict(const paf::string &uri,
							  paf::string *outErrorXml,
							  paf::string *outAppXml)
{
	paf::string hintText;
	SceAppMgrAppStatus suspendedStatus;
	if (sceAppMgrGetStatusByAppId(TELEPORT_APPMGR_SUSPENDED_APP_ID, &suspendedStatus) != 0 ||
	    suspendedStatus.appId == TELEPORT_SHELL_APP_ID)
		return 0;

	if (CheckRunningAppByStatus(suspendedStatus, uri, outAppXml, &hintText) == 1)
		return 1;

	char errCodeBuf[16];
	memset(errCodeBuf, 0, sizeof(errCodeBuf));
	sce_paf_snprintf(errCodeBuf, sizeof(errCodeBuf), "0x%08X",
			 (unsigned int)TELEPORT_ERR_SUSPENDED_APP);

	*outErrorXml = "<teleport>";
	*outErrorXml += "<error>";
	outErrorXml->append((paf::string("<code>") + errCodeBuf + "</code>").c_str());
	outErrorXml->append((paf::string("<hint>") + hintText + "</hint>").c_str());
	*outErrorXml += "</error>";
	outErrorXml->append(outAppXml->c_str(), outAppXml->size());
	*outErrorXml += "</teleport>";
	return -1;
}

/* 0x8139EEEA: decode uppercase hex string into raw byte array */
SceInt32 TeleportAppInfoHelper::DecodeHexString(const paf::string &hexStr, SceUInt8 *outBytes,
						SceSize maxBytes)
{
	SceSize outCount = 0;
	for (SceSize idx = 0; idx < hexStr.size() && outCount < maxBytes; ++idx) {
		if ((idx % 2) != 0)
			continue;

		char hiChar = hexStr[idx];
		SceUInt8 hiNibble;
		if (hiChar >= 'A' && hiChar <= 'F')
			hiNibble = (SceUInt8)((hiChar - 'A' + 10) & 0xFu);
		else if (hiChar >= '0' && hiChar <= '9')
			hiNibble = (SceUInt8)((hiChar - '0') & 0xFu);
		else
			return -1;

		char loChar = hexStr[idx + 1];
		SceUInt8 loNibble;
		if (loChar >= 'A' && loChar <= 'F')
			loNibble = (SceUInt8)((loChar - 'A' + 10) & 0xFu);
		else if (loChar >= '0' && loChar <= '9')
			loNibble = (SceUInt8)((loChar - '0') & 0xFu);
		else
			return -1;

		outBytes[outCount++] = (SceUInt8)((hiNibble << 4) | loNibble);
	}
	return 0;
}

/* 0x8139F0E4: verify client double-HMAC-SHA256 passcode digest against /CONFIG/NP2/TELEPORT */
SceInt32 TeleportAppInfoHelper::VerifyPasscodeAuth(const paf::string &clientDigestHex,
						   const paf::string &saltHex)
{
	char regPasscode[32];
	memset(regPasscode, 0, sizeof(regPasscode));
	SceInt32 regRet = sceRegMgrGetKeyStr("/CONFIG/NP2/TELEPORT", "passcode_server",
					     regPasscode, sizeof(regPasscode));
	if (regPasscode[0] == '\0' || regRet != 0)
		return 0;

	if (clientDigestHex.empty())
		return -2;
	if (saltHex.empty())
		return -1;

	for (SceSize idx = 0; idx < saltHex.size(); ++idx) {
		char ch = saltHex[idx];
		if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F')))
			return -1;
	}

	paf::string passcodeStr(regPasscode);
	SceUInt8 passcodeBytes[4];
	for (SceSize idx = 0; idx < 4; ++idx)
		passcodeBytes[idx] = (SceUInt8)passcodeStr[idx];

	SceUInt8 decodedSalt[16];
	memset(decodedSalt, 0, sizeof(decodedSalt));
	if (DecodeHexString(saltHex, decodedSalt, sizeof(decodedSalt)) != 0)
		return -1;

	SceUInt8 clientDigest[32];
	if (DecodeHexString(clientDigestHex, clientDigest, sizeof(clientDigest)) != 0)
		return -1;

	SceUInt8 saltCopy[16];
	memset(saltCopy, 0, sizeof(saltCopy));
	memcpy(saltCopy, decodedSalt, sizeof(saltCopy));

	SceUChar8 innerDigest[SCE_HMAC_SHA256_DIGEST_SIZE];
	SceUChar8 outerDigest[SCE_HMAC_SHA256_DIGEST_SIZE];
	memset(innerDigest, 0, sizeof(innerDigest));
	memset(outerDigest, 0, sizeof(outerDigest));

	sceHmacSha256Digest(saltCopy, sizeof(saltCopy), passcodeBytes, sizeof(passcodeBytes),
			    innerDigest);
	sceHmacSha256Digest(innerDigest, sizeof(innerDigest), passcodeBytes, sizeof(passcodeBytes),
			    outerDigest);

	for (SceSize idx = 0; idx < SCE_HMAC_SHA256_DIGEST_SIZE; ++idx) {
		if (outerDigest[idx] != clientDigest[idx])
			return -2;
	}
	return 0;
}

/* 0x8139F3F6: verify allowlist line matches "AAAA00000:0000" (4 uppercase + 5 digits + ':' + 4 digits) */
SceInt32 TeleportAppInfoHelper::ValidateAllowlistEntry(const char *entry, SceSize length)
{
	if (length != 14)
		return 0;

	for (SceSize idx = 0; idx < 14; ++idx) {
		char ch = entry[idx];
		if (idx <= 3) {
			if (ch < 'A' || ch > 'Z')
				return 0;
		} else if (idx <= 8) {
			if (ch < '0' || ch > '9')
				return 0;
		} else if (idx == 9) {
			if (ch != ':')
				return 0;
		} else {
			if (ch < '0' || ch > '9')
				return 0;
		}
	}
	return 1;
}

/* 0x8139F46E: resolve <name>/<version>/<uri> XML fragments, title ID, and version for a URI */
SceInt32 TeleportAppInfoHelper::ResolveAppXmlAndTitle(const paf::string &uri,
						      paf::string *outAppFieldsXml,
						      paf::string *outTitleId,
						      paf::string *outVersion)
{
	paf::string uriCopy(uri);

	AppInfoQClient qclient;
	SceLsdb_A6D8772C(&qclient);
	SceLsdb_E90B81CD(&qclient);

	paf::vector<paf::string> titleIds;
	QueryLaunchableTitles(&titleIds);

	for (SceSize idx = 0; idx < titleIds.size(); ++idx) {
		LsdbValue uriVal;
		SceInt32 getRet = SceLsdb_65ECEF39(&qclient, &titleIds[idx],
						   TELEPORT_LSDB_KEY_URI_SCHEME, &uriVal);
		if (getRet != 0 && getRet != (SceInt32)LSDB_ERROR_NOT_FOUND)
			continue;

		paf::string scheme = extractAppUriScheme(uriVal, titleIds[idx]);
		if (sce_paf_strncmp(scheme.c_str(), uriCopy.c_str(), scheme.size()) != 0)
			continue;

		LsdbAppInfoPtr appInfo(new LsdbAppInfo());
		SceLsdb_D239031A(&qclient, &titleIds[idx], &appInfo);

		paf::string version;
		paf::string title;
		GetAppVersionAndTitle(appInfo, &version, &title);

		*outAppFieldsXml = paf::string("<name>") + title + "</name>";
		outAppFieldsXml->append((paf::string("<version>") + version + "</version>").c_str());
		outAppFieldsXml->append((paf::string("<uri>") + scheme + "</uri>").c_str());
		*outTitleId = titleIds[idx];
		*outVersion = version;
	}

	SceLsdb_E2101B03(&qclient);
	return 0;
}

/* 0x8139FE78: check tm0:/teleport/list_launch_teleport.dat signed allowlist for CEX launches */
SceInt32 TeleportAppInfoHelper::CheckLaunchAllowlist(const paf::string &uri)
{
	paf::string appFieldsXml;
	paf::string titleId;
	paf::string appVersion;
	SceInt32 result = 0;
	SceSize verifiedSize = TELEPORT_ALLOWLIST_MAX_BYTES;
	char *listBuffer = new char[TELEPORT_ALLOWLIST_MAX_BYTES];
	GameWhiteListTeleportHeader header;

	sce_paf_memset(&header, 0, sizeof(header));
	sce_paf_memset(listBuffer, 0, TELEPORT_ALLOWLIST_MAX_BYTES);

	ResolveAppXmlAndTitle(uri, &appFieldsXml, &titleId, &appVersion);
	if (titleId.empty()) {
		delete[] listBuffer;
		return -3;
	}

	if (!paf::LocalFile::Exists(TELEPORT_ALLOWLIST_PATH)) {
		delete[] listBuffer;
		return -4;
	}

	paf::common::SharedPtr<paf::File> localFile =
		paf::LocalFile::Open(TELEPORT_ALLOWLIST_PATH, 1u, 0, &result);
	if (localFile.get() == NULL) {
		delete[] listBuffer;
		return -4;
	}

	if (localFile->Read(listBuffer, TELEPORT_ALLOWLIST_MAX_BYTES) < 0) {
		delete[] listBuffer;
		return -4;
	}

	SceInt32 verifyRet = gameWhiteListReadTeleportFile(localFile, listBuffer, &verifiedSize);
	if (verifyRet < 0 || verifiedSize >= TELEPORT_ALLOWLIST_MAX_BYTES) {
		gameWhiteListRequestTeleportUpdate();
		delete[] listBuffer;
		return -4;
	}

	listBuffer[verifiedSize] = '\0';
	if (gameWhiteListParseTeleportHeader(listBuffer, verifiedSize, &header) < 0) {
		gameWhiteListRequestTeleportUpdate();
		delete[] listBuffer;
		return -4;
	}

	if (header.format > 1) {
		delete[] listBuffer;
		return -5;
	}

	if (header.type != GAME_WHITE_LIST_TYPE_BLACK) {
		delete[] listBuffer;
		return -4;
	}

	if (header.headerSize == verifiedSize) {
		delete[] listBuffer;
		return 0;
	}

	paf::string bodyText(listBuffer + header.headerSize);
	paf::vector<paf::string> lines;
	teleportSplitString(&lines, bodyText, paf::string("\n"), TELEPORT_ALLOWLIST_MAX_ENTRIES);

	for (SceSize idx = 0; idx < lines.size(); ++idx) {
		for (SceSize pos = 0; pos < lines[idx].size(); ++pos) {
			if (lines[idx][pos] == '\r')
				lines[idx].erase(pos, paf::string::npos);
		}
		if (ValidateAllowlistEntry(lines[idx].c_str(), lines[idx].size()) == 0) {
			delete[] listBuffer;
			return -4;
		}
	}

	for (SceSize idx = 0; idx < lines.size(); ++idx) {
		double appVerDouble = sce_paf_atof(appVersion.c_str());
		char entryTitleId[10];
		char entryVerStr[5];
		sce_paf_memset(entryTitleId, 0, sizeof(entryTitleId));
		sce_paf_memset(entryVerStr, 0, sizeof(entryVerStr));

		for (SceSize pos = 0; pos < 9 && pos < lines[idx].size(); ++pos)
			entryTitleId[pos] = lines[idx][pos];
		for (SceSize pos = 10; pos < 14 && pos < lines[idx].size(); ++pos)
			entryVerStr[pos - 10] = lines[idx][pos];

		SceInt32 blockedVer = sce_paf_atoi(entryVerStr);
		if (sce_paf_strncmp(entryTitleId, titleId.c_str(), 9u) == 0) {
			float appVerScaled = (float)(appVerDouble * 100.0);
			delete[] listBuffer;
			return (appVerScaled <= (float)blockedVer) ? -4 : 0;
		}
	}

	delete[] listBuffer;
	return result;
}

/* 0x813A0602: verify UTF-8 string is well-formed and does not begin with a UTF-8 BOM */
bool TeleportAppInfoHelper::ValidateUtf8NoBom(const paf::string &text)
{
	uint32_t convSize = 0;
	uint32_t utf32Char = 0;
	if (sceCesUtf8ToUtf32((const uint8_t *)text.c_str(), (uint32_t)text.size(),
			      &convSize, &utf32Char) != 0)
		return false;

	if (convSize == 3 && utf32Char == 0xFEFFu)
		return false;

	SceCesUcsContext ucsCtx;
	sceClibMemset(&ucsCtx, 0, sizeof(ucsCtx));
	if (sceCesUcsContextInit(&ucsCtx) != 0)
		return false;

	uint32_t utf8Len = 0;
	uint32_t utf16Len = 0;
	return sceCesUtf8StrGetUtf16Len(&ucsCtx, (const uint8_t *)text.c_str(),
					(uint32_t)text.size(), &utf8Len, &utf16Len) == 0;
}

/* 0x813A0684: full pre-launch validation pipeline (UTF-8, control chars, allowlist, parental, auth, conflicts) */
SceInt32 TeleportAppInfoHelper::ValidateLaunchRequest(const paf::string &uri,
						      const paf::string &clientDigestHex,
						      const paf::string &saltHex,
						      SceInt32 launchOption,
						      paf::string *outErrorXml,
						      paf::string *outAppXml)
{
	if (!ValidateUtf8NoBom(uri))
		return -1;
	if (teleportHasNoControlChars(uri) == 0)
		return -1;

	if (vshSblAimgrIsCEX() == 1) {
		SceInt32 allowRet = CheckLaunchAllowlist(uri);
		if (allowRet < 0)
			return allowRet;
	}

	if (parentalGetRemainingSec() == 0)
		return -1;

	SceInt32 authRet = VerifyPasscodeAuth(clientDigestHex, saltHex);
	if (authRet < 0)
		return authRet;

	SceInt32 suspendedRet = CheckSuspendedAppConflict(uri, outErrorXml, outAppXml);
	if (suspendedRet != 0)
		return suspendedRet;

	if (IsAppRunningByUri(uri) == 1)
		return 1;

	SceAppMgrAppStatus fgStatus;
	if (sceAppMgrGetStatusByAppId(TELEPORT_APPMGR_FOREGROUND_APP_ID, &fgStatus) != 0)
		return 0;

	if (launchOption == 1) {
		sceAppMgrDestroyAppByAppId(TELEPORT_APPMGR_FOREGROUND_APP_ID);
		return 0;
	}

	paf::string fgTitle;
	SceInt32 fgRet = CheckForegroundAppConflict(uri.c_str(), uri.size(), outAppXml, &fgTitle);
	if (fgRet < 0 && fgRet != -3) {
		char errCodeBuf[16];
		memset(errCodeBuf, 0, sizeof(errCodeBuf));
		sce_paf_snprintf(errCodeBuf, sizeof(errCodeBuf), "0x%08X",
				 (unsigned int)TELEPORT_ERR_FOREGROUND_APP);

		*outErrorXml = "<teleport>";
		*outErrorXml += "<error>";
		outErrorXml->append((paf::string("<code>") + errCodeBuf + "</code>").c_str());
		outErrorXml->append((paf::string("<hint>") + fgTitle + "</hint>").c_str());
		*outErrorXml += "</error>";
		outErrorXml->append(outAppXml->c_str(), outAppXml->size());
		*outErrorXml += "</teleport>";
	}
	return fgRet;
}
