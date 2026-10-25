/*
 * The real-time kernel's service calls: a µITRON 4.0 kernel (its service calls pass µITRON
 * 4.0's function codes to the kernel's hook), declared as the application calls them. In the
 * large model every pointer is far.
 */
#ifndef KERNEL_H
#define KERNEL_H

typedef signed int	ER;
typedef signed int	ID;
typedef signed int	PRI;
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

/* ref_flg's and ref_mbx's packets */
typedef struct t_rflg {
	ID		wtskid;		/* the task waiting, 0 none */
	FLGPTN		flgptn;		/* the flag's pattern */
} T_RFLG;

typedef struct t_rmbx {
	ID		wtskid;
	T_MSG		*pk_msg;	/* the first message queued, NULL none */
} T_RMBX;

#define E_OK		0
#define E_TMOUT		(-50)

#define TWF_ANDW	0x00
#define TWF_ORW		0x01

#define TMO_POL		0
#define TMO_FEVR	(-1)

#define TSK_SELF	0

extern ER	act_tsk(ID tskid);
extern void	ext_tsk(void);
extern ER	chg_pri(ID tskid, PRI tskpri);
extern ER	dly_tsk(RELTIM dlytim);

extern ER	set_flg(ID flgid, FLGPTN setptn);
extern ER	iset_flg(ID flgid, FLGPTN setptn);
extern ER	clr_flg(ID flgid, FLGPTN clrptn);
extern ER	wai_flg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN *p_flgptn);
extern ER	twai_flg(ID flgid, FLGPTN waiptn, MODE wfmode, FLGPTN *p_flgptn, TMO tmout);
extern ER	ref_flg(ID flgid, T_RFLG *pk_rflg);

extern ER	snd_mbx(ID mbxid, T_MSG *pk_msg);
extern ER	rcv_mbx(ID mbxid, T_MSG **ppk_msg);
extern ER	ref_mbx(ID mbxid, T_RMBX *pk_rmbx);

extern ER	get_mpf(ID mpfid, VP *p_blk);
extern ER	pget_mpf(ID mpfid, VP *p_blk);
extern ER	rel_mpf(ID mpfid, VP blk);

extern ER	sta_cyc(ID cycid);

extern ER	isig_tim(void);

extern ER	dis_dsp(void);
extern ER	ena_dsp(void);

#endif
