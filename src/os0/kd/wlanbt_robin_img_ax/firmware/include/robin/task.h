#ifndef ROBIN_TASK_H
#define ROBIN_TASK_H

#include <stdint.h>
#include "robin/threadx.h"

int osaTaskSuspend(TX_THREAD *task);
int osaTaskResume(TX_THREAD *task);
UINT osaTaskSleep(ULONG ticks);
int osaTaskSetPriority(TX_THREAD *task, UINT priority, uint8_t *previous);
int osaTaskGetCurrent(TX_THREAD **task);
void osaTaskRelinquish(void);

#endif /* ROBIN_TASK_H */
