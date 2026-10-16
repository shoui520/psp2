/* RAM task adapters around the external ThreadX ROM interface. */
#include "robin/task.h"

int osaTaskSuspend(TX_THREAD *task)
{
	return (uint8_t)tx_thread_suspend(task);
}

int osaTaskResume(TX_THREAD *task)
{
	return (uint8_t)tx_thread_resume(task);
}

UINT osaTaskSleep(ULONG ticks)
{
	if (ticks)
		return tx_thread_sleep(ticks);
	return 0;
}

int osaTaskSetPriority(TX_THREAD *task, UINT priority, uint8_t *previous)
{
	UINT previousPriority;

	tx_thread_priority_change(task, priority, &previousPriority);
	*previous = previousPriority;
	return 0;
}

int osaTaskGetCurrent(TX_THREAD **task)
{
	*task = tx_thread_identify();
	return 0;
}

void osaTaskRelinquish(void)
{
	tx_thread_relinquish();
}

