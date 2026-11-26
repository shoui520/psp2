/*
 * np_sns_fb_share.cpp: SceNpSnsFb purchase, rating, and trophy feed attachment
 * builders, JSON string escaping, and session-expired impose notification
 * (0x8133C994..0x8133F63F).
 */
#include "np_sns_fb_internal.h"

#include <libkernel/libkernel.h>
#include <plugin.h>
#include <std/string>
#include <std/string.h>
#include <versatileparam/idparam.h>
#include <common/char_encoding.h>
#include "../../../plugins.h"

/* livearea_plugin localized string resource hashes for Facebook sharing */
#define FB_STR_PURCHASE_GAME_NORMAL		0x0F5733A2u	/*  257373282 */
#define FB_STR_PURCHASE_GAME_PREORDER		0x9553B7B9u	/* -1789676551 */
#define FB_STR_PURCHASE_GAME_RENTAL		0x272A5E4Du	/*  657088461 */
#define FB_STR_PURCHASE_VIDEO_NORMAL		0x93A3F5DDu	/* -1818010419 */
#define FB_STR_PURCHASE_VIDEO_PREORDER		0x9195296Bu	/* -1852522517 */
#define FB_STR_PURCHASE_VIDEO_RENTAL		0x12194989u	/*  303646857 */
#define FB_STR_PURCHASE_COMIC_NORMAL		0x39B8314Bu	/*  968379339 */
#define FB_STR_PURCHASE_COMIC_PREORDER		0xD84AC4F5u	/* -666159179 */
#define FB_STR_PURCHASE_COMIC_RENTAL		0x0B683AA7u	/*  191377255 */

#define FB_STR_RATING_ONE_STAR			0xBDEEBE5Au	/* -1108449350 */
#define FB_STR_RATING_MULTI_STAR		0x657689E5u	/*  1702303781 */

#define FB_STR_TROPHY_HIDDEN_COUNT		0xD89D45DFu	/* -660758561 */
#define FB_STR_TROPHY_GRADE_PLATINUM		0x733A2CE4u	/*  1933134084 */
#define FB_STR_TROPHY_GRADE_GOLD		0x7840A5AEu	/*  2017455662 */
#define FB_STR_TROPHY_GRADE_SILVER		0x9E147851u	/* -1642880623 */
#define FB_STR_TROPHY_GRADE_BRONZE		0xB6395DB0u	/* -1237742992 */
#define FB_STR_TROPHY_SINGLE			0x084C2EC7u	/*  139210631 */
#define FB_STR_TROPHY_SINGLE_HIDDEN_NOTE	0x93A1F126u	/* -1818142682 */
#define FB_STR_TROPHY_SINGLE_PLATINUM		0x07304EA0u	/*  120604128 */
#define FB_STR_TROPHY_MULTI			0xF5F2F2F2u	/* -168631182 */

#define FB_STR_SESSION_EXPIRED_NOTICE		0x048E5B30u	/*  76439568 */

#define FB_TROPHY_CAPTION_MAX_LEN		1000
#define FB_TROPHY_SHARE_MAX_COUNT		10
#define FB_RATING_SCORE_MAX			5

static const char *const kJsonEscapeTable[8] = {
	"\\\"",
	"\\\\",
	"\\/",
	"\\b",
	"\\f",
	"\\n",
	"\\r",
	"\\t"
};

static int lookupLiveareaString(SceUInt32 hash, paf::wstring *outText)
{
	paf::IDParam resId("", hash);
	paf::Plugin *plugin = paf::Plugin::Find("livearea_plugin");
	if (plugin == NULL)
		return NP_SNS_FB_ERR_INTERNAL_ERROR;

	const wchar_t *raw = plugin->GetString(resId);
	if (raw != NULL)
		outText->assign(raw, sce_paf_wcslen(raw));
	else
		outText->assign(L"", 0);
	return 0;
}

static void truncateUtf8WithEllipsis(char *buf, SceSize capacity)
{
	if (capacity <= 6)
		return;

	char *endPos = buf + capacity;
	char *cursor = endPos - 1;
	if (cursor == NULL)
		return;

	if (*cursor == '\0')
		cursor = endPos - 2;

	*cursor = '\0';
	*(cursor - 1) = '\0';
	char *walk = cursor - 2;
	int contCount = 0;
	while (contCount < 3) {
		if ((*walk & 0xC0) != 0x80)
			break;
		contCount++;
		*walk-- = '\0';
	}
	walk[0] = '.';
	walk[1] = '.';
	walk[2] = '.';
}

/* 0x8133CD68 */
static int npSnsFbJsonEscape(const char *src, char *dst, SceSize dstSize, SceSize *outLen)
{
	SceSize written = 0;
	const char *cursor = src;

	while (*cursor != '\0') {
		const char *escaped = NULL;
		switch (*cursor) {
		case '"':
			escaped = kJsonEscapeTable[0];
			break;
		case '\\':
			escaped = kJsonEscapeTable[1];
			break;
		case '/':
			escaped = kJsonEscapeTable[2];
			break;
		case '\b':
			escaped = kJsonEscapeTable[3];
			break;
		case '\f':
			escaped = kJsonEscapeTable[4];
			break;
		case '\n':
			escaped = kJsonEscapeTable[5];
			break;
		case '\r':
			escaped = kJsonEscapeTable[6];
			break;
		case '\t':
			escaped = kJsonEscapeTable[7];
			break;
		default:
			break;
		}

		SceSize stepLen = 1;
		if (escaped != NULL)
			stepLen = sceClibStrnlen(escaped, 0x7FFFFFFFu);

		if (dst != NULL) {
			if (dstSize - written < stepLen + 1)
				return NP_SNS_FB_ERR_OUT_OF_MEMORY;
			if (escaped != NULL)
				sceClibMemcpy(dst + written, escaped, stepLen);
			else
				dst[written] = *cursor;
			dst[written + stepLen] = '\0';
		}

		written += stepLen;
		cursor++;
	}

	if (outLen != NULL)
		*outLen = written;
	return 0;
}

/* 0x8133CE6A */
static int npSnsFbBuildPurchaseAttachment(paf::string *outAttachment, const char *titleName,
					  const char *contentId, SceInt32 isRental,
					  const char *imageUrl, SceInt32 contentType,
					  SceInt32 isPreOrder)
{
	(void)contentId;
	if (titleName == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	SceUInt32 msgHash = 0;
	if (contentType == 0) {
		if (isRental != 0)
			msgHash = FB_STR_PURCHASE_GAME_RENTAL;
		else if (isPreOrder != 0)
			msgHash = FB_STR_PURCHASE_GAME_PREORDER;
		else
			msgHash = FB_STR_PURCHASE_GAME_NORMAL;
	} else if (contentType == 1) {
		if (isRental != 0)
			msgHash = FB_STR_PURCHASE_VIDEO_RENTAL;
		else if (isPreOrder != 0)
			msgHash = FB_STR_PURCHASE_VIDEO_PREORDER;
		else
			msgHash = FB_STR_PURCHASE_VIDEO_NORMAL;
	} else if (contentType == 2) {
		if (isRental != 0)
			msgHash = FB_STR_PURCHASE_COMIC_RENTAL;
		else if (isPreOrder != 0)
			msgHash = FB_STR_PURCHASE_COMIC_PREORDER;
		else
			msgHash = FB_STR_PURCHASE_COMIC_NORMAL;
	} else {
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;
	}

	paf::wstring formatTmpl;
	int ret = lookupLiveareaString(msgHash, &formatTmpl);
	if (ret < 0)
		return ret;

	paf::string titleUtf8(titleName);
	paf::wstring titleWide;
	paf::common::Utf8ToUtf16(titleUtf8, &titleWide, 0);

	paf::wstring formattedWide = shellFormatText(formatTmpl.c_str(), titleWide.c_str());

	char *workBuf = static_cast<char *>(npSnsFbMemAlloc(0x800u));
	if (workBuf == NULL)
		return NP_SNS_FB_ERR_OUT_OF_MEMORY;

	sceClibMemset(workBuf, 0, 0x800u);

	paf::string nameUtf8;
	paf::common::Utf16ToUtf8(formattedWide, &nameUtf8);

	SceSize escapedLen = 0;
	npSnsFbJsonEscape(nameUtf8.c_str(), workBuf, 0x800u, &escapedLen);
	paf::string escapedName(workBuf);

	if (imageUrl != NULL) {
		npSnsFbJsonEscape(imageUrl, workBuf, 0x800u, &escapedLen);
		paf::string escapedImage(workBuf);
		sceClibSnprintf(
			workBuf, 0x800u,
			"{\"name\":\"%s\",\"media\":[{\"href\":\"%s\",\"src\":\"%s\",\"type\":\"image\"}]}",
			escapedName.c_str(), escapedImage.c_str(), escapedImage.c_str());
	} else {
		sceClibSnprintf(workBuf, 0x800u, "{\"name\":\"%s\"}", escapedName.c_str());
	}

	*outAttachment = workBuf;
	npSnsFbMemFree(workBuf);
	return 0;
}

/* 0x8133D3C4 */
static int npSnsFbBuildActionLinks(paf::string *outActionLinks, SceInt32 *outHasLinks,
				   SceInt32 serviceType)
{
	SceInt32 hasLinks = (serviceType == 1) ? 1 : 0;
	*outHasLinks = hasLinks;
	if (!hasLinks)
		return 0;

	char *workBuf = static_cast<char *>(npSnsFbMemAlloc(0x100u));
	if (workBuf == NULL)
		return NP_SNS_FB_ERR_OUT_OF_MEMORY;

	sceClibMemset(workBuf, 0, 0x100u);

	SceSize escapedLen = 0;
	npSnsFbJsonEscape("Video Unlimited", workBuf, 0x100u, &escapedLen);
	paf::string escapedText(workBuf);

	npSnsFbJsonEscape("http://www.sonyentertainmentnetwork.com/video-unlimited", workBuf,
			  0x100u, &escapedLen);
	paf::string escapedHref(workBuf);

	sceClibMemset(workBuf, 0, 0x100u);
	sceClibSnprintf(workBuf, 0xFFu, "[{\"text\":\"%s\",\"href\":\"%s\"}]",
			escapedText.c_str(), escapedHref.c_str());

	*outActionLinks = workBuf;
	npSnsFbMemFree(workBuf);
	return 0;
}

/* 0x8133D508 */
static int npSnsFbBuildRatingAttachment(paf::string *outAttachment, const char *titleName,
					const char *imageUrl, SceUInt32 ratingScore)
{
	if (titleName == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	SceUInt32 msgHash = (ratingScore == 1) ? FB_STR_RATING_ONE_STAR : FB_STR_RATING_MULTI_STAR;
	paf::wstring formatTmpl;
	int ret = lookupLiveareaString(msgHash, &formatTmpl);
	if (ret < 0)
		return ret;

	paf::string titleUtf8(titleName);
	paf::wstring titleWide;
	paf::common::Utf8ToUtf16(titleUtf8, &titleWide, 0);

	paf::wstring formattedWide;
	if (ratingScore == 1) {
		formattedWide = shellFormatText(formatTmpl.c_str(), titleWide.c_str());
	} else {
		paf::wstring scoreWide(L"");
		if (ratingScore <= FB_RATING_SCORE_MAX) {
			char numBuf[8];
			sce_paf_memset(numBuf, 0, sizeof(numBuf));
			sceClibSnprintf(numBuf, sizeof(numBuf), "%d", ratingScore);
			paf::string scoreUtf8(numBuf);
			paf::common::Utf8ToUtf16(scoreUtf8, &scoreWide, 0);
		}
		formattedWide = shellFormatText(formatTmpl.c_str(), titleWide.c_str(),
						scoreWide.c_str());
	}

	char *workBuf = static_cast<char *>(npSnsFbMemAlloc(0x800u));
	if (workBuf == NULL)
		return NP_SNS_FB_ERR_OUT_OF_MEMORY;

	sceClibMemset(workBuf, 0, 0x800u);

	paf::string nameUtf8;
	paf::common::Utf16ToUtf8(formattedWide, &nameUtf8);

	SceSize escapedLen = 0;
	npSnsFbJsonEscape(nameUtf8.c_str(), workBuf, 0x800u, &escapedLen);
	paf::string escapedName(workBuf);

	if (imageUrl != NULL) {
		npSnsFbJsonEscape(imageUrl, workBuf, 0x800u, &escapedLen);
		paf::string escapedImage(workBuf);
		sceClibSnprintf(
			workBuf, 0x800u,
			"{\"name\":\"%s\",\"media\":[{\"href\":\"%s\",\"src\":\"%s\",\"type\":\"image\"}]}",
			escapedName.c_str(), escapedImage.c_str(), escapedImage.c_str());
	} else {
		sceClibSnprintf(workBuf, 0x800u, "{\"name\":\"%s\"}", escapedName.c_str());
	}

	*outAttachment = workBuf;
	npSnsFbMemFree(workBuf);
	return 0;
}

/* 0x8133DAE4 */
static int npSnsFbBuildRatingActionLinks(paf::string *outActionLinks, SceInt32 *outHasLinks,
					 SceInt32 serviceType)
{
	return npSnsFbBuildActionLinks(outActionLinks, outHasLinks, serviceType);
}

/* 0x8133DAEC */
static SceInt32 npSnsFbEstimateTrophyCaptionSize(const NpSnsFbTrophyShareItem *trophies,
						 SceInt32 trophyCount)
{
	SceInt32 totalLen = 0;
	SceInt32 hiddenCount = 0;

	for (SceInt32 idx = 0; idx < trophyCount; idx++) {
		if (trophies[idx].isHidden != 0)
			hiddenCount++;
	}

	if (hiddenCount == 1 && trophyCount == 1)
		return 0;

	if (hiddenCount > 0)
		totalLen = 64;

	for (SceInt32 idx = 0; idx < trophyCount; idx++) {
		if (trophies[idx].isHidden == 0 && trophies[idx].name != NULL) {
			SceSize nameLen = sceClibStrnlen(trophies[idx].name, 0x80u);
			SceSize detailLen = 1;
			if (trophies[idx].detail != NULL)
				detailLen = sceClibStrnlen(trophies[idx].detail, 0x400u);
			totalLen += static_cast<SceInt32>(nameLen + detailLen + 32);
		}
	}

	if (totalLen > FB_TROPHY_CAPTION_MAX_LEN)
		return FB_TROPHY_CAPTION_MAX_LEN;
	return totalLen;
}

/* 0x8133DB64 */
static SceInt32 npSnsFbEstimateTrophyAttachmentSize(const char *titleName, const char *iconUrl,
						    const NpSnsFbTrophyShareItem *trophies,
						    SceInt32 trophyCount)
{
	if (titleName == NULL || iconUrl == NULL || trophies == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	SceSize titleLen = sceClibStrnlen(titleName, 0x80u);
	SceSize iconLen = sceClibStrnlen(iconUrl, 0x400u);
	SceInt32 captionLen = npSnsFbEstimateTrophyCaptionSize(trophies, trophyCount);
	return static_cast<SceInt32>(titleLen + 2 * iconLen + 448) + captionLen;
}

/* 0x8133DBA6 */
static paf::wstring npSnsFbFormatCountWstring(SceUInt32 count)
{
	paf::wstring result(L"");
	if (count <= FB_TROPHY_SHARE_MAX_COUNT) {
		char numBuf[8];
		sce_paf_memset(numBuf, 0, sizeof(numBuf));
		sceClibSnprintf(numBuf, sizeof(numBuf), "%d", count);
		paf::string numUtf8(numBuf);
		paf::common::Utf8ToUtf16(numUtf8, &result, 0);
	}
	return result;
}

/* 0x8133DC86 */
static int npSnsFbBuildTrophyCaption(const NpSnsFbTrophyShareItem *trophies, SceInt32 trophyCount,
				     char *outBuf, SceInt32 outBufSize, SceInt32 *outHasCaption)
{
	*outHasCaption = 0;
	if (outBuf == NULL || outBufSize <= 0)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	SceInt32 hiddenCount = 0;
	for (SceInt32 idx = 0; idx < trophyCount; idx++) {
		if (trophies[idx].isHidden != 0)
			hiddenCount++;
	}

	if (hiddenCount == 1 && trophyCount == 1)
		return 0;

	char *writePos = outBuf;
	SceInt32 remaining = outBufSize;
	int status = 0;

	if (hiddenCount > 0) {
		paf::wstring hiddenTmpl;
		status = lookupLiveareaString(FB_STR_TROPHY_HIDDEN_COUNT, &hiddenTmpl);
		if (status < 0)
			return status;

		paf::wstring countWide = npSnsFbFormatCountWstring(static_cast<SceUInt32>(hiddenCount));
		*outHasCaption = 1;
		paf::wstring formattedHidden = shellFormatText(hiddenTmpl.c_str(), countWide.c_str());

		paf::string hiddenUtf8;
		paf::common::Utf16ToUtf8(formattedHidden, &hiddenUtf8);

		int written = sceClibSnprintf(writePos, static_cast<SceSize>(remaining),
					      "(%s)", hiddenUtf8.c_str());
		if (written < 0)
			return NP_SNS_FB_ERR_OUT_OF_MEMORY;

		remaining -= written;
		writePos += written;
		if (remaining <= 0) {
			truncateUtf8WithEllipsis(outBuf, static_cast<SceSize>(outBufSize));
			return 0;
		}
		status = written;
	}

	for (SceInt32 idx = 0; idx < trophyCount; idx++) {
		if (trophies[idx].isHidden != 0)
			continue;

		SceUInt32 gradeHash = 0;
		switch (trophies[idx].grade) {
		case 1:
			gradeHash = FB_STR_TROPHY_GRADE_PLATINUM;
			break;
		case 2:
			gradeHash = FB_STR_TROPHY_GRADE_GOLD;
			break;
		case 3:
			gradeHash = FB_STR_TROPHY_GRADE_SILVER;
			break;
		case 4:
			gradeHash = FB_STR_TROPHY_GRADE_BRONZE;
			break;
		default:
			continue;
		}

		paf::wstring gradeWide;
		status = lookupLiveareaString(gradeHash, &gradeWide);
		if (status < 0)
			return status;

		if (trophies[idx].name == NULL)
			continue;

		paf::string nameUtf8(trophies[idx].name);
		paf::wstring nameWide;
		paf::common::Utf8ToUtf16(nameUtf8, &nameWide, 0);

		paf::string detailUtf8(trophies[idx].detail != NULL ? trophies[idx].detail : "");
		paf::wstring detailWide;
		paf::common::Utf8ToUtf16(detailUtf8, &detailWide, 0);

		paf::wstring entryWide = shellFormatText(L"%1 (%2)\n%3\n\n", nameWide.c_str(),
							 gradeWide.c_str(), detailWide.c_str());
		*outHasCaption = 1;

		paf::string entryUtf8;
		paf::common::Utf16ToUtf8(entryWide, &entryUtf8);

		int written = sceClibSnprintf(writePos, static_cast<SceSize>(remaining),
					      "%s", entryUtf8.c_str());
		if (written < 0)
			return NP_SNS_FB_ERR_OUT_OF_MEMORY;

		remaining -= written;
		writePos += written;
		if (remaining <= 0) {
			truncateUtf8WithEllipsis(outBuf, static_cast<SceSize>(outBufSize));
			return 0;
		}
		status = written;
	}

	return status;
}

/* 0x8133E6D2 */
static int npSnsFbBuildTrophyAttachment(char *outBuf, SceInt32 outBufSize, const char *titleName,
					const char *iconUrl, const NpSnsFbTrophyShareItem *trophies,
					SceUInt32 trophyCount)
{
	if (trophyCount == 0 || trophyCount > FB_TROPHY_SHARE_MAX_COUNT ||
	    trophies == NULL || titleName == NULL || outBuf == NULL || outBufSize <= 0)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	paf::string sanitizedTitle(titleName);
	paf::string::size_type nlPos = sanitizedTitle.find("\n");
	while (nlPos != paf::string::npos) {
		sanitizedTitle.replace(nlPos, 1, " ");
		nlPos = sanitizedTitle.find("\n");
	}

	paf::wstring titleWide;
	paf::common::Utf8ToUtf16(sanitizedTitle, &titleWide, 0);

	paf::wstring msgTmpl;
	int ret = 0;
	if (trophyCount == 1) {
		if (trophies[0].isHidden != 0) {
			paf::wstring hiddenNote;
			ret = lookupLiveareaString(FB_STR_TROPHY_SINGLE, &msgTmpl);
			if (ret < 0)
				return ret;
			ret = lookupLiveareaString(FB_STR_TROPHY_SINGLE_HIDDEN_NOTE, &hiddenNote);
			if (ret < 0)
				return ret;
			msgTmpl = shellFormatText(L"%1\n(%2)", msgTmpl.c_str(), hiddenNote.c_str());
		} else if (trophies[0].grade == 1) {
			ret = lookupLiveareaString(FB_STR_TROPHY_SINGLE_PLATINUM, &msgTmpl);
			if (ret < 0)
				return ret;
		} else {
			ret = lookupLiveareaString(FB_STR_TROPHY_SINGLE, &msgTmpl);
			if (ret < 0)
				return ret;
		}
	} else {
		ret = lookupLiveareaString(FB_STR_TROPHY_MULTI, &msgTmpl);
		if (ret < 0)
			return ret;
	}

	paf::wstring headlineWide;
	if (trophyCount == 1) {
		headlineWide = shellFormatText(msgTmpl.c_str(), titleWide.c_str());
	} else {
		paf::wstring countWide = npSnsFbFormatCountWstring(trophyCount);
		headlineWide = shellFormatText(msgTmpl.c_str(), titleWide.c_str(), countWide.c_str());
	}

	SceInt32 captionEst = npSnsFbEstimateTrophyCaptionSize(trophies,
							       static_cast<SceInt32>(trophyCount));
	char *captionBuf = NULL;
	char *escapedIconBuf = NULL;
	SceInt32 hasCaption = 0;

	if (captionEst > 0) {
		captionBuf = static_cast<char *>(npSnsFbMemAlloc(static_cast<SceSize>(captionEst + 1)));
		if (captionBuf == NULL)
			return NP_SNS_FB_ERR_OUT_OF_MEMORY;
		sceClibMemset(captionBuf, 0, static_cast<SceSize>(captionEst + 1));
		ret = npSnsFbBuildTrophyCaption(trophies, static_cast<SceInt32>(trophyCount),
						captionBuf, captionEst, &hasCaption);
		if (ret < 0) {
			npSnsFbMemFree(captionBuf);
			return ret;
		}
	}

	char *writePos = outBuf;
	SceInt32 remaining = outBufSize;
	int prefixLen = sceClibSnprintf(writePos, static_cast<SceSize>(remaining), "{\"name\":\"");
	if (prefixLen < 0 || remaining - prefixLen <= 0) {
		if (captionBuf != NULL)
			npSnsFbMemFree(captionBuf);
		return NP_SNS_FB_ERR_OUT_OF_MEMORY;
	}
	writePos += prefixLen;
	remaining -= prefixLen;

	paf::string headlineUtf8;
	paf::common::Utf16ToUtf8(headlineWide, &headlineUtf8);

	SceSize escapedLen = 0;
	ret = npSnsFbJsonEscape(headlineUtf8.c_str(), writePos, static_cast<SceSize>(remaining),
				&escapedLen);
	if (ret < 0 || remaining - static_cast<SceInt32>(escapedLen) <= 0) {
		if (captionBuf != NULL)
			npSnsFbMemFree(captionBuf);
		return (ret < 0) ? ret : NP_SNS_FB_ERR_OUT_OF_MEMORY;
	}
	writePos += escapedLen;
	remaining -= static_cast<SceInt32>(escapedLen);

	if (hasCaption != 0 && captionBuf != NULL) {
		int capKeyLen = sceClibSnprintf(writePos, static_cast<SceSize>(remaining),
						"\",\"caption\":\"");
		if (capKeyLen < 0 || remaining - capKeyLen <= 0) {
			npSnsFbMemFree(captionBuf);
			return NP_SNS_FB_ERR_OUT_OF_MEMORY;
		}
		writePos += capKeyLen;
		remaining -= capKeyLen;

		ret = npSnsFbJsonEscape(captionBuf, writePos, static_cast<SceSize>(remaining),
					&escapedLen);
		if (ret < 0 || remaining - static_cast<SceInt32>(escapedLen) <= 0) {
			npSnsFbMemFree(captionBuf);
			return (ret < 0) ? ret : NP_SNS_FB_ERR_OUT_OF_MEMORY;
		}
		writePos += escapedLen;
		remaining -= static_cast<SceInt32>(escapedLen);
	}

	if (iconUrl != NULL) {
		escapedIconBuf = static_cast<char *>(npSnsFbMemAlloc(0x400u));
		if (escapedIconBuf == NULL) {
			if (captionBuf != NULL)
				npSnsFbMemFree(captionBuf);
			return NP_SNS_FB_ERR_OUT_OF_MEMORY;
		}
		sceClibMemset(escapedIconBuf, 0, 0x400u);
		ret = npSnsFbJsonEscape(iconUrl, escapedIconBuf, 0x400u, &escapedLen);
		if (ret >= 0) {
			if (sceClibSnprintf(
				    writePos, static_cast<SceSize>(remaining),
				    "\",\"media\":[{\"href\":\"%s\",\"src\":\"%s\",\"type\":\"image\"}]}",
				    escapedIconBuf, escapedIconBuf) < 0)
				ret = NP_SNS_FB_ERR_OUT_OF_MEMORY;
			else
				ret = 0;
		}
		npSnsFbMemFree(escapedIconBuf);
	} else {
		if (sceClibSnprintf(writePos, static_cast<SceSize>(remaining), "\"}") < 0)
			ret = NP_SNS_FB_ERR_OUT_OF_MEMORY;
		else
			ret = 0;
	}

	if (captionBuf != NULL)
		npSnsFbMemFree(captionBuf);
	return ret;
}

/* 0x8133F49E */
static int npSnsFbNotifySessionExpired(void)
{
	paf::Plugin *imposePlugin = paf::Plugin::Find("impose_plugin");
	if (imposePlugin == NULL)
		return NP_SNS_FB_ERR_INTERNAL_ERROR;

	ImposePluginInterface *imposeIface =
		static_cast<ImposePluginInterface *>(imposePlugin->GetInterface(1));
	if (imposeIface == NULL)
		return NP_SNS_FB_ERR_INTERNAL_ERROR;

	ImposeNotice notice;
	paf::wstring messageText;
	int ret = lookupLiveareaString(FB_STR_SESSION_EXPIRED_NOTICE, &messageText);
	if (ret >= 0) {
		notice.message = messageText.c_str();
		imposeIface->showNotice(&notice);
	}
	return ret;
}

/* 0x8133C994 */
int npSnsFbSharePurchase(SceInt32 reqId, SceInt32 serviceType, const char *titleName,
			 const char *contentId, SceInt32 isRental, const char *imageUrl,
			 SceInt32 contentType, SceInt32 isPreOrder, SceInt32 *outFbError)
{
	SceInt32 fbError = 0;
	SceInt32 hasActionLinks = 0;
	SceInt32 shareEnabled = 0;
	paf::string attachment;
	paf::string actionLinks;
	int ret = 0;

	if (!npSnsFbUtilIsAccountBound()) {
		ret = NP_SNS_FB_ERR_ACCOUNT_NOT_BOUND;
	} else {
		ret = npSnsFbUtilIsSharePurchaseEnabled(&shareEnabled);
		if (ret >= 0) {
			if (shareEnabled == 0) {
				ret = NP_SNS_FB_ERR_CONFIG_DISABLED;
			} else {
				const char *safeTitle = (titleName != NULL) ? titleName : "";
				ret = npSnsFbBuildPurchaseAttachment(&attachment, safeTitle,
								     contentId, isRental,
								     imageUrl, contentType,
								     isPreOrder);
				if (ret >= 0) {
					ret = npSnsFbBuildActionLinks(&actionLinks, &hasActionLinks,
								      serviceType);
					if (ret >= 0) {
						const char *linksStr = (hasActionLinks == 1)
									       ? actionLinks.c_str()
									       : NULL;
						ret = npSnsFbUtilPostFeed(reqId, NULL,
									  attachment.c_str(),
									  linksStr, NULL, 0,
									  &fbError);
					}
				}
			}
		}
	}

	if (ret < 0 && outFbError != NULL)
		*outFbError = fbError;
	return ret;
}

/* 0x8133CADC */
int npSnsFbShareRating(SceInt32 reqId, SceInt32 serviceType, const char *titleName,
		       const char *imageUrl, SceUInt32 ratingScore, SceInt32 *outFbError)
{
	SceInt32 fbError = 0;
	SceInt32 hasActionLinks = 0;
	SceInt32 shareEnabled = 0;
	paf::string attachment;
	paf::string actionLinks;
	int ret = 0;

	if (!npSnsFbUtilIsAccountBound()) {
		ret = NP_SNS_FB_ERR_ACCOUNT_NOT_BOUND;
	} else {
		ret = npSnsFbUtilIsShareRatingEnabled(&shareEnabled);
		if (ret >= 0) {
			if (shareEnabled == 0) {
				ret = NP_SNS_FB_ERR_CONFIG_DISABLED;
			} else {
				const char *safeTitle = (titleName != NULL) ? titleName : "";
				ret = npSnsFbBuildRatingAttachment(&attachment, safeTitle,
								   imageUrl, ratingScore);
				if (ret >= 0) {
					ret = npSnsFbBuildRatingActionLinks(&actionLinks,
									    &hasActionLinks,
									    serviceType);
					if (ret >= 0) {
						const char *linksStr = (hasActionLinks == 1)
									       ? actionLinks.c_str()
									       : NULL;
						ret = npSnsFbUtilPostFeed(reqId, NULL,
									  attachment.c_str(),
									  linksStr, NULL, 0,
									  &fbError);
					}
				}
			}
		}
	}

	if (ret < 0 && outFbError != NULL)
		*outFbError = fbError;
	return ret;
}

/* 0x8133CC14 */
int npSnsFbShareTrophy(SceInt32 reqId, const char *titleName, const char *iconUrl,
		       const NpSnsFbTrophyShareItem *trophies, SceUInt32 trophyCount,
		       SceInt32 *outFbError)
{
	if (trophyCount == 0 || trophyCount > FB_TROPHY_SHARE_MAX_COUNT || trophies == NULL)
		return NP_SNS_FB_ERR_INVALID_ARGUMENT;

	SceInt32 fbError = 0;
	SceInt32 shareEnabled = 0;
	char *attachmentBuf = NULL;
	int ret = 0;

	if (!npSnsFbUtilIsAccountBound()) {
		ret = NP_SNS_FB_ERR_ACCOUNT_NOT_BOUND;
	} else {
		ret = npSnsFbUtilIsShareTrophyEnabled(&shareEnabled);
		if (ret >= 0) {
			if (shareEnabled == 0) {
				ret = NP_SNS_FB_ERR_CONFIG_DISABLED;
			} else {
				const char *safeTitle = (titleName != NULL) ? titleName : "";
				const char *safeIcon = (iconUrl != NULL) ? iconUrl : "";
				SceInt32 bufSize = npSnsFbEstimateTrophyAttachmentSize(
					safeTitle, safeIcon, trophies,
					static_cast<SceInt32>(trophyCount));
				if (bufSize <= 0)
					return NP_SNS_FB_ERR_INTERNAL_ERROR;

				attachmentBuf = static_cast<char *>(
					npSnsFbMemAlloc(static_cast<SceSize>(bufSize)));
				if (attachmentBuf == NULL)
					return NP_SNS_FB_ERR_OUT_OF_MEMORY;

				sceClibMemset(attachmentBuf, 0, static_cast<SceSize>(bufSize));
				ret = npSnsFbBuildTrophyAttachment(attachmentBuf, bufSize,
								   safeTitle, iconUrl, trophies,
								   trophyCount);
				if (ret >= 0) {
					ret = npSnsFbUtilPostFeed(reqId, NULL, attachmentBuf,
								  NULL, NULL, 0, &fbError);
					if (ret >= 0)
						ret = 0;
				}
			}
		}
	}

	if (outFbError != NULL)
		*outFbError = fbError;
	if (attachmentBuf != NULL)
		npSnsFbMemFree(attachmentBuf);
	if (ret == NP_SNS_FB_ERR_SESSION_INVALID)
		npSnsFbNotifySessionExpired();
	return ret;
}
