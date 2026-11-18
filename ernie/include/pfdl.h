/*
 * Renesas' RL78 data flash library (PFDL), the interface USS-1001's flash.c calls: Renesas'
 * names for its functions, types and codes. The data flash is 8 blocks of 0x400 bytes at
 * 0xf1000, addressed by the library from 0.
 */
#ifndef ERNIE_PFDL_H
#define ERNIE_PFDL_H

typedef unsigned char	pfdl_u08;
typedef unsigned int	pfdl_u16;
typedef unsigned char	pfdl_status_t;
typedef unsigned char	pfdl_command_t;

typedef struct {
	pfdl_u08	fx_MHz_u08;		/* the CPU clock, in MHz */
	pfdl_u08	wide_voltage_mode_u08;	/* 1: the wide voltage mode */
} pfdl_descriptor_t;

typedef struct {
	pfdl_u16	index_u16;		/* the byte, or the block to erase */
	pfdl_u08 __near	*data_pu08;
	pfdl_u16	bytecount_u16;
	pfdl_command_t	command_enu;
} pfdl_request_t;

/* commands */
#define PFDL_CMD_READ_BYTES		0x00
#define PFDL_CMD_ERASE_BLOCK		0x03
#define PFDL_CMD_WRITE_BYTES		0x04
#define PFDL_CMD_IVERIFY_BYTES		0x06
#define PFDL_CMD_BLANKCHECK_BYTES	0x08

/* status */
#define PFDL_OK				0x00
#define PFDL_ERR_ERASE			0x1a
#define PFDL_ERR_MARGIN			0x1b	/* the blank check: not blank */
#define PFDL_ERR_WRITE			0x1c
#define PFDL_BUSY			0xff

extern pfdl_status_t PFDL_Open(pfdl_descriptor_t __near *descriptor_pstr);
extern void PFDL_Close(void);
extern pfdl_status_t PFDL_Execute(pfdl_request_t __near *request_pstr);
extern pfdl_status_t PFDL_Handler(void);

#endif
