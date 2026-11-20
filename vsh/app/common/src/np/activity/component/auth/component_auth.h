/*
 * The authentication component: the signed-in user's ticket, mini ticket and NP ID, and the
 * chat restriction, each read through a SceNpManager request (Sony's file and method names:
 * component_auth.cpp's checks; the class name is ours).
 */
#ifndef NP_ACTIVITY_COMPONENT_AUTH_COMPONENT_AUTH_H
#define NP_ACTIVITY_COMPONENT_AUTH_COMPONENT_AUTH_H

#include <scetypes.h>
#include <np/np_common.h>
#include "../abstract_component.h"

/* the restriction bit of SceNpManager_2FE3625C's restrictions: chat is restricted */
#define AUTH_RESTRICTION_CHAT_BYTE	14
#define AUTH_RESTRICTION_CHAT		0x80

#define AUTH_ACCOUNT_DATA_SIZE		464
#define AUTH_RESTRICTION_SIZE		16

class ComponentAuth : public Component {
public:
	ComponentAuth();
	virtual ~ComponentAuth();

	/* aborts the SceNpManager request running */
	virtual void SetAbort();

	/* the ticket, base64-encoded into a buffer the caller frees with activityFree */
	virtual int GetFullTicketBase64Str(char **ticket, SceSize *size);
	virtual int GetMiniTicketBase64Str(char **ticket, SceSize *size);
	virtual int GetMyNpId(SceNpId *npId);
	/* NP_ACTIVITY_ERROR_CHAT_RESTRICTED when the user may not chat */
	virtual int CheckChatRestriction();

private:
	void SetRequestId(int requestId);

	int m_requestId;	/* the SceNpManager request running, 0 for none */
};

#endif
