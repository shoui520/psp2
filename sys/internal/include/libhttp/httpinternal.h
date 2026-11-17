/*
 * libhttp/httpinternal.h: SceHttp's exports with no Sony name that other modules call. File name
 * ours; the link names are SceHttp_<NID>.
 */
#ifndef LIBHTTP_HTTPINTERNAL_H
#define LIBHTTP_HTTPINTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* sets the default option 22 to VALUE (vitasdk: none) */
int SceHttp_855D5254(int value);

#ifdef __cplusplus
}
#endif

#endif
