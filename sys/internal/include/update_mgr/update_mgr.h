/*
 * The user side of SceSblSsUpdateMgr (update_mgr's SceSblSsUpdateMgr library): the update
 * status a system updater reports, which the system shows on the display.
 *
 * A program reaches each system call through the stub library's wrapper of the same name
 * without the leading underscore (cui_setupper: 0x81015a10 to 0x81015ba8), as it reaches
 * SceVshBridge's. The header's name is ours.
 */
#ifndef UPDATE_MGR_UPDATE_MGR_H
#define UPDATE_MGR_UPDATE_MGR_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the system calls (sceSblUs*ForUser in update_mgr), weak: a system may not have them */
int _sceSblUsInformUpdateStarted(int number1, int number2, const char *str, SceSize len);
int _sceSblUsInformUpdateOngoing(int number1, int number2);
int _sceSblUsInformUpdateFinished(int number, const char *str, SceSize len);
int _sceSblUsSetSwInfoInt(const char *name, SceSize nameLength, SceUInt32 value);
int _sceSblUsSetSwInfoStr(const char *name, SceSize nameLength, const char *value,
			  SceSize valueLength);
int _sceSblUsSetSwInfoBin(const char *name, SceSize nameLength, const void *value,
			  SceSize valueLength);

/* the stub library's wrappers */
int sceSblUsInformUpdateStarted(int number1, int number2, const char *str, SceSize len);
int sceSblUsInformUpdateOngoing(int number1, int number2);
int sceSblUsInformUpdateFinished(int number, const char *str, SceSize len);
int sceSblUsSetSwInfoInt(const char *name, SceSize nameLength, SceUInt32 value);
int sceSblUsSetSwInfoStr(const char *name, SceSize nameLength, const char *value,
			 SceSize valueLength);
int sceSblUsSetSwInfoBin(const char *name, SceSize nameLength, const void *value,
			 SceSize valueLength);

#ifdef __cplusplus
}
#endif

#endif
