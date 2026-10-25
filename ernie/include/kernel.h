/*
 * The real-time kernel's service calls: a µITRON 4.0 kernel (its service calls pass µITRON
 * 4.0's function codes to the kernel's hook), declared as the application calls them. In the
 * large model every pointer is far.
 */
#ifndef KERNEL_H
#define KERNEL_H

typedef signed int	ER;
typedef signed int	ID;
typedef unsigned int	MODE;
typedef unsigned int	FLGPTN;
typedef signed long	TMO;
typedef unsigned long	RELTIM;
typedef signed long	VP_INT;
typedef void		*VP;

/* the head of a mailbox message */
typedef struct t_msg {
	struct t_msg	*next;
} T_MSG;

#define E_OK		0

#define TWF_ANDW	0x00
#define TWF_ORW		0x01

#define TMO_POL		0
#define TMO_FEVR	(-1)

extern void	ext_tsk(void);
extern ER	dly_tsk(RELTIM dlytim);

extern ER	set_flg(ID flgid, FLGPTN setptn);
extern ER	clr_flg(ID flgid, FLGPTN clrptn);
extern ER	wai_flg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN *p_flgptn);
extern ER	twai_flg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN *p_flgptn, TMO tmout);

extern ER	snd_mbx(ID mbxid, T_MSG *pk_msg);
extern ER	rcv_mbx(ID mbxid, T_MSG **ppk_msg);

extern ER	get_mpf(ID mpfid, VP *p_blk);
extern ER	pget_mpf(ID mpfid, VP *p_blk);
extern ER	rel_mpf(ID mpfid, VP blk);

extern ER	dis_dsp(void);
extern ER	ena_dsp(void);

#endif
