/*
 * The C view of paf::Timer, TimerListener and TimerListenerList
 * (include/timer/) for the C code still using them: the main thread's call
 * list and the plugin.
 */
#ifndef PAF_TIMER_TIMER_H
#define PAF_TIMER_TIMER_H

#include <stddef.h>

typedef struct Timer Timer;

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

#define TimerInitTimer _ZN3paf5Timer9InitTimerEfiiiPKf
void TimerInitTimer(Timer *self, float total, int func, int repeat,
		    int rep_count, const float *param);

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

#define TimerListenerOnTick _ZN3paf13TimerListener6OnTickEPvS1_
#define TimerListenerDestroyBase _ZN3paf13TimerListenerD2Ev
void TimerListenerOnTick(TimerListener *self, void *data1, void *data2);
TimerListener *TimerListenerDestroyBase(TimerListener *self);

typedef struct TimerListenerList TimerListenerList;

typedef struct ListenerParam {
	TimerListener *listener;
	void *data1;
	void *data2;
	unsigned char interval : 1;
} ListenerParam;

#define s_default_list _ZN3paf17TimerListenerList14s_default_listE
extern TimerListenerList *s_default_list;

#define ListenerParamConstruct _ZN3paf17TimerListenerList13ListenerParamC1EPNS_13TimerListenerEbPvS4_
#define TimerListenerListRegister _ZN3paf17TimerListenerList8RegisterERKNS0_13ListenerParamE
#define TimerListenerListUnregister _ZN3paf17TimerListenerList10UnregisterEPNS_13TimerListenerE
ListenerParam *ListenerParamConstruct(ListenerParam *self, TimerListener *listener,
				      unsigned char interval, void *data1, void *data2);
void TimerListenerListRegister(TimerListenerList *self, const ListenerParam *param);
void TimerListenerListUnregister(TimerListenerList *self, TimerListener *listener);

#endif
