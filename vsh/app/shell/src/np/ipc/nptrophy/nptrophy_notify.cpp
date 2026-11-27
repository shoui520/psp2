/*
 * nptrophy_notify.cpp: Trophy unlock Activity feed posting and LiveArea
 * NewEvent notification + 64x64 DDS thumbnail builder (0x81229A58..0x8122ACC0).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/stdio.h>
#include <std/string.h>
#include <framework.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <common/char_encoding.h>
#include <common/shared_ptr.h>
#include <image/image.h>
#include <memory/heap_allocator.h>
#include "src/shell_svc/src/shell_svc_newevent.h"
#include "nptrophy_internal.h"

#define NP_TROPHY_DDS_ICON_SIZE		0x1080U
#define NP_TROPHY_ICON_DIM		64U
#define NP_TROPHY_EARNED_MSG_HASH	0x0C1F9F33U
#define NP_TROPHY_SE_ID			16
#define NP_TROPHY_EXEC_MODE_URI		0x00020000

/* Trophy app title ID (NPXS10008) */
static const char s_trophyAppTitleId[] = "NPXS10008";

/* Resolved title ID buffer for Activity posting (0x815AA410) */
static char s_trophyActivityTitleId[16];

/* Lower-layer NP Activity and JID helpers */
struct NpTrophyUserJidBuf {
	char rawJid[39];
	char formattedJid[128];
};

extern int npTrophyResolvePidTitleId(SceUID pid, char *outTitleId);		/* 0x812CB034 */
extern int npTrophyGetLocalNpId(SceNpId *outNpId);				/* 0x812EA0E6 */
extern int npTrophyFormatNpIdJid(const SceNpId *npId, char *outBuf,
				 SceSize outSize, bool stripOpt);		/* 0x812EA0F8 */
extern int npActivityPostTrophyStandard(const char *titleId, const char *gameTitle,
					const char *trophyName, int grade,
					const char *uri);			/* 0x81362108 */
extern int npActivityPostTrophyHidden(const char *titleId, const char *gameTitle,
				      const char *uri);				/* 0x81362148 */
extern int npActivityPostTrophyPlatinum(const char *titleId, const char *gameTitle,
					const char *uri, const char *trophyName); /* 0x81362170 */

extern "C" {
extern int SceLsdb_315B9FD6(NewEventDbEntry *entry, SceUInt8 popupFlag);
}

/* 0x81229AC8: build online trophy URI with URL-encoded JID */
static int npTrophyBuildOnlineUri(const NpTrophyDetails *trophyDetails,
				  const NpTrophyCommId *commId,
				  paf::string *outUri)
{
	SceNpId localNpId;
	char formattedJid[128];
	char commIdStr[13];

	sce_paf_memset(&localNpId, 0, sizeof(localNpId));
	sce_paf_memset(formattedJid, 0, sizeof(formattedJid));
	sce_paf_memset(commIdStr, 0, sizeof(commIdStr));
	int ret = SceNpCommon_E426FEE8(commId, commIdStr, sizeof(commIdStr));
	if (ret < 0)
		return ret;
	ret = npTrophyGetLocalNpId(&localNpId);
	if (ret < 0)
		return ret;
	ret = npTrophyFormatNpIdJid(&localNpId, formattedJid, sizeof(formattedJid), true);
	if (ret < 0)
		return ret;

	char trophyIdStr[64];
	sce_paf_snprintf(trophyIdStr, sizeof(trophyIdStr), "%lld",
			 static_cast<long long>(trophyDetails->trophyId));

	*outUri = "pstc://browse?mode=online&npcommid=";
	*outUri += commIdStr;
	*outUri += "&trophyid=";
	*outUri += trophyIdStr;
	*outUri += "&jid=";
	*outUri += formattedJid;

	size_t atPos = outUri->find("@");
	if (atPos != paf::string::npos)
		outUri->replace(atPos, 1, "%40");

	return 0;
}

/* 0x81229E9C: post Platinum trophy unlock to Activity feed */
static void npTrophyPostActivityPlatinum(const char *titleId, const NpTrophyDetails *trophyDetails,
					 const NpTrophyGameDetails *gameDetails,
					 const NpTrophyCommId *commId)
{
	paf::string uri;
	if (npTrophyBuildOnlineUri(trophyDetails, commId, &uri) >= 0)
		npActivityPostTrophyPlatinum(titleId, gameDetails->title, uri.c_str(),
					     trophyDetails->name);
}

/* 0x81229F2C: post Gold/Silver/Bronze trophy unlock to Activity feed */
static void npTrophyPostActivityStandard(const char *titleId, const NpTrophyDetails *trophyDetails,
					 const NpTrophyGameDetails *gameDetails,
					 const NpTrophyCommId *commId)
{
	paf::string uri;
	if (npTrophyBuildOnlineUri(trophyDetails, commId, &uri) >= 0)
		npActivityPostTrophyStandard(titleId, gameDetails->title, trophyDetails->name,
					     trophyDetails->trophyGrade, uri.c_str());
}

/* 0x81229FBE: post hidden trophy unlock to Activity feed */
static void npTrophyPostActivityHidden(const char *titleId, const NpTrophyDetails *trophyDetails,
				       const NpTrophyGameDetails *gameDetails,
				       const NpTrophyCommId *commId)
{
	paf::string uri;
	if (npTrophyBuildOnlineUri(trophyDetails, commId, &uri) >= 0)
		npActivityPostTrophyHidden(titleId, gameDetails->title, uri.c_str());
}

/* 0x81229A58: dispatch trophy unlock Activity post by grade/hidden state */
void npTrophyPostUnlockActivity(SceUID clientPid, const NpTrophyDetails *trophyDetails,
				const NpTrophyGameDetails *gameDetails,
				const NpTrophyCommId *commId)
{
	int grade = trophyDetails->trophyGrade;
	int hidden = trophyDetails->hidden;

	if (npTrophyResolvePidTitleId(clientPid, s_trophyActivityTitleId) < 0)
		return;

	if (grade == NP_TROPHY_GRADE_PLATINUM)
		npTrophyPostActivityPlatinum(s_trophyActivityTitleId, trophyDetails,
					     gameDetails, commId);
	else if (hidden != 0)
		npTrophyPostActivityHidden(s_trophyActivityTitleId, trophyDetails,
					   gameDetails, commId);
	else
		npTrophyPostActivityStandard(s_trophyActivityTitleId, trophyDetails,
					     gameDetails, commId);
}

/* 0x8122A2D4: fetch localized "You have earned a trophy." text from indicator_plugin */
static paf::string npTrophyGetEarnedText(void)
{
	paf::string result;
	paf::Plugin *plugin = paf::Plugin::Find("indicator_plugin");
	if (plugin != NULL) {
		paf::IDParam msgId(NP_TROPHY_EARNED_MSG_HASH);
		const wchar_t *wideText = plugin->GetString(msgId);
		paf::wstring wideStr(wideText != NULL ? wideText : L"");
		paf::common::Utf16ToUtf8(wideStr, &result);
	}
	return result;
}

/* 0x8122A3F8: map trophy grade to PlayStation private-use Unicode icon glyph */
static wchar_t npTrophyGradeToGlyph(int grade)
{
	switch (grade) {
	case NP_TROPHY_GRADE_PLATINUM:	return static_cast<wchar_t>(0xF8AF);
	case NP_TROPHY_GRADE_GOLD:	return static_cast<wchar_t>(0xF8AE);
	case NP_TROPHY_GRADE_SILVER:	return static_cast<wchar_t>(0xF8AD);
	case NP_TROPHY_GRADE_BRONZE:	return static_cast<wchar_t>(0xF8AC);
	default:			return 0;
	}
}

/* 0x8122A42E: prepend grade glyph to trophy name and encode as UTF-8 */
static paf::string npTrophyFormatNoticeTitle(const NpTrophyDetails *trophyDetails)
{
	paf::string result;
	wchar_t glyph = npTrophyGradeToGlyph(trophyDetails->trophyGrade);
	if (glyph == 0)
		return result;

	paf::wstring prefix(1, glyph);
	paf::string narrowName(trophyDetails->name);
	paf::wstring wideName;
	paf::common::Utf8ToUtf16(narrowName, &wideName, 0);

	paf::wstring combined = prefix + wideName;
	paf::common::Utf16ToUtf8(combined, &result);
	return result;
}

/* 0x8122A5F4: build offline trophy URI for LiveArea NewEvent click action */
static paf::string npTrophyBuildOfflineUri(const NpTrophyDetails *trophyDetails,
					   const NpTrophyCommId *commId)
{
	char commIdStr[16];
	sce_paf_memset(commIdStr, 0, 13);
	if (SceNpCommon_E426FEE8(commId, commIdStr, 13) != 0)
		sce_paf_memset(commIdStr, 0, 13);

	char trophyIdStr[64];
	sce_paf_snprintf(trophyIdStr, sizeof(trophyIdStr), "%lld",
			 static_cast<long long>(trophyDetails->trophyId));

	paf::string uri("pstc://browse?mode=offline&npcommid=");
	uri += commIdStr;
	uri += "&trophyid=";
	uri += trophyIdStr;
	return uri;
}

/* 0x8122A7C4: scale decoded image to 64x64 RGBA8888 */
static paf::common::SharedPtr<paf::image::Image> npTrophyResizeIconToRgba(
	paf::common::SharedPtr<paf::image::Image> image)
{
	paf::image::ImageExtent targetExtent(NP_TROPHY_ICON_DIM, NP_TROPHY_ICON_DIM);
	if (!image->SetDecExtent(targetExtent, paf::image::ImageResizeType_NearestNeighbor))
		return paf::common::SharedPtr<paf::image::Image>();

	if (image->GetMode() == ImageMode_RGBA8888)
		return image;

	return image->ConvertMode(ImageMode_RGBA8888, paf::image::ImageConvertModeType_Default);
}

/* 0x8122A95E: convert icon to 64x64 DXT3 and serialize as DDS */
static int npTrophyEncodeIconDds(paf::common::SharedPtr<paf::image::Image> image,
				 void *outDdsBuf, SceSize outDdsSize)
{
	paf::common::SharedPtr<paf::image::Image> rgba = npTrophyResizeIconToRgba(image);
	if (rgba.get() == NULL)
		return 0;

	paf::common::SharedPtr<paf::image::Image> dxt =
		rgba->ConvertMode(ImageMode_DXT3, paf::image::ImageConvertModeType_Default);
	if (dxt.get() == NULL)
		return 0;

	paf::image::ImageBuffer pixelBuf = dxt->ToBuffer(false);
	if (pixelBuf.buff == NULL)
		return 0;

	paf::image::ImageExtent extent(NP_TROPHY_ICON_DIM, NP_TROPHY_ICON_DIM);
	return paf::image::Image::SaveAsDds(outDdsBuf, outDdsSize, pixelBuf.buff,
					    NP_TROPHY_ICON_DIM, extent, ImageMode_DXT3, 0);
}

/* 0x8122AAE0: open trophy icon file and encode to 64x64 DXT3 DDS buffer */
static int npTrophyLoadIconDds(const NpTrophyCommId *commId, int trophyId,
			       paf::memory::HeapAllocator *decodeHeap,
			       void *outDdsBuf, SceSize outDdsSize)
{
	char iconPath[256];
	sce_paf_memset(iconPath, 0, sizeof(iconPath));
	if (npTrophyCoreGetTrophyIconPath(commId, trophyId, iconPath, sizeof(iconPath)) != 0)
		return 0;

	paf::common::SharedPtr<paf::image::Image> image = paf::image::Image::Open(
		iconPath, decodeHeap, paf::image::ImageFormat_None, 0, 0);
	if (image.get() == NULL)
		return 0;

	return npTrophyEncodeIconDds(image, outDdsBuf, outDdsSize);
}

/* 0x8122AC20: fallback grade thumbnail path when trophy icon cannot be loaded */
static paf::string npTrophyGetFallbackIconPath(int grade)
{
	switch (grade) {
	case NP_TROPHY_GRADE_PLATINUM:
		return paf::string("vs0:vsh/shell/trophy/platinum_thum.gim");
	case NP_TROPHY_GRADE_GOLD:
		return paf::string("vs0:vsh/shell/trophy/gold_thum.gim");
	case NP_TROPHY_GRADE_SILVER:
		return paf::string("vs0:vsh/shell/trophy/silver_thum.gim");
	case NP_TROPHY_GRADE_BRONZE:
		return paf::string("vs0:vsh/shell/trophy/bronze_thum.gim");
	default:
		return paf::string();
	}
}

/* 0x8122A048: post LiveArea NewEvent popup notification for unlocked trophy */
void npTrophyPostUnlockNewEvent(const NpTrophyDetails *trophyDetails,
				const NpTrophyCommId *commId)
{
	NewEventDbEntry entry;
	entry.m_titleId = s_trophyAppTitleId;
	entry.m_itemId = "0";
	entry.m_newFlag = 0;
	entry.m_msgType = 256;
	entry.m_actType = 2;
	entry.m_execTitleId = s_trophyAppTitleId;
	entry.m_title = npTrophyFormatNoticeTitle(trophyDetails);
	entry.m_desc = npTrophyGetEarnedText();
	entry.m_execMode = NP_TROPHY_EXEC_MODE_URI;
	entry.m_execArg = npTrophyBuildOfflineUri(trophyDetails, commId);

	paf::memory::HeapAllocator *decodeHeap = paf::Framework::Instance()->GetDecodeHeap();
	void *ddsBuf = decodeHeap->sce_paf_malloc(NP_TROPHY_DDS_ICON_SIZE);
	if (ddsBuf != NULL) {
		sce_paf_memset(ddsBuf, 0, NP_TROPHY_DDS_ICON_SIZE);
		if (npTrophyLoadIconDds(commId, trophyDetails->trophyId, decodeHeap,
					ddsBuf, NP_TROPHY_DDS_ICON_SIZE) != 0) {
			entry.m_iconData = ddsBuf;
			entry.m_iconDataSize = NP_TROPHY_DDS_ICON_SIZE;
		}
	}
	if (entry.m_iconData == NULL || entry.m_iconDataSize == 0) {
		entry.m_iconData = NULL;
		entry.m_iconDataSize = 0;
		entry.m_iconPath = npTrophyGetFallbackIconPath(trophyDetails->trophyGrade);
	}

	entry.m_seId = NP_TROPHY_SE_ID;
	entry.m_seVolume = 1.0f;
	SceLsdb_315B9FD6(&entry, 1);

	if (ddsBuf != NULL)
		decodeHeap->sce_paf_free(ddsBuf);
}
