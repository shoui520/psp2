/*
 * SceInvitationDlgImplPlugin (vs0:sys/external/libcdlg_invitation_impl.suprx):
 * WebAPI core client (SceSiCoreImpl) for session invitations, sessions,
 * and invitation data/icon retrieval.
 */
#ifndef INVITATION_IMPL_SI_CORE_H
#define INVITATION_IMPL_SI_CORE_H

#include <scetypes.h>
#include <std/string.h>
#include <std/vector>
#include <thread/thread.h>

struct SessionInvitationSummary {
	paf::string invitationId;
	paf::string sessionId;
	paf::string fromOnlineId;
	paf::string sessionName;
	paf::string sessionStatus;
	paf::string message;
	paf::string npTitleId;
	paf::string npTitleName;
	paf::string npCommunicationId;
	SceUInt32 maxUsers;
	bool isPrivate;
	bool seenFlag;
	bool usedFlag;
};

struct SessionCreateRequest {
	paf::string sessionType;
	paf::string sessionPrivacy;
	paf::string sessionName;
	paf::string sessionStatus;
	SceUInt32 sessionMaxUser;
	paf::vector<paf::string> recipients;
	paf::string invitationMessage;
};

class SceSiCoreImpl {
public:
	SceSiCoreImpl();
	~SceSiCoreImpl();

	SceInt32 Initialize(SceInt32 libHttpCtxId, const char *npLanguage);
	void Terminate();

	SceInt32 FetchInvitationList(const char *onlineId, SceInt32 limit, SceInt32 offset,
				     paf::vector<SessionInvitationSummary> &outList);
	SceInt32 FetchSessionPart(const char *sessionId, const char *partName,
				  paf::vector<SceUInt8> &outBytes);
	SceInt32 FetchInvitationPart(const char *onlineId, const char *invitationId,
				     const char *partName, paf::vector<SceUInt8> &outBytes);
	SceInt32 CreateSession(const SessionCreateRequest &request, paf::string &outSessionId);
	SceInt32 UpdateInvitationFlag(const char *onlineId, const char *invitationId,
				      const char *flagName);

	static paf::string BuildTmdbUrl(const char *envPrefix, const char *titleId,
					const char *hashPart);

private:
	SceInt32 m_webApiCtxId;
	SceInt32 m_userCtxId;
	paf::string m_npLanguage;
	paf::thread::RMutex m_sendSizeLock;
	bool m_initialized;
};

#endif /* INVITATION_IMPL_SI_CORE_H */
