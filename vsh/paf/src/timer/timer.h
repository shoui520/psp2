/* Vita layouts used by the callback timer listener. */
#ifndef PAF_TIMER_TIMER_H
#define PAF_TIMER_TIMER_H

typedef struct Timer Timer;

typedef struct TimerVTable {
	void (*Destroy)(Timer *self);
	void (*Delete)(Timer *self);
} TimerVTable;

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

typedef struct TimerListenerList TimerListenerList;

typedef struct ListenerParam {
	TimerListener *listener;
	void *data1;
	void *data2;
	unsigned char interval;
} ListenerParam;

extern TimerListenerList *s_default_list;

void ScePafMisc_06BD5A37(Timer *self, float total, int func, int repeat,
		      int repeatCount, const float *param); /* Timer::InitTimer */
void ScePafMisc_70E604B9(Timer *self); /* Timer::~Timer */
void ScePafMisc_D29258BB(Timer *self); /* deleting Timer::~Timer */
void ScePafMisc_CE6EB459(TimerListener *self); /* TimerListener::~TimerListener */
ListenerParam *ScePafMisc_B90DBCA6(ListenerParam *self, TimerListener *listener,
				unsigned char interval, void *data1, void *data2);
void ScePafMisc_C565FD29(TimerListenerList *self, const ListenerParam *param);
void ScePafMisc_A4554B25(TimerListenerList *self, TimerListener *listener);

#endif
