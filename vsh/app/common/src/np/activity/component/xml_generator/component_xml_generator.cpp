/*
 * The XML generator: the requests the commands send, written into their buffers (Sony's file)
 */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include <np_common/np_common.h>
#include "../../common/activity_debug.h"
#include "../../common/activity_memory.h"
#include "../../np_activity.h"
#include "../utility/component_utility.h"
#include "component_xml_generator.h"

#define ACTIVITY_FILE	ACTIVITY_SRC_DIR "component\\xml_generator\\component_xml_generator.cpp"

/* the longest text each element takes, with its NUL */
#define GENERATOR_ACTION_URL_SIZE	0x401
#define GENERATOR_ICON_URL_SIZE		0x101
#define GENERATOR_TEXT_SIZE		0x401
#define GENERATOR_TITLE_NAME_SIZE	0x81
#define GENERATOR_CONTENT_ID_SIZE	48
#define GENERATOR_COMMENT_TEXT_SIZE	NP_ACTIVITY_COMMENT_POST_MESSAGE_SIZE

/* the icon URLs a status names in place of a trophy grade's icon */
#define GENERATOR_TROPHY_ICON_BRONZE	"psactivity:icon?type=trophy&grade=bronze"
#define GENERATOR_TROPHY_ICON_SILVER	"psactivity:icon?type=trophy&grade=silver"
#define GENERATOR_TROPHY_ICON_GOLD	"psactivity:icon?type=trophy&grade=gold"
#define GENERATOR_TROPHY_ICON_HIDDEN	"psactivity:icon?type=trophy&grade=hidden"
#define GENERATOR_TROPHY_ICON_PLATINUM	"psactivity:icon?type=trophy&grade=platinum"

ComponentXmlGenerator::ComponentXmlGenerator()
{
	sceClibMemset(m_jid, 0, sizeof(m_jid));
}

ComponentXmlGenerator::~ComponentXmlGenerator()
{
}

SceSize ComponentXmlGenerator::GetGetAccountSettingRequestSize(SceSize ticketSize)
{
	return ticketSize + 640;
}

/* <declaration><activity platform= sv=><ticket/><ticketjid/> */
int ComponentXmlGenerator::writeHeader(const SceNpId *npId, const char *ticket, SceSize ticketSize)
{
	int ret;

	sceClibMemset(m_jid, 0, sizeof(m_jid));
	ret = m_utility->ConvertNpIdToJid(npId, m_jid, sizeof(m_jid));
	if (ret < 0)
		return ret;
	ret = m_writer.WriteDeclaration(m_utility->GetXmlDeclaration());
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("activity");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttribute("platform", m_utility->GetPlatform());
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttribute("sv", m_utility->GetNpVersion());
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("ticket");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteText(ticket);
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("ticket");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("ticketjid");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteText(m_jid);
	if (ret < 0)
		return ret;
	return m_writer.EndElement("ticketjid");
}

/* the user's JID in a <name type="0"> element */
inline int ComponentXmlGenerator::writeUserId(const char *name)
{
	int ret;

	ret = m_writer.StartElement(name);
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("type", NP_ACTIVITY_OWNER_USER);
	if (ret < 0)
		return ret;
	ret = m_writer.WriteText(m_jid);
	if (ret < 0)
		return ret;
	return m_writer.EndElement(name);
}

/* ends the document; *size gets its length without the terminator */
inline int ComponentXmlGenerator::writeFooter(SceSize *size)
{
	SceSize length = 0;
	int ret;

	ret = m_writer.EndElement("activity");
	if (ret >= 0) {
		ret = m_writer.Finish(&length);
		if (ret >= 0 && length != 0)
			length--;
	}
	if (ret >= 0)
		*size = length;
	return ret;
}

int ComponentXmlGenerator::CreateGetAccountSettingRequest(const SceNpId *npId, const char *ticket,
	SceSize ticketSize, char *buffer, SceSize bufferSize, SceSize *size)
{
	int ret;

#line 108
	ACTIVITY_ASSERT(m_utility != NULL, "CreateGetAccountSettingRequest");
	ret = m_writer.SetBuffer(buffer, bufferSize);
	if (ret < 0)
		return ret;
	ret = writeHeader(npId, ticket, ticketSize);
	if (ret < 0)
		return ret;
	ret = writeUserId("account");
	if (ret < 0)
		return ret;
	return writeFooter(size);
}

SceSize ComponentXmlGenerator::GetSetAccountSettingRequestSize(SceSize ticketSize)
{
	return ticketSize + 704;
}

int ComponentXmlGenerator::CreateSetAccountSettingRequest(const SceNpId *npId, const char *ticket,
	SceSize ticketSize, const int *setting, char *buffer, SceSize bufferSize, SceSize *size)
{
	int ret;

#line 172
	ACTIVITY_ASSERT(m_utility != NULL, "CreateSetAccountSettingRequest");
	ret = m_writer.SetBuffer(buffer, bufferSize);
	if (ret < 0)
		return ret;
	ret = writeHeader(npId, ticket, ticketSize);
	if (ret < 0)
		return ret;
	ret = writeUserId("account");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("user-range");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("public");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteInt(setting[0]);
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("public");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("editable");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteInt(setting[1]);
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("editable");
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("user-range");
	if (ret < 0)
		return ret;
	return writeFooter(size);
}

SceSize ComponentXmlGenerator::GetAddStatusListRequestSize(SceSize ticketSize, SceSize count)
{
	return ticketSize + count * 3636 + 512;
}

int ComponentXmlGenerator::CreateAddStatusListRequest(const SceNpId *npId, const char *ticket,
	SceSize ticketSize, const NpActivityStatusInfo *infos, SceSize count, char *buffer,
	SceSize bufferSize, SceSize *size)
{
	const NpActivityStatusInfo *info;
	SceRtcTick now;
	SceSize i;
	int ret;

#line 273
	ACTIVITY_ASSERT(m_utility != NULL, "CreateAddStatusListRequest");
	ret = m_utility->GetCurrentNetworkTime(&now);
	if (ret < 0)
		return ret;
	ret = m_writer.SetBuffer(buffer, bufferSize);
	if (ret < 0)
		return ret;
	ret = writeHeader(npId, ticket, ticketSize);
	if (ret < 0)
		return ret;
	ret = writeUserId("fromid");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("status-list");
	if (ret < 0)
		return ret;
	for (i = 0; i < count; i++) {
		info = &infos[i];
		ret = m_writer.StartElement("status");
		if (ret < 0)
			return ret;
		ret = m_writer.WriteAttributeInt("setindex", i + 1);
		if (ret < 0)
			return ret;
		ret = m_writer.WriteAttribute("pf", m_utility->GetPlatform());
		if (ret < 0)
			return ret;
		ret = m_writer.WriteAttributeInt("type", convertStatusType(info->type));
		if (ret < 0)
			return ret;
		/* a status from the future, or of no time, is dated now */
		if (info->creationTime <= now.tick && !isInvalidCreateTime(&info->creationTime)) {
			char date[GENERATOR_TIME_TEXT_SIZE];

			sceClibMemset(date, 0, sizeof(date));
			ret = m_utility->ConvertSceRtcTickToTimeText(
				(const SceRtcTick *)&info->creationTime, date, sizeof(date));
			if (ret >= 0)
				ret = m_writer.WriteAttribute("date", date);
		} else {
			char date[GENERATOR_TIME_TEXT_SIZE];

			sceClibMemset(date, 0, sizeof(date));
			ret = m_utility->ConvertSceRtcTickToTimeText(&now, date, sizeof(date));
			if (ret >= 0)
				ret = m_writer.WriteAttribute("date", date);
		}
		if (ret < 0)
			return ret;
		ret = writeTitleId(info->titleId);
		if (ret < 0)
			return ret;
		ret = writeTitleName(info->titleName);
		if (ret < 0)
			return ret;
		ret = writeContentId(info->contentId);
		if (ret < 0)
			return ret;
		ret = writeText(info->message);
		if (ret < 0)
			return ret;
		ret = writeIconUrl(info->iconUrl);
		if (ret < 0)
			return ret;
		ret = writeActionUrl(info->actionUrl);
		if (ret < 0)
			return ret;
		ret = writeInfo(info->info, info->infoSize);
		if (ret < 0)
			return ret;
		ret = m_writer.EndElement("status");
		if (ret < 0)
			return ret;
	}
	ret = m_writer.EndElement("status-list");
	if (ret < 0)
		return ret;
	return writeFooter(size);
}

SceSize ComponentXmlGenerator::GetAddCommentRequestSize(SceSize ticketSize)
{
	return ticketSize + 1993;
}

int ComponentXmlGenerator::CreateAddCommentRequest(const SceNpId *npId, const char *ticket,
	SceSize ticketSize, const NpActivityStatusKey *key, const NpActivityCommentPost *comment,
	char *buffer, SceSize bufferSize, SceSize *size)
{
	int ret;

#line 442
	ACTIVITY_ASSERT(m_utility != NULL, "CreateAddCommentRequest");
	ret = m_writer.SetBuffer(buffer, bufferSize);
	if (ret < 0)
		return ret;
	ret = writeHeader(npId, ticket, ticketSize);
	if (ret < 0)
		return ret;
	ret = writeUserId("fromid");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("status");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("id", key->statusId);
	if (ret < 0)
		return ret;
	ret = writeOwnerId(&key->owner);
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("comment");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttribute("pf", m_utility->GetPlatform());
	if (ret < 0)
		return ret;
	ret = writeCommentText(comment->message);
	if (ret < 0)
		return ret;
	ret = writeCommentInfo(comment->info, comment->infoSize);
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("comment");
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("status");
	if (ret < 0)
		return ret;
	return writeFooter(size);
}

SceSize ComponentXmlGenerator::GetAddVoteGoodJobRequestSize(SceSize ticketSize)
{
	return ticketSize + 640;
}

int ComponentXmlGenerator::CreateAddVoteGoodJobRequest(const SceNpId *npId, const char *ticket,
	SceSize ticketSize, const NpActivityStatusKey *key, char *buffer, SceSize bufferSize,
	SceSize *size)
{
	int ret;

#line 550
	ACTIVITY_ASSERT(m_utility != NULL, "CreateAddVoteGoodJobRequest");
	ret = m_writer.SetBuffer(buffer, bufferSize);
	if (ret < 0)
		return ret;
	ret = writeHeader(npId, ticket, ticketSize);
	if (ret < 0)
		return ret;
	ret = writeUserId("fromid");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("status");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("id", key->statusId);
	if (ret < 0)
		return ret;
	ret = writeOwnerId(&key->owner);
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("status");
	if (ret < 0)
		return ret;
	return writeFooter(size);
}

SceSize ComponentXmlGenerator::GetCheckStatusRequestSize(SceSize ticketSize, SceSize count)
{
	return ticketSize + count * 30 + 576;
}

int ComponentXmlGenerator::CreateCheckStatusRequest(const SceNpId *npId, const char *ticket,
	SceSize ticketSize, const SceRtcTick *since,
	const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE], SceSize count, char *buffer,
	SceSize bufferSize, SceSize *size)
{
	char date[GENERATOR_TIME_TEXT_SIZE];
	SceSize i;
	int ret;

#line 628
	ACTIVITY_ASSERT(m_utility != NULL, "CreateCheckStatusRequest");
	ret = m_writer.SetBuffer(buffer, bufferSize);
	if (ret < 0)
		return ret;
	ret = writeHeader(npId, ticket, ticketSize);
	if (ret < 0)
		return ret;
	ret = writeUserId("fromid");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("from-time");
	if (ret < 0)
		return ret;
	sceClibMemset(date, 0, sizeof(date));
	ret = m_utility->ConvertSceRtcTickToTimeText(since, date, sizeof(date));
	if (ret >= 0)
		ret = m_writer.WriteText(date);
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("from-time");
	if (ret < 0)
		return ret;
	if (count != 0 && titleIds != NULL) {
		ret = m_writer.StartElement("titleid-list");
		if (ret < 0)
			return ret;
		for (i = 0; i < count; i++) {
			ret = writeTitleId(titleIds[i]);
			if (ret < 0)
				return ret;
		}
		ret = m_writer.EndElement("titleid-list");
		if (ret < 0)
			return ret;
	}
	return writeFooter(size);
}

SceSize ComponentXmlGenerator::GetGetFriendStatusTimelineRequestSize(SceSize ticketSize)
{
	return ticketSize + 640;
}

int ComponentXmlGenerator::CreateGetFriendStatusTimelineRequest(const SceNpId *npId,
	const char *ticket, SceSize ticketSize, const NpActivityRange *range, char *buffer,
	SceSize bufferSize, SceSize *size)
{
	int ret;

#line 722
	ACTIVITY_ASSERT(m_utility != NULL, "CreateGetFriendStatusTimelineRequest");
	ret = m_writer.SetBuffer(buffer, bufferSize);
	if (ret < 0)
		return ret;
	ret = writeHeader(npId, ticket, ticketSize);
	if (ret < 0)
		return ret;
	ret = writeUserId("fromid");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("timeline-status-list");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttribute("type", "oaf");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("start", range->start);
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("max", range->count);
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("timeline-status-list");
	if (ret < 0)
		return ret;
	return writeFooter(size);
}

SceSize ComponentXmlGenerator::GetGetFriendStatusTimelineByTitleRequestSize(SceSize ticketSize,
	SceSize count)
{
	return ticketSize + count * 128 + 512;
}

int ComponentXmlGenerator::CreateGetFriendStatusTimelineByTitleRequest(const SceNpId *npId,
	const char *ticket, SceSize ticketSize, const char (*titleIds)[NP_ACTIVITY_TITLE_ID_SIZE],
	SceSize count, const NpActivityRange *range, char *buffer, SceSize bufferSize,
	SceSize *size)
{
	char titleId[NP_ACTIVITY_TITLE_ID_SIZE];
	SceSize i;
	int ret;

#line 800
	ACTIVITY_ASSERT(m_utility != NULL, "CreateGetFriendStatusTimelineByTitleRequest");
	ret = m_writer.SetBuffer(buffer, bufferSize);
	if (ret < 0)
		return ret;
	ret = writeHeader(npId, ticket, ticketSize);
	if (ret < 0)
		return ret;
	ret = writeUserId("fromid");
	if (ret < 0)
		return ret;
	for (i = 0; i < count; i++) {
		ret = m_writer.StartElement("timeline-status-list");
		if (ret < 0)
			return ret;
		ret = m_writer.WriteAttribute("type", "all");
		if (ret < 0)
			return ret;
		ret = m_writer.WriteAttributeInt("start", range->start);
		if (ret < 0)
			return ret;
		ret = m_writer.WriteAttributeInt("max", range->count);
		if (ret < 0)
			return ret;
		sceClibMemset(titleId, 0, sizeof(titleId));
		sceClibMemcpy_safe(titleId, titleIds[i], sizeof(titleId));
		titleId[sizeof(titleId) - 1] = '\0';
		ret = m_writer.WriteAttribute("titleid", titleId);
		if (ret < 0)
			return ret;
		ret = m_writer.EndElement("timeline-status-list");
		if (ret < 0)
			return ret;
	}
	return writeFooter(size);
}

SceSize ComponentXmlGenerator::GetGetStatusDetailRequestSize(SceSize ticketSize)
{
	return ticketSize + 768;
}

int ComponentXmlGenerator::CreateGetStatusDetailRequest(const SceNpId *npId, const char *ticket,
	SceSize ticketSize, const NpActivityStatusKey *key, const NpActivityDetailParam *param,
	char *buffer, SceSize bufferSize, SceSize *size)
{
	int ret;

#line 882
	ACTIVITY_ASSERT(m_utility != NULL, "CreateGetStatusDetailRequest");
	ret = m_writer.SetBuffer(buffer, bufferSize);
	if (ret < 0)
		return ret;
	ret = writeHeader(npId, ticket, ticketSize);
	if (ret < 0)
		return ret;
	ret = writeUserId("fromid");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("status-list");
	if (ret < 0)
		return ret;
	if (param->locale != -1) {
		ret = m_writer.WriteAttributeInt("lang", param->locale);
		if (ret < 0)
			return ret;
	}
	ret = m_writer.StartElement("status");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("id", key->statusId);
	if (ret < 0)
		return ret;
	ret = writeOwnerId(&key->owner);
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("status");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("comment-list");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("start", param->comments.start);
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("max", param->comments.count);
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("comment-list");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("vote-list");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("start", param->voters.start);
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("max", param->voters.count);
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("vote-list");
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("status-list");
	if (ret < 0)
		return ret;
	return writeFooter(size);
}

SceSize ComponentXmlGenerator::GetGetStatusDetailMultiRequestSize(SceSize ticketSize,
	SceSize count)
{
	return ticketSize + count * 128 + 768;
}

int ComponentXmlGenerator::CreateGetStatusDetailMultiRequest(const SceNpId *npId,
	const char *ticket, SceSize ticketSize, const NpActivityStatusKey *keys, SceSize count,
	const NpActivityDetailParam *param, char *buffer, SceSize bufferSize, SceSize *size)
{
	SceSize i;
	int ret;

#line 1001
	ACTIVITY_ASSERT(m_utility != NULL, "CreateGetStatusDetailMultiRequest");
	ret = m_writer.SetBuffer(buffer, bufferSize);
	if (ret < 0)
		return ret;
	ret = writeHeader(npId, ticket, ticketSize);
	if (ret < 0)
		return ret;
	ret = writeUserId("fromid");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("status-list");
	if (ret < 0)
		return ret;
	if (param->locale != -1) {
		ret = m_writer.WriteAttributeInt("lang", param->locale);
		if (ret < 0)
			return ret;
	}
	for (i = 0; i < count; i++) {
		ret = m_writer.StartElement("status");
		if (ret < 0)
			return ret;
		ret = m_writer.WriteAttributeInt("id", keys[i].statusId);
		if (ret < 0)
			return ret;
		ret = writeOwnerId(&keys[i].owner);
		if (ret < 0)
			return ret;
		ret = m_writer.EndElement("status");
		if (ret < 0)
			return ret;
	}
	ret = m_writer.StartElement("comment-list");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("start", param->comments.start);
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("max", param->comments.count);
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("comment-list");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("vote-list");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("start", param->voters.start);
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("max", param->voters.count);
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("vote-list");
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("status-list");
	if (ret < 0)
		return ret;
	return writeFooter(size);
}

SceSize ComponentXmlGenerator::GetGetStatusInfoRequestSize(SceSize ticketSize, SceSize count)
{
	return ticketSize + count * 128 + 512;
}

int ComponentXmlGenerator::CreateGetStatusInfoRequest(const SceNpId *npId, const char *ticket,
	SceSize ticketSize, const NpActivityStatusKey *keys, SceSize count, const int *locale,
	char *buffer, SceSize bufferSize, SceSize *size)
{
	SceSize i;
	int ret;

#line 1122
	ACTIVITY_ASSERT(m_utility != NULL, "CreateGetStatusInfoRequest");
	ret = m_writer.SetBuffer(buffer, bufferSize);
	if (ret < 0)
		return ret;
	ret = writeHeader(npId, ticket, ticketSize);
	if (ret < 0)
		return ret;
	ret = writeUserId("fromid");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("status-list");
	if (ret < 0)
		return ret;
	if (*locale != -1) {
		ret = m_writer.WriteAttributeInt("lang", *locale);
		if (ret < 0)
			return ret;
	}
	for (i = 0; i < count; i++) {
		ret = m_writer.StartElement("status");
		if (ret < 0)
			return ret;
		ret = m_writer.WriteAttributeInt("id", keys[i].statusId);
		if (ret < 0)
			return ret;
		ret = writeOwnerId(&keys[i].owner);
		if (ret < 0)
			return ret;
		ret = m_writer.EndElement("status");
		if (ret < 0)
			return ret;
	}
	ret = m_writer.EndElement("status-list");
	if (ret < 0)
		return ret;
	return writeFooter(size);
}

SceSize ComponentXmlGenerator::GetGetStatusTimelineByAccountRequestSize(SceSize ticketSize,
	SceSize count)
{
	return ticketSize + count * 160 + 512;
}

int ComponentXmlGenerator::CreateGetStatusTimelineByAccountRequest(const SceNpId *npId,
	const char *ticket, SceSize ticketSize, const NpActivityOwner *owners, SceSize count,
	const int *max, char *buffer, SceSize bufferSize, SceSize *size)
{
	SceSize i;
	int ret;

#line 1220
	ACTIVITY_ASSERT(m_utility != NULL, "CreateGetStatusTimelineByAccountRequest");
	ret = m_writer.SetBuffer(buffer, bufferSize);
	if (ret < 0)
		return ret;
	ret = writeHeader(npId, ticket, ticketSize);
	if (ret < 0)
		return ret;
	ret = writeUserId("fromid");
	if (ret < 0)
		return ret;
	for (i = 0; i < count; i++) {
		ret = m_writer.StartElement("account-status-list");
		if (ret < 0)
			return ret;
		ret = m_writer.WriteAttributeInt("max", *max);
		if (ret < 0)
			return ret;
		ret = writeOwnerId(&owners[i]);
		if (ret < 0)
			return ret;
		ret = m_writer.EndElement("account-status-list");
		if (ret < 0)
			return ret;
	}
	return writeFooter(size);
}

SceSize ComponentXmlGenerator::GetGetStatusTimelineByAccountAndTitleRequestSize(
	SceSize ticketSize, SceSize count)
{
	return ticketSize + count * 160 + 512;
}

int ComponentXmlGenerator::CreateGetStatusTimelineByAccountAndTitleRequest(const SceNpId *npId,
	const char *ticket, SceSize ticketSize, const NpActivityOwner *owners, SceSize count,
	const char *titleId, const int *max, char *buffer, SceSize bufferSize, SceSize *size)
{
	char text[NP_ACTIVITY_TITLE_ID_SIZE];
	SceSize i;
	int ret;

#line 1308
	ACTIVITY_ASSERT(m_utility != NULL, "CreateGetStatusTimelineByAccountAndTitleRequest");
	ret = m_writer.SetBuffer(buffer, bufferSize);
	if (ret < 0)
		return ret;
	ret = writeHeader(npId, ticket, ticketSize);
	if (ret < 0)
		return ret;
	ret = writeUserId("fromid");
	if (ret < 0)
		return ret;
	for (i = 0; i < count; i++) {
		ret = m_writer.StartElement("account-status-list");
		if (ret < 0)
			return ret;
		sceClibMemset(text, 0, sizeof(text));
		sceClibMemcpy_safe(text, titleId, sizeof(text));
		text[sizeof(text) - 1] = '\0';
		ret = m_writer.WriteAttribute("titleid", text);
		if (ret < 0)
			return ret;
		ret = writeOwnerId(&owners[i]);
		if (ret < 0)
			return ret;
		ret = m_writer.EndElement("account-status-list");
		if (ret < 0)
			return ret;
	}
	return writeFooter(size);
}

SceSize ComponentXmlGenerator::GetRemoveCommentRequestSize(SceSize ticketSize)
{
	return ticketSize + 672;
}

int ComponentXmlGenerator::CreateRemoveCommentRequest(const SceNpId *npId, const char *ticket,
	SceSize ticketSize, const NpActivityCommentKey *key, char *buffer, SceSize bufferSize,
	SceSize *size)
{
	int ret;

#line 1396
	ACTIVITY_ASSERT(m_utility != NULL, "CreateRemoveCommentRequest");
	ret = m_writer.SetBuffer(buffer, bufferSize);
	if (ret < 0)
		return ret;
	ret = writeHeader(npId, ticket, ticketSize);
	if (ret < 0)
		return ret;
	ret = writeUserId("fromid");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("status");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("id", key->statusId);
	if (ret < 0)
		return ret;
	ret = writeOwnerId(&key->owner);
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("comment");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("id", key->commentId);
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("comment");
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("status");
	if (ret < 0)
		return ret;
	return writeFooter(size);
}

SceSize ComponentXmlGenerator::GetRemoveStatusRequestSize(SceSize ticketSize)
{
	return ticketSize + 576;
}

int ComponentXmlGenerator::CreateRemoveStatusRequest(const SceNpId *npId, const char *ticket,
	SceSize ticketSize, int statusId, char *buffer, SceSize bufferSize, SceSize *size)
{
	int ret;

#line 1479
	ACTIVITY_ASSERT(m_utility != NULL, "CreateRemoveStatusRequest");
	ret = m_writer.SetBuffer(buffer, bufferSize);
	if (ret < 0)
		return ret;
	ret = writeHeader(npId, ticket, ticketSize);
	if (ret < 0)
		return ret;
	ret = writeUserId("fromid");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("status");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("id", statusId);
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("status");
	if (ret < 0)
		return ret;
	return writeFooter(size);
}

SceSize ComponentXmlGenerator::GetRemoveVoteGoodJobRequestSize(SceSize ticketSize)
{
	return ticketSize + 640;
}

int ComponentXmlGenerator::CreateRemoveVoteGoodJobRequest(const SceNpId *npId, const char *ticket,
	SceSize ticketSize, const NpActivityStatusKey *key, char *buffer, SceSize bufferSize,
	SceSize *size)
{
	int ret;

#line 1547
	ACTIVITY_ASSERT(m_utility != NULL, "CreateRemoveVoteGoodJobRequest");
	ret = m_writer.SetBuffer(buffer, bufferSize);
	if (ret < 0)
		return ret;
	ret = writeHeader(npId, ticket, ticketSize);
	if (ret < 0)
		return ret;
	ret = writeUserId("fromid");
	if (ret < 0)
		return ret;
	ret = m_writer.StartElement("status");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("id", key->statusId);
	if (ret < 0)
		return ret;
	ret = writeOwnerId(&key->owner);
	if (ret < 0)
		return ret;
	ret = m_writer.EndElement("status");
	if (ret < 0)
		return ret;
	return writeFooter(size);
}

/*
 * The helpers. SNC places each before the first method that calls it.
 */

/* <info>: size bytes of info in base64 */
int ComponentXmlGenerator::writeInfo(const SceUInt8 *info, SceSize size)
{
	char *text;
	int ret;

	if (size == 0)
		return 0;
	if (size > GENERATOR_INFO_SIZE_MAX)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = m_writer.StartElement("info");
	if (ret < 0)
		return ret;
	text = (char *)activityMalloc(GENERATOR_INFO_TEXT_SIZE);
	if (text == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	sceClibMemset(text, 0, GENERATOR_INFO_TEXT_SIZE);
	ret = m_utility->Base64Encode(info, size, text, GENERATOR_INFO_TEXT_SIZE);
	if (ret < 0) {
		activityFree(text);
		return ret;
	}
	ret = m_writer.WriteText(text);
	activityFree(text);
	if (ret < 0)
		return ret;
	return m_writer.EndElement("info");
}

int ComponentXmlGenerator::writeActionUrl(const char *url)
{
	char *escaped;
	int length;
	int ret;

	length = sceClibStrnlen(url, GENERATOR_ACTION_URL_SIZE);
	if (length == 0)
		return 0;
	if (length >= GENERATOR_ACTION_URL_SIZE)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = m_writer.StartElement("action-url");
	if (ret < 0)
		return ret;
	ret = createEscapedXmlString(url, GENERATOR_ACTION_URL_SIZE, &escaped);
	if (ret < 0) {
		if (escaped != NULL)
			activityFree(escaped);
		return ret;
	}
	ret = m_writer.WriteText(escaped);
	if (escaped != NULL)
		activityFree(escaped);
	if (ret < 0)
		return ret;
	return m_writer.EndElement("action-url");
}

/* <icon-url>; a trophy grade's icon is the base URL and the grade's icon the server gave */
int ComponentXmlGenerator::writeIconUrl(const char *url)
{
	char trophyUrl[GENERATOR_ICON_URL_SIZE];
	const char *iconUrl;
	char *escaped;
	int length;
	int ret;

	length = sceClibStrnlen(url, GENERATOR_ICON_URL_SIZE);
	if (length == 0)
		return 0;
	if (length >= GENERATOR_ICON_URL_SIZE)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	iconUrl = url;
	sceClibMemset(trophyUrl, 0, sizeof(trophyUrl));
	if (sceClibStrncmp(url, GENERATOR_TROPHY_ICON_BRONZE, GENERATOR_ICON_URL_SIZE) == 0) {
		if (sceClibStrnlen(m_utility->GetTrophyIconBaseUrl(), GENERATOR_ICON_URL_SIZE) == 0 ||
		    sceClibStrnlen(m_utility->GetTrophyIconBronze(), GENERATOR_ICON_URL_SIZE) == 0)
			return 0;
		iconUrl = trophyUrl;
		sceClibSnprintf(trophyUrl, sizeof(trophyUrl), "%s%s",
			m_utility->GetTrophyIconBaseUrl(), m_utility->GetTrophyIconBronze());
	} else if (sceClibStrncmp(url, GENERATOR_TROPHY_ICON_SILVER, GENERATOR_ICON_URL_SIZE) == 0) {
		if (sceClibStrnlen(m_utility->GetTrophyIconBaseUrl(), GENERATOR_ICON_URL_SIZE) == 0 ||
		    sceClibStrnlen(m_utility->GetTrophyIconSilver(), GENERATOR_ICON_URL_SIZE) == 0)
			return 0;
		iconUrl = trophyUrl;
		sceClibSnprintf(trophyUrl, sizeof(trophyUrl), "%s%s",
			m_utility->GetTrophyIconBaseUrl(), m_utility->GetTrophyIconSilver());
	} else if (sceClibStrncmp(url, GENERATOR_TROPHY_ICON_GOLD, GENERATOR_ICON_URL_SIZE) == 0) {
		if (sceClibStrnlen(m_utility->GetTrophyIconBaseUrl(), GENERATOR_ICON_URL_SIZE) == 0 ||
		    sceClibStrnlen(m_utility->GetTrophyIconGold(), GENERATOR_ICON_URL_SIZE) == 0)
			return 0;
		iconUrl = trophyUrl;
		sceClibSnprintf(trophyUrl, sizeof(trophyUrl), "%s%s",
			m_utility->GetTrophyIconBaseUrl(), m_utility->GetTrophyIconGold());
	} else if (sceClibStrncmp(url, GENERATOR_TROPHY_ICON_HIDDEN, GENERATOR_ICON_URL_SIZE) == 0) {
		if (sceClibStrnlen(m_utility->GetTrophyIconBaseUrl(), GENERATOR_ICON_URL_SIZE) == 0 ||
		    sceClibStrnlen(m_utility->GetTrophyIconHidden(), GENERATOR_ICON_URL_SIZE) == 0)
			return 0;
		iconUrl = trophyUrl;
		sceClibSnprintf(trophyUrl, sizeof(trophyUrl), "%s%s",
			m_utility->GetTrophyIconBaseUrl(), m_utility->GetTrophyIconHidden());
	} else if (sceClibStrncmp(url, GENERATOR_TROPHY_ICON_PLATINUM, GENERATOR_ICON_URL_SIZE) == 0) {
		if (sceClibStrnlen(m_utility->GetTrophyIconBaseUrl(), GENERATOR_ICON_URL_SIZE) == 0 ||
		    sceClibStrnlen(m_utility->GetTrophyIconPlatinum(), GENERATOR_ICON_URL_SIZE) == 0)
			return 0;
		iconUrl = trophyUrl;
		sceClibSnprintf(trophyUrl, sizeof(trophyUrl), "%s%s",
			m_utility->GetTrophyIconBaseUrl(), m_utility->GetTrophyIconPlatinum());
	}
	ret = m_writer.StartElement("icon-url");
	if (ret < 0)
		return ret;
	ret = createEscapedXmlString(iconUrl, GENERATOR_ICON_URL_SIZE, &escaped);
	if (ret < 0) {
		if (escaped != NULL)
			activityFree(escaped);
		return ret;
	}
	ret = m_writer.WriteText(escaped);
	if (escaped != NULL)
		activityFree(escaped);
	if (ret < 0)
		return ret;
	return m_writer.EndElement("icon-url");
}

/* the status's message */
int ComponentXmlGenerator::writeText(const char *text)
{
	char *escaped;
	int ret;

	if ((int)sceClibStrnlen(text, GENERATOR_TEXT_SIZE) >= GENERATOR_TEXT_SIZE)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = m_writer.StartElement("text");
	if (ret < 0)
		return ret;
	ret = createEscapedXmlString(text, GENERATOR_TEXT_SIZE, &escaped);
	if (ret < 0) {
		if (escaped != NULL)
			activityFree(escaped);
		return ret;
	}
	ret = m_writer.WriteTextString(escaped);
	if (escaped != NULL)
		activityFree(escaped);
	if (ret < 0)
		return ret;
	return m_writer.EndElement("text");
}

int ComponentXmlGenerator::writeTitleName(const char *name)
{
	char *escaped;
	int length;
	int ret;

	length = sceClibStrnlen(name, GENERATOR_TITLE_NAME_SIZE);
	if (length == 0)
		return 0;
	if (length >= GENERATOR_TITLE_NAME_SIZE)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = m_writer.StartElement("title-name");
	if (ret < 0)
		return ret;
	ret = createEscapedXmlString(name, GENERATOR_TITLE_NAME_SIZE, &escaped);
	if (ret < 0) {
		if (escaped != NULL)
			activityFree(escaped);
		return ret;
	}
	ret = m_writer.WriteTextString(escaped);
	if (escaped != NULL)
		activityFree(escaped);
	if (ret < 0)
		return ret;
	return m_writer.EndElement("title-name");
}

/* NpActivityStatusInfo::type as the server knows it; 100 is no type to send */
int ComponentXmlGenerator::convertStatusType(int type)
{
	switch (type) {
	case 0:
		return 0;
	case 1:
		return 1;
	case 2:
		return 2;
	case 3:
		return 3;
	case 4:
		return 4;
	case NP_ACTIVITY_STATUS_TYPE_USER:
		return NP_ACTIVITY_STATUS_TYPE_USER;
	case 99:
		return 99;
	case 100:
#line 1635
		ACTIVITY_CHECK("convertStatusType");
		return 0;
	default:
		return type;
	}
}

/* <titleid>, at most 9 characters */
inline int ComponentXmlGenerator::writeTitleId(const char *titleId)
{
	char text[NP_ACTIVITY_TITLE_ID_SIZE];
	int ret;

	ret = m_writer.StartElement("titleid");
	if (ret < 0)
		return ret;
	sceClibMemset(text, 0, sizeof(text));
	sceClibMemcpy_safe(text, titleId, sizeof(text));
	text[sizeof(text) - 1] = '\0';
	ret = m_writer.WriteText(text);
	if (ret < 0)
		return ret;
	return m_writer.EndElement("titleid");
}

inline int ComponentXmlGenerator::writeContentId(const char *contentId)
{
	int length;
	int ret;

	length = sceClibStrnlen(contentId, GENERATOR_CONTENT_ID_SIZE);
	if (length == 0)
		return 0;
	if (length >= GENERATOR_CONTENT_ID_SIZE)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = m_writer.StartElement("contentid");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteText(contentId);
	if (ret < 0)
		return ret;
	return m_writer.EndElement("contentid");
}

/* <info> of a comment: as writeInfo */
int ComponentXmlGenerator::writeCommentInfo(const SceUInt8 *info, SceSize size)
{
	char *text;
	int ret;

	if (size == 0)
		return 0;
	if (size > GENERATOR_INFO_SIZE_MAX)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = m_writer.StartElement("info");
	if (ret < 0)
		return ret;
	text = (char *)activityMalloc(GENERATOR_INFO_TEXT_SIZE);
	if (text == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	sceClibMemset(text, 0, GENERATOR_INFO_TEXT_SIZE);
	ret = m_utility->Base64Encode(info, size, text, GENERATOR_INFO_TEXT_SIZE);
	if (ret < 0) {
		activityFree(text);
		return ret;
	}
	ret = m_writer.WriteText(text);
	activityFree(text);
	if (ret < 0)
		return ret;
	return m_writer.EndElement("info");
}

/* the comment's message */
int ComponentXmlGenerator::writeCommentText(const char *text)
{
	char *escaped;
	int ret;

	if ((int)sceClibStrnlen(text, GENERATOR_COMMENT_TEXT_SIZE) >= GENERATOR_COMMENT_TEXT_SIZE)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = m_writer.StartElement("text");
	if (ret < 0)
		return ret;
	ret = createEscapedXmlString(text, GENERATOR_COMMENT_TEXT_SIZE, &escaped);
	if (ret < 0) {
		if (escaped != NULL)
			activityFree(escaped);
		return ret;
	}
	ret = m_writer.WriteTextString(escaped);
	if (escaped != NULL)
		activityFree(escaped);
	if (ret < 0)
		return ret;
	return m_writer.EndElement("text");
}

/* <ownerid type=>: a user's JID or a title's ID */
int ComponentXmlGenerator::writeOwnerId(const NpActivityOwner *owner)
{
	int ret;

	ret = m_writer.StartElement("ownerid");
	if (ret < 0)
		return ret;
	ret = m_writer.WriteAttributeInt("type", owner->type);
	if (ret < 0)
		return ret;
	switch (owner->type) {
	case NP_ACTIVITY_OWNER_USER: {
		char jid[GENERATOR_JID_SIZE];

		sceClibMemset(jid, 0, sizeof(jid));
		ret = m_utility->ConvertNpIdToJid(&owner->npId, jid, sizeof(jid));
		if (ret >= 0)
			ret = m_writer.WriteText(jid);
		break;
	}
	case NP_ACTIVITY_OWNER_TITLE: {
		char titleId[NP_ACTIVITY_TITLE_ID_SIZE];

		sceClibMemset(titleId, 0, sizeof(titleId));
		sceClibMemcpy_safe(titleId, owner->titleId, sizeof(titleId));
		titleId[sizeof(titleId) - 1] = '\0';
		ret = m_writer.WriteText(titleId);
		break;
	}
	default:
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	}
	if (ret < 0)
		return ret;
	return m_writer.EndElement("ownerid");
}

/* a copy of text, at most max bytes, with XML's special characters escaped; the caller frees it */
inline int ComponentXmlGenerator::createEscapedXmlString(const char *text, SceSize max,
	char **escaped)
{
	SceSize length;
	SceSize size;
	char *buffer;
	int ret;

	*escaped = NULL;
	size = 0;
	if (text == NULL) {
#line 1658
		ACTIVITY_CHECK("createEscapedXmlString");
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	}
	length = sceClibStrnlen(text, max);
	if (length == 0) {
		buffer = (char *)activityMalloc(1);
		if (buffer != NULL)
			buffer[0] = '\0';
		*escaped = buffer;
		return 0;
	}
	ret = SceNpCommon_C2BF5C1F(NULL, 0, (const SceUInt8 *)text, length, &size);
	if (ret < 0)
		return ret;
	buffer = (char *)activityMalloc(size);
	if (buffer == NULL)
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	sceClibMemset(buffer, 0, size);
	ret = SceNpCommon_C2BF5C1F(buffer, size, (const SceUInt8 *)text, length, NULL);
	if (ret < 0) {
		activityFree(buffer);
		return ret;
	}
	*escaped = buffer;
	return 0;
}

/* a status's creation time of 0: none given */
inline int ComponentXmlGenerator::isInvalidCreateTime(const SceUInt64 *time)
{
#line 1707
	ACTIVITY_ASSERT(time != NULL, "isInvalidCreateTime");
	return *time == 0;
}
