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
	Timer *(*Destroy)(Timer *self);
	void (*Delete)(Timer *self);
} TimerVTable;

typedef struct TimerClass {
	int offset_to_top;
	const void *type_info;
	TimerVTable methods;
} TimerClass;

#define timerClass ScePafMisc_E05FACA7
extern const TimerClass timerClass;

enum TimerFunc {
	FUNC_LINEAR,
	FUNC_SLOWDOWN,
	FUNC_FASTUP,
	FUNC_ANGULAR,
	FUNC_EXPONENT,
	FUNC_BEZIER_3D,
	FUNC_SWING,
	FUNC_QUAD_IN,
	FUNC_CUBIC_IN,
	FUNC_QUART_IN,
	FUNC_QUINT_IN,
	FUNC_SINE_IN,
	FUNC_EXPO_IN,
	FUNC_EXPO_IN_UNK_13,
	FUNC_EXPO_IN_UNK_14,
	FUNC_EXPO_IN_UNK_15,
	FUNC_EXPO_IN_UNK_16,
	FUNC_CIRQ_IN,
	FUNC_BACK_IN,
	FUNC_ELASTIC_IN,
	FUNC_BOUNCE_IN,
	FUNC_QUAD_OUT,
	FUNC_CUBIC_OUT,
	FUNC_QUART_OUT,
	FUNC_QUINT_OUT,
	FUNC_SINE_OUT,
	FUNC_EXPO_OUT,
	FUNC_EXPO_OUT_UNK_27,
	FUNC_EXPO_OUT_UNK_28,
	FUNC_EXPO_OUT_UNK_29,
	FUNC_EXPO_OUT_UNK_30,
	FUNC_CIRQ_OUT,
	FUNC_BACK_OUT,
	FUNC_ELASTIC_OUT,
	FUNC_BOUNCE_OUT,
	FUNC_QUAD_INOUT,
	FUNC_CUBIC_INOUT,
	FUNC_QUART_INOUT,
	FUNC_QUINT_INOUT,
	FUNC_SINE_INOUT,
	FUNC_EXPO_INOUT,
	FUNC_EXPO_INOUT_UNK_41,
	FUNC_EXPO_INOUT_UNK_42,
	FUNC_EXPO_INOUT_UNK_43,
	FUNC_EXPO_INOUT_UNK_44,
	FUNC_CIRQ_INOUT,
	FUNC_BACK_INOUT,
	FUNC_ELASTIC_INOUT,
	FUNC_BOUNCE_INOUT,
	FUNC_UNK_49,
	FUNC_UNK_50,
	FUNC_UNK_51,
	FUNC_UNK_52,
	FUNC_UNK_53,
	FUNC_UNK_54,
	FUNC_CUSTOM_EXPO_IN,
	FUNC_CUSTOM_EXPO_OUT,
	FUNC_CUSTOM_EXPO_INOUT,
	FUNC_CUSTOM_EXPO_OUTIN,
	FUNC_CUSTOM_PARABOLIC_IN,
	FUNC_CUSTOM_PARABOLIC_OUT,
	FUNC_CUSTOM_PARABOLIC_INOUT,
	FUNC_CUSTOM_PARABOLIC_OUTIN,
	FUNC_CUSTOM_BOUNCE_IN,
	FUNC_CUSTOM_BOUNCE_OUT,
	FUNC_CUSTOM_BOUNCE_INOUT,
	FUNC_CUSTOM_BOUNCE_OUTIN,
	FUNC_CUSTOM_ELASTIC_IN,
	FUNC_CUSTOM_ELASTIC_OUT,
	FUNC_CUSTOM_ELASTIC_INOUT,
	FUNC_CUSTOM_ELASTIC_OUTIN,
	FUNC_CUSTOM_UNK_71,
	FUNC_CUSTOM_UNK_72,
	FUNC_CUSTOM_UNK_73,
	FUNC_CUSTOM_UNK_74,
	FUNC_CUSTOM_UNK_75,
	FUNC_CUSTOM_UNK_76,
	FUNC_CUSTOM_UNK_77,
	FUNC_CUSTOM_UNK_78,
	FUNC_CUSTOM
};

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

#define timer2DClass ScePafMisc_148462FD
extern const TimerClass timer2DClass;
#define Timer2DDestroy ScePafMisc_F8E57331
#define Timer2DDelete ScePafMisc_4A13DFAE
#define Timer2DDestroyBase ScePafMisc_1EC815F8
#define Timer2DSetFuncY ScePafMisc_9DA95A52
Timer *Timer2DDestroy(Timer *base);
void Timer2DDelete(Timer *base);
Timer *Timer2DDestroyBase(Timer *base);
void Timer2DSetFuncY(Timer2D *self, int func, const float *param);

typedef struct Timer3D {
	Timer2D base;
	unsigned int m_func_z : 8;
	unsigned int m_param_z_num : 4;
	float *m_param_z;
	float m_mapped_z;
} Timer3D;

typedef char Timer3DSizeCheck[sizeof(Timer3D) == 52 ? 1 : -1];

#define timer3DClass ScePafMisc_9A199A59
extern const TimerClass timer3DClass;
#define Timer3DDestroy ScePafMisc_B43B1701
#define Timer3DDelete ScePafMisc_0B1A14C2
#define Timer3DDestroyBase ScePafMisc_DB70D425
#define Timer3DSetFuncZ ScePafMisc_8502EEDA
Timer *Timer3DDestroy(Timer *base);
void Timer3DDelete(Timer *base);
Timer *Timer3DDestroyBase(Timer *base);
void Timer3DSetFuncZ(Timer3D *self, int func, const float *param);

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

#define TimerDestroy ScePafMisc_70E604B9
#define TimerDelete ScePafMisc_D29258BB
#define TimerDestroyBase ScePafMisc_A7D9DBA1
Timer *TimerDestroy(Timer *self);
void TimerDelete(Timer *self);
Timer *TimerDestroyBase(Timer *self);

#define TimerInitTickUnit ScePafMisc_25B066A8
#define TimerSetCurrTickInterval ScePafMisc_8D59045A
#define TimerDoTick ScePafMisc_50253C80
#define TimerResetTimer ScePafMisc_B3794389
void TimerResetTimer(Timer *self);
#define TimerCheckBezierParam ScePafMisc_E89DF174
int TimerCheckBezierParam(Timer *self, float *param);
int TimerCheckCustomParam(Timer *self, float *param);
#define TimerInitFuncParam ScePafMisc_C6FAB673
int TimerInitFuncParam(Timer *self, int func, const float *param,
		       float **ret_param, int *ret_param_num);
#define TimerSetFuncX ScePafMisc_9BD302A5
void TimerSetFuncX(Timer *self, int func, const float *param);
#define TimerInitTimer ScePafMisc_06BD5A37
void TimerInitTimer(Timer *self, float total, int func, int repeat,
		    int rep_count, const float *param);
#define TimerGetValueV4 ScePafMisc_A60CFE13
/* The v4 result storage is the hidden first argument in the C++ ABI. */
float *TimerGetValueV4(float ret[4], const Timer *self);
#define TimerDoFunc ScePafMisc_6C15F987
void TimerInitTickUnit(float tick_unit);
void TimerSetCurrTickInterval(float tick_interval);
float TimerDoTick(Timer *self);
float TimerDoFunc(const Timer *self, float rate, int func, float pre,
		 float *param);

#define TimerDoFuncCustomElasticIn ScePafMisc_F9BA6FDC
#define TimerDoFuncCustomElasticOutIn ScePafMisc_184CDC5F
#define TimerDoFuncCustomElasticInOut ScePafMisc_DF101936
#define TimerDoFuncCustomElasticOut ScePafMisc_028CC1E2
float TimerDoFuncCustomElasticIn(float rate, const float *param);
float TimerDoFuncCustomElasticOutIn(float rate, const float *param);
float TimerDoFuncCustomElasticInOut(float rate, const float *param);
float TimerDoFuncCustomElasticOut(float rate, const float *param);

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
#define TimerDoFuncExponent_i ScePafMisc_ABAC8071
float TimerDoFuncExponent_i(float curr, float total, const float *param);
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

float TimerDoFunc49_52(float rate, const float *param);
float TimerDoFunc50_53(float rate, const float *param);
float TimerDoFunc51_54(float rate, const float *param);
float TimerDoFunc71(float rate, const float *param);
float TimerDoFunc72(float rate, const float *param);
float TimerDoFunc73(float rate, const float *param);
float TimerDoFunc74(float rate, const float *param);
float TimerDoFunc75(float rate, const float *param);
float TimerDoFunc76(float rate, const float *param);
float TimerDoFunc77(float rate, const float *param);
float TimerDoFunc78(float rate, const float *param);

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

void ScePafMisc_CE6EB459(TimerListener *self); /* TimerListener::~TimerListener */
ListenerParam *ScePafMisc_B90DBCA6(ListenerParam *self, TimerListener *listener,
				unsigned char interval, void *data1, void *data2);
void ScePafMisc_C565FD29(TimerListenerList *self, const ListenerParam *param);
void ScePafMisc_A4554B25(TimerListenerList *self, TimerListener *listener);

#endif
