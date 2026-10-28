#ifndef PAF_CACHE_CACHE_MANAGER_H
#define PAF_CACHE_CACHE_MANAGER_H

#include <stdint.h>
#include <stddef.h>

typedef struct CacheManager CacheManager;

typedef struct CacheManagerNode {
	struct CacheManagerNode *prev;
	struct CacheManagerNode *next;
	CacheManager *val;
} CacheManagerNode;

typedef struct CacheManagerList {
	CacheManagerNode *root_node;
	int32_t count;
	unsigned char alloc_value;
	unsigned char alloc_node;
} CacheManagerList;

typedef char CacheManagerNodeSizeCheck[sizeof(CacheManagerNode) == 12 ? 1 : -1];
typedef char CacheManagerListSizeCheck[sizeof(CacheManagerList) == 12 ? 1 : -1];

#define CacheManagerCacheList ScePafMisc_EF0AD323
extern CacheManagerList CacheManagerCacheList;

#define CacheManagerGetCacheManagerCount ScePafMisc_DC369733
size_t CacheManagerGetCacheManagerCount(void);

CacheManagerList *CacheManagerListConstruct(CacheManagerList *self,
					 const void *allocator);
CacheManagerList *CacheManagerListDestroy(CacheManagerList *self);
void CacheManagerInitialize(void);

#endif
