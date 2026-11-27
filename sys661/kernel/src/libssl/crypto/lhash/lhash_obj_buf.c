/*
 * SceSsl / sceSsl_Module (ePSP 6.61) - BUF_MEM, CRYPTO_EX_DATA, LHASH, OBJ_NAME, OBJ_DAT.
 *
 * Implements functions:
 *   - [274..276] (0x012588..0x0126e7): buffer.c (BUF_MEM)
 *   - [287..289] (0x012d98..0x012f63): ex_data.c (CRYPTO_EX_DATA)
 *   - [290..300] (0x012f64..0x013787): lhash.c (LHASH dynamic hash table)
 *   - [309..316] (0x0138e4..0x013d9b): o_names.c (OBJ_NAME registry)
 *   - [317..325] (0x013d9c..0x0140ef): obj_dat.c (NID/SN/LN/OID tables)
 */
#include "../../libssl_internal.h"
#include "obj_dat_table.h"

typedef struct SslObjNameEntry {
    int type;
    int alias;
    const char *name;
    const void *value;
} SslObjNameEntry;

static SslLhash *g_objNamesHash = NULL;
static SslStack *g_objNameCmpFuncs = NULL;
static SslStack *g_objNameHashFuncs = NULL;
static SslStack *g_objNameFreeFuncs = NULL;
static int g_objCleanupType = 0;

/* -------------------------------------------------------------------------
 * buffer.c (BUF_MEM) [274..276]
 * ------------------------------------------------------------------------- */

/* [274] 0x012588 */
LIBSSL_NOINLINE SslBufMem *sslBufMemNew(void)
{
    SslBufMem *buf = (SslBufMem *)sceSsl_lib_0861D934(sizeof(SslBufMem));
    if (buf == NULL) {
        sceSsl_lib_9CDCA0D7(7, 101, 33, NULL, 0);
        return NULL;
    }
    buf->length = 0;
    buf->data = NULL;
    buf->maxLen = 0;
    return buf;
}

/* [275] 0x0125e0 */
LIBSSL_NOINLINE void sslBufMemFree(SslBufMem *buf)
{
    if (buf == NULL) {
        return;
    }
    if (buf->data != NULL) {
        sslMemset(buf->data, 0, (u32)buf->maxLen);
        sceSsl_lib_5E5C873A(buf->data);
    }
    sceSsl_lib_5E5C873A(buf);
}

/* [276] 0x01262c */
LIBSSL_NOINLINE int sslBufMemGrow(SslBufMem *buf, int newLen)
{
    if (buf->length >= newLen) {
        buf->length = newLen;
        return newLen;
    }
    if (buf->maxLen >= newLen) {
        sslMemset(buf->data + buf->length, 0, (u32)(newLen - buf->length));
        buf->length = newLen;
        return newLen;
    }

    {
        int allocLen = newLen + 16;
        u8 *nextMem;

        if (buf->data != NULL) {
            nextMem = (u8 *)sslCryptoRealloc(buf->data, (u32)allocLen);
        } else {
            nextMem = (u8 *)sceSsl_lib_0861D934((u32)allocLen);
        }
        if (nextMem == NULL) {
            sceSsl_lib_9CDCA0D7(7, 100, 33, NULL, 0);
            return 0;
        }
        buf->data = nextMem;
        buf->maxLen = allocLen;
        buf->length = newLen;
        return newLen;
    }
}

/* -------------------------------------------------------------------------
 * ex_data.c (CRYPTO_EX_DATA) [287..289]
 * ------------------------------------------------------------------------- */

/* [287] 0x012d98 */
LIBSSL_NOINLINE void *sslCryptoGetExData(const SslCryptoExData *exData, int idx)
{
    if (exData->stack == NULL || idx >= exData->stack->num) {
        return NULL;
    }
    return exData->stack->data[idx];
}

/* [288] 0x012dc8 */
LIBSSL_NOINLINE void sslCryptoFreeExData(SslStack *funcs, void *ownerObj, SslCryptoExData *exData)
{
    if (funcs != NULL) {
        int count = funcs->num;
        int idx;
        for (idx = 0; idx < count; idx++) {
            SslCryptoExDataFuncs *entry = (SslCryptoExDataFuncs *)funcs->data[idx];
            if (entry != NULL && entry->freeFunc != NULL) {
                void *val = sslCryptoGetExData(exData, idx);
                entry->freeFunc(ownerObj, val, exData, idx, entry->argLong, entry->argPtr);
            }
        }
    }
    if (exData->stack != NULL) {
        sslSkFree(exData->stack);
        exData->stack = NULL;
    }
}

/* [289] 0x012ea0 */
LIBSSL_NOINLINE void sslCryptoNewExData(SslStack *funcs, void *ownerObj, SslCryptoExData *exData)
{
    exData->stack = NULL;
    if (funcs != NULL) {
        int count = funcs->num;
        int idx;
        for (idx = 0; idx < count; idx++) {
            SslCryptoExDataFuncs *entry = (SslCryptoExDataFuncs *)funcs->data[idx];
            if (entry != NULL && entry->newFunc != NULL) {
                void *val = sslCryptoGetExData(exData, idx);
                entry->newFunc(ownerObj, val, exData, idx, entry->argLong, entry->argPtr);
            }
        }
    }
}

/* -------------------------------------------------------------------------
 * lhash.c (LHASH dynamic hash table) [290..300]
 * ------------------------------------------------------------------------- */

/* [290] 0x012f64 */
LIBSSL_NOINLINE SslLhash *sslLhNew(SslLhHashFn hashFn, SslLhCmpFn cmpFn)
{
    SslLhash *lh;
    u32 idx;

    lh = (SslLhash *)sceSsl_lib_0861D934(sizeof(SslLhash));
    if (lh == NULL) {
        return NULL;
    }
    lh->buckets = (SslLhashNode **)sceSsl_lib_0861D934(16U * sizeof(SslLhashNode *));
    if (lh->buckets == NULL) {
        sceSsl_lib_5E5C873A(lh);
        return NULL;
    }
    for (idx = 0; idx < 16U; idx++) {
        lh->buckets[idx] = NULL;
    }

    lh->comp = (cmpFn != NULL) ? cmpFn : (SslLhCmpFn)sslStrcmp;
    lh->hash = (hashFn != NULL) ? hashFn : (SslLhHashFn)sslLhStrHash;
    lh->numNodes = 8;
    lh->numAllocNodes = 16;
    lh->splitPos = 0;
    lh->pmax = 8;
    lh->upLoad = 512;
    lh->downLoad = 256;
    lh->numItems = 0;
    lh->numExpands = 0;
    lh->numExpandReallocs = 0;
    lh->numContracts = 0;
    lh->numContractReallocs = 0;
    lh->numHashCalls = 0;
    lh->numCompCalls = 0;
    lh->numInsert = 0;
    lh->numReplace = 0;
    lh->numDelete = 0;
    lh->numNoDelete = 0;
    lh->numRetrieve = 0;
    lh->numRetrieveMiss = 0;
    lh->numHashComps = 0;
    lh->error = 0;
    return lh;
}

/* [291] 0x013078 */
LIBSSL_NOINLINE void sslLhFree(SslLhash *lh)
{
    u32 idx;

    for (idx = 0; idx < lh->numNodes; idx++) {
        SslLhashNode *node = lh->buckets[idx];
        while (node != NULL) {
            SslLhashNode *next = node->next;
            sceSsl_lib_5E5C873A(node);
            node = next;
        }
    }
    sceSsl_lib_5E5C873A(lh->buckets);
    sceSsl_lib_5E5C873A(lh);
}

/* [292] 0x013100 */
LIBSSL_NOINLINE void *sslLhInsert(SslLhash *lh, void *value)
{
    u32 hashVal = 0;
    SslLhashNode **slot;
    SslLhashNode *node;

    lh->error = 0;
    if ((lh->numItems * 256U) / lh->numNodes >= lh->upLoad) {
        sslLhExpand(lh);
    }

    slot = sslLhGetBucketSlot(lh, value, &hashVal);
    if (*slot != NULL) {
        void *prev = (*slot)->value;
        (*slot)->value = value;
        lh->numReplace++;
        return prev;
    }

    node = (SslLhashNode *)sceSsl_lib_0861D934(sizeof(SslLhashNode));
    if (node == NULL) {
        lh->error++;
        return NULL;
    }
    node->value = value;
    node->next = NULL;
    node->hash = hashVal;
    *slot = node;
    lh->numInsert++;
    lh->numItems++;
    return NULL;
}

/* [293] 0x013200 */
LIBSSL_NOINLINE void *sslLhDelete(SslLhash *lh, const void *value)
{
    u32 hashVal = 0;
    SslLhashNode **slot;
    SslLhashNode *node;
    void *removed;

    lh->error = 0;
    slot = sslLhGetBucketSlot(lh, value, &hashVal);
    node = *slot;
    if (node == NULL) {
        lh->numNoDelete++;
        return NULL;
    }

    removed = node->value;
    *slot = node->next;
    sceSsl_lib_5E5C873A(node);
    lh->numDelete++;
    lh->numItems--;

    if (lh->numNodes > 16U && lh->downLoad >= (lh->numItems * 256U) / lh->numNodes) {
        sslLhContract(lh);
    }
    return removed;
}

/* [294] 0x0132c0 */
LIBSSL_NOINLINE void *sslLhRetrieve(SslLhash *lh, const void *value)
{
    u32 hashVal = 0;
    SslLhashNode **slot;

    lh->error = 0;
    slot = sslLhGetBucketSlot(lh, value, &hashVal);
    if (*slot == NULL) {
        lh->numRetrieveMiss++;
        return NULL;
    }
    lh->numRetrieve++;
    return (*slot)->value;
}

/* [295] 0x013320 */
LIBSSL_NOINLINE void sslLhDoAll(SslLhash *lh, SslLhDoAllFn cb)
{
    sslLhDoAllArg(lh, (SslLhDoAllArgFn)cb, NULL);
}

/* [296] 0x01333c */
LIBSSL_NOINLINE void sslLhDoAllArg(SslLhash *lh, SslLhDoAllArgFn cb, void *userArg)
{
    int idx;

    for (idx = (int)lh->numNodes - 1; idx >= 0; idx--) {
        SslLhashNode *node = lh->buckets[idx];
        while (node != NULL) {
            SslLhashNode *next = node->next;
            cb(node->value, userArg);
            node = next;
        }
    }
}

/* [297] 0x0133cc */
LIBSSL_NOINLINE void sslLhExpand(SslLhash *lh)
{
    u32 splitPos = lh->splitPos;
    u32 pmax = lh->pmax;
    u32 allocCount = lh->numAllocNodes;
    SslLhashNode **oldSlot = &lh->buckets[splitPos];
    SslLhashNode **newSlot = &lh->buckets[splitPos + pmax];
    SslLhashNode *node;

    *newSlot = NULL;
    lh->numNodes++;
    lh->numExpands++;
    lh->splitPos = splitPos + 1U;

    for (node = *oldSlot; node != NULL; node = *oldSlot) {
        if ((node->hash % allocCount) != splitPos) {
            *oldSlot = node->next;
            node->next = *newSlot;
            *newSlot = node;
        } else {
            oldSlot = &node->next;
        }
    }

    if (lh->splitPos >= pmax) {
        u32 nextAlloc = allocCount * 2U;
        SslLhashNode **grown = (SslLhashNode **)sslCryptoRealloc(
            lh->buckets, nextAlloc * sizeof(SslLhashNode *));
        if (grown == NULL) {
            lh->splitPos = 0;
            lh->error++;
            return;
        }
        {
            u32 idx;
            for (idx = lh->numAllocNodes; idx < nextAlloc; idx++) {
                grown[idx] = NULL;
            }
        }
        lh->buckets = grown;
        lh->pmax = lh->numAllocNodes;
        lh->numAllocNodes = nextAlloc;
        lh->numExpandReallocs++;
        lh->splitPos = 0;
    }
}

/* [298] 0x013510 */
LIBSSL_NOINLINE void sslLhContract(SslLhash *lh)
{
    SslLhashNode *tailList = lh->buckets[lh->splitPos + lh->pmax - 1U];
    SslLhashNode *headNode;

    lh->buckets[lh->splitPos + lh->pmax - 1U] = NULL;
    if (lh->splitPos == 0) {
        u32 halfAlloc = lh->numAllocNodes >> 1;
        SslLhashNode **shrunk = (SslLhashNode **)sslCryptoRealloc(
            lh->buckets, halfAlloc * sizeof(SslLhashNode *));
        if (shrunk == NULL) {
            lh->error++;
            return;
        }
        lh->numContractReallocs++;
        lh->numAllocNodes = halfAlloc;
        lh->pmax >>= 1;
        lh->splitPos = lh->pmax - 1U;
        lh->buckets = shrunk;
    } else {
        lh->splitPos--;
    }

    lh->numNodes--;
    lh->numContracts++;

    headNode = lh->buckets[lh->splitPos];
    if (headNode == NULL) {
        lh->buckets[lh->splitPos] = tailList;
    } else {
        while (headNode->next != NULL) {
            headNode = headNode->next;
        }
        headNode->next = tailList;
    }
}

/* [299] 0x01361c */
LIBSSL_NOINLINE SslLhashNode **sslLhGetBucketSlot(SslLhash *lh, const void *value, u32 *outHash)
{
    u32 hashVal;
    u32 bucketIdx;
    SslLhashNode **slot;
    SslLhashNode *node;

    hashVal = lh->hash(value);
    lh->numHashCalls++;
    *outHash = hashVal;

    bucketIdx = hashVal % lh->pmax;
    if (bucketIdx < lh->splitPos) {
        bucketIdx = hashVal % lh->numAllocNodes;
    }

    slot = &lh->buckets[bucketIdx];
    node = *slot;
    while (node != NULL) {
        lh->numHashComps++;
        if (node->hash == hashVal) {
            lh->numCompCalls++;
            if (lh->comp(node->value, value) == 0) {
                break;
            }
        }
        slot = &node->next;
        node = node->next;
    }
    return slot;
}

/* [300] 0x013724 */
LIBSSL_NOINLINE u32 sslLhStrHash(const char *str)
{
    u32 hashVal = 0;
    u32 step = 0x100U;

    if (str == NULL || *str == '\0') {
        return 0;
    }

    while (*str != '\0') {
        u32 term = step | (u32)(signed char)(*str++);
        u32 rot = ((term >> 2) ^ term) & 0x0fU;
        if (rot != 0) {
            hashVal = (hashVal << rot) | (hashVal >> (32U - rot));
        }
        hashVal ^= term * term;
        step += 0x100U;
    }

    return (hashVal >> 16) ^ hashVal;
}

/* -------------------------------------------------------------------------
 * o_names.c (OBJ_NAME registry) [309..316]
 * ------------------------------------------------------------------------- */

/* [309] 0x0138e4 */
LIBSSL_NOINLINE int sslObjNameInit(void)
{
    if (g_objNamesHash != NULL) {
        return 1;
    }
    g_objNamesHash = sslLhNew((SslLhHashFn)sslObjNameHash, (SslLhCmpFn)sslObjNameCmp);
    return (g_objNamesHash != NULL);
}

/* [310] 0x013934 */
LIBSSL_NOINLINE int sslObjNameCmp(const void *lhsPtr, const void *rhsPtr)
{
    const SslObjNameEntry *lhs = (const SslObjNameEntry *)lhsPtr;
    const SslObjNameEntry *rhs = (const SslObjNameEntry *)rhsPtr;
    int diff = lhs->type - rhs->type;

    if (diff != 0) {
        return diff;
    }
    if (g_objNameCmpFuncs != NULL && lhs->type < g_objNameCmpFuncs->num) {
        SslObjNameCmpFn cmpFn = (SslObjNameCmpFn)g_objNameCmpFuncs->data[lhs->type];
        return cmpFn(lhs->name, rhs->name);
    }
    return sslStrcmp(lhs->name, rhs->name);
}

/* [311] 0x0139a8 */
LIBSSL_NOINLINE u32 sslObjNameHash(const void *entryPtr)
{
    const SslObjNameEntry *entry = (const SslObjNameEntry *)entryPtr;
    u32 hashVal;

    if (g_objNameHashFuncs != NULL && entry->type < g_objNameHashFuncs->num) {
        SslObjNameHashFn hashFn = (SslObjNameHashFn)g_objNameHashFuncs->data[entry->type];
        hashVal = hashFn(entry->name);
    } else {
        hashVal = sslLhStrHash(entry->name);
    }
    return hashVal ^ (u32)entry->type;
}

/* [312] 0x013a20 */
LIBSSL_NOINLINE const void *sslObjNameGet(const char *name, int type)
{
    SslObjNameEntry key;
    SslObjNameEntry *found;
    int noAlias;
    int depth = 0;

    if (name == NULL) {
        return NULL;
    }
    if (g_objNamesHash == NULL && !sslObjNameInit()) {
        return NULL;
    }

    noAlias = type & 0x8000;
    key.type = type & ~0x8000;
    key.alias = 0;
    key.name = name;
    key.value = NULL;

    for (;;) {
        depth++;
        found = (SslObjNameEntry *)sslLhRetrieve(g_objNamesHash, &key);
        if (found == NULL) {
            return NULL;
        }
        if (found->alias == 0 || noAlias != 0) {
            return found->value;
        }
        if (depth >= 11) {
            return NULL;
        }
        key.name = (const char *)found->value;
    }
}

/* [313] 0x013af0 */
LIBSSL_NOINLINE int sslObjNameAdd(const char *name, int type, const void *value)
{
    SslObjNameEntry *entry;
    SslObjNameEntry *replaced;

    if (g_objNamesHash == NULL && !sslObjNameInit()) {
        return 0;
    }

    entry = (SslObjNameEntry *)sceSsl_lib_0861D934(sizeof(SslObjNameEntry));
    if (entry == NULL) {
        return 0;
    }
    entry->type = type & ~0x8000;
    entry->alias = type & 0x8000;
    entry->name = name;
    entry->value = value;

    replaced = (SslObjNameEntry *)sslLhInsert(g_objNamesHash, entry);
    if (replaced != NULL) {
        if (g_objNameFreeFuncs != NULL && replaced->type < g_objNameFreeFuncs->num) {
            SslObjNameFreeFn freeFn = (SslObjNameFreeFn)g_objNameFreeFuncs->data[replaced->type];
            freeFn(replaced->name, replaced->type, replaced->value);
        }
        sceSsl_lib_5E5C873A(replaced);
        return 1;
    }
    return (g_objNamesHash->error == 0);
}

/* [314] 0x013bf4 */
LIBSSL_NOINLINE int sslObjNameRemove(const char *name, int type)
{
    SslObjNameEntry key;
    SslObjNameEntry *removed;

    if (g_objNamesHash == NULL) {
        return 0;
    }
    key.type = type & ~0x8000;
    key.alias = 0;
    key.name = name;
    key.value = NULL;

    removed = (SslObjNameEntry *)sslLhDelete(g_objNamesHash, &key);
    if (removed == NULL) {
        return 0;
    }
    if (g_objNameFreeFuncs != NULL && key.type < g_objNameFreeFuncs->num) {
        SslObjNameFreeFn freeFn = (SslObjNameFreeFn)g_objNameFreeFuncs->data[key.type];
        freeFn(removed->name, removed->type, removed->value);
    }
    sceSsl_lib_5E5C873A(removed);
    return 1;
}

/* [315] 0x013ca0 */
LIBSSL_NOINLINE void sslObjNameCleanupCallback(void *entryPtr)
{
    SslObjNameEntry *entry = (SslObjNameEntry *)entryPtr;
    if (g_objCleanupType < 0 || g_objCleanupType == entry->type) {
        sslObjNameRemove(entry->name, entry->type);
    }
}

/* [316] 0x013cdc */
LIBSSL_NOINLINE void sslObjNameCleanup(int type)
{
    u32 savedDownLoad;

    if (g_objNamesHash == NULL) {
        return;
    }
    savedDownLoad = g_objNamesHash->downLoad;
    g_objNamesHash->downLoad = 0;
    g_objCleanupType = type;
    sslLhDoAll(g_objNamesHash, sslObjNameCleanupCallback);

    if (type < 0) {
        sslLhFree(g_objNamesHash);
        sslSkFree(g_objNameHashFuncs);
        sslSkFree(g_objNameCmpFuncs);
        sslSkFree(g_objNameFreeFuncs);
        g_objNamesHash = NULL;
        g_objNameHashFuncs = NULL;
        g_objNameCmpFuncs = NULL;
        g_objNameFreeFuncs = NULL;
    } else {
        g_objNamesHash->downLoad = savedDownLoad;
    }
}

/* -------------------------------------------------------------------------
 * obj_dat.c (NID / SN / LN / OID tables) [317..325]
 * ------------------------------------------------------------------------- */

/* [317] 0x013d9c */
LIBSSL_NOINLINE void sslObjCleanup(void)
{
}

/* [318] 0x013da4 */
LIBSSL_NOINLINE SslAsn1Object *sslObjNidToObj(u32 nid)
{
    const SslAsn1Object *obj;

    if (nid >= 134U) {
        return NULL;
    }
    obj = &g_nidObjs[nid];
    if (nid != 0 && obj->nid == 0) {
        sceSsl_lib_9CDCA0D7(8, 103, 101, NULL, 0);
        return NULL;
    }
    return (SslAsn1Object *)obj;
}

/* [319] 0x013e10 */
LIBSSL_NOINLINE const char *sslObjNidToSn(u32 nid)
{
    const SslAsn1Object *obj;

    if (nid >= 134U) {
        return NULL;
    }
    obj = &g_nidObjs[nid];
    if (nid != 0 && obj->nid == 0) {
        sceSsl_lib_9CDCA0D7(8, 104, 101, NULL, 0);
        return NULL;
    }
    return obj->shortName;
}

/* [320] 0x013e84 */
LIBSSL_NOINLINE const char *sslObjNidToLn(u32 nid)
{
    const SslAsn1Object *obj;

    if (nid >= 134U) {
        return NULL;
    }
    obj = &g_nidObjs[nid];
    if (nid != 0 && obj->nid == 0) {
        sceSsl_lib_9CDCA0D7(8, 102, 101, NULL, 0);
        return NULL;
    }
    return obj->longName;
}

/* [321] 0x013ef8 */
LIBSSL_NOINLINE int sslObjObjToNid(const SslAsn1Object *obj)
{
    const SslAsn1Object *const *found;

    if (obj == NULL) {
        return 0;
    }
    if (obj->nid != 0) {
        return obj->nid;
    }
    found = (const SslAsn1Object *const *)sslObjBsearch(
        &obj, g_oidObjs, 106, (int)sizeof(const SslAsn1Object *), sslObjOidCmp);
    if (found == NULL) {
        return 0;
    }
    return (*found)->nid;
}

/* [322] 0x013f5c */
LIBSSL_NOINLINE int sslObjSnToNid(const char *shortName)
{
    SslAsn1Object keyObj;
    const SslAsn1Object *keyPtr = &keyObj;
    const SslAsn1Object *const *found;

    keyObj.shortName = shortName;
    found = (const SslAsn1Object *const *)sslObjBsearch(
        &keyPtr, g_snObjs, 105, (int)sizeof(const SslAsn1Object *), sslObjSnCmp);
    if (found == NULL) {
        return 0;
    }
    return (*found)->nid;
}

/* [323] 0x013fb0 */
LIBSSL_NOINLINE int sslObjOidCmp(const void *lhsPtr, const void *rhsPtr)
{
    const SslAsn1Object *lhs = *(const SslAsn1Object *const *)lhsPtr;
    const SslAsn1Object *rhs = *(const SslAsn1Object *const *)rhsPtr;
    int diff = lhs->length - rhs->length;

    if (diff != 0) {
        return diff;
    }
    return sslMemcmp(lhs->data, rhs->data, lhs->length);
}

/* [324] 0x013ff8 */
LIBSSL_NOINLINE const void *sslObjBsearch(const void *key, const void *basePtr, int elemCount,
                                          int elemSize, SslQsortCmpFn cmpFn)
{
    const u8 *baseBytes = (const u8 *)basePtr;
    int low = 0;
    int high = elemCount;

    if (elemCount == 0) {
        return NULL;
    }

    while (low < high) {
        int mid = (low + high) / 2;
        const u8 *elem = &baseBytes[mid * elemSize];
        int cmp = cmpFn(key, elem);
        if (cmp < 0) {
            high = mid;
        } else if (cmp > 0) {
            low = mid + 1;
        } else {
            return elem;
        }
    }
    return NULL;
}

/* [325] 0x0140c8 */
LIBSSL_NOINLINE int sslObjSnCmp(const void *lhsPtr, const void *rhsPtr)
{
    const SslAsn1Object *lhs = *(const SslAsn1Object *const *)lhsPtr;
    const SslAsn1Object *rhs = *(const SslAsn1Object *const *)rhsPtr;
    return sslStrcmp(lhs->shortName, rhs->shortName);
}
