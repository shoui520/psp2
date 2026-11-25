/* SQLite 3.7.6.3 + PSP2 VFS configured with VideoSearchEmpr's mspace allocator. */

#include "video_search_empr.h"
#include <kernel/threadmgr.h>

#define sce_paf_malloc  vseMalloc
#define sce_paf_realloc vseRealloc
#define sce_paf_free    vseFree
#define sce_paf_memcpy  vseMemcpy
#define sce_paf_memset  vseMemset
#define sce_paf_memcmp  vseMemcmp
#define sce_paf_strcmp  vseStrcmp
#define sce_paf_strncmp vseStrncmp
#define sce_paf_strlen  vseStrlen

#include "sqlite3.c"
#include "os_psp2.c"
