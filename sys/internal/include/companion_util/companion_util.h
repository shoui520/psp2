/*
 * SceCompanionUtil (vs0:sys/external/libSceCompanionUtil.suprx): the link to a companion device
 * (PS4 Link, Remote Play) whose keyboard types into the system's text fields. The library is not
 * converted yet: what is here is what its callers show of it, and every name is ours (no Sony
 * record, string or SDK header names these exports).
 */
#ifndef COMPANION_UTIL_COMPANION_UTIL_H
#define COMPANION_UTIL_COMPANION_UTIL_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * What the companion tells its client: the event (9: a text field opens for its keyboard, with
 * the text, its flags and its length in data; 16 and 10: it closes; 17: the text the keyboard
 * typed) and its data.
 */
typedef void (*SceCompanionUtilEventHandler)(SceUInt16 event, void *data);

/* the client's handler (NULL to remove it); 0 returned */
int SceCompanionUtil_0634FFD8(SceCompanionUtilEventHandler handler, int arg);
/* the companion asked for the client's state again */
int SceCompanionUtil_52AA755B(void);
/* the text field the companion types into closed, entered (1) or not (0) */
int SceCompanionUtil_18D13C74(int entered);
/*
 * The text field's text changed: the span the IME is composing (its start and length), the
 * caret, the span the text replaced (its start and length) and the text.
 */
int SceCompanionUtil_04BC85DC(int composeStart, int composeLength, SceUInt32 caret,
			      int replaceStart, int replaceLength, const SceWChar16 *text);

#ifdef __cplusplus
}
#endif

#endif
