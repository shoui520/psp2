/*
 * SceSigninExt (vs0:sys/external/signin_ext.suprx):
 * Internal architecture shared across the SceSigninExt plugin, custom
 * widgets (ListEdge, PlaneShadow), flow scenes, models, SceNpCommerce2
 * tasks, HTTP/image clients, and signup_library UI helpers.
 */
#ifndef SYS_USERMODE_SIGNIN_EXT_INTERNAL_H
#define SYS_USERMODE_SIGNIN_EXT_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/list>
#include <std/string>
#include <std/vector>
#include <buffer/buffer.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <image/image.h>
#include <job/job.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <thread/thread.h>
#include <url/url.h>
#include <versatileparam/idparam.h>
#include <widget/core/event.h>
#include <widget/core/handler.h>
#include <widget/core/widget.h>
#include <widget/factory.h>
#include <widget/w_button_base.h>
#include <widget/w_num_spin.h>
#include <widget/w_plane.h>
#include <widget/w_rich_text.h>
#include <widget/w_scene.h>
#include <widget/w_scroll_view_base.h>
#include <widget/w_text.h>
#include <widget/w_text_box.h>

/* ---- Error codes ---- */

#define SIGNIN_EXT_ERROR_ALREADY_CREATED	(-2136014816)	/* 0x80AF0020 */
#define SIGNIN_EXT_ERROR_BUSY			(-2136014815)	/* 0x80AF0021 */
#define SIGNIN_EXT_ERROR_NOMEM			(-2136014814)	/* 0x80AF0022 */
#define SIGNIN_EXT_ERROR_INVALID_ARG		(-2136014705)	/* 0x80AF008F */
#define SIGNIN_EXT_ERROR_HTTP_STATUS		(-2135994284)	/* 0x80AF5054 */

#define SIGNIN_EXT_ERROR_COMMERCE_BAD_REQUEST	(-2141909226)	/* 0x80550F16 */
#define SIGNIN_EXT_ERROR_COMMERCE_NOT_FOUND	(-2141909101)	/* 0x80550F93 */
#define SIGNIN_EXT_ERROR_COMMERCE_PW_WEAK	(-2141908987)	/* 0x80551005 */
#define SIGNIN_EXT_ERROR_COMMERCE_PW_EXPIRED	(-2141908913)	/* 0x8055104F */
#define SIGNIN_EXT_ERROR_COMMERCE_SESSION_EXPIRED (-2141908909)	/* 0x80551053 */
#define SIGNIN_EXT_ERROR_COMMERCE_RESET_SENT	(-2141908906)	/* 0x80551056 */
#define SIGNIN_EXT_ERROR_NP_ACCOUNT_NOT_MATCH	(-2141908892)	/* 0x80551064 */

#define SIGNIN_EXT_ERROR_PW_INVALID_CHAR	(-2130706431)	/* 0x81000001 */
#define SIGNIN_EXT_ERROR_PW_POLICY		(-2130706430)	/* 0x81000002 */
#define SIGNIN_EXT_ERROR_PW_MISMATCH		(-2130706429)	/* 0x81000003 */
#define SIGNIN_EXT_ERROR_PW_SAME_AS_OLD		(-2130706428)	/* 0x81000004 */
#define SIGNIN_EXT_ERROR_ACCOUNT_NOT_FOUND	(-2130706426)	/* 0x81000006 */

/* Two-character lowercase ASCII country code packed in little-endian uint32_t */
#define COUNTRY_CODE(c0, c1)			((uint32_t)(uint8_t)(c0) | ((uint32_t)(uint8_t)(c1) << 8))

/* ---- Resource ID hashes (signin_ext_plugin.rco) ---- */

#define RCO_PAGE_RESET_PW_DONE			0x91AFCC75U
#define RCO_PAGE_EULA				0xC92199E8U
#define RCO_PAGE_COMPLETE_NOTICE		0x0B081F1AU
#define RCO_PAGE_SESSION_EXPIRED		0x91AE1CE5U
#define RCO_PAGE_CHANGE_PW			0xC9DBCC76U
#define RCO_PAGE_VERIFY_DOB			0x8B6478E9U
#define RCO_PAGE_BUSY_CANCELABLE		0x2E6BF45EU
#define RCO_PAGE_BUSY				0xE6DE2C60U
#define RCO_PAGE_POPUP_INFO			0xB25B02A7U
#define RCO_PAGE_ERROR_WITH_TITLE		0xD69249FCU
#define RCO_PAGE_ERROR_NO_TITLE			0x70160E65U
#define RCO_PAGE_DOB_DIALOG			0x3AD8DBD1U
#define RCO_PAGE_INFO_DIALOG			0x255E0D6CU
#define RCO_PAGE_LIST_DIALOG			0xDA85BB8DU

#define RCO_DIALOG_PANEL			0x1405DF66U
#define RCO_DIALOG_CONTENT			0x514BC8E5U
#define RCO_DIALOG_SCROLL			0xC8E58A1FU
#define RCO_DIALOG_SCROLLBAR			0xD2C8F0C2U
#define RCO_DIALOG_TITLE			0x3C6DE1B7U
#define RCO_DIALOG_BUSY_INDICATOR		0xE18AC039U
#define RCO_DIALOG_POPUP_CONTAINER		0xEE1E6BA9U
#define RCO_DIALOG_POPUP_TEXT			0x4295D144U
#define RCO_BTN_POPUP_ACTION			0x7B7F1E78U
#define RCO_BTN_OK				0xCB4F8CA9U
#define RCO_BTN_CANCEL				0x28BE1C5CU
#define RCO_BTN_CLOSE				0xE42F94EEU
#define RCO_BTN_ERROR_OK			0xCB4F8CA9U
#define RCO_LIST_VIEW				0x38B62BE4U
#define RCO_LIST_ITEM_ROOT			0x63678FEEU
#define RCO_LIST_ITEM_BTN			0x68EF628AU

#define RCO_DOB_SPIN_BOX			0xBD0425DDU
#define RCO_DOB_SPIN_1				0x80099D52U
#define RCO_DOB_SLASH_1				0x5A83A724U
#define RCO_DOB_SPIN_2				0xECA4B275U
#define RCO_DOB_SLASH_2				0x81F08705U
#define RCO_DOB_SPIN_3				0x97911E76U

#define RCO_TMPL_LIST_ITEM			0x1124B9A6U
#define RCO_TMPL_CHECKMARK			0x6C9A7860U
#define RCO_STYLE_DIALOG_SHADOW			0x7D361522U
#define RCO_STYLE_LIST_EDGE_STD			0x2F02B8C7U
#define RCO_STYLE_LIST_EDGE_NO_TITLE		0x029DD057U

#define RCO_STR_YEAR_PLACEHOLDER		0x8622F99BU
#define RCO_STR_MONTH_PLACEHOLDER		0x5C0F43EEU
#define RCO_STR_DAY_PLACEHOLDER			0xECDD4854U

/* ---- SceNpCommerce2 structs & private imports ---- */

struct SceNpCommerce2SessionResult {
	uint8_t opaque[112];
};

struct SceNpCommerce2AccountInfo {
	uint8_t header[96];
	uint64_t accountId;
	const char *onlineId;
	const char *countryCode;
	const char *languageCode;
	uint32_t reserved;
	const char *signInId;
	uint8_t tail[28];
};

struct SceNpCommerce2CountryList {
	uint8_t header[92];
	uint32_t count;
};

struct SceNpCommerce2CountryInfo {
	uint8_t header[92];
	const char *countryCode;
	uint32_t dateFormat;
	uint32_t langCount;
	int32_t flag;
};

struct SceNpCommerce2ErrorList {
	uint8_t header[92];
	uint32_t count;
};

struct SceNpCommerce2ErrorItem {
	uint8_t header[96];
	int32_t code;
};

extern "C" {
int32_t SceNpCommerce2_25C58D33(void);
int32_t SceNpCommerce2_AA090F5E(uint32_t version, uint32_t *ctxId);
int32_t SceNpCommerce2_1EC16D19(uint32_t ctxId);
int32_t SceNpCommerce2_8ECAE666(uint32_t ctxId, int32_t env);
int32_t SceNpCommerce2_34CDA223(int32_t mode, const wchar_t *password, uint32_t *flags);
int32_t SceNpCommerce2_749C8571(uint32_t ctxId, uint32_t *reqId);
int32_t SceNpCommerce2_D09A38DA(uint32_t reqId);
int32_t SceNpCommerce2_45ACE466(uint32_t reqId, void *buf, SceSize bufSize, SceSize *recvSize);
int32_t SceNpCommerce2_86405E79(SceNpCommerce2CountryList *list, const void *buf, SceSize size);
int32_t SceNpCommerce2_4BE534B6(const SceNpCommerce2CountryList *list, uint32_t index,
				SceNpCommerce2CountryInfo *info);
int32_t SceNpCommerce2_BDECB406(const SceNpCommerce2CountryInfo *info, uint32_t index,
				const char **langCode);
int32_t SceNpCommerce2_B75D54D3(uint32_t ctxId, uint32_t *reqId);
int32_t SceNpCommerce2_FE0EB375(uint32_t reqId, int32_t flag, const char *id,
				const char *pw, int32_t opt);
int32_t SceNpCommerce2_4A36892A(uint32_t reqId, void *buf, SceSize bufSize, SceSize *recvSize);
int32_t SceNpCommerce2_115DF6B1(SceNpCommerce2SessionResult *res, const void *buf, SceSize size);
int32_t SceNpCommerce2_B37493FA(const SceNpCommerce2SessionResult *res,
				SceNpCommerce2AccountInfo *info);
int32_t SceNpCommerce2_76DA0D80(uint32_t ctxId, uint32_t *reqId);
int32_t SceNpCommerce2_F50D7BE4(uint32_t reqId, int32_t opt);
int32_t SceNpCommerce2_F40D2B1E(uint32_t reqId, void *buf, SceSize bufSize, SceSize *recvSize);
int32_t SceNpCommerce2_E1C27906(uint32_t ctxId, uint32_t *reqId);
int32_t SceNpCommerce2_1B229032(uint32_t reqId, const char *id,
				int32_t year, int32_t month, int32_t day);
int32_t SceNpCommerce2_2A22BEAF(uint32_t reqId, void *buf, SceSize bufSize, SceSize *recvSize);
int32_t SceNpCommerce2_4615377F(uint32_t ctxId, uint32_t *reqId);
int32_t SceNpCommerce2_536DA1DA(uint32_t reqId);
int32_t SceNpCommerce2_2AD3766D(uint32_t reqId, void *buf, SceSize bufSize, SceSize *recvSize);
int32_t SceNpCommerce2_F4DD3FD8(uint32_t ctxId, uint32_t *reqId);
int32_t SceNpCommerce2_A4C52416(uint32_t reqId, const char *newPw, int32_t opt);
int32_t SceNpCommerce2_2FC09E2B(uint32_t reqId, void *buf, SceSize bufSize, SceSize *recvSize);
int32_t SceNpCommerce2_8F24F115(SceNpCommerce2ErrorList *list, const void *buf, SceSize size);
int32_t SceNpCommerce2_1B0B5798(const SceNpCommerce2ErrorList *list, uint32_t index,
				SceNpCommerce2ErrorItem *item);
int32_t SceNpCommerce2_9FD5705B(uint32_t ctxId, uint32_t *reqId);
int32_t SceNpCommerce2_B7552026(uint32_t reqId, const char *id);
int32_t SceNpCommerce2_FFFC0101(uint32_t reqId, void *buf, SceSize bufSize, SceSize *recvSize);
int32_t SceNpCommerce2_C737A929(uint32_t ctxId, uint32_t *reqId);
int32_t SceNpCommerce2_2DC2A0F9(uint32_t reqId);
int32_t SceNpCommerce2_7E3C9196(uint32_t reqId, void *buf, SceSize bufSize, SceSize *recvSize);

int32_t SceNpManager_455E05C1(uint64_t accountId, const char *id, const char *pw);
int32_t ScePafWidget_42C5A4D4(paf::ui::Widget *widget, int32_t a1, int32_t a2);
}

/* ---- Generic callback & event listener templates ---- */

class Callback0 {
public:
	virtual ~Callback0() {}
	virtual void Invoke() = 0;
};

template <typename A1>
class Callback1 {
public:
	virtual ~Callback1() {}
	virtual void Invoke(A1 a1) = 0;
};

template <typename A1, typename A2>
class Callback2 {
public:
	virtual ~Callback2() {}
	virtual void Invoke(A1 a1, A2 a2) = 0;
};

template <typename A1, typename A2, typename A3>
class Callback3 {
public:
	virtual ~Callback3() {}
	virtual void Invoke(A1 a1, A2 a2, A3 a3) = 0;
};

template <typename R, typename A1, typename A2, typename A3>
class FuncCallback3 {
public:
	virtual ~FuncCallback3() {}
	virtual R Invoke(A1 a1, A2 a2, A3 a3) = 0;
};

template <typename Obj, typename Arg = void *>
class MemberCallback0 : public Callback0 {
public:
	typedef void (Obj::*Method)(Arg);

	MemberCallback0(Obj *obj, Method method, Arg arg = (Arg)0)
		: m_obj(obj), m_method(method), m_arg(arg)
	{
	}

	virtual ~MemberCallback0() {}

	virtual void Invoke()
	{
		if (m_obj != NULL && m_method != NULL)
			(m_obj->*m_method)(m_arg);
	}

	Obj *m_obj;
	Method m_method;
	Arg m_arg;
};

template <typename Obj, typename A1>
class VirtualMemberCallback1 : public Callback1<A1> {
public:
	typedef void (Obj::*Method)(A1);

	VirtualMemberCallback1(Obj *obj, Method method)
		: m_obj(obj), m_method(method)
	{
	}

	virtual ~VirtualMemberCallback1() {}

	virtual void Invoke(A1 a1)
	{
		if (m_obj != NULL && m_method != NULL)
			(m_obj->*m_method)(a1);
	}

	Obj *m_obj;
	Method m_method;
};

template <typename Obj, typename A1, typename Arg = void *>
class MemberCallback1 : public Callback1<A1> {
public:
	typedef void (Obj::*Method)(A1, Arg);

	MemberCallback1(Obj *obj, Method method, Arg arg = (Arg)0)
		: m_obj(obj), m_method(method), m_arg(arg)
	{
	}

	virtual ~MemberCallback1() {}

	virtual void Invoke(A1 a1)
	{
		if (m_obj != NULL && m_method != NULL)
			(m_obj->*m_method)(a1, m_arg);
	}

	Obj *m_obj;
	Method m_method;
	Arg m_arg;
};

template <typename Obj, typename A1, typename A2, typename Arg = void *>
class MemberCallback2 : public Callback2<A1, A2> {
public:
	typedef void (Obj::*Method)(A1, A2, Arg);

	MemberCallback2(Obj *obj, Method method, Arg arg = (Arg)0)
		: m_obj(obj), m_method(method), m_arg(arg)
	{
	}

	virtual ~MemberCallback2() {}

	virtual void Invoke(A1 a1, A2 a2)
	{
		if (m_obj != NULL && m_method != NULL)
			(m_obj->*m_method)(a1, a2, m_arg);
	}

	Obj *m_obj;
	Method m_method;
	Arg m_arg;
};

template <typename Obj, typename A1, typename A2, typename A3, typename Arg = void *>
class MemberCallback3 : public Callback3<A1, A2, A3> {
public:
	typedef void (Obj::*Method)(A1, A2, A3, Arg);

	MemberCallback3(Obj *obj, Method method, Arg arg = (Arg)0)
		: m_obj(obj), m_method(method), m_arg(arg)
	{
	}

	virtual ~MemberCallback3() {}

	virtual void Invoke(A1 a1, A2 a2, A3 a3)
	{
		if (m_obj != NULL && m_method != NULL)
			(m_obj->*m_method)(a1, a2, a3, m_arg);
	}

	Obj *m_obj;
	Method m_method;
	Arg m_arg;
};

template <typename Obj, typename R, typename A1, typename A2, typename A3, typename Arg = void *>
class MemberFuncCallback3 : public FuncCallback3<R, A1, A2, A3> {
public:
	typedef R (Obj::*Method)(A1, A2, A3, Arg);

	MemberFuncCallback3(Obj *obj, Method method, Arg arg = (Arg)0)
		: m_obj(obj), m_method(method), m_arg(arg)
	{
	}

	virtual ~MemberFuncCallback3() {}

	virtual R Invoke(A1 a1, A2 a2, A3 a3)
	{
		if (m_obj != NULL && m_method != NULL)
			return (m_obj->*m_method)(a1, a2, a3, m_arg);
		return R();
	}

	Obj *m_obj;
	Method m_method;
	Arg m_arg;
};

template <typename Obj>
class MemberEventListener : public paf::ui::EventListener {
public:
	typedef void (Obj::*Method)(int32_t, paf::ui::Handler *, paf::ui::Event *, void *);

	MemberEventListener(Obj *obj, Method method, void *userData)
		: m_obj(obj), m_method(method), m_userData(userData)
	{
	}

	virtual ~MemberEventListener() {}

	virtual int32_t Do(int32_t type, paf::ui::Handler *self, paf::ui::Event *e)
	{
		if (m_killed)
			return -1;
		if (m_obj != NULL && m_method != NULL)
			(m_obj->*m_method)(type, self, e, m_userData);
		return 0;
	}

	Obj *m_obj;
	Method m_method;
	void *m_userData;
};

/* ---- Forward declarations ---- */

class SigninExtState;
class SigninExtApp;
class SceneBase;
class HttpClient;
class HttpRequestJob;
class HttpImageClient;
class HttpImageFetchJob;

/* ---- Plugin entrypoint & singleton (signin_ext_module.cpp) ---- */

void SigninExtSetParam(paf::Plugin::InitParam2 *param);
void SigninExtInit(paf::Plugin *plugin);
void SigninExtStart(paf::Plugin *plugin);
void SigninExtStop(paf::Plugin *plugin);
void SigninExtExit(paf::Plugin *plugin);

class SigninExtPlugin {
public:
	explicit SigninExtPlugin(paf::Plugin *plugin)
		: m_plugin(plugin), m_app(NULL), m_mutex("SceSigninExt", 0)
	{
	}
	virtual ~SigninExtPlugin();

	static void Create(paf::Plugin *plugin);
	static void Destroy(void);
	static void StartInstance(void);
	static void StopInstance(void);
	static paf::Plugin *GetPlugin(void)
	{
		return s_instance != NULL ? s_instance->m_plugin : NULL;
	}

	void Start(void);
	void OnAppFinished(int32_t result, void *userData);

	static SigninExtPlugin *s_instance;

	paf::Plugin *m_plugin;
	SigninExtApp *m_app;
	paf::thread::Mutex m_mutex;
};

static inline paf::Plugin *SigninExtGetPlugin(void)
{
	return SigninExtPlugin::GetPlugin();
}

/* ---- SigninExtApp & factory registration (signin_ext_app.cpp) ---- */

class SigninExtApp {
public:
	SigninExtApp();
	virtual ~SigninExtApp();

	void Start(Callback1<int32_t> *callback);
	void Abort(void);
	void CleanupModels(void);
	void Finish(void);
	void OnStateFinished(int32_t result, void *userData);

	int32_t m_mode;
	wchar_t *m_idArg;
	wchar_t *m_pwArg;
	Callback1<int32_t> *m_finishCallback;
	SigninExtState *m_state;
	int32_t m_result;
};

void SigninExtRegisterWidgetFactory(void);
void SigninExtUnregisterWidgetFactory(void);

/* ---- Custom ListEdgeShader, ListEdgeObj, ListEdge, PlaneShadow ---- */

struct ListEdgeProgramPair;

class ListEdgeShader : public paf::graph::Shader {
public:
	ListEdgeShader();
	virtual ~ListEdgeShader();
	virtual int32_t Draw(paf::graph::PlaneObjDrawParams &params);

	int32_t DrawListEdge(paf::graph::PlaneObjDrawParams &params);

	ListEdgeProgramPair *m_program;
	paf::math::v2 m_coefficient;
};

void ListEdgeShaderInit(void);
void ListEdgeShaderTerm(void);
const paf::common::SharedPtr<paf::graph::Shader> &ListEdgeShaderGet(void);

class ListEdgeObj : public paf::graph::PlaneObj {
public:
	enum EdgeSide {
		EDGE_UPPER = 0,
		EDGE_LOWER = 1
	};

	static const char m_drawobj_type[];

	explicit ListEdgeObj(const paf::graph::DrawStyle &style);
	virtual ~ListEdgeObj();

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;
	virtual void OnDraw(const paf::math::matrix &parent, uint32_t color, float alpha,
			    paf::graph::DrawInfo &info);
	virtual void UpdateLayout();
	virtual void UpdateDivVerticesPosition();
	virtual void UpdateDivVerticesTexcoord();
	virtual void Draw(const paf::math::matrix &world, uint32_t color, float alpha);

	void SetBasePlane(const paf::math::v2 &pos, const paf::math::v2 &size);
	float InterpolateTexCoord(float pos, const float *positions, const float *texCoords,
				  int32_t count) const;

	EdgeSide m_edgeSide;
	float m_gradient;
	float m_intercept;
	paf::math::v2 m_basePos;
	paf::math::v2 m_baseSize;
	paf::math::v4 m_customTexRegion;
};

class ListEdge : public paf::ui::Widget {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	ListEdge(paf::ui::Widget *parent, CreateParam *param);
	virtual ~ListEdge();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual uint32_t CheckState();
	virtual void UpdateSize();
	virtual int32_t SetTexture(const paf::intrusive_ptr<paf::graph::Surface> &surf,
				   int32_t obj, uint32_t index);
	using paf::ui::Widget::SetTexture;

	void UpdateEdges(void);
	void AttachTarget(paf::ui::Widget *target);

	paf::ui::Widget *m_targetWidget;
	float m_topMargin;
	float m_bottomMargin;
};

class StyleListEdge : public paf::ui::WidgetStyleParam {
public:
	static const char m_param_name[];

	StyleListEdge();
	virtual ~StyleListEdge();

	virtual const char *GetParamName() const;
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(paf::ui::Widget *widget);
	virtual int32_t GetEach(paf::ui::Widget *widget);

	float m_topMargin;
	float m_topGradient;
	float m_topIntercept;
	float m_bottomMargin;
	float m_bottomGradient;
	float m_bottomIntercept;
	paf::graph::PlaneObjParam m_upperObj;
	paf::graph::PlaneObjParam m_lowerObj;
};

class PlaneShadow : public paf::ui::Plane {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	PlaneShadow(paf::ui::Widget *parent, CreateParam *param);
	virtual ~PlaneShadow();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void UpdateSize();

	void EnsureShadowObj(void);
	void EnsureHighlightObj(void);
};

class StylePlaneShadow : public paf::ui::PlaneStyleParam {
public:
	static const char m_param_name[];

	StylePlaneShadow();
	virtual ~StylePlaneShadow();

	virtual const char *GetParamName() const;
	virtual int32_t SetEach(paf::ui::Widget *widget);

	paf::graph::PlaneObjParam m_shadowObj;
	paf::graph::PlaneObjParam m_highlightObj;
};

extern paf::ui::FactoryFuncTable g_factoryListEdge;
extern paf::ui::FactoryFuncTable g_factoryPlaneShadow;

/* ---- JobQueue & memory allocator (signin_ext_job_queue.cpp) ---- */

void SigninExtJobQueueInit(void);
void SigninExtJobQueueTerm(void);
void *SigninExtAlloc(SceSize size);
void SigninExtFree(void *ptr);
paf::common::SharedPtr<paf::Buffer> SigninExtAllocBuffer(SceSize size);
paf::graph::SurfacePool *SigninExtGetSurfacePool(void);
paf::job::JobQueue *SigninExtGetJobQueue(void);

/* ---- PowerTick, RegMgr, SE, TaskManager (signin_ext_reg.cpp) ---- */

class PowerTickGuard {
public:
	PowerTickGuard();
	~PowerTickGuard();

	void Enable(void);
	static void OnTick(void *userData);

	bool m_registered;
};

class TaskBase {
public:
	explicit TaskBase(Callback1<int32_t> *callback);
	virtual ~TaskBase();

	virtual void Start(void) = 0;
	virtual void Cancel(void) = 0;
	virtual void Complete(int32_t result);
	virtual void OnComplete(int32_t result);

	Callback1<int32_t> *m_callback;
};

class JobTask : public TaskBase, public paf::job::JobItem {
public:
	JobTask(paf::job::JobQueue *queue, Callback1<int32_t> *callback, const char *name);
	virtual ~JobTask();

	virtual void Start(void);
	virtual void Cancel(void);
	virtual void Complete(int32_t result);
	virtual int32_t Run(void) = 0;
	virtual void Finish(int32_t result);

	paf::job::JobQueue *m_queue;
};

class RegDateFormatTask : public JobTask {
public:
	explicit RegDateFormatTask(Callback1<int32_t> *callback);
	virtual ~RegDateFormatTask();
	virtual int32_t Run(void);
};

void SigninExtPlayCommonSE(uint32_t index);
bool SigninExtTaskIsRunning(void);
void SigninExtTaskCancel(void);
void SigninExtTaskStart(TaskBase *task);

/* ---- HTTP & Image clients (signin_ext_http.cpp, signin_ext_eula.cpp, signin_ext_image_manager.cpp) ---- */

void SigninExtHttpInit(void);
void SigninExtHttpTerm(void);

class HttpConnection {
public:
	HttpConnection(int32_t templateId, const paf::Url &url);
	~HttpConnection();

	paf::string m_baseUrl;
	int32_t m_connId;
	bool m_isHttps;
};

class HttpRequestJob : public paf::job::JobItem {
public:
	HttpRequestJob(HttpClient *client, const char *url, int32_t responseMode,
		       Callback2<HttpRequestJob *, int32_t> *callback);
	virtual ~HttpRequestJob();

	virtual int32_t Run(void);
	virtual void Finish(int32_t result);
	virtual void Cancel(void);
	virtual void OnFinished(int32_t result);

	int32_t ReadFixedLength(uint32_t contentLength);
	int32_t ReadChunked(void);
	int32_t ExecuteHttp(void);

	HttpClient *m_client;
	paf::Url m_url;
	int32_t m_responseMode;
	Callback2<HttpRequestJob *, int32_t> *m_callback;
	int32_t m_reqId;
	paf::common::SharedPtr<paf::Buffer> m_buffer;
	int32_t m_statusCode;
};

class HttpClient {
public:
	explicit HttpClient(paf::job::JobQueue *queue);
	virtual ~HttpClient();

	paf::common::SharedPtr<HttpConnection> AcquireConnection(const paf::Url &url);
	void ReleaseConnection(paf::common::SharedPtr<HttpConnection> &conn);
	void Enqueue(const char *url, int32_t responseMode,
		     Callback2<HttpRequestJob *, int32_t> *callback);
	void CancelAll(void);
	bool HasActiveJobs(void) const;

	paf::job::JobQueue *m_jobQueue;
	paf::list<paf::common::SharedPtr<paf::job::JobItem> > m_activeJobs;
	int32_t m_maxHttpConns;
	int32_t m_maxHttpsConns;
	paf::list<paf::common::SharedPtr<HttpConnection> > m_httpPool;
	paf::list<paf::common::SharedPtr<HttpConnection> > m_httpsPool;
};

struct HttpImageRequestOption {
	int32_t width;
	int32_t height;
	paf::graph::SurfacePool *surfacePool;
	FuncCallback3<paf::intrusive_ptr<paf::graph::Surface>,
		      paf::graph::SurfacePool *,
		      const paf::common::SharedPtr<paf::image::Image> &,
		      int32_t> *surfaceCreator;
	paf::common::SharedPtr<paf::Buffer> preloadedBuffer;
};

class HttpImageFetchJob : public HttpRequestJob {
public:
	HttpImageFetchJob(HttpClient *client, const char *url,
			  Callback2<HttpImageFetchJob *, int32_t> *callback,
			  const HttpImageRequestOption *opt);
	virtual ~HttpImageFetchJob();

	virtual int32_t Run(void);
	virtual void OnFinished(int32_t result);

	int32_t DecodeImage(void);

	int32_t m_width;
	int32_t m_height;
	paf::graph::SurfacePool *m_surfacePool;
	FuncCallback3<paf::intrusive_ptr<paf::graph::Surface>,
		      paf::graph::SurfacePool *,
		      const paf::common::SharedPtr<paf::image::Image> &,
		      int32_t> *m_surfaceCreator;
	Callback2<HttpImageFetchJob *, int32_t> *m_imageCallback;
	paf::intrusive_ptr<paf::graph::Surface> m_surface;
};

class HttpImageClient : public HttpClient {
public:
	explicit HttpImageClient(paf::job::JobQueue *queue);
	virtual ~HttpImageClient();

	void EnqueueImage(const char *url,
			  Callback2<HttpImageFetchJob *, int32_t> *callback,
			  const HttpImageRequestOption *opt);
};

typedef Callback3<const char *, int32_t, paf::intrusive_ptr<paf::graph::Surface> > ImageLoadCallback;

struct ImageExtentRequest {
	int32_t width;
	int32_t height;
};

class ImageCacheEntry {
public:
	ImageCacheEntry();
	~ImageCacheEntry();

	paf::string m_url;
	paf::intrusive_ptr<paf::graph::Surface> m_surface;
	paf::list<ImageLoadCallback *> m_callbacks;
};

class SigninExtImageManager {
public:
	explicit SigninExtImageManager(paf::graph::SurfacePool *pool);
	~SigninExtImageManager();

	void NotifyEntry(ImageCacheEntry *entry, int32_t result);
	void RequestImage(const char *url, ImageLoadCallback *callback,
			  const ImageExtentRequest *extent);
	void CancelAll(Callback0 *onCanceled);
	void ClearAll(void);
	paf::intrusive_ptr<paf::graph::Surface> CreateSurface(
		paf::graph::SurfacePool *pool,
		const paf::common::SharedPtr<paf::image::Image> &image,
		int32_t imageType, void *userData);
	void OnImageFetched(HttpImageFetchJob *job, int32_t result, ImageCacheEntry *entry);

	paf::graph::SurfacePool *m_surfacePool;
	paf::job::JobQueue *m_jobQueue;
	HttpImageClient *m_httpClient;
	paf::list<ImageCacheEntry *> m_cachedEntries;
	paf::list<ImageCacheEntry *> m_pendingEntries;
	paf::thread::RMutex m_mutex;
	Callback0 *m_cancelCallback;
};

void SigninExtImageManagerCreate(paf::graph::SurfacePool *pool);
void SigninExtImageManagerDestroy(void);
void SigninExtImageRequest(const char *url, ImageLoadCallback *callback,
			   const ImageExtentRequest *extent);
void SigninExtImageCancelAll(Callback0 *onCanceled);
void SigninExtImageClearAll(void);

/* ---- Commerce2 tasks (signin_ext_commerce.cpp) ---- */

int32_t SigninExtCommerceInit(void);
int32_t SigninExtCommerceTerm(void);
int32_t SigninExtCommerceSetServerEnv(int32_t envMode);

class Commerce2InitTask : public JobTask {
public:
	explicit Commerce2InitTask(Callback1<int32_t> *callback);
	virtual ~Commerce2InitTask() {}
	virtual int32_t Run(void);
};

class Commerce2TermTask : public JobTask {
public:
	explicit Commerce2TermTask(Callback1<int32_t> *callback);
	virtual ~Commerce2TermTask() {}
	virtual int32_t Run(void);
};

class Commerce2RequestTask : public JobTask {
public:
	explicit Commerce2RequestTask(Callback1<int32_t> *callback);
	virtual ~Commerce2RequestTask();

	virtual int32_t Run(void);
	virtual void Finish(int32_t result);
	virtual void Cancel(void);
	virtual int32_t CreateReq(void) = 0;
	virtual int32_t SendReq(void) = 0;
	virtual int32_t RecvResp(void) = 0;

	uint32_t m_reqId;
	SceSize m_recvSize;
};

class Commerce2GetCountryListTask : public Commerce2RequestTask {
public:
	explicit Commerce2GetCountryListTask(Callback1<int32_t> *callback);
	virtual ~Commerce2GetCountryListTask() {}
	virtual int32_t CreateReq(void);
	virtual int32_t SendReq(void);
	virtual int32_t RecvResp(void);
	virtual void OnComplete(int32_t result);
};

class Commerce2EulaLoginTask : public Commerce2RequestTask {
public:
	explicit Commerce2EulaLoginTask(Callback1<int32_t> *callback);
	virtual ~Commerce2EulaLoginTask() {}
	virtual int32_t Run(void);
	virtual int32_t CreateReq(void);
	virtual int32_t SendReq(void);
	virtual int32_t RecvResp(void);
	virtual void OnComplete(int32_t result);
};

class Commerce2AcceptEulaTask : public Commerce2RequestTask {
public:
	explicit Commerce2AcceptEulaTask(Callback1<int32_t> *callback);
	virtual ~Commerce2AcceptEulaTask() {}
	virtual int32_t CreateReq(void);
	virtual int32_t SendReq(void);
	virtual int32_t RecvResp(void);
};

class Commerce2VerifyDobTask : public Commerce2RequestTask {
public:
	explicit Commerce2VerifyDobTask(Callback1<int32_t> *callback);
	virtual ~Commerce2VerifyDobTask() {}
	virtual int32_t Run(void);
	virtual int32_t CreateReq(void);
	virtual int32_t SendReq(void);
	virtual int32_t RecvResp(void);
	virtual void OnComplete(int32_t result);
};

class Commerce2SendResetMailTask : public Commerce2RequestTask {
public:
	explicit Commerce2SendResetMailTask(Callback1<int32_t> *callback);
	virtual ~Commerce2SendResetMailTask() {}
	virtual int32_t CreateReq(void);
	virtual int32_t SendReq(void);
	virtual int32_t RecvResp(void);
	virtual void OnComplete(int32_t result);
};

class Commerce2PasswordLoginTask : public Commerce2RequestTask {
public:
	explicit Commerce2PasswordLoginTask(Callback1<int32_t> *callback);
	virtual ~Commerce2PasswordLoginTask() {}
	virtual int32_t Run(void);
	virtual int32_t CreateReq(void);
	virtual int32_t SendReq(void);
	virtual int32_t RecvResp(void);
	virtual void OnComplete(int32_t result);
};

class Commerce2ChangePasswordTask : public Commerce2RequestTask {
public:
	explicit Commerce2ChangePasswordTask(Callback1<int32_t> *callback);
	virtual ~Commerce2ChangePasswordTask() {}
	virtual int32_t CreateReq(void);
	virtual int32_t SendReq(void);
	virtual int32_t RecvResp(void);
	virtual void OnComplete(int32_t result);
};

class Commerce2ResetPasswordTask : public Commerce2RequestTask {
public:
	explicit Commerce2ResetPasswordTask(Callback1<int32_t> *callback);
	virtual ~Commerce2ResetPasswordTask() {}
	virtual int32_t CreateReq(void);
	virtual int32_t SendReq(void);
	virtual int32_t RecvResp(void);
	virtual void OnComplete(int32_t result);
};

class Commerce2LogoutTask : public Commerce2RequestTask {
public:
	explicit Commerce2LogoutTask(Callback1<int32_t> *callback);
	virtual ~Commerce2LogoutTask() {}
	virtual int32_t Run(void);
	virtual int32_t CreateReq(void);
	virtual int32_t SendReq(void);
	virtual int32_t RecvResp(void);
	virtual void OnComplete(int32_t result);
};

/* ---- Models (signin_ext_model.cpp & signin_ext_eula.cpp) ---- */

class ModelBase {
public:
	virtual ~ModelBase() {}
};

class TextInputField {
public:
	TextInputField(int32_t maxLen, int32_t fieldType);
	virtual ~TextInputField();

	void ToUtf8(paf::string *out) const;
	void SetWide(const wchar_t *src);
	void SetUtf8(const char *src);
	void Clear(void);
	bool Equals(const TextInputField &other) const;

	wchar_t *m_buf;
	int32_t m_maxLen;
	int32_t m_length;
	int32_t m_fieldType;
};

struct CountryEntry {
	uint32_t code;
	paf::vector<int32_t> languages;
	int32_t dateFormat;
	bool flag;
};

void SigninExtCountryTableClear(void);
int32_t SigninExtCountryTableParse(const void *buf, SceSize size);
const CountryEntry *SigninExtCountryTableGet(int32_t index);
void SigninExtCountryTableAddFallback(const char *countryCode, const char *langCode);
int32_t SigninExtCountryTableCount(void);

class SelectListModel {
public:
	SelectListModel() : m_selectedIndex(-1) {}
	virtual ~SelectListModel() {}

	virtual int32_t GetCount(void) = 0;
	virtual const wchar_t *GetLabel(int32_t index) = 0;

	void SetSelectedIndex(int32_t index)
	{
		if (index >= 0 && index < GetCount())
			m_selectedIndex = index;
		else
			m_selectedIndex = -1;
	}

	int32_t m_selectedIndex;
};

class CountryModel : public SelectListModel {
public:
	CountryModel() {}
	virtual ~CountryModel() {}

	virtual int32_t GetCount(void);
	virtual const wchar_t *GetLabel(int32_t index);

	const char *GetCountryCode(int32_t index);
	int32_t FindByCode(const char *code);
};

class LanguageModel : public SelectListModel {
public:
	LanguageModel();
	virtual ~LanguageModel() {}

	virtual int32_t GetCount(void);
	virtual const wchar_t *GetLabel(int32_t index);

	void BindCountry(const CountryModel *country);
	int32_t GetLanguageId(int32_t index);
	const char *GetLanguageCode(int32_t index);
	int32_t FindByCode(const char *langCode);

	const CountryEntry *m_countryEntry;
};

class AccountInfoModel : public ModelBase {
public:
	AccountInfoModel();
	virtual ~AccountInfoModel();

	int32_t LoadFromCommerceAccount(const SceNpCommerce2AccountInfo *info);
	void SetEulaDocCount(int32_t count);
	void AdvanceEulaDoc(void);

	CountryModel m_country;
	LanguageModel m_language;
	int32_t m_eulaDocCount;
	int32_t m_eulaDocIndex;
};

class SpinFieldModel {
public:
	virtual ~SpinFieldModel() {}
	virtual int32_t GetMin(void) { return m_min; }
	virtual int32_t GetMax(void) { return m_max; }
	virtual void SetValue(int32_t value);
	virtual int32_t GetValue(void) { return m_value; }
	virtual const wchar_t *FormatValue(int32_t value) = 0;

	int32_t m_min;
	int32_t m_max;
	int32_t m_value;
};

class YearField : public SpinFieldModel {
public:
	YearField(int32_t minYear, int32_t maxYear, int32_t value);
	virtual ~YearField() {}
	virtual const wchar_t *FormatValue(int32_t value);
};

class MonthField : public SpinFieldModel {
public:
	MonthField();
	virtual ~MonthField() {}
	virtual const wchar_t *FormatValue(int32_t value);
};

class DayField : public SpinFieldModel {
public:
	DayField();
	virtual ~DayField() {}
	virtual const wchar_t *FormatValue(int32_t value);
};

class BirthDateModel {
public:
	BirthDateModel();
	virtual ~BirthDateModel() {}

	int32_t Validate(void);
	int32_t SetField(int32_t fieldIndex, int32_t value);
	int32_t CopyFrom(const BirthDateModel &src);
	int32_t SetYear(int32_t year);
	int32_t SetMonth(int32_t month);
	int32_t SetDay(int32_t day);
	paf::wstring FormatDisplayString(void);

	int32_t m_dateFormat;
	YearField m_year;
	MonthField m_month;
	DayField m_day;
	int32_t m_status;
	int32_t m_errorCode;
};

class PasswordField : public TextInputField {
public:
	PasswordField();
	virtual ~PasswordField() {}

	void Validate(void);
	void Clear(void);
	void MarkConflict(uint32_t conflictBit);
	void SetServerResult(int32_t err);

	int32_t m_status;
	int32_t m_errorCode;
	uint32_t m_ruleFlags;
};

class ConfirmPasswordField : public TextInputField {
public:
	ConfirmPasswordField();
	virtual ~ConfirmPasswordField() {}

	void Clear(void);
	int32_t CheckMatch(const PasswordField &pw);

	int32_t m_status;
	int32_t m_errorCode;
};

class PasswordChangeModel : public ModelBase {
public:
	PasswordChangeModel();
	virtual ~PasswordChangeModel();

	void Reset(bool clearCredentials);
	int32_t LoadFromCommerceAccount(const SceNpCommerce2AccountInfo *info);
	void ClearNewPassword(void);
	void ClearConfirmPassword(void);
	int32_t SetNewPassword(const wchar_t *pw);
	int32_t SetConfirmPassword(const wchar_t *pw);

	TextInputField m_signInId;
	TextInputField m_currentPassword;
	TextInputField m_onlineId;
	uint64_t m_accountId;
	uint32_t m_countryCode;
	PasswordField m_newPassword;
	ConfirmPasswordField m_confirmPassword;
};

class DobVerifyModel : public ModelBase {
public:
	DobVerifyModel();
	virtual ~DobVerifyModel();

	void Reset(bool clearSignInId);

	TextInputField m_signInId;
	BirthDateModel m_birthDate;
};

class ExternalStringField {
public:
	explicit ExternalStringField(int32_t maxLen);
	~ExternalStringField();

	void AttachBuffer(wchar_t *buf);
	void ToUtf8(paf::string *out) const;
	void SetString(const wchar_t *src);

	wchar_t *m_buf;
	int32_t m_maxLen;
	int32_t m_length;
};

class SignInCredentialsModel : public ModelBase {
public:
	SignInCredentialsModel();
	virtual ~SignInCredentialsModel();

	ExternalStringField m_id;
	ExternalStringField m_password;
};

extern SignInCredentialsModel *g_credentialsModel;
extern AccountInfoModel *g_accountInfoModel;
extern PasswordChangeModel *g_passwordChangeModel;
extern DobVerifyModel *g_dobVerifyModel;

int32_t SigninExtFindLanguageIndex(const char *langCode);
int32_t SigninExtAdjustLanguageForCountry(int32_t langIndex, uint32_t countryCode);
void SigninExtModelsInit(void);
void SigninExtClearAllModels(void);

/* ---- signup_library UI helpers (signin_ext_eula.cpp & signin_ext_ui.cpp) ---- */

class EulaUrlBuilder {
public:
	EulaUrlBuilder();
	~EulaUrlBuilder();

	void SetVersioned(int32_t versioned);
	void SetDocType(int32_t docType);
	void SetCountry(const char *country);
	void SetLanguage(const char *language);
	void SetPageIndex(int32_t pageIndex);
	const char *GetUrl(void);

	int32_t m_versioned;
	int32_t m_docType;
	const char *m_country;
	const char *m_language;
	int32_t m_pageIndex;
	paf::string m_url;
	bool m_dirty;
};

class DialogBase {
public:
	DialogBase();
	virtual ~DialogBase();

	virtual void PlayOpenAnim(void);
	virtual void PlayCloseAnim(void);
	virtual void Close(bool animate);

	void PrepareAndShow(bool playOpenAnim);

	paf::IDParam m_pageId;
	int32_t m_commonDialogId;
	paf::ui::Widget *m_parentScene;
	paf::ui::Scene *m_scene;
	paf::ui::Widget *m_panel;
};

class ScrollListEdgeHelper {
public:
	ScrollListEdgeHelper();
	virtual ~ScrollListEdgeHelper();

	void Attach(paf::ui::Widget *parent, paf::ui::Widget *target, const paf::IDParam &styleId);

	ListEdge *m_listEdge;
};

class SigninExtScrollBarController : public paf::ui::ScrollViewScrollBarController {
public:
	SigninExtScrollBarController() {}
	virtual ~SigninExtScrollBarController() {}
	virtual void UpdateLayout(const paf::ui::ScrollViewBarLayout &layout);
};

class NumSpinController {
public:
	NumSpinController();
	virtual ~NumSpinController();

	void Bind(paf::ui::Widget *widget, SpinFieldModel *model, Callback1<int32_t> *onChanged);
	void SyncFromModel(void);
	void OnValueChanged(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *userData);

	paf::ui::NumSpin *m_spin;
	SpinFieldModel *m_model;
	Callback1<int32_t> *m_onChanged;
};

class DobSelectButton {
public:
	DobSelectButton();
	virtual ~DobSelectButton();

	void Bind(paf::ui::Widget *button, const paf::IDParam &titleId, BirthDateModel *model,
		  Callback1<BirthDateModel *> *onSelected);
	void OpenDialog(void);
	void RefreshText(void);
	void OnButtonPressed(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *userData);
	void OnDialogResult(BirthDateModel *result, void *userData);

	paf::ui::Widget *m_button;
	paf::IDParam m_titleId;
	BirthDateModel *m_model;
	Callback1<BirthDateModel *> *m_onSelected;
};

class ListSelectButton {
public:
	ListSelectButton();
	virtual ~ListSelectButton();

	void Bind(paf::ui::Widget *button, const paf::IDParam &titleId, SelectListModel *model,
		  Callback1<int32_t> *onSelected);
	void OpenDialog(void);
	void RefreshText(void);
	void OnButtonPressed(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *userData);
	void OnBindItem(paf::ui::Widget *itemWidget, int32_t index, void *userData);
	void OnDialogResult(int32_t index, void *userData);

	paf::ui::Widget *m_button;
	paf::IDParam m_titleId;
	SelectListModel *m_model;
	Callback1<int32_t> *m_onSelected;
};

void SigninExtDobDialogInit(void);
void SigninExtDobDialogCloseImmediate(void);
void SigninExtDobDialogTerm(void);
void SigninExtListDialogInit(void);
void SigninExtListDialogCloseImmediate(void);
void SigninExtListDialogTerm(void);
int32_t SigninExtShowInfoScrollDialog(const paf::IDParam &titleId,
				      const paf::IDParam &templateId,
				      paf::ui::Widget *parentScene,
				      Callback0 *onClosed);

paf::wstring SigninExtFormatMessage(const wchar_t *fmt,
				    const wchar_t *arg1,
				    const wchar_t *arg2 = NULL,
				    const wchar_t *arg3 = NULL,
				    const wchar_t *arg4 = NULL,
				    const wchar_t *arg5 = NULL,
				    const wchar_t *arg6 = NULL,
				    const wchar_t *arg7 = NULL,
				    const wchar_t *arg8 = NULL,
				    const wchar_t *arg9 = NULL,
				    const wchar_t *arg10 = NULL,
				    const wchar_t *arg11 = NULL);

/* ---- Environment, IME, Popup, Error dialogs, Form widgets (signin_ext_ui.cpp) ---- */

void SigninExtEnvInit(void);
void SigninExtEnvTerm(void);
void SigninExtSetLanguageId(int32_t locale);
const wchar_t *SigninExtGetString(const paf::IDParam &id);
paf::intrusive_ptr<paf::graph::Surface> SigninExtGetTexture(const paf::IDParam &id);
paf::ui::Scene *SigninExtPageOpenWithParam(const paf::IDParam &id,
					   paf::Plugin::PageOpenParam &param);
paf::ui::Scene *SigninExtPageOpen(const paf::IDParam &id);
void SigninExtPageClose(const paf::IDParam &id);
void SigninExtPageCloseAll(void);
int32_t SigninExtTemplateOpen(paf::ui::Widget *parent, const paf::IDParam &id,
			      const paf::Plugin::TemplateOpenParam &param);
paf::ui::Widget *SigninExtCreateWidget(paf::ui::Widget *parent, const char *type,
				       const paf::IDParam &id, const paf::IDParam &style);
void SigninExtCloseAllDialogs(void);

void SigninExtImeInit(void);
void SigninExtImeEndEdit(void);
void SigninExtImeSuspendEdit(void);
void SigninExtImeResumeEdit(void);

void SigninExtSetAutoShrinkText(paf::ui::Widget *widget, const wchar_t *text,
				float extraWidth = 0.0f);

struct ErrorDialogParam {
	paf::IDParam titleId;
	int32_t errorCode;
	uint32_t pwRuleFlags;
	paf::wstring message;

	ErrorDialogParam(const paf::IDParam &title, int32_t code, uint32_t flags = 0)
		: titleId(title), errorCode(code), pwRuleFlags(flags), message()
	{
	}
};

void SigninExtPopupInit(void);
void SigninExtPopupCloseImmediate(void);
void SigninExtPopupTerm(void);
void SigninExtPopupClose(int32_t result);
int32_t SigninExtPopupShow(int32_t type, const paf::wstring *text,
			   paf::ui::Widget *parentScene, Callback1<int32_t> *callback);

void SigninExtErrorInit(void);
void SigninExtErrorCloseImmediate(void);
void SigninExtErrorTerm(void);
int32_t SigninExtErrorShow(ErrorDialogParam *param, paf::ui::Widget *parentScene,
			   Callback0 *callback);

/* ---- Scene classes (signin_ext_ui.cpp) ---- */

class SceneBase {
public:
	SceneBase();
	virtual ~SceneBase();

	virtual void Start(Callback1<int32_t> *callback);
	virtual void Abort(void);
	virtual void Finish(void);
	virtual void OnSessionExpired(void);

	Callback1<int32_t> *m_finishCallback;
	int32_t m_result;
};

template <typename Scene>
class SceneTaskCallback : public Callback1<int32_t> {
public:
	typedef void (Scene::*Method)(int32_t, void *);

	SceneTaskCallback(Scene *scene, Method method, void *userData = NULL)
		: m_scene(scene), m_method(method), m_userData(userData)
	{
	}

	virtual ~SceneTaskCallback() {}

	virtual void Invoke(int32_t result)
	{
		if (m_scene->m_result != 0x7FFFFFFF) {
			m_scene->Finish();
			return;
		}
		if (result == -1) {
			SigninExtPopupClose(-2);
			return;
		}
		if (result == -2141908987) {
			m_scene->OnSessionExpired();
			return;
		}
		if (m_scene != NULL && m_method != NULL)
			(m_scene->*m_method)(result, m_userData);
	}

	Scene *m_scene;
	Method m_method;
	void *m_userData;
};

template <typename Scene>
class SceneHttpCallback : public Callback2<HttpRequestJob *, int32_t> {
public:
	typedef void (Scene::*Method)(HttpRequestJob *, int32_t, void *);

	SceneHttpCallback(Scene *scene, Method method, void *userData = NULL)
		: m_scene(scene), m_method(method), m_userData(userData)
	{
	}

	virtual ~SceneHttpCallback() {}

	virtual void Invoke(HttpRequestJob *job, int32_t result)
	{
		if (m_scene->m_result != 0x7FFFFFFF) {
			m_scene->Finish();
			return;
		}
		if (result == -1) {
			SigninExtPopupClose(-2);
			return;
		}
		if (m_scene != NULL && m_method != NULL)
			(m_scene->*m_method)(job, result, m_userData);
	}

	Scene *m_scene;
	Method m_method;
	void *m_userData;
};

class PageBase : public SceneBase {
public:
	explicit PageBase(const paf::IDParam &pageId);
	virtual ~PageBase();

	virtual void Start(Callback1<int32_t> *callback);
	virtual void Abort(void);
	virtual void Finish(void);
	virtual void OnCreate(void) = 0;
	virtual void OnOpenAnimDone(void);
	virtual void OnCloseAnimStart(void);
	virtual void OnCloseAnimDone(void);
	virtual void OnButtonPressed(int32_t buttonId);

	void CloseWithResult(int32_t result);
	void PlayOpenAnim(void);
	void PlayCloseAnim(void);
	void OnCloseTimer(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *userData);

	paf::IDParam m_pageId;
	paf::ui::Scene *m_scene;
};

class FieldLabel {
public:
	FieldLabel();
	virtual ~FieldLabel();

	void Bind(paf::ui::Widget *widget, const paf::IDParam &textId);
	void SetErrorState(int32_t errCode);

	paf::ui::Widget *m_widget;
	paf::IDParam m_textId;
};

class FormRowBase {
public:
	FormRowBase() : m_container(NULL) {}
	virtual ~FormRowBase() {}
	virtual float GetBoxSpacing(void) { return 0.0f; }

	void SetupContainer(paf::ui::Widget *parent, bool verticalBox, bool openWrapperTemplate);

	paf::ui::Widget *m_container;
};

class TextBoxController {
public:
	TextBoxController();
	virtual ~TextBoxController();

	void ConfigureIme(const TextInputField *field);
	void Bind(paf::ui::TextBox *textBox, TextInputField *field,
		  Callback1<const wchar_t *> *onCommit);
	void SetPlaceholder(const paf::IDParam &textId);
	void BeginEdit(void);
	void SyncFromModel(void);
	void OnEditBegin(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *userData);
	void OnEnterPressed(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *userData);
	void OnEditEnd(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *userData);

	paf::ui::TextBox *m_textBox;
	TextInputField *m_field;
	Callback1<const wchar_t *> *m_onCommit;
	int32_t m_editState;
	bool m_suspended;
};

class TextFieldRow : public FormRowBase {
public:
	TextFieldRow();
	virtual ~TextFieldRow();

	void BindDirect(paf::ui::Widget *labelWidget, paf::ui::Widget *textBoxWidget,
			const paf::IDParam &labelId, TextInputField *field,
			Callback1<const wchar_t *> *onCommit);
	void BindTemplate(paf::ui::Widget *parent, const paf::IDParam &labelId,
			  TextInputField *field, Callback1<const wchar_t *> *onCommit,
			  bool openWrapper);
	void SyncFromModel(int32_t errCode);

	FieldLabel m_label;
	TextBoxController m_textBox;
};

class DobFieldRow : public FormRowBase {
public:
	DobFieldRow();
	virtual ~DobFieldRow();

	void Bind(paf::ui::Widget *parent, const paf::IDParam &labelId,
		  const paf::IDParam &dialogTitleId, BirthDateModel *model,
		  Callback1<BirthDateModel *> *onSelected, bool openWrapper);
	void SyncFromModel(void);

	FieldLabel m_label;
	DobSelectButton m_dobButton;
	BirthDateModel *m_model;
};

class SelectListFieldRow : public FormRowBase {
public:
	SelectListFieldRow();
	virtual ~SelectListFieldRow();

	void Bind(paf::ui::Widget *labelWidget, paf::ui::Widget *btnWidget,
		  const paf::IDParam &labelId, const paf::IDParam &dialogTitleId,
		  SelectListModel *model, Callback1<int32_t> *onSelected);
	void SyncFromModel(int32_t errCode);

	FieldLabel m_label;
	ListSelectButton m_selectButton;
};

class HeaderBar {
public:
	HeaderBar();
	virtual ~HeaderBar();

	void Init(paf::ui::Widget *parent, const paf::IDParam &titleId);

	paf::ui::Widget *m_titleWidget;
	paf::IDParam m_titleId;
};

class FooterBar {
public:
	FooterBar();
	virtual ~FooterBar();

	void Init(paf::ui::Widget *parent, Callback1<int32_t> *onButton);
	void SetButtonVisible(int32_t index, bool visible);
	void SetNextLabel(const paf::IDParam &labelId);
	void SetNextEnabled(bool enabled, bool animated);
	void OnButtonEvent(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *userData);

	Callback1<int32_t> *m_onButton;
	paf::ui::Widget *m_root;
	paf::ui::Widget *m_buttons[2];
	paf::IDParam m_nextLabelId;
};

class PasswordInputGroup : public FormRowBase {
public:
	PasswordInputGroup();
	virtual ~PasswordInputGroup();

	virtual float GetBoxSpacing(void) { return 0.0f; }

	void Bind(paf::ui::Widget *parent, PasswordField *pwField,
		  ConfirmPasswordField *confirmField,
		  Callback2<int32_t, const wchar_t *> *onFieldCommit, bool openWrapper);
	void SyncFromModels(void);
	void BeginEditField(int32_t fieldIndex);
	int32_t GetFieldEditState(int32_t fieldIndex) const;
	void OnFieldCommit(const wchar_t *text, int32_t fieldIndex);
	void OnPolicyInfoClicked(int32_t type, paf::ui::Handler *self,
				 paf::ui::Event *e, void *userData);
	void OnPolicyLabelLayout(int32_t type, paf::ui::Handler *self,
				 paf::ui::Event *e, void *userData);

	TextFieldRow m_pwRow;
	TextFieldRow m_confirmRow;
	PasswordField *m_pwField;
	ConfirmPasswordField *m_confirmField;
	Callback2<int32_t, const wchar_t *> *m_onFieldCommit;
};

class RichTextController {
public:
	RichTextController();
	virtual ~RichTextController();

	void Bind(paf::ui::RichText *richText, Callback1<int32_t> *onStatus);
	void LoadUrl(const char *url);
	void Clear(void);
	void CancelAndNotify(void);
	void OnHttpDone(HttpRequestJob *job, int32_t result, void *userData);
	void OnReqImageSize(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *userData);
	void OnScrollInItem(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *userData);
	void OnImageSizeLoaded(const char *url, int32_t result,
			       paf::intrusive_ptr<paf::graph::Surface> surf, int32_t imgIndex);
	void OnImageTexLoaded(const char *url, int32_t result,
			      paf::intrusive_ptr<paf::graph::Surface> surf, int32_t itemIndex);
	void OnLayoutFinished(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *userData);

	HttpClient m_httpClient;
	paf::ui::RichText *m_richText;
	Callback1<int32_t> *m_onStatus;
	paf::string m_url;
	int32_t m_lastHttpStatus;
	bool m_active;
};

/* ---- Concrete flow scenes ---- */

class ResetPasswordDoneScene : public SceneBase {
public:
	ResetPasswordDoneScene();
	virtual ~ResetPasswordDoneScene();

	virtual void Start(Callback1<int32_t> *callback);

	void ShowConfirmPopup(void);
	void OnConfirmResult(int32_t result, void *userData);
	void OnHelpClosed(int32_t result, void *userData);
	void OnSentOk(int32_t result, void *userData);
	void OnBusyCancel(int32_t result, void *userData);
	void OnResetMailDone(int32_t result, void *userData);
	void OnErrorDismissed(int32_t errCode);

	const wchar_t *m_email;
};

class EulaScene : public PageBase {
public:
	EulaScene();
	virtual ~EulaScene();

	virtual void OnCreate(void);
	virtual void OnOpenAnimDone(void);
	virtual void OnCloseAnimStart(void);
	virtual void OnButtonPressed(int32_t buttonId);

	void StartLoadDocument(void);
	void OnLanguageChanged(int32_t index, void *userData);
	void OnRichTextStatus(int32_t status, void *userData);
	void OnCancelConfirm(int32_t result, void *userData);
	void OnAcceptDone(int32_t result, void *userData);
	void OnAcceptPopupResult(int32_t result, void *userData);
	void OnErrorDismissed(void *userData);
	void OnLoadErrorDismissed(void *userData);

	SelectListFieldRow m_langRow;
	RichTextController m_richTextCtrl;
	ScrollListEdgeHelper m_edgeBinder;
	HeaderBar m_header;
	FooterBar m_footer;
	paf::ui::Widget *m_busyIndicator;
	EulaUrlBuilder m_urlBuilder;
	bool m_eulaAccepted;
	bool m_loading;
};

class CompleteNoticeScene : public PageBase {
public:
	explicit CompleteNoticeScene(int32_t noticeType);
	virtual ~CompleteNoticeScene();

	virtual void OnCreate(void);
	virtual void OnButtonPressed(int32_t buttonId);

	int32_t m_noticeType;
	HeaderBar m_header;
	FooterBar m_footer;
};

class InitSessionScene : public SceneBase {
public:
	explicit InitSessionScene(int32_t mode);
	virtual ~InitSessionScene();

	virtual void Start(Callback1<int32_t> *callback);
	virtual void Abort(void);

	void OnPopupResult(int32_t result, void *userData);
	void OnCommerceInitDone(int32_t result, void *userData);
	void OnCountryListDone(int32_t result, void *userData);
	void OnEulaLoginDone(int32_t result, void *userData);
	void OnEulaDatFetched(HttpRequestJob *job, int32_t result, void *userData);
	void OnRegDateFormatDone(int32_t result, void *userData);
	void OnPasswordLoginDone(int32_t result, void *userData);
	void OnResetPasswordDone(int32_t result, void *userData);
	void OnErrorDismissed(void *userData);
	void HandleError(int32_t errCode);

	int32_t m_mode;
	HttpClient m_httpClient;
	bool m_aborted;
};

class SessionExpiredScene : public PageBase {
public:
	explicit SessionExpiredScene(int32_t reason);
	virtual ~SessionExpiredScene();

	virtual void OnCreate(void);
	virtual void OnButtonPressed(int32_t buttonId);

	void OnSendMailDone(int32_t result, void *userData);
	void OnSendMailPopupResult(int32_t result, void *userData);

	int32_t m_reason;
	HeaderBar m_header;
	FooterBar m_footer;
};

class TermSessionScene : public SceneBase {
public:
	TermSessionScene();
	virtual ~TermSessionScene();

	virtual void Start(Callback1<int32_t> *callback);
	virtual void Abort(void);

	void OnLogoutDone(int32_t result, void *userData);
	void OnImageCanceled(void *userData);
	void OnCommerceTermDone(int32_t result, void *userData);
};

class ChangePasswordScene : public PageBase {
public:
	ChangePasswordScene();
	virtual ~ChangePasswordScene();

	virtual void OnCreate(void);
	virtual void OnButtonPressed(int32_t buttonId);

	void OnPasswordFieldChanged(int32_t fieldIndex, const wchar_t *text, void *userData);
	void OnChangePasswordDone(int32_t result, void *userData);
	void OnLogoutAfterChangeDone(int32_t result, void *userData);
	void OnChangePopupResult(int32_t result, void *userData);

	PasswordInputGroup m_passwordGroup;
	HeaderBar m_header;
	FooterBar m_footer;
	PasswordChangeModel *m_model;
};

class VerifyDobScene : public PageBase {
public:
	VerifyDobScene();
	virtual ~VerifyDobScene();

	virtual void OnCreate(void);
	virtual void OnButtonPressed(int32_t buttonId);

	void OnSignInIdCommit(const wchar_t *text, void *userData);
	void OnDobSelected(BirthDateModel *date, void *userData);
	void OnVerifyDone(int32_t result, void *userData);
	void OnSessionExpiredDismissed(void *userData);
	void OnVerifyPopupResult(int32_t result, void *userData);

	TextFieldRow m_signInIdRow;
	DobFieldRow m_dobRow;
	HeaderBar m_header;
	FooterBar m_footer;
	DobVerifyModel *m_model;
};

#endif
