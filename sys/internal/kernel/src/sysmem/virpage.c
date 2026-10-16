/*
 * SceSysmem: virtual pages, the pieces a memblock's address range is made of.
 * Each covers part of the range and, once backed, the physical pages behind
 * it. When one contiguous allocation fails a page is divided into several
 * backed by smaller allocations.
 */

#include "sysmem_internal.h"



/* phymem.c */
struct PhyPage;
int PhyMemPartAllocPhyPage(void *part, SceSize size, SceUInt32 flags, struct PhyPage **page);
int PhyMemPartFreePhyPage(void *part, struct PhyPage *page);
SceSize PhyMemPartGetFreeSizeInternal(void *part);
const char *PhyMemPartGetName(void *part);
SceUInt32 PhyMemPartTestAttr(void *part, SceUInt32 mask);
SceSize PhyPageGetSizeRaw(const struct PhyPage *page);

/* partition.c */
int PartitionAllocVirPage(void *partition, struct VirPage **vp);
void PartitionVirPageRelease(void *partition);

#define PHYMEM_PART_ATTR_DIVIDE		0x40	/* pages may be backed piece by piece */

/* VirPage: sysmem_internal.h */

#define VIRPAGE_MAGIC			0xd2519e9b

/* name unknown: an empty virtual page */
int VirPageInit(VirPage *vp)
{
	vp->magic = VIRPAGE_MAGIC;
	vp->next = NULL;
	vp->flags = 0;
	vp->vbase = 0;
	vp->size = 0;
	vp->page = NULL;
	vp->pbase = 0;
	return 0;
}

/* the first physical page's address (PhyPage info: pindex in bits 0..19) */
static inline uintptr_t VirPagePagePbase(const struct PhyPage *page)
{
	return *(const SceUInt32 *)page << 12;
}

/*
 * The rest of a page backed piece by piece: whole MiBs while they last, then
 * (with the partition's divide attribute) halving pieces down to 4 KiB. Each
 * piece after the first gets a new virtual page linked behind it.
 */
static inline int VirPageAllocPhyPageDivide(VirPage *vp, void *partition, void *phyPart,
					    SceSize rest, VirPage **out)
{
	VirPage *cur = vp, *prev;
	struct PhyPage *page;
	uintptr_t va;
	SceSize chunk = 0x100000, want = rest, size;
	int ret;

	if (PhyMemPartGetFreeSizeInternal(phyPart) < rest) {
		ret = SCE_KERNEL_ERROR_NO_FREE_PHYSICAL_PAGE;
		SCE_DBG_PRINTF(0xb, 139, "free size check for 0x%08x > 0x%08x failed\n", rest,
			       PhyMemPartGetFreeSizeInternal(phyPart));
		goto failed;
	}
	va = vp->vbase;
	while (rest != 0) {
		if (chunk >= 0x100000 && rest >= 0x100000) {
			want = rest & 0xfff00000;
			ret = PhyMemPartAllocPhyPage(phyPart, want, 0x20, &page);
			if (ret >= 0)
				goto got;
			chunk >>= 1;
			SCE_DBG_PRINTF(0xb, 161, "PhyMemPartAllocPhyPage(0x%08x) failed 0x%08x\n",
				       want, ret);
		} else {
			ret = SCE_KERNEL_ERROR_NO_FREE_PHYSICAL_PAGE;
		}
		for (;;) {
			if (!PhyMemPartTestAttr(phyPart, PHYMEM_PART_ATTR_DIVIDE))
				goto failed;
			if (chunk >= 0x100000)
				chunk = 0x80000;
			while (rest < chunk)
				chunk >>= 1;
			want = chunk;
			ret = PhyMemPartAllocPhyPage(phyPart, chunk, 0x10, &page);
			if (ret >= 0) {
				SCE_DBG_PRINTF(0xb, 195,
					       "PhyMemPartAllocPhyPage(0x%08x)-Small OK rest=0x%08x\n",
					       chunk, rest);
				break;
			}
			if (chunk <= 0x1000)
				goto failed;
			chunk >>= 1;
		}
got:
		cur->vbase = va;
		cur->flags = (cur->flags & ~0x3f) | VIRPAGE_TYPE_DIVIDED;
		size = PhyPageGetSizeRaw(page);
		rest -= size;
		va += size;
		cur->size = size;
		cur->page = page;
		cur->pbase = VirPagePagePbase(page);
		if (rest == 0)
			break;
		prev = cur;
		PartitionAllocVirPage(partition, &cur);
		cur->next = prev->next;
		prev->next = cur;
	}
	*out = cur;
	return 0;

failed:
#if FW_DEBUG
	SCE_DBG_PRINTF(0xb, 230, "Cannot allocate physical memory from [%s] 0x%08x\n",
		       PhyMemPartGetName(phyPart), PhyMemPartGetFreeSizeInternal(phyPart));
	SCE_DBG_PRINTF(0xb, 231, "\tREQUEST=0x%08x\n", want);
#else
	SCE_PRINTF_WITH_INFO(0, 235, "Cannot allocate physical memory from [%s] 0x%08x\n",
			     PhyMemPartGetName(phyPart), PhyMemPartGetFreeSizeInternal(phyPart));
	SCE_PRINTF_WITH_INFO(0, 236, "\tREQUEST=0x%08x\n", want);
#endif
	for (; vp != cur; vp = vp->next) {
		PhyMemPartFreePhyPage(phyPart, vp->page);
		vp->page = NULL;
	}
	return ret;
}

/* physical pages for the page; *out is the last piece when it had to be divided */
int VirPageAllocPhyPage(VirPage *vp, void *partition, void *phyPart, VirPage **out)
{
	struct PhyPage *page;
	SceSize size;
	int ret;

	*out = vp;
	if (vp->page != NULL) {
		SCE_DBG_ASSERT_FAIL(258);
		return SCE_KERNEL_ERROR_VIRPAGE_ERROR;
	}
	switch (VIRPAGE_TYPE(vp)) {
	case VIRPAGE_TYPE_1:
	case VIRPAGE_TYPE_8:
	case VIRPAGE_TYPE_16:
		return 0;
	case VIRPAGE_TYPE_PHY:
	case VIRPAGE_TYPE_DIVIDED:
		size = vp->size;
		if (vp->flags & VIRPAGE_ROUND_UP) {
			if (size < 0x100000) {
				SceSize pow2 = 1u << (31 - __builtin_clz(size));

				if (size != pow2)
					pow2 <<= 1;
				size = pow2;
			} else {
				size = (size + 0xfffff) & 0xfff00000;
			}
		}
		ret = PhyMemPartAllocPhyPage(phyPart, size, 0x10, &page);
		if (ret < 0)
			return VirPageAllocPhyPageDivide(vp, partition, phyPart, size, out);
		vp->page = page;
		vp->pbase = VirPagePagePbase(page);
		*out = vp;
		return ret;
	default:
		SCE_DBG_ASSERT_FAIL(273);
		return SCE_KERNEL_ERROR_ILLEGAL_VIRPAGE_TYPE;
	}
}

/* the physical range a page maps, none for the unbacked types */
int VirPageGetMapRange(VirPage *vp, uintptr_t *pbase, SceSize *size)
{
	uintptr_t pa = 0;
	SceSize len = 0;

	switch (VIRPAGE_TYPE(vp)) {
	case VIRPAGE_TYPE_1:
	case VIRPAGE_TYPE_PHY:
	case VIRPAGE_TYPE_8:
	case VIRPAGE_TYPE_32:
		break;
	case VIRPAGE_TYPE_DIVIDED:
		/* an allocated block (link bits 28..29 both set) */
		if (vp->page != NULL && (*(const SceUInt32 *)vp->page & 0x30000000) != 0x30000000) {
			SCE_DBG_ASSERT_FAIL(340);
			return SCE_KERNEL_ERROR_BLOCK_ERROR;
		}
		/* fall through */
	case VIRPAGE_TYPE_16:
		pa = vp->pbase;
		len = vp->size;
		break;
	default:
		SCE_DBG_ASSERT_FAIL(359);
		break;
	}
	*pbase = pa;
	*size = len;
	return 0;
}

int VirPageGetPhyBase(VirPage *vp, uintptr_t *pbase)
{
	if (vp == NULL) {
		SCE_DBG_ASSERT_FAIL(371);
	} else if (vp->page != NULL) {
		*pbase = VirPagePagePbase(vp->page);
		return 0;
	} else {
		SCE_DBG_ASSERT_FAIL(377);
	}
	*pbase = 0;
	return SCE_KERNEL_ERROR_VIRPAGE_ERROR;
}

/* name unknown: a virtual page cleared and back to the partition's heap */
int VirPageFree(void *partition, FixedHeap *heap, VirPage *vp)
{
	int ret;

	vp->flags = 0;
	vp->vbase = 0;
	vp->size = 0;
	vp->page = NULL;
	vp->pbase = 0;
	vp->magic = 0;
	ret = FixedHeapFreeObject(heap, vp);
	if (ret >= 0)
		PartitionVirPageRelease(partition);
	return ret;
}

/* name unknown: the virtual page class constructor */
int VirPageCreate(void)
{
	return 0;
}

/* name unknown: the virtual page class destructor */
int VirPageDestroy(void)
{
	return 0;
}

/*
 * The memblock allocation parameters (sysmem_internal.h).
 * PLACEHOLDER: the file these helpers belong to is not known (they sit
 * between virpage.c and l1pagetable.c).
 */

void AllocMemBlockCommandInit(SceKernelAllocMemBlockCommand *command, const char *name,
			      SceSize size)
{
	command->name = name;
	command->size = size;
	command->offset = 0;
	command->padLow = 0;
	command->padHigh = 0;
	memset(&command->opt, 0, sizeof(command->opt));
	command->opt.size = sizeof(command->opt);
	command->block = NULL;
	command->result = NULL;
	command->typeInfo = NULL;
	command->addressSpace = NULL;
	command->phyMemPart = NULL;
	command->partition = NULL;
	command->cmdFlags = 0;
	command->opt.pid = -1;
}

void AllocMemBlockCommandInitWithType(SceKernelAllocMemBlockCommand *command, const char *name,
				      SceUInt32 type, SceSize size)
{
	AllocMemBlockCommandInit(command, name, size);
	command->type = type;
}

void AllocMemBlockCommandSetExtraLow(SceKernelAllocMemBlockCommand *command, SceUInt32 extraLow)
{
	command->opt.extraLow = extraLow;
	command->opt.attr |= ALLOC_ATTR_HAS_EXTRA_LOW;
}

void AllocMemBlockCommandSetExtraHigh(SceKernelAllocMemBlockCommand *command, SceUInt32 extraHigh)
{
	command->opt.extraHigh = extraHigh;
	command->opt.attr |= ALLOC_ATTR_HAS_EXTRA_HIGH;
}
