/*
 * nptrophy_job.cpp: all 31 SceNpTrophyServer asynchronous worker jobs and
 * enqueue entrypoints (0x81227FB8..0x8122998C, 0x814277E8..0x814288B8).
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <ipmi/session.h>
#include "nptrophy_internal.h"

#define NP_TROPHY_MAX_STREAM_READ_SIZE	2048U

/* 1. CheckNetSyncJob (72 bytes; 0x81427820 / 0x8142784C) */
class CheckNetSyncJob : public SceNpTrophyJobBase {
public:
	CheckNetSyncJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
			int ctxId, SceUInt64 options)
		: SceNpTrophyJobBase("CheckNetSyncJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_options(options),
		  m_outNeedSync(0)
	{
	}

	virtual int Run()
	{
		return npTrophyCoreCheckNetSync(m_session->getProcessId(), m_session, m_handleId,
						m_ctxId, m_options, &m_outNeedSync);
	}

	virtual void Finish(int result)
	{
		npTrophySendCheckNetSyncResponse(m_session, m_ipcReqId, result, &m_outNeedSync);
	}

private:
	int m_ctxId;
	SceUInt64 m_options;
	int m_outNeedSync;
};

/* 2. NetSyncTitlesJob (64 bytes; 0x81427880 / 0x814278BC) */
class NetSyncTitlesJob : public SceNpTrophyJobBase {
public:
	NetSyncTitlesJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
			 int ctxId, SceUInt64 options)
		: SceNpTrophyJobBase("NetSyncTitlesJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_options(options)
	{
	}

	virtual int Run()
	{
		unsigned int netToken = SceShell_6B8772A2(m_session->getProcessId());
		int ret = npTrophyCoreNetSyncTitles(m_session->getProcessId(), m_session,
						    m_handleId, m_ctxId, m_options);
		SceShell_794BF5BE(netToken);
		return ret;
	}

	virtual void Finish(int result)
	{
		npTrophySendNetSyncTitlesResponse(m_session, m_ipcReqId, result);
	}

private:
	int m_ctxId;
	SceUInt64 m_options;
};

/* 3. CheckRecoveryRequiredJob (56 bytes; 0x81427924 / 0x8142794C) */
class CheckRecoveryRequiredJob : public SceNpTrophyJobBase {
public:
	CheckRecoveryRequiredJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
				 int ctxId)
		: SceNpTrophyJobBase("CheckRecoveryRequiredJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_outRequired(0)
	{
	}

	virtual int Run()
	{
		return npTrophyCoreCheckRecovery(m_session->getProcessId(), m_session, m_handleId,
						 m_ctxId, &m_outRequired);
	}

	virtual void Finish(int result)
	{
		npTrophySendCheckRecoveryResponse(m_session, m_ipcReqId, result, &m_outRequired);
	}

private:
	int m_ctxId;
	int m_outRequired;
};

/* 4. PerformRecoveryJob (52 bytes; 0x81427980 / 0x81427998) */
class PerformRecoveryJob : public SceNpTrophyJobBase {
public:
	PerformRecoveryJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId, int ctxId)
		: SceNpTrophyJobBase("PerformRecoveryJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId)
	{
	}

	virtual int Run()
	{
		return npTrophyCorePerformRecovery(m_session->getProcessId(), m_session,
						   m_handleId, m_ctxId);
	}

	virtual void Finish(int result)
	{
		npTrophySendPerformRecoveryResponse(m_session, m_ipcReqId, result);
	}

private:
	int m_ctxId;
};

/* 5. GetTitleForRemovalJob (1352 bytes; 0x81427A1C / 0x81427A4C) */
class GetTitleForRemovalJob : public SceNpTrophyJobBase {
public:
	GetTitleForRemovalJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId, int ctxId)
		: SceNpTrophyJobBase("GetTitleForRemovalJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId)
	{
		sceClibMemset(m_titleInfo, 0, sizeof(m_titleInfo));
		sceClibMemset(m_titleExtra, 0, sizeof(m_titleExtra));
		sceClibMemset(&m_gameDetails, 0, sizeof(m_gameDetails));
		m_gameDetails.size = sizeof(m_gameDetails);
	}

	virtual int Run()
	{
		return npTrophyCoreGetTitleForRemoval(m_session->getProcessId(), m_session,
						      m_handleId, m_ctxId, m_titleInfo,
						      m_titleExtra, &m_gameDetails);
	}

	virtual void Finish(int result)
	{
		npTrophySendGetTitleForRemovalResponse(m_session, m_ipcReqId, result,
						       m_titleInfo, m_titleExtra, &m_gameDetails);
	}

private:
	int m_ctxId;
	int m_alignPad;
	SceUInt8 m_titleInfo[104];
	SceUInt8 m_titleExtra[8];
	NpTrophyGameDetails m_gameDetails;
};

/* 6. RemoveTitleJob (56 bytes; 0x81427A90 / 0x81427AB0) */
class RemoveTitleJob : public SceNpTrophyJobBase {
public:
	RemoveTitleJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
		       int ctxId, int titleHandle)
		: SceNpTrophyJobBase("RemoveTitleJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_titleHandle(titleHandle)
	{
	}

	virtual int Run()
	{
		return npTrophyCoreRemoveTitle(m_session->getProcessId(), m_session, m_handleId,
					       m_ctxId, m_titleHandle);
	}

	virtual void Finish(int result)
	{
		npTrophySendRemoveTitleResponse(m_session, m_ipcReqId, result);
	}

private:
	int m_ctxId;
	int m_titleHandle;
};

/* 7. RemoveAllTitlesJob (52 bytes; 0x81427AE0 / 0x81427AF8) */
class RemoveAllTitlesJob : public SceNpTrophyJobBase {
public:
	RemoveAllTitlesJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId, int ctxId)
		: SceNpTrophyJobBase("RemoveAllTitlesJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId)
	{
	}

	virtual int Run()
	{
		return npTrophyCoreRemoveAllTitles(m_session->getProcessId(), m_session,
						   m_handleId, m_ctxId);
	}

	virtual void Finish(int result)
	{
		npTrophySendRemoveAllTitlesResponse(m_session, m_ipcReqId, result);
	}

private:
	int m_ctxId;
};

/* 8. GetUserFileStatusJob (72 bytes; 0x81427B60 / 0x81427B88) */
class GetUserFileStatusJob : public SceNpTrophyJobBase {
public:
	GetUserFileStatusJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId, int ctxId)
		: SceNpTrophyJobBase("GetUserFileStatusJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId)
	{
		sceClibMemset(m_statusBuf, 0, sizeof(m_statusBuf));
	}

	virtual int Run()
	{
		return npTrophyCoreGetUserFileStatus(m_session->getProcessId(), m_session,
						     m_handleId, m_ctxId, m_statusBuf);
	}

	virtual void Finish(int result)
	{
		npTrophySendGetUserFileStatusResponse(m_session, m_ipcReqId, result, m_statusBuf);
	}

private:
	int m_ctxId;
	int m_alignPad;
	SceUInt8 m_statusBuf[16];
};

/* 9. GetUserTitleInfoJob (88 bytes; 0x81427BBC / 0x81427BE4) */
class GetUserTitleInfoJob : public SceNpTrophyJobBase {
public:
	GetUserTitleInfoJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId, int ctxId)
		: SceNpTrophyJobBase("GetUserTitleInfoJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId)
	{
		sceClibMemset(m_titleInfoBuf, 0, sizeof(m_titleInfoBuf));
	}

	virtual int Run()
	{
		return npTrophyCoreGetUserTitleInfo(m_session->getProcessId(), m_session,
						    m_handleId, m_ctxId, m_titleInfoBuf);
	}

	virtual void Finish(int result)
	{
		npTrophySendGetUserTitleInfoResponse(m_session, m_ipcReqId, result, m_titleInfoBuf);
	}

private:
	int m_ctxId;
	int m_alignPad;
	SceUInt8 m_titleInfoBuf[32];
};

/* 10. ReadStreamJob (64 bytes; 0x81427C18..0x81427CBC) */
class ReadStreamJob : public SceNpTrophyJobBase {
public:
	ReadStreamJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
		      int streamId, SceSize maxReadSize)
		: SceNpTrophyJobBase("ReadStreamJob", session, ipcReqId, handleId),
		  m_streamId(streamId),
		  m_maxReadSize(maxReadSize),
		  m_readBuf(NULL),
		  m_actualReadSize(0)
	{
	}

	virtual ~ReadStreamJob()
	{
		if (m_readBuf != NULL) {
			npTrophyHeapFree(m_readBuf);
			m_readBuf = NULL;
		}
	}

	virtual int Run()
	{
		m_readBuf = npTrophyHeapAlloc(m_maxReadSize);
		if (m_readBuf == NULL)
			return NP_TROPHY_ERR_NO_MEMORY;

		int ret = npTrophyCoreReadStream(m_session->getProcessId(), m_session, m_handleId,
						 m_streamId, m_readBuf, m_maxReadSize,
						 &m_actualReadSize);
		if (ret >= 0)
			return 0;

		if (m_readBuf != NULL) {
			npTrophyHeapFree(m_readBuf);
			m_readBuf = NULL;
		}
		return ret;
	}

	virtual void Finish(int result)
	{
		npTrophySendReadStreamResponse(m_session, m_ipcReqId, result,
					       &m_actualReadSize, m_readBuf);
		if (m_readBuf != NULL) {
			npTrophyHeapFree(m_readBuf);
			m_readBuf = NULL;
		}
	}

private:
	int m_streamId;
	SceSize m_maxReadSize;
	void *m_readBuf;
	SceSize m_actualReadSize;
};

/* 11. OpenTrophyConfStreamJob (88 bytes; 0x81427D00 / 0x81427D38) */
class OpenTrophyConfStreamJob : public SceNpTrophyJobBase {
public:
	OpenTrophyConfStreamJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
				int ctxId, int mode, SceUInt64 options, int wantSize)
		: SceNpTrophyJobBase("OpenTrophyConfStreamJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_mode(mode),
		  m_options(options),
		  m_streamId(0),
		  m_streamSize(0),
		  m_sizeOutPtr(wantSize != 0 ? &m_streamSize : NULL)
	{
	}

	virtual int Run()
	{
		return npTrophyCoreOpenConfStream(m_session->getProcessId(), m_session, m_handleId,
						  m_ctxId, m_mode, m_options, &m_streamId,
						  m_sizeOutPtr);
	}

	virtual void Finish(int result)
	{
		npTrophySendOpenConfStreamResponse(m_session, m_ipcReqId, result,
						   &m_streamId, &m_streamSize);
	}

private:
	int m_ctxId;
	int m_mode;
	SceUInt64 m_options;
	int m_streamId;
	SceUInt64 m_streamSize;
	SceUInt64 *m_sizeOutPtr;
};

/* 12. GetTrophyTitleDataJob (304 bytes; 0x81427DCC / 0x81427DFC) */
class GetTrophyTitleDataJob : public SceNpTrophyJobBase {
public:
	GetTrophyTitleDataJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
			      int ctxId, int titleHandle)
		: SceNpTrophyJobBase("GetTrophyTitleDataJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_titleHandle(titleHandle)
	{
		sceClibMemset(m_titleSummary, 0, sizeof(m_titleSummary));
		sceClibMemset(m_titleStats, 0, sizeof(m_titleStats));
	}

	virtual int Run()
	{
		return npTrophyCoreGetTrophyTitleData(m_session->getProcessId(), m_session,
						      m_handleId, m_ctxId, m_titleHandle,
						      m_titleSummary, m_titleStats);
	}

	virtual void Finish(int result)
	{
		npTrophySendGetTrophyTitleDataResponse(m_session, m_ipcReqId, result,
						       m_titleSummary, m_titleStats);
	}

private:
	int m_ctxId;
	int m_titleHandle;
	SceUInt8 m_titleSummary[108];
	int m_alignPad;
	SceUInt8 m_titleStats[136];
};

/* 13. GetTrophyGroupDataJob (184 bytes; 0x81427E3C / 0x81427E6C) */
class GetTrophyGroupDataJob : public SceNpTrophyJobBase {
public:
	GetTrophyGroupDataJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
			      int ctxId, int titleHandle, int groupId)
		: SceNpTrophyJobBase("GetTrophyGroupDataJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_titleHandle(titleHandle),
		  m_groupId(groupId)
	{
		sceClibMemset(m_groupSummary, 0, sizeof(m_groupSummary));
		sceClibMemset(m_groupStats, 0, sizeof(m_groupStats));
	}

	virtual int Run()
	{
		return npTrophyCoreGetTrophyGroupData(m_session->getProcessId(), m_session,
						      m_handleId, m_ctxId, m_titleHandle,
						      m_groupId, m_groupSummary, m_groupStats);
	}

	virtual void Finish(int result)
	{
		npTrophySendGetTrophyGroupDataResponse(m_session, m_ipcReqId, result,
						       m_groupSummary, m_groupStats);
	}

private:
	int m_ctxId;
	int m_titleHandle;
	int m_groupId;
	SceUInt8 m_groupSummary[68];
	SceUInt8 m_groupStats[56];
};

/* 14. GetTrophyDataJob (136 bytes; 0x81427EAC / 0x81427EDC) */
class GetTrophyDataJob : public SceNpTrophyJobBase {
public:
	GetTrophyDataJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
			 int ctxId, int titleHandle, int trophyId)
		: SceNpTrophyJobBase("GetTrophyDataJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_titleHandle(titleHandle),
		  m_trophyId(trophyId)
	{
		sceClibMemset(m_trophySummary, 0, sizeof(m_trophySummary));
		sceClibMemset(m_trophyStats, 0, sizeof(m_trophyStats));
	}

	virtual int Run()
	{
		return npTrophyCoreGetTrophyData(m_session->getProcessId(), m_session, m_handleId,
						 m_ctxId, m_titleHandle, m_trophyId,
						 m_trophySummary, m_trophyStats);
	}

	virtual void Finish(int result)
	{
		npTrophySendGetTrophyDataResponse(m_session, m_ipcReqId, result,
						  m_trophySummary, m_trophyStats);
	}

private:
	int m_ctxId;
	int m_titleHandle;
	int m_trophyId;
	SceUInt8 m_trophySummary[36];
	SceUInt8 m_trophyStats[40];
};

/* 15. GetNextTitleEntryStatusJob (168 bytes; 0x81427F38 / 0x81427F68) */
class GetNextTitleEntryStatusJob : public SceNpTrophyJobBase {
public:
	GetNextTitleEntryStatusJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
				   int ctxId, int cursor)
		: SceNpTrophyJobBase("GetNextTitleEntryStatusJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_cursor(cursor),
		  m_outNextCursor(-1)
	{
		sceClibMemset(m_titleEntry, 0, sizeof(m_titleEntry));
	}

	virtual int Run()
	{
		return npTrophyCoreGetNextTitleEntry(m_session->getProcessId(), m_session,
						     m_handleId, m_ctxId, m_cursor,
						     &m_outNextCursor, m_titleEntry);
	}

	virtual void Finish(int result)
	{
		npTrophySendGetNextTitleStatusResponse(m_session, m_ipcReqId, result,
						       &m_outNextCursor, m_titleEntry);
	}

private:
	int m_ctxId;
	int m_cursor;
	int m_outNextCursor;
	int m_alignPad;
	SceUInt8 m_titleEntry[104];
};

/* 16. OpenTitleIconStreamJob (88 bytes; 0x81427FFC / 0x81428034) */
class OpenTitleIconStreamJob : public SceNpTrophyJobBase {
public:
	OpenTitleIconStreamJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
			       int ctxId, int mode, SceUInt64 options, int wantSize)
		: SceNpTrophyJobBase("OpenTitleIconStreamJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_mode(mode),
		  m_options(options),
		  m_streamId(0),
		  m_streamSize(0),
		  m_sizeOutPtr(wantSize != 0 ? &m_streamSize : NULL)
	{
	}

	virtual int Run()
	{
		return npTrophyCoreOpenTitleIconStream(m_session->getProcessId(), m_session,
						       m_handleId, m_ctxId, m_mode, m_options,
						       &m_streamId, m_sizeOutPtr);
	}

	virtual void Finish(int result)
	{
		npTrophySendOpenTitleIconStreamResponse(m_session, m_ipcReqId, result,
							&m_streamId, &m_streamSize);
	}

private:
	int m_ctxId;
	int m_mode;
	SceUInt64 m_options;
	int m_streamId;
	SceUInt64 m_streamSize;
	SceUInt64 *m_sizeOutPtr;
};

/* 17. OpenGroupIconStreamJob (96 bytes; 0x81428074 / 0x814280B0) */
class OpenGroupIconStreamJob : public SceNpTrophyJobBase {
public:
	OpenGroupIconStreamJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
			       int ctxId, int mode, int groupId, SceUInt64 options, int wantSize)
		: SceNpTrophyJobBase("OpenGroupIconStreamJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_mode(mode),
		  m_groupId(groupId),
		  m_options(options),
		  m_streamId(0),
		  m_streamSize(0),
		  m_sizeOutPtr(wantSize != 0 ? &m_streamSize : NULL)
	{
	}

	virtual int Run()
	{
		return npTrophyCoreOpenGroupIconStream(m_session->getProcessId(), m_session,
						       m_handleId, m_ctxId, m_mode, m_groupId,
						       m_options, &m_streamId, m_sizeOutPtr);
	}

	virtual void Finish(int result)
	{
		npTrophySendOpenGroupIconStreamResponse(m_session, m_ipcReqId, result,
							&m_streamId, &m_streamSize);
	}

private:
	int m_ctxId;
	int m_mode;
	int m_groupId;
	SceUInt64 m_options;
	int m_streamId;
	SceUInt64 m_streamSize;
	SceUInt64 *m_sizeOutPtr;
};

/* 18. OpenTrophyIconStreamJob (96 bytes; 0x814280F0 / 0x8142812C) */
class OpenTrophyIconStreamJob : public SceNpTrophyJobBase {
public:
	OpenTrophyIconStreamJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
				int ctxId, int mode, int trophyId, SceUInt64 options, int wantSize)
		: SceNpTrophyJobBase("OpenTrophyIconStreamJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_mode(mode),
		  m_trophyId(trophyId),
		  m_options(options),
		  m_streamId(0),
		  m_streamSize(0),
		  m_sizeOutPtr(wantSize != 0 ? &m_streamSize : NULL)
	{
	}

	virtual int Run()
	{
		return npTrophyCoreOpenTrophyIconStream(m_session->getProcessId(), m_session,
							m_handleId, m_ctxId, m_mode, m_trophyId,
							m_options, &m_streamId, m_sizeOutPtr);
	}

	virtual void Finish(int result)
	{
		npTrophySendOpenTrophyIconStreamResponse(m_session, m_ipcReqId, result,
							 &m_streamId, &m_streamSize);
	}

private:
	int m_ctxId;
	int m_mode;
	int m_trophyId;
	SceUInt64 m_options;
	int m_streamId;
	SceUInt64 m_streamSize;
	SceUInt64 *m_sizeOutPtr;
};

/* 19. GetGameInfoJob (1272 bytes; 0x814281C0 / 0x8142820C) */
class GetGameInfoJob : public SceNpTrophyJobBase {
public:
	GetGameInfoJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
		       int ctxId, int mode, SceUInt64 options)
		: SceNpTrophyJobBase("GetGameInfoJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_mode(mode),
		  m_options(options)
	{
		sceClibMemset(&m_gameDetails, 0, sizeof(m_gameDetails));
		sceClibMemset(&m_gameData, 0, sizeof(m_gameData));
	}

	virtual int Run()
	{
		m_gameDetails.size = sizeof(m_gameDetails);
		m_gameData.size = sizeof(m_gameData);
		return npTrophyCoreGetGameInfo(m_session->getProcessId(), m_session, m_handleId,
					       m_ctxId, m_mode, m_options, &m_gameDetails,
					       &m_gameData);
	}

	virtual void Finish(int result)
	{
		npTrophySendGetGameInfoResponse(m_session, m_ipcReqId, result,
						&m_gameDetails, &m_gameData);
	}

private:
	int m_ctxId;
	int m_mode;
	SceUInt64 m_options;
	NpTrophyGameDetails m_gameDetails;
	NpTrophyGameData m_gameData;
};

/* 20. GetGroupInfoJob (1288 bytes; 0x8142824C / 0x8142829C) */
class GetGroupInfoJob : public SceNpTrophyJobBase {
public:
	GetGroupInfoJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
			int ctxId, int mode, int groupId, SceUInt64 options)
		: SceNpTrophyJobBase("GetGroupInfoJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_mode(mode),
		  m_groupId(groupId),
		  m_options(options)
	{
		sceClibMemset(&m_groupDetails, 0, sizeof(m_groupDetails));
		sceClibMemset(&m_groupData, 0, sizeof(m_groupData));
	}

	virtual int Run()
	{
		m_groupDetails.size = sizeof(m_groupDetails);
		m_groupData.size = sizeof(m_groupData);
		return npTrophyCoreGetGroupInfo(m_session->getProcessId(), m_session, m_handleId,
						m_ctxId, m_mode, m_groupId, m_options,
						&m_groupDetails, &m_groupData);
	}

	virtual void Finish(int result)
	{
		npTrophySendGetGroupInfoResponse(m_session, m_ipcReqId, result,
						 &m_groupDetails, &m_groupData);
	}

private:
	int m_ctxId;
	int m_mode;
	int m_groupId;
	SceUInt64 m_options;
	NpTrophyGroupDetails m_groupDetails;
	NpTrophyGroupData m_groupData;
};

/* 21. GetTrophyInfoJob (1272 bytes; 0x814282DC / 0x81428328) */
class GetTrophyInfoJob : public SceNpTrophyJobBase {
public:
	GetTrophyInfoJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
			 int ctxId, int mode, int trophyId, SceUInt64 options)
		: SceNpTrophyJobBase("GetTrophyInfoJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_mode(mode),
		  m_trophyId(trophyId),
		  m_options(options)
	{
		sceClibMemset(&m_trophyDetails, 0, sizeof(m_trophyDetails));
		sceClibMemset(&m_trophyData, 0, sizeof(m_trophyData));
	}

	virtual int Run()
	{
		m_trophyDetails.size = sizeof(m_trophyDetails);
		m_trophyData.size = sizeof(m_trophyData);
		return npTrophyCoreGetTrophyInfo(m_session->getProcessId(), m_session, m_handleId,
						 m_ctxId, m_mode, m_trophyId, m_options,
						 &m_trophyDetails, &m_trophyData);
	}

	virtual void Finish(int result)
	{
		npTrophySendGetTrophyInfoResponse(m_session, m_ipcReqId, result,
						  &m_trophyDetails, &m_trophyData);
	}

private:
	int m_ctxId;
	int m_mode;
	int m_trophyId;
	SceUInt64 m_options;
	NpTrophyDetails m_trophyDetails;
	NpTrophyData m_trophyData;
};

/* 22. GetTitleStatusForGameJob (60 bytes; 0x814283D8 / 0x81428400) */
class GetTitleStatusForGameJob : public SceNpTrophyJobBase {
public:
	GetTitleStatusForGameJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
				 int titleSession, int ctxId)
		: SceNpTrophyJobBase("GetTitleStatusForGameJob", session, ipcReqId, handleId),
		  m_titleSession(titleSession),
		  m_ctxId(ctxId),
		  m_outStatus(0)
	{
	}

	virtual int Run()
	{
		return npTrophyCoreGetTitleStatusForGame(m_session->getProcessId(), m_session,
							 m_handleId, m_titleSession, m_ctxId,
							 &m_outStatus);
	}

	virtual void Finish(int result)
	{
		npTrophySendGetTitleStatusForGameResponse(m_session, m_ipcReqId, result,
							  &m_outStatus);
	}

private:
	int m_titleSession;
	int m_ctxId;
	int m_outStatus;
};

/* 23. InstallTitleJob (60 bytes; 0x81428434 / 0x8142845C) */
class InstallTitleJob : public SceNpTrophyJobBase {
public:
	InstallTitleJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
			int titleSession, int ctxId)
		: SceNpTrophyJobBase("InstallTitleJob", session, ipcReqId, handleId),
		  m_titleSession(titleSession),
		  m_ctxId(ctxId),
		  m_outResult(-1)
	{
	}

	virtual int Run()
	{
		return npTrophyCoreInstallTitle(m_session->getProcessId(), m_session, m_handleId,
						m_titleSession, m_ctxId, &m_outResult);
	}

	virtual void Finish(int result)
	{
		npTrophySendInstallTitleResponse(m_session, m_ipcReqId, result, &m_outResult);
	}

private:
	int m_titleSession;
	int m_ctxId;
	int m_outResult;
};

/* 24. UpdateTitleJob (60 bytes; 0x81428490 / 0x814284B8) */
class UpdateTitleJob : public SceNpTrophyJobBase {
public:
	UpdateTitleJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
		       int titleSession, int ctxId)
		: SceNpTrophyJobBase("UpdateTitleJob", session, ipcReqId, handleId),
		  m_titleSession(titleSession),
		  m_ctxId(ctxId),
		  m_outResult(-1)
	{
	}

	virtual int Run()
	{
		return npTrophyCoreUpdateTitle(m_session->getProcessId(), m_session, m_handleId,
					       m_titleSession, m_ctxId, &m_outResult);
	}

	virtual void Finish(int result)
	{
		npTrophySendUpdateTitleResponse(m_session, m_ipcReqId, result, &m_outResult);
	}

private:
	int m_titleSession;
	int m_ctxId;
	int m_outResult;
};

/* 25. SetupTitleJob (60 bytes; 0x814284EC / 0x81428514) */
class SetupTitleJob : public SceNpTrophyJobBase {
public:
	SetupTitleJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
		      int titleSession, int ctxId)
		: SceNpTrophyJobBase("SetupTitleJob", session, ipcReqId, handleId),
		  m_titleSession(titleSession),
		  m_ctxId(ctxId),
		  m_outResult(-1)
	{
	}

	virtual int Run()
	{
		return npTrophyCoreSetupTitle(m_session->getProcessId(), m_session, m_handleId,
					      m_titleSession, m_ctxId, &m_outResult);
	}

	virtual void Finish(int result)
	{
		npTrophySendSetupTitleResponse(m_session, m_ipcReqId, result, &m_outResult);
	}

private:
	int m_titleSession;
	int m_ctxId;
	int m_outResult;
};

/* 26. GetTitleFileStatusJob (160 bytes; 0x81428580 / 0x814285A8) */
class GetTitleFileStatusJob : public SceNpTrophyJobBase {
public:
	GetTitleFileStatusJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
			      int titleSession, int ctxId)
		: SceNpTrophyJobBase("GetTitleFileStatusJob", session, ipcReqId, handleId),
		  m_titleSession(titleSession),
		  m_ctxId(ctxId)
	{
		sceClibMemset(m_statusBuf, 0, sizeof(m_statusBuf));
	}

	virtual int Run()
	{
		return npTrophyCoreGetTitleFileStatus(m_session->getProcessId(), m_session,
						      m_handleId, m_titleSession, m_ctxId,
						      m_statusBuf);
	}

	virtual void Finish(int result)
	{
		npTrophySendGetTitleFileStatusResponse(m_session, m_ipcReqId, result, m_statusBuf);
	}

private:
	int m_titleSession;
	int m_ctxId;
	SceUInt8 m_statusBuf[104];
};

/* 27. GetTitleSyncStatusJob (64 bytes; 0x814285DC / 0x81428604) */
class GetTitleSyncStatusJob : public SceNpTrophyJobBase {
public:
	GetTitleSyncStatusJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
			      int titleSession, int ctxId)
		: SceNpTrophyJobBase("GetTitleSyncStatusJob", session, ipcReqId, handleId),
		  m_titleSession(titleSession),
		  m_ctxId(ctxId)
	{
		sceClibMemset(m_syncStatusBuf, 0, sizeof(m_syncStatusBuf));
	}

	virtual int Run()
	{
		return npTrophyCoreGetTitleSyncStatus(m_session->getProcessId(), m_session,
						      m_handleId, m_titleSession, m_ctxId,
						      m_syncStatusBuf);
	}

	virtual void Finish(int result)
	{
		npTrophySendGetTitleSyncStatusResponse(m_session, m_ipcReqId, result,
						       m_syncStatusBuf);
	}

private:
	int m_titleSession;
	int m_ctxId;
	SceUInt8 m_syncStatusBuf[8];
};

/* 28. DebugUnlockTrophyJob (68 bytes; 0x814286A8 / 0x814286D8) */
class DebugUnlockTrophyJob : public SceNpTrophyJobBase {
public:
	DebugUnlockTrophyJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
			     int ctxId, int trophyId, const SceUInt64 *timestamp)
		: SceNpTrophyJobBase("DebugUnlockTrophyJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_trophyId(trophyId),
		  m_timestamp(*timestamp),
		  m_outPlatinumId(-1)
	{
	}

	virtual int Run()
	{
		return npTrophyCoreDebugUnlockTrophy(m_session->getProcessId(), m_session,
						     m_handleId, m_ctxId, m_trophyId,
						     &m_timestamp, &m_outPlatinumId);
	}

	virtual void Finish(int result)
	{
		npTrophySendDebugUnlockTrophyResponse(m_session, m_ipcReqId, result,
						      &m_outPlatinumId);
	}

private:
	int m_ctxId;
	int m_trophyId;
	SceUInt64 m_timestamp;
	int m_outPlatinumId;
};

/* 29. DebugLockTrophyJob (68 bytes; 0x8142870C / 0x8142873C) */
class DebugLockTrophyJob : public SceNpTrophyJobBase {
public:
	DebugLockTrophyJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
			   int ctxId, int trophyId, const SceUInt64 *timestamp)
		: SceNpTrophyJobBase("DebugLockTrophyJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_trophyId(trophyId),
		  m_timestamp(*timestamp),
		  m_outPlatinumId(-1)
	{
	}

	virtual int Run()
	{
		return npTrophyCoreDebugLockTrophy(m_session->getProcessId(), m_session,
						   m_handleId, m_ctxId, m_trophyId,
						   &m_timestamp, &m_outPlatinumId);
	}

	virtual void Finish(int result)
	{
		npTrophySendDebugLockTrophyResponse(m_session, m_ipcReqId, result,
						    &m_outPlatinumId);
	}

private:
	int m_ctxId;
	int m_trophyId;
	SceUInt64 m_timestamp;
	int m_outPlatinumId;
};

/* 30. UnlockTrophyJob (3604 bytes; 0x81428770 / 0x814287D4) */
class UnlockTrophyJob : public SceNpTrophyJobBase {
public:
	UnlockTrophyJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
			int ctxId, int trophyId, int flags, int inFlag)
		: SceNpTrophyJobBase("UnlockTrophyJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_trophyId(trophyId),
		  m_flags(flags),
		  m_outPlatinumId(-1),
		  m_ioUnlockedFlag(inFlag)
	{
	}

	virtual int Run()
	{
		m_gameDetails.size = sizeof(m_gameDetails);
		m_trophyDetails.size = sizeof(m_trophyDetails);
		m_platinumDetails.size = sizeof(m_platinumDetails);
		return npTrophyCoreUnlockTrophy(m_session->getProcessId(), m_session, m_handleId,
						m_ctxId, m_trophyId, m_flags, &m_outPlatinumId,
						&m_gameDetails, &m_trophyDetails,
						&m_platinumDetails, &m_commId, &m_ioUnlockedFlag);
	}

	virtual void Finish(int result)
	{
		if (result == 0) {
			npTrophyPostUnlockNewEvent(&m_trophyDetails, &m_commId);
			npTrophyPostUnlockActivity(m_session->getProcessId(), &m_trophyDetails,
						   &m_gameDetails, &m_commId);
			if (m_outPlatinumId != -1) {
				npTrophyPostUnlockNewEvent(&m_platinumDetails, &m_commId);
				npTrophyPostUnlockActivity(m_session->getProcessId(),
							   &m_platinumDetails, &m_gameDetails,
							   &m_commId);
			}
		}
		npTrophySendUnlockTrophyResponse(m_session, m_ipcReqId, result,
						 &m_outPlatinumId, &m_ioUnlockedFlag);
	}

private:
	int m_ctxId;				/* +0x30 */
	int m_trophyId;				/* +0x34 */
	int m_flags;				/* +0x38 */
	int m_outPlatinumId;			/* +0x3C */
	NpTrophyDetails m_trophyDetails;	/* +0x40 (1172 bytes) */
	NpTrophyDetails m_platinumDetails;	/* +0x4D4 (1172 bytes) */
	NpTrophyGameDetails m_gameDetails;	/* +0x968 (1180 bytes) */
	NpTrophyCommId m_commId;		/* +0xE04 (12 bytes) */
	int m_ioUnlockedFlag;			/* +0xE10 */
};

/* 31. GetUnlockedTrophyFlagArrayJob (76 bytes; 0x81428868 / 0x81428898) */
class GetUnlockedTrophyFlagArrayJob : public SceNpTrophyJobBase {
public:
	GetUnlockedTrophyFlagArrayJob(IPMI::Session *session, SceUInt32 ipcReqId, int handleId,
				      int ctxId, int mode)
		: SceNpTrophyJobBase("GetUnlockedTrophyFlagArrayJob", session, ipcReqId, handleId),
		  m_ctxId(ctxId),
		  m_mode(mode),
		  m_outCount(0)
	{
		sceClibMemset(&m_flags, 0, sizeof(m_flags));
	}

	virtual int Run()
	{
		return npTrophyCoreGetUnlockedFlagArray(m_session->getProcessId(), m_session,
							m_handleId, m_ctxId, m_mode, &m_flags,
							&m_outCount);
	}

	virtual void Finish(int result)
	{
		npTrophySendGetUnlockedFlagArrayResponse(m_session, m_ipcReqId, result,
							 &m_flags, &m_outCount);
	}

private:
	int m_ctxId;
	int m_mode;
	NpTrophyFlagArray m_flags;
	SceUInt32 m_outCount;
};

/* Enqueue entrypoints (0x81227FB8..0x8122998C) */

int npTrophyEnqueueCheckNetSyncJob(IPMI::Session *session, SceUInt32 ipcReqId,
				   int handleId, int ctxId, SceUInt64 options)
{
	CheckNetSyncJob *job = new CheckNetSyncJob(session, ipcReqId, handleId, ctxId, options);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	int zeroNeed = 0;
	npTrophySendCheckNetSyncResponse(session, ipcReqId, ret, &zeroNeed);
	return 0;
}

int npTrophyEnqueueNetSyncTitlesJob(IPMI::Session *session, SceUInt32 ipcReqId,
				    int handleId, int ctxId, SceUInt64 options)
{
	NetSyncTitlesJob *job = new NetSyncTitlesJob(session, ipcReqId, handleId, ctxId, options);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	npTrophySendNetSyncTitlesResponse(session, ipcReqId, ret);
	return 0;
}

int npTrophyEnqueueCheckRecoveryJob(IPMI::Session *session, SceUInt32 ipcReqId,
				    int handleId, int ctxId)
{
	CheckRecoveryRequiredJob *job =
		new CheckRecoveryRequiredJob(session, ipcReqId, handleId, ctxId);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	int zeroReq = 0;
	npTrophySendCheckRecoveryResponse(session, ipcReqId, ret, &zeroReq);
	return 0;
}

int npTrophyEnqueuePerformRecoveryJob(IPMI::Session *session, SceUInt32 ipcReqId,
				      int handleId, int ctxId)
{
	PerformRecoveryJob *job = new PerformRecoveryJob(session, ipcReqId, handleId, ctxId);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	npTrophySendPerformRecoveryResponse(session, ipcReqId, ret);
	return 0;
}

int npTrophyEnqueueGetTitleForRemovalJob(IPMI::Session *session, SceUInt32 ipcReqId,
					 int handleId, int ctxId)
{
	GetTitleForRemovalJob *job =
		new GetTitleForRemovalJob(session, ipcReqId, handleId, ctxId);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	SceUInt8 emptyInfo[104];
	SceUInt8 emptyExtra[8];
	NpTrophyGameDetails emptyDetails;
	sceClibMemset(emptyInfo, 0, sizeof(emptyInfo));
	sceClibMemset(emptyExtra, 0, sizeof(emptyExtra));
	sceClibMemset(&emptyDetails, 0, sizeof(emptyDetails));
	emptyDetails.size = sizeof(emptyDetails);
	npTrophySendGetTitleForRemovalResponse(session, ipcReqId, ret, emptyInfo,
					       emptyExtra, &emptyDetails);
	return 0;
}

int npTrophyEnqueueRemoveTitleJob(IPMI::Session *session, SceUInt32 ipcReqId,
				  int handleId, int ctxId, int titleHandle)
{
	RemoveTitleJob *job = new RemoveTitleJob(session, ipcReqId, handleId, ctxId, titleHandle);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	npTrophySendRemoveTitleResponse(session, ipcReqId, ret);
	return 0;
}

int npTrophyEnqueueRemoveAllTitlesJob(IPMI::Session *session, SceUInt32 ipcReqId,
				      int handleId, int ctxId)
{
	RemoveAllTitlesJob *job = new RemoveAllTitlesJob(session, ipcReqId, handleId, ctxId);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	npTrophySendRemoveAllTitlesResponse(session, ipcReqId, ret);
	return 0;
}

int npTrophyEnqueueGetUserFileStatusJob(IPMI::Session *session, SceUInt32 ipcReqId,
					int handleId, int ctxId)
{
	GetUserFileStatusJob *job = new GetUserFileStatusJob(session, ipcReqId, handleId, ctxId);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	SceUInt8 emptyStatus[16];
	sceClibMemset(emptyStatus, 0, sizeof(emptyStatus));
	npTrophySendGetUserFileStatusResponse(session, ipcReqId, ret, emptyStatus);
	return 0;
}

int npTrophyEnqueueGetUserTitleInfoJob(IPMI::Session *session, SceUInt32 ipcReqId,
				       int handleId, int ctxId)
{
	GetUserTitleInfoJob *job = new GetUserTitleInfoJob(session, ipcReqId, handleId, ctxId);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	SceUInt8 emptyInfo[32];
	sceClibMemset(emptyInfo, 0, sizeof(emptyInfo));
	npTrophySendGetUserTitleInfoResponse(session, ipcReqId, ret, emptyInfo);
	return 0;
}

int npTrophyEnqueueReadStreamJob(IPMI::Session *session, SceUInt32 ipcReqId,
				 int handleId, int streamId, SceSize readSize)
{
	SceSize clampedSize = (readSize > NP_TROPHY_MAX_STREAM_READ_SIZE)
		? NP_TROPHY_MAX_STREAM_READ_SIZE : readSize;
	ReadStreamJob *job = new ReadStreamJob(session, ipcReqId, handleId, streamId, clampedSize);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	SceSize zeroRead = 0;
	npTrophySendReadStreamResponse(session, ipcReqId, ret, &zeroRead, NULL);
	return 0;
}

int npTrophyEnqueueOpenConfStreamJob(IPMI::Session *session, SceUInt32 ipcReqId,
				     int handleId, int ctxId, int mode, SceUInt64 options,
				     int wantSize)
{
	OpenTrophyConfStreamJob *job = new OpenTrophyConfStreamJob(
		session, ipcReqId, handleId, ctxId, mode, options, wantSize);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	int zeroStream = 0;
	SceUInt64 zeroSize = 0;
	npTrophySendOpenConfStreamResponse(session, ipcReqId, ret, &zeroStream, &zeroSize);
	return 0;
}

int npTrophyEnqueueGetTrophyTitleDataJob(IPMI::Session *session, SceUInt32 ipcReqId,
					 int handleId, int ctxId, int titleHandle)
{
	GetTrophyTitleDataJob *job =
		new GetTrophyTitleDataJob(session, ipcReqId, handleId, ctxId, titleHandle);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	SceUInt8 emptySummary[108];
	SceUInt8 emptyStats[136];
	sceClibMemset(emptySummary, 0, sizeof(emptySummary));
	sceClibMemset(emptyStats, 0, sizeof(emptyStats));
	npTrophySendGetTrophyTitleDataResponse(session, ipcReqId, ret, emptySummary, emptyStats);
	return 0;
}

int npTrophyEnqueueGetTrophyGroupDataJob(IPMI::Session *session, SceUInt32 ipcReqId,
					 int handleId, int ctxId, int titleHandle, int groupId)
{
	GetTrophyGroupDataJob *job = new GetTrophyGroupDataJob(
		session, ipcReqId, handleId, ctxId, titleHandle, groupId);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	SceUInt8 emptySummary[68];
	SceUInt8 emptyStats[56];
	sceClibMemset(emptySummary, 0, sizeof(emptySummary));
	sceClibMemset(emptyStats, 0, sizeof(emptyStats));
	npTrophySendGetTrophyGroupDataResponse(session, ipcReqId, ret, emptySummary, emptyStats);
	return 0;
}

int npTrophyEnqueueGetTrophyDataJob(IPMI::Session *session, SceUInt32 ipcReqId,
				    int handleId, int ctxId, int titleHandle, int trophyId)
{
	GetTrophyDataJob *job = new GetTrophyDataJob(
		session, ipcReqId, handleId, ctxId, titleHandle, trophyId);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	SceUInt8 emptySummary[36];
	SceUInt8 emptyStats[40];
	sceClibMemset(emptySummary, 0, sizeof(emptySummary));
	sceClibMemset(emptyStats, 0, sizeof(emptyStats));
	npTrophySendGetTrophyDataResponse(session, ipcReqId, ret, emptySummary, emptyStats);
	return 0;
}

int npTrophyEnqueueGetNextTitleStatusJob(IPMI::Session *session, SceUInt32 ipcReqId,
					 int handleId, int ctxId, int cursor)
{
	GetNextTitleEntryStatusJob *job =
		new GetNextTitleEntryStatusJob(session, ipcReqId, handleId, ctxId, cursor);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	int invalidCursor = -1;
	SceUInt8 emptyEntry[104];
	sceClibMemset(emptyEntry, 0, sizeof(emptyEntry));
	npTrophySendGetNextTitleStatusResponse(session, ipcReqId, ret, &invalidCursor, emptyEntry);
	return 0;
}

int npTrophyEnqueueOpenTitleIconStreamJob(IPMI::Session *session, SceUInt32 ipcReqId,
					  int handleId, int ctxId, int mode, SceUInt64 options,
					  int wantSize)
{
	OpenTitleIconStreamJob *job = new OpenTitleIconStreamJob(
		session, ipcReqId, handleId, ctxId, mode, options, wantSize);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	int zeroStream = 0;
	SceUInt64 zeroSize = 0;
	npTrophySendOpenTitleIconStreamResponse(session, ipcReqId, ret, &zeroStream, &zeroSize);
	return 0;
}

int npTrophyEnqueueOpenGroupIconStreamJob(IPMI::Session *session, SceUInt32 ipcReqId,
					  int handleId, int ctxId, int mode, int groupId,
					  SceUInt64 options, int wantSize)
{
	OpenGroupIconStreamJob *job = new OpenGroupIconStreamJob(
		session, ipcReqId, handleId, ctxId, mode, groupId, options, wantSize);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	int zeroStream = 0;
	SceUInt64 zeroSize = 0;
	npTrophySendOpenGroupIconStreamResponse(session, ipcReqId, ret, &zeroStream, &zeroSize);
	return 0;
}

int npTrophyEnqueueOpenTrophyIconStreamJob(IPMI::Session *session, SceUInt32 ipcReqId,
					   int handleId, int ctxId, int mode, int trophyId,
					   SceUInt64 options, int wantSize)
{
	OpenTrophyIconStreamJob *job = new OpenTrophyIconStreamJob(
		session, ipcReqId, handleId, ctxId, mode, trophyId, options, wantSize);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	int zeroStream = 0;
	SceUInt64 zeroSize = 0;
	npTrophySendOpenTrophyIconStreamResponse(session, ipcReqId, ret, &zeroStream, &zeroSize);
	return 0;
}

int npTrophyEnqueueGetGameInfoJob(IPMI::Session *session, SceUInt32 ipcReqId,
				  int handleId, int ctxId, int mode, SceUInt64 options)
{
	GetGameInfoJob *job =
		new GetGameInfoJob(session, ipcReqId, handleId, ctxId, mode, options);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	NpTrophyGameDetails emptyDetails;
	NpTrophyGameData emptyData;
	sceClibMemset(&emptyDetails, 0, sizeof(emptyDetails));
	emptyDetails.size = sizeof(emptyDetails);
	sceClibMemset(&emptyData, 0, sizeof(emptyData));
	emptyData.size = sizeof(emptyData);
	npTrophySendGetGameInfoResponse(session, ipcReqId, ret, &emptyDetails, &emptyData);
	return 0;
}

int npTrophyEnqueueGetGroupInfoJob(IPMI::Session *session, SceUInt32 ipcReqId,
				   int handleId, int ctxId, int mode, int groupId,
				   SceUInt64 options)
{
	GetGroupInfoJob *job = new GetGroupInfoJob(
		session, ipcReqId, handleId, ctxId, mode, groupId, options);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	NpTrophyGroupDetails emptyDetails;
	NpTrophyGroupData emptyData;
	sceClibMemset(&emptyDetails, 0, sizeof(emptyDetails));
	emptyDetails.size = sizeof(emptyDetails);
	sceClibMemset(&emptyData, 0, sizeof(emptyData));
	emptyData.size = sizeof(emptyData);
	npTrophySendGetGroupInfoResponse(session, ipcReqId, ret, &emptyDetails, &emptyData);
	return 0;
}

int npTrophyEnqueueGetTrophyInfoJob(IPMI::Session *session, SceUInt32 ipcReqId,
				    int handleId, int ctxId, int mode, int trophyId,
				    SceUInt64 options)
{
	GetTrophyInfoJob *job = new GetTrophyInfoJob(
		session, ipcReqId, handleId, ctxId, mode, trophyId, options);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	NpTrophyDetails emptyDetails;
	NpTrophyData emptyData;
	sceClibMemset(&emptyDetails, 0, sizeof(emptyDetails));
	emptyDetails.size = sizeof(emptyDetails);
	sceClibMemset(&emptyData, 0, sizeof(emptyData));
	emptyData.size = sizeof(emptyData);
	npTrophySendGetTrophyInfoResponse(session, ipcReqId, ret, &emptyDetails, &emptyData);
	return 0;
}

int npTrophyEnqueueGetTitleStatusForGameJob(IPMI::Session *session, SceUInt32 ipcReqId,
					    int handleId, int titleSession, int ctxId)
{
	GetTitleStatusForGameJob *job =
		new GetTitleStatusForGameJob(session, ipcReqId, handleId, titleSession, ctxId);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	int zeroStatus = 0;
	npTrophySendGetTitleStatusForGameResponse(session, ipcReqId, ret, &zeroStatus);
	return 0;
}

int npTrophyEnqueueInstallTitleJob(IPMI::Session *session, SceUInt32 ipcReqId,
				   int handleId, int titleSession, int ctxId)
{
	InstallTitleJob *job =
		new InstallTitleJob(session, ipcReqId, handleId, titleSession, ctxId);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	int invalidRes = -1;
	npTrophySendInstallTitleResponse(session, ipcReqId, ret, &invalidRes);
	return 0;
}

int npTrophyEnqueueUpdateTitleJob(IPMI::Session *session, SceUInt32 ipcReqId,
				  int handleId, int titleSession, int ctxId)
{
	UpdateTitleJob *job = new UpdateTitleJob(session, ipcReqId, handleId, titleSession, ctxId);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	int invalidRes = -1;
	npTrophySendUpdateTitleResponse(session, ipcReqId, ret, &invalidRes);
	return 0;
}

int npTrophyEnqueueSetupTitleJob(IPMI::Session *session, SceUInt32 ipcReqId,
				 int handleId, int titleSession, int ctxId)
{
	SetupTitleJob *job = new SetupTitleJob(session, ipcReqId, handleId, titleSession, ctxId);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	int invalidRes = -1;
	npTrophySendSetupTitleResponse(session, ipcReqId, ret, &invalidRes);
	return 0;
}

int npTrophyEnqueueGetTitleFileStatusJob(IPMI::Session *session, SceUInt32 ipcReqId,
					 int handleId, int titleSession, int ctxId)
{
	GetTitleFileStatusJob *job =
		new GetTitleFileStatusJob(session, ipcReqId, handleId, titleSession, ctxId);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	SceUInt8 emptyStatus[104];
	sceClibMemset(emptyStatus, 0, sizeof(emptyStatus));
	npTrophySendGetTitleFileStatusResponse(session, ipcReqId, ret, emptyStatus);
	return 0;
}

int npTrophyEnqueueGetTitleSyncStatusJob(IPMI::Session *session, SceUInt32 ipcReqId,
					 int handleId, int titleSession, int ctxId)
{
	GetTitleSyncStatusJob *job =
		new GetTitleSyncStatusJob(session, ipcReqId, handleId, titleSession, ctxId);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	SceUInt8 emptyStatus[8];
	sceClibMemset(emptyStatus, 0, sizeof(emptyStatus));
	npTrophySendGetTitleSyncStatusResponse(session, ipcReqId, ret, emptyStatus);
	return 0;
}

int npTrophyEnqueueDebugUnlockTrophyJob(IPMI::Session *session, SceUInt32 ipcReqId,
					int handleId, int ctxId, int trophyId,
					const SceUInt64 *timestamp)
{
	DebugUnlockTrophyJob *job = new DebugUnlockTrophyJob(
		session, ipcReqId, handleId, ctxId, trophyId, timestamp);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	int invalidPlat = -1;
	npTrophySendDebugUnlockTrophyResponse(session, ipcReqId, ret, &invalidPlat);
	return 0;
}

int npTrophyEnqueueDebugLockTrophyJob(IPMI::Session *session, SceUInt32 ipcReqId,
				      int handleId, int ctxId, int trophyId,
				      const SceUInt64 *timestamp)
{
	DebugLockTrophyJob *job = new DebugLockTrophyJob(
		session, ipcReqId, handleId, ctxId, trophyId, timestamp);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	int invalidPlat = -1;
	npTrophySendDebugLockTrophyResponse(session, ipcReqId, ret, &invalidPlat);
	return 0;
}

int npTrophyEnqueueUnlockTrophyJob(IPMI::Session *session, SceUInt32 ipcReqId,
				   int handleId, int ctxId, int trophyId, int flags,
				   const int *inFlag)
{
	UnlockTrophyJob *job = new UnlockTrophyJob(
		session, ipcReqId, handleId, ctxId, trophyId, flags, *inFlag);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	int invalidPlat = -1;
	npTrophySendUnlockTrophyResponse(session, ipcReqId, ret, &invalidPlat, inFlag);
	return 0;
}

int npTrophyEnqueueGetUnlockedFlagArrayJob(IPMI::Session *session, SceUInt32 ipcReqId,
					   int handleId, int ctxId, int mode)
{
	GetUnlockedTrophyFlagArrayJob *job =
		new GetUnlockedTrophyFlagArrayJob(session, ipcReqId, handleId, ctxId, mode);
	int ret = NP_TROPHY_ERR_NO_MEMORY;
	if (job != NULL) {
		ret = npTrophyQueueSubmitJob(job);
		if (ret >= 0)
			return 0;
		delete job;
	}
	NpTrophyFlagArray emptyFlags;
	SceUInt32 zeroCount = 0;
	memset(&emptyFlags, 0, sizeof(emptyFlags));
	npTrophySendGetUnlockedFlagArrayResponse(session, ipcReqId, ret, &emptyFlags, &zeroCount);
	return 0;
}
