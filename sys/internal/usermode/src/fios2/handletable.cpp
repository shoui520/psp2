/*
 * handletable.cpp: the op, file handle and directory handle tables. A handle is its unit's
 * index, the table (1 to 3) and the allocator's generation:
 * index | table << 14 | generation << 16, kept in the unit's first word.
 */
#include <libdbg.h>
#include "fios2_internal.h"

HandleTable s_handleTables[3] = {
	{ { "op", 0, 0, 0xffffffff, 0, 1 } },
	{ { "FH", 0, 0, 0xffffffff, 0, 1 } },
	{ { "DH", 0, 0, 0xffffffff, 0, 1 } },
};
HandleTable *s_pOpTable = &s_handleTables[0];
HandleTable *s_pFHTable = &s_handleTables[1];
HandleTable *s_pDHTable = &s_handleTables[2];

#define HANDLE_INDEX(h)		((h) & 0x3fff)
#define HANDLE_TABLE(h)		(((uint16_t)(h) >> 14) - 1)

/* which of s_handleTables a table is */
static inline int tableIndex(HandleTable *pTable)
{
	return ((uint8_t *)pTable - (uint8_t *)s_handleTables) / (int)sizeof(HandleTable);
}

int HandleTable::Init(SceFiosBuffer buffer, size_t alignment, size_t unitSize)
{
	return m_allocator.Init(buffer, 0x4000, unitSize, alignment);
}

int HandleTable::Term()
{
	m_allocator.Term();
	return SCE_OK;
}

SceFiosHandle HandleTable::Allocate()
{
	int index = m_allocator.Allocate(1);

	if (index != -1) {
		SceFiosHandle *pHandle = Unit(index);
		SceFiosHandle h = (index & 0x3fff) | ((m_allocator.m_generation & 0x7fff) << 16 |
						      ((tableIndex(this) + 1) & 3) << 14);

		*pHandle = h;
		return h;
	}
	return 0;
}

/* the unit of a handle this table allocated, or NULL */
void *HandleTable::Lookup(SceFiosHandle h)
{
	uint32_t index = HANDLE_INDEX(h);

	if (h > 0 && tableIndex(this) == HANDLE_TABLE(h) && m_allocator.m_initialized &&
	    m_allocator.IsAllocatedIndex(index, 1)) {
		SceFiosHandle *pHandle = Unit(index);

		if (*pHandle == h)
			return pHandle;
	}
	return NULL;
}

void HandleTable::Free(SceFiosHandle h)
{
	SceFiosHandle *pHandle = Unit(HANDLE_INDEX(h));

	if (*pHandle != h) {
		const char *pDump;

		sceFiosPrintf("FIOS: Unexpected error while deallocating a handle\n");
		sceFiosPrintf("FIOS: pHandle->h 0x%08x, h 0x%08x\n", *pHandle, h);
		if (s_pFHTable->Lookup(h) != NULL)
			pDump = sceFiosDebugDumpFH(h, NULL, 0);
		else if (s_pOpTable->Lookup(h) != NULL)
			pDump = sceFiosDebugDumpOp(h, NULL, 0);
		else if (s_pDHTable->Lookup(h) != NULL)
			pDump = sceFiosDebugDumpDH(h, NULL, 0);
		else {
			sceFiosPrintf("FIOS: could not find a handle matching h\n");
			goto check;
		}
		if (pDump != NULL)
			sceFiosPrintf("%s\n", pDump);
check:
		if (*pHandle != h &&
		    sceDbgAssertionHandler(FIOS_SOURCE_DIR "handletable.cpp", 111, true, "Fios",
					   "Assertion failed: %s\n", "pHandle->h == h"))
			SCE_BREAK();
	}
	*pHandle = 0;
	m_allocator.Free(HANDLE_INDEX(h), 1);
}

/* the unit of a handle's index, allocated or not */
void *HandleTable::GetUnit(SceFiosHandle h)
{
	uint32_t index = HANDLE_INDEX((uint16_t)h);

	if (tableIndex(this) == HANDLE_TABLE(h) && index < m_allocator.m_count)
		return Unit(index);
	return NULL;
}

/* the allocated handles, up to max, the rest of pHandles zeroed; the count allocated */
uint32_t HandleTable::GetAll(SceFiosHandle *pHandles, uint32_t max)
{
	uint32_t found = 0, index;
	SceFiosHandle *p = pHandles;

	if (pHandles == NULL)
		max = 0;
	for (index = 0; index < m_allocator.m_count && found < max; index++) {
		if (m_allocator.m_pBitmap[index >> 3] & (1 << (7 - (index & 7)))) {
			found++;
			*p++ = *Unit(index);
			if (found >= m_allocator.m_allocated)
				break;
		}
	}
	if (max > found) {
		SceFiosHandle *pRest = &pHandles[found];
		uint32_t n = max - found;

		do {
			n--;
			*pRest++ = 0;
		} while (n != 0);
	}
	return m_allocator.m_allocated;
}
