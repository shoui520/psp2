/*
 * SceNpTrophy's internal exports (vs0:sys/external/np_trophy.suprx) that the trophy setup
 * dialog's plugin (libcdlg_np_trophy_setup) calls. No Sony name is known for any of them, and the
 * library is not converted yet: what each takes and does is read from that caller. A trophy
 * "session" here is what the dialog opens and closes around its work; a "handle" is the short
 * lived one some calls take beside it (made and destroyed around the one call).
 */
#ifndef NP_TROPHY_NP_TROPHY_H
#define NP_TROPHY_NP_TROPHY_H

#include <scetypes.h>
#include <np/np_trophy.h>

#ifdef __cplusplus
extern "C" {
#endif

/* opens the dialog's session into *session; 0 or a negative error */
int SceNpTrophy_17FB1D9E(SceInt32 *session);
/* aborts what the session is doing (from another thread) */
int SceNpTrophy_FD34F7CF(SceInt32 session);
/* closes the session */
int SceNpTrophy_44E0F73E(SceInt32 session);

/* makes and destroys a handle for one call on an installed title: info is that title's
   information as SceNpTrophy_061DE3F5 read it (NULL is an invalid argument), options 0 */
int SceNpTrophy_C80794CF(SceInt32 *handle, const void *info, SceUInt64 options);
int SceNpTrophy_10371F25(SceInt32 handle);

/* opens the application's trophy title into *title: the dialog passes 0x01230001, 16 and
   zeros for the rest */
int SceNpTrophy_D70DAFA7(SceInt32 session, SceUInt32 arg1, SceUInt32 arg2, SceUInt32 arg3,
			 SceUInt32 arg4, SceUInt32 arg5, SceUInt32 *title);
/* the title's state for the context: 1 to 5 */
int SceNpTrophy_62E37599(SceInt32 session, SceUInt32 title, SceNpTrophyContext context,
			 SceInt32 *status);
/* closes the title */
int SceNpTrophy_97D19AB6(SceUInt32 title);

/* what is installed of the title: 32 bytes, the first word a percentage */
int SceNpTrophy_0AC3CEA8(SceInt32 session, SceUInt32 title, void *state);
/* the installed title's information: 104 bytes, 8 bytes, and its game details */
int SceNpTrophy_061DE3F5(SceInt32 session, SceUInt32 title, void *info, void *extra,
			 SceNpTrophyGameDetails *details);
/* the installed title's icon into buffer (bufferSize bytes); its size into *size */
int SceNpTrophy_2EE76D88(SceInt32 session, SceUInt32 title, SceInt32 handle, void *buffer,
			 SceSize bufferSize, SceSize *size);
/* removes the installed title */
int SceNpTrophy_C338E582(SceInt32 session, SceUInt32 title, SceInt32 handle);

/* installs the title for the context, for each of the states the dialog sets up (1, 2 and 5;
   3; 4) */
int SceNpTrophy_2B0CC831(SceInt32 session, SceUInt32 title, SceNpTrophyContext context,
			 SceInt32 *result);
int SceNpTrophy_3AC04020(SceInt32 session, SceUInt32 title, SceNpTrophyContext context,
			 SceInt32 *result);
int SceNpTrophy_10182F1B(SceInt32 session, SceUInt32 title, SceNpTrophyContext context,
			 SceInt32 *result);

#ifdef __cplusplus
}
#endif

#endif
