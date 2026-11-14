/*
 * stdlib.h: the SDK's general utilities, with Sony's psp2 declarations (SDK Release 03.570.011,
 * target/include/stdlib.h and yvals.h). The macro forms of atoi, strtoul and the others, which
 * call the user-mode C library's internal converters, come with the user-mode SDK headers.
 */
#ifndef _STDLIB
#define _STDLIB

#ifndef NULL
#define NULL		0
#endif

#define EXIT_FAILURE	1
#define EXIT_SUCCESS	0
#define MB_CUR_MAX	((size_t)4)
#define RAND_MAX	0x7fffffff

#if !defined(_SIZE_T) && !defined(_SIZET) && !defined(_SIZE_T_DEFINED) && \
    !defined(_BSD_SIZE_T_DEFINED_)
#define _SIZE_T
#define _SIZET
#define _BSD_SIZE_T_DEFINED_
#define _SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

/* wchar_t is 16 bits on psp2; C++ has it built in */
#if !defined(__cplusplus) && !defined(_WCHART) && !defined(_WCHAR_T_DEFINED)
#define _WCHART
#define _WCHAR_T_DEFINED
typedef unsigned short wchar_t;
#endif

typedef struct {
	int quot;
	int rem;
} div_t;

typedef struct {
	long quot;
	long rem;
} ldiv_t;

typedef struct {
	long long quot;
	long long rem;
} lldiv_t;

/* the C library's heap */
struct malloc_managed_size {
	size_t max_system_size;
	size_t current_system_size;
	size_t max_inuse_size;
	size_t current_inuse_size;
	size_t reserved[4];
};

#ifdef __cplusplus
extern "C" {
#endif

void exit(int status) __attribute__((noreturn));
void _Exit(int status) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));

int abs(int i);
long labs(long i);
long long llabs(long long i);
div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
lldiv_t lldiv(long long numer, long long denom);

void *calloc(size_t nelem, size_t size);
void free(void *ptr);
void *malloc(size_t size);
void *realloc(void *ptr, size_t size);
void *reallocalign(void *ptr, size_t size_arg, size_t boundary);
void *memalign(size_t boundary, size_t size);
size_t malloc_usable_size(void *ptr);
int malloc_stats(struct malloc_managed_size *mmsize);
int malloc_stats_fast(struct malloc_managed_size *mmsize);

int mblen(const char *s, size_t n);
size_t mbstowcs(wchar_t *__restrict wcs, const char *__restrict s, size_t n);
int mbtowc(wchar_t *__restrict pwc, const char *__restrict s, size_t n);
size_t wcstombs(char *__restrict s, const wchar_t *__restrict wcs, size_t n);
int wctomb(char *s, wchar_t wchar);

int rand(void);
void srand(unsigned int seed);
int rand_r(unsigned *seed);
void lcong48(unsigned short param[7]);
unsigned short *seed48(unsigned short seed16v[3]);
void srand48(long seedval);
double drand48(void);
long lrand48(void);
long mrand48(void);
double erand48(unsigned short xsubi[3]);
long nrand48(unsigned short xsubi[3]);
long jrand48(unsigned short xsubi[3]);

int atexit(void (*func)(void));
void *bsearch(const void *key, const void *base, size_t nelem, size_t size,
	      int (*compar)(const void *, const void *));
typedef int _Cmpfun(const void *, const void *);
void qsort(void *base, size_t nelem, size_t size, _Cmpfun *cmp);

double atof(const char *s);
float atoff(const char *s);
int atoi(const char *s);
long atol(const char *s);
long long atoll(const char *s);
double strtod(const char *__restrict s, char **__restrict endptr);
float strtof(const char *__restrict s, char **__restrict endptr);
long double strtold(const char *__restrict s, char **__restrict endptr);
long strtol(const char *__restrict s, char **__restrict endptr, int base);
unsigned long strtoul(const char *__restrict s, char **__restrict endptr, int base);
long long strtoll(const char *__restrict s, char **__restrict endptr, int base);
unsigned long long strtoull(const char *__restrict s, char **__restrict endptr, int base);

int getargc(void);
char **getargv(void);
void sceLibcSetHeapInitError(unsigned int err_number);

/* the library's converters (the macro forms of atoi and the others call them) */
unsigned long _Stoul(const char *s, char **endptr, int base);
float _Stof(const char *s, char **endptr, long base);
double _Stod(const char *s, char **endptr, long base);
long double _Stold(const char *s, char **endptr, long base);
long long _Stoll(const char *s, char **endptr, int base);
unsigned long long _Stoull(const char *s, char **endptr, int base);

float _Stofx(const char *s, char **endptr, long base, int *perr);
double _Stodx(const char *s, char **endptr, long base, int *perr);
long double _Stoldx(const char *s, char **endptr, long base, int *perr);
long _Stolx(const char *s, char **endptr, int base, int *perr);
unsigned long _Stoulx(const char *s, char **endptr, int base, int *perr);
long long _Stollx(const char *s, char **endptr, int base, int *perr);
unsigned long long _Stoullx(const char *s, char **endptr, int base, int *perr);

size_t _Getmbcurmax(void);

/* the bounds-checking interfaces (C11 Annex K) */
#if __STDC_WANT_LIB_EXT1__
#define _EXT1_FAIL(mesg)	_Fail_s(mesg, sizeof(mesg))

int _Fail_s(const char *mesg, size_t size);

#if !defined(_ERRNO_T_DEFINED)
#define _ERRNO_T_DEFINED
typedef int errno_t;
#endif
#if !defined(_RSIZE_T_DEFINED)
#define _RSIZE_T_DEFINED
typedef size_t rsize_t;
#endif

typedef void (*constraint_handler_t)(const char *__restrict msg, void *__restrict ptr,
				     errno_t error);

constraint_handler_t set_constraint_handler_s(constraint_handler_t handler);
void abort_handler_s(const char *__restrict msg, void *__restrict ptr, errno_t error);
void ignore_handler_s(const char *__restrict msg, void *__restrict ptr, errno_t error);

typedef int _Cmpfun_s(const void *, const void *, void *);

void *bsearch_s(const void *key, const void *base, rsize_t nelem, rsize_t size,
		_Cmpfun_s *compar, void *context);
errno_t qsort_s(void *base, rsize_t nelem, rsize_t size, _Cmpfun_s *compar, void *context);

errno_t wctomb_s(int *__restrict status, char *__restrict s, rsize_t smax, wchar_t wc);
errno_t mbstowcs_s(size_t *__restrict retval, wchar_t *__restrict dst, rsize_t dstmax,
		   const char *__restrict src, rsize_t len);
errno_t wcstombs_s(size_t *__restrict retval, char *__restrict dst, rsize_t dstmax,
		   const wchar_t *__restrict src, rsize_t len);
#endif /* __STDC_WANT_LIB_EXT1__ */

#ifdef __cplusplus
}
#endif

#endif /* _STDLIB */
