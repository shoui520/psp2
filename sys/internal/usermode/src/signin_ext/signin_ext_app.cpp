/*
 * SceSigninExt (vs0:sys/external/signin_ext.suprx):
 * SigninExtApp — flow state machine coordinating EULA acceptance, DOB
 * verification, password reset/change, and Commerce2 session setup/teardown,
 * plus custom widget factory registration (list_edge, plane_shadow).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <graphics/color.h>
#include <plugin.h>
#include <widget/factory.h>
#include "signin_ext_internal.h"

using namespace paf;
using namespace paf::ui;

static int32_t s_factoryRefCount = 0;

/* ---- Flow state machine classes ---- */

class SigninExtState {
public:
	SigninExtState() : m_scene(NULL) {}

	virtual ~SigninExtState() {}
	virtual void CreateScene(void) = 0;
	virtual SigninExtState *NextState(int32_t result) = 0;

	SceneBase *m_scene;
};

class StateTermSession : public SigninExtState {
public:
	virtual ~StateTermSession() {}
	virtual void CreateScene(void) { m_scene = new TermSessionScene(); }
	virtual SigninExtState *NextState(int32_t result);
};

class StateCompleteNotice : public SigninExtState {
public:
	explicit StateCompleteNotice(int32_t noticeType) : m_noticeType(noticeType) {}
	virtual ~StateCompleteNotice() {}
	virtual void CreateScene(void) { m_scene = new CompleteNoticeScene(m_noticeType); }
	virtual SigninExtState *NextState(int32_t result);

	int32_t m_noticeType;
};

class StateResetPasswordDone : public SigninExtState {
public:
	virtual ~StateResetPasswordDone() {}
	virtual void CreateScene(void) { m_scene = new ResetPasswordDoneScene(); }
	virtual SigninExtState *NextState(int32_t result);
};

class StateSessionExpired : public SigninExtState {
public:
	explicit StateSessionExpired(int32_t reason) : m_reason(reason) {}
	virtual ~StateSessionExpired() {}
	virtual void CreateScene(void) { m_scene = new SessionExpiredScene(m_reason); }
	virtual SigninExtState *NextState(int32_t result);

	int32_t m_reason;
};

class StateChangePassword : public SigninExtState {
public:
	virtual ~StateChangePassword() {}
	virtual void CreateScene(void) { m_scene = new ChangePasswordScene(); }
	virtual SigninExtState *NextState(int32_t result);
};

class StateVerifyDob : public SigninExtState {
public:
	virtual ~StateVerifyDob() {}
	virtual void CreateScene(void) { m_scene = new VerifyDobScene(); }
	virtual SigninExtState *NextState(int32_t result);
};

class StateEula : public SigninExtState {
public:
	virtual ~StateEula() {}
	virtual void CreateScene(void) { m_scene = new EulaScene(); }
	virtual SigninExtState *NextState(int32_t result);
};

class StateInitSession : public SigninExtState {
public:
	explicit StateInitSession(int32_t mode) : m_mode(mode) {}
	virtual ~StateInitSession() {}
	virtual void CreateScene(void) { m_scene = new InitSessionScene(m_mode); }
	virtual SigninExtState *NextState(int32_t result);

	int32_t m_mode;
};

class StateInitEula : public StateInitSession {
public:
	StateInitEula() : StateInitSession(0) {}
	virtual ~StateInitEula() {}
	virtual SigninExtState *NextState(int32_t result);
};

class StateInitDob : public StateInitSession {
public:
	StateInitDob() : StateInitSession(1) {}
	virtual ~StateInitDob() {}
	virtual SigninExtState *NextState(int32_t result);
};

class StateInitChangePassword : public StateInitSession {
public:
	StateInitChangePassword() : StateInitSession(2) {}
	virtual ~StateInitChangePassword() {}
	virtual SigninExtState *NextState(int32_t result);
};

class StateInitResetPassword : public StateInitSession {
public:
	StateInitResetPassword() : StateInitSession(3) {}
	virtual ~StateInitResetPassword() {}
	virtual SigninExtState *NextState(int32_t result);
};

SigninExtState *StateInitSession::NextState(int32_t result)
{
	(void)result;
	return NULL;
}

SigninExtState *StateInitEula::NextState(int32_t result)
{
	if (result == 0)
		return new StateEula();
	return NULL;
}

SigninExtState *StateInitDob::NextState(int32_t result)
{
	if (result == 0)
		return new StateVerifyDob();
	return NULL;
}

SigninExtState *StateInitChangePassword::NextState(int32_t result)
{
	if (result == 0)
		return new StateChangePassword();
	if (result == 4)
		return new StateSessionExpired(1);
	return NULL;
}

SigninExtState *StateInitResetPassword::NextState(int32_t result)
{
	if (result == 0)
		return new StateResetPasswordDone();
	return NULL;
}

SigninExtState *StateEula::NextState(int32_t result)
{
	if (result == 0)
		return new StateTermSession();
	if (result == 4)
		return new StateEula();
	return NULL;
}

SigninExtState *StateVerifyDob::NextState(int32_t result)
{
	if (result == 0)
		return new StateSessionExpired(0);
	return NULL;
}

SigninExtState *StateSessionExpired::NextState(int32_t result)
{
	if (result != 0)
		return NULL;

	int32_t noticeType;
	const wchar_t *signInId;
	if (m_reason != 0) {
		noticeType = 2;
		signInId = g_passwordChangeModel->m_signInId.m_buf;
	} else {
		noticeType = 0;
		signInId = g_dobVerifyModel->m_signInId.m_buf;
	}

	g_credentialsModel->m_id.SetString(signInId);
	g_credentialsModel->m_password.SetString(L"");
	return new StateCompleteNotice(noticeType);
}

SigninExtState *StateChangePassword::NextState(int32_t result)
{
	if (result != 0)
		return NULL;

	g_credentialsModel->m_id.SetString(g_passwordChangeModel->m_signInId.m_buf);
	g_credentialsModel->m_password.SetString(g_passwordChangeModel->m_newPassword.m_buf);
	return new StateCompleteNotice(1);
}

SigninExtState *StateCompleteNotice::NextState(int32_t result)
{
	if (result == 0)
		return new StateTermSession();
	return NULL;
}

SigninExtState *StateResetPasswordDone::NextState(int32_t result)
{
	if (result == 0)
		return new StateTermSession();
	return NULL;
}

SigninExtState *StateTermSession::NextState(int32_t result)
{
	(void)result;
	return NULL;
}

/* ---- SigninExtApp ---- */

SigninExtApp::SigninExtApp()
	: m_mode(-1), m_idArg(NULL), m_pwArg(NULL),
	  m_finishCallback(NULL), m_state(NULL), m_result(0)
{
	SigninExtEnvInit();
	Plugin *plugin = SigninExtGetPlugin();
	bool found = false;

	Plugin::Argument argMode = plugin->GetArg("mode", &found);
	m_mode = (int32_t)argMode;
	if (!found)
		m_mode = -1;

	Plugin::Argument argId = plugin->GetArg("id", &found);
	m_idArg = (wchar_t *)(intptr_t)(int32_t)argId;
	if (!found)
		m_idArg = NULL;

	Plugin::Argument argPw = plugin->GetArg("pw", &found);
	m_pwArg = (wchar_t *)(intptr_t)(int32_t)argPw;
	if (!found)
		m_pwArg = NULL;
}

SigninExtApp::~SigninExtApp()
{
	SigninExtPageCloseAll();
	SigninExtEnvTerm();
}

void SigninExtApp::CleanupModels(void)
{
	SigninExtClearAllModels();
}

void SigninExtApp::Finish(void)
{
	CleanupModels();
	if (m_finishCallback != NULL) {
		Callback1<int32_t> *cb = m_finishCallback;
		m_finishCallback = NULL;
		cb->Invoke(m_result);
		delete cb;
	}
}

void SigninExtApp::Start(Callback1<int32_t> *callback)
{
	m_finishCallback = callback;
	SigninExtModelsInit();

	switch (m_mode) {
	case 0:
		if (m_idArg != NULL && m_pwArg != NULL)
			m_state = new StateInitEula();
		break;
	case 1:
		if (m_idArg != NULL && m_pwArg != NULL)
			m_state = new StateInitDob();
		break;
	case 2:
		if (m_idArg != NULL && m_pwArg != NULL)
			m_state = new StateInitChangePassword();
		break;
	case 3:
		if (m_idArg != NULL)
			m_state = new StateInitResetPassword();
		break;
	default:
		break;
	}

	if (m_state == NULL) {
		m_result = -3;
		Finish();
		return;
	}

	if (g_credentialsModel == NULL)
		g_credentialsModel = new SignInCredentialsModel();
	g_credentialsModel->m_id.AttachBuffer(m_idArg);
	g_credentialsModel->m_password.AttachBuffer(m_pwArg);

	switch (m_mode) {
	case 0:
		if (g_accountInfoModel == NULL)
			g_accountInfoModel = new AccountInfoModel();
		break;
	case 1:
		if (g_dobVerifyModel == NULL)
			g_dobVerifyModel = new DobVerifyModel();
		g_dobVerifyModel->Reset(false);
		break;
	case 2:
		if (g_passwordChangeModel == NULL)
			g_passwordChangeModel = new PasswordChangeModel();
		g_passwordChangeModel->Reset(false);
		break;
	default:
		break;
	}

	Callback1<int32_t> *stepCb = new MemberCallback1<SigninExtApp, int32_t>(
		this, &SigninExtApp::OnStateFinished);
	m_state->CreateScene();
	m_state->m_scene->Start(stepCb);
}

void SigninExtApp::Abort(void)
{
	if (m_state != NULL && m_state->m_scene != NULL)
		m_state->m_scene->Abort();
}

void SigninExtApp::OnStateFinished(int32_t result, void *userData)
{
	(void)userData;
	if (m_state->m_scene != NULL) {
		delete m_state->m_scene;
		m_state->m_scene = NULL;
	}

	SigninExtState *next = m_state->NextState(result);
	delete m_state;
	m_state = next;

	if (result == 1) {
		Finish();
		return;
	}

	if (m_state == NULL) {
		m_result = result;
		m_state = new StateTermSession();
	}

	Callback1<int32_t> *stepCb = new MemberCallback1<SigninExtApp, int32_t>(
		this, &SigninExtApp::OnStateFinished);
	m_state->CreateScene();
	m_state->m_scene->Start(stepCb);
}

/* ---- Widget factory registration ---- */

void SigninExtRegisterWidgetFactory(void)
{
	if (s_factoryRefCount++ == 0) {
		Plugin::RegistFactoryTable(g_factoryListEdge.m_name, g_factoryListEdge);
		Plugin::RegistFactoryTable(g_factoryPlaneShadow.m_name, g_factoryPlaneShadow);
		ListEdgeShaderInit();
	}
}

void SigninExtUnregisterWidgetFactory(void)
{
	if (--s_factoryRefCount == 0) {
		ListEdgeShaderTerm();
		Plugin::RemoveFactoryTable(g_factoryPlaneShadow.m_name);
		Plugin::RemoveFactoryTable(g_factoryListEdge.m_name);
	}
	if (s_factoryRefCount < 0)
		s_factoryRefCount = 0;
}
