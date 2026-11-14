/* unwinder.h: the ARM exception handling ABI's (EHABI) unwinder interface: the exception
 * object the unwinder sees, the virtual register set, the personality routines' results and
 * the debugger hook. The names are EHABI's. */
#ifndef _UNWINDER_H
#define _UNWINDER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
	{	/* unwinder and personality routine results */
	_URC_OK = 0,
	_URC_FOREIGN_EXCEPTION_CAUGHT = 1,
	_URC_HANDLER_FOUND = 6,
	_URC_INSTALL_CONTEXT = 7,
	_URC_CONTINUE_UNWIND = 8,
	_URC_FAILURE = 9
	} _Unwind_Reason_Code;

typedef enum
	{	/* what a personality routine is asked to do */
	_US_VIRTUAL_UNWIND_FRAME = 0,
	_US_UNWIND_FRAME_STARTING = 1,
	_US_UNWIND_FRAME_RESUME = 2
	} _Unwind_State;

typedef struct _Unwind_Control_Block
	{	/* the exception object the unwinder sees */
	char exception_class[8];
	void (*exception_cleanup)(_Unwind_Reason_Code, struct _Unwind_Control_Block *);
	struct
		{	/* the unwinder's own state */
		unsigned int reserved1;	/* nonzero while a copy of the state is saved */
		unsigned int reserved2;
		unsigned int reserved3;
		unsigned int reserved4;
		unsigned int reserved5;
		} unwinder_cache;
	struct
		{	/* what phase 1 found, for phase 2 */
		unsigned int sp;
		unsigned int bitpattern[5];
		} barrier_cache;
	struct
		{	/* the personality routine's cleanup state */
		unsigned int bitpattern[4];
		} cleanup_cache;
	struct
		{	/* the current frame's exception table entry */
		unsigned int fnstart;
		unsigned int *ehtp;
		unsigned int additional;
		unsigned int reserved1;
		} pr_cache;
	long long : 0;
	} _Unwind_Control_Block;

typedef struct _Unwind_Context _Unwind_Context;

		/* THE VIRTUAL REGISTER SET */
typedef enum
	{	/* register classes */
	_UVRSC_CORE = 0,
	_UVRSC_VFP = 1,
	_UVRSC_WMMXD = 3,
	_UVRSC_WMMXC = 4
	} _Unwind_VRS_RegClass;

typedef enum
	{	/* register data representations */
	_UVRSD_UINT32 = 0,
	_UVRSD_VFPX = 1,
	_UVRSD_UINT64 = 3,
	_UVRSD_FLOAT = 4,
	_UVRSD_DOUBLE = 5
	} _Unwind_VRS_DataRepresentation;

typedef enum
	{	/* register set results */
	_UVRSR_OK = 0,
	_UVRSR_NOT_IMPLEMENTED = 1,
	_UVRSR_FAILED = 2
	} _Unwind_VRS_Result;

_Unwind_VRS_Result _Unwind_VRS_Set(_Unwind_Context *context, _Unwind_VRS_RegClass regclass,
	unsigned int regno, _Unwind_VRS_DataRepresentation representation, void *valuep);
_Unwind_VRS_Result _Unwind_VRS_Get(_Unwind_Context *context, _Unwind_VRS_RegClass regclass,
	unsigned int regno, _Unwind_VRS_DataRepresentation representation, void *valuep);
_Unwind_VRS_Result _Unwind_VRS_Pop(_Unwind_Context *context, _Unwind_VRS_RegClass regclass,
	unsigned int discriminator, _Unwind_VRS_DataRepresentation representation);

		/* THE UNWINDER */
_Unwind_Reason_Code _Unwind_RaiseException(_Unwind_Control_Block *ucbp);
void _Unwind_Resume(_Unwind_Control_Block *ucbp);
void _Unwind_Complete(_Unwind_Control_Block *ucbp);
void _Unwind_DeleteException(_Unwind_Control_Block *ucbp);

		/* THE DEBUGGER HOOK */
#define _UASUBSYS_CPP		0x00	/* the C++ runtime */
#define _UAACT_STARTING		0x0	/* a throw starts */
#define _UAACT_ENDING		0x1	/* unwinding ends: arg is one of _UAARG_ENDING_* */
#define _UAACT_BARRIERFOUND	0x2	/* phase 1 found a handler: arg is its landing pad */
#define _UAACT_PADENTRY		0x3	/* a landing pad is entered: arg is its address */
#define _UAACT_CPP_TYPEINFO	0x80	/* the handler's type: arg is its type_info */
#define _UAARG_ENDING_UNSPECIFIED		0
#define _UAARG_ENDING_TABLECORRUPT		1
#define _UAARG_ENDING_NOUNWIND			2
#define _UAARG_ENDING_VRSFAILED			3
#define _UAARG_ENDING_CPP_BADOPCODE		4
#define _UAARG_ENDING_UNWINDER_LOOKUPFAILED	5
#define _UAARG_ENDING_UNWINDER_BUFFERFAILED	6
void _Unwind_Activity(_Unwind_Control_Block *ucbp, unsigned int reason, unsigned int arg);

#ifdef __cplusplus
}
#endif
#endif /* _UNWINDER_H */
