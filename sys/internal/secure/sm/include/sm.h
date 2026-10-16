/*
 * Secure modules (os0:sm, run by the Cmep at 0x80b000): build selection and the entry's
 * interface. Names follow the henkaku wiki until Sony's are found.
 */
#ifndef SM_H
#define SM_H

#define SM_LOAD_ADDRESS		0x80b000

extern unsigned int __stack_chk_guard;

void call_ctors(void);
void call_dtors(void);
int sm_main(void);

#endif
