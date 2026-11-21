/* the system's sign-in dialog for PSN (SceAppUtilNpSignin_56C91A8E) */
#include <scetypes.h>
#include <appmgr.h>
#include "apputil_private.h"

int npSigninState(void)
{
	return sceAppMgrLaunchAppByUri(APPUTIL_NP_SIGNIN_LAUNCH_MODE, "settings_dlg:signin");
}
