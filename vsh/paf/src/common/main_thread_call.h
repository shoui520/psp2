/* C layout of the recovered main-thread call interface. */
#ifndef PAF_COMMON_MAIN_THREAD_CALL_H
#define PAF_COMMON_MAIN_THREAD_CALL_H

#include "call_list.h"

typedef struct StateCheck StateCheck;

typedef struct StateCheckVTable {
	void (*Destroy)(StateCheck *self);
	void (*Delete)(StateCheck *self);
	int (*IsReady)(StateCheck *self);
	void (*Sleep)(StateCheck *self);
} StateCheckVTable;

struct StateCheck {
	const StateCheckVTable *vtable;
};

typedef struct MainThreadCall {
	StateCheck *m_check;
	Function m_func;
	void *m_userdata;
} MainThreadCall;

typedef char MainThreadCallSizeCheck[sizeof(MainThreadCall) == 12 ? 1 : -1];

/* The reference's mangled names do not reproduce these NIDs. */
#define StateCheckSleep ScePafCommon_83FFAFCA
#define MainThreadCallConstruct ScePafCommon_168104A7
#define MainThreadCallConstructBase ScePafCommon_8D3DC5B5
#define MainThreadCallDestroy ScePafCommon_CDAADD57
#define MainThreadCallDestroyBase ScePafCommon_A7A6F8C0
#define MainThreadCallRun ScePafCommon_AC79C8D3

void StateCheckSleep(StateCheck *self);
MainThreadCall *MainThreadCallConstruct(MainThreadCall *self, StateCheck *check,
				      Function func, void *userdata);
MainThreadCall *MainThreadCallConstructBase(MainThreadCall *self, StateCheck *check,
					  Function func, void *userdata);
MainThreadCall *MainThreadCallDestroy(MainThreadCall *self);
MainThreadCall *MainThreadCallDestroyBase(MainThreadCall *self);
void MainThreadCallRun(void *userdata);

#endif
