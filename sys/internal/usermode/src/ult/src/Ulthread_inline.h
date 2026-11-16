/*
 * Ulthread_inline.h: an ulthread's state changes, each made at once on the whole AtomicState
 * (LDREXD/STREXD).
 */
#ifndef ULTHREAD_INLINE_H
#define ULTHREAD_INLINE_H

#define ULTHREAD_INLINE_ASSERT_EQUAL(line, text, actual, expected) \
	ULT_ASSERT_EQUAL_AT("Ulthread_inline.h", line, text, actual, expected)

#define ATOMIC_STATE_LOAD(as)		((as).asInt = __ldrexd((volatile long long *)&atomicState))
#define ATOMIC_STATE_STORE(as)		(__strexd((as).asInt, (volatile long long *)&atomicState) == 0)

/* initialized or ready to running: SCE_ULT_ERROR_STATE otherwise */
int Ulthread::setRunning()
{
	AtomicState as, next;

	do {
		ATOMIC_STATE_LOAD(as);
		if (as.asStr.state != kInitialized && as.asStr.state != kReady)
			return SCE_ULT_ERROR_STATE;
		next = as;
		next.asStr.state = kRunning;
	} while (!ATOMIC_STATE_STORE(next));
	razorWrite(RAZOR_ULTHREAD_PACKET,
		   as.asStr.state == kInitialized ? RAZOR_ULT_SWITCH_IN : RAZOR_ULT_SWITCH_IN_RESUME,
		   this, NULL);
	return SCE_OK;
}

/* running to ready */
int Ulthread::setReady()
{
	AtomicState as, next;

	do {
		ATOMIC_STATE_LOAD(as);
		if (as.asStr.state != kRunning)
			return SCE_ULT_ERROR_STATE;
		next = as;
		next.asStr.state = kReady;
	} while (!ATOMIC_STATE_STORE(next));
	razorWrite(RAZOR_ULTHREAD_PACKET,
		   as.asStr.state == kWaiting ? RAZOR_ULT_WAKEUP : RAZOR_ULT_SWITCH_OUT, this, NULL);
	return SCE_OK;
}

/* running to exited: the thread waiting for its end is signalled */
int Ulthread::setExit()
{
	AtomicState as, next;

	do {
		ATOMIC_STATE_LOAD(as);
		if (as.asStr.state != kRunning)
			return SCE_ULT_ERROR_STATE;
		next = as;
		next.asStr.state = kExit;
		next.asStr.joiner = AbstractThread::INVALID_ID;
	} while (!ATOMIC_STATE_STORE(next));
	razorWrite(RAZOR_ULTHREAD_PACKET, RAZOR_ULT_SWITCH_OUT_EXIT, this, NULL);
	if (as.asStr.joiner != AbstractThread::INVALID_ID) {
		int ret = AbstractThread::sendSignal(as.asStr.joiner);
		ULTHREAD_INLINE_ASSERT_EQUAL(68, "ret==0", ret, 0);
	}
	return SCE_OK;
}

/*
 * running to waiting: START_WAITING (it waits), or SCE_OK when a signal was sent before (taken,
 * it goes on running)
 */
int Ulthread::setWaiting()
{
	AtomicState as, next;
	int ret;

	do {
		ATOMIC_STATE_LOAD(as);
		if (as.asStr.state != kRunning)
			return SCE_ULT_ERROR_STATE;
		next = as;
		if (as.asStr.signal != 0) {
			next.asStr.signal = 0;
			ret = SCE_OK;
		} else {
			next.asStr.state = kWaiting;
			ret = START_WAITING;
		}
	} while (!ATOMIC_STATE_STORE(next));
	if (ret == START_WAITING)
		razorWrite(RAZOR_ULTHREAD_PACKET, RAZOR_ULT_SWITCH_OUT_WAIT, this, NULL);
	return ret;
}

/* a waiting ulthread is ready again (3); one that is not keeps the signal for its next wait */
int Ulthread::signal()
{
	AtomicState as;
	int ret;

	do {
		ATOMIC_STATE_LOAD(as);
		if (as.asStr.state == kWaiting) {
			ULTHREAD_INLINE_ASSERT_EQUAL(130, "as.asStr.signal==0", as.asStr.signal, 0);
			ret = 3;
			as.asStr.state = kReady;
		} else if (as.asStr.signal != 0) {
			ret = SCE_ULT_ERROR_STATE;
		} else {
			as.asStr.signal = 1;
			ret = SCE_OK;
		}
	} while (!ATOMIC_STATE_STORE(as));
	if (ret == 3)
		razorWrite(RAZOR_ULTHREAD_PACKET, RAZOR_ULT_WAKEUP, this, NULL);
	return ret;
}

/* exited to joined, by the thread that waited for the end */
void Ulthread::setJoined()
{
	ULTHREAD_INLINE_ASSERT_EQUAL(193, "asStr.state==kExit", atomicState.getState(), kExit);
	atomicState.asStr.state = kJoined;
	razorWrite(RAZOR_ULTHREAD_PACKET, RAZOR_ULT_JOIN, this, name);
}

#endif
