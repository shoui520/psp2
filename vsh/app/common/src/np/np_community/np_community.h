/*
 * The NP community helper the common dialogs and several applications link: it starts
 * SceNpUtility's NP community once for the process and stops it. Nothing names the helper or
 * its file: every name here is ours.
 */
#ifndef NP_COMMUNITY_H
#define NP_COMMUNITY_H

#include <scetypes.h>
#include <np/np_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/* how the community gets its memory */
#define NP_COMMUNITY_INIT_DEFAULT	0
#define NP_COMMUNITY_INIT_ALLOCATOR	1
#define NP_COMMUNITY_INIT_PARAM		2	/* an mspace on the caller's memory */

int npCommunityInit(SceUInt32 type, void *(*alloc)(SceSize size), void (*free)(void *ptr),
		    SceSize size, void *memblock);
int npCommunityInitWithAllocator(void *(*alloc)(SceSize size), void (*free)(void *ptr));
int npCommunityTerm(void);

/* the community's title contexts and requests, and the profiles of a list of users */
int npCommunityCreateTitleCtx(const SceNpCommunicationId *commId, const SceNpId *selfNpId);
int npCommunityDeleteTitleCtx(SceInt32 titleCtxId);
int npCommunityCreateRequest(SceInt32 titleCtxId);
int npCommunityDeleteRequest(SceInt32 reqId);
int npCommunityGetUserProfiles(SceInt32 reqId, SceInt32 avatarSizeType, SceInt32 panelSizeType,
			       const SceNpId *npIds, SceUInt32 count,
			       struct NpLookupProfile *profiles, SceInt32 *results);

/* a title's metadata, read from its title-info XML on the TMDB server */
#define NP_COMMUNITY_TITLE_ID_SIZE	13
#define NP_COMMUNITY_TITLE_NAME_SIZE	129
#define NP_COMMUNITY_TITLE_SNAME_SIZE	52
#define NP_COMMUNITY_TITLE_ICON_SIZE	1025
#define NP_COMMUNITY_CONTENT_ID_SIZE	48

typedef struct NpCommunityTitleInfo {
	int rev;					/* 0x000: the title-info's rev */
	char titleId[NP_COMMUNITY_TITLE_ID_SIZE];	/* 0x004: "<id>_<ver>" */
	char name[NP_COMMUNITY_TITLE_NAME_SIZE];	/* 0x011: the name without a lang */
	char sname[NP_COMMUNITY_TITLE_SNAME_SIZE];	/* 0x092 */
	char reserved;
	char iconUrl[NP_COMMUNITY_TITLE_ICON_SIZE];	/* 0x0C7: the 128x128 icon */
	SceInt32 locale;				/* 0x4C8: the lang asked for */
	char localName[NP_COMMUNITY_TITLE_ID_SIZE];	/* 0x4CC: the name in it (13 bytes) */
	char localSname[NP_COMMUNITY_TITLE_SNAME_SIZE];	/* 0x4D9 */
	char reserved2;
	char contentId[NP_COMMUNITY_CONTENT_ID_SIZE];	/* 0x50E */
	char padding[2];
} NpCommunityTitleInfo;

/* a title's metadata (its NP title ID, "NPWR12345_00"), in the locale's lang */
int npCommunityGetTitleInfo(SceInt32 reqId, const char *titleId, SceInt32 locale,
			    NpCommunityTitleInfo *info);

#ifdef __cplusplus
}
#endif

#endif
