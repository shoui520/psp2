/*
 * ctype.h: the SDK's character classes, with Sony's declarations (SDK Release 03.570.011,
 * target/include/ctype.h). The macro forms, which read the user-mode C library's class tables,
 * come with the user-mode SDK headers; the kernel classifies with SceSysclib's
 * look_ctype_table (sysmem/ctype.h).
 */
#ifndef _CTYPE
#define _CTYPE

#ifndef EOF
#define EOF		(-1)
#endif

#ifdef __cplusplus
extern "C" {
#endif

int isalnum(int c);
int isalpha(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);
int tolower(int c);
int toupper(int c);

#ifdef __cplusplus
}
#endif

#endif /* _CTYPE */
