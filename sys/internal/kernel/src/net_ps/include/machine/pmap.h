/*
 * SCE: the psp2 port's pmap: the stack maps no pages of its own; the types uvm's headers
 * name, without arm32's pmap.
 */
#ifndef _PSP2_PMAP_H_
#define _PSP2_PMAP_H_

struct pmap {
	int pm_unused;
};
typedef struct pmap *pmap_t;

struct vm_page_md {
	int pvh_unused;
};
#define VM_MDPAGE_INIT(pg)	((void)0)

extern struct pmap kernel_pmap_store;
#define pmap_kernel()		(&kernel_pmap_store)
#define pmap_resident_count(pmap)	0
#define pmap_wired_count(pmap)	0

#endif /* _PSP2_PMAP_H_ */
