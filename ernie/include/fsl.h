/*
 * Renesas' RL78 flash self-programming library (FSL, type 01), the interface USS-1001's flash.c
 * calls: Renesas' names for its functions and types. FSL_PrepareFunctions and
 * FSL_PrepareExtFunctions make the commands that run from RAM (copied to 0xfd090 by _rcopy) ready.
 * Which of the extended commands are FSL_InvertBootFlag and FSL_SwapBootCluster is read from what
 * they do: the second sets the address to go on at and enters the boot swap.
 */
#ifndef ERNIE_FSL_H
#define ERNIE_FSL_H

typedef unsigned char	fsl_u08;
typedef unsigned int	fsl_u16;
typedef unsigned long	fsl_u32;

typedef struct {
	fsl_u08		fsl_flash_voltage_u08;		/* 1: the wide voltage mode */
	fsl_u08		fsl_frequency_u08;		/* the CPU clock, in MHz */
	fsl_u08		fsl_auto_status_check_u08;	/* 1: each command runs to its end */
} fsl_descriptor_t;

typedef struct {
	fsl_u08 __near	*fsl_data_buffer_p_u08;
	fsl_u32		fsl_destination_address_u32;
	fsl_u08		fsl_word_count_u08;		/* words of 4 bytes */
} fsl_write_t;

#define FSL_OK			0x00
#define FSL_ERR_BLANKCHECK	0x1b

#define FSL_BLOCK_SIZE		0x400

extern fsl_u08 FSL_Init(const fsl_descriptor_t __far *descriptor_pstr);
extern void FSL_Open(void);
extern void FSL_Close(void);
extern void FSL_PrepareFunctions(void);
extern void FSL_PrepareExtFunctions(void);
extern fsl_u08 FSL_BlankCheck(fsl_u16 block_u16);
extern fsl_u08 FSL_Erase(fsl_u16 block_u16);
extern fsl_u08 FSL_IVerify(fsl_u16 block_u16);
extern fsl_u08 FSL_Write(fsl_write_t __near *write_pstr);
extern fsl_u08 FSL_InvertBootFlag(void);
extern fsl_u08 FSL_SwapBootCluster(void);

/* CA78K0R's copy of the code that runs from RAM: 1, the library's commands */
extern int _rcopy(int number);

#endif
