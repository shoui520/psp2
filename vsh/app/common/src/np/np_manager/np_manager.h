/*
 * SceNpManager's exports to the system's modules that the SDK does not declare: the account
 * information the NP service keeps for the signed-in user.
 */
#ifndef NP_MANAGER_NP_MANAGER_H
#define NP_MANAGER_NP_MANAGER_H

#include <scetypes.h>
#include <np/np_common.h>

/* how the account's profile panel is drawn (NP's "panelBgc" and "panelUrl") */
#define NP_MANAGER_PANEL_NONE	0	/* the default panel */
#define NP_MANAGER_PANEL_COLOR	1	/* panelBgc */
#define NP_MANAGER_PANEL_IMAGE	2	/* panelBgc and the image at panelUrl */

/* the account information the NP service keeps (sceNpManagerGetCachedParam's source) */
typedef struct NpManagerAccountInfo {
	SceNpId npId;			/* 0x000 */
	SceUInt8 reserved1[20];		/* 0x024 */
	SceNpAvatarUrl avatarUrl;	/* 0x038 */
	SceUInt32 panelBgc;		/* 0x0B8: the panel's colour */
	SceUInt32 panelType;		/* 0x0BC: NP_MANAGER_PANEL_* */
	SceUInt8 reserved2[4];		/* 0x0C0 */
	char panelUrl[128];		/* 0x0C4 */
	SceUInt8 reserved3[4];		/* 0x144 */
} NpManagerAccountInfo;

#ifdef __cplusplus
extern "C" {
#endif

/* the signed-in user's account information */
int SceNpManager_E45E4003(NpManagerAccountInfo *info);

/* a request for the signed-in user's ticket, the NP ID it holds (a SceNpId in buf), and the
   request's release */
int SceNpManagerTicket_37F7F258(void);
int SceNpManagerTicket_877251F3(int reqId, void *buf);
int SceNpManagerTicket_2B813EC7(int reqId);

#ifdef __cplusplus
}
#endif

#endif
