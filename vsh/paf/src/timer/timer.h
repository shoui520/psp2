/* Vita layouts used by the callback timer listener. */
#ifndef PAF_TIMER_TIMER_H
#define PAF_TIMER_TIMER_H

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

#define TimerTickUnit ScePafMisc_C437D34C
#define TimerTickInv ScePafMisc_DD506F1E
#define TimerTickInterval ScePafMisc_EB7A8105
extern float TimerTickUnit;
extern float TimerTickInv;
extern float TimerTickInterval;

#define TimerInitTickUnit ScePafMisc_25B066A8
#define TimerSetCurrTickInterval ScePafMisc_8D59045A
#define TimerDoTick ScePafMisc_50253C80
#define TimerDoFunc ScePafMisc_6C15F987
void TimerInitTickUnit(float tick_unit);
void TimerSetCurrTickInterval(float tick_interval);
float TimerDoTick(Timer *self);
float TimerDoFunc(const Timer *self, float rate, int func, float pre,
		 float *param);

#define TimerDoFuncCustomParabolicIn ScePafMisc_CBB88FB0
#define TimerDoFuncCustomParabolicOutIn ScePafMisc_CD7BB14F
#define TimerDoFuncCustomParabolicInOut ScePafMisc_B9E991D5
#define TimerDoFuncCustomParabolicOut ScePafMisc_4C88CADF
float TimerDoFuncCustomParabolicIn(float rate, const float *param);
float TimerDoFuncCustomParabolicOutIn(float rate, const float *param);
float TimerDoFuncCustomParabolicInOut(float rate, const float *param);
float TimerDoFuncCustomParabolicOut(float rate, const float *param);

#define TimerDoFuncLinear ScePafMisc_E872D707
#define TimerDoFuncCustomExpoIn ScePafMisc_E5CC9FFD
#define TimerDoFuncCustomExpoOutIn ScePafMisc_DE7DCA6C
#define TimerDoFuncCustomExpoInOut ScePafMisc_45BF143C
#define TimerDoFuncCustomExpoOut ScePafMisc_2D6089DC
float TimerDoFuncCustomExpoIn(float rate, const float *param);
float TimerDoFuncCustomExpoOutIn(float rate, const float *param);
float TimerDoFuncCustomExpoInOut(float rate, const float *param);
float TimerDoFuncCustomExpoOut(float rate, const float *param);

#define TimerDoFuncExpoIn ScePafMisc_61EA5DF3
#define TimerDoFuncExpoInOut ScePafMisc_DE4015AE
#define TimerDoFuncExpoOut ScePafMisc_12DBDCD0
float TimerDoFuncExpoIn(float rate);
float TimerDoFuncExpoInOut(float rate);
float TimerDoFuncExpoOut(float rate);

#define TimerDoFuncElasticIn ScePafMisc_88204819
#define TimerDoFuncElasticInOut ScePafMisc_E5801EA1
#define TimerDoFuncElasticOut ScePafMisc_19F2E022
float TimerDoFuncElasticIn(float rate);
float TimerDoFuncElasticInOut(float rate);
float TimerDoFuncElasticOut(float rate);

#define TimerDoFuncExponent ScePafMisc_CF30F373
float TimerDoFuncExponentCurve(float rate);
/* Suffixes identify Timer::Func variants, whose method names are unknown. */
float TimerDoFuncExpoOut27(float rate);
float TimerDoFuncExpoOut28(float rate);
float TimerDoFuncExpoOut30(float rate);
float TimerDoFuncExpoInOut41(float rate);
float TimerDoFuncExpoInOut42(float rate);
float TimerDoFuncExpoInOut43(float rate);
float TimerDoFuncExpoInOut44(float rate);
float TimerDoFuncExponent(float rate, float total, float initval,
			 const float *param);

#define TimerCbrt ScePafMisc_51E31BC8
#define TimerDoFuncBezier3d ScePafMisc_18968E8A
float TimerCbrt(float input);
float TimerDoFuncBezier3d(float rate, const float *param);

#define TimerDoFuncBounceIn ScePafMisc_AE6719F6
#define TimerDoFuncBounceInOut ScePafMisc_2DA1C059
#define TimerDoFuncBounceOut ScePafMisc_27705AD9
float TimerDoFuncBounceIn(float rate);
float TimerDoFuncBounceInOut(float rate);
float TimerDoFuncBounceOut(float rate);

#define TimerDoFuncQuintIn ScePafMisc_3DB2BC0D
#define TimerDoFuncQuartIn ScePafMisc_F4849FCC
#define TimerDoFuncCubicIn ScePafMisc_C65A563E
#define TimerDoFuncFaster ScePafMisc_BF0D952A
#define TimerDoFuncQuadIn ScePafMisc_C5D538A2
#define TimerDoFuncSlower ScePafMisc_963A768B
float TimerDoFuncLinear(float rate);
float TimerDoFuncQuintIn(float rate);
float TimerDoFuncQuartIn(float rate);
float TimerDoFuncCubicIn(float rate);
float TimerDoFuncFaster(float rate);
float TimerDoFuncQuadIn(float rate);
float TimerDoFuncSlower(float rate);

#define TimerDoFuncQuintInOut ScePafMisc_66E302B6
#define TimerDoFuncQuartInOut ScePafMisc_2409570B
#define TimerDoFuncCubicInOut ScePafMisc_8C32850A
#define TimerDoFuncQuadInOut ScePafMisc_F15840C3
#define TimerDoFuncQuintOut ScePafMisc_AB4E6BDC
#define TimerDoFuncQuartOut ScePafMisc_6BBC0E11
#define TimerDoFuncCubicOut ScePafMisc_4F703BED
#define TimerDoFuncQuadOut ScePafMisc_F5246747
float TimerDoFuncQuintInOut(float rate);
float TimerDoFuncQuartInOut(float rate);
float TimerDoFuncCubicInOut(float rate);
float TimerDoFuncQuadInOut(float rate);
float TimerDoFuncQuintOut(float rate);
float TimerDoFuncQuartOut(float rate);
float TimerDoFuncCubicOut(float rate);
float TimerDoFuncQuadOut(float rate);

#define TimerDoFuncBackIn ScePafMisc_57034632
#define TimerDoFuncBackInOut ScePafMisc_CD383801
#define TimerDoFuncCirqIn ScePafMisc_4B6B9A60
#define TimerDoFuncCirqInOut ScePafMisc_A9A54EAB
#define TimerDoFuncBackOut ScePafMisc_CDCE62BB
#define TimerDoFuncCirqOut ScePafMisc_DE3BE4C2
float TimerDoFuncBackIn(float rate);
float TimerDoFuncBackInOut(float rate);
float TimerDoFuncCirqIn(float rate);
float TimerDoFuncCirqInOut(float rate);
float TimerDoFuncBackOut(float rate);
float TimerDoFuncCirqOut(float rate);

#define TimerDoFuncSineIn ScePafMisc_48B923C3
#define TimerDoFuncSineInOut ScePafMisc_04DEE7F8
#define TimerDoFuncSineOut ScePafMisc_2607CDE2
#define TimerDoFuncSwing ScePafMisc_CAF6AD0B
#define TimerDoFuncAngular ScePafMisc_466AF8BF
float TimerDoFuncSineIn(float rate);
float TimerDoFuncSineInOut(float rate);
float TimerDoFuncSineOut(float rate);
float TimerDoFuncSwing(float rate);
float TimerDoFuncAngular(float rate);

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
