/* SCE: the psp2 port's VM parameters: arm32's 4 KiB pages (arm/arm32/vmparam.h) */
#ifndef _PSP2_VMPARAM_H_
#define _PSP2_VMPARAM_H_

#define PAGE_SHIFT	12
#define PAGE_SIZE	(1 << PAGE_SHIFT)
#define PAGE_MASK	(PAGE_SIZE - 1)

/* one physical segment and free list, which uvm's headers size their tables by */
#define VM_PHYSSEG_MAX		1
#define VM_PHYSSEG_STRAT	VM_PSTRAT_BSEARCH
#define VM_NFREELIST		1
#define VM_FREELIST_DEFAULT	0

#endif /* _PSP2_VMPARAM_H_ */
