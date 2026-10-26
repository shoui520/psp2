/*
 * Dispatcher: the tasks the main loop runs, queued from anywhere (the interrupts
 * included) and run one by one in the order posted. Each task has a few words of data;
 * the one running is a copy, so its slot in the queue is free while it runs.
 */
#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <string.h>

#include "Callback.h"
#include "RingBuffer.h"

/* a function, its argument and its data */
class Task : public Callback<void (u32 *)> {
public:
	Task() {}

	void Set(Func func, void *arg)
	{
		Callback<void (u32 *)>::Set(func, arg);
	}

	void SetData(u32 *data)
	{
		data_ = data;
	}

	/* the task's function, argument and words of data */
	void Copy(const Task *task, u32 words)
	{
		Callback<void (u32 *)>::Set(task->func_, task->arg_);
		memcpy(data_, task->data_, words * 4);
	}

	void Run()
	{
		(*this)(data_);
	}

private:
	u32 *data_;
};

class Dispatcher : public RingBuffer<Task> {
public:
	Dispatcher(Task *tasks, u32 *data, u32 count, u32 words);
	bool Post(Task::Func func, void *arg);
	void Dispatch();

private:
	Task *current_;		/* the last of the tasks: the copy that runs */
	u32 words_;		/* each task's data */
};

#endif
