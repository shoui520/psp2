/*
 * SceNpCommonPs4: NP tokens (npext_npm/np_token.cpp), the user's and the title's access tokens (strings) and the
 * requests that fetch them from the NP manager (user token) and from the NP auth service (title token).
 */

#include "../np_common_ps4_internal.h"

extern "C" {

/* a Scope that cancels the token request when it is cancelled (the request id follows the Scope) */
struct NpCommonPs4NpManagerScope {
	struct NpCommonPs4Scope scope;
	SceInt32 id;
};

extern const void *g_userTokenVtable[];		/* 0x8102aae8 */
extern const void *g_userTokenScopeVtable[];	/* 0x8102aaf8 */
extern const void *g_titleTokenVtable[];	/* 0x8102ab10 */
extern const void *g_titleTokenScopeVtable[];	/* 0x8102ab20 */

struct NpCommonPs4NpManagerScope *npCommonPs4UserTokenScopeDestruct(struct NpCommonPs4NpManagerScope *scope);
void npCommonPs4UserTokenScopeDelete(struct NpCommonPs4NpManagerScope *scope);
int npCommonPs4UserTokenScopeCancel(struct NpCommonPs4NpManagerScope *scope);
struct NpCommonPs4NpManagerScope *npCommonPs4TitleTokenScopeDestruct(struct NpCommonPs4NpManagerScope *scope);
void npCommonPs4TitleTokenScopeDelete(struct NpCommonPs4NpManagerScope *scope);
int npCommonPs4TitleTokenScopeCancel(struct NpCommonPs4NpManagerScope *scope);

__attribute__((noinline)) struct NpCommonPs4UserToken *SceNpCommonPs4_988DFD87(struct NpCommonPs4UserToken *token)
{
	token->vtable = NP_COMMON_PS4_VTABLE(g_userTokenVtable);
	sceClibMemset(token->data, 0, sizeof(token->data));
	return token;
}

struct NpCommonPs4UserToken *SceNpCommonPs4_31A1DB1B(struct NpCommonPs4UserToken *token)
{
	token->vtable = NP_COMMON_PS4_VTABLE(g_userTokenVtable);
	sceClibMemset(token->data, 0, sizeof(token->data));
	return token;
}

struct NpCommonPs4UserToken *SceNpCommonPs4_C61286CE(struct NpCommonPs4UserToken *token,
	const struct NpCommonPs4UserToken *source)
{
	token->vtable = NP_COMMON_PS4_VTABLE(g_userTokenVtable);
	sceClibMemcpy_safe(token->data, source->data, sizeof(token->data));
	return token;
}

struct NpCommonPs4UserToken *SceNpCommonPs4_91486660(struct NpCommonPs4UserToken *token,
	const struct NpCommonPs4UserToken *source)
{
	token->vtable = NP_COMMON_PS4_VTABLE(g_userTokenVtable);
	sceClibMemcpy_safe(token->data, source->data, sizeof(token->data));
	return token;
}

struct NpCommonPs4UserToken *SceNpCommonPs4_EA577C3D(struct NpCommonPs4UserToken *token, const void *data)
{
	token->vtable = NP_COMMON_PS4_VTABLE(g_userTokenVtable);
	sceClibMemcpy_safe(token->data, data, sizeof(token->data));
	return token;
}

struct NpCommonPs4UserToken *SceNpCommonPs4_244684E6(struct NpCommonPs4UserToken *token, const void *data)
{
	token->vtable = NP_COMMON_PS4_VTABLE(g_userTokenVtable);
	sceClibMemcpy_safe(token->data, data, sizeof(token->data));
	return token;
}

__attribute__((noinline)) struct NpCommonPs4UserToken *SceNpCommonPs4_78CC9FCD(struct NpCommonPs4UserToken *token)
{
	token->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return token;
}

int SceNpCommonPs4_E012D58B(struct NpCommonPs4UserToken *token)
{
	token->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(token);
}

struct NpCommonPs4UserToken *SceNpCommonPs4_1109C56A(struct NpCommonPs4UserToken *token)
{
	token->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return token;
}

void *SceNpCommonPs4_EF60709C(struct NpCommonPs4UserToken *token)
{
	return sceClibMemset(token->data, 0, sizeof(token->data));
}

int SceNpCommonPs4_4253A7FB(const struct NpCommonPs4UserToken *token)
{
	int empty;
	char zero[264];

	sceClibMemset(zero, 0, sizeof(zero));
	empty = sceClibMemcmp(token->data, zero, sizeof(zero)) == 0;
	return empty;
}

int SceNpCommonPs4_191AEA9E(struct NpCommonPs4Handle *handle, struct NpCommonPs4OnlineId *onlineId,
	struct NpCommonPs4UserToken *token)
{
	struct NpCommonPs4NpManagerScope scope;
	SceInt32 userId;
	SceInt32 scopeId;
	int result;
	int tokenResult;

	userId = 0;
	result = SceNpManager_4110B787(&userId);
	scopeId = userId;
	if (userId >= 0) {
		SceNpCommonPs4_5BBB1ABA(&scope.scope);
		scope.id = scopeId;
		scope.scope.cancelable.vtable = NP_COMMON_PS4_VTABLE(g_userTokenScopeVtable);
		result = SceNpCommonPs4_DDFD56E9(&scope.scope, handle, 0, 0, 0);
		if (result >= 0) {
			tokenResult = SceNpManager_FDE78C74(userId, token->data);
			result = SceNpCommonPs4_3D0FA7C9(&scope.scope, tokenResult, 0, 0, 0);
			scope.scope.cancelable.vtable = NP_COMMON_PS4_VTABLE(g_userTokenScopeVtable);
			SceNpCommonPs4_101259AD(&scope.scope);
			if (result >= 0)
				result = 0;
		} else {
			scope.scope.cancelable.vtable = NP_COMMON_PS4_VTABLE(g_userTokenScopeVtable);
			SceNpCommonPs4_101259AD(&scope.scope);
		}
		if (userId != 0)
			SceNpManager_3907F96A(userId);
	}
	return result;
}

__attribute__((noinline)) struct NpCommonPs4TitleToken *SceNpCommonPs4_DC4787E9(struct NpCommonPs4TitleToken *token)
{
	token->vtable = NP_COMMON_PS4_VTABLE(g_titleTokenVtable);
	sceClibMemset(token->data, 0, sizeof(token->data));
	return token;
}

struct NpCommonPs4TitleToken *SceNpCommonPs4_6E17E491(struct NpCommonPs4TitleToken *token)
{
	token->vtable = NP_COMMON_PS4_VTABLE(g_titleTokenVtable);
	sceClibMemset(token->data, 0, sizeof(token->data));
	return token;
}

__attribute__((noinline)) struct NpCommonPs4TitleToken *SceNpCommonPs4_FD80FE40(struct NpCommonPs4TitleToken *token)
{
	token->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return token;
}

int SceNpCommonPs4_68D17964(struct NpCommonPs4TitleToken *token)
{
	token->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(token);
}

struct NpCommonPs4TitleToken *SceNpCommonPs4_31508709(struct NpCommonPs4TitleToken *token)
{
	token->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return token;
}

__attribute__((noinline)) const char *SceNpCommonPs4_80D16974(const struct NpCommonPs4TitleToken *token)
{
	return token->data;
}

int SceNpCommonPs4_9BC491E3(struct NpCommonPs4Handle *handle, const char *onlineId, struct NpCommonPs4TitleToken *token)
{
	struct NpCommonPs4NpManagerScope scope;
	SceInt32 requestId;
	int result;
	int tokenResult;

	result = SceNpCommonPs4_639086A3(0);
	requestId = result;
	if (result >= 0) {
		SceNpCommonPs4_5BBB1ABA(&scope.scope);
		scope.id = requestId;
		scope.scope.cancelable.vtable = NP_COMMON_PS4_VTABLE(g_titleTokenScopeVtable);
		tokenResult = SceNpCommonPs4_DDFD56E9(&scope.scope, handle, 0, 0, 0);
		if (tokenResult >= 0) {
			result = SceNpCommonPs4_13798894(requestId, onlineId, token->data, 0);
			tokenResult = SceNpCommonPs4_3D0FA7C9(&scope.scope, result, 0, 0, 0);
			scope.scope.cancelable.vtable = NP_COMMON_PS4_VTABLE(g_titleTokenScopeVtable);
			SceNpCommonPs4_101259AD(&scope.scope);
			if (tokenResult >= 0)
				tokenResult = 0;
		} else {
			scope.scope.cancelable.vtable = NP_COMMON_PS4_VTABLE(g_titleTokenScopeVtable);
			SceNpCommonPs4_101259AD(&scope.scope);
		}
		if (requestId != -1)
			SceNpCommonPs4_530CAFDA(requestId);
		result = tokenResult;
	}
	return result;
}

struct NpCommonPs4NpManagerScope *npCommonPs4UserTokenScopeDestruct(struct NpCommonPs4NpManagerScope *scope)
{
	scope->scope.cancelable.vtable = NP_COMMON_PS4_VTABLE(g_userTokenScopeVtable);
	SceNpCommonPs4_101259AD(&scope->scope);
	return scope;
}

void npCommonPs4UserTokenScopeDelete(struct NpCommonPs4NpManagerScope *scope)
{
	scope->scope.cancelable.vtable = NP_COMMON_PS4_VTABLE(g_userTokenScopeVtable);
	SceNpCommonPs4_101259AD(&scope->scope);
	operator delete(scope);
}

int npCommonPs4UserTokenScopeCancel(struct NpCommonPs4NpManagerScope *scope)
{
	return SceNpManager_737B2820(scope->id);
}

struct NpCommonPs4NpManagerScope *npCommonPs4TitleTokenScopeDestruct(struct NpCommonPs4NpManagerScope *scope)
{
	scope->scope.cancelable.vtable = NP_COMMON_PS4_VTABLE(g_titleTokenScopeVtable);
	SceNpCommonPs4_101259AD(&scope->scope);
	return scope;
}

void npCommonPs4TitleTokenScopeDelete(struct NpCommonPs4NpManagerScope *scope)
{
	scope->scope.cancelable.vtable = NP_COMMON_PS4_VTABLE(g_titleTokenScopeVtable);
	SceNpCommonPs4_101259AD(&scope->scope);
	operator delete(scope);
}

int npCommonPs4TitleTokenScopeCancel(struct NpCommonPs4NpManagerScope *scope)
{
	return SceNpCommonPs4_68D08C92(scope->id);
}

const void *g_userTokenVtable[] = {		/* 0x8102aae8 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_78CC9FCD,
	(const void *)SceNpCommonPs4_E012D58B,
};

const void *g_userTokenScopeVtable[] = {	/* 0x8102aaf8 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_A6C7D48D,
	(const void *)npCommonPs4UserTokenScopeDestruct,
	(const void *)npCommonPs4UserTokenScopeDelete,
	(const void *)npCommonPs4UserTokenScopeCancel,
};

const void *g_titleTokenVtable[] = {		/* 0x8102ab10 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_FD80FE40,
	(const void *)SceNpCommonPs4_68D17964,
};

const void *g_titleTokenScopeVtable[] = {	/* 0x8102ab20 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_A6C7D48D,
	(const void *)npCommonPs4TitleTokenScopeDestruct,
	(const void *)npCommonPs4TitleTokenScopeDelete,
	(const void *)npCommonPs4TitleTokenScopeCancel,
};

}
