/*
 * Dispatcher: a task is taken from the queue, and copied, with the interrupts off; it
 * runs with them on.
 */

#include <stdio.h>

extern "C" {
#include "cortexm3_macro.h"
}
#include "Dispatcher.h"

Dispatcher::Dispatcher(Task *tasks, u32 *data, u32 count, u32 words)
	: RingBuffer<Task>(tasks, count - 1)
{
	current_ = &tasks[count - 1];
	words_ = words;
	while (count-- != 0) {
		tasks->SetData(data);
		tasks++;
		data += words;
	}
}

bool Dispatcher::Post(Task::Func func, void *arg)
{
	Task *task;

	__SETPRIMASK();
	task = Reserve();
	if (task == 0) {
		printf("Dispatcher full.\n");
		__RESETPRIMASK();
		return false;
	}
	task->Set(func, arg);
	__RESETPRIMASK();
	return true;
}

/* every task queued, those posted meanwhile included */
void Dispatcher::Dispatch()
{
	Task *task;

	for (;;) {
		__SETPRIMASK();
		task = Pop();
		if (task == 0)
			break;
		current_->Copy(task, words_);
		__RESETPRIMASK();
		current_->Run();
	}
	__RESETPRIMASK();
}
