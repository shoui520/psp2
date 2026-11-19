/*
 * libSceFt2's exports beyond FreeType's own API (vs0:sys/external/libSceFt2.suprx). The FreeType
 * functions are declared by FreeType's headers (contrib/freetype-7f316f22d/include).
 */
#ifndef LIBSCEFT2_LIBSCEFT2_H
#define LIBSCEFT2_LIBSCEFT2_H

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYSTEM_H

/* makes the lightweight mutex the library's file access takes ("SceLibFt2IoMutex"), once */
void __sceLibFt2SetLibCHook(void);
void __sceLibFt2DoneLibCHook(void);

/* a FreeType memory object for user, itself allocated with allocSelf(user, sizeof) */
FT_Memory SceFt2_04DCBE87(void *user, void *(*allocSelf)(void *user, long size),
	FT_Alloc_Func alloc, FT_Realloc_Func realloc, FT_Free_Func free);

#endif
