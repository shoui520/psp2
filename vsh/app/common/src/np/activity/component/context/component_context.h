/*
 * A context: one user of the library, the components it runs its commands with, and the
 * commands (Sony's names: component_context.cpp's checks).
 */
#ifndef NP_ACTIVITY_COMPONENT_CONTEXT_COMPONENT_CONTEXT_H
#define NP_ACTIVITY_COMPONENT_CONTEXT_COMPONENT_CONTEXT_H

#include <scetypes.h>
#include <np/np_common.h>
#include "../abstract_component.h"
#include "../../np_activity.h"

class AbstractContextManager;
class ComponentCommandExecutor;
class ComponentAuth;
class ComponentHttp;
class ComponentXmlParser;
class ComponentXmlGenerator;
class ComponentUtility;

/* ComponentContext::m_state */
#define CONTEXT_STATE_IDLE		0	/* made, connection not open */
#define CONTEXT_STATE_OPENING		1
#define CONTEXT_STATE_CLOSING		2
#define CONTEXT_STATE_OPEN		3	/* connection open, no command running */
#define CONTEXT_STATE_RUNNING		4	/* a command is running */

class ComponentContext : public Component {
public:
	ComponentContext();
	virtual ~ComponentContext();

	virtual int Initialize();
	virtual int Finalize();
	virtual void SetManager(AbstractContextManager *manager)
	{
		m_manager = manager;
	}
	virtual int AllocStaticResourceBuffer();
	virtual void FreeStaticResourceBuffer();
	virtual int ExecuteAbort();

	virtual int OpenConnection(int mode);
	virtual int CloseConnection();
	virtual int GetAccountInformation(const SceNpId *npId, void *info);
	virtual int SetAccountInformation(const SceNpId *npId, const void *info);
	virtual int CheckStatusUpdate(int flags, const SceUInt64 *since,
		const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
		NpActivityUpdates *updates);
	virtual int GetFriendStatusTimeline(int flags, const NpActivityRange *range,
		NpActivityStatus *statuses, int max, int *count);
	virtual int GetFriendStatusTimelineByTitle(int flags,
		const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
		const NpActivityRange *range, NpActivityStatusList *lists);
	virtual int GetStatusTimelineByAccount(int flags, const NpActivityOwner *owners,
		SceSize count, int *max, NpActivityStatusList *lists);
	virtual int GetStatusTimelineByAccountAndTitle(int flags, const NpActivityOwner *owners,
		SceSize count, const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], int *max,
		NpActivityStatusList *lists);
	virtual int GetStatusInfo(int flags, const NpActivityStatusKey *keys, SceSize count,
		const int *locale, void *work, NpActivityDetailState *state,
		NpActivityStatusInfo *infos);
	virtual int GetStatusInfoWithCallback(int flags, const NpActivityStatusKey *keys,
		SceSize count, const int *locale, void *work, NpActivityDetailState *state,
		NpActivityStatusDetailCallback callback, void *arg);
	virtual int GetStatusDetail(int flags, const NpActivityStatusKey *key,
		const NpActivityDetailParam *param, NpActivityStatusInfo *info,
		NpActivityComment *comments, int maxComments, int *numComments, int *isVoted,
		SceNpId *voters, int maxVoters, SceUInt32 *numVoters);
	virtual int AddStatusList(int flags, const NpActivityStatusInfo *infos, SceSize count,
		NpActivityPostResult *results);
	virtual int RemoveStatus(int flags, int statusId);
	virtual int AddComment(int flags, const NpActivityStatusKey *key,
		const NpActivityCommentPost *comment, NpActivityCommentRequest *request);
	virtual int RemoveComment(int flags, const NpActivityCommentKey *key,
		NpActivityRequest *request);
	virtual int AddVoteGoodJob(int flags, const NpActivityStatusKey *key,
		NpActivityRequest *request);
	virtual int RemoveVoteGoodJob(int flags, const NpActivityStatusKey *key,
		NpActivityRequest *request);

	ComponentContext *m_next;		/* the library's contexts */
	ComponentContext *m_prev;
	int m_state;				/* CONTEXT_STATE_* */
	void *m_staticResource;			/* AllocStaticResourceBuffer's buffer */
	SceSize m_staticResourceSize;
	ActivityMutex *m_componentMutex;	/* the components' mutex */
	AbstractContextManager *m_manager;
	ComponentCommandExecutor *m_executor;
	ComponentAuth *m_auth;
	ComponentHttp *m_http;
	ComponentXmlParser *m_xmlParser;
	ComponentXmlGenerator *m_xmlGenerator;
	ComponentUtility *m_utility;
};

#endif
