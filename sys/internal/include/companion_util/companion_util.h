/*
 * SceCompanionUtil (vs0:sys/external/libSceCompanionUtil.suprx): the link to a PS4 (PS4 Link,
 * Remote Play) whose keyboard types into the system's text fields. Six exports have Sony's names
 * (the messages PS4 Link prints after them, NPXS10013 3.630.011-DEBUGGER); the rest, and the
 * types, are ours.
 */
#ifndef COMPANION_UTIL_COMPANION_UTIL_H
#define COMPANION_UTIL_COMPANION_UTIL_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* sceCompanionUtilInit's parameter */
typedef struct CompanionUtilInitParam {
	void *workMemory;
	SceSize workMemorySize;		/* 512 KiB at least */
	int threadPriority;		/* 0: SCE_KERNEL_DEFAULT_PRIORITY_USER */
	SceSize threadStackSize;	/* 0: 16 KiB */
	int threadCpuAffinityMask;
	void (*systemCallback)(int event);
	SceUInt8 reserved[32];
} CompanionUtilInitParam;

/* a PS4 to connect to (the export takes it as given, the search list gives it) */
typedef struct CompanionUtilTarget {
	SceUInt32 address;
	SceUInt16 port;
	char hostId[13];
	char registKey[4];
	SceUInt8 padding;
	int userCredential;
} CompanionUtilTarget;

/* the host IDs the connect export restricts the search to */
typedef struct CompanionUtilHostIdList {
	const char **hostIds;
	SceUInt16 count;
} CompanionUtilHostIdList;

/*
 * What the companion tells its client: the event (9: a text field opens for its keyboard, with
 * the text, its flags and its length in data; 16 and 10: it closes; 17: the text the keyboard
 * typed) and its data.
 */
typedef void (*SceCompanionUtilEventHandler)(SceUInt16 event, void *data);

int sceCompanionUtilInit(const CompanionUtilInitParam *param);
int sceCompanionUtilTerm(void);
int sceCompanionUtilStop(void);
/* 0: not running, 1: running, 2: stopped */
int SceCompanionUtil_7AB433EC(SceUInt16 *status);
/* a handler (NULL to remove it) in one of the two slots */
int sceCompanionUtilSetCallbackHandlerInternal(SceCompanionUtilEventHandler handler, int index);
int sceCompanionUtilSearchToOrbis(void);
/* the target's address, as the search last found it */
int SceCompanionUtil_53445DBF(CompanionUtilTarget *target);
/* connect for PS4 Link, to the target or to the host IDs listed */
int SceCompanionUtil_2B0EF5F8(const CompanionUtilTarget *target,
			      const CompanionUtilHostIdList *list);
/* connect, waking the target if it is in standby */
int SceCompanionUtil_C93BB8F0(const CompanionUtilTarget *target);
int sceCompanionUtilPowerOnOrbisForRemotePlay(const CompanionUtilTarget *target);
/* ask the companion for the text field's state again */
int SceCompanionUtil_52AA755B(void);
/*
 * The text field's text changed: the span the IME is composing (its start and length), the
 * caret, the span the text replaced (its start and length) and the text.
 */
int SceCompanionUtil_04BC85DC(int composeStart, int composeLength, SceUInt32 caret,
			      int replaceStart, int replaceLength, const SceWChar16 *text);
/* the text field the companion types into closed, entered (1) or not (0) */
int SceCompanionUtil_18D13C74(int entered);
/* packet 24 with two words */
int SceCompanionUtil_CC52810C(int arg0, int arg1);

#ifdef __cplusplus
}
#endif

#endif
