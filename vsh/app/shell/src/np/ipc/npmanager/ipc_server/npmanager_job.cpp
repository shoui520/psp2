/*
 * npmanager_job.cpp: SceNpManagerIpcServerJob base class and 22 asynchronous
 * NP Manager IPC job implementations (0x81220064..0x812214B4, 0x814266E0..0x814277A8).
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <ipmi/session.h>
#include <np/np_common.h>
#include <np/np_auth_oauth.h>
#include "src/np/ipc/np_ipc_internal.h"

extern "C" {
extern void *sceNpCommonMallocNpServerName(SceSize size);
extern void sceNpCommonFreeNpServerName(void *ptr);
}

#if defined(FW_TYPE_CEX)
#define NPMANAGER_JOB_BUILD_ID "J01509657"
#elif FW_VERSION == 0x03630011
#define NPMANAGER_JOB_BUILD_ID "J01508051"
#else
#define NPMANAGER_JOB_BUILD_ID "J01509658"
#endif

static const char s_npManagerJobSourcePath[] =
	"W:\\Build\\" NPMANAGER_JOB_BUILD_ID "\\vsh\\app\\shell\\src\\np\\ipc\\npmanager\\ipc_server\\npmanager_job.cpp";

/*
 * Base class for all SceNpManager IPC worker jobs (48 bytes).
 * Vtable at 0x81534A6C; dtor 0x814266E0/0x81426704.
 */
class SceNpManagerIpcServerJob : public NpJobBase {
public:
	SceNpManagerIpcServerJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId)
		: NpJobBase("SceNpManagerIpcServerJob"),
		  m_session(session),
		  m_ipcReqId(ipcReqId),
		  m_coreReqHandle(npManagerResolveRequestHandle(clientReqId,
								session->getProcessId()))
	{
	}

	virtual ~SceNpManagerIpcServerJob()
	{
	}

protected:
	IPMI::Session *m_session;	/* +0x24 */
	SceUInt32 m_ipcReqId;		/* +0x28 */
	int m_coreReqHandle;		/* +0x2C */
};

/* 1. GetNpIdJob (84 bytes; Run 0x81426724, Finish 0x81426750) */
class NpGetNpIdJob : public SceNpManagerIpcServerJob {
public:
	NpGetNpIdJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId)
	{
	}

	virtual int Run()
	{
		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npAccountGetNpId(m_coreReqHandle, &m_npId);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED)
			npManagerSendGetNpIdResponse(m_session, m_ipcReqId, result, &m_npId);
		npLwMutexUnlock(mutex);
	}

private:
	SceNpId m_npId;			/* +0x30 (36 bytes) */
};

/* 2. GetNpIdForceJob (84 bytes; Run 0x81426798, Finish 0x814267C4) */
class NpGetNpIdForceJob : public SceNpManagerIpcServerJob {
public:
	NpGetNpIdForceJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId)
	{
	}

	virtual int Run()
	{
		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npAccountGetNpIdForce(m_coreReqHandle, &m_npId);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED)
			npManagerSendGetNpIdForceResponse(m_session, m_ipcReqId, result, &m_npId);
		npLwMutexUnlock(mutex);
	}

private:
	SceNpId m_npId;			/* +0x30 */
};

/* 3. GetTicketJob (92 bytes; Run 0x8142680C, Finish 0x81426860) */
class NpGetTicketJob : public SceNpManagerIpcServerJob {
public:
	NpGetTicketJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
		       SceSize bufferSize)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_ticketSize(bufferSize),
		  m_ticketBuf(NULL)
	{
	}

	virtual int Run()
	{
		if (m_ticketSize != 0)
			m_ticketBuf = sceNpCommonMallocNpServerName(m_ticketSize);
		else
			m_ticketBuf = NULL;

		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npAccountGetTicket(m_coreReqHandle, m_ticketBuf, &m_ticketSize);
		SceShell_794BF5BE(netToken);
		if (ret < 0 && m_ticketBuf != NULL) {
			sceNpCommonFreeNpServerName(m_ticketBuf);
			m_ticketBuf = NULL;
		}
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED) {
			SceSize copySize = (m_ticketBuf != NULL) ? m_ticketSize : 0;
			npManagerSendGetTicketResponse(m_session, m_ipcReqId, result,
						       &m_ticketSize, &copySize, m_ticketBuf);
		}
		if (m_ticketBuf != NULL) {
			sceNpCommonFreeNpServerName(m_ticketBuf);
			m_ticketBuf = NULL;
		}
		npLwMutexUnlock(mutex);
	}

private:
	SceNpId m_unusedNpId;		/* +0x30 (36 bytes, reserved in base layout) */
	SceSize m_ticketSize;		/* +0x54 */
	void *m_ticketBuf;		/* +0x58 */
};

/* 4. GetTicketForceJob (92 bytes; Run 0x814268FC, Finish 0x81426950) */
class NpGetTicketForceJob : public SceNpManagerIpcServerJob {
public:
	NpGetTicketForceJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
			    SceSize bufferSize)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_ticketSize(bufferSize),
		  m_ticketBuf(NULL)
	{
	}

	virtual int Run()
	{
		if (m_ticketSize != 0)
			m_ticketBuf = sceNpCommonMallocNpServerName(m_ticketSize);
		else
			m_ticketBuf = NULL;

		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npAccountGetTicketForce(m_coreReqHandle, m_ticketBuf, &m_ticketSize);
		SceShell_794BF5BE(netToken);
		if (ret < 0 && m_ticketBuf != NULL) {
			sceNpCommonFreeNpServerName(m_ticketBuf);
			m_ticketBuf = NULL;
		}
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED) {
			SceSize copySize = (m_ticketBuf != NULL) ? m_ticketSize : 0;
			npManagerSendGetTicketForceResponse(m_session, m_ipcReqId, result,
							    &m_ticketSize, &copySize, m_ticketBuf);
		}
		if (m_ticketBuf != NULL) {
			sceNpCommonFreeNpServerName(m_ticketBuf);
			m_ticketBuf = NULL;
		}
		npLwMutexUnlock(mutex);
	}

private:
	SceNpId m_unusedNpId;		/* +0x30 */
	SceSize m_ticketSize;		/* +0x54 */
	void *m_ticketBuf;		/* +0x58 */
};

/* 5. GetTicketCachedJob (92 bytes; Run 0x814269EC, Finish 0x81426A40) */
class NpGetTicketCachedJob : public SceNpManagerIpcServerJob {
public:
	NpGetTicketCachedJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
			     SceSize bufferSize)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_ticketSize(bufferSize),
		  m_ticketBuf(NULL)
	{
	}

	virtual int Run()
	{
		if (m_ticketSize != 0)
			m_ticketBuf = sceNpCommonMallocNpServerName(m_ticketSize);
		else
			m_ticketBuf = NULL;

		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npAccountGetTicketCached(m_coreReqHandle, m_ticketBuf, &m_ticketSize);
		SceShell_794BF5BE(netToken);
		if (ret < 0 && m_ticketBuf != NULL) {
			sceNpCommonFreeNpServerName(m_ticketBuf);
			m_ticketBuf = NULL;
		}
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED) {
			SceSize copySize = (m_ticketBuf != NULL) ? m_ticketSize : 0;
			npManagerSendGetTicketCachedResponse(m_session, m_ipcReqId, result,
							     &m_ticketSize, &copySize, m_ticketBuf);
		}
		if (m_ticketBuf != NULL) {
			sceNpCommonFreeNpServerName(m_ticketBuf);
			m_ticketBuf = NULL;
		}
		npLwMutexUnlock(mutex);
	}

private:
	SceNpId m_unusedNpId;		/* +0x30 */
	SceSize m_ticketSize;		/* +0x54 */
	void *m_ticketBuf;		/* +0x58 */
};

/* 6. GetAccountInfoJob (536 bytes; Run 0x81426ADC, Finish 0x81426B0C) */
class NpGetAccountInfoJob : public SceNpManagerIpcServerJob {
public:
	NpGetAccountInfoJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
			    int wantProfile, int wantMeta)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_wantProfile(wantProfile),
		  m_wantMeta(wantMeta)
	{
	}

	virtual int Run()
	{
		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npAccountGetProfileAndMeta(m_coreReqHandle, m_profileBuf,
						     m_accountMetaBuf);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED) {
			npManagerSendGetAccountInfoResponse(m_session, m_ipcReqId, result,
							    m_profileBuf, m_accountMetaBuf);
		}
		npLwMutexUnlock(mutex);
	}

private:
	SceUInt8 m_profileBuf[464];	/* +0x30 */
	SceUInt8 m_accountMetaBuf[16];	/* +0x200 */
	int m_wantProfile;		/* +0x210 */
	int m_wantMeta;			/* +0x214 */
};

/* 7. UpdateAccountJob (56 bytes; Run 0x81426B60, Finish 0x81426B8C) */
class NpUpdateAccountJob : public SceNpManagerIpcServerJob {
public:
	NpUpdateAccountJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
			   const void *loginId, const void *password)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_loginId(loginId),
		  m_password(password)
	{
	}

	virtual int Run()
	{
		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npAccountUpdateCredentials(m_coreReqHandle, m_loginId, m_password);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED)
			npManagerSendUpdateAccountResponse(m_session, m_ipcReqId, result);
		npLwMutexUnlock(mutex);
	}

private:
	const void *m_loginId;		/* +0x30 */
	const void *m_password;		/* +0x34 */
};

/* 8. CheckSysUpdateJob (64 bytes; Run 0x81426BD0, Finish 0x81426C08) */
class NpCheckSysUpdateJob : public SceNpManagerIpcServerJob {
public:
	NpCheckSysUpdateJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
			    int checkType, int checkMode, int checkFlag)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_checkType(checkType),
		  m_checkMode(checkMode),
		  m_checkFlag(checkFlag),
		  m_outStatus(0)
	{
	}

	virtual int Run()
	{
		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npSysUpdateCheck(m_coreReqHandle, m_checkType, &m_outStatus,
					   m_checkMode, m_checkFlag);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED) {
			npManagerSendCheckSysUpdateResponse(m_session, m_ipcReqId, result,
							    &m_outStatus);
		}
		npLwMutexUnlock(mutex);
	}

private:
	int m_checkType;		/* +0x30 */
	int m_checkMode;		/* +0x34 */
	int m_checkFlag;		/* +0x38 */
	int m_outStatus;		/* +0x3C */
};

/* 9. NpOAuthGetAccessTokenJob (312 bytes; Run 0x81426C50, Finish 0x81426C7C) */
class NpOAuthGetAccessTokenJob : public SceNpManagerIpcServerJob {
public:
	NpOAuthGetAccessTokenJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId)
	{
		memset(m_accessTokenBuf, 0, sizeof(m_accessTokenBuf));
	}

	virtual int Run()
	{
		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npOAuthGetAccessToken(m_coreReqHandle, m_accessTokenBuf);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED) {
			npManagerSendOAuthGetAccessTokenResponse(m_session, m_ipcReqId, result,
								 m_accessTokenBuf);
		}
		npLwMutexUnlock(mutex);
	}

private:
	SceUInt8 m_accessTokenBuf[264];	/* +0x30 */
};

/* 10. NpOAuthGetAuthorizationCodeJob (1352 bytes; Run 0x81426CC4, Finish 0x81426CF4) */
class NpOAuthGetAuthorizationCodeJob : public SceNpManagerIpcServerJob {
public:
	NpOAuthGetAuthorizationCodeJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
				       const SceNpClientId *clientId, const char *scope)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_issuerId(0)
	{
		memset(&m_request, 0, sizeof(m_request));
		memset(&m_authCode, 0, sizeof(m_authCode));
		if (sceClibStrnlen(scope, 0xFFFFFFFFU) > 0x400U) {
			int printed = sceClibPrintf("Assertion in %s(%s:%d): %s(%s)\n",
						    "NpOAuthGetAuthorizationCodeJob",
						    s_npManagerJobSourcePath, 1005,
						    "ASSERT", "0");
			SceNpCommonPs4_F50BD96A(printed);
		}
		sceClibSnprintf(m_request.scope, 0x400U, scope);
		m_request.clientId = *clientId;
	}

	virtual int Run()
	{
		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npOAuthGetAuthorizationCode(m_coreReqHandle, &m_request,
						      &m_authCode, &m_issuerId);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED) {
			npManagerSendOAuthGetAuthCodeResponse(m_session, m_ipcReqId, result,
							      &m_authCode, &m_issuerId);
		}
		npLwMutexUnlock(mutex);
	}

private:
	NpOAuthAuthCodeRequest m_request;	/* +0x30 (1161 bytes) */
	SceNpAuthorizationCode m_authCode;	/* +0x4B9 (136 bytes) */
	int m_issuerId;				/* +0x544 */
};

/* 11. NpOAuthGetAuthorizationCodeWithRedirectUriSpecifiedJob (3400 bytes; 0x81426D48/0x81426D78) */
class NpOAuthGetAuthorizationCodeWithRedirectUriSpecifiedJob : public SceNpManagerIpcServerJob {
public:
	NpOAuthGetAuthorizationCodeWithRedirectUriSpecifiedJob(IPMI::Session *session,
							       SceUInt32 ipcReqId,
							       int clientReqId,
							       const SceNpClientId *clientId,
							       const char *scope,
							       const char *redirectUri)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_issuerId(0)
	{
		memset(&m_request, 0, sizeof(m_request));
		memset(&m_authCode, 0, sizeof(m_authCode));
		m_request.clientId = *clientId;

		if (sceClibStrnlen(scope, 0xFFFFFFFFU) > 0x400U) {
			int printed = sceClibPrintf(
				"Assertion in %s(%s:%d): %s(%s)\n",
				"NpOAuthGetAuthorizationCodeWithRedirectUriSpecifiedJob",
				s_npManagerJobSourcePath, 1095, "ASSERT", "0");
			SceNpCommonPs4_F50BD96A(printed);
		}
		sceClibSnprintf(m_request.scope, 0x400U, scope);

		if (sceClibStrnlen(redirectUri, 0xFFFFFFFFU) > 0x800U) {
			int printed = sceClibPrintf(
				"Assertion in %s(%s:%d): %s(%s)\n",
				"NpOAuthGetAuthorizationCodeWithRedirectUriSpecifiedJob",
				s_npManagerJobSourcePath, 1100, "ASSERT", "0");
			SceNpCommonPs4_F50BD96A(printed);
		}
		sceClibSnprintf(m_request.redirectUri, 0x800U, redirectUri);
	}

	virtual int Run()
	{
		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npOAuthGetAuthorizationCodeWithRedirect(m_coreReqHandle, &m_request,
								  &m_authCode, &m_issuerId);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED) {
			npManagerSendOAuthGetAuthCodeWithRedirectResponse(
				m_session, m_ipcReqId, result, &m_authCode, &m_issuerId);
		}
		npLwMutexUnlock(mutex);
	}

private:
	NpOAuthAuthCodeWithRedirectRequest m_request;	/* +0x30 (3210 bytes) */
	SceNpAuthorizationCode m_authCode;		/* +0xCBA (136 bytes) */
	int m_issuerId;					/* +0xD44 */
};

/* 12. GetProfileJob (512 bytes; Run 0x81426DCC, Finish 0x81426DF8) */
class NpGetProfileJob : public SceNpManagerIpcServerJob {
public:
	NpGetProfileJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
			const void *npIdOrJid, int langCode)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_npIdOrJid(npIdOrJid),
		  m_langCode(langCode)
	{
	}

	virtual int Run()
	{
		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npProfileGetProfile(m_coreReqHandle, m_npIdOrJid, m_langCode,
					      m_profileBuf);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED)
			npManagerSendGetProfileResponse(m_session, m_ipcReqId, result,
							m_profileBuf);
		npLwMutexUnlock(mutex);
	}

private:
	const void *m_npIdOrJid;	/* +0x30 */
	int m_langCode;			/* +0x34 */
	SceUInt8 m_profileBuf[456];	/* +0x38 */
};

/* 13. UpdateProfileJob (568 bytes; Run 0x81426E38, Finish 0x81426EB8) */
class NpUpdateProfileJob : public SceNpManagerIpcServerJob {
public:
	NpUpdateProfileJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
			   const void *aboutMe, const void *avatarUrl, const void *languages,
			   const int *panelColor, const void *panelId, int countryCode,
			   const int *privacyFlag, const void *commId, int avatarId,
			   const void *customBuf, SceSize customSize)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_aboutMe(aboutMe),
		  m_avatarUrl(avatarUrl),
		  m_languages(languages),
		  m_panelColor(panelColor),
		  m_panelId(panelId),
		  m_countryCode(countryCode),
		  m_privacyFlag(privacyFlag),
		  m_commId(commId),
		  m_avatarId(avatarId),
		  m_customBuf(customBuf),
		  m_customSize(customSize),
		  m_reservedAlign(0)
	{
	}

	virtual int Run()
	{
		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npProfileUpdateProfile(m_coreReqHandle, m_aboutMe, m_avatarUrl,
						 m_languages, m_panelColor, m_panelId,
						 m_countryCode, m_privacyFlag, m_commId,
						 m_avatarId, m_profileBuf, &m_outStatus,
						 &m_outFlags, &m_outStamp, m_customBuf,
						 m_customSize);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED) {
			npManagerSendUpdateProfileResponse(m_session, m_ipcReqId, result,
							   m_profileBuf, &m_outStatus,
							   &m_outFlags, &m_outStamp);
		}
		npLwMutexUnlock(mutex);
	}

private:
	const void *m_aboutMe;		/* +0x30 */
	const void *m_avatarUrl;	/* +0x34 */
	const void *m_languages;	/* +0x38 */
	const int *m_panelColor;	/* +0x3C */
	const void *m_panelId;		/* +0x40 */
	int m_countryCode;		/* +0x44 */
	const int *m_privacyFlag;	/* +0x48 */
	const void *m_commId;		/* +0x4C */
	int m_avatarId;			/* +0x50 */
	const void *m_customBuf;	/* +0x54 */
	SceSize m_customSize;		/* +0x58 */
	int m_reservedAlign;		/* +0x5C */
	SceUInt8 m_profileBuf[456];	/* +0x60 */
	int m_outStatus;		/* +0x228 */
	int m_outFlags;			/* +0x22C */
	SceUInt64 m_outStamp;		/* +0x230 */
};

/* 14. GetAvatarListJob (68 bytes; Run 0x81426F10, Finish 0x81426F68) */
class NpGetAvatarListJob : public SceNpManagerIpcServerJob {
public:
	NpGetAvatarListJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
			   int categoryId, int startOffset, SceUInt32 maxCount)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_categoryId(categoryId),
		  m_startOffset(startOffset),
		  m_maxCount(maxCount),
		  m_entriesBuf(NULL),
		  m_outTotal(0)
	{
	}

	virtual int Run()
	{
		m_entriesBuf = sceNpCommonMallocNpServerName(412 * m_maxCount);
		if (m_entriesBuf == NULL)
			return NP_MGR_ERR_NO_MEMORY;

		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npProfileGetAvatarList(m_coreReqHandle, m_categoryId, m_startOffset,
						 m_entriesBuf, m_maxCount, &m_outTotal);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED) {
			if (m_entriesBuf == NULL)
				m_maxCount = 0;
			npManagerSendGetAvatarListResponse(m_session, m_ipcReqId, result,
							   &m_maxCount, m_entriesBuf, &m_outTotal);
		}
		if (m_entriesBuf != NULL) {
			sceNpCommonFreeNpServerName(m_entriesBuf);
			m_entriesBuf = NULL;
		}
		npLwMutexUnlock(mutex);
	}

private:
	int m_categoryId;		/* +0x30 */
	int m_startOffset;		/* +0x34 */
	SceUInt32 m_maxCount;		/* +0x38 */
	void *m_entriesBuf;		/* +0x3C */
	int m_outTotal;			/* +0x40 */
};

/* 15. GetAvatarUrlsJob (76 bytes; Run 0x81426FCC, Finish 0x81427044) */
class NpGetAvatarUrlsJob : public SceNpManagerIpcServerJob {
public:
	NpGetAvatarUrlsJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
			   int categoryId, int startOffset, int sortMode, SceUInt32 maxCount)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_categoryId(categoryId),
		  m_startOffset(startOffset),
		  m_sortMode(sortMode),
		  m_maxCount(maxCount),
		  m_avatarIds(NULL),
		  m_urlEntries(NULL),
		  m_outTotal(0)
	{
	}

	virtual int Run()
	{
		m_avatarIds = static_cast<int *>(
			sceNpCommonMallocNpServerName(sizeof(int) * m_maxCount));
		if (m_avatarIds == NULL)
			return NP_MGR_ERR_NO_MEMORY;
		m_urlEntries = sceNpCommonMallocNpServerName(128 * m_maxCount);
		if (m_urlEntries == NULL)
			return NP_MGR_ERR_NO_MEMORY;

		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npProfileGetAvatarUrls(m_coreReqHandle, &m_categoryId, m_startOffset,
						 m_sortMode, m_avatarIds, m_urlEntries,
						 m_maxCount, &m_outTotal);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED) {
			if (m_avatarIds == NULL || m_urlEntries == NULL)
				m_maxCount = 0;
			npManagerSendGetAvatarUrlsResponse(m_session, m_ipcReqId, result,
							   &m_maxCount, m_avatarIds,
							   m_urlEntries, &m_outTotal);
		}
		if (m_avatarIds != NULL) {
			sceNpCommonFreeNpServerName(m_avatarIds);
			m_avatarIds = NULL;
		}
		if (m_urlEntries != NULL) {
			sceNpCommonFreeNpServerName(m_urlEntries);
			m_urlEntries = NULL;
		}
		npLwMutexUnlock(mutex);
	}

private:
	int m_categoryId;		/* +0x30 */
	int m_startOffset;		/* +0x34 */
	int m_sortMode;			/* +0x38 */
	SceUInt32 m_maxCount;		/* +0x3C */
	int *m_avatarIds;		/* +0x40 */
	void *m_urlEntries;		/* +0x44 */
	int m_outTotal;			/* +0x48 */
};

/* 16. GetPanelCategoriesJob (68 bytes; Run 0x814270BC, Finish 0x81427114) */
class NpGetPanelCategoriesJob : public SceNpManagerIpcServerJob {
public:
	NpGetPanelCategoriesJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
				int startOffset, int sortMode, SceUInt32 maxCount)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_startOffset(startOffset),
		  m_sortMode(sortMode),
		  m_maxCount(maxCount),
		  m_entriesBuf(NULL),
		  m_outTotal(0)
	{
	}

	virtual int Run()
	{
		m_entriesBuf = sceNpCommonMallocNpServerName(412 * m_maxCount);
		if (m_entriesBuf == NULL)
			return NP_MGR_ERR_NO_MEMORY;

		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npProfileGetPanelCategories(m_coreReqHandle, m_startOffset, m_sortMode,
						      m_entriesBuf, m_maxCount, &m_outTotal);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED) {
			if (m_entriesBuf == NULL)
				m_maxCount = 0;
			npManagerSendGetPanelCategoriesResponse(m_session, m_ipcReqId, result,
								&m_maxCount, m_entriesBuf,
								&m_outTotal);
		}
		if (m_entriesBuf != NULL) {
			sceNpCommonFreeNpServerName(m_entriesBuf);
			m_entriesBuf = NULL;
		}
		npLwMutexUnlock(mutex);
	}

private:
	int m_startOffset;		/* +0x30 */
	int m_sortMode;			/* +0x34 */
	SceUInt32 m_maxCount;		/* +0x38 */
	void *m_entriesBuf;		/* +0x3C */
	int m_outTotal;			/* +0x40 */
};

/* 17. GetPanelUrlsJob (80 bytes; Run 0x81427178, Finish 0x81427210) */
class NpGetPanelUrlsJob : public SceNpManagerIpcServerJob {
public:
	NpGetPanelUrlsJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
			  int categoryId, int startOffset, int sortMode, SceUInt32 maxCount)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_categoryId(categoryId),
		  m_startOffset(startOffset),
		  m_sortMode(sortMode),
		  m_maxCount(maxCount),
		  m_panelIds(NULL),
		  m_bgColors(NULL),
		  m_urlEntries(NULL),
		  m_outTotal(0)
	{
	}

	virtual int Run()
	{
		m_panelIds = static_cast<int *>(
			sceNpCommonMallocNpServerName(sizeof(int) * m_maxCount));
		if (m_panelIds == NULL)
			return NP_MGR_ERR_NO_MEMORY;
		m_bgColors = static_cast<int *>(
			sceNpCommonMallocNpServerName(sizeof(int) * m_maxCount));
		if (m_bgColors == NULL)
			return NP_MGR_ERR_NO_MEMORY;
		m_urlEntries = sceNpCommonMallocNpServerName(128 * m_maxCount);
		if (m_urlEntries == NULL)
			return NP_MGR_ERR_NO_MEMORY;

		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npProfileGetPanelUrls(m_coreReqHandle, &m_categoryId, m_startOffset,
						m_sortMode, m_panelIds, m_bgColors, m_urlEntries,
						m_maxCount, &m_outTotal);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED) {
			if (m_panelIds == NULL || m_bgColors == NULL || m_urlEntries == NULL)
				m_maxCount = 0;
			npManagerSendGetPanelUrlsResponse(m_session, m_ipcReqId, result,
							  &m_maxCount, m_panelIds, m_bgColors,
							  m_urlEntries, &m_outTotal);
		}
		if (m_panelIds != NULL) {
			sceNpCommonFreeNpServerName(m_panelIds);
			m_panelIds = NULL;
		}
		if (m_bgColors != NULL) {
			sceNpCommonFreeNpServerName(m_bgColors);
			m_bgColors = NULL;
		}
		if (m_urlEntries != NULL) {
			sceNpCommonFreeNpServerName(m_urlEntries);
			m_urlEntries = NULL;
		}
		npLwMutexUnlock(mutex);
	}

private:
	int m_categoryId;		/* +0x30 */
	int m_startOffset;		/* +0x34 */
	int m_sortMode;			/* +0x38 */
	SceUInt32 m_maxCount;		/* +0x3C */
	int *m_panelIds;		/* +0x40 */
	int *m_bgColors;		/* +0x44 */
	void *m_urlEntries;		/* +0x48 */
	int m_outTotal;			/* +0x4C */
};

/* 18. GetCatalogListJob (68 bytes; Run 0x8142729C, Finish 0x814272F4) */
class NpGetCatalogListJob : public SceNpManagerIpcServerJob {
public:
	NpGetCatalogListJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
			    int categoryId, int startOffset, SceUInt32 maxCount)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_categoryId(categoryId),
		  m_startOffset(startOffset),
		  m_maxCount(maxCount),
		  m_entriesBuf(NULL),
		  m_outTotal(0)
	{
	}

	virtual int Run()
	{
		m_entriesBuf = sceNpCommonMallocNpServerName(412 * m_maxCount);
		if (m_entriesBuf == NULL)
			return NP_MGR_ERR_NO_MEMORY;

		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npProfileGetCatalogList(m_coreReqHandle, m_categoryId, m_startOffset,
						  m_entriesBuf, m_maxCount, &m_outTotal);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED) {
			if (m_entriesBuf == NULL)
				m_maxCount = 0;
			npManagerSendGetCatalogListResponse(m_session, m_ipcReqId, result,
							    &m_maxCount, m_entriesBuf,
							    &m_outTotal);
		}
		if (m_entriesBuf != NULL) {
			sceNpCommonFreeNpServerName(m_entriesBuf);
			m_entriesBuf = NULL;
		}
		npLwMutexUnlock(mutex);
	}

private:
	int m_categoryId;		/* +0x30 */
	int m_startOffset;		/* +0x34 */
	SceUInt32 m_maxCount;		/* +0x38 */
	void *m_entriesBuf;		/* +0x3C */
	int m_outTotal;			/* +0x40 */
};

/* 19. GetAvatarBatchJob (96 bytes; Run 0x81427358, Finish 0x81427410) */
class NpGetAvatarBatchJob : public SceNpManagerIpcServerJob {
public:
	NpGetAvatarBatchJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
			    const void *ticketBuf, SceSize ticketSize, int categoryId,
			    int startOffset, int sortMode, SceUInt32 maxCount, int skipExtraIds)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_ticketBuf(ticketBuf),
		  m_ticketSize(ticketSize),
		  m_categoryId(categoryId),
		  m_startOffset(startOffset),
		  m_sortMode(sortMode),
		  m_maxCount(maxCount),
		  m_skipExtraIds(skipExtraIds),
		  m_avatarIds(NULL),
		  m_urlEntries(NULL),
		  m_extraIds(NULL),
		  m_outTotal(0),
		  m_outNextOffset(0)
	{
	}

	virtual int Run()
	{
		m_avatarIds = static_cast<int *>(
			sceNpCommonMallocNpServerName(sizeof(int) * m_maxCount));
		if (m_avatarIds == NULL)
			return NP_MGR_ERR_NO_MEMORY;
		m_urlEntries = sceNpCommonMallocNpServerName(128 * m_maxCount);
		if (m_urlEntries == NULL)
			return NP_MGR_ERR_NO_MEMORY;

		if (m_skipExtraIds != 0) {
			m_extraIds = NULL;
		} else {
			m_extraIds = static_cast<int *>(
				sceNpCommonMallocNpServerName(sizeof(int) * m_maxCount));
			if (m_extraIds == NULL)
				return NP_MGR_ERR_NO_MEMORY;
		}

		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npProfileGetAvatarBatch(m_coreReqHandle, m_ticketBuf, m_ticketSize,
						  &m_categoryId, m_startOffset, m_sortMode,
						  m_avatarIds, m_urlEntries, m_extraIds,
						  m_maxCount, &m_outTotal, &m_outNextOffset);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED) {
			SceUInt32 extraCount = (m_extraIds != NULL) ? m_maxCount : 0;
			if (m_avatarIds == NULL || m_urlEntries == NULL)
				m_maxCount = 0;
			npManagerSendGetAvatarBatchResponse(m_session, m_ipcReqId, result,
							    &m_maxCount, m_avatarIds,
							    m_urlEntries, &extraCount,
							    m_extraIds, &m_outTotal,
							    &m_outNextOffset);
		}
		if (m_avatarIds != NULL) {
			sceNpCommonFreeNpServerName(m_avatarIds);
			m_avatarIds = NULL;
		}
		if (m_urlEntries != NULL) {
			sceNpCommonFreeNpServerName(m_urlEntries);
			m_urlEntries = NULL;
		}
		if (m_extraIds != NULL) {
			sceNpCommonFreeNpServerName(m_extraIds);
			m_extraIds = NULL;
		}
		npLwMutexUnlock(mutex);
	}

private:
	const void *m_ticketBuf;	/* +0x30 */
	SceSize m_ticketSize;		/* +0x34 */
	int m_categoryId;		/* +0x38 */
	int m_startOffset;		/* +0x3C */
	int m_sortMode;			/* +0x40 */
	SceUInt32 m_maxCount;		/* +0x44 */
	int m_skipExtraIds;		/* +0x48 */
	int *m_avatarIds;		/* +0x4C */
	void *m_urlEntries;		/* +0x50 */
	int *m_extraIds;		/* +0x54 */
	int m_outTotal;			/* +0x58 */
	int m_outNextOffset;		/* +0x5C */
};

/* 20. GetAvatarUrlJob (192 bytes; Run 0x814274D4, Finish 0x81427510) */
class NpGetAvatarUrlJob : public SceNpManagerIpcServerJob {
public:
	NpGetAvatarUrlJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
			  const void *ticketBuf, SceSize ticketSize, int avatarId, int sizeType)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_ticketBuf(ticketBuf),
		  m_ticketSize(ticketSize),
		  m_avatarId(avatarId),
		  m_sizeType(sizeType)
	{
	}

	virtual int Run()
	{
		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npProfileGetAvatarUrl(m_coreReqHandle, m_ticketBuf, m_ticketSize,
						m_avatarId, m_sizeType, m_urlBuf);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED)
			npManagerSendGetAvatarUrlResponse(m_session, m_ipcReqId, result, m_urlBuf);
		npLwMutexUnlock(mutex);
	}

private:
	const void *m_ticketBuf;	/* +0x30 */
	SceSize m_ticketSize;		/* +0x34 */
	int m_avatarId;			/* +0x38 */
	int m_sizeType;			/* +0x3C */
	char m_urlBuf[128];		/* +0x40 */
};

/* 21. GetTicketAvatarBatchJob (96 bytes; Run 0x81427550, Finish 0x81427670) */
class NpGetTicketAvatarBatchJob : public SceNpManagerIpcServerJob {
public:
	NpGetTicketAvatarBatchJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
				  int authReqId, SceSize ticketSize, int categoryId,
				  int startOffset, int sortMode, SceUInt32 maxCount,
				  int skipExtraIds)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_authReqId(authReqId),
		  m_ticketSize(ticketSize),
		  m_categoryId(categoryId),
		  m_startOffset(startOffset),
		  m_sortMode(sortMode),
		  m_maxCount(maxCount),
		  m_skipExtraIds(skipExtraIds),
		  m_avatarIds(NULL),
		  m_urlEntries(NULL),
		  m_extraIds(NULL),
		  m_outTotal(0),
		  m_outNextOffset(0)
	{
	}

	virtual int Run()
	{
		void *ticketBuf = sceNpCommonMallocNpServerName(m_ticketSize);
		if (ticketBuf == NULL)
			return NP_MGR_ERR_NO_MEMORY;

		int ret = NP_MGR_ERR_NO_MEMORY;
		m_avatarIds = static_cast<int *>(
			sceNpCommonMallocNpServerName(sizeof(int) * m_maxCount));
		if (m_avatarIds != NULL) {
			m_urlEntries = sceNpCommonMallocNpServerName(128 * m_maxCount);
			if (m_urlEntries != NULL) {
				bool extraOk = true;
				if (m_skipExtraIds != 0) {
					m_extraIds = NULL;
				} else {
					m_extraIds = static_cast<int *>(
						sceNpCommonMallocNpServerName(sizeof(int) * m_maxCount));
					if (m_extraIds == NULL)
						extraOk = false;
				}
				if (extraOk) {
					ret = npAuthServerReadTicket(NULL, m_authReqId, ticketBuf,
								     0, m_ticketSize);
					if (ret >= 0) {
						unsigned int netToken =
							SceShell_6B8772A2(m_session->getProcessId());
						ret = npProfileGetAvatarBatch(
							m_coreReqHandle, ticketBuf, m_ticketSize,
							&m_categoryId, m_startOffset, m_sortMode,
							m_avatarIds, m_urlEntries, m_extraIds,
							m_maxCount, &m_outTotal, &m_outNextOffset);
						SceShell_794BF5BE(netToken);
					}
				}
			}
		}

		sceNpCommonFreeNpServerName(ticketBuf);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED) {
			SceUInt32 extraCount = (m_extraIds != NULL) ? m_maxCount : 0;
			if (m_avatarIds == NULL || m_urlEntries == NULL)
				m_maxCount = 0;
			npManagerSendGetTicketAvatarBatchResponse(m_session, m_ipcReqId, result,
								  &m_maxCount, m_avatarIds,
								  m_urlEntries, &extraCount,
								  m_extraIds, &m_outTotal,
								  &m_outNextOffset);
		}
		if (m_avatarIds != NULL) {
			sceNpCommonFreeNpServerName(m_avatarIds);
			m_avatarIds = NULL;
		}
		if (m_urlEntries != NULL) {
			sceNpCommonFreeNpServerName(m_urlEntries);
			m_urlEntries = NULL;
		}
		if (m_extraIds != NULL) {
			sceNpCommonFreeNpServerName(m_extraIds);
			m_extraIds = NULL;
		}
		npLwMutexUnlock(mutex);
	}

private:
	int m_authReqId;		/* +0x30 */
	SceSize m_ticketSize;		/* +0x34 */
	int m_categoryId;		/* +0x38 */
	int m_startOffset;		/* +0x3C */
	int m_sortMode;			/* +0x40 */
	SceUInt32 m_maxCount;		/* +0x44 */
	int m_skipExtraIds;		/* +0x48 */
	int *m_avatarIds;		/* +0x4C */
	void *m_urlEntries;		/* +0x50 */
	int *m_extraIds;		/* +0x54 */
	int m_outTotal;			/* +0x58 */
	int m_outNextOffset;		/* +0x5C */
};

/* 22. GetTicketAvatarUrlJob (192 bytes; Run 0x81427734, Finish 0x814277A8) */
class NpGetTicketAvatarUrlJob : public SceNpManagerIpcServerJob {
public:
	NpGetTicketAvatarUrlJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
				int authReqId, SceSize ticketSize, int avatarId, int sizeType)
		: SceNpManagerIpcServerJob(session, ipcReqId, clientReqId),
		  m_authReqId(authReqId),
		  m_ticketSize(ticketSize),
		  m_avatarId(avatarId),
		  m_sizeType(sizeType)
	{
	}

	virtual int Run()
	{
		void *ticketBuf = sceNpCommonMallocNpServerName(m_ticketSize);
		if (ticketBuf == NULL)
			return NP_MGR_ERR_NO_MEMORY;

		int ret = npAuthServerReadTicket(NULL, m_authReqId, ticketBuf, 0, m_ticketSize);
		if (ret >= 0) {
			unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
			ret = npProfileGetAvatarUrl(m_coreReqHandle, ticketBuf, m_ticketSize,
						    m_avatarId, m_sizeType, m_urlBuf);
			SceShell_794BF5BE(netToken);
		}
		sceNpCommonFreeNpServerName(ticketBuf);
		return ret;
	}

	virtual void Finish(int result)
	{
		NpIpcLwMutex *mutex = npManagerGetIpcMutex();
		npLwMutexLock(mutex);
		if (!IsCanceled() || result != NP_MGR_ERR_DESTROYED)
			npManagerSendGetTicketAvatarUrlResponse(m_session, m_ipcReqId, result,
								m_urlBuf);
		npLwMutexUnlock(mutex);
	}

private:
	int m_authReqId;		/* +0x30 */
	SceSize m_ticketSize;		/* +0x34 */
	int m_avatarId;			/* +0x38 */
	int m_sizeType;			/* +0x3C */
	char m_urlBuf[128];		/* +0x40 */
};

/* Enqueue entrypoints (0x81220064..0x812213D6) */

/* 0x81220064 */
int npManagerEnqueueGetNpIdJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId)
{
	NpGetNpIdJob *job = new NpGetNpIdJob(session, ipcReqId, clientReqId);
	int ret = NP_MGR_ERR_NO_MEMORY;
	if (job != NULL) {
		int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
		ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	SceNpId emptyNpId;
	sceClibMemset(&emptyNpId, 0, sizeof(emptyNpId));
	npManagerSendGetNpIdResponse(session, ipcReqId, ret, &emptyNpId);
	return 0;
}

/* 0x8122013A */
int npManagerEnqueueGetNpIdForceJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId)
{
	NpGetNpIdForceJob *job = new NpGetNpIdForceJob(session, ipcReqId, clientReqId);
	int ret = NP_MGR_ERR_NO_MEMORY;
	if (job != NULL) {
		int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
		ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	SceNpId emptyNpId;
	sceClibMemset(&emptyNpId, 0, sizeof(emptyNpId));
	npManagerSendGetNpIdForceResponse(session, ipcReqId, ret, &emptyNpId);
	return 0;
}

/* 0x81220210 */
int npManagerEnqueueGetTicketJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
				 SceSize bufferSize)
{
	NpGetTicketJob *job = new NpGetTicketJob(session, ipcReqId, clientReqId, bufferSize);
	int ret = NP_MGR_ERR_NO_MEMORY;
	if (job != NULL) {
		int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
		ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	SceSize zeroTotal = 0;
	SceSize zeroCopy = 0;
	npManagerSendGetTicketResponse(session, ipcReqId, ret, &zeroTotal, &zeroCopy, NULL);
	return 0;
}

/* 0x812202EA */
int npManagerEnqueueGetTicketForceJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
				      SceSize bufferSize)
{
	NpGetTicketForceJob *job = new NpGetTicketForceJob(session, ipcReqId, clientReqId,
							   bufferSize);
	int ret = NP_MGR_ERR_NO_MEMORY;
	if (job != NULL) {
		int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
		ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	SceSize zeroTotal = 0;
	SceSize zeroCopy = 0;
	npManagerSendGetTicketForceResponse(session, ipcReqId, ret, &zeroTotal, &zeroCopy, NULL);
	return 0;
}

/* 0x812203C4 */
int npManagerEnqueueGetTicketCachedJob(IPMI::Session *session, SceUInt32 ipcReqId,
				       int clientReqId, SceSize bufferSize)
{
	NpGetTicketCachedJob *job = new NpGetTicketCachedJob(session, ipcReqId, clientReqId,
							     bufferSize);
	int ret = NP_MGR_ERR_NO_MEMORY;
	if (job != NULL) {
		int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
		ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	SceSize zeroTotal = 0;
	SceSize zeroCopy = 0;
	npManagerSendGetTicketCachedResponse(session, ipcReqId, ret, &zeroTotal, &zeroCopy, NULL);
	return 0;
}

/* 0x8122049E */
int npManagerEnqueueGetAccountInfoJob(IPMI::Session *session, SceUInt32 ipcReqId,
				      int clientReqId, int wantProfile, int wantMeta)
{
	NpGetAccountInfoJob *job = new NpGetAccountInfoJob(session, ipcReqId, clientReqId,
							   wantProfile, wantMeta);
	int ret = NP_MGR_ERR_NO_MEMORY;
	if (job != NULL) {
		int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
		ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	SceUInt8 emptyProfile[464];
	SceUInt8 emptyMeta[16];
	sceClibMemset(emptyProfile, 0, sizeof(emptyProfile));
	sceClibMemset(emptyMeta, 0, sizeof(emptyMeta));
	npManagerSendGetAccountInfoResponse(session, ipcReqId, ret, emptyProfile, emptyMeta);
	return 0;
}

/* 0x81220594 */
int npManagerEnqueueUpdateAccountJob(IPMI::Session *session, SceUInt32 ipcReqId,
				     int clientReqId, const void *loginId, const void *password)
{
	NpUpdateAccountJob *job = new NpUpdateAccountJob(session, ipcReqId, clientReqId,
							 loginId, password);
	int ret = NP_MGR_ERR_NO_MEMORY;
	if (job != NULL) {
		int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
		ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	npManagerSendUpdateAccountResponse(session, ipcReqId, ret);
	return 0;
}

/* 0x81220642 */
int npManagerEnqueueCheckSysUpdateJob(IPMI::Session *session, SceUInt32 ipcReqId,
				      int clientReqId, int checkType, int checkMode, int checkFlag)
{
	NpCheckSysUpdateJob *job = new NpCheckSysUpdateJob(session, ipcReqId, clientReqId,
							   checkType, checkMode, checkFlag);
	int ret = NP_MGR_ERR_NO_MEMORY;
	if (job != NULL) {
		int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
		ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	int zeroStatus = 0;
	npManagerSendCheckSysUpdateResponse(session, ipcReqId, ret, &zeroStatus);
	return 0;
}

/* 0x81220730 */
int npManagerEnqueueOAuthGetAccessTokenJob(IPMI::Session *session, SceUInt32 ipcReqId,
					   int clientReqId)
{
	NpOAuthGetAccessTokenJob *job = new NpOAuthGetAccessTokenJob(session, ipcReqId,
								     clientReqId);
	if (job == NULL)
		return NP_MGR_ERR_NO_MEMORY;
	int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
	return npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
}

/* 0x812207AA */
int npManagerEnqueueOAuthGetAuthCodeJob(IPMI::Session *session, SceUInt32 ipcReqId,
					int clientReqId, const SceNpClientId *clientId,
					const char *scope)
{
	NpOAuthGetAuthorizationCodeJob *job = new NpOAuthGetAuthorizationCodeJob(
		session, ipcReqId, clientReqId, clientId, scope);
	if (job == NULL)
		return NP_MGR_ERR_NO_MEMORY;
	int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
	return npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
}

/* 0x812208BE */
int npManagerEnqueueOAuthGetAuthCodeWithRedirectJob(IPMI::Session *session, SceUInt32 ipcReqId,
						    int clientReqId,
						    const SceNpClientId *clientId,
						    const char *scope, const char *redirectUri)
{
	NpOAuthGetAuthorizationCodeWithRedirectUriSpecifiedJob *job =
		new NpOAuthGetAuthorizationCodeWithRedirectUriSpecifiedJob(
			session, ipcReqId, clientReqId, clientId, scope, redirectUri);
	if (job == NULL)
		return NP_MGR_ERR_NO_MEMORY;
	int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
	return npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
}

/* 0x81220A40 */
int npManagerEnqueueGetProfileJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
				  const void *npIdOrJid, int langCode)
{
	NpGetProfileJob *job = new NpGetProfileJob(session, ipcReqId, clientReqId,
						   npIdOrJid, langCode);
	if (job == NULL) {
		SceUInt8 emptyProfile[456];
		sceClibMemset(emptyProfile, 0, sizeof(emptyProfile));
		npManagerSendGetProfileResponse(session, ipcReqId, NP_MGR_ERR_NO_MEMORY,
						emptyProfile);
		return 0;
	}
	int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
	int ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81220B0C */
int npManagerEnqueueUpdateProfileJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
				     const void *aboutMe, int aboutMeAbsent,
				     const void *avatarUrl, int avatarUrlAbsent,
				     const void *languages, int languagesAbsent,
				     const int *panelColor, int panelColorAbsent,
				     const void *panelId, int panelIdAbsent,
				     int countryCode, const int *privacyFlag,
				     int privacyFlagAbsent, const void *commId,
				     int commIdAbsent, int avatarId, const void *customBuf,
				     SceSize customSize)
{
	NpUpdateProfileJob *job = new NpUpdateProfileJob(
		session, ipcReqId, clientReqId,
		aboutMeAbsent ? NULL : aboutMe,
		avatarUrlAbsent ? NULL : avatarUrl,
		languagesAbsent ? NULL : languages,
		panelColorAbsent ? NULL : panelColor,
		panelIdAbsent ? NULL : panelId,
		countryCode,
		privacyFlagAbsent ? NULL : privacyFlag,
		commIdAbsent ? NULL : commId,
		avatarId, customBuf, customSize);
	if (job == NULL) {
		SceUInt8 emptyProfile[456];
		int zeroStatus = 0;
		int zeroFlags = 0;
		SceUInt64 zeroStamp = 0;
		sceClibMemset(emptyProfile, 0, sizeof(emptyProfile));
		npManagerSendUpdateProfileResponse(session, ipcReqId, NP_MGR_ERR_NO_MEMORY,
						   emptyProfile, &zeroStatus, &zeroFlags,
						   &zeroStamp);
		return 0;
	}
	int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
	int ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81220CA8 */
int npManagerEnqueueGetAvatarListJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
				     int categoryId, int startOffset, SceUInt32 maxCount)
{
	NpGetAvatarListJob *job = new NpGetAvatarListJob(session, ipcReqId, clientReqId,
							 categoryId, startOffset, maxCount);
	if (job == NULL) {
		SceUInt32 zeroCount = 0;
		int zeroTotal = 0;
		npManagerSendGetAvatarListResponse(session, ipcReqId, NP_MGR_ERR_NO_MEMORY,
						   &zeroCount, NULL, &zeroTotal);
		return 0;
	}
	int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
	int ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81220D78 */
int npManagerEnqueueGetAvatarUrlsJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
				     const int *categoryIdPtr, int startOffset, int sortMode,
				     SceUInt32 maxCount)
{
	NpGetAvatarUrlsJob *job = new NpGetAvatarUrlsJob(session, ipcReqId, clientReqId,
							 *categoryIdPtr, startOffset, sortMode,
							 maxCount);
	if (job == NULL) {
		SceUInt32 zeroCount = 0;
		int zeroTotal = 0;
		npManagerSendGetAvatarUrlsResponse(session, ipcReqId, NP_MGR_ERR_NO_MEMORY,
						   &zeroCount, NULL, NULL, &zeroTotal);
		return 0;
	}
	int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
	int ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81220E5A */
int npManagerEnqueueGetPanelCategoriesJob(IPMI::Session *session, SceUInt32 ipcReqId,
					  int clientReqId, int startOffset, int sortMode,
					  SceUInt32 maxCount)
{
	NpGetPanelCategoriesJob *job = new NpGetPanelCategoriesJob(session, ipcReqId, clientReqId,
								   startOffset, sortMode, maxCount);
	if (job == NULL) {
		SceUInt32 zeroCount = 0;
		int zeroTotal = 0;
		npManagerSendGetPanelCategoriesResponse(session, ipcReqId, NP_MGR_ERR_NO_MEMORY,
							&zeroCount, NULL, &zeroTotal);
		return 0;
	}
	int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
	int ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81220F2A */
int npManagerEnqueueGetPanelUrlsJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
				    const int *categoryIdPtr, int startOffset, int sortMode,
				    SceUInt32 maxCount)
{
	NpGetPanelUrlsJob *job = new NpGetPanelUrlsJob(session, ipcReqId, clientReqId,
						       *categoryIdPtr, startOffset, sortMode,
						       maxCount);
	if (job == NULL) {
		SceUInt32 zeroCount = 0;
		int zeroTotal = 0;
		npManagerSendGetPanelUrlsResponse(session, ipcReqId, NP_MGR_ERR_NO_MEMORY,
						  &zeroCount, NULL, NULL, NULL, &zeroTotal);
		return 0;
	}
	int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
	int ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
	return (ret >= 0) ? 0 : ret;
}

/* 0x81221010 */
int npManagerEnqueueGetCatalogListJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
				      int categoryId, int startOffset, SceUInt32 maxCount)
{
	NpGetCatalogListJob *job = new NpGetCatalogListJob(session, ipcReqId, clientReqId,
							   categoryId, startOffset, maxCount);
	if (job == NULL) {
		SceUInt32 zeroCount = 0;
		int zeroTotal = 0;
		npManagerSendGetCatalogListResponse(session, ipcReqId, NP_MGR_ERR_NO_MEMORY,
						    &zeroCount, NULL, &zeroTotal);
		return 0;
	}
	int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
	int ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
	return (ret >= 0) ? 0 : ret;
}

/* 0x812210E0 */
int npManagerEnqueueGetAvatarBatchJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
				      const void *ticketBuf, SceSize ticketSize,
				      const int *categoryIdPtr, int startOffset, int sortMode,
				      SceUInt32 maxCount, int skipExtraIds)
{
	NpGetAvatarBatchJob *job = new NpGetAvatarBatchJob(
		session, ipcReqId, clientReqId, ticketBuf, ticketSize, *categoryIdPtr,
		startOffset, sortMode, maxCount, skipExtraIds);
	if (job == NULL) {
		SceUInt32 zeroCount = 0;
		SceUInt32 zeroExtra = 0;
		int zeroTotal = 0;
		int zeroNext = 0;
		npManagerSendGetAvatarBatchResponse(session, ipcReqId, NP_MGR_ERR_NO_MEMORY,
						    &zeroCount, NULL, NULL, &zeroExtra, NULL,
						    &zeroTotal, &zeroNext);
		return 0;
	}
	int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
	int ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
	return (ret >= 0) ? 0 : ret;
}

/* 0x812211E8 */
int npManagerEnqueueGetAvatarUrlJob(IPMI::Session *session, SceUInt32 ipcReqId, int clientReqId,
				    const void *ticketBuf, SceSize ticketSize, int avatarId,
				    int sizeType)
{
	NpGetAvatarUrlJob *job = new NpGetAvatarUrlJob(session, ipcReqId, clientReqId,
						       ticketBuf, ticketSize, avatarId, sizeType);
	if (job == NULL) {
		char emptyUrl[128];
		sceClibMemset(emptyUrl, 0, sizeof(emptyUrl));
		npManagerSendGetAvatarUrlResponse(session, ipcReqId, NP_MGR_ERR_NO_MEMORY,
						  emptyUrl);
		return 0;
	}
	int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
	int ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
	return (ret >= 0) ? 0 : ret;
}

/* 0x812212C4 */
int npManagerEnqueueGetTicketAvatarBatchJob(IPMI::Session *session, SceUInt32 ipcReqId,
					    int clientReqId, int authReqId, SceSize ticketSize,
					    const int *categoryIdPtr, int startOffset, int sortMode,
					    SceUInt32 maxCount, int skipExtraIds)
{
	NpGetTicketAvatarBatchJob *job = new NpGetTicketAvatarBatchJob(
		session, ipcReqId, clientReqId, authReqId, ticketSize, *categoryIdPtr,
		startOffset, sortMode, maxCount, skipExtraIds);
	if (job == NULL) {
		SceUInt32 zeroCount = 0;
		SceUInt32 zeroExtra = 0;
		int zeroTotal = 0;
		int zeroNext = 0;
		npManagerSendGetTicketAvatarBatchResponse(session, ipcReqId, NP_MGR_ERR_NO_MEMORY,
							  &zeroCount, NULL, NULL, &zeroExtra,
							  NULL, &zeroTotal, &zeroNext);
		return 0;
	}
	int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
	int ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
	return (ret >= 0) ? 0 : ret;
}

/* 0x812213D6 */
int npManagerEnqueueGetTicketAvatarUrlJob(IPMI::Session *session, SceUInt32 ipcReqId,
					  int clientReqId, int authReqId, SceSize ticketSize,
					  int avatarId, int sizeType)
{
	NpGetTicketAvatarUrlJob *job = new NpGetTicketAvatarUrlJob(
		session, ipcReqId, clientReqId, authReqId, ticketSize, avatarId, sizeType);
	if (job == NULL) {
		char emptyUrl[128];
		sceClibMemset(emptyUrl, 0, sizeof(emptyUrl));
		npManagerSendGetTicketAvatarUrlResponse(session, ipcReqId, NP_MGR_ERR_NO_MEMORY,
							emptyUrl);
		return 0;
	}
	int coreHandle = npManagerResolveRequestHandle(clientReqId, session->getProcessId());
	int ret = npJobQueueEnqueue(npManagerGetJobQueue(), coreHandle, job);
	return (ret >= 0) ? 0 : ret;
}
