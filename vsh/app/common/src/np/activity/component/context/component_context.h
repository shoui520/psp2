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
class AbstractCommand;

/* ComponentContext::m_state */
#define CONTEXT_STATE_IDLE		0	/* made, connection not open */
#define CONTEXT_STATE_OPENING		1
#define CONTEXT_STATE_CLOSING		2
#define CONTEXT_STATE_OPEN		3	/* connection open, no command running */
#define CONTEXT_STATE_RUNNING		4	/* a command is running */

/* AllocStaticResourceBuffer's buffer: the HTTP component's */
#define CONTEXT_STATIC_RESOURCE_BUFFER_SIZE	0xC00

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
	virtual int CheckStatusUpdate(const SceNpId *npId, const SceUInt64 *since,
		const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
		NpActivityUpdates *updates);
	virtual int GetFriendStatusTimeline(const SceNpId *npId, const NpActivityRange *range,
		NpActivityStatus *statuses, int max, int *count);
	virtual int GetFriendStatusTimelineByTitle(const SceNpId *npId,
		const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
		const NpActivityRange *range, NpActivityStatusList *lists);
	virtual int GetStatusTimelineByAccount(const SceNpId *npId, const NpActivityOwner *owners,
		SceSize count, int *max, NpActivityStatusList *lists);
	virtual int GetStatusTimelineByAccountAndTitle(const SceNpId *npId, const NpActivityOwner *owners,
		SceSize count, const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], int *max,
		NpActivityStatusList *lists);
	virtual int GetStatusInfo(const SceNpId *npId, const NpActivityStatusKey *keys, SceSize count,
		const int *locale, void *work, NpActivityDetailState *state,
		NpActivityStatusInfo *infos);
	virtual int GetStatusInfoWithCallback(const SceNpId *npId, const NpActivityStatusKey *keys,
		SceSize count, const int *locale, void *work, NpActivityDetailState *state,
		NpActivityStatusDetailCallback callback, void *arg);
	virtual int GetStatusDetail(const SceNpId *npId, const NpActivityStatusKey *key,
		const NpActivityDetailParam *param, NpActivityStatusInfo *info,
		NpActivityComment *comments, int maxComments, int *numComments, int *isVoted,
		SceNpId *voters, int maxVoters, SceUInt32 *numVoters);
	virtual int AddStatusList(const SceNpId *npId, const NpActivityStatusInfo *infos, SceSize count,
		NpActivityPostResult *results);
	virtual int RemoveStatus(const SceNpId *npId, int statusId);
	virtual int AddComment(const SceNpId *npId, const NpActivityStatusKey *key,
		const NpActivityCommentPost *comment, NpActivityCommentRequest *request);
	virtual int RemoveComment(const SceNpId *npId, const NpActivityCommentKey *key,
		NpActivityRequest *request);
	virtual int AddVoteGoodJob(const SceNpId *npId, const NpActivityStatusKey *key,
		NpActivityRequest *request);
	virtual int RemoveVoteGoodJob(const SceNpId *npId, const NpActivityStatusKey *key,
		NpActivityRequest *request);

private:
	void setState(int state);
	void destroyComponents();
	void clearAbortAll();
	int endCommandLocked(int result);
	int endCommand(int result);
	int getNpId(const SceNpId *npId, SceNpId *self, const SceNpId **id);
	int runCommand(AbstractCommand *command);
	int getStaticResource();

public:
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
