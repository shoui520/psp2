#ifndef PAF_URL_URL_H
#define PAF_URL_URL_H

#include "../std/string.h"

typedef struct Url Url;

#define UrlIsValid ScePafMisc_0E846E5C
#define UrlGetSchemeString ScePafMisc_4D885C6E
#define UrlGetHostString ScePafMisc_0ACE5EBA
#define UrlGetPort ScePafMisc_D00CCC22
#define UrlGetUserString ScePafMisc_DD45D368
#define UrlGetPasswordString ScePafMisc_BF130361
#define UrlGetPathString ScePafMisc_1E5D19F2
#define UrlGetQueryString ScePafMisc_63446442
#define UrlGetAnchorString ScePafMisc_23F9DD51
#define UrlGetParamaterString ScePafMisc_ADCFF4C7

unsigned char UrlIsValid(const Url *self);
void UrlGetSchemeString(PafString *result, const Url *self);
void UrlGetHostString(PafString *result, const Url *self);
int32_t UrlGetPort(const Url *self);
void UrlGetUserString(PafString *result, const Url *self);
void UrlGetPasswordString(PafString *result, const Url *self);
void UrlGetPathString(PafString *result, const Url *self);
void UrlGetQueryString(PafString *result, const Url *self);
void UrlGetAnchorString(PafString *result, const Url *self);
void UrlGetParamaterString(PafString *result, const Url *self);

#endif
