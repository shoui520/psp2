/*
 * string.h: the SDK's string functions, with Sony's declarations (SDK Release 03.570.011,
 * target/include/string.h). The kernel's SceSysclib exports more of them (strnlen, strlcpy,
 * ...); sysmem/sysclib.h declares those as Sysclib defines them.
 */
#ifndef _STRING
#define _STRING

#ifndef NULL
#define NULL		0
#endif

#if !defined(_SIZE_T) && !defined(_SIZET) && !defined(_SIZE_T_DEFINED) && \
    !defined(_BSD_SIZE_T_DEFINED_)
#define _SIZE_T
#define _SIZET
#define _BSD_SIZE_T_DEFINED_
#define _SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

int memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void *__restrict s1, const void *__restrict s2, size_t n);
void *memset(void *s, int c, size_t n);
char *strcat(char *__restrict s1, const char *__restrict s2);
int strcmp(const char *s1, const char *s2);
char *strcpy(char *__restrict s1, const char *__restrict s2);
size_t strlen(const char *s);
void *memmove(void *s1, const void *s2, size_t n);
int strcoll(const char *s1, const char *s2);
size_t strcspn(const char *s1, const char *s2);
char *strerror(int errcode);
char *strncat(char *__restrict s1, const char *__restrict s2, size_t n);
int strncmp(const char *s1, const char *s2, size_t n);
char *strncpy(char *__restrict s1, const char *__restrict s2, size_t n);
size_t strspn(const char *s1, const char *s2);
char *strtok(char *__restrict s1, const char *__restrict s2);
size_t strxfrm(char *__restrict s1, const char *__restrict s2, size_t n);

/* extensions */
char *strdup(const char *s);
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
char *strtok_r(char *s, const char *sep, char **last);

char *strchr(const char *s, int c);
char *strpbrk(const char *s1, const char *s2);
char *strrchr(const char *s, int c);
char *strstr(const char *s1, const char *s2);
void *memchr(const void *s, int c, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* _STRING */
