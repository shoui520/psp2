/*
 * The C library's thread-local data: each object is a slot of the calling thread's TLS area
 * (sceKernelGetTLSAddr).
 */
#include <errno.h>
#include <stddef.h>
#include <wchar.h>
#include <libkernel/libkernel.h>

#include "libc_internal.h"
#include "xtime.h"

/* the TLS slots */
#define TLS_ASCTIME_BUF		128	/* asctime's result */
#define TLS_ERRNO		136	/* errno */
#define TLS_MBRLEN_STATE	152	/* mbrlen's shift state */
#define TLS_MBRTOWC_STATE	160	/* mbrtowc's shift state */
#define TLS_MBSRTOWCS_STATE	168	/* mbsrtowcs's shift state */
#define TLS_MBTOWC_STATE	176	/* mbtowc's shift state */
#define TLS_WCRTOMB_STATE	184	/* wcrtomb's shift state */
#define TLS_WCTOMB_S_STATE	188	/* wctomb_s's shift state */
#define TLS_WCSRTOMBS_STATE	192	/* wcsrtombs's shift state */
#define TLS_WCTOMB_STATE	195	/* wctomb's shift state */
#define TLS_STRERROR_BUF	197	/* strerror's result */
#define TLS_TM_BUF		202	/* gmtime's and localtime's result */
#define TLS_STRTOK_PTR		211	/* strtok's position */
#define TLS_JMPBUF_GUARD	6	/* the key setjmp scrambles a jmp_buf with */
#define TLS_PTR_GUARD		7	/* the key handler pointers are scrambled with */
#define TLS_BUF			221	/* a buffer for requests of up to TLS_BUF_SIZE bytes */
#define TLS_BUF_SIZE		0x88

static int errnoFallback;	/* errno when the thread has no TLS area */

int *_sceLibcErrnoLoc(void)
{
	int *p = sceKernelGetTLSAddr(TLS_ERRNO);

	if (p == NULL) {
		errnoFallback = EDOM;
		return &errnoFallback;
	}
	return p;
}

char *_Getasctimebuf(void)
{
	return sceKernelGetTLSAddr(TLS_ASCTIME_BUF);
}

mbstate_t *_Getmbrlenstate(void)
{
	return sceKernelGetTLSAddr(TLS_MBRLEN_STATE);
}

mbstate_t *_Getmbrtowcstate(void)
{
	return sceKernelGetTLSAddr(TLS_MBRTOWC_STATE);
}

mbstate_t *_Getmbsrtowcsstate(void)
{
	return sceKernelGetTLSAddr(TLS_MBSRTOWCS_STATE);
}

mbstate_t *_Getmbtowcstate(void)
{
	return sceKernelGetTLSAddr(TLS_MBTOWC_STATE);
}

mbstate_t *_Getwcrtombstate(void)
{
	return sceKernelGetTLSAddr(TLS_WCRTOMB_STATE);
}

mbstate_t *_Getwcsrtombsstate(void)
{
	return sceKernelGetTLSAddr(TLS_WCSRTOMBS_STATE);
}

mbstate_t *_Getwctombstate(void)
{
	return sceKernelGetTLSAddr(TLS_WCTOMB_STATE);
}

mbstate_t *_Getwctomb_sstate(void)
{
	return sceKernelGetTLSAddr(TLS_WCTOMB_S_STATE);
}

char *_Getstrerrorbuf(void)
{
	return sceKernelGetTLSAddr(TLS_STRERROR_BUF);
}

struct tm *_Gettmbuf(void)
{
	return sceKernelGetTLSAddr(TLS_TM_BUF);
}

char **_Getstrtokptr(void)
{
	return sceKernelGetTLSAddr(TLS_STRTOK_PTR);
}

unsigned int _Getjmpbufguard(void)
{
	return *(unsigned int *)sceKernelGetTLSAddr(TLS_JMPBUF_GUARD);
}

unsigned int _Getptrguard(void)
{
	return *(unsigned int *)sceKernelGetTLSAddr(TLS_PTR_GUARD);
}

void *_Gettlsbuf(size_t size)
{
	if (TLS_BUF_SIZE < size)
		return NULL;
	return sceKernelGetTLSAddr(TLS_BUF);
}
