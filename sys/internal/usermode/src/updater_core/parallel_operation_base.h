/*
 * ParallelOperationBase (parallel_operation_base.cpp): an operation split over two threads.
 * The derived class's mainOperation fills elements taken from the free channel and passes
 * them on; the consumer thread runs each element's Operate and gives it back. The class name
 * is the file's; mainOperation and threadFunc are the 0.995 build's names, the others ours.
 */
#ifndef UPDATER_CORE_PARALLEL_OPERATION_BASE_H
#define UPDATER_CORE_PARALLEL_OPERATION_BASE_H

#include <scetypes.h>
#include "updater_object.h"
#include "producer_consumer_channel.h"

#define PARALLEL_OPERATION_ELEMENT_NUM	2

/* one unit of work the consumer thread runs */
class ParallelOperationElement : public UpdaterObject {
public:
	virtual int Operate(void) = 0;
};

class ParallelOperationBase : public UpdaterObject {
public:
	ParallelOperationBase();
	virtual ~ParallelOperationBase();

protected:
	virtual int CreateElement(ParallelOperationElement **element) = 0;
	virtual int mainOperation(void *arg) = 0;

	int SetUp(void);
	int Execute(void *arg);

	ProducerConsumerChannel m_fullChannel;		/* elements to operate on */
	ProducerConsumerChannel m_freeChannel;		/* elements to fill */

private:
	int consumerOperation(void);
	static SceInt32 threadFunc(SceSize args, void *argp);

	ParallelOperationElement *m_elements[PARALLEL_OPERATION_ELEMENT_NUM];
	int m_consumerResult;
};

#endif
