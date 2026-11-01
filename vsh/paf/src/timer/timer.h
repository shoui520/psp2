/*
 * The C view of paf::Timer (include/timer/timer.h) that the timer listener
 * code, still in C, uses.
 */
#ifndef PAF_TIMER_TIMER_H
#define PAF_TIMER_TIMER_H

#include "../thread/mutex.h"
#include <stddef.h>

typedef struct Timer Timer;

enum TimerState {
	STATE_STANDBY = 0,
	STATE_ACTIVE = 1,
	STATE_FINISHED = 2,
	STATE_HOLD = 3,
	STATE_DISABLED = 4
};

enum TimerRepeat {
	REPEAT_NONE = 0,
	REPEAT_LOOP = 1,
	REPEAT_ROUND = 4,
	REPEAT_RBACK = 12
};

typedef struct TimerVTable {
	Timer *(*Destroy)(Timer *self);
	void (*Delete)(Timer *self);
} TimerVTable;

typedef struct TimerClass {
	int offset_to_top;
	const void *type_info;
	TimerVTable methods;
} TimerClass;

#define timerClass _ZTVN3paf5TimerE
extern const TimerClass timerClass;

struct Timer {
	const TimerVTable *vtable;
	float m_total;
	float m_total_inv;
	float m_curr;
	unsigned int m_repeat_count : 8;
	unsigned int m_repeat : 4;
	unsigned int m_state : 4;
	unsigned int m_first_tick : 1;
	unsigned int m_nonpositive_total : 1;
	unsigned int m_is_2d : 1;
	unsigned int m_is_3d : 1;
	unsigned int m_func_x : 8;
	unsigned int m_param_x_num : 4;
	float *m_param_x;
	float m_mapped_x;
};

typedef char TimerSizeCheck[sizeof(Timer) == 28 ? 1 : -1];

typedef struct Timer2D {
	Timer base;
	unsigned int m_func_y : 8;
	unsigned int m_param_y_num : 4;
	float *m_param_y;
	float m_mapped_y;
} Timer2D;

typedef char Timer2DSizeCheck[sizeof(Timer2D) == 40 ? 1 : -1];

typedef struct Timer3D {
	Timer2D base;
	unsigned int m_func_z : 8;
	unsigned int m_param_z_num : 4;
	float *m_param_z;
	float m_mapped_z;
} Timer3D;

typedef char Timer3DSizeCheck[sizeof(Timer3D) == 52 ? 1 : -1];

typedef struct TimerListener TimerListener;

typedef struct TimerListenerVTable {
	TimerListener *(*Destroy)(TimerListener *self);
	void (*Delete)(TimerListener *self);
	void (*OnTick)(TimerListener *self, void *data1, void *data2);
	void (*OnFinish)(TimerListener *self, void *data1, void *data2);
} TimerListenerVTable;

struct TimerListener {
	const TimerListenerVTable *vtable;
	Timer *m_timer;
	void *m_data;
};

typedef struct TimerListenerClass {
	int offset_to_top;
	const void *type_info;
	TimerListenerVTable methods;
} TimerListenerClass;

#define timerListenerClass ScePafMisc_CBA79C25
#define TimerListenerDestroy ScePafMisc_7731B37D
#define TimerListenerDelete ScePafMisc_A8DFBD8D
#define TimerListenerDestroyBase ScePafMisc_CE6EB459
extern const TimerListenerClass timerListenerClass;
TimerListener *TimerListenerDestroy(TimerListener *self);
void TimerListenerDelete(TimerListener *self);
TimerListener *TimerListenerDestroyBase(TimerListener *self);
void TimerListenerOnTick(TimerListener *self, void *data1, void *data2);
void TimerListenerOnFinish(TimerListener *self, void *data1, void *data2);
#define TimerListenerResetTimer ScePafMisc_1F758B43
void TimerListenerResetTimer(TimerListener *self);
#define Timer2DDoTick _ZN3paf7Timer2D6DoTickEv
#define Timer3DDoTick _ZN3paf7Timer3D6DoTickEv
#define TimerListenerDoTick ScePafMisc_6C0856E5
float Timer2DDoTick(Timer2D *self);
float Timer3DDoTick(Timer3D *self);
float TimerListenerDoTick(TimerListener *self, void *data1, void *data2);

typedef struct TimerListenerList TimerListenerList;

typedef struct ListenerParam {
	TimerListener *listener;
	void *data1;
	void *data2;
	unsigned char interval : 1;
} ListenerParam;

typedef struct ListenerParamNode {
	struct ListenerParamNode *prev;
	struct ListenerParamNode *next;
	ListenerParam val;
} ListenerParamNode;

typedef struct ListenerParamList {
	ListenerParamNode *root_node;
	unsigned int count;
	unsigned char alloc_value;
	unsigned char alloc_node;
} ListenerParamList;

struct TimerListenerList {
	ListenerParamList m_list;
	ListenerParamNode *m_iterator;
	unsigned char m_iterator_updated;
	RMutex m_list_mutex;
};

typedef char ListenerParamNodeSizeCheck[sizeof(ListenerParamNode) == 24 ? 1 : -1];
typedef char TimerListenerListSizeCheck[sizeof(TimerListenerList) == 56 ? 1 : -1];
typedef char TimerListenerListMutexOffsetCheck[offsetof(TimerListenerList, m_list_mutex) == 24 ? 1 : -1];

#define TimerListenerListConstruct ScePafMisc_841F0EFC
#define TimerListenerListConstructBase ScePafMisc_E1666417
#define TimerListenerListUpdate ScePafMisc_07602568
#define TimerListenerListInit ScePafMisc_86997E06
#define TimerListenerListExit ScePafMisc_A0198D81
TimerListenerList *TimerListenerListConstruct(TimerListenerList *self);
TimerListenerList *TimerListenerListConstructBase(TimerListenerList *self);
void TimerListenerListUpdate(TimerListenerList *self);
void TimerListenerListInit(void);
void TimerListenerListExit(void);

#define ListenerParamConstruct ScePafMisc_B90DBCA6
ListenerParam *ListenerParamConstruct(ListenerParam *self, TimerListener *listener,
				      unsigned char interval, void *data1, void *data2);

#define s_default_list ScePafMisc_A55320A6
extern TimerListenerList *s_default_list;

/* paf::Timer, written in C++ (timer.cpp): what the C listener code calls. */
#define TimerDoTick _ZN3paf5Timer6DoTickEv
#define TimerResetTimer _ZN3paf5Timer10ResetTimerEv
#define TimerInitTimer _ZN3paf5Timer9InitTimerEfiiiPKf
float TimerDoTick(Timer *self);
void TimerResetTimer(Timer *self);
void TimerInitTimer(Timer *self, float total, int func, int repeat,
		    int rep_count, const float *param);

void ScePafMisc_C565FD29(TimerListenerList *self, const ListenerParam *param);
void ScePafMisc_A4554B25(TimerListenerList *self, TimerListener *listener);

#endif
