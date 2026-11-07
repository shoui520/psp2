/*
 * ::StateCheck in C++: what a MainThreadCall or an HTTP request asks whether to go on, and
 * sleeps on while it waits (src/common/main_thread_call.h is its C side). No Sony name: the
 * exports are ScePafCommon_F166B292 (its vtable) and ScePafCommon_83FFAFCA (Sleep), and no
 * mangled name reproduces either NID, while SNC ignores asm labels on members. A C++ user binds
 * _ZTV10StateCheck and _ZN10StateCheck5SleepEv to those two at its link.
 */
#ifndef PAF_COMMON_STATE_CHECK_H
#define PAF_COMMON_STATE_CHECK_H

class StateCheck {
public:
	virtual ~StateCheck()
	{
	}

	virtual int IsReady();
	virtual void Sleep();
};

#endif
