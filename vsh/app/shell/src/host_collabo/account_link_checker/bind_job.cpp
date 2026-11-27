/*
 * bind_job.cpp: MTP host account-link checker job and MTPR3 host/account helpers.
 * Sony path: vsh/app/common/src/host_collabo/account_link_checker/bind_job.cpp
 * 3.65-CEX: 0x813BFA32 - 0x813C0C4B
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/stdlib.h>
#include <std/stdio.h>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <rtc.h>
#include <libkernel/libkernel.h>
#include <bXCe/bxce_internal.h>

#include "../../mtp/src/mtp_server_internal.h"
#include "../../livespace_db/lsdb_keys.h"

#define MTPR3_ERROR_INVALID_ARGUMENT		(-2146410495)	/* 0x80106001 */
#define MTPR3_ERROR_TIMEOUT			(-2146410494)	/* 0x80106002 */
#define MTPR3_ERROR_UNSUPPORTED_HOST		(-2146410493)	/* 0x80106003 */
#define MTPR3_ERROR_BUSY			(-2146410492)	/* 0x80106004 */
#define MTPR3_ERROR_NO_MEMORY			12

#define MTPR3_MTP_ERROR_PROTOCOL_ABORT		(-2141978620)	/* 0x80540004 */
#define MTPR3_MTP_ERROR_UNSUPPORTED_SETTING	(-2141978578)	/* 0x8054002E */

#define MTPR3_POLL_TIMEOUT_US			3000000ULL	/* 0x2DC6C0 */
#define MTPR3_POLL_INTERVAL_US			10000u
#define MTPR3_XML_BUFFER_SIZE			0x2000u
#define MTPR3_MIN_CONNECT_JOB_US		333000ULL	/* 0x514C8 */

#define MTPR3_HOST_KIND_PS3			1
#define MTPR3_HOST_KIND_PC			2

#define MTPR3_ACCOUNT_STATUS_ONLINE		1
#define MTPR3_ACCOUNT_STATUS_OFFLINE		2
#define MTPR3_ACCOUNT_STATUS_NONE		3

#define MTPR3_SAX_PATH_MAX			256

extern "C" {
extern int SceNpManager_B24FC028(int *isBound);
extern int SceNpManager_DDF30B0E(char *signInId, char *password, SceUInt64 *accountId);
extern int sceMtpGetCurrentInterface(int *interfaceType);
extern SceInt64 sceKernelGetSystemTimeWide(void);
extern int sceKernelDelayThread(SceUInt32 delayUsec);
extern int vshSysconIsIduMode(void);
extern int sceClibPrintf(const char *fmt, ...);
}

struct Mtpr3HostAccountXmlInfo {
	SceInt32 status;
	char userName[64];
	char signInId[128];
	SceUInt64 accountId;
	char countryCode[16];
	char langCode[16];
	SceInt16 birthYear;
	SceInt8 birthMonth;
	SceInt8 birthDay;
};

struct LocalNpAccountSummary {
	bool isBound;
	SceUInt8 alignmentGap[7];
	SceUInt64 accountId;
	char signInId[65];
	SceUInt8 tailAlignment[7];
};

struct RemoteNpAccountSummary {
	bool hasAccount;
	bool isOnlineUser;
	SceUInt8 alignmentGap[6];
	SceUInt64 accountId;
	char signInId[65];
	char countryCode[3];
	char langCode[6];
	SceUInt8 midAlignment[2];
	SceInt32 birthYear;
	SceInt32 birthMonth;
	SceInt32 birthDay;
};

struct AccountLinkCheckResult {
	int status;
	int errorCode;
	int hostKind;
	int interfaceKind;
	LocalNpAccountSummary localAccount;
	RemoteNpAccountSummary hostAccount;
};

typedef void (*AccountLinkFinishedCallback)(const AccountLinkCheckResult *result, void *userData);

static const char kBindJobSourcePath[] =
	"W:\\Build\\J01509657\\vsh\\app\\common\\src\\host_collabo\\account_link_checker\\bind_job.cpp";

static paf::job::JobQueue *s_bindJobQueue = NULL;		/* 0x815492E8 */

/* SAX parser handler state for /settingInfo/accounts/npAccount/ */
static Mtpr3HostAccountXmlInfo *s_saxTargetAccount = NULL;	/* 0x815492EC */
static bool *s_saxFoundUserName = NULL;				/* 0x815492F0 */
static bool *s_saxFoundOnlineUser = NULL;			/* 0x815492F4 */
static SceUInt8 s_saxPathLength = 0;				/* 0x815492F8 */
static char s_saxPathBuffer[MTPR3_SAX_PATH_MAX + 2];		/* 0x815CC130 */

static const char *bindJobBasename(const char *path)
{
	const char *slash = sce_paf_strrchr(path, '/');
	if (slash != NULL)
		return slash + 1;
	const char *backslash = sce_paf_strrchr(path, '\\');
	if (backslash != NULL)
		return backslash + 1;
	return path;
}

/* 0x813BFAA6 */
static void loadLocalNpAccountSummary(LocalNpAccountSummary *summary)
{
	summary->isBound = false;
	int boundFlag = 0;
	int ret = SceNpManager_B24FC028(&boundFlag);
	if (ret < 0) {
		sceClibPrintf("%s %s():%d ", bindJobBasename(kBindJobSourcePath), "set", 61);
		sceClibPrintf("sceNpUtilIsAccountBound() failed %X.\n", ret);
		return;
	}

	summary->isBound = (boundFlag == 1);
	if (!summary->isBound)
		return;

	char signInId[68];
	char password[36];
	SceUInt64 accountId = 0;
	sce_paf_memset(signInId, 0, 65);
	sce_paf_memset(password, 0, 33);

	ret = SceNpManager_DDF30B0E(signInId, password, &accountId);
	if (ret < 0) {
		sceClibPrintf("%s %s():%d ", bindJobBasename(kBindJobSourcePath), "set", 75);
		sceClibPrintf("sceNpManagerGetCredentialFromRegistry() failed %X.\n", ret);
		summary->isBound = false;
		return;
	}

	summary->accountId = accountId;
	sce_paf_strncpy(summary->signInId, signInId, 65);
	summary->signInId[64] = '\0';
}

/* 0x813BFC20 */
static void copyRemoteAccountFromXml(RemoteNpAccountSummary *summary,
				     const Mtpr3HostAccountXmlInfo *xmlInfo)
{
	sce_paf_strncpy(summary->signInId, xmlInfo->signInId, 65);
	summary->signInId[64] = '\0';
	sce_paf_strncpy(summary->countryCode, xmlInfo->countryCode, 3);
	summary->countryCode[2] = '\0';
	sce_paf_strncpy(summary->langCode, xmlInfo->langCode, 6);
	summary->langCode[5] = '\0';
	summary->accountId = xmlInfo->accountId;
	summary->birthYear = xmlInfo->birthYear;
	summary->birthMonth = xmlInfo->birthMonth;
	summary->birthDay = xmlInfo->birthDay;
}

class Mtpr3AccountSaxHandler : public bXCe::Sax::DocumentHandler {
public:
	/* 0x813C0C0C */
	Mtpr3AccountSaxHandler(Mtpr3HostAccountXmlInfo *target, bool *foundUser, bool *foundOnline)
	{
		s_saxTargetAccount = target;
		s_saxFoundUserName = foundUser;
		*foundUser = false;
		*foundOnline = false;
		s_saxFoundOnlineUser = foundOnline;
		s_saxPathBuffer[0] = '\0';
	}

	virtual ~Mtpr3AccountSaxHandler() {}

	/* 0x813C094C */
	virtual int startDocument(void *userData)
	{
		(void)userData;
		SceSize curLen = sce_paf_strlen(s_saxPathBuffer);
		sce_paf_strncat(s_saxPathBuffer, "/", (MTPR3_SAX_PATH_MAX + 1) - curLen);
		s_saxPathLength = 1;
		return 0;
	}

	/* 0x813C0984 */
	virtual int startElement(void *userData, const char *name, int length,
				 const bXCe::AttributeList *attrList)
	{
		(void)userData;
		if (s_saxPathLength + length > MTPR3_SAX_PATH_MAX)
			return 0;

		sce_paf_strncat(s_saxPathBuffer, name, static_cast<SceSize>(length));
		sce_paf_strncat(s_saxPathBuffer, "/", 1);
		s_saxPathLength = static_cast<SceUInt8>(s_saxPathLength + length + 1);

		if (sce_paf_strcmp(s_saxPathBuffer, "/settingInfo/accounts/npAccount/") != 0 ||
		    *s_saxFoundOnlineUser) {
			for (int i = 0; i < attrList->getLength(); ++i)
				(void)attrList->getAttribute(i);
			return 0;
		}

		for (int i = 0; i < attrList->getLength(); ++i) {
			const BxceAttr *attr = attrList->getAttribute(i);
			const char *attrName = attr->name;
			const char *attrValue = attr->value;

			if (sce_paf_strcmp(attrName, "userName") == 0) {
				sce_paf_strncpy(s_saxTargetAccount->userName, attrValue, 63);
				*s_saxFoundUserName = true;
			} else if (sce_paf_strcmp(attrName, "signInId") == 0) {
				sce_paf_strncpy(s_saxTargetAccount->signInId, attrValue, 127);
			} else if (sce_paf_strcmp(attrName, "accountId") == 0) {
				char hexAccount[20];
				sce_paf_strncpy(hexAccount, attrValue, 17);
				SceUInt64 parsedId =
					static_cast<SceUInt64>(sce_paf_strtoll(hexAccount, NULL, 16));
				s_saxTargetAccount->accountId = parsedId;
			} else if (sce_paf_strcmp(attrName, "countryCode") == 0) {
				sce_paf_strncpy(s_saxTargetAccount->countryCode, attrValue, 15);
			} else if (sce_paf_strcmp(attrName, "langCode") == 0) {
				sce_paf_strncpy(s_saxTargetAccount->langCode, attrValue, 15);
			} else if (sce_paf_strcmp(attrName, "birthday") == 0) {
				char birthdayText[16];
				sce_paf_strncpy(birthdayText, attrValue, sizeof(birthdayText));
				SceUInt32 packedDate =
					static_cast<SceUInt32>(sce_paf_atoi(birthdayText));
				s_saxTargetAccount->birthYear =
					static_cast<SceInt16>(packedDate >> 16);
				s_saxTargetAccount->birthMonth =
					static_cast<SceInt8>((packedDate >> 8) & 0xFFu);
				s_saxTargetAccount->birthDay =
					static_cast<SceInt8>(packedDate & 0xFFu);
			} else if (sce_paf_strcmp(attrName, "onlineUser") == 0 &&
				   sce_paf_strcmp("1", attrValue) == 0) {
				*s_saxFoundOnlineUser = true;
			}
		}
		return 0;
	}

	/* 0x813C0BE8 */
	virtual int endElement(void *userData, const char *name, int length)
	{
		(void)userData;
		(void)name;
		int newLen = static_cast<int>(s_saxPathLength) - length - 1;
		s_saxPathBuffer[newLen] = '\0';
		s_saxPathLength = static_cast<SceUInt8>(newLen);
		return 0;
	}
};

struct Mtpr3AccountSaxParser {
	bXCe::Sax::Parser *parser;
	Mtpr3AccountSaxHandler *handler;
	bool foundUserName;
	bool foundOnlineUser;
	Mtpr3HostAccountXmlInfo *target;

	/* 0x813C054E */
	Mtpr3AccountSaxParser(bool skipWhitespace, bool resolveEntity,
			      Mtpr3HostAccountXmlInfo *outAccount, int *outError)
		: parser(NULL),
		  handler(NULL),
		  foundUserName(false),
		  foundOnlineUser(false),
		  target(outAccount)
	{
		if (outAccount == NULL || outError == NULL) {
			for (;;)
				;
		}

		int err;
		parser = new bXCe::Sax::Parser();
		if (parser != NULL) {
			err = parser->initialize(NULL);
			if (err == 0) {
				parser->setResolveEntity(resolveEntity);
				parser->setSkipIgnorableWhiteSpace(skipWhitespace);
				handler = new Mtpr3AccountSaxHandler(target, &foundUserName,
								     &foundOnlineUser);
				if (handler != NULL)
					parser->setDocumentHandler(handler);
				else
					err = MTPR3_ERROR_NO_MEMORY;
			}
		} else {
			err = MTPR3_ERROR_NO_MEMORY;
		}
		*outError = err;
	}

	~Mtpr3AccountSaxParser()
	{
		if (handler != NULL) {
			delete handler;
			handler = NULL;
		}
		if (parser != NULL) {
			parser->terminate();
			delete parser;
			parser = NULL;
		}
	}
};

/* 0x813C034C */
static int mtpr3GetConnectionInfo(int *outConnected)
{
	if (outConnected == NULL)
		return MTPR3_ERROR_INVALID_ARGUMENT;

	*outConnected = 0;
	SceInt64 startUs = sceKernelGetSystemTimeWide();
	for (;;) {
		if (static_cast<SceUInt64>(sceKernelGetSystemTimeWide() - startUs) >
		    MTPR3_POLL_TIMEOUT_US)
			return MTPR3_ERROR_TIMEOUT;

		MtpStatus hostStatus;
		int ret = sceMtpGetHostInfo(&hostStatus);
		if (ret != 0)
			return ret;

		int interfaceType = 0;
		ret = sceMtpGetCurrentInterface(&interfaceType);
		if (ret != 0)
			return ret;

		if (interfaceType == MTP_IF_TYPE_TCPIP) {
			if (hostStatus.reason == 3 || hostStatus.reason == 8 ||
			    hostStatus.reason == 0)
				return MTPR3_ERROR_TIMEOUT;
			*outConnected = 1;
			return 0;
		}

		if (hostStatus.state > 2 && hostStatus.state <= 4) {
			*outConnected = 1;
			return 0;
		}

		sceKernelDelayThread(MTPR3_POLL_INTERVAL_US);
	}
}

/* 0x813C0410 */
static int mtpr3GetCurrentInterface(int *outInterface)
{
	if (outInterface == NULL)
		return MTPR3_ERROR_INVALID_ARGUMENT;

	*outInterface = 0;
	int currentIf = 0;
	int ret = sceMtpGetCurrentInterface(&currentIf);
	if (ret == 0)
		*outInterface = currentIf;
	return ret;
}

/* 0x813C0452 */
static int mtpr3GetHostInfo(int *outHostKind)
{
	if (outHostKind == NULL)
		return MTPR3_ERROR_INVALID_ARGUMENT;

	SceInt64 startUs = sceKernelGetSystemTimeWide();
	for (;;) {
		if (static_cast<SceUInt64>(sceKernelGetSystemTimeWide() - startUs) >
		    MTPR3_POLL_TIMEOUT_US)
			return MTPR3_ERROR_TIMEOUT;

		MtpStatus hostStatus;
		int ret = sceMtpGetHostInfo(&hostStatus);
		if (ret != 0)
			return ret;

		int interfaceType = 0;
		ret = sceMtpGetCurrentInterface(&interfaceType);
		if (ret != 0)
			return ret;

		if (interfaceType == MTP_IF_TYPE_TCPIP) {
			MtpNetHost netHost;
			ret = SceLibMtp_ED13170C(&netHost);
			if (ret != 0)
				return ret;
			if (netHost.hostType >= 1 && netHost.hostType <= 3) {
				*outHostKind = MTPR3_HOST_KIND_PC;
				return 0;
			}
			if (netHost.hostType == MTP_NET_HOST_TYPE_PS3) {
				*outHostKind = MTPR3_HOST_KIND_PS3;
				return 0;
			}
			return MTPR3_ERROR_UNSUPPORTED_HOST;
		}

		if (hostStatus.state > 2 && hostStatus.state <= 4) {
			if (hostStatus.platformClass == 1) {
				*outHostKind = MTPR3_HOST_KIND_PC;
				return 0;
			}
			if (hostStatus.platformClass == 2) {
				*outHostKind = MTPR3_HOST_KIND_PS3;
				return 0;
			}
			return MTPR3_ERROR_UNSUPPORTED_HOST;
		}

		sceKernelDelayThread(MTPR3_POLL_INTERVAL_US);
	}
}

/* 0x813C05F4 */
static int mtpr3GetNpAccountInfo(Mtpr3HostAccountXmlInfo *outAccount)
{
	if (outAccount == NULL)
		return MTPR3_ERROR_INVALID_ARGUMENT;

	sce_paf_memset(outAccount, 0, sizeof(*outAccount));

	int err = 0;
	Mtpr3AccountSaxParser saxParser(true, true, outAccount, &err);
	if (err != 0)
		return err;

	char *xmlChunk = static_cast<char *>(sce_paf_malloc(MTPR3_XML_BUFFER_SIZE));
	if (xmlChunk == NULL)
		return MTPR3_ERROR_NO_MEMORY;

	SceUInt32 contextId = 0;
	err = sceMtpBeginSetSystemSetting(1, &contextId);
	if (err == 0) {
		bool isFinalChunk = false;
		for (;;) {
			SceSize receivedSize = 0;
			SceUInt64 remainingSize = 0;
			err = SceLibMtp_CA704934(contextId, xmlChunk, MTPR3_XML_BUFFER_SIZE,
						 &receivedSize, &remainingSize);
			if (err != 0)
				break;

			if (remainingSize == 0) {
				--receivedSize;
				isFinalChunk = true;
			}

			if (saxParser.parser == NULL) {
				for (;;)
					;
			}

			int parseRet = saxParser.parser->parse(
				xmlChunk, static_cast<int>(receivedSize), isFinalChunk);
			err = (parseRet < 0) ? parseRet : 0;
			if (err != 0)
				break;

			if (remainingSize == 0) {
				if (saxParser.foundOnlineUser) {
					outAccount->status = MTPR3_ACCOUNT_STATUS_ONLINE;
				} else {
					sce_paf_memset(outAccount, 0, sizeof(*outAccount));
					outAccount->status = saxParser.foundUserName
								     ? MTPR3_ACCOUNT_STATUS_OFFLINE
								     : MTPR3_ACCOUNT_STATUS_NONE;
				}
				break;
			}
		}

		int endRet = SceLibMtp_D5FB089F(contextId);
		if (endRet != 0) {
			if (endRet == MTPR3_MTP_ERROR_PROTOCOL_ABORT || err == 0)
				err = endRet;
		}
	}

	sce_paf_free(xmlChunk);
	return err;
}

static int mtpr3CheckHostNotBusy(void)
{
	MtpStatus hostStatus;
	int ret = sceMtpGetHostInfo(&hostStatus);
	if (ret == 0 && hostStatus.flags == 1)
		return MTPR3_ERROR_BUSY;
	return ret;
}

/* 0x813C0786 */
static int mtpr3SetAccountId(const SceUInt8 accountIdBytes[8])
{
	if (accountIdBytes == NULL)
		return MTPR3_ERROR_INVALID_ARGUMENT;

	char *xmlBuffer = static_cast<char *>(sce_paf_malloc(MTPR3_XML_BUFFER_SIZE));
	if (xmlBuffer == NULL)
		return MTPR3_ERROR_NO_MEMORY;

	SceUInt64 bigEndianId = 0;
	for (int i = 0; i < 8; ++i)
		bigEndianId = (bigEndianId << 8) | accountIdBytes[i];

	sce_paf_snprintf(
		xmlBuffer, MTPR3_XML_BUFFER_SIZE,
		"<settingInfo><accounts><npAccount userName=\"mtpr3InvalidUserName\" "
		"signInId=\"InvalidSignInId\" accountId=\"%016llx\" countryCode=\"xx\" "
		"langCode=\"xx\" birthday=\"129238552\" onlineUser=\"0\" "
		"passwd=\"InvalidPassword\"/></accounts></settingInfo>",
		static_cast<unsigned long long>(bigEndianId));

	SceSize remainingToSend = sce_paf_strlen(xmlBuffer) + 1;
	SceUInt32 contextId = 0;
	bool settingOpened = false;

	int err = mtpr3CheckHostNotBusy();
	if (err == 0) {
		err = SceLibMtp_CFA3A9DA(3, static_cast<int>(remainingToSend), &contextId);
		if (err == 0) {
			settingOpened = true;
			err = mtpr3CheckHostNotBusy();
		}
	}

	if (err == 0) {
		const char *cursor = xmlBuffer;
		for (;;) {
			err = mtpr3CheckHostNotBusy();
			SceSize sentSize = 0;
			SceUInt64 remainingTotal = 0;
			if (err == 0) {
				err = sceMtpSendData(contextId, cursor, remainingToSend,
						     &sentSize, &remainingTotal);
				if (err == 0)
					err = mtpr3CheckHostNotBusy();
			}
			if (err != 0)
				break;
			remainingToSend -= sentSize;
			cursor += sentSize;
			if (remainingTotal == 0)
				break;
		}
	}

	if (settingOpened) {
		int endRet = SceLibMtp_EAA968F4(contextId);
		if (endRet != 0) {
			if (endRet == MTPR3_MTP_ERROR_PROTOCOL_ABORT || err == 0)
				err = endRet;
		}
	}

	sce_paf_free(xmlBuffer);
	return err;
}

class ConnectJob : public paf::job::JobItem {
public:
	/* 0x813BFD40 */
	ConnectJob(AccountLinkFinishedCallback callback, void *userData)
		: paf::job::JobItem("ConnectJob", NULL),
		  m_callback(callback),
		  m_userData(userData)
	{
		m_result.status = -1;
		m_result.errorCode = 0;
		m_result.hostKind = 0;
		m_result.localAccount.isBound = false;
		m_result.localAccount.accountId = 0;
		sce_paf_memset(m_result.localAccount.signInId, 0,
			       sizeof(m_result.localAccount.signInId));
		m_result.hostAccount.hasAccount = false;
		m_result.hostAccount.isOnlineUser = false;
		m_result.hostAccount.accountId = 0;
		sce_paf_memset(m_result.hostAccount.signInId, 0,
			       sizeof(m_result.hostAccount.signInId));
		sce_paf_memset(m_result.hostAccount.countryCode, 0,
			       sizeof(m_result.hostAccount.countryCode));
		sce_paf_memset(m_result.hostAccount.langCode, 0,
			       sizeof(m_result.hostAccount.langCode));
		m_result.hostAccount.birthYear = 0;
		m_result.hostAccount.birthMonth = 0;
		m_result.hostAccount.birthDay = 0;
	}

	/* 0x813C0250 / 0x813C026C */
	virtual ~ConnectJob() {}

	/* 0x813BFDF8 */
	int Run_Impl()
	{
		if (IsCanceled())
			return 0;

		int connected = 0;
		mtpr3GetConnectionInfo(&connected);
		if (connected != 1) {
			sceClibPrintf("%s %s():%d ", bindJobBasename(kBindJobSourcePath),
				      "Run_Impl", 184);
			sceClibPrintf("MTPR3GetConnectionInfo() NOT CONNECTED.\n");
			m_result.status = -1;
			return -1;
		}

		if (IsCanceled())
			return 0;

		int hostKind = 0;
		int ret = mtpr3GetHostInfo(&hostKind);
		if (ret != 0) {
			sceClibPrintf("%s %s():%d ", bindJobBasename(kBindJobSourcePath),
				      "Run_Impl", 196);
			sceClibPrintf("MTPR3GetHostInfo() failed %X.\n", ret);
			m_result.errorCode = ret;
			m_result.status = -1;
			return ret;
		}

		if (IsCanceled())
			return 0;

		int currentInterface = 0;
		ret = mtpr3GetCurrentInterface(&currentInterface);
		if (ret != 0) {
			sceClibPrintf("%s %s():%d ", bindJobBasename(kBindJobSourcePath),
				      "Run_Impl", 207);
			sceClibPrintf("MTPR3GetCurrentInterface() failed %X.\n", ret);
			m_result.errorCode = ret;
			m_result.status = -1;
			return ret;
		}

		if (currentInterface == MTP_IF_TYPE_USB)
			m_result.interfaceKind = 1;
		else if (currentInterface == MTP_IF_TYPE_TCPIP)
			m_result.interfaceKind = 2;
		else
			m_result.interfaceKind = 0;

		m_result.hostKind = (hostKind == MTPR3_HOST_KIND_PS3) ? MTPR3_HOST_KIND_PS3
								      : MTPR3_HOST_KIND_PC;

		if (IsCanceled())
			return 0;

		if (m_result.hostKind == MTPR3_HOST_KIND_PS3) {
			Mtpr3HostAccountXmlInfo hostXmlInfo;
			ret = mtpr3GetNpAccountInfo(&hostXmlInfo);
			if (ret != 0) {
				sceClibPrintf("%s %s():%d ", bindJobBasename(kBindJobSourcePath),
					      "Run_Impl", 226);
				sceClibPrintf("MTPR3GetNpAccountInfo() failed %X.\n", ret);
				m_result.errorCode = ret;
				m_result.status = -1;
				return ret;
			}

			if (hostXmlInfo.status == MTPR3_ACCOUNT_STATUS_ONLINE) {
				m_result.hostAccount.hasAccount = true;
				m_result.hostAccount.isOnlineUser = true;
				copyRemoteAccountFromXml(&m_result.hostAccount, &hostXmlInfo);
			} else if (hostXmlInfo.status == MTPR3_ACCOUNT_STATUS_OFFLINE) {
				m_result.hostAccount.hasAccount = true;
				m_result.hostAccount.isOnlineUser = false;
				copyRemoteAccountFromXml(&m_result.hostAccount, &hostXmlInfo);
			} else {
				m_result.hostAccount.hasAccount = false;
				m_result.hostAccount.isOnlineUser = false;
			}
		}

		loadLocalNpAccountSummary(&m_result.localAccount);
		if (m_result.hostKind == MTPR3_HOST_KIND_PC && vshSysconIsIduMode() != 0) {
			m_result.localAccount.isBound = true;
			m_result.localAccount.accountId = 0;
		}

		if (m_result.hostKind == MTPR3_HOST_KIND_PC && m_result.localAccount.isBound) {
			SceUInt8 accountIdBytes[8];
			sce_paf_memcpy(accountIdBytes, &m_result.localAccount.accountId,
				       sizeof(accountIdBytes));
			ret = mtpr3SetAccountId(accountIdBytes);
			if (ret != MTPR3_MTP_ERROR_UNSUPPORTED_SETTING && ret != 0) {
				sceClibPrintf("%s %s():%d ", bindJobBasename(kBindJobSourcePath),
					      "Run_Impl", 290);
				sceClibPrintf("MTPR3SetAccountId() failed %X.\n", ret);
				m_result.errorCode = ret;
				m_result.status = -1;
				return ret;
			}
		}

		m_result.status = 0;
		return 0;
	}

	/* 0x813C0196 */
	virtual int Run()
	{
		SceRtcTick startTick;
		SceRtcTick endTick;
		sceClibMemset(&startTick, 0, sizeof(startTick));
		sceClibMemset(&endTick, 0, sizeof(endTick));

		sceRtcGetCurrentTick(&startTick);
		int ret = Run_Impl();
		sceRtcGetCurrentTick(&endTick);

		if (!IsCanceled() && (endTick.tick - startTick.tick) < MTPR3_MIN_CONNECT_JOB_US) {
			SceUInt32 elapsedUs = static_cast<SceUInt32>(endTick.tick - startTick.tick);
			sceKernelDelayThread(static_cast<SceUInt32>(MTPR3_MIN_CONNECT_JOB_US) -
					     elapsedUs);
		}
		return ret;
	}

	/* 0x813C0230 */
	virtual void Finish(int result)
	{
		(void)result;
		if (!IsCanceled() && m_callback != NULL)
			m_callback(&m_result, m_userData);
	}

private:
	AccountLinkCheckResult m_result;
	AccountLinkFinishedCallback m_callback;
	void *m_userData;
};

/* 0x813BFC7A */
static void bindJobEnqueue(paf::job::JobItem *jobItem)
{
	if (jobItem == NULL)
		return;

	if (s_bindJobQueue != NULL) {
		paf::common::SharedPtr<paf::job::JobItem> jobHandle(jobItem);
		s_bindJobQueue->Enqueue(jobHandle);
	} else {
		delete jobItem;
	}
}

/* 0x813BFDD4 */
void mtpStartAccountLinkCheck(AccountLinkFinishedCallback callback, void *userData)
{
	ConnectJob *connectJob = new ConnectJob(callback, userData);
	bindJobEnqueue(connectJob);
}

int mtpRegisterAccountLinkCb(void (*callback)(const void *, int))
{
	mtpStartAccountLinkCheck(reinterpret_cast<AccountLinkFinishedCallback>(callback), NULL);
	return 0;
}

/* 0x813C028C */
void mtpJobQueueInit(paf::job::JobQueue::Option *option)
{
	if (s_bindJobQueue != NULL)
		return;

	if (option != NULL) {
		s_bindJobQueue = new paf::job::JobQueue("SceBindJobQueue", option);
	} else {
		paf::job::JobQueue::Option defaultOption;
		defaultOption.priority = 0x10000100;
		defaultOption.numThreads = 2;
		s_bindJobQueue = new paf::job::JobQueue("SceBindJobQueue", &defaultOption);
	}
}

/* 0x813C0322 */
void mtpJobQueueFini(void)
{
	if (s_bindJobQueue != NULL) {
		delete s_bindJobQueue;
		s_bindJobQueue = NULL;
	}
}
