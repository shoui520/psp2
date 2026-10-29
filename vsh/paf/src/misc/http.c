/* HTTP adapters operating on wrapped request and connection handles. */
#include <stdint.h>
#include <libhttp.h>

typedef struct PafHttpHandle {
	uint32_t reserved[3];
	int32_t id;
} PafHttpHandle;

typedef struct PafHttpRequest {
	uint8_t reserved[76];
	int32_t id;
} PafHttpRequest;

typedef struct PafHttpRequestOwner {
	PafHttpRequest *request;
} PafHttpRequestOwner;

extern int32_t SceHttp_43A7CC70(int32_t id, uint32_t flags);
extern int32_t SceHttp_FD76E1FB(int32_t id, uint32_t option);
extern int32_t SceHttp_F580D304(int32_t id, uint32_t a, uint32_t b, uint32_t c);
extern int32_t SceHttp_EA61662F(int32_t id, uint32_t a, uint32_t b, uint32_t c);
extern int32_t SceHttp_27071691(int32_t id, uint32_t a, uint32_t b, uint32_t c);
extern int32_t SceHttp_9CA58B99(int32_t id, uint32_t a, uint32_t b, uint32_t c);
extern int32_t SceHttp_F30073EC(int32_t id, uint32_t flags);
extern int32_t SceHttp_6EAD73EB(int32_t id, int32_t *state);

int32_t ScePafMisc_23F9BC0A(const PafHttpRequestOwner *owner, uint32_t flags)
{
	return SceHttp_43A7CC70(owner->request->id, flags);
}

int32_t ScePafMisc_AA992DF5(const PafHttpRequestOwner *owner, uint32_t option)
{
	return SceHttp_FD76E1FB(owner->request->id, option);
}

int32_t ScePafMisc_12DC2F23(const PafHttpHandle *owner, uint32_t a,
			    uint32_t b, uint32_t c)
{
	return SceHttp_F580D304(owner->id, a, b, c);
}

int32_t ScePafMisc_7EA133E0(const PafHttpHandle *owner, uint32_t a,
			    uint32_t b, uint32_t c)
{
	return SceHttp_EA61662F(owner->id, a, b, c);
}

int32_t ScePafMisc_807E22C4(const PafHttpHandle *owner, uint32_t a,
			    uint32_t b, uint32_t c)
{
	return SceHttp_27071691(owner->id, a, b, c);
}

int32_t ScePafMisc_CECA5E52(const PafHttpHandle *owner, uint32_t a,
			    uint32_t b, uint32_t c)
{
	return SceHttp_9CA58B99(owner->id, a, b, c);
}

int32_t ScePafMisc_71F59160(const PafHttpHandle *owner, uint8_t enabled)
{
	return sceHttpSetAutoRedirect(owner->id, !!enabled);
}

int32_t ScePafMisc_D3E4B405(const PafHttpHandle *owner,
			    const char *name, const char *value)
{
	int32_t result = sceHttpAddRequestHeader(owner->id, name, value, 0);

	return result < 0 ? result : 0;
}

int32_t ScePafMisc_5BF60DE9(const PafHttpHandle *owner, uint32_t options)
{
	uint32_t flags = ((options >> 1) & 1U) |
			((options >> 1) & 2U) |
			((options >> 1) & 4U);

	return SceHttp_F30073EC(owner->id, flags);
}

int32_t ScePafMisc_07A54D87(const PafHttpHandle *owner, uint8_t *active)
{
	int32_t state = 0;
	int32_t result = SceHttp_6EAD73EB(owner->id, &state);

	if (!result && state == 1)
		*active = 1;
	return result;
}
