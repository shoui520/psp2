/*
 * The host's commands (0x18a0e..0x1ae88); see wlan/hostcmd.c.
 */
#ifndef ROBIN_HOSTCMD_H
#define ROBIN_HOSTCMD_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"

int hostCmdProcess(void);
int hostCmdRespond(struct WlanConnection *connection, int (*release)(OsaBuffer *buffer));
void hostCmdUnlock(void);
int hostCmdIsNodeSet(const uint8_t *command);
uint16_t hostCmdNodeGet(void);
unsigned int hostCmdCurrentCode(void);

#endif /* ROBIN_HOSTCMD_H */
