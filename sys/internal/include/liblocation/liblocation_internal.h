/*
 * SceLibLocation's exports outside the SDK that other modules call (vs0:sys/external/liblocation:
 * vsh/app/shell/src/liblocation/src/location_client). No record names them.
 */
#ifndef LIBLOCATION_LIBLOCATION_INTERNAL_H
#define LIBLOCATION_LIBLOCATION_INTERNAL_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* a value of the location service's own, read over a connection of its own: SceMsgDialogPlugin
   reads its bits 2 and 4 to pick its location messages */
SceInt32 SceLibLocation_C1CFEC92(SceUInt32 *value);

#ifdef __cplusplus
}
#endif

#endif
