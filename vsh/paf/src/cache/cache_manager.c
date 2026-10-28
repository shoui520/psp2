#include "cache_manager.h"
#include "cache_manager_impl.h"
#include "../std/string.h"
#include "../std/stdc++/atexit.h"

CacheManagerList CacheManagerCacheList;

size_t CacheManagerGetCacheManagerCount(void)
{
	size_t count;

	ScePafThread_F9FFA0BE(&CacheManagerImplRMutex);
	count = CacheManagerCacheList.count;
	ScePafThread_95F9C45F(&CacheManagerImplRMutex);
	return count;
}

CacheManagerList *CacheManagerListConstruct(CacheManagerList *self,
					 const void *allocator)
{
	CacheManagerNode *root;

	self->count = 0;
	root = PafNew(sizeof(*root));
	if (!root)
		ScePafStdc_D608246A();
	self->root_node = root;
	if (root) {
		root->prev = root;
		root->next = root;
	}
	return self;
}

CacheManagerList *CacheManagerListDestroy(CacheManagerList *self)
{
	CacheManagerNode *root = self->root_node;

	if (root) {
		CacheManagerNode *node = root->next;

		while (node != root) {
			CacheManagerNode *next = node->next;

			PafDelete(node);
			root = self->root_node;
			node = next;
		}
		PafDelete(root);
	}
	self->root_node = (CacheManagerNode *)-1;
	self->count = -1;
	return self;
}

void CacheManagerInitialize(void)
{
	extern void *__dso_handle;
	unsigned char allocator[4];

	CacheManagerListConstruct(&CacheManagerCacheList, allocator);
	ScePafStdc_C95732BB(&CacheManagerCacheList,
			  (void (*)(void *))CacheManagerListDestroy, &__dso_handle);
}
