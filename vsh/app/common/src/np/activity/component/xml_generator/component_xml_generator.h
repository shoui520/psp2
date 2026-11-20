/*
 * The XML generator component: writes the requests, 116 bytes (Sony's file:
 * component_xml_generator.cpp, whose checks name the Create*Request methods and the helpers
 * convertStatusType, createEscapedXmlString and isInvalidCreateTime; the class name, the size
 * methods and the other helpers are ours).
 *
 * Each request is an <activity> document: the declaration, the user's ticket and JID, then the
 * request's elements. A Create method writes it into the caller's buffer and gives its length
 * without the terminator; its size method gives the buffer the request needs for a ticket of
 * ticketSize bytes (and count items).
 */
#ifndef NP_ACTIVITY_COMPONENT_XML_GENERATOR_COMPONENT_XML_GENERATOR_H
#define NP_ACTIVITY_COMPONENT_XML_GENERATOR_COMPONENT_XML_GENERATOR_H

#include <np/np_common.h>
#include <rtc.h>
#include "../abstract_component.h"
#include "../../common/activity_xml_writer.h"
#include "../../np_activity.h"

/* a JID's text, with its NUL */
#define GENERATOR_JID_SIZE		65
/* a time's text (RFC 3339), with room to spare */
#define GENERATOR_TIME_TEXT_SIZE	128
/* a status's or comment's info: its bytes, and their base64 text */
#define GENERATOR_INFO_SIZE_MAX		0x100
#define GENERATOR_INFO_TEXT_SIZE	0x180

class ComponentXmlGenerator : public Component {
public:
	ComponentXmlGenerator();
	virtual ~ComponentXmlGenerator();

	virtual SceSize GetGetAccountSettingRequestSize(SceSize ticketSize);
	virtual int CreateGetAccountSettingRequest(const SceNpId *npId, const char *ticket,
		SceSize ticketSize, char *buffer, SceSize bufferSize, SceSize *size);
	virtual SceSize GetSetAccountSettingRequestSize(SceSize ticketSize);
	/* setting: whether the user's statuses are public, and editable */
	virtual int CreateSetAccountSettingRequest(const SceNpId *npId, const char *ticket,
		SceSize ticketSize, const int *setting, char *buffer, SceSize bufferSize,
		SceSize *size);
	virtual SceSize GetAddStatusListRequestSize(SceSize ticketSize, SceSize count);
	virtual int CreateAddStatusListRequest(const SceNpId *npId, const char *ticket,
		SceSize ticketSize, const NpActivityStatusInfo *infos, SceSize count, char *buffer,
		SceSize bufferSize, SceSize *size);
	virtual SceSize GetAddCommentRequestSize(SceSize ticketSize);
	virtual int CreateAddCommentRequest(const SceNpId *npId, const char *ticket,
		SceSize ticketSize, const NpActivityStatusKey *key, const NpActivityCommentPost *comment,
		char *buffer, SceSize bufferSize, SceSize *size);
	virtual SceSize GetAddVoteGoodJobRequestSize(SceSize ticketSize);
	virtual int CreateAddVoteGoodJobRequest(const SceNpId *npId, const char *ticket,
		SceSize ticketSize, const NpActivityStatusKey *key, char *buffer, SceSize bufferSize,
		SceSize *size);
	virtual SceSize GetCheckStatusRequestSize(SceSize ticketSize, SceSize count);
	virtual int CreateCheckStatusRequest(const SceNpId *npId, const char *ticket,
		SceSize ticketSize, const SceRtcTick *since,
		const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count, char *buffer,
		SceSize bufferSize, SceSize *size);
	virtual SceSize GetGetFriendStatusTimelineRequestSize(SceSize ticketSize,
		const NpActivityRange *range);
	virtual int CreateGetFriendStatusTimelineRequest(const SceNpId *npId, const char *ticket,
		SceSize ticketSize, const NpActivityRange *range, char *buffer, SceSize bufferSize,
		SceSize *size);
	virtual SceSize GetGetFriendStatusTimelineByTitleRequestSize(SceSize ticketSize,
		SceSize count, const NpActivityRange *range);
	virtual int CreateGetFriendStatusTimelineByTitleRequest(const SceNpId *npId,
		const char *ticket, SceSize ticketSize,
		const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count,
		const NpActivityRange *range, char *buffer, SceSize bufferSize, SceSize *size);
	virtual SceSize GetGetStatusDetailRequestSize(SceSize ticketSize,
		const NpActivityDetailParam *param);
	virtual int CreateGetStatusDetailRequest(const SceNpId *npId, const char *ticket,
		SceSize ticketSize, const NpActivityStatusKey *key, const NpActivityDetailParam *param,
		char *buffer, SceSize bufferSize, SceSize *size);
	virtual SceSize GetGetStatusDetailMultiRequestSize(SceSize ticketSize, SceSize count);
	virtual int CreateGetStatusDetailMultiRequest(const SceNpId *npId, const char *ticket,
		SceSize ticketSize, const NpActivityStatusKey *keys, SceSize count,
		const NpActivityDetailParam *param, char *buffer, SceSize bufferSize, SceSize *size);
	virtual SceSize GetGetStatusInfoRequestSize(SceSize ticketSize, SceSize count);
	virtual int CreateGetStatusInfoRequest(const SceNpId *npId, const char *ticket,
		SceSize ticketSize, const NpActivityStatusKey *keys, SceSize count, const int *locale,
		char *buffer, SceSize bufferSize, SceSize *size);
	virtual SceSize GetGetStatusTimelineByAccountRequestSize(SceSize ticketSize, SceSize count,
		const int *max);
	virtual int CreateGetStatusTimelineByAccountRequest(const SceNpId *npId, const char *ticket,
		SceSize ticketSize, const NpActivityOwner *owners, SceSize count, const int *max,
		char *buffer, SceSize bufferSize, SceSize *size);
	virtual SceSize GetGetStatusTimelineByAccountAndTitleRequestSize(SceSize ticketSize,
		SceSize count);
	/* the request names the first title only, and no maximum */
	virtual int CreateGetStatusTimelineByAccountAndTitleRequest(const SceNpId *npId,
		const char *ticket, SceSize ticketSize, const NpActivityOwner *owners, SceSize count,
		const char *titleId, const int *max, char *buffer, SceSize bufferSize,
		SceSize *size);
	virtual SceSize GetRemoveCommentRequestSize(SceSize ticketSize);
	virtual int CreateRemoveCommentRequest(const SceNpId *npId, const char *ticket,
		SceSize ticketSize, const NpActivityCommentKey *key, char *buffer, SceSize bufferSize,
		SceSize *size);
	virtual SceSize GetRemoveStatusRequestSize(SceSize ticketSize);
	virtual int CreateRemoveStatusRequest(const SceNpId *npId, const char *ticket,
		SceSize ticketSize, int statusId, char *buffer, SceSize bufferSize, SceSize *size);
	virtual SceSize GetRemoveVoteGoodJobRequestSize(SceSize ticketSize);
	virtual int CreateRemoveVoteGoodJobRequest(const SceNpId *npId, const char *ticket,
		SceSize ticketSize, const NpActivityStatusKey *key, char *buffer, SceSize bufferSize,
		SceSize *size);

private:
	int writeHeader(const SceNpId *npId, const char *ticket, SceSize ticketSize);
	int writeUserId(const char *name);
	int writeFooter(SceSize *size);
	int writeInfo(const SceUInt8 *info, SceSize size);
	int writeActionUrl(const char *url);
	int writeIconUrl(const char *url);
	int writeText(const char *text);
	int writeTitleName(const char *name);
	int writeTitleId(const char *titleId);
	int writeContentId(const char *contentId);
	int writeCommentInfo(const SceUInt8 *info, SceSize size);
	int writeCommentText(const char *text);
	int writeOwnerId(const NpActivityOwner *owner);
	int convertStatusType(int type);
	int createEscapedXmlString(const char *text, SceSize max, char **escaped);
	int isInvalidCreateTime(const SceUInt64 *time);

	ActivityXmlWriter m_writer;
	char m_jid[GENERATOR_JID_SIZE];		/* the user's, from the NP ID the request is for */
};

#endif
